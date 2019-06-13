/* hyscan-gtk-map-geomark-layer.c
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
 * SECTION: hyscan-gtk-map-geomark-layer
 * @Short_description: Слой с географическим метками
 * @Title: HyScanGtkMapGeomarkLayer
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Слой #HyScanGtkMapGeomarkLayer выводит на карте записанные в проект географические
 * метки, а также позволяет пользователю:
 * - создавать новый метки,
 * - изменять размер существующих меток,
 * - удалять метки.
 *
 */

#include "hyscan-gtk-map-geomark-layer.h"
#include "hyscan-gtk-map.h"
#include <hyscan-cartesian.h>

#define DISTANCE_TO_METERS      0.001                         /* Коэффициент перевода размеров метки в метры. */
#define HOVER_RADIUS            7                             /* Радиус хэндла. */

enum
{
  PROP_O,
  PROP_MODEL
};

enum
{
  MODE_NONE,
  MODE_DRAG,
  MODE_RESIZE,
};

typedef struct
{
  HyScanMarkGeo         *mark;      /* Указатель на метку. */
  gchar                 *mark_id;   /* Идентификатор метки. */

  /* Ключевые точки - они же хэндлы. */
  HyScanGeoCartesian2D   c2d;       /* Координаты центра. */
  HyScanGeoCartesian2D   corner[4]; /* Координаты углов. */

  gdouble                height;    /* Высота. */
  gdouble                width;     /* Ширина. */
  gboolean               pending;   /* Признак того, что метка обрабатывается. */
} HyScanGtkMapGeomarkLayerLocation;

struct _HyScanGtkMapGeomarkLayerPrivate
{
  HyScanGtkMap                            *map;                /* Карта. */
  HyScanMarkModel                         *model;              /* Модель меток. */

  gboolean                                 visible;            /* Признак видимости слоя. */

  GRWLock                                  mark_lock;          /* Блокировка доступа к меткам. */
  GHashTable                              *marks;              /* Список меток на слое. */
  gint                                     count;              /* Счётчик количества меток. */

  guint                                    mode;               /* Режим работы слоя (тип взаимодействия). */
  HyScanGtkMapGeomarkLayerLocation        *drag_mark;          /* Активная метка, с которой идёт взаимодействие. */
  gchar                                   *drag_mark_id;       /* Идентификатор активной метки drag_mark. */

  gchar                                   *hover_id;           /* Идентификатор метки под курсором мыши */

  gchar                                   *hover_handle_id;    /* Идентификатор метки, чей хэндл под курсором мыши. */
  HyScanGeoCartesian2D                     hover_handle;       /* Координаты хэндла. */

  /* Стиль оформления. */
  gdouble                                  line_width;         /* Толщина линий. */
  GdkRGBA                                  color;              /* Основной цвет. */
  GdkRGBA                                  color_hover;        /* Цвет метки под курсором мыши. */
  GdkRGBA                                  color_pending;      /* Цвет обрабатываемой метки. */
  GdkRGBA                                  color_blackout;     /* Цвет затемнения. */
};

static void    hyscan_gtk_map_geomark_layer_interface_init       (HyScanGtkLayerInterface          *iface);
static void    hyscan_gtk_map_geomark_layer_set_property         (GObject                          *object,
                                                                  guint                             prop_id,
                                                                  const GValue                     *value,
                                                                  GParamSpec                       *pspec);
static void    hyscan_gtk_map_geomark_layer_object_constructed   (GObject                          *object);
static void    hyscan_gtk_map_geomark_layer_object_finalize      (GObject                          *object);
static void    hyscan_gtk_map_geomark_layer_model_changed        (HyScanGtkMapGeomarkLayer         *gm_layer);
static void    hyscan_gtk_map_geomark_layer_location_free        (HyScanGtkMapGeomarkLayerLocation *location);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapGeomarkLayer, hyscan_gtk_map_geomark_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapGeomarkLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_geomark_layer_interface_init))

