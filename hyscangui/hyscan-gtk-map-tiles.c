/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-gtk-map-tile-source.h>
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP,
  PROP_CACHE,
  PROP_SOURCE
};

struct _HyScanGtkMapTilesPrivate
{
  HyScanGtkMap                *map;            /* Виджет карты, на котором показываются тайлы. */
  HyScanCache                 *cache;          /* Кэш тайлов. */

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
static void                 hyscan_gtk_map_tiles_cache_key                (HyScanGtkMapTile         *tile,
                                                                           gchar                    *key,
                                                                           gsize                     key_len);
static gboolean             hyscan_gtk_map_tiles_wrap_tile_surface        (HyScanBuffer             *buffer,
                                                                           HyScanGtkMapTile         *tile);
static gboolean             hyscan_gtk_map_tiles_fill_tile                (HyScanGtkMapTilesPrivate *priv,
                                                                           HyScanGtkMapTile         *tile);
static void                 hyscan_gtk_map_tiles_draw_tile                (HyScanGtkMapTiles        *layer,
                                                                           HyScanGtkMapTile         *tile,
                                                                           cairo_t                  *cairo);

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
      HyScanBuffer *buffer;
      gchar key[256];

      /* Помещаем найденный тайл в кэш. */
      buffer = hyscan_buffer_new ();
      hyscan_gtk_map_tiles_wrap_tile_surface (buffer, tile);
      hyscan_gtk_map_tiles_cache_key (tile, key, 256);
      hyscan_cache_set (priv->cache, key, NULL, buffer);

      /* Запрашиваем перерисовку карты. todo: add if (tile inside visible area) { ... } */
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      g_object_unref (buffer);
    }
}

/* Оборачивает данные поверхности @surface в буфер @buffer. */
static gboolean
hyscan_gtk_map_tiles_wrap_tile_surface (HyScanBuffer     *buffer,
                                        HyScanGtkMapTile *tile)
{
  guchar *data;
  gssize dsize;
  cairo_surface_t *surface;

  surface = hyscan_gtk_map_tile_get_surface (tile);

  /* Готовим буфер для кэша. */
  dsize = cairo_image_surface_get_stride (surface);
  dsize *= cairo_image_surface_get_height (surface);

  if (dsize <= 0)
    {
      g_warning ("HyScanGtkMapTiles: tile surface is corrupted");
      return FALSE;
    }

  data = cairo_image_surface_get_data (surface);
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, (guint32) dsize);

  return TRUE;
}

/* Записывает в буфер @key длины @key_len ключ кэша для тайла @tile. */
static void
hyscan_gtk_map_tiles_cache_key (HyScanGtkMapTile *tile,
                                gchar            *key,
                                gsize             key_len)
{
  g_snprintf (key, key_len, "tile.%d.%d.%d.%d",
              hyscan_gtk_map_tile_get_zoom (tile),
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_size (tile));
}

/* Получает информацию об указанном тайле. Сначала пробует искать в кэше, потом
 * идёт к источнику тайлов.
 * Возвращает TRUE, если тайл загружен. Иначе FALSE. */
static gboolean
hyscan_gtk_map_tiles_fill_tile (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  HyScanBuffer *cache_buffer;
  gchar key[256];
  gboolean found;

  cache_buffer = hyscan_buffer_new ();
  hyscan_gtk_map_tiles_wrap_tile_surface (cache_buffer, tile);

  /* Формируем ключ кэшированных данных. */
  hyscan_gtk_map_tiles_cache_key (tile, key, 256);

  found = hyscan_cache_get (priv->cache, key, NULL, cache_buffer);

  if (!found) /* Тайл в кэше не найден, добавляем его в очередь на загрузку. */
    {
      /* Передаём тайл в очередь. */
      g_object_ref (tile);
      hyscan_task_queue_push (priv->task_queue, tile);
    }

  g_object_unref (cache_buffer);

  return found;
}

/* Отрисовывает отдельный тайл. */
static void
hyscan_gtk_map_tiles_draw_tile (HyScanGtkMapTiles *layer,
                                HyScanGtkMapTile  *tile,
                                cairo_t           *cairo)
{
  gdouble x, y;
  cairo_surface_t *surface;

  /* Переводим координаты тайла в координаты для рисования. */
  hyscan_gtk_map_tile_to_point (layer->priv->map, &x, &y,
                                hyscan_gtk_map_tile_get_x (tile),
                                hyscan_gtk_map_tile_get_y (tile));

  /* todo: use dummy surface if null */
  surface = hyscan_gtk_map_tile_get_surface (tile);

  /* Рисуем тайл в нужном месте. */
  cairo_surface_mark_dirty (surface);
  cairo_set_source_surface (cairo, surface, x, y);
  cairo_paint (cairo);
}

/* Рисует тайлы текущей видимой области по сигналу "visible-draw". */
static void
hyscan_gtk_map_tiles_draw (HyScanGtkMapTiles *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  guint x0, y0, xn, yn, x, y;
  guint zoom;

  /* Получаем границы тайлов, покрывающих видимую область. */
  hyscan_gtk_map_get_tile_view_i (priv->map, &x0, &xn, &y0, &yn);
  zoom = hyscan_gtk_map_get_zoom (priv->map);

  /* Очищаем очередь загрузки тайлов, которую мы сформировали в прошлый раз. */
  hyscan_task_queue_clear (priv->task_queue);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  for (x = x0; x <= xn; ++x)
    {
      for (y = y0; y <= yn; ++y)
        {
          HyScanGtkMapTile *tile;

          tile = hyscan_gtk_map_tile_new (x, y, zoom);
          hyscan_gtk_map_tiles_fill_tile (priv, tile);
          hyscan_gtk_map_tiles_draw_tile (layer, tile, cairo);

          g_object_unref (tile);
        }
    }
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