/* hyscan-gtk-map-tiled-layer.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-gtk-map-tiled-layer
 * @Short_description: Класс тайлового слоя
 * @Title: HyScanGtkMapTiledLayer
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap, #GtkCifroArea
 *
 * Класс реализует интерфейса слоя #HyScanGtkLayer и предназначен для изображения
 * вычислительно сложного слоя с изменяемым содержимым в виде тайлов.
 *
 * Слой разбивает всю область рисования на тайлы и рисует изображаемую область
 * потайлово. Заполнение тайлов выполняет только при изменении параметров слоя
 * или входных данных. В остальное время слой получает тайлы из кэша, и таким
 * образом экономит вычислительные ресурсы.
 *
 * Для использования тайлового слоя необходимо унаследовать класс #HyScanGtkMapTiledLayer
 * и реализовать функцию класса #HyScanGtkMapTiledLayerClass.fill_tile.
 *
 * Класс предоставляет следующие функции:
 * - hyscan_gtk_map_tiled_layer_draw() - рисует текущую видимую область;
 * - hyscan_gtk_map_tiled_layer_set_area_mod() - фиксирует факт изменения указанной области;
 * - hyscan_gtk_map_tiled_layer_set_param_mod() - фиксирует факт изменения параметров слоя;
 * - hyscan_gtk_map_tiled_layer_request_draw() - запрашивает перерисовку слоя.
 *
 */

#include "hyscan-gtk-map-tiled-layer.h"
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <hyscan-buffer.h>

#define CACHE_TRACK_MOD_MAGIC  0x4d546d64    /* Идентификатор заголовка кэша. */
#define CACHE_HEADER_MAGIC     0xcfadec2d    /* Идентификатор заголовка кэша. */
#define CACHE_KEY_LEN          127           /* Длина ключа кэша. */
#define TILE_SIZE              256           /* Размер тайла. */

/* Раскомментируйте строку ниже для вывода отладочной информации на тайлах. */
// #define DEBUG_TILES

enum
{
  PROP_O,
  PROP_CACHE
};

typedef enum
{
  STATE_ACTUAL,                    /* Тайл в актуальном состоянии. */
  STATE_OUTDATED,                  /* Тайл не в актуальном состоянии, но можно показывать. */
  STATE_IRRELEVANT                 /* Тайл не в актуальном состоянии, нельзя показывать. */
} HyScanGtkMapTiledLayerModState;

/* Структура с информацией о последнем изменении трека внутри некоторого тайла. */
typedef struct
{
  guint32                              magic;          /* Идентификатор заголовка. */
  guint64                              mod;            /* Номер изменения трека. */
} HyScanGtkMapTiledLayerMod;

typedef struct
{
  guint32                              magic;          /* Идентификатор заголовка. */
  guint                                mod;            /* Номер изменения трека в момент отрисовки тайла. */
  gsize                                size;           /* Размер данных поверхности. */
} HyScanGtkMapTiledLayerCacheHeader;

struct _HyScanGtkMapTiledLayerPrivate
{
  HyScanGtkMap                 *map;                        /* Виджет карты, на котором размещен слой. */
  HyScanGtkMapTileGrid         *tile_grid;                  /* Тайловая сетка. */
  HyScanTaskQueue              *task_queue;                 /* Очередь по загрузке тайлов. */
  HyScanCache                  *cache;                      /* Кэш тайлов. */
  HyScanBuffer                 *tile_buffer;                /* Буфер для получения поверхности тайла. */
  HyScanBuffer                 *cache_buffer;               /* Буфер для получения заголовка кэша. */
  gchar                         cache_key[CACHE_KEY_LEN];   /* Ключ кэша. */

  guint                         mod_count;                  /* Номер изменения данных. */
  guint                         param_mod_count;            /* Номер изменения в параметрах отображения слоя. */

  gboolean                      redraw;                     /* Признак необходимости перерисовки. */
  guint                         redraw_tag;                 /* Тэг функции, которая запрашивает перерисовку. */
};

static void    hyscan_gtk_map_tiled_layer_interface_init           (HyScanGtkLayerInterface *iface);
static void    hyscan_gtk_map_tiled_layer_set_property             (GObject                *object,
                                                                    guint                   prop_id,
                                                                    const GValue           *value,
                                                                    GParamSpec             *pspec);
static void    hyscan_gtk_map_tiled_layer_object_constructed       (GObject                *object);
static void    hyscan_gtk_map_tiled_layer_object_finalize          (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTiledLayer, hyscan_gtk_map_tiled_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapTiledLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_tiled_layer_interface_init))

static void
hyscan_gtk_map_tiled_layer_class_init (HyScanGtkMapTiledLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiled_layer_set_property;

  object_class->constructed = hyscan_gtk_map_tiled_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiled_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Tile cache", "HyScanCache",
                          HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tiled_layer_init (HyScanGtkMapTiledLayer *gtk_map_tiled_layer)
{
  gtk_map_tiled_layer->priv = hyscan_gtk_map_tiled_layer_get_instance_private (gtk_map_tiled_layer);
}

static void
hyscan_gtk_map_tiled_layer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tiled_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tiled_layer_parent_class)->constructed (object);

  priv->cache_buffer = hyscan_buffer_new ();
  priv->tile_buffer = hyscan_buffer_new ();
}

static void
hyscan_gtk_map_tiled_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  g_clear_object (&priv->cache);
  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->tile_buffer);

  G_OBJECT_CLASS (hyscan_gtk_map_tiled_layer_parent_class)->finalize (object);
}

/* Ключ кэширования. */
static void
hyscan_gtk_map_tiled_layer_tile_cache_key (HyScanGtkMapTiledLayer *tiled_layer,
                                           HyScanGtkMapTile       *tile,
                                           gchar                  *key,
                                           gulong                  key_len)
{
  // todo: add id of layer
  g_snprintf (key, key_len,
              "GtkMapTiledLayer.%u.%d.%d.%u",
              g_atomic_int_get (&tiled_layer->priv->param_mod_count),
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_zoom (tile));
}

static void
hyscan_gtk_map_tiled_layer_mod_cache_key (HyScanGtkMapTiledLayer *tiled_layer,
                                          guint                   x,
                                          guint                   y,
                                          guint                   z,
                                          gchar                  *key,
                                          gulong                  key_len)
{
  // todo: add id of layer
  g_snprintf (key, key_len,
              "GtkMapTiledLayer.mc.%d.%d.%d.%d",
              g_atomic_int_get (&tiled_layer->priv->param_mod_count),
              x, y, z);
}

/* Возвращает статус актуальности тайла. */
static HyScanGtkMapTiledLayerModState
hyscan_gtk_map_tiled_layer_get_mod_actual (HyScanGtkMapTiledLayer *tiled_layer,
                                           HyScanGtkMapTile       *tile,
                                           guint                   mod)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  gboolean found;
  gchar mod_cache_key[127];
  guint x, y, z;
  HyScanBuffer *buffer;

  HyScanGtkMapTiledLayerMod last_update;
  gint state;

  x = hyscan_gtk_map_tile_get_x (tile);
  y = hyscan_gtk_map_tile_get_y (tile);
  z = hyscan_gtk_map_tile_get_zoom (tile);

  hyscan_gtk_map_tiled_layer_mod_cache_key (tiled_layer, x, y, z, mod_cache_key, sizeof (mod_cache_key));

  buffer = hyscan_buffer_new ();
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, &last_update, sizeof (last_update));

  found = hyscan_cache_get (priv->cache, mod_cache_key, NULL, buffer);

  /* Если кэш не попал, то запишем в него значение, которое сделает тайл неактуальным. */
  if (!found || last_update.magic != CACHE_TRACK_MOD_MAGIC)
    {
      last_update.magic = CACHE_TRACK_MOD_MAGIC;
      last_update.mod = mod;
      hyscan_cache_set (priv->cache, mod_cache_key, NULL, buffer);
    }

  if (last_update.mod > mod + 5)
    state = STATE_IRRELEVANT;
  else if (last_update.mod > mod)
    state = STATE_OUTDATED;
  else
    state = STATE_ACTUAL;

  g_object_unref (buffer);

  return state;
}

/* Получает из кэша пискельные данные тайла и записывает их в tile_buffer. */
static gboolean
hyscan_gtk_map_tiled_layer_cache_get (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanGtkMapTile       *tile,
                                      HyScanBuffer           *tile_buffer,
                                      gboolean               *refill)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;
  HyScanGtkMapTiledLayerCacheHeader header;

  gboolean found = FALSE;
  gboolean refill_ = TRUE;

  if (priv->cache == NULL)
    goto exit;

  /* Ищем в кэше. */
  hyscan_gtk_map_tiled_layer_tile_cache_key (tiled_layer, tile, priv->cache_key, CACHE_KEY_LEN);
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->cache_key, NULL, sizeof (header), priv->cache_buffer, tile_buffer))
    goto exit;

  /* Верифицируем. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.size != hyscan_buffer_get_size (tile_buffer)))
    {
      goto exit;
    }

  /* Проверяем, насколько актуален найденный тайл. */
  switch (hyscan_gtk_map_tiled_layer_get_mod_actual (tiled_layer, tile, header.mod))
    {
    case STATE_ACTUAL:
      refill_ = FALSE;
      found = TRUE;
      break;

    case STATE_OUTDATED:
      refill_ = TRUE;
      found = TRUE;
      break;

    case STATE_IRRELEVANT:
      refill_ = TRUE;
      found = FALSE;
      break;
    }