static void
hyscan_gtk_map_geomark_layer_class_init (HyScanGtkMapGeomarkLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_geomark_layer_set_property;
  object_class->constructed = hyscan_gtk_map_geomark_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_geomark_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Geo mark model",
                         "The HyScanMarkModel with geo marks", HYSCAN_TYPE_MARK_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_geomark_layer_init (HyScanGtkMapGeomarkLayer *gm_layer)
{
  gm_layer->priv = hyscan_gtk_map_geomark_layer_get_instance_private (gm_layer);
}

static void
hyscan_gtk_map_geomark_layer_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (object);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_geomark_layer_object_constructed (GObject *object)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (object);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_geomark_layer_parent_class)->constructed (object);

  g_rw_lock_init (&priv->mark_lock);
  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_geomark_layer_location_free);

  /* Оформление по умолчанию. */
  priv->line_width = 1.0;
  gdk_rgba_parse (&priv->color,          "#B25D43");
  gdk_rgba_parse (&priv->color_hover,    "#9443B2");
  gdk_rgba_parse (&priv->color_pending,  "rgba(0,0,0,0.5)");
  gdk_rgba_parse (&priv->color_blackout, "rgba(0,0,0,0.5)");

  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_map_geomark_layer_model_changed), gm_layer);
}

static void
hyscan_gtk_map_geomark_layer_object_finalize (GObject *object)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (object);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->model, gm_layer);

  g_clear_object (&priv->model);
  g_rw_lock_clear (&priv->mark_lock);
  g_free (priv->hover_handle_id);
  g_free (priv->hover_id);
  g_clear_pointer (&priv->marks, g_hash_table_destroy);

  G_OBJECT_CLASS (hyscan_gtk_map_geomark_layer_parent_class)->finalize (object);
}

/* Освобождает память, занятую HyScanGtkMapGeomarkLayerLocation. */
static void
hyscan_gtk_map_geomark_layer_location_free (HyScanGtkMapGeomarkLayerLocation *location)
{
  hyscan_mark_free ((HyScanMark *) location->mark);
  g_free (location->mark_id);
  g_slice_free (HyScanGtkMapGeomarkLayerLocation, location);
}

/* Копирует местоположение метки. */
static HyScanGtkMapGeomarkLayerLocation *
hyscan_gtk_map_geomark_layer_location_copy (HyScanGtkMapGeomarkLayerLocation *location)
{
  HyScanGtkMapGeomarkLayerLocation *copy;

  copy = g_slice_new (HyScanGtkMapGeomarkLayerLocation);
  copy->mark_id = g_strdup (location->mark_id);
  copy->pending = location->pending;
  copy->height = location->height;
  copy->width = location->width;
  copy->c2d = location->c2d;
  copy->mark = (HyScanMarkGeo *) hyscan_mark_copy ((HyScanMark *) location->mark);

  return copy;
}

/* Проецирует метку на карту.
 * Функция должна вызываться за g_rw_lock_writer_lock (&priv->mark_lock). */
static void
hyscan_gtk_map_geomark_layer_project_location (HyScanGtkMapGeomarkLayer         *layer,
                                               HyScanGtkMapGeomarkLayerLocation *location)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = layer->priv;
  HyScanMarkGeo *mark = location->mark;
  gdouble scale;
  gdouble width2, height2;

  if (priv->map == NULL)
    return;

  /* Определяем размеры метки в логической СК. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, &mark->center);
  location->width = DISTANCE_TO_METERS * mark->width / scale;
  location->height = DISTANCE_TO_METERS * mark->height / scale;

  /* Находим координаты центра и вершин прямоугольника. */
  hyscan_gtk_map_geo_to_value (priv->map, mark->center, &location->c2d);

  width2 = location->width / 2.0;
  height2 = location->height / 2.0;

  location->corner[0].x = location->c2d.x - width2;
  location->corner[0].y = location->c2d.y - height2;

  location->corner[1].x = location->c2d.x + width2;
  location->corner[1].y = location->c2d.y - height2;

  location->corner[2].x = location->c2d.x + width2;
  location->corner[2].y = location->c2d.y + height2;

  location->corner[3].x = location->c2d.x - width2;
  location->corner[3].y = location->c2d.y + height2;
}

/* Добавляет метку в список меток слоя.
 * Функция должна вызываться за g_rw_lock_writer_lock (&priv->mark_lock). */
