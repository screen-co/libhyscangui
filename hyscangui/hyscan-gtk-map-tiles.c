/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-gtk-map-tile-source.h>
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <math.h>
#include <string.h>

#define CACHE_HEADER_MAGIC     0x308d32d9    /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_MAP,
  PROP_CACHE,
  PROP_SOURCE
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;          /* Идентификатор заголовка. */
  guint32                      length;         /* Размер данных поверхности. */
} HyScanGtkMapTilesCacheHeader;

struct _HyScanGtkMapTilesPrivate
{
  HyScanGtkMap                *map;            /* Виджет карты, на котором показываются тайлы. */
  HyScanCache                 *cache;          /* Кэш тайлов. */

  HyScanBuffer                *cache_buffer;   /* Буфер заголовка кэша данных. */
  HyScanBuffer                *tile_buffer;    /* Буфер данных поверхности тайла. */
  gchar                       *key;            /* Ключ кэширования. */
  size_t                       key_length;     /* Длина ключа. */

  HyScanGtkMapTileSource      *source;         /* Источник тайлов. */
  HyScanTaskQueue             *task_queue;     /* Очередь задач по созданию тайлов. */
};

static void                 hyscan_gtk_map_tiles_set_property             (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           const GValue             *value,
                                                                           GParamSpec               *pspec);
static void                 hyscan_gtk_map_tiles_object_constructed       (GObject                  *object);
static void                 hyscan_gtk_map_tiles_object_finalize          (GObject                  *object);
static void                 hyscan_gtk_map_tiles_draw                     (HyScanGtkMapTiles        *layer,
                                                                           cairo_t                  *cairo);
static void                 hyscan_gtk_map_tiles_load                     (HyScanGtkMapTile         *tile,
                                                                           HyScanGtkMapTiles        *layer);
static void                 hyscan_gtk_map_tiles_update_cache_key         (HyScanGtkMapTilesPrivate *priv,
                                                                           HyScanGtkMapTile         *tile);
static gboolean             hyscan_gtk_map_tiles_fill_tile                (HyScanGtkMapTilesPrivate *priv,
                                                                           HyScanGtkMapTile         *tile);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapTiles, hyscan_gtk_map_tiles, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tiles_class_init (HyScanGtkMapTilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiles_set_property;

  object_class->constructed = hyscan_gtk_map_tiles_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiles_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
                                   g_param_spec_object ("map", "Map", "GtkMap object",
                                                        HYSCAN_TYPE_GTK_MAP,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
                                   g_param_spec_object ("cache", "Cache", "Cache object",
                                                        HYSCAN_TYPE_CACHE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SOURCE,
                                   g_param_spec_object ("source", "Tile source", "GtkMapTileSource object",
                                                        HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tiles_init (HyScanGtkMapTiles *gtk_map_tiles)
{
  gtk_map_tiles->priv = hyscan_gtk_map_tiles_get_instance_private (gtk_map_tiles);
}

static void
hyscan_gtk_map_tiles_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tiles_object_constructed (GObject *object)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_parent_class)->constructed (object);

  /* Создаём очередь задач по поиску тайлов. */
  priv->task_queue = hyscan_task_queue_new ((GFunc) hyscan_gtk_map_tiles_load, gtk_map_tiles,
                                            (GDestroyNotify) g_object_unref ,
                                            (GCompareFunc) hyscan_gtk_map_tile_compare);

  priv->cache_buffer = hyscan_buffer_new ();
  priv->tile_buffer = hyscan_buffer_new ();

  /* Подключаемся к сигналу обновления видимой области карты. */
  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_tiles_draw), gtk_map_tiles);
}

static void
hyscan_gtk_map_tiles_object_finalize (GObject *object)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  /* Отключаемся от сигналов. */
  g_signal_handlers_disconnect_by_data (priv->map, gtk_map_tiles);

  /* Освобождаем память. */
  g_object_unref (priv->task_queue);
  g_object_unref (priv->map);
  g_object_unref (priv->source);
  g_object_unref (priv->cache);
  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->tile_buffer);
  g_free (priv->key);

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_parent_class)->finalize (object);
}

/* Заполняет запрошенный тайл из источника тайлов. */
static void
hyscan_gtk_map_tiles_load (HyScanGtkMapTile  *tile,
                           HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  /* Если удалось заполнить новый тайл, то запрашиваем обновление области
   * виджета с новым тайлом. */
  if (hyscan_gtk_map_tile_source_fill (priv->source, tile))
    {
      HyScanGtkMapTilesCacheHeader header;
      HyScanBuffer *buffer;

      guchar *data;
      guint dsize;

      /* Оборачиваем в буфер данные поверхности тайла. */
      data = hyscan_gtk_map_tile_get_data (tile, &dsize);
      buffer = hyscan_buffer_new ();
      hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, (guint32) dsize);

      /* Оборачиваем в буфер заголовок. */
      header.magic = CACHE_HEADER_MAGIC;
      header.length = dsize;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      /* Помещаем данные тайла в кэш. */
      hyscan_gtk_map_tiles_update_cache_key (priv, tile);
      hyscan_cache_set2 (priv->cache, priv->key, NULL, priv->cache_buffer, buffer);

      /* Запрашиваем перерисовку карты. todo: add if (tile inside visible area) { ... } */
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      g_object_unref (buffer);
    }
}