exit:
  *refill = refill_;

  return found;
}

static void
hyscan_gtk_map_tiled_layer_update_grid (HyScanGtkMapTiledLayerPrivate *priv)
{
  guint scales_len, i;
  gdouble *scales;

  scales = hyscan_gtk_map_get_scales_cifro (priv->map, &scales_len);
  for (i = 0; i < scales_len; ++i)
    scales[i] *= TILE_SIZE;

  // todo: lock mutex when use/update tile_grid?
  g_clear_object (&priv->tile_grid);
  priv->tile_grid = hyscan_gtk_map_tile_grid_new_from_cifro (GTK_CIFRO_AREA (priv->map), 0, TILE_SIZE);
  hyscan_gtk_map_tile_grid_set_scales (priv->tile_grid, scales, scales_len);
  g_free (scales);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_tiled_layer_proj_notify (HyScanGtkMapTiledLayer *tiled_layer,
                                        GParamSpec             *pspec)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  hyscan_gtk_map_tiled_layer_update_grid (priv);
}

static guint
hyscan_gtk_map_tiled_layer_fill_tile (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTiledLayerClass *klass = HYSCAN_GTK_MAP_TILED_LAYER_GET_CLASS (tiled_layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;
  guint mod_count;

  g_return_val_if_fail (klass->fill_tile != NULL, 0);

  /* Запоминаем номер изменения данных, соответствующий тайлу. */
  mod_count = g_atomic_int_get (&priv->mod_count);
  klass->fill_tile (tiled_layer, tile);

#ifdef DEBUG_TILES
  {
    GRand *rand;
    gchar tile_num[100];
    gint x, y;
    guint tile_size;
    cairo_t *cairo;
    cairo_surface_t *surface;

    surface = hyscan_gtk_map_tile_get_surface (tile);
    cairo = cairo_create (surface);

    x = hyscan_gtk_map_tile_get_x (tile);
    y = hyscan_gtk_map_tile_get_y (tile);
    tile_size = hyscan_gtk_map_tile_get_size (tile);
    rand = g_rand_new ();
    g_snprintf (tile_num, sizeof (tile_num), "tile %d, %d", x, y);
    cairo_move_to (cairo, tile_size / 2.0, tile_size / 2.0);
    cairo_set_source_rgba (cairo,
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           0.1);
    cairo_paint (cairo);
    cairo_set_source_rgb (cairo, 0.2, 0.2, 0);
    cairo_show_text (cairo, tile_num);

    cairo_surface_destroy (surface);
    cairo_destroy (cairo);
  }
#endif

  return mod_count;
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_tiled_layer_cache_set (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanGtkMapTile       *tile,
                                      guint                   mod_count)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  gchar cache_key[CACHE_KEY_LEN];

  HyScanGtkMapTiledLayerCacheHeader header;
  HyScanBuffer *header_buf;

  const guint8 *tile_data;
  HyScanBuffer *tile_buf;

  cairo_surface_t *surface;

  if (priv->cache == NULL)
    return;

  surface = hyscan_gtk_map_tile_get_surface (tile);
  if (surface == NULL)
    return;

  tile_buf = hyscan_buffer_new ();
  header_buf = hyscan_buffer_new ();

  /* Оборачиваем в буфер заголовок с информацией об изображении. */
  header.magic = CACHE_HEADER_MAGIC;
  header.mod = mod_count;
  header.size = cairo_image_surface_get_stride (surface) * cairo_image_surface_get_height (surface);
  hyscan_buffer_wrap_data (header_buf, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Оборачиваем в буфер пиксельные данные. */
  tile_data = cairo_image_surface_get_data (surface);
  hyscan_buffer_wrap_data (tile_buf, HYSCAN_DATA_BLOB, (gpointer) tile_data, header.size);

  /* Помещаем все данные в кэш. */
  hyscan_gtk_map_tiled_layer_tile_cache_key (tiled_layer, tile, cache_key, CACHE_KEY_LEN);
  hyscan_cache_set2 (priv->cache, cache_key, NULL, header_buf, tile_buf);

  cairo_surface_destroy (surface);
  g_object_unref (header_buf);
  g_object_unref (tile_buf);
}


/* Обработчик задачи из очереди HyScanTaskQueue.
 * Заполняет переданный тайл. */
static void
hyscan_gtk_map_tiled_layer_process (GObject      *task,
                                    gpointer      user_data,
                                    GCancellable *cancellable)
{
  HyScanGtkMapTile *tile = HYSCAN_GTK_MAP_TILE (task);
  HyScanGtkMapTiledLayer*tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (user_data);

  guint mod_count;

  /* Заполняет тайл и помещаем его в кэш. */
  mod_count = hyscan_gtk_map_tiled_layer_fill_tile (tiled_layer, tile);
  hyscan_gtk_map_tiled_layer_cache_set (tiled_layer, tile, mod_count);

  /* Просим перерисовать. */
  hyscan_gtk_map_tiled_layer_request_draw (tiled_layer);
}

/* Запрашивает перерисовку виджета карты, если пришёл хотя бы один тайл из видимой области. */
static gboolean
hyscan_gtk_map_tiled_layer_queue_draw (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  if (g_atomic_int_compare_and_exchange (&priv->redraw, TRUE, FALSE))
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return G_SOURCE_CONTINUE;
}

/* Реализация HyScanGtkLayerInterface.added.
 * Подключается к сигналам карты при добавлении слоя на карту. */
static void
hyscan_gtk_map_tiled_layer_added (HyScanGtkLayer          *layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTiledLayer *tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  /* Создаем очередь задач по заполнению тайлов. */
  priv->task_queue = hyscan_task_queue_new (hyscan_gtk_map_tiled_layer_process, tiled_layer,
                                            (GCompareFunc) hyscan_gtk_map_tile_compare);

  /* Подключаемся к карте. */
  priv->map = g_object_ref (container);
  hyscan_gtk_map_tiled_layer_update_grid (tiled_layer->priv);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_tiled_layer_proj_notify), tiled_layer);

  /* Запускаем функцию перерисовки. Ограничим ее периодом не чаще 1 раза в 40 милисекунд, чтобы не перегружать ЦП. */
  priv->redraw_tag = g_timeout_add (40, (GSourceFunc) hyscan_gtk_map_tiled_layer_queue_draw, tiled_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Отключается от сигналов карты при удалении слоя с карты. */
static void
hyscan_gtk_map_tiled_layer_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapTiledLayer *tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  /* Останавливаем перерисовку. */
  g_source_remove (priv->redraw_tag);
  priv->redraw_tag = 0;

  /* Останавливаем очередь. */
  hyscan_task_queue_shutdown (priv->task_queue);
  g_clear_object (&priv->task_queue);

  /* Отключаемся от карты. */
  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
  g_clear_object (&priv->tile_grid);
}

static void
hyscan_gtk_map_tiled_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_tiled_layer_added;
  iface->removed = hyscan_gtk_map_tiled_layer_removed;
}