static gboolean
hyscan_gtk_map_geomark_layer_insert_mark (gpointer  key,
                                          gpointer  value,
                                          gpointer  user_data)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (user_data);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLayerLocation *location;

  location = g_slice_new0 (HyScanGtkMapGeomarkLayerLocation);
  location->mark = value;
  location->mark_id = g_strdup (key);

  hyscan_gtk_map_geomark_layer_project_location (gm_layer, location);
  g_hash_table_insert (priv->marks, key, location);

  return TRUE;
}

static void
hyscan_gtk_map_geomark_layer_model_changed (HyScanGtkMapGeomarkLayer *gm_layer)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  GHashTable *marks;

  marks = hyscan_mark_model_get (priv->model);

  g_rw_lock_writer_lock (&priv->mark_lock);

  g_hash_table_remove_all (priv->marks);
  g_hash_table_foreach_steal (marks, hyscan_gtk_map_geomark_layer_insert_mark, gm_layer);
  priv->count = g_hash_table_size (priv->marks);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_hash_table_unref (marks);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_geomark_layer_draw_location (HyScanGtkMapGeomarkLayer         *gm_layer,
                                            cairo_t                          *cairo,
                                            HyScanGtkMapGeomarkLayerLocation *location,
                                            gboolean                          blackout)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  gdouble x, y;
  gdouble scale_x, scale_y;
  gdouble width, height;
  gdouble dot_radius;
  GdkRGBA *color;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, location->c2d.x, location->c2d.y);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, &scale_y);
  width = location->width / scale_x;
  height = location->height / scale_y;

  cairo_save (cairo);
  cairo_translate (cairo, x, y);

  cairo_rectangle (cairo, -width / 2.0, -height / 2.0, width, height);

  /* Рисуем затемнение вокруг метки. */
  if (blackout)
    {
      guint w, h;

      gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->map), &w, &h);
      cairo_rectangle (cairo, -x, -y, w, h);
      cairo_set_source_rgba (cairo, 0, 0, 0, 0.5);
      cairo_set_fill_rule (cairo, CAIRO_FILL_RULE_EVEN_ODD);
      cairo_fill_preserve (cairo);
    }

  /* Определяем цвет. */
  if (location->pending)
    color = &priv->color_pending;
  else if (priv->hover_id != NULL && g_strcmp0 (location->mark_id, priv->hover_id) == 0)
    color = &priv->color_hover;
  else
    color = &priv->color;

  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, color);

  /* Контур метки. */
  cairo_stroke (cairo);

  /* Центральный хэндл. */
  dot_radius = 2.0;
  cairo_arc (cairo, 0, 0, dot_radius, 0, 2.0 * G_PI);
  cairo_fill (cairo);

  /* Угловые хэндлы. */
  cairo_arc (cairo, -width / 2.0, -height / 2.0, dot_radius, 0, 2.0 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo,  width / 2.0, -height / 2.0, dot_radius, 0, 2.0 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo, -width / 2.0,  height / 2.0, dot_radius, 0, 2.0 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo,  width / 2.0,  height / 2.0, dot_radius, 0, 2.0 * G_PI);
  cairo_fill (cairo);

  cairo_restore (cairo);
}

/* Функция должна вызываться за g_rw_lock_reader_lock(&priv->mark_lock). */
static gchar *
hyscan_gtk_map_geomark_layer_handle_at (HyScanGtkMapGeomarkLayer *gm_layer,
                                        gdouble                   x,
                                        gdouble                   y,
                                        guint                    *mode,
                                        HyScanGeoCartesian2D     *point)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGeoCartesian2D cursor;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLayerLocation *location;
  gchar *mark_id;

  gdouble scale;
  gdouble max_dist;

  gdouble handle_dist = G_MAXDOUBLE;
  guint handle_mode = MODE_NONE;
  HyScanGeoCartesian2D handle_point = {0, 0};
  const gchar *handle_id = NULL;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), x, y, &cursor.x, &cursor.y);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
  max_dist = HOVER_RADIUS * scale;

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &location))
    {
      guint i;
      gdouble dist;

      if (location->pending)
        continue;

      dist = hyscan_cartesian_distance (&location->c2d, &cursor);
      if (dist < max_dist && dist < handle_dist)
        {
          handle_dist = dist;
          handle_mode = MODE_DRAG;
          handle_id = mark_id;
          handle_point = location->c2d;
        }

      for (i = 0; i < G_N_ELEMENTS (location->corner); ++i)
        {
          HyScanGeoCartesian2D *corner = &location->corner[i];

          dist = hyscan_cartesian_distance (corner, &cursor);
          if (dist < max_dist && dist < handle_dist)
            {
              handle_dist = dist;
              handle_mode = MODE_RESIZE;
              handle_id = mark_id;
              handle_point = *corner;
            }
        }
    }

  if (mode != NULL)
    *mode = handle_mode;

  if (point != NULL)
    *point = handle_point;

  return g_strdup (handle_id);
}

