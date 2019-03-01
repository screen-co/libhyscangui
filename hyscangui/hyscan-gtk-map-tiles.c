/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Раскомментируйте следующую строку для вывода отладочной информации. */
/* #define HYSCAN_GTK_MAP_TILES_DEBUG */

#define CACHE_HEADER_MAGIC     0x308d32d9    /* Идентификатор заголовка кэша. */
#define ZOOM_THRESHOLD         .3            /* Зум округляется вверх, если его дробная часть больше этого значения. */

#define TOTAL_TILES(zoom)      ((zoom) == 0 ? 1 : 2u << (zoom - 1)) - 1
#define CLAMP_TILE(val, zoom)  CLAMP((gint)(val), 0, (gint)TOTAL_TILES ((zoom)))

enum
{
  PROP_O,
  PROP_CACHE,
  PROP_SOURCE,
  PROP_PRELOAD_MARGIN,
};

/* Структруа заголовка кэша: содержит данные для gdk_pixbuf_new_from_data(). */
typedef struct
{
  guint32                      magic;               /* Идентификатор заголовка. */
  
  gsize                        size;                /* Размер пиксельных данных. */
  gboolean                     has_alpha;           /* Есть ли альфа-канал? */
  gint                         bits_per_sample;     /* Количество битов на точку. */
  gint                         width;               /* Ширина изображения. */
  gint                         height;              /* Высота изображения. */
  gint                         rowstride;           /* Количество байтов в одной строке изображения. */
} HyScanGtkMapTilesCacheHeader;

struct _HyScanGtkMapTilesPrivate
{
  HyScanGtkMap                *map;                 /* Виджет карты, на котором показываются тайлы. */
  HyScanTaskQueue             *task_queue;          /* Очередь задач по созданию тайлов. */
  HyScanGtkMapTileSource      *source;              /* Источник тайлов. */

  /* Кэш. */
  HyScanCache                 *cache;               /* Кэш тайлов. */
  HyScanBuffer                *cache_buffer;        /* Буфер заголовка кэша данных для чтения. */
  HyScanBuffer                *tile_buffer;         /* Буфер данных поверхности тайла для чтения. */

  GList                       *filled_buffer;       /* Список заполненных тайлов, которые можно нарисовать. */
  GMutex                       filled_lock;         /* Блокировка доступа к filled_buffer. */

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
static void                 hyscan_gtk_map_tiles_interface_init           (HyScanGtkLayerInterface  *iface);
static void                 hyscan_gtk_map_tiles_object_constructed       (GObject                  *object);
static void                 hyscan_gtk_map_tiles_object_finalize          (GObject                  *object);
static void                 hyscan_gtk_map_tiles_draw                     (HyScanGtkMapTiles        *layer,
                                                                           cairo_t                  *cairo);
static void                 hyscan_gtk_map_tiles_load                     (HyScanGtkMapTile         *tile,
                                                                           HyScanGtkMapTiles        *layer);
static gboolean             hyscan_gtk_map_tiles_filled_buffer_flush      (HyScanGtkMapTiles        *layer);
static gboolean             hyscan_gtk_map_tiles_buffer_is_visible        (HyScanGtkMapTiles        *layer);
static void                 hyscan_gtk_map_tiles_get_cache_key            (HyScanGtkMapTile         *tile,
                                                                           gchar                    *key,
                                                                           gsize                     key_length);
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
static gboolean             hyscan_gtk_map_tiles_cache_get                (HyScanGtkMapTilesPrivate *priv,
                                                                           HyScanGtkMapTile         *tile);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTiles, hyscan_gtk_map_tiles, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTiles)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_tiles_interface_init))

static void
hyscan_gtk_map_tiles_class_init (HyScanGtkMapTilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiles_set_property;

  object_class->constructed = hyscan_gtk_map_tiles_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiles_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "Cache object",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SOURCE,
    g_param_spec_object ("source", "Tile source", "GtkMapTileSource object",
                         HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PRELOAD_MARGIN,
    g_param_spec_uint ("preload-margin", "Preload margin", "GtkMapTileSource object", 0, G_MAXUINT, 0,
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
    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PRELOAD_MARGIN:
      priv->preload_margin = g_value_get_uint (value);
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

  g_mutex_init (&priv->filled_lock);
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
  g_clear_object (&priv->map);
  g_object_unref (priv->task_queue);
  g_object_unref (priv->source);
  g_object_unref (priv->cache);
  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->tile_buffer);
  g_mutex_clear (&priv->filled_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_parent_class)->finalize (object);
}

/* Реализация HyScanGtlLayerInterface.added() - подключается к сигналам виджета карты. */
static void
hyscan_gtk_map_tiles_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTilesPrivate *priv = HYSCAN_GTK_MAP_TILES (gtk_layer)->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  priv->map = g_object_ref (container);

  /* Подключаемся к сигналу обновления видимой области карты. */
  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_tiles_draw), gtk_layer);
}

