/* hyscan-gtk-map-track-layer.c
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
 * SECTION: hyscan-gtk-map-track-layer
 * @Short_description: слой карты с изображением галсов
 * @Title: HyScanGtkMapTrackLayer
 * @See_also: #HyScanGtkLayer
 *
 * Слой отображает галсы выбранного проекта. Каждый галс изображется линией
 * движения, которая строится по GPS-отметкам, и линиями ширины трека.
 *
 * Для создания и отображения слоя используются функции:
 * - hyscan_gtk_map_track_layer_new() создает слой
 * - hyscan_gtk_map_track_layer_track_enable() устанавливает видимость
 *   указанного галса
 *
 * По умолчанию слой самостоятельно выбирает одни из доступных каналов данных
 * для получения навигационной и акустической информации по галсу. Чтобы получить
 * или установить номера используемых каналов, следует обратиться к функциям:
 * - hyscan_gtk_map_track_layer_track_get_channel()
 * - hyscan_gtk_map_track_layer_track_max_channel()
 * - hyscan_gtk_map_track_layer_track_set_channel().
 *
 * Стиль оформления галсов можно определить с помощью следующих сеттеров:
 * - hyscan_gtk_map_track_layer_set_color_track()
 * - hyscan_gtk_map_track_layer_set_color_port()
 * - hyscan_gtk_map_track_layer_set_color_starboard()
 * - hyscan_gtk_map_track_layer_set_bar_width()
 * - hyscan_gtk_map_track_layer_set_bar_margin()
 *
 * Также слой поддерживает загрузку конфигурационных ini-файлов. Подробнее в
 * функции hyscan_gtk_layer_container_load_key_file().
 *
 */

#include "hyscan-gtk-map-track-layer.h"
#include <hyscan-gtk-map.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-projector.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-depthometer.h>
#include <math.h>

/* Стиль оформления по умолчанию. */
#define DEFAULT_COLOR_TRACK           "#bbbb00"            /* Цвет линии движения. */
#define DEFAULT_COLOR_PORT            "#bb0000"            /* Цвет левого борта. */
#define DEFAULT_COLOR_STARBOARD       "#00bb00"            /* Цвет правого борта. */
#define DEFAULT_COLOR_STROKE          "#000000"            /* Цвет обводки некоторых элементов. */
#define DEFAULT_COLOR_SHADOW          "rgba(0,0,0,0.5)"    /* Цвет затенения. */
#define DEFAULT_BAR_WIDTH             10                   /* Ширина полос дальности. */
#define DEFAULT_BAR_MARGIN            30                   /* Расстояние между соседними полосами. */
#define DEFAULT_LINE_WIDTH            1                    /* Толщина линии движения. */
#define DEFAULT_STROKE_WIDTH          0.5                  /* Толщина обводки. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_CACHE
};

typedef struct {
  HyScanGeoGeodetic               geo;              /* Географические координаты точки и курс движения. */
  guint32                         index;            /* Индекс записи в канале данных. */
  gint64                          time;             /* Время фиксации. */
  gdouble                         l_width;          /* Дальность по левому борту, метры. */
  gdouble                         r_width;          /* Дальность по правому борту, метры. */

  /* Координаты на картографической проекции (зависят от текущей проекции). */
  gdouble                         x;                /* Координата X путевой точки. */
  gdouble                         y;                /* Координата Y путевой точки. */
  gdouble                         dist;             /* Расстояние до предыдущей точки. */
  gdouble                         l_dist;           /* Дальность по левому борту, единицы проекции. */
  gdouble                         r_dist;           /* Дальность по правому борту, единицы проекции. */
} HyScanGtkMapTrackLayerPoint;

typedef struct {
  HyScanAmplitude                *amplitude;        /* Амплитудные данные трека. */
  HyScanProjector                *projector;        /* Сопоставления индексов и отсчётов реальным координатам. */
} HyScanGtkMapTrackLayerBoard;