/* Рисует хэндл, над которым находится курсор мыши. */
static void
hyscan_gtk_map_geomark_layer_draw_handle (HyScanGtkMapGeomarkLayer *gm_layer,
                                          cairo_t                  *cairo)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  GdkRGBA *color;
  gdouble x, y;

  if (priv->hover_handle_id == NULL)
    return;

  color = g_strcmp0 (priv->hover_id, priv->hover_handle_id) == 0 ? &priv->color_hover : &priv->color;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y,
                                         priv->hover_handle.x, priv->hover_handle.y);
  cairo_arc (cairo, x, y, HOVER_RADIUS, 0, 2 * G_PI);
  gdk_cairo_set_source_rgba (cairo, color);
  cairo_fill (cairo);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_geomark_layer_draw (GtkCifroArea             *carea,
                                   cairo_t                  *cairo,
                                   HyScanGtkMapGeomarkLayer *gm_layer)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLayerLocation *location;
  gchar *mark_id;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (gm_layer)))
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &location))
    {
      /* Пропускаем активную метку. */
      if (priv->drag_mark_id != NULL && g_str_equal (mark_id, priv->drag_mark_id))
        continue;

      hyscan_gtk_map_geomark_layer_draw_location (gm_layer, cairo, location, FALSE);
    }

  /* Рисуем активную метку. */
  if (priv->drag_mark != NULL)
    hyscan_gtk_map_geomark_layer_draw_location (gm_layer, cairo, priv->drag_mark, TRUE);
  
  /* Если активной метки нет, то рисуем хэндл, за который можно активировать метку. */
  else
    hyscan_gtk_map_geomark_layer_draw_handle (gm_layer, cairo);

  g_rw_lock_reader_unlock (&priv->mark_lock);
}

/* Обработка движения мыши в режиме изменения размера. */
static void
hyscan_gtk_map_geomark_layer_resize (HyScanGtkMapGeomarkLayer *gm_layer,
                                     GdkEventMotion           *event)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLayerLocation *location = priv->drag_mark;
  gdouble x, y;

  g_return_if_fail (location != NULL);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &x, &y);
  location->width = 2.0 * ABS (x - location->c2d.x);
  location->height = 2.0 * ABS (y - location->c2d.y);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обработка движения мыши в обычном режиме. */
static void
hyscan_gtk_map_geomark_layer_hover (HyScanGtkMapGeomarkLayer *gm_layer,
                                    GdkEventMotion           *event)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  gchar *mark_id;

  mark_id = hyscan_gtk_map_geomark_layer_handle_at (gm_layer, event->x, event->y, NULL, &priv->hover_handle);

  if (mark_id != NULL || priv->hover_handle_id != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_free (priv->hover_handle_id);
  priv->hover_handle_id = mark_id;
}

/* Обработка движения мыши в режиме перемещения. */
static void
hyscan_gtk_map_geomark_layer_drag (HyScanGtkMapGeomarkLayer *gm_layer,
                                   GdkEventMotion           *event)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLayerLocation *location = priv->drag_mark;

  g_return_if_fail (location != NULL);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y,
                                 &location->c2d.x, &location->c2d.y);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обработчки ::motion-notify-event.
 * Определяет метку под курсором мыши. */
static gboolean
hyscan_gtk_map_geomark_layer_motion_notify (GtkWidget      *widget,
                                            GdkEventMotion *event,
                                            gpointer        user_data)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (user_data);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  if (priv->mode == MODE_NONE)
    hyscan_gtk_map_geomark_layer_hover (gm_layer, event);

  else if (priv->mode == MODE_RESIZE)
    hyscan_gtk_map_geomark_layer_resize (gm_layer, event);

  else if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_geomark_layer_drag (gm_layer, event);

  else
    g_warn_if_reached ();

  return GDK_EVENT_PROPAGATE;
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_geomark_layer_configure (HyScanGtkMapGeomarkLayer *gm_layer,
                                        GdkEvent                 *screen)
{
  // todo: implement

  return FALSE;
}

