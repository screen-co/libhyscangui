/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map-tiles-osm.h>
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

static void                 hyscan_gtk_map_tiles_set_property             (GObject                *object,
                                                                           guint                   prop_id,
                                                                           const GValue           *value,
                                                                           GParamSpec             *pspec);
static void                 hyscan_gtk_map_tiles_object_constructed       (GObject                *object);
static void                 hyscan_gtk_map_tiles_object_finalize          (GObject                *object);
static void                 hyscan_gtk_map_tiles_draw                     (HyScanGtkMapTiles      *layer,
                                                                           cairo_t                *cairo);
static gint                 hyscan_gtk_map_tiles_compare                  (HyScanGtkMapTile       *a,
                                                                           HyScanGtkMapTile       *b);
static void                 hyscan_gtk_map_tiles_load                     (HyScanGtkMapTile       *tile,
                                                                           HyScanGtkMapTiles      *layer);
static void                 hyscan_gtk_map_tiles_key                      (HyScanGtkMapTile       *tile,
                                                                           gchar                  *key,
                                                                           gsize                   key_len);
static gboolean             hyscan_gtk_map_tiles_wrap_surface             (HyScanBuffer           *buffer,
                                                                           cairo_surface_t        *surface);
static void                 hyscan_gtk_map_tiles_tile_free                (HyScanGtkMapTile       *tile);
static HyScanGtkMapTile *   hyscan_gtk_map_tiles_tile_copy                (HyScanGtkMapTile       *tile);

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
      priv->map = g_value_get_object (value);
      break;

    case PROP_SOURCE:
      priv->source = g_value_get_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_get_object (value);
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
                                            (GDestroyNotify) hyscan_gtk_map_tiles_tile_free,
                                            (GCompareFunc) hyscan_gtk_map_tiles_compare);

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
      hyscan_gtk_map_tiles_wrap_surface (buffer, tile->surface);
      hyscan_gtk_map_tiles_key (tile, key, 256);
      hyscan_cache_set (priv->cache, key, NULL, buffer);

      /* Запрашиваем перерисовку карты. todo: add if (tile inside visible area) { ... } */
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      g_object_unref (buffer);
    }
}

/* Сравнивает тайлы a и b. Возвращает 0, если тайлы одинаковые. */
static gint
hyscan_gtk_map_tiles_compare (HyScanGtkMapTile *a,
                              HyScanGtkMapTile *b)
{
  if (a->x == b->x && a->y == b->y && a->zoom == b->zoom)
    return 0;
  else
    return 1;
}

/* Оборачивает данные поверхности @surface в буфер @buffer. */
static gboolean
hyscan_gtk_map_tiles_wrap_surface (HyScanBuffer    *buffer,
                                   cairo_surface_t *surface)
{
  guchar *data;
  gssize dsize;

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
hyscan_gtk_map_tiles_key (HyScanGtkMapTile *tile,
                          gchar            *key,
                          gsize             key_len)
{
  g_snprintf (key, key_len, "tile.%d.%d.%d", tile->zoom, tile->x, tile->y);
}

/* Получает информацию об указанном тайле. Если тайла нет в кэше, то делает
 * запрос в источник тайлов.
 * Возвращает TRUE, если тайл загружен. Иначе FALSE. */
static gboolean
hyscan_gtk_map_tiles_get_tile (HyScanGtkMapTilesPrivate *priv,
                               HyScanGtkMapTile         *tile)
{
  HyScanBuffer *cache_buffer;
  gchar key[256];
  gboolean found;

  cache_buffer = hyscan_buffer_new ();
  hyscan_gtk_map_tiles_wrap_surface (cache_buffer, tile->surface);

  /* Формируем ключ кэшированных данных. */
  hyscan_gtk_map_tiles_key (tile, key, 256);

  found = hyscan_cache_get (priv->cache, key, NULL, cache_buffer);

  if (!found) /* Тайл в кэше не найден, добавляем его в очередь на загрузку. */
    {
      HyScanGtkMapTile *new_tile;

      /* Делаем копию тайла и передаём её в очередь. */
      new_tile = hyscan_gtk_map_tiles_tile_copy (tile);
      hyscan_task_queue_push (priv->task_queue, new_tile);
    }

  g_object_unref (cache_buffer);

  return found;
}

/* Освобождает память, занятую тайлом. */
static void
hyscan_gtk_map_tiles_tile_free (HyScanGtkMapTile *tile)
{
  g_clear_pointer (&tile->surface, cairo_surface_destroy);
  g_slice_free (HyScanGtkMapTile, tile);
//  g_free (tile);
}

/* Создаёт копию тайла. */
static HyScanGtkMapTile *
hyscan_gtk_map_tiles_tile_copy (HyScanGtkMapTile *tile)
{
  HyScanGtkMapTile *new_tile;

  new_tile = g_slice_new (HyScanGtkMapTile);
//  new_tile = g_new0 (HyScanGtkMapTile, 1);
  new_tile->x = tile->x;
  new_tile->y = tile->y;
  new_tile->zoom = tile->zoom;
  new_tile->surface = cairo_surface_create_similar_image (tile->surface, CAIRO_FORMAT_RGB24, 256, 256);

  return new_tile;
}

/* Отрисовывает отдельный тайл. */
static void
hyscan_gtk_map_tiles_draw_tile (HyScanGtkMapTiles *layer,
                                HyScanGtkMapTile  *tile,
                                GtkCifroArea      *carea,
                                cairo_t           *cairo)
{
  gdouble x, y;

  /* Переводим координаты тайла в координаты для рисования. */
  hyscan_gtk_map_tile_to_point (layer->priv->map, &x, &y, tile->x, tile->y);

  /* Рисуем тайл в нужном месте. */
  cairo_surface_mark_dirty (tile->surface);
  cairo_set_source_surface (cairo, tile->surface, x, y);
  cairo_paint (cairo);
}

/* Рисует тайлы текущей видимой области по сигналу "visible-draw". */
static void
hyscan_gtk_map_tiles_draw (HyScanGtkMapTiles *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  guint x0, y0, xn, yn, x, y;

  guint zoom;

  /* Получаем границы тайлов, покрывающих видимую область. */
  hyscan_gtk_map_get_tile_view_i (priv->map, &x0, &xn, &y0, &yn);
  zoom = hyscan_gtk_map_get_zoom (priv->map);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  for (x = x0; x <= xn; ++x)
    {
      for (y = y0; y <= yn; ++y)
        {
          HyScanGtkMapTile tile;

          tile.surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 256, 256);
          tile.x = x;
          tile.y = y;
          tile.zoom = zoom;

          hyscan_gtk_map_tiles_get_tile (priv, &tile);
          hyscan_gtk_map_tiles_draw_tile (layer, &tile, carea, cairo);

          cairo_surface_destroy (tile.surface);
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