static void
hyscan_gtk_map_tiles_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->get_visible = NULL;
  iface->set_visible = NULL;
  iface->added = hyscan_gtk_map_tiles_added;
  iface->grab_input = NULL;
  iface->get_icon_name = NULL;
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_tiles_cache_set (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  HyScanBuffer *header_buffer;
  HyScanGtkMapTilesCacheHeader header;

  HyScanBuffer *data_buffer;
  const guint8 *data;
  guint dsize;

  gchar cache_key[255];

  GdkPixbuf *pixbuf;

  /* Будем помещать в кэш изображение pixbuf. */
  pixbuf = hyscan_gtk_map_tile_get_pixbuf (tile);

  /* Не получится использовать буферы объекта, т.к. к ним идёт доступ из разных потоков. */
  data_buffer = hyscan_buffer_new ();
  header_buffer = hyscan_buffer_new ();

  /* Оборачиваем в буфер пиксельные данные. */
  dsize = gdk_pixbuf_get_byte_length (pixbuf);
  data = gdk_pixbuf_read_pixels (pixbuf);
  hyscan_buffer_wrap_data (data_buffer, HYSCAN_DATA_BLOB, (gpointer) data, dsize);

  /* Оборачиваем в буфер заголовок с информацией об изображении. */
  header.magic = CACHE_HEADER_MAGIC;
  header.size = dsize;
  header.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  header.rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  header.height = gdk_pixbuf_get_height (pixbuf);
  header.width = gdk_pixbuf_get_width (pixbuf);
  header.bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  hyscan_buffer_wrap_data (header_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Помещаем все данные в кэш. */
  hyscan_gtk_map_tiles_get_cache_key (tile, cache_key, sizeof (cache_key));
  hyscan_cache_set2 (priv->cache, cache_key, NULL, header_buffer, data_buffer);

  g_object_unref (header_buffer);
  g_object_unref (data_buffer);
}

/* Проверяет, есть ли в буфере тайлы, которые сейчас видны. */
static gboolean
hyscan_gtk_map_tiles_buffer_is_visible (HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  GList *tile_l;

  gint from_x;
  gint from_y;
  gint to_x;
  gint to_y;

  if (priv->filled_buffer == NULL)
    return FALSE;

  /* Определяем видимую область. */
  hyscan_gtk_map_tiles_get_view (layer, priv->zoom, &from_x, &to_x, &from_y, &to_y);

  /* Проверяем, есть ли в буфере тайлы из видимой области. */
  for (tile_l = priv->filled_buffer; tile_l != NULL; tile_l = tile_l->next)
    {
      HyScanGtkMapTile *tile = tile_l->data;

      gint x;
      gint y;

      guint zoom;

      /* Пропускаем тайлы другого зума. */
      zoom = hyscan_gtk_map_tile_get_zoom (tile);
      if (zoom != priv->zoom)
        continue;

      x = hyscan_gtk_map_tile_get_x (tile);
      y = hyscan_gtk_map_tile_get_y (tile);
      if (from_x <= x && x <= to_x && from_y <= y && y <= to_y)
        return TRUE;
    }

  return FALSE;
}

/* Запрашивает перерисовку виджета карты, если пришёл хотя бы один тайл из видимой области. */
static gboolean
hyscan_gtk_map_tiles_filled_buffer_flush (HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  g_mutex_lock (&priv->filled_lock);

  if (hyscan_gtk_map_tiles_buffer_is_visible (layer))
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_list_free_full (priv->filled_buffer, g_object_unref);
  priv->filled_buffer = NULL;

  g_mutex_unlock (&priv->filled_lock);

  return G_SOURCE_REMOVE;
}

/* Заполняет запрошенный тайл из источника тайлов (выполняется в отдельном потоке). */
static void
hyscan_gtk_map_tiles_load (HyScanGtkMapTile  *tile,
                           HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  /* Если удалось заполнить новый тайл, то запрашиваем обновление области
   * виджета с новым тайлом. */
  if (hyscan_gtk_map_tile_source_fill (priv->source, tile))
    {
      hyscan_gtk_map_tiles_cache_set (priv, tile);

      /* Добавляем тайл в буфер заполненных тайлов. */
      g_mutex_lock (&priv->filled_lock);
      priv->filled_buffer = g_list_append (priv->filled_buffer, g_object_ref (tile));
      g_mutex_unlock (&priv->filled_lock);

      gdk_threads_add_idle ((GSourceFunc) hyscan_gtk_map_tiles_filled_buffer_flush, layer);
    }
}

/* Устанавливает ключ кэширования для тайла @tile. */
static void
hyscan_gtk_map_tiles_get_cache_key (HyScanGtkMapTile *tile,
                                    gchar            *key,
                                    gsize             key_length)
{
  g_snprintf (key, key_length,
              "tile.%u.%u.%u.%u",
              hyscan_gtk_map_tile_get_zoom (tile),
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_size (tile));
}

/* Копирует пиксельные данные изображения pixbuf.
 * см. hyscan_gtk_map_tiles_tile_data_destroy(). */
static guchar *
hyscan_gtk_map_tiles_tile_data_copy (guchar   *data,
                                     gsize     block_size)
{
  guchar *new_data;
  new_data = g_slice_copy (block_size, data);

  return new_data;
}

/* Освобождает память из-под данных изображения pixbuf.
 * см. hyscan_gtk_map_tiles_tile_data_destroy(). */
static void
hyscan_gtk_map_tiles_tile_data_destroy (guchar   *data,
                                        gpointer  block_size)
{
  g_slice_free1 (GPOINTER_TO_SIZE (block_size), data);
}

/* Функция проверяет кэш на наличие данных и считывает их. */
static gboolean
hyscan_gtk_map_tiles_cache_get (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  HyScanGtkMapTilesCacheHeader header;
  guchar *cached_data;
  guint32 size;

  gchar cache_key[255];

  GdkPixbuf *pixbuf;

  if (priv->cache == NULL)
    return FALSE;

  /* Формируем ключ кэшированных данных. */
  hyscan_gtk_map_tiles_get_cache_key (tile, cache_key, sizeof (cache_key));

  /* Ищем данные в кэше. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, cache_key, NULL, sizeof (header), priv->cache_buffer, priv->tile_buffer))
    return FALSE;

  /* Верификация данных. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.size != hyscan_buffer_get_size (priv->tile_buffer)))
    {
      return FALSE;
    }

  /* Установка в тайл данных изображения. */
  cached_data = hyscan_buffer_get_data (priv->tile_buffer, &size);
  pixbuf = gdk_pixbuf_new_from_data (hyscan_gtk_map_tiles_tile_data_copy (cached_data, size),
                                     GDK_COLORSPACE_RGB,
                                     header.has_alpha,
                                     header.bits_per_sample,
                                     header.width,
                                     header.height,
                                     header.rowstride,
                                     hyscan_gtk_map_tiles_tile_data_destroy, GSIZE_TO_POINTER (size));
  hyscan_gtk_map_tile_set_pixbuf (tile, pixbuf);
  g_object_unref (pixbuf);

  return TRUE;
}

/* Загружает изображение тайла.
 * Возвращает TRUE, если тайл загружен. Иначе FALSE. */
static gboolean
hyscan_gtk_map_tiles_fill_tile (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  gboolean found;

  found = hyscan_gtk_map_tiles_cache_get (priv, tile);

  /* Тайл не найден, добавляем его в очередь на загрузку. */
  if (!found)
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
  guint tile_size_px;
  gdouble min_x, max_x;

  /* Размер тайла в логических единицах. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  tile_size = (max_x - min_x) / pow (2, zoom);

  /* Размер тайла в пикселах. */
  tile_size_px = hyscan_gtk_map_tile_source_get_tile_size (priv->source);

  /* Размер пиксела в логических единицах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &pixel_size, NULL);

  return tile_size / (tile_size_px * pixel_size);
}

/* Устанавливает подходящий zoom в зависимости от выбранного масштаба. */
static guint
hyscan_gtk_map_tiles_get_optimal_zoom (HyScanGtkMapTilesPrivate *priv)
{
  gdouble scale;
  gdouble optimal_zoom;
  guint izoom;

  guint min_zoom;
  guint max_zoom;

  gdouble tile_size;
  gdouble min_x, max_x;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);

  /* Целевой размер тайла в логических единицах, чтобы тайлы не растягивались. */
  tile_size = hyscan_gtk_map_tile_source_get_tile_size (priv->source);
  tile_size = tile_size * scale;

  /* Подбираем зум, чтобы тайлы были нужного размера...
   * tile_size = area_limits / pow (2, zoom). */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  optimal_zoom = log2 ((max_x - min_x) / tile_size);

  /* ... и делаем зум целочисленным. */
  izoom = (guint) optimal_zoom;
  izoom = (optimal_zoom - izoom) > ZOOM_THRESHOLD ? izoom + 1 : izoom;

  hyscan_gtk_map_tile_source_get_zoom_limits (priv->source, &min_zoom, &max_zoom);

  return CLAMP (izoom, min_zoom, max_zoom);
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

  tile_size = hyscan_gtk_map_tile_source_get_tile_size (priv->source);

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

  tile_size = hyscan_gtk_map_tile_source_get_tile_size (priv->source);
  tile = hyscan_gtk_map_tile_new (x, y, zoom, tile_size);

  /* Если получилось заполнить тайл, то рисуем его в буфер. */
  filled = hyscan_gtk_map_tiles_fill_tile (priv, tile);
  if (filled)
    {
      GdkPixbuf *pixbuf;

      pixbuf = hyscan_gtk_map_tile_get_pixbuf (tile);
      gdk_cairo_set_source_pixbuf (cairo, pixbuf, (x - x0) * tile_size, (y - y0) * tile_size);
      cairo_paint (cairo);
    }

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  /* Номер тайла для отладки. */
  {
    gchar label[255];

    cairo_move_to (cairo, (x - x0) * tile_size, (y - y0) * tile_size);
    cairo_set_source_rgb (cairo, 0, 0, 0);
    g_snprintf (label, sizeof (label), "%d, %d", x, y);
    cairo_show_text (cairo, label);
  }
#endif

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

  guint tile_size;

  gboolean filled_ret = TRUE;

  tile_size = hyscan_gtk_map_tile_source_get_tile_size (priv->source);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (priv->to_x - priv->from_x + 1) * tile_size,
                                        (priv->to_y - priv->from_y + 1) * tile_size);
  cairo = cairo_create (surface);

  /* Очищаем очередь загрузки тайлов, которую мы сформировали в прошлый раз. */
  hyscan_task_queue_clear (priv->task_queue);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  {
    gint r;
    gint max_r;
    gint xc, yc;
    gint x;
    gint y;

    /* Определяем центр области и максимальное расстояние до ее границ. */
    xc = (priv->to_x + priv->from_x + 1) / 2;
    yc = (priv->to_y + priv->from_y + 1) / 2;
    max_r = MAX (MAX (priv->to_x - xc, xc - priv->from_x),
                 MAX (priv->to_y - yc, yc - priv->from_y));

    /* Рисуем тайлы двигаясь из центра (xc, yc) к краям. */
    for (r = 0; r <= max_r; ++r)
      {
        for (x = priv->from_x; x <= (gint) priv->to_x; ++x)
          {
            for (y = priv->from_y; y <= (gint) priv->to_y; ++y)
              {
                /* Пока пропускаем тайлы снаружи радиуса. */
                if (abs (y - yc) > r || abs (x - xc) > r)
                  continue;

                /* И рисуем тайлы на расстоянии r от центра по одной из координат. */
                if (abs (x - xc) != r && abs (y - yc) != r)
                  continue;

                filled_ret = hyscan_gtk_map_tiles_draw_tile (priv, cairo, x, y) && filled_ret;
              }
          }
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

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  gdouble time;

  g_test_timer_start ();
#endif

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

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  time = g_test_timer_elapsed ();
  g_message ("Draw tiles fps %.1f", 1.0 / time);
#endif
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
hyscan_gtk_map_tiles_new (HyScanCache             *cache,
                          HyScanGtkMapTileSource  *source)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILES,
                       "cache", cache,
                       "source", source, NULL);
}

/**
 * hyscan_gtk_map_set_zoom:
 * @map: указатель на #HyScanGtkMap
 * @zoom: степень детализации карты
 *
 * Устанавливает новый уровень детализации карты. Допустимые значения @zoom определяются
 * используемым источником тайлов hyscan_gtk_map_tile_source_get_zoom_limits().
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

  guint min_zoom;
  guint max_zoom;
  guint tile_size_real;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILES (layer));

  priv = layer->priv;

  hyscan_gtk_map_tile_source_get_zoom_limits (priv->source, &min_zoom, &max_zoom);
  if (zoom < min_zoom || zoom > max_zoom)
    {
      g_warning ("HyScanGtkMap: Zoom must be in interval from %d to %d", min_zoom, max_zoom);
      return;
    }

  /* Получаем координаты центра в логических координатах. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &x0_value, &xn_value, &y0_value, &yn_value);
  x = (x0_value + xn_value) / 2;
  y = (y0_value + yn_value) / 2;

  /* Определяем размер тайла в логических координатах. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &min_x, &max_x, NULL, NULL);
  tile_size = (max_x - min_x) / pow (2, zoom);

  /* Определяем размер тайла в пикселах. */
  tile_size_real = hyscan_gtk_map_tile_source_get_tile_size (priv->source);

  /* Расcчитываем scale, чтобы тайлы не были растянутыми. */
  scale = tile_size / tile_size_real;

  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), scale, scale, x, y);
}