/* Обработчик сигнала HyScanGtkMap::notify::projection.
 * Пересчитывает координаты меток, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_geomark_layer_proj_notify (HyScanGtkMap *map,
                                          GParamSpec   *pspec,
                                          gpointer      user_data)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (user_data);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLayerLocation *location;

  g_rw_lock_writer_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_gtk_map_geomark_layer_project_location (gm_layer, location);

  g_rw_lock_writer_unlock (&priv->mark_lock);
}

/* Обработка "button-release-event" на слое.
 * Создаёт новую точку в том месте, где пользователь кликнул мышью. */
static gboolean
hyscan_gtk_map_geomark_layer_button_release (GtkWidget      *widget,
                                             GdkEventButton *event,
                                             HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (layer);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  HyScanGtkLayerContainer *container;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);

  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return GDK_EVENT_PROPAGATE;

  /* Проверяем, что у нас есть право ввода. */
  if (hyscan_gtk_layer_container_get_input_owner (container) != layer)
    return GDK_EVENT_PROPAGATE;

  /* Если слой не видно, то никак не реагируем. */
  if (!hyscan_gtk_layer_get_visible (layer))
    return GDK_EVENT_PROPAGATE;

  if (priv->mode != MODE_NONE)
    return GDK_EVENT_PROPAGATE;

  /* Создаём новую метку. */
  {
    HyScanGtkMapGeomarkLayerLocation *location;
    HyScanGeoGeodetic center;
    gchar *mark_name;

    g_rw_lock_writer_lock (&priv->mark_lock);

    location = g_slice_new0 (HyScanGtkMapGeomarkLayerLocation);
    location->mark = (HyScanMarkGeo *) hyscan_mark_new (HYSCAN_MARK_GEO);
    gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &location->c2d.x, &location->c2d.y);

    mark_name = g_strdup_printf ("Geo Mark #%d", ++priv->count);
    hyscan_gtk_map_value_to_geo (priv->map, &center, location->c2d);
    hyscan_mark_geo_set_center (location->mark, center);
    hyscan_mark_set_size ((HyScanMark *) location->mark, 0, 0);
    hyscan_mark_set_text ((HyScanMark *) location->mark, mark_name, "", "");
    hyscan_mark_set_ctime ((HyScanMark *) location->mark, g_get_real_time ());
    g_free (mark_name);

    /* Захватывем хэндл и начинаем изменение размера. */
    priv->mode = MODE_RESIZE;
    priv->drag_mark = location;

    g_rw_lock_writer_unlock (&priv->mark_lock);
  }

  hyscan_gtk_layer_container_set_handle_grabbed (container, layer);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

static void
hyscan_gtk_map_geomark_layer_drag_clear (HyScanGtkMapGeomarkLayer *gm_layer,
                                         gboolean                  free_)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  if (free_)
    {
      g_free (priv->drag_mark_id);
      hyscan_gtk_map_geomark_layer_location_free (priv->drag_mark);
    }

  priv->mode = MODE_NONE;
  priv->drag_mark = NULL;
  priv->drag_mark_id = NULL;
}

/* Обработчик сигнала "handle-release".
 * Возвращает %TRUE, если мы разрешаем отпустить хэндл. */