typedef struct {
  gchar                           *name;             /* Название галса. */

  gboolean                        opened;            /* Признак того, что каналы галса открыты. */
  gboolean                        loaded;            /* Признак того, что данные галса загружены. */
  gboolean                        enabled;           /* Признак того, что галс необходимо показать. */

  /* Каналы данных. */
  guint                           channel_starboard; /* Номер канала правого борта. */
  guint                           channel_port;      /* Номер канала левого борта. */
  guint                           channel_rmc;       /* Номер канала nmea с RMC. */
  guint                           channel_dpt;       /* Номер канала nmea с DPT. */

  HyScanGtkMapTrackLayerBoard     port;              /* Данные левого борта. */
  HyScanGtkMapTrackLayerBoard     starboard;         /* Данные правого борта. */
  HyScanDepthometer              *depthometer;       /* Определение глубины. */
  HyScanNavData                  *lat_data;          /* Навигационные данные - широта. */
  HyScanNavData                  *lon_data;          /* Навигационные данные - долгота. */
  HyScanNavData                  *angle_data;        /* Навигационные данные - курс в градусах. */

  guint32                         lat_mod_count;     /* Mod-count канала навигационных данных. */
  guint32                         first_index;       /* Первый индекс в канале навигационных данных. */
  guint32                         last_index;        /* Последний индекс в канале навигационных данных. */

  GList                          *points;            /* Список точек трека HyScanGtkMapTrackLayerPoint. */
} HyScanGtkMapTrackLayerTrack;

struct _HyScanGtkMapTrackLayerPrivate
{
  HyScanGtkMap              *map;             /* Карта. */
  gboolean                   visible;         /* Признак видимости слоя. */

  HyScanDB                  *db;              /* База данных. */
  gchar                     *project;         /* Название проекта. */
  HyScanCache               *cache;           /* Кэш для навигационных данных. */

  GHashTable                *tracks;          /* Таблица отображаемых галсов:
                                               * ключ - название трека, значение - HyScanGtkMapTrackLayerTrack. */

  /* Стиль оформления. */
  GdkRGBA                    color_left;      /* Цвет левого борта. */
  GdkRGBA                    color_right;     /* Цвет правого борта. */
  GdkRGBA                    color_track;     /* Цвет линии движения. */
  GdkRGBA                    color_stroke;    /* Цвет обводки. */
  GdkRGBA                    color_shadow;    /* Цвет затенения (рекомендуется полупрозрачный чёрный). */
  gdouble                    bar_width;       /* Толщина линии ширины. */
  gdouble                    bar_margin;      /* Расстояние между соседними линиями ширины. */
  gdouble                    line_width;      /* Толщина линии движения. */
  gdouble                    stroke_width;    /* Толщина линии обводки. */
};

static void    hyscan_gtk_map_track_layer_interface_init           (HyScanGtkLayerInterface        *iface);
static void    hyscan_gtk_map_track_layer_set_property             (GObject                        *object,
                                                                    guint                           prop_id,
                                                                    const GValue                   *value,
                                                                    GParamSpec                     *pspec);
static void    hyscan_gtk_map_track_layer_track_open               (HyScanGtkMapTrackLayer         *track_layer,
                                                                    HyScanGtkMapTrackLayerTrack    *track);
static void    hyscan_gtk_map_track_layer_track_load               (HyScanGtkMapTrackLayer         *track_layer,
                                                                    HyScanGtkMapTrackLayerTrack    *track);
static void    hyscan_gtk_map_track_layer_object_constructed       (GObject                        *object);
static void    hyscan_gtk_map_track_layer_object_finalize          (GObject                        *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackLayer, hyscan_gtk_map_track_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapTrackLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_track_layer_interface_init))

static void
hyscan_gtk_map_track_layer_class_init (HyScanGtkMapTrackLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_layer_set_property;

  object_class->constructed = hyscan_gtk_map_track_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Db", "HyScanDb", HYSCAN_TYPE_DB,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name", NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache", HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_layer_init (HyScanGtkMapTrackLayer *gtk_map_track_layer)
{
  gtk_map_track_layer->priv = hyscan_gtk_map_track_layer_get_instance_private (gtk_map_track_layer);
}

static void
hyscan_gtk_map_track_layer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Особождает память, занятую структурой HyScanGtkMapTrackLayerTrack. */
static void
hyscan_gtk_map_track_layer_track_free (gpointer data)
{
  HyScanGtkMapTrackLayerTrack *track = data;

  g_free (track->name);
  g_clear_object (&track->lat_data);
  g_clear_object (&track->lon_data);
  g_clear_object (&track->angle_data);
  g_clear_object (&track->starboard.amplitude);
  g_clear_object (&track->starboard.projector);
  g_clear_object (&track->port.amplitude);
  g_clear_object (&track->port.projector);
  g_clear_object (&track->depthometer);
  g_list_free_full (track->points, g_free);
  g_free (track);
}

static void
hyscan_gtk_map_track_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->constructed (object);

  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, hyscan_gtk_map_track_layer_track_free);

  /* Оформление трека. */
  priv->bar_width = DEFAULT_BAR_WIDTH;
  priv->bar_margin = DEFAULT_BAR_MARGIN;
  priv->line_width = DEFAULT_LINE_WIDTH;
  priv->stroke_width = DEFAULT_STROKE_WIDTH;
  gdk_rgba_parse (&priv->color_left, DEFAULT_COLOR_PORT);
  gdk_rgba_parse (&priv->color_right, DEFAULT_COLOR_STARBOARD);
  gdk_rgba_parse (&priv->color_track, DEFAULT_COLOR_TRACK);
  gdk_rgba_parse (&priv->color_stroke, DEFAULT_COLOR_STROKE);
  gdk_rgba_parse (&priv->color_shadow, DEFAULT_COLOR_SHADOW);
}