/* Устанавливает ключ кэширования для тайла @tile. */
static void
hyscan_gtk_map_tiles_update_cache_key (HyScanGtkMapTilesPrivate *priv,
                                       HyScanGtkMapTile         *tile)
{
  if (priv->key == NULL)
    {
      priv->key = g_strdup_printf ("tile.%u.%u.%u.%u", G_MAXUINT, G_MAXUINT, G_MAXUINT, G_MAXUINT);
      priv->key_length = strlen (priv->key);
    }

  g_snprintf (priv->key, priv->key_length,
              "tile.%u.%u.%u.%u",
              hyscan_gtk_map_tile_get_zoom (tile),
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_size (tile));
}

/* Функция проверяет кэш на наличие данных и считывает их. */
static gboolean
hyscan_gtk_map_tiles_check_cache (HyScanGtkMapTilesPrivate *priv,
                                  HyScanGtkMapTile         *tile)
{
  HyScanGtkMapTilesCacheHeader header;

  if (priv->cache == NULL)
    return FALSE;

  /* Формируем ключ кэшированных данных. */
  hyscan_gtk_map_tiles_update_cache_key (priv, tile);

  /* Ищем данные в кэше. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->key, NULL, sizeof (header), priv->cache_buffer, priv->tile_buffer))
    return FALSE;

  /* Верификация данных. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.length != hyscan_buffer_get_size (priv->tile_buffer)))
    {
      return FALSE;
    }

  return TRUE;
}

/* Получает информацию об указанном тайле. Сначала пробует искать в кэше, потом
 * идёт к источнику тайлов.
 * Возвращает TRUE, если тайл загружен. Иначе FALSE. */
static gboolean
hyscan_gtk_map_tiles_fill_tile (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  gboolean found;

  found = hyscan_gtk_map_tiles_check_cache (priv, tile);
  if (found)
    /* Тайл найден в кэше, копируем его данные. */
    {
      guint32 size;
      guchar *data;

      data = hyscan_buffer_get_data (priv->tile_buffer, &size);
      hyscan_gtk_map_tile_set_data (tile, data, size);
    }
  else
    /* Тайл не найден, добавляем его в очередь на загрузку. */
    {
      g_object_ref (tile);
      hyscan_task_queue_push (priv->task_queue, tile);
    }

  return found;
}

/* Рисует тайлы текущей видимой области по сигналу "visible-draw". */
static void
hyscan_gtk_map_tiles_draw (HyScanGtkMapTiles *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  gint x0, xn, y0, yn, x, y;
  guint zoom;
  guint tile_size;
  gint last_tile;

  cairo_t *tiles_cairo;
  cairo_surface_t *tiles_surface;

  /* Получаем границы тайлов, покрывающих видимую область. */
  hyscan_gtk_map_get_tile_view_i (priv->map, &x0, &xn, &y0, &yn);
  zoom = hyscan_gtk_map_get_zoom (priv->map);
  tile_size = hyscan_gtk_map_get_tile_size (priv->map);

  /* Берём только валидные номера тайлов: от 0 до 2^zoom - 1. */
  last_tile = (zoom == 0 ? 1 : 2 << (zoom - 1)) - 1;
  x0 = CLAMP (x0, 0, last_tile);
  y0 = CLAMP (y0, 0, last_tile);
  xn = CLAMP (xn, 0, last_tile);
  yn = CLAMP (yn, 0, last_tile);

  /* Создаем буфер для отрисовки тайлов в исходном размере tile_size. */
  tiles_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                              tile_size * (xn - x0 + 1),
                                              tile_size * (yn - y0 + 1));
  tiles_cairo = cairo_create (tiles_surface);

  /* Очищаем очередь загрузки тайлов, которую мы сформировали в прошлый раз. */
  hyscan_task_queue_clear (priv->task_queue);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  for (x = x0; x <= xn; ++x)
    {
      for (y = y0; y <= yn; ++y)
        {
          HyScanGtkMapTile *tile;

          tile = hyscan_gtk_map_tile_new ((guint) x, (guint) y, zoom, tile_size);

          /* Если получилось заполнить тайл, то рисуем его в буфер. */
          if (hyscan_gtk_map_tiles_fill_tile (priv, tile))
            {
              cairo_surface_t *tile_surface;

              tile_surface = hyscan_gtk_map_tile_get_surface (tile);
              cairo_set_source_surface (tiles_cairo, tile_surface, (x - x0) * tile_size, (y - y0) * tile_size);
              cairo_paint (tiles_cairo);
            }

          g_object_unref (tile);
        }
    }

  /* Растягиваем буфер под необходимый размер тайла и переносим его на поверхность виджета. */
  {
    gdouble scale;
    gdouble xs, ys;

    scale = hyscan_gtk_map_get_tile_scaling (priv->map);
    hyscan_gtk_map_tile_to_point (priv->map, &xs, &ys, x0, y0);

    cairo_save (cairo);
    cairo_scale (cairo, scale, scale);
    cairo_set_source_surface (cairo, tiles_surface, xs / scale, ys / scale);
    cairo_paint (cairo);
    cairo_restore (cairo);
  }

  cairo_destroy (tiles_cairo);
  cairo_surface_destroy (tiles_surface);
}

/**
 * hyscan_gtk_map_tiles_new:
 * @map: указатель на объект #HyScanGtkMap
 *
 * Создаёт новый объект #HyScanGtkMapTiles, который рисует слой тайлов на карте
 * @map.
 *
 * Returns: указатель на #HyScanGtkMapTiles. Для удаления g_object_unref().
 */
HyScanGtkMapTiles *
hyscan_gtk_map_tiles_new (HyScanGtkMap            *map,
                          HyScanCache             *cache,
                          HyScanGtkMapTileSource *source)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILES,
                       "map", map,
                       "cache", cache,
                       "source", source, NULL);
}