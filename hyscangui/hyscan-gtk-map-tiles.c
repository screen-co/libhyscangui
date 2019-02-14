/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-gtk-map-tile-source.h>
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <math.h>
#include <string.h>

#define CACHE_HEADER_MAGIC     0x308d32d9    /* Идентификатор заголовка кэша. */
#define ZOOM_THRESHOLD         .3            /* Зум округляется вверх, если его дробная часть больше этого значения. */

#define TOTAL_TILES(zoom) ((zoom) == 0 ? 1 : (guint) 2 << (zoom - 1)) - 1
#define CLAMP_TILE(val, zoom) CLAMP((gint)(val), 0, (gint)TOTAL_TILES ((zoom)))

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
  HyScanGtkMap                *map;                 /* Виджет карты, на котором показываются тайлы. */
  HyScanTaskQueue             *task_queue;          /* Очередь задач по созданию тайлов. */

  /* Источник тайлов и его параметры. todo: перенести параметры в HyScanGtkMapTileSource. */
  HyScanGtkMapTileSource      *source;              /* Источник тайлов. */
  guint                        max_zoom;            /* Максимальный масштаб (включительно). */
  guint                        min_zoom;            /* Минимальный масштаб (включительно). */
  guint                        tile_size_real;      /* Реальный размер тайла в пикселах. */

  /* Кэш. */
  HyScanCache                 *cache;               /* Кэш тайлов. */
  HyScanBuffer                *cache_buffer;        /* Буфер заголовка кэша данных. */
  HyScanBuffer                *tile_buffer;         /* Буфер данных поверхности тайла. */
  gchar                       *key;                 /* Ключ кэширования. */
  size_t                       key_length;          /* Длина ключа. */

  /* Поверхность cairo с тайлами видимой области. */
  cairo_surface_t             *surface;             /* Поверхность cairo с тайлами. */
  gboolean                     surface_filled;      /* Признак того, что все тайлы заполнены. */
  guint                        from_x;              /* Координата по оси x левого тайла. */
  guint                        to_x;                /* Координата по оси x правого тайла. */
  guint                        from_y;              /* Координата по оси y верхнего тайла. */
  guint                        to_y;                /* Координата по оси y нижнего тайла. */
  guint                        zoom;                /* Зум отрисованных тайлов. */
  gdouble                      scale;               /* Масштаб отрисованных тайлов. */
  guint                        preload_margin;      /* Количество дополнительно загружаемых тайлов за пределами
                                                     * видимой области с каждой стороны. */
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
static void                 hyscan_gtk_map_tiles_tile_to_point            (HyScanGtkMapTilesPrivate *priv,
                                                                           gdouble                  *x,
                                                                           gdouble                  *y,
                                                                           gdouble                   x_tile,
                                                                           gdouble                   y_tile,
                                                                           guint                     zoom);
static gboolean             hyscan_gtk_map_tiles_is_the_same_scale        (HyScanGtkMapTilesPrivate *priv,
                                                                           gdouble                   scale);
static gboolean             hyscan_gtk_map_tiles_draw_tile                (HyScanGtkMapTilesPrivate *priv,
                                                                           cairo_t                  *cairo,
                                                                           guint                     x,
                                                                           guint                     y);
static cairo_surface_t *    hyscan_gtk_map_tile_surface_make              (HyScanGtkMapTilesPrivate *priv,
                                                                           gboolean                 *filled);
static cairo_surface_t *    hyscan_gtk_map_tile_surface_scale             (cairo_surface_t          *origin,
                                                                           gdouble                   scale);
static guint                hyscan_gtk_map_tiles_get_optimal_zoom         (HyScanGtkMapTilesPrivate *priv);
static gdouble              hyscan_gtk_map_tiles_get_scaling              (HyScanGtkMapTilesPrivate *priv,
                                                                           gdouble                   zoom);
static void                 hyscan_gtk_map_tiles_get_view                 (HyScanGtkMapTiles        *layer,
                                                                           guint                     zoom,
                                                                           gint                     *from_tile_x,
                                                                           gint                     *to_tile_x,
                                                                           gint                     *from_tile_y,
                                                                           gint                     *to_tile_y);
static void                 hyscan_gtk_map_tiles_value_to_tile            (HyScanGtkMapTiles        *layer,
                                                                           guint                     zoom,
                                                                           gdouble                   x,
                                                                           gdouble                   y,
                                                                           gdouble                  *x_tile,
                                                                           gdouble                  *y_tile);
static gboolean             hyscan_gtk_map_tiles_check_cache              (HyScanGtkMapTilesPrivate *priv,
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

  priv->min_zoom = 0;
  priv->max_zoom = 19;
  priv->tile_size_real = 256;
  priv->preload_margin = 3;

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
  g_clear_pointer (&priv->surface, cairo_surface_destroy);
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
      gdk_threads_add_idle ((GSourceFunc) gtk_widget_queue_draw, (GTK_WIDGET (priv->map)));

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

/* Переводит из логической СК в СК тайлов. */
static void
hyscan_gtk_map_tiles_value_to_tile (HyScanGtkMapTiles *layer,
                                    guint              zoom,
                                    gdouble            x,
                                    gdouble            y,
                                    gdouble           *x_tile,
                                    gdouble           *y_tile)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;
  gdouble tile_size_x;
  gdouble tile_size_y;
  gdouble min_x, max_x, max_y, min_y;

  /* Размер тайла в логических единицах. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, &min_y, &max_y);
  tile_size_x = (max_x - min_x) / pow (2, zoom);
  tile_size_y = (max_y - min_y) / pow (2, zoom);

  (y_tile != NULL) ? *y_tile = (max_y - y) / tile_size_y : 0;
  (x_tile != NULL) ? *x_tile = (x - min_x) / tile_size_x : 0;
}

/* Получает целочисленные координаты верхнего левого и правого нижнего тайлов,
 * полностью покрывающих видимую область. */
static void
hyscan_gtk_map_tiles_get_view (HyScanGtkMapTiles *layer,
                               guint              zoom,
                               gint              *from_tile_x,
                               gint              *to_tile_x,
                               gint              *from_tile_y,
                               gint              *to_tile_y)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;
  gdouble from_x, to_x, from_y, to_y;
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  /* Получаем тайлы, соответствующие границам видимой части карты. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_tiles_value_to_tile (layer, zoom, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_gtk_map_tiles_value_to_tile (layer, zoom, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  (to_tile_y != NULL) ? *to_tile_y = (gint) MAX (from_tile_y_d, to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = (gint) MIN (from_tile_y_d, to_tile_y_d) : 0;

  (to_tile_x != NULL) ? *to_tile_x = (gint) MAX (from_tile_x_d, to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = (gint) MIN (from_tile_x_d, to_tile_x_d) : 0;
}

/* Растяжение тайла при текущем масштабе карты и указанном зуме. */
static gdouble
hyscan_gtk_map_tiles_get_scaling (HyScanGtkMapTilesPrivate *priv,
                                  gdouble                   zoom)
{
  gdouble pixel_size;
  gdouble tile_size;
  gdouble min_x, max_x;

  /* Размер пиксела в логических единицах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &pixel_size, NULL);

  /* Размер тайла в логических единицах. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  tile_size = (max_x - min_x) / pow (2, zoom);

  return tile_size / (pixel_size * priv->tile_size_real);
}

/* Устанавливает подходящий zoom в зависимости от выбранного масштаба. */
static guint
hyscan_gtk_map_tiles_get_optimal_zoom (HyScanGtkMapTilesPrivate *priv)
{
  gdouble scale;
  gdouble optimal_zoom;
  guint izoom;
  gdouble tile_size;
  gdouble min_x, max_x;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);

  /* Целевой размер тайла в логических единицах, чтобы тайлы не растягивались. */
  tile_size = priv->tile_size_real * scale;

  /* Подбираем зум, чтобы тайлы были нужного размера...
   * tile_size = area_limits / pow (2, zoom). */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  optimal_zoom = log2 ((max_x - min_x) / tile_size);

  /* ... и делаем зум целочисленным. */
  izoom = (guint) optimal_zoom;
  izoom = (optimal_zoom - izoom) > ZOOM_THRESHOLD ? izoom + 1 : izoom;

  return CLAMP (izoom, priv->min_zoom, priv->max_zoom);
}

/* Переводит координаты тайла в систему координат видимой области. */
static void
hyscan_gtk_map_tiles_tile_to_point (HyScanGtkMapTilesPrivate *priv,
                                    gdouble                  *x,
                                    gdouble                  *y,
                                    gdouble                   x_tile,
                                    gdouble                   y_tile,
                                    guint                     zoom)
{
  gdouble x_val, y_val;
  gdouble tiles_count;

  gdouble min_x, max_x, max_y, min_y;

  /* Получаем координаты тайла в логической СК. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, &min_y, &max_y);
  tiles_count = pow (2, zoom);
  y_val = (min_y - max_y) / tiles_count * y_tile + max_y;
  x_val = (max_x - min_x) / tiles_count * x_tile + min_x;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), x, y, x_val, y_val);
}

/* Проверяет, что scale и priv->scale дают один и тот же размер поверхности. */
static gboolean
hyscan_gtk_map_tiles_is_the_same_scale (HyScanGtkMapTilesPrivate *priv,
                                        gdouble                   scale)
{
  guint tile_size;

  guint width;
  guint height;

  tile_size = priv->tile_size_real;

  width = (guint) tile_size * (priv->to_x - priv->from_x + 1);
  height = (guint) tile_size * (priv->to_y - priv->from_y + 1);

  return (gint) (width / scale) == (gint) (width / priv->scale) &&
         (gint) (height / scale) == (gint) (height / priv->scale);
}

/* Риусет тайл (x, y) в контексте cairo. */
static gboolean
hyscan_gtk_map_tiles_draw_tile (HyScanGtkMapTilesPrivate *priv,
                                cairo_t                  *cairo,
                                guint                     x,
                                guint                     y)
{
  HyScanGtkMapTile *tile;

  guint tile_size;
  gboolean filled;

  guint x0;
  guint y0;
  guint zoom;

  x0 = priv->from_x;
  y0 = priv->from_y;
  zoom = priv->zoom;

  tile_size = priv->tile_size_real;

  tile = hyscan_gtk_map_tile_new (x, y, zoom, tile_size);

  /* Если получилось заполнить тайл, то рисуем его в буфер. */
  filled = hyscan_gtk_map_tiles_fill_tile (priv, tile);
  if (filled)
    {
      cairo_surface_t *tile_surface;

      tile_surface = hyscan_gtk_map_tile_get_surface (tile);
      cairo_set_source_surface (cairo, tile_surface, (x - x0) * tile_size, (y - y0) * tile_size);
      cairo_paint (cairo);
    }

  /* Номер тайла для отладки. */
  if (FALSE)
    {
      gchar label[255];

      cairo_move_to (cairo, (x - x0) * tile_size, (y - y0) * tile_size);
      cairo_set_source_rgb (cairo, 0, 0, 0);
      g_snprintf (label, sizeof (label), "%d, %d", x, y);
      cairo_show_text (cairo, label);
    }

  g_object_unref (tile);

  return filled;
}

/* Создаёт новую поверхность cairo с тайлами
 * и устанавливает filled = TRUE, если все тайлы заполнены. */
static cairo_surface_t *
hyscan_gtk_map_tile_surface_make (HyScanGtkMapTilesPrivate *priv,
                                  gboolean                 *filled)
{
  cairo_surface_t *surface;
  cairo_t *cairo;

  guint x;
  guint y;

  gboolean filled_ret = TRUE;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (priv->to_x - priv->from_x + 1) * priv->tile_size_real,
                                        (priv->to_y - priv->from_y + 1) * priv->tile_size_real);
  cairo = cairo_create (surface);

  /* Очищаем очередь загрузки тайлов, которую мы сформировали в прошлый раз. */
  hyscan_task_queue_clear (priv->task_queue);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  for (x = priv->from_x; x <= priv->to_x; ++x)
    {
      for (y = priv->from_y; y <= priv->to_y; ++y)
        {
          filled_ret = hyscan_gtk_map_tiles_draw_tile (priv, cairo, x, y) && filled_ret;
        }
    }

  cairo_destroy (cairo);

  *filled = filled_ret;
  return surface;
}

/* Создаёт новую поверхность cairo с растянутым изображением origin. */
static cairo_surface_t *
hyscan_gtk_map_tile_surface_scale (cairo_surface_t *origin,
                                   gdouble          scale)
{
  cairo_surface_t *surface;
  cairo_t *cairo;

  guint width;
  guint height;

  width = cairo_image_surface_get_width (origin);
  height = cairo_image_surface_get_height (origin);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (guint) fabs (width * scale),
                                        (guint) fabs (height * scale));

  cairo = cairo_create (surface);
  cairo_scale (cairo, scale, scale);
  cairo_set_source_surface (cairo, origin, 0, 0);

  /* Делаем быстрый билинейный ресемплинг изображения, чтобы gui не тормозил. */
  cairo_pattern_set_filter (cairo_get_source (cairo), CAIRO_FILTER_BILINEAR);
  cairo_paint (cairo);

  cairo_destroy (cairo);

  return surface;
}

/* Обновляет поверхность cairo, чтобы она содержала запрошенную область с тайлами. */
static void
hyscan_gtk_map_refresh_surface (HyScanGtkMapTilesPrivate *priv,
                                guint                     x0,
                                guint                     xn,
                                guint                     y0,
                                guint                     yn,
                                guint                     zoom)
{
  cairo_surface_t *surface;
  gdouble scale;

  g_return_if_fail (x0 <= xn && y0 <= yn);

  scale = hyscan_gtk_map_tiles_get_scaling (priv, zoom);

  /* Если нужный регион уже отрисован, то ничего не делаем. */
  if (priv->surface_filled &&
      hyscan_gtk_map_tiles_is_the_same_scale (priv, scale) &&
      priv->from_x <= x0 && priv->to_x >= xn &&
      priv->from_y <= y0 && priv->to_y >= yn &&
      priv->zoom == zoom)
    {
      return;
    }

  /* Устанавливаем новые размеры поверхности с запасом preload_margin. */
  priv->from_x = CLAMP_TILE (x0 - priv->preload_margin, zoom);
  priv->to_x = CLAMP_TILE (xn + priv->preload_margin, zoom);
  priv->from_y = CLAMP_TILE (y0 - priv->preload_margin, zoom);
  priv->to_y = CLAMP_TILE (yn + priv->preload_margin, zoom);
  priv->zoom = zoom;
  priv->scale = scale;

  /* Рисуем все нужные тайлы в их исходном размере. */
  surface = hyscan_gtk_map_tile_surface_make (priv, &priv->surface_filled);

  /* Затем делаем ресемплинг изображения. */
  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  priv->surface = hyscan_gtk_map_tile_surface_scale (surface, scale);

  cairo_surface_destroy (surface);
}

/* Рисует тайлы текущей видимой области по сигналу "visible-draw". */
static void
hyscan_gtk_map_tiles_draw (HyScanGtkMapTiles *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  gint x0, xn, y0, yn;
  guint zoom;

  gdouble time;

  g_test_timer_start ();

  /* Устанавливаем подходящий zoom для видимой области. */
  zoom = hyscan_gtk_map_tiles_get_optimal_zoom (priv);

  /* Получаем границы тайлов, покрывающих видимую область. */
  hyscan_gtk_map_tiles_get_view (layer, zoom, &x0, &xn, &y0, &yn);

  /* Берём только валидные номера тайлов: от 0 до 2^zoom - 1. */
  x0 = CLAMP_TILE (x0, zoom);
  y0 = CLAMP_TILE (y0, zoom);
  xn = CLAMP_TILE (xn, zoom);
  yn = CLAMP_TILE (yn, zoom);

  hyscan_gtk_map_refresh_surface (priv, x0, xn, y0, yn, zoom);

  /* Растягиваем буфер под необходимый размер тайла и переносим его на поверхность виджета. */
  {
    gdouble xs, ys;

    hyscan_gtk_map_tiles_tile_to_point (priv, &xs, &ys, priv->from_x, priv->from_y, zoom);
    cairo_set_source_surface (cairo, priv->surface, xs, ys);
    cairo_paint (cairo);
  }

  time = g_test_timer_elapsed ();
  g_message ("Draw tiles fps %.1f", 1.0 / time);
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

/**
 * hyscan_gtk_map_set_zoom:
 * @map: указатель на #HyScanGtkMap
 * @zoom: масштаб карты (от 1 до 19)
 *
 * Устанавливает новый масштаб карты.
 */
void
hyscan_gtk_map_tiles_set_zoom (HyScanGtkMapTiles *layer,
                               guint              zoom)
{
  HyScanGtkMapTilesPrivate *priv;

  gdouble tile_size;
  gdouble scale;

  gdouble x0_value, y0_value, xn_value, yn_value;
  gdouble x, y;
  gdouble min_x, max_x;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILES (layer));

  priv = layer->priv;
  if (zoom < priv->min_zoom || zoom > priv->max_zoom)
    {
      g_warning ("HyScanGtkMap: Zoom must be in interval from %d to %d", priv->min_zoom, priv->max_zoom);
      return;
    }

  /* Получаем координаты центра в логических координатах. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &x0_value, &xn_value, &y0_value, &yn_value);
  x = (x0_value + xn_value) / 2;
  y = (y0_value + yn_value) / 2;

  /* Определяем размер тайла в логических координатах. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  tile_size = (max_x - min_x) / pow (2, zoom);

  /* Расcчитываем scale, чтобы тайлы не были растянутыми. */
  scale = tile_size / priv->tile_size_real;

  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), scale, scale, x, y);
}