static void
hyscan_gtk_map_track_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_free (priv->project);
  g_hash_table_destroy (priv->tracks);

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->finalize (object);
}

/* Рисует отдельный галс. */
static void
hyscan_gtk_map_track_layer_draw_track (HyScanGtkMapTrackLayer      *track_layer,
                                       HyScanGtkMapTrackLayerTrack *track,
                                       cairo_t                     *cairo)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GList *point_l;
  HyScanGtkMapTrackLayerPoint *point;
  gdouble x, y;

  gdouble scale;
  gdouble threshold;
  gdouble dist;

  if (!track->enabled)
    return;

  /* Загружаем новый данные по треку (если что-то изменилось в канале данных). */
  hyscan_gtk_map_track_layer_track_load (track_layer, track);

  if (track->points == NULL)
    return;

  /* Минимальное расстояние между двумя полосками. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
  threshold = scale * (priv->bar_margin + priv->bar_width);
  dist = threshold;

  /* Рисуем полосы от бортов. */
  cairo_set_line_width (cairo, priv->bar_width);
  cairo_new_path (cairo);
  for (point_l = track->points; point_l != NULL; point_l = point_l->next)
    {
      gdouble x0, y0;
      gdouble shadow_len;

      point = point_l->data;

      dist += point->dist;
      if (dist < threshold || (point->l_dist == 0.0 && point->r_dist == 0.0))
        continue;

      dist = 0;

      cairo_save (cairo);
      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x0, &y0, point->x, point->y);
      cairo_translate (cairo, x0, y0);
      cairo_rotate (cairo, point->geo.h / 180 * G_PI);

      /* Правый борт. */
      cairo_rectangle (cairo, 0, -priv->bar_width / 2.0, point->r_dist / scale, priv->bar_width);
      cairo_set_line_width (cairo, priv->stroke_width);
      gdk_cairo_set_source_rgba (cairo, &priv->color_stroke);
      cairo_stroke_preserve (cairo);
      gdk_cairo_set_source_rgba (cairo, &priv->color_right);
      cairo_fill (cairo);

      /* Левый борт. */
      cairo_rectangle (cairo, 0, -priv->bar_width / 2.0, -point->l_dist / scale, priv->bar_width);
      cairo_set_line_width (cairo, priv->stroke_width);
      gdk_cairo_set_source_rgba (cairo, &priv->color_stroke);
      cairo_stroke_preserve (cairo);
      gdk_cairo_set_source_rgba (cairo, &priv->color_left);
      cairo_fill (cairo);

      /* Тень в пересечении линии движения и полос от бортов. */
      gdk_cairo_set_source_rgba (cairo, &priv->color_shadow);
      shadow_len =  MIN (point->r_dist / scale, point->l_dist / scale) - 1.0;
      shadow_len = MIN (priv->bar_width, shadow_len);
      cairo_rectangle (cairo, - shadow_len / 2.0, - priv->bar_width / 2.0, shadow_len, priv->bar_width);
      cairo_fill (cairo);

      cairo_restore (cairo);
    }

  /* Рисуем линию движения. */
  gdk_cairo_set_source_rgba (cairo, &priv->color_track);
  cairo_set_line_width (cairo, priv->line_width);
  cairo_new_path (cairo);
  for (point_l = track->points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, point->x, point->y);
      cairo_line_to (cairo, x, y);
    }
  cairo_stroke (cairo);

  /* Точка начала трека. */
  point = track->points->data;
  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, point->x, point->y);
  cairo_arc (cairo, x, y, 2.0 * priv->line_width, 0, 2.0 * G_PI);

  gdk_cairo_set_source_rgba (cairo, &priv->color_track);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, priv->stroke_width);
  gdk_cairo_set_source_rgba (cairo, &priv->color_stroke);
  cairo_stroke (cairo);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_track_layer_draw (HyScanGtkMap            *map,
                                 cairo_t                 *cairo,
                                 HyScanGtkMapTrackLayer  *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GHashTableIter iter;

  HyScanGtkMapTrackLayerTrack *track;
  gchar *key;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    return;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
    hyscan_gtk_map_track_layer_draw_track (track_layer, track, cairo);
}

/* Переводит координаты трека из географических в проекцию карты. */
static void
hyscan_gtk_map_track_layer_track_points (HyScanGtkMapTrackLayer *layer,
                                         GList                  *points)
{
  HyScanGtkMapTrackLayerPrivate *priv = layer->priv;
  GList *point_l;

  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapTrackLayerPoint *point = point_l->data;
      gdouble scale;

      hyscan_gtk_map_geo_to_value (priv->map, point->geo, &point->x, &point->y);

      /* Определяем расстояние до предыдущей точки. */
      if (point_l->prev != NULL)
        {
          HyScanGtkMapTrackLayerPoint *prev_point = point_l->prev->data;
          gdouble dx, dy;

          dx = prev_point->x - point->x;
          dy = prev_point->y - point->y;
          point->dist = sqrt (dx * dx + dy * dy);
        }
      else
        {
          point->dist = 0;
        }

      /* Масштаб перевода из метров в логические координаты. */
      scale = hyscan_gtk_map_get_value_scale (priv->map, &point->geo);


      /* Правый борт. */
      point->r_dist = point->r_width / scale;

      /* Левый борт. */
      point->l_dist = point->l_width / scale;
    }
}

/* Обрабатывает сигнал об изменении картографической проекции. */
static void
hyscan_gtk_map_track_layer_projection_notify (HyScanGtkMap           *map,
                                              GParamSpec             *pspec,
                                              HyScanGtkMapTrackLayer *layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = layer->priv;
  GHashTableIter iter;

  gchar *track_name;
  HyScanGtkMapTrackLayerTrack *track;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_name, (gpointer *) &track))
    hyscan_gtk_map_track_layer_track_points (layer, track->points);
}

/* Реализация HyScanGtkLayerInterface.added.
 * Обрабатывает добавление слой на карту. */
static void
hyscan_gtk_map_track_layer_added (HyScanGtkLayer          *gtk_layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  g_signal_connect_after (priv->map, "visible-draw", 
                          G_CALLBACK (hyscan_gtk_map_track_layer_draw), track_layer);
  g_signal_connect (priv->map, "notify::projection",
                    G_CALLBACK (hyscan_gtk_map_track_layer_projection_notify), track_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Обрабатывает удаление слоя с карты. */
static void
hyscan_gtk_map_track_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, track_layer);
  g_clear_object (&priv->map);
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_track_layer_set_visible (HyScanGtkLayer *layer,
                                        gboolean        visible)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_track_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  return priv->visible;
}

/* Реализация HyScanGtkLayerInterface.load_key_file.
 * Загружает параметры отображения слоя из конфигурационного файла. */
static gboolean
hyscan_gtk_map_track_layer_load_key_file (HyScanGtkLayer *gtk_layer,
                                          GKeyFile       *key_file,
                                          const gchar    *group)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  GdkRGBA rgba;
  gdouble value;

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "color-track", DEFAULT_COLOR_TRACK);
  hyscan_gtk_map_track_layer_set_color_track (track_layer, rgba);

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "color-port", DEFAULT_COLOR_PORT);
  hyscan_gtk_map_track_layer_set_color_port (track_layer, rgba);

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "color-starboard", DEFAULT_COLOR_STARBOARD);
  hyscan_gtk_map_track_layer_set_color_starboard (track_layer, rgba);

  value = g_key_file_get_double (key_file, group, "bar-width", NULL);
  hyscan_gtk_map_track_layer_set_bar_width (track_layer, (value > 0.0) ? value : DEFAULT_BAR_WIDTH);

  value = g_key_file_get_double (key_file, group, "bar-margin", NULL);
  hyscan_gtk_map_track_layer_set_bar_margin (track_layer, (value > 0.0) ? value : DEFAULT_BAR_MARGIN);

  return TRUE;
}

static void
hyscan_gtk_map_track_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->load_key_file = hyscan_gtk_map_track_layer_load_key_file;
  iface->added = hyscan_gtk_map_track_layer_added;
  iface->removed = hyscan_gtk_map_track_layer_removed;
  iface->set_visible = hyscan_gtk_map_track_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_track_layer_get_visible;
}

/* Определяет ширину трека в момент времени time по борту board. */
static gdouble
hyscan_gtk_map_track_layer_track_width (HyScanGtkMapTrackLayerBoard *board,
                                        HyScanDepthometer           *depthometer,
                                        gint64                       time)
{
  guint32 amp_rindex, amp_lindex;
  guint32 nvals;
  gdouble depth;
  gdouble distance;

  HyScanDBFindStatus find_status;

  if (board->amplitude == NULL)
    return 0.0;

  find_status = hyscan_amplitude_find_data (board->amplitude, time, &amp_lindex, &amp_rindex, NULL, NULL);

  if (find_status != HYSCAN_DB_FIND_OK)
    return 0.0;

  depth = (depthometer != NULL) ? hyscan_depthometer_get (depthometer, time) : 0;
  if (depth < 0)
    depth = 0;

  hyscan_amplitude_get_size_time (board->amplitude, amp_lindex, &nvals, NULL);
  hyscan_projector_count_to_coord (board->projector, nvals, &distance, depth);

  return distance;
}
static GList *
hyscan_gtk_map_track_layer_track_load_range (HyScanGtkMapTrackLayer      *track_layer,
                                             HyScanGtkMapTrackLayerTrack *track,
                                             guint32                      first,
                                             guint32                      last)
{
  GList *points = NULL;
  guint32 index;

  for (index = first; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoGeodetic coords;
      HyScanGtkMapTrackLayerPoint *point;

      if (!hyscan_nav_data_get (track->lat_data, index, &time, &coords.lat))
        continue;

      if (!hyscan_nav_data_get (track->lon_data, index, &time, &coords.lon))
        continue;

      if (!hyscan_nav_data_get (track->angle_data, index, &time, &coords.h))
        continue;

      point = g_new (HyScanGtkMapTrackLayerPoint, 1);
      point->index = index;
      point->geo = coords;

      /* Определяем ширину отснятых данных в этот момент. */
      point->r_width = hyscan_gtk_map_track_layer_track_width (&track->starboard, track->depthometer, time);
      point->l_width = hyscan_gtk_map_track_layer_track_width (&track->port,      track->depthometer, time);

      points = g_list_append (points, point);
    }

  /* Переводим географические координаты в логические. */
  hyscan_gtk_map_track_layer_track_points (track_layer, points);

  return points;
}

/* Загружает путевые точки трека и его ширину. */
static void
hyscan_gtk_map_track_layer_track_load (HyScanGtkMapTrackLayer      *track_layer,
                                       HyScanGtkMapTrackLayerTrack *track)
{
  guint32 first_index, last_index;
  guint32 mod_count;
  GList *points, *point_l;

  /* Открываем каналы данных. */
  if (!track->opened)
    hyscan_gtk_map_track_layer_track_open (track_layer, track);

  /* Без данных по координатам ничего не получится - выходим. */
  if (track->lat_data == NULL)
    return;

  /* Если галс загружен и в актуальном состоянии, то выходим. */
  mod_count = hyscan_nav_data_get_mod_count (track->lat_data);
  if (track->loaded && mod_count == track->lat_mod_count)
    return;

  if (!hyscan_nav_data_get_range (track->lat_data, &first_index, &last_index))
    return;

  /* Приводим список точек в соответствие с флагом track->loaded. */
  if (!track->loaded)
    {
      g_list_free_full (track->points, g_free);
      track->points = NULL;
    }

  /* Удаляем точки, не попадающий в новый диапазон данных. */
  for (point_l = track->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapTrackLayerPoint *point = point_l->data;

      if (point->index >= first_index || point->index <= last_index)
        continue;

      g_free (point);
      track->points = g_list_remove_link (track->points, point_l);
    }

  /* Добавляем точки из нового диапазона, которых ещё нет. */
  if (track->points == NULL)
    {
      track->points = hyscan_gtk_map_track_layer_track_load_range (track_layer, track, first_index, last_index);
    }
  else
    {
      /* Добавляем точки в начало списка. */
      if (track->first_index > first_index)
        {
          points = hyscan_gtk_map_track_layer_track_load_range (track_layer, track, first_index, track->first_index - 1);
          track->points = g_list_concat (points, track->points);
        }

      /* Добавляем точки в конец списка. */
      if (track->last_index < last_index)
        {
          points = hyscan_gtk_map_track_layer_track_load_range (track_layer, track, track->last_index + 1, last_index);
          track->points = g_list_concat (track->points, points);
        }
    }

  track->lat_mod_count = mod_count;
  track->first_index = first_index;
  track->last_index = last_index;

  track->loaded = TRUE;
}

/**
 * hyscan_gtk_map_track_layer_new:
 * @db
 * @project
 * @cache
 *
 * Returns: указатель на новый слой #HyScanGtkMapTrackLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_track_layer_new (HyScanDB    *db,
                                const gchar *project,
                                HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LAYER,
                       "db", db,
                       "project", project,
                       "cache", cache, NULL);
}

/* Открывает (или переоткрывает) каналы данных в указанном галсе. */
static void
hyscan_gtk_map_track_layer_track_open (HyScanGtkMapTrackLayer      *track_layer,
                                       HyScanGtkMapTrackLayerTrack *track)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanNMEAParser *dpt_parser;

  g_clear_object (&track->starboard.amplitude);
  g_clear_object (&track->starboard.projector);
  g_clear_object (&track->port.amplitude);
  g_clear_object (&track->port.projector);
  g_clear_object (&track->lat_data);
  g_clear_object (&track->lon_data);
  g_clear_object (&track->angle_data);
  g_clear_object (&track->depthometer);

  if (track->channel_starboard > 0)
    {
      track->starboard.amplitude = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (priv->db, priv->cache, priv->project,
                                                                               track->name,
                                                                               HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                                                               track->channel_starboard, FALSE));
      track->starboard.projector = hyscan_projector_new (track->starboard.amplitude);
    }
  if (track->channel_port > 0)
    {
      track->port.amplitude = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (priv->db, priv->cache, priv->project,
                                                                          track->name,
                                                                          HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                                                          track->channel_port, FALSE));
      track->port.projector = hyscan_projector_new (track->port.amplitude);
    }


  if (track->channel_rmc > 0)
    {
      track->lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                 track->name, track->channel_rmc,
                                                                 HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
      track->lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                track->name, track->channel_rmc,
                                                                HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
      track->angle_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                   track->name, track->channel_rmc,
                                                                   HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));
    }

  if (track->channel_dpt > 0)
    {
      dpt_parser = hyscan_nmea_parser_new (priv->db, priv->cache,
                                           priv->project, track->name, track->channel_dpt,
                                           HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH);
      if (dpt_parser != NULL)
        track->depthometer = hyscan_depthometer_new (HYSCAN_NAV_DATA (dpt_parser), priv->cache);

      g_clear_object (&dpt_parser);
    }

  track->loaded = FALSE;
  track->opened = TRUE;
}

/* Устанавливает номер канала для указанного трека. */
static void
hyscan_gtk_map_track_layer_track_set_channel_real (HyScanGtkMapTrackLayer      *track_layer,
                                                   HyScanGtkMapTrackLayerTrack *track,
                                                   guint                        channel,
                                                   guint                        channel_num)
{
  guint max_channel;

  max_channel = hyscan_gtk_map_track_layer_track_max_channel (track_layer, track->name, channel);
  channel_num = MIN (channel_num, max_channel);

  switch (channel)
    {
    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT:
      track->channel_dpt = channel_num;
      break;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC:
      track->channel_rmc = channel_num;
      break;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD:
      track->channel_starboard = channel_num;
      break;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT:
      track->channel_port = channel_num;
      break;

    default:
      g_warning ("HyScanGtkMapTrackLayer: invalid channel");
    }
}

/* Создаёт галс с параметрами по умолчанию. */
static HyScanGtkMapTrackLayerTrack *
hyscan_gtk_map_track_layer_track_new (HyScanGtkMapTrackLayer *track_layer,
                                      const gchar            *track_name)
{
  HyScanGtkMapTrackLayerTrack *track;

  /* Создаём новый галс. */
  track = g_new0 (HyScanGtkMapTrackLayerTrack, 1);
  track->name = g_strdup (track_name);

  /* Устанавливаем номера каналов по умолчанию. */
  hyscan_gtk_map_track_layer_track_set_channel_real (track_layer, track, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC, 1);
  hyscan_gtk_map_track_layer_track_set_channel_real (track_layer, track, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT, 2);
  hyscan_gtk_map_track_layer_track_set_channel_real (track_layer, track, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD, 1);
  hyscan_gtk_map_track_layer_track_set_channel_real (track_layer, track, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT, 1);

  return track;
}

/* Ищет галс в хэш-таблице; если галс не найден, то создает новый и добавляет
 * его в таблицу. */
static HyScanGtkMapTrackLayerTrack *
hyscan_gtk_map_track_layer_get_track (HyScanGtkMapTrackLayer *track_layer,
                                      const gchar            *track_name)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackLayerTrack *track;

  track = g_hash_table_lookup (priv->tracks, track_name);
  if (track == NULL)
    {
      track = hyscan_gtk_map_track_layer_track_new (track_layer, track_name);
      g_hash_table_insert (priv->tracks, g_strdup (track_name), track);
    }

  return track;
}

/**
 * hyscan_gtk_map_track_layer_track_enable:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @track_name: название галса
 * @enable: %TRUE, если галс надо показать; иначе %FALSE
 *
 * Включает или отключает показ галса с названием @track_name.
 */
void
hyscan_gtk_map_track_layer_track_enable (HyScanGtkMapTrackLayer *track_layer,
                                         const gchar            *track_name,
                                         gboolean                enable)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  HyScanGtkMapTrackLayerTrack *track;
  
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));
  g_return_if_fail (track_name != NULL);
  priv = track_layer->priv;

  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name);
  track->enabled = enable;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_track_layer_track_max_channel:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @track_name: название галса
 * @channel: источник данных
 *
 * Returns: максимальный доступный номер канала для указанного источника данных
 *          @channel.
 */
guint
hyscan_gtk_map_track_layer_track_max_channel (HyScanGtkMapTrackLayer *track_layer,
                                              const gchar            *track_name,
                                              guint                   channel)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  guint max_channel_num = 0;
  gint i;
  gchar **channel_list;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer), 0);
  g_return_val_if_fail (track_name != NULL, 0);
  priv = track_layer->priv;

  gint32 project_id, track_id;
  
  project_id = hyscan_db_project_open (priv->db, priv->project);
  track_id = hyscan_db_track_open (priv->db, project_id, track_name);
  channel_list = hyscan_db_channel_list (priv->db, track_id);
  
  if (channel_list == NULL)
    goto exit;
  
  for (i = 0; channel_list[i] != NULL; ++i)
    {
      guint channel_num;
      HyScanSourceType source;
      HyScanChannelType type;

      hyscan_channel_get_types_by_name (channel_list[i], &source, &type, &channel_num);

      /* Отфильтровываем каналы по типу данных. */
      switch (channel)
        {
        case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC:
        case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT:
          if (source != HYSCAN_SOURCE_NMEA)
            continue;
          break;

        case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT:
          if (source != HYSCAN_SOURCE_SIDE_SCAN_PORT)
            continue;
          break;

        case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD:
          if (source != HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
            continue;
          break;

        default:
          continue;
        }

      max_channel_num = MAX (max_channel_num, channel_num);
    }
  
exit:
  g_clear_pointer (&channel_list, g_strfreev);
  hyscan_db_close (priv->db, track_id);
  hyscan_db_close (priv->db, project_id);

  return max_channel_num;
}

/**
 * hyscan_gtk_map_track_layer_track_get_channel:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @track_name: название галса
 * @channel: источник данных
 *
 * Returns: возвращает номер канала указанного источника или 0, если канал не
 *          открыт или произошла ошибка.
 */
guint
hyscan_gtk_map_track_layer_track_get_channel (HyScanGtkMapTrackLayer *track_layer,
                                              const gchar            *track_name,
                                              guint                   channel)
{
  HyScanGtkMapTrackLayerTrack *track;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer), 0);
  g_return_val_if_fail (track_name != NULL, 0);

  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name);

  switch (channel)
    {
    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT:
      return track->channel_dpt;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC:
      return track->channel_rmc;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD:
      return track->channel_starboard;

    case HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT:
      return track->channel_port;

    default:
      g_warning ("HyScanGtkMapTrackLayer: invalid channel");
      return 0;
    }
}

/**
 * hyscan_gtk_map_track_layer_track_configure:
 * @track_layer: указатель на слой
 * @track_name: название трека
 * @channel: источник данных
 * @channel_num: номер канала или 0, чтобы отключить указанный источник
 *
 * Устанавливает номер канала для указанного трека. Чтобы не загружать данные по
 * указанном истоничку, необходимо передать @channel_num = 0. Максимальный
 * доступный номер канала можно получить с помощью функции
 * hyscan_gtk_map_track_layer_track_max_channel().
 */
void
hyscan_gtk_map_track_layer_track_set_channel (HyScanGtkMapTrackLayer *track_layer,
                                              const gchar            *track_name,
                                              guint                   channel,
                                              guint                   channel_num)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  HyScanGtkMapTrackLayerTrack *track;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));
  g_return_if_fail (track_name != NULL);
  priv = track_layer->priv;

  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name);
  hyscan_gtk_map_track_layer_track_set_channel_real (track_layer, track, channel, channel_num);

  /* Переоткрываем трек с новыми номерами каналов. */
  track->opened = FALSE;
  track->loaded = FALSE;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_track_layer_track_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrackLayer
 * @track_name: название галса
 * @from_x: (out): координата левой границы по оси X
 * @to_x: (out): координата правой границы по оси X
 * @from_y: (out): координата нижней границы по оси Y
 * @to_y: (out): координата верхней границы по оси Y
 *
 * Показывает галс @track_name на карте. Функция устанавливает границы видимой
 * области карты так, чтобы галс был виден полностью.
 *
 * Returns: %TRUE, если получилось определить границы галса; иначе %FALSE
 */
gboolean 
hyscan_gtk_map_track_layer_track_view (HyScanGtkMapTrackLayer *track_layer,
                                       const gchar            *track_name)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  HyScanGtkMapTrackLayerTrack *track;
  HyScanGtkMapTrackLayerPoint *point;
  GList *point_l;

  gdouble max_margin = 0;
  gdouble from_x = G_MAXDOUBLE;
  gdouble to_x = G_MINDOUBLE;
  gdouble from_y = G_MAXDOUBLE;
  gdouble to_y = G_MINDOUBLE;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer), FALSE);
  g_return_val_if_fail (track_name != NULL, FALSE);
  priv = track_layer->priv;
  
  g_return_val_if_fail (priv->map != NULL, FALSE);

  /* Находим запрошенный галс. */
  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name);

  /* Если данные по галсу не загружен, то загружаем. */
  if (!track->loaded)
    hyscan_gtk_map_track_layer_track_load (track_layer, track);

  if (track->points == NULL)
    return FALSE;

  /* Определеяем границы координат путевых точек галса. */
  for (point_l = track->points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;
      
      from_x = MIN (from_x, point->x);
      to_x = MAX (to_x, point->x);
      from_y = MIN (from_y, point->y);
      to_y = MAX (to_y, point->y);
      
      max_margin = MAX (max_margin, point->r_dist);
      max_margin = MAX (max_margin, point->l_dist);
    }

  /* Добавим к границам отступы в размере длины максимальной дальности. */
  from_x -= max_margin;
  to_x += max_margin;
  from_y -= max_margin;
  to_y += max_margin;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map), from_x, to_x, from_y, to_y);

  return TRUE;
}

/**
 * hyscan_gtk_map_track_layer_set_color_track:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет линии движения
 *
 * Устанавливает цвет линии движения судна.
 */
void
hyscan_gtk_map_track_layer_set_color_track (HyScanGtkMapTrackLayer *track_layer,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->color_track = color;

  if (track_layer->priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (track_layer->priv->map));
}

/**
 * hyscan_gtk_map_track_layer_set_color_port:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет полос дальности для левого борта
 *
 * Устанавливает цвет полос дальности для левого борта.
 */
void
hyscan_gtk_map_track_layer_set_color_port (HyScanGtkMapTrackLayer *track_layer,
                                           GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->color_left = color;

  if (track_layer->priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (track_layer->priv->map));
}

/**
 * hyscan_gtk_map_track_layer_set_color_starboard:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет полос дальности для правого борта
 *
 * Устанавливает цвет полос дальности для правого борта.
 */
void
hyscan_gtk_map_track_layer_set_color_starboard (HyScanGtkMapTrackLayer *track_layer,
                                                GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->color_right = color;

  if (track_layer->priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (track_layer->priv->map));
}

/**
 * hyscan_gtk_map_track_layer_set_bar_width:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @bar_width: ширина полосы в пикселях
 *
 * Устанавливает ширину полосы дальности.
 */
void
hyscan_gtk_map_track_layer_set_bar_width (HyScanGtkMapTrackLayer *track_layer,
                                          gboolean                 bar_width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->bar_width = bar_width;

  if (track_layer->priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (track_layer->priv->map));
}

/**
 * hyscan_gtk_map_track_layer_set_bar_margin:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @bar_margin: отступ в пикселях вдоль линии трека
 *
 * Устанавливает расстоянием между двумя соседними полосами дальности.
 */
void
hyscan_gtk_map_track_layer_set_bar_margin (HyScanGtkMapTrackLayer *track_layer,
                                           gdouble                 bar_margin)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->bar_margin = bar_margin;

  if (track_layer->priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (track_layer->priv->map));
}