static gboolean
hyscan_gtk_map_geomark_layer_handle_release (HyScanGtkLayerContainer  *container,
                                             GdkEventMotion           *event,
                                             gconstpointer             howner,
                                             HyScanGtkMapGeomarkLayer *gm_layer)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLayerLocation *location;
  gchar *mark_id;

  HyScanMark *mark;
  HyScanGeoGeodetic center;
  gdouble scale;

  if (howner != gm_layer)
    return FALSE;

  if (priv->mode == MODE_NONE)
    return FALSE;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Копируем информацию о метке. */
  location = hyscan_gtk_map_geomark_layer_location_copy (priv->drag_mark);
  mark_id = g_strdup (priv->drag_mark_id);

  /* Помещаем метку в общий список. */
  priv->drag_mark->pending = TRUE;
  if (priv->drag_mark_id != NULL)
    g_hash_table_insert (priv->marks, priv->drag_mark_id, priv->drag_mark);
  else
    hyscan_gtk_map_geomark_layer_location_free (priv->drag_mark);

  /* Устанавливаем состояние в MODE_NONE. */
  hyscan_gtk_map_geomark_layer_drag_clear (gm_layer, FALSE);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  /* Устанавливаем новые параметры метки. */
  hyscan_gtk_map_value_to_geo (priv->map, &center, location->c2d);
  hyscan_mark_geo_set_center (location->mark, center);

  mark = (HyScanMark *) location->mark;
  scale = 1.0 / hyscan_gtk_map_get_value_scale (priv->map, &location->mark->center);
  scale *= DISTANCE_TO_METERS;
  hyscan_mark_set_size (mark, (guint) (location->width / scale), (guint) (location->height / scale));
  hyscan_mark_set_mtime (mark, g_get_real_time ());

  /* Обновляем модель меток. */
  if (mark_id == NULL)
    hyscan_mark_model_add_mark (priv->model, mark);
  else
    hyscan_mark_model_modify_mark (priv->model, mark_id, mark);

  hyscan_gtk_map_geomark_layer_location_free (location);
  g_free (mark_id);

  /* Перерисовываем. */
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

/* Переводит слой в режим изменения размера метки или перемещения. */
static gconstpointer
hyscan_gtk_map_geomark_layer_start_mode (HyScanGtkMapGeomarkLayer *gm_layer,
                                         const gchar              *mark_id,
                                         guint                     mode)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLayerLocation *location;

  /* Хэндл работает только в двух режимах: изменение размера и перемещение метки. */
  if (mode != MODE_RESIZE && mode != MODE_DRAG)
    return NULL;

  g_rw_lock_writer_lock (&priv->mark_lock);

  location = g_hash_table_lookup (priv->marks, mark_id);
  if (location == NULL)
    {
      g_rw_lock_writer_unlock (&priv->mark_lock);
      return NULL;
    }

  g_clear_pointer (&priv->hover_handle_id, g_free);

  priv->mode = mode;
  priv->drag_mark = hyscan_gtk_map_geomark_layer_location_copy (location);
  priv->drag_mark_id = g_strdup (mark_id);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));

  return gm_layer;
}

/* Обработчик сигнала "handle-grab". Захватывает хэндл под указателем мыши. */
static gconstpointer
hyscan_gtk_map_geomark_layer_handle_grab (HyScanGtkLayerContainer  *container,
                                          GdkEventMotion           *event,
                                          HyScanGtkMapGeomarkLayer *layer)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = layer->priv;
  gconstpointer result = NULL;
  gchar *mark_id;
  guint handle_mode;

  g_rw_lock_reader_lock (&priv->mark_lock);
  mark_id = hyscan_gtk_map_geomark_layer_handle_at (layer, event->x, event->y, &handle_mode, NULL);
  g_rw_lock_reader_unlock (&priv->mark_lock);

  result = hyscan_gtk_map_geomark_layer_start_mode (layer, mark_id, handle_mode);

  g_free (mark_id);

  return result;
}