/* Рисует трек по тайлам. */
void
hyscan_gtk_map_tiled_layer_draw (HyScanGtkMapTiledLayer *tiled_layer,
                                 cairo_t                *cairo)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  gint scale_idx;
  gdouble scale;

  gint x, y;
  gint from_tile_x, to_tile_x, from_tile_y, to_tile_y;

  /* Инициализируем тайловую сетку grid. */
  scale_idx = hyscan_gtk_map_get_scale_idx (priv->map, &scale);

  /* Определяем область рисования. */
  hyscan_gtk_map_tile_grid_get_view_cifro (priv->tile_grid, GTK_CIFRO_AREA (priv->map), scale_idx,
                                           &from_tile_x, &to_tile_x, &from_tile_y, &to_tile_y);

  /* Рисуем тайлы по очереди. */
  for (x = from_tile_x; x <= to_tile_x; x++)
    for (y = from_tile_y; y <= to_tile_y; y++)
      {
        HyScanGeoCartesian2D coord;
        gdouble x_source, y_source;
        HyScanGtkMapTile *tile;
        gboolean refill;
        gboolean found;

        /* Заполняем тайл. */
        tile = hyscan_gtk_map_tile_new (priv->tile_grid, x, y, scale_idx);
        found = hyscan_gtk_map_tiled_layer_cache_get (tiled_layer, tile, priv->tile_buffer, &refill);

        /* Если надо, отправляем тайл на перерисовку. */
        if (refill)
          hyscan_task_queue_push (priv->task_queue, G_OBJECT (tile));

        /* Если тайл наден, то рисуем его. */
        if (found)
          {
            guint32 size;
            gpointer cached_data;

            cairo_surface_t *surface;

            /* Загружаем поверхность тайла из буфера. */
            cached_data = hyscan_buffer_get_data (priv->tile_buffer, &size);
            hyscan_gtk_map_tile_set_surface_data (tile, cached_data, size);
            surface = hyscan_gtk_map_tile_get_surface (tile);

            /* Переносим поверхность тайла на изображение слоя. */
            hyscan_gtk_map_tile_get_bounds (tile, &coord, NULL);
            gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x_source, &y_source, coord.x, coord.y);
            cairo_set_source_surface (cairo, surface, x_source, y_source);
            cairo_paint (cairo);

            /* Освобождаем поверхность. */
            cairo_surface_destroy (surface);
            hyscan_gtk_map_tile_set_surface (tile, NULL);
          }

        g_object_unref (tile);
      }

  /* Отправляем на заполнение все тайлы. */
  hyscan_task_queue_push_end (priv->task_queue);
}