/* Обработка Gdk-событий, которые дошли из контейнера. */
static gboolean
hyscan_gtk_map_geomark_layer_key_press (HyScanGtkMapGeomarkLayer *gm_layer,
                                        GdkEventKey              *event)
{
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  if (priv->mode == MODE_NONE)
    return GDK_EVENT_PROPAGATE;

  /* Удаляем активную точку. */
  if (event->keyval == GDK_KEY_Delete)
    {
      gchar *mark_id;

      g_rw_lock_writer_lock (&priv->mark_lock);

      mark_id = g_strdup (priv->drag_mark_id);
      if (mark_id != NULL)
        g_hash_table_remove (priv->marks, priv->drag_mark_id);
      hyscan_gtk_map_geomark_layer_drag_clear (gm_layer, TRUE);

      g_rw_lock_writer_unlock (&priv->mark_lock);

      if (mark_id != NULL)
        hyscan_mark_model_remove_mark (priv->model, mark_id);
      g_free (mark_id);

      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  /* Отменяем текущее изменение. */
  else if (event->keyval == GDK_KEY_Escape)
    {
      g_rw_lock_writer_lock (&priv->mark_lock);

      hyscan_gtk_map_geomark_layer_drag_clear (gm_layer, TRUE);

      g_rw_lock_writer_unlock (&priv->mark_lock);

      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Обработка добавления слоя на карту. */
static void
hyscan_gtk_map_geomark_layer_added (HyScanGtkLayer          *gtk_layer,
                                    HyScanGtkLayerContainer *container)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (gtk_layer);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  g_signal_connect (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_geomark_layer_motion_notify), gm_layer);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_geomark_layer_draw), gm_layer);
  g_signal_connect_swapped (priv->map, "configure-event", G_CALLBACK (hyscan_gtk_map_geomark_layer_configure), gm_layer);
  g_signal_connect (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_geomark_layer_proj_notify), gm_layer);
  g_signal_connect (priv->map, "handle-release", G_CALLBACK (hyscan_gtk_map_geomark_layer_handle_release), gm_layer);
  g_signal_connect (priv->map, "handle-grab", G_CALLBACK (hyscan_gtk_map_geomark_layer_handle_grab), gm_layer);
  g_signal_connect_swapped (priv->map, "key-press-event", G_CALLBACK (hyscan_gtk_map_geomark_layer_key_press), gm_layer);

  g_signal_connect_after (container, "button-release-event",
                          G_CALLBACK (hyscan_gtk_map_geomark_layer_button_release), gm_layer);
}

/* Обработка удаления слоя с карты. */
static void
hyscan_gtk_map_geomark_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (gtk_layer);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gm_layer);
  g_clear_object (&priv->map);
}

/* Захватывает пользовательский ввод в контейнере.
 * Реализация HyScanGtkLayerInterface.grab_input(). */
static gboolean
hyscan_gtk_map_geomark_layer_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (layer);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  if (priv->map == NULL)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);

  return TRUE;
}

/* Устанавливает видимость слоя.
 * Реализация HyScanGtkLayerInterface.set_visible(). */
static void
hyscan_gtk_map_geomark_set_visible (HyScanGtkLayer *layer,
                                    gboolean        visible)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (layer);
  HyScanGtkMapGeomarkLayerPrivate *priv = gm_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Возвращает видимость слоя.
 * Реализация HyScanGtkLayerInterface.get_visible(). */
static gboolean
hyscan_gtk_map_geomark_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomarkLayer *gm_layer = HYSCAN_GTK_MAP_GEOMARK_LAYER (layer);

  return gm_layer->priv->visible;
}

static void
hyscan_gtk_map_geomark_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_geomark_layer_added;
  iface->removed = hyscan_gtk_map_geomark_layer_removed;
  iface->grab_input = hyscan_gtk_map_geomark_layer_grab_input;
  iface->get_visible = hyscan_gtk_map_geomark_get_visible;
  iface->set_visible = hyscan_gtk_map_geomark_set_visible;
}

/**
 * hyscan_gtk_map_geomark_layer_new:
 * @model: указатель на #HyScanMarkModel
 *
 * Returns: новый объект #HyScanGtkMapGeomarkLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_geomark_layer_new (HyScanMarkModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GEOMARK_LAYER,
                       "model", model, NULL);
}

/**
 * hyscan_gtk_map_wfmark_layer_mark_view:
 * @gm_layer: указатель на #HyScanGtkMapGeomarkLayer
 * @mark_id: идентификатор метки
 *
 * Перемещает область видимости карты в положение метки @mark_id и выделяет
 * эту метку.
 */
void
hyscan_gtk_map_geomark_layer_mark_view (HyScanGtkMapGeomarkLayer *gm_layer,
                                        const gchar              *mark_id)
{
  HyScanGtkMapGeomarkLayerPrivate *priv;
  HyScanGtkMapGeomarkLayerLocation *location;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_GEOMARK_LAYER (gm_layer));
  priv = gm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  location = g_hash_table_lookup (priv->marks, mark_id);
  if (location != NULL)
    {
      gdouble x_margin, y_margin;

      x_margin = 0.1 * location->width;
      y_margin = 0.1 * location->height;
      gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map),
                               location->corner[0].x - x_margin, location->corner[2].x + x_margin,
                               location->corner[0].y - y_margin, location->corner[2].y + y_margin);
      g_free (priv->hover_id);
      priv->hover_id = g_strdup (location->mark_id);
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
}