/* Записывает, что актуальная версия тайлов с указанным отрезком point0 - point1
 * соответствует номеру изменения mod_count.
 *
 * Функция должна вызываться за мьютексом! */
void
hyscan_gtk_map_tiled_layer_set_area_mod (HyScanGtkMapTiledLayer *tiled_layer,
                                         HyScanGeoCartesian2D   *point0,
                                         HyScanGeoCartesian2D   *point1)
{
  HyScanGtkMapTiledLayerPrivate *priv;

  gdouble *scales;
  guint scales_len;
  guint scale_idx;

  HyScanGtkMapTiledLayerMod track_mod;
  HyScanBuffer *buffer;

  gchar cache_key[CACHE_KEY_LEN];

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));
  priv = tiled_layer->priv;

  /* Увеличиваем счетчик изменений. */
  g_atomic_int_inc (&priv->mod_count);

  /* Оборачиваем номер изменения в буфер. */
  track_mod.magic = CACHE_TRACK_MOD_MAGIC;
  track_mod.mod = g_atomic_int_get (&priv->mod_count);
  buffer = hyscan_buffer_new ();
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, &track_mod, sizeof (track_mod));

  /* Для каждого масштаба определяем тайлы, на которых лежит этот отрезок. */
  scales = hyscan_gtk_map_get_scales_cifro (priv->map, &scales_len);
  for (scale_idx = 0; scale_idx < scales_len; scale_idx++)
    {
      gint x, y, to_x, to_y;
      gdouble x0, y0, x1, y1;

      /* Определяем на каких тайлах лежат концы отрезка. */
      hyscan_gtk_map_tile_grid_value_to_tile (priv->tile_grid, scale_idx, point0->x, point0->y, &x0, &y0);
      hyscan_gtk_map_tile_grid_value_to_tile (priv->tile_grid, scale_idx, point1->x, point1->y, &x1, &y1);

      /* Проходим все тайлы внутри найденной области и обновляем номер изменения трека. */
      to_x = MAX (x0, x1);
      to_y = MAX (y0, y1);
      for (x = MIN (x0, x1); x <= to_x; ++x)
        for (y = MIN (y0, y1); y <= to_y; ++y)
          {
            /* Обновляем в кэше номер изменения трека. */
            hyscan_gtk_map_tiled_layer_mod_cache_key (tiled_layer, x, y, scale_idx, cache_key, sizeof (cache_key));
            hyscan_cache_set (priv->cache, cache_key, NULL, buffer);
          }
    }

  g_object_unref (buffer);
  g_free (scales);
}

void
hyscan_gtk_map_tiled_layer_set_param_mod (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));
  priv = tiled_layer->priv;

  g_atomic_int_inc (&priv->param_mod_count);
}


gboolean
hyscan_gtk_map_tiled_layer_has_cache (HyScanGtkMapTiledLayer *tiled_layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer), FALSE);

  return tiled_layer->priv->cache != NULL;
}

void
hyscan_gtk_map_tiled_layer_request_draw (HyScanGtkMapTiledLayer *tiled_layer)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));

  g_atomic_int_set (&tiled_layer->priv->redraw, TRUE);
}
