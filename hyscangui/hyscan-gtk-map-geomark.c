/* hyscan-gtk-map-geomark.c
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
 * SECTION: hyscan-gtk-map-geomark
 * @Short_description: Слой с географическим метками
 * @Title: HyScanGtkMapGeomark
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Слой #HyScanGtkMapGeomark выводит на карте записанные в проект географические
 * метки, а также позволяет пользователю:
 * - создавать новый метки,
 * - изменять размер существующих меток,
 * - удалять метки.
 *
 */

#include "hyscan-gtk-map-geomark.h"
#include "hyscan-gtk-map.h"
#include <hyscan-cartesian.h>

#define HOVER_RADIUS            7                             /* Радиус хэндла. */
#define DEFAULT_LINE_WIDTH      1.0
#define DEFAULT_COLOR           "#B25D43"
#define DEFAULT_HOVER_COLOR     "#9443B2"
#define DEFAULT_PENDING_COLOR   "rgba(0,0,0,0.5)"
#define DEFAULT_BLACKOUT_COLOR  "rgba(0,0,0,0.5)"

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
} HyScanGtkMapGeomarkLocation;

struct _HyScanGtkMapGeomarkPrivate
{
  HyScanGtkMap                            *map;                /* Карта. */
  HyScanMarkModel                         *model;              /* Модель меток. */

  gboolean                                 visible;            /* Признак видимости слоя. */

  GRWLock                                  mark_lock;          /* Блокировка доступа к меткам. */
  GHashTable                              *marks;              /* Список меток на слое. */
  gint                                     count;              /* Счётчик количества меток. */

  guint                                    mode;               /* Режим работы слоя (тип взаимодействия). */
  HyScanGtkMapGeomarkLocation             *drag_mark;          /* Активная метка, с которой идёт взаимодействие. */
  gchar                                   *drag_mark_id;       /* Идентификатор активной метки drag_mark. */

  gchar                                   *hover_id;           /* Идентификатор метки под курсором мыши */
  gchar                                   *active_mark_id;     /* Идентификатор активной метки */

  gchar                                   *hover_handle_id;    /* Идентификатор метки, чей хэндл под курсором мыши. */
  HyScanGeoCartesian2D                     hover_handle;       /* Координаты хэндла. */

  /* Стиль оформления. */
  gdouble                                  line_width;         /* Толщина линий. */
  GdkRGBA                                  color;              /* Основной цвет. */
  GdkRGBA                                  color_hover;        /* Цвет метки под курсором мыши. */
  GdkRGBA                                  color_pending;      /* Цвет обрабатываемой метки. */
  GdkRGBA                                  color_blackout;     /* Цвет затемнения. */
};

static void    hyscan_gtk_map_geomark_interface_init       (HyScanGtkLayerInterface          *iface);
static void    hyscan_gtk_map_geomark_set_property         (GObject                          *object,
                                                            guint                             prop_id,
                                                            const GValue                     *value,
                                                            GParamSpec                       *pspec);
static void    hyscan_gtk_map_geomark_object_constructed   (GObject                          *object);
static void    hyscan_gtk_map_geomark_object_finalize      (GObject                          *object);
static void    hyscan_gtk_map_geomark_model_changed        (HyScanGtkMapGeomark         *gm_layer);
static void    hyscan_gtk_map_geomark_location_free        (HyScanGtkMapGeomarkLocation *location);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapGeomark, hyscan_gtk_map_geomark, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapGeomark)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_geomark_interface_init))

static void
hyscan_gtk_map_geomark_class_init (HyScanGtkMapGeomarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_geomark_set_property;
  object_class->constructed = hyscan_gtk_map_geomark_object_constructed;
  object_class->finalize = hyscan_gtk_map_geomark_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Geo mark model",
                         "The HyScanMarkModel with geo marks", HYSCAN_TYPE_MARK_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_geomark_init (HyScanGtkMapGeomark *gm_layer)
{
  gm_layer->priv = hyscan_gtk_map_geomark_get_instance_private (gm_layer);
}

static void
hyscan_gtk_map_geomark_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (object);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

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
hyscan_gtk_map_geomark_object_constructed (GObject *object)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (object);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_geomark_parent_class)->constructed (object);

  g_rw_lock_init (&priv->mark_lock);
  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_geomark_location_free);

  /* Оформление по умолчанию. */
  priv->line_width = DEFAULT_LINE_WIDTH;
  gdk_rgba_parse (&priv->color,          DEFAULT_COLOR);
  gdk_rgba_parse (&priv->color_hover,    DEFAULT_HOVER_COLOR);
  gdk_rgba_parse (&priv->color_pending,  DEFAULT_PENDING_COLOR);
  gdk_rgba_parse (&priv->color_blackout, DEFAULT_BLACKOUT_COLOR);

  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_map_geomark_model_changed), gm_layer);
}

static void
hyscan_gtk_map_geomark_object_finalize (GObject *object)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (object);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->model, gm_layer);

  g_clear_object (&priv->model);
  g_rw_lock_clear (&priv->mark_lock);
  g_free (priv->hover_handle_id);
  g_free (priv->hover_id);
  g_free (priv->active_mark_id);
  g_clear_pointer (&priv->marks, g_hash_table_destroy);

  G_OBJECT_CLASS (hyscan_gtk_map_geomark_parent_class)->finalize (object);
}

/* Освобождает память, занятую HyScanGtkMapGeomarkLocation. */
static void
hyscan_gtk_map_geomark_location_free (HyScanGtkMapGeomarkLocation *location)
{
  hyscan_mark_free ((HyScanMark *) location->mark);
  g_free (location->mark_id);
  g_slice_free (HyScanGtkMapGeomarkLocation, location);
}

/* Копирует местоположение метки. */
static HyScanGtkMapGeomarkLocation *
hyscan_gtk_map_geomark_location_copy (HyScanGtkMapGeomarkLocation *location)
{
  HyScanGtkMapGeomarkLocation *copy;

  copy = g_slice_new (HyScanGtkMapGeomarkLocation);
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
hyscan_gtk_map_geomark_project_location (HyScanGtkMapGeomark         *layer,
                                         HyScanGtkMapGeomarkLocation *location)
{
  HyScanGtkMapGeomarkPrivate *priv = layer->priv;
  HyScanMarkGeo *mark = location->mark;
  gdouble scale;
  gdouble width2, height2;

  if (priv->map == NULL)
    return;

  /* Определяем размеры метки в логической СК. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, mark->center);
  location->width = mark->width / scale;
  location->height = mark->height / scale;

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
hyscan_gtk_map_geomark_insert_mark (gpointer  key,
                                    gpointer  value,
                                    gpointer  user_data)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (user_data);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLocation *location;

  location = g_slice_new0 (HyScanGtkMapGeomarkLocation);
  location->mark = value;
  location->mark_id = g_strdup (key);

  hyscan_gtk_map_geomark_project_location (gm_layer, location);
  g_hash_table_insert (priv->marks, key, location);

  return TRUE;
}

static void
hyscan_gtk_map_geomark_model_changed (HyScanGtkMapGeomark *gm_layer)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  GHashTable *marks;

  marks = hyscan_mark_model_get (priv->model);

  g_rw_lock_writer_lock (&priv->mark_lock);

  g_hash_table_remove_all (priv->marks);
  g_hash_table_foreach_steal (marks, hyscan_gtk_map_geomark_insert_mark, gm_layer);
  priv->count = g_hash_table_size (priv->marks);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_hash_table_unref (marks);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_geomark_draw_location (HyScanGtkMapGeomark         *gm_layer,
                                      cairo_t                     *cairo,
                                      HyScanGtkMapGeomarkLocation *location,
                                      gboolean                     blackout)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
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

  if (priv->active_mark_id != NULL && g_strcmp0 (location->mark_id, priv->active_mark_id) == 0)
    cairo_set_line_width (cairo, 2.0 * priv->line_width);
  else
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
hyscan_gtk_map_geomark_handle_at (HyScanGtkMapGeomark  *gm_layer,
                                  gdouble               x,
                                  gdouble               y,
                                  guint                *mode,
                                  HyScanGeoCartesian2D *point)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGeoCartesian2D cursor;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLocation *location;
  gchar *mark_id;

  gdouble scale;
  gdouble max_dist;

  gdouble handle_dist = G_MAXDOUBLE;
  guint handle_mode = MODE_NONE;
  HyScanGeoCartesian2D handle_point = {0, 0};
  const gchar *handle_id = NULL;

  gconstpointer howner;

  /* Никак не реагируем на точки, если выполнено хотя бы одно из условий (1-3): */

  /* 1. какой-то хэндл захвачен, ... */
  howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
  if (howner != NULL)
    return NULL;

  /* 2. редактирование запрещено, ... */
  if (!hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (priv->map)))
    return NULL;

  /* 3. слой не отображается. */
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (gm_layer)))
    return NULL;

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
hyscan_gtk_map_geomark_draw_handle (HyScanGtkMapGeomark *gm_layer,
                                    cairo_t             *cairo)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

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
hyscan_gtk_map_geomark_draw (GtkCifroArea        *carea,
                             cairo_t             *cairo,
                             HyScanGtkMapGeomark *gm_layer)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLocation *location;
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

      hyscan_gtk_map_geomark_draw_location (gm_layer, cairo, location, FALSE);
    }

  /* Рисуем активную метку. */
  if (priv->drag_mark != NULL)
    hyscan_gtk_map_geomark_draw_location (gm_layer, cairo, priv->drag_mark, TRUE);
  
  /* Если активной метки нет, то рисуем хэндл, за который можно активировать метку. */
  else
    hyscan_gtk_map_geomark_draw_handle (gm_layer, cairo);

  g_rw_lock_reader_unlock (&priv->mark_lock);
}

/* Обработка движения мыши в режиме изменения размера. */
static void
hyscan_gtk_map_geomark_resize (HyScanGtkMapGeomark *gm_layer,
                               GdkEventMotion      *event)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLocation *location = priv->drag_mark;
  gdouble x, y;

  g_return_if_fail (location != NULL);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &x, &y);
  location->width = 2.0 * ABS (x - location->c2d.x);
  location->height = 2.0 * ABS (y - location->c2d.y);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обработка движения мыши в обычном режиме. */
static void
hyscan_gtk_map_geomark_hover (HyScanGtkMapGeomark *gm_layer,
                              GdkEventMotion      *event)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  gchar *mark_id;

  mark_id = hyscan_gtk_map_geomark_handle_at (gm_layer, event->x, event->y, NULL, &priv->hover_handle);

  if (mark_id != NULL || priv->hover_handle_id != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_free (priv->hover_handle_id);
  priv->hover_handle_id = mark_id;
}

/* Обработка движения мыши в режиме перемещения. */
static void
hyscan_gtk_map_geomark_drag (HyScanGtkMapGeomark *gm_layer,
                             GdkEventMotion      *event)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLocation *location = priv->drag_mark;

  g_return_if_fail (location != NULL);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y,
                                 &location->c2d.x, &location->c2d.y);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обработчки ::motion-notify-event.
 * Определяет метку под курсором мыши. */
static gboolean
hyscan_gtk_map_geomark_motion_notify (GtkWidget      *widget,
                                      GdkEventMotion *event,
                                      gpointer        user_data)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (user_data);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  if (priv->mode == MODE_NONE)
    hyscan_gtk_map_geomark_hover (gm_layer, event);

  else if (priv->mode == MODE_RESIZE)
    hyscan_gtk_map_geomark_resize (gm_layer, event);

  else if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_geomark_drag (gm_layer, event);

  else
    g_warn_if_reached ();

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик сигнала HyScanGtkMap::notify::projection.
 * Пересчитывает координаты меток, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_geomark_proj_notify (HyScanGtkMap *map,
                                    GParamSpec   *pspec,
                                    gpointer      user_data)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (user_data);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapGeomarkLocation *location;

  g_rw_lock_writer_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_gtk_map_geomark_project_location (gm_layer, location);

  g_rw_lock_writer_unlock (&priv->mark_lock);
}

/* Обработка "button-release-event" на слое.
 * Создаёт новую точку в том месте, где пользователь кликнул мышью. */
static gboolean
hyscan_gtk_map_geomark_button_release (GtkWidget      *widget,
                                       GdkEventButton *event,
                                       HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

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
    HyScanGtkMapGeomarkLocation *location;
    HyScanGeoGeodetic center;
    gchar *mark_name;

    g_rw_lock_writer_lock (&priv->mark_lock);

    location = g_slice_new0 (HyScanGtkMapGeomarkLocation);
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
hyscan_gtk_map_geomark_drag_clear (HyScanGtkMapGeomark *gm_layer,
                                   gboolean             free_)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  if (free_)
    {
      g_free (priv->drag_mark_id);
      hyscan_gtk_map_geomark_location_free (priv->drag_mark);
    }

  priv->mode = MODE_NONE;
  priv->drag_mark = NULL;
  priv->drag_mark_id = NULL;
}

/* Обработчик сигнала "handle-release".
 * Возвращает %TRUE, если мы разрешаем отпустить хэндл. */
static gboolean
hyscan_gtk_map_geomark_handle_release (HyScanGtkLayerContainer *container,
                                       GdkEventMotion          *event,
                                       gconstpointer            howner,
                                       HyScanGtkMapGeomark     *gm_layer)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLocation *location;
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
  location = hyscan_gtk_map_geomark_location_copy (priv->drag_mark);
  mark_id = g_strdup (priv->drag_mark_id);

  /* Помещаем метку в общий список. */
  priv->drag_mark->pending = TRUE;
  if (priv->drag_mark_id != NULL)
    g_hash_table_insert (priv->marks, priv->drag_mark_id, priv->drag_mark);
  else
    hyscan_gtk_map_geomark_location_free (priv->drag_mark);

  /* Устанавливаем состояние в MODE_NONE. */
  hyscan_gtk_map_geomark_drag_clear (gm_layer, FALSE);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  /* Устанавливаем новые параметры метки. */
  hyscan_gtk_map_value_to_geo (priv->map, &center, location->c2d);
  hyscan_mark_geo_set_center (location->mark, center);

  mark = (HyScanMark *) location->mark;
  scale = 1.0 / hyscan_gtk_map_get_value_scale (priv->map, location->mark->center);
  hyscan_mark_set_size (mark, location->width / scale, location->height / scale);
  hyscan_mark_set_mtime (mark, g_get_real_time ());

  /* Обновляем модель меток. */
  if (mark_id == NULL)
    hyscan_mark_model_add_mark (priv->model, mark);
  else
    hyscan_mark_model_modify_mark (priv->model, mark_id, mark);

  hyscan_gtk_map_geomark_location_free (location);
  g_free (mark_id);

  /* Перерисовываем. */
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

/* Переводит слой в режим изменения размера метки или перемещения. */
static gconstpointer
hyscan_gtk_map_geomark_start_mode (HyScanGtkMapGeomark *gm_layer,
                                   const gchar         *mark_id,
                                   guint                mode)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  HyScanGtkMapGeomarkLocation *location;

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
  priv->drag_mark = hyscan_gtk_map_geomark_location_copy (location);
  priv->drag_mark_id = g_strdup (mark_id);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));

  return gm_layer;
}

/* Обработчик сигнала "handle-grab". Захватывает хэндл под указателем мыши. */
static gconstpointer
hyscan_gtk_map_geomark_handle_grab (HyScanGtkLayerContainer *container,
                                    GdkEventMotion          *event,
                                    HyScanGtkMapGeomark     *layer)
{
  HyScanGtkMapGeomarkPrivate *priv = layer->priv;
  gconstpointer result = NULL;
  gchar *mark_id;
  guint handle_mode;

  g_rw_lock_reader_lock (&priv->mark_lock);
  mark_id = hyscan_gtk_map_geomark_handle_at (layer, event->x, event->y, &handle_mode, NULL);
  g_rw_lock_reader_unlock (&priv->mark_lock);

  result = hyscan_gtk_map_geomark_start_mode (layer, mark_id, handle_mode);

  g_free (mark_id);

  return result;
}

/* Обработка Gdk-событий, которые дошли из контейнера. */
static gboolean
hyscan_gtk_map_geomark_key_press (HyScanGtkMapGeomark *gm_layer,
                                  GdkEventKey         *event)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

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
      hyscan_gtk_map_geomark_drag_clear (gm_layer, TRUE);

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

      hyscan_gtk_map_geomark_drag_clear (gm_layer, TRUE);

      g_rw_lock_writer_unlock (&priv->mark_lock);

      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Обработка добавления слоя на карту. */
static void
hyscan_gtk_map_geomark_added (HyScanGtkLayer          *gtk_layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (gtk_layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  g_signal_connect (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_geomark_motion_notify), gm_layer);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_geomark_draw), gm_layer);
  g_signal_connect (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_geomark_proj_notify), gm_layer);
  g_signal_connect (priv->map, "handle-release", G_CALLBACK (hyscan_gtk_map_geomark_handle_release), gm_layer);
  g_signal_connect (priv->map, "handle-grab", G_CALLBACK (hyscan_gtk_map_geomark_handle_grab), gm_layer);
  g_signal_connect_swapped (priv->map, "key-press-event", G_CALLBACK (hyscan_gtk_map_geomark_key_press), gm_layer);

  g_signal_connect_after (container, "button-release-event",
                          G_CALLBACK (hyscan_gtk_map_geomark_button_release), gm_layer);
}

/* Обработка удаления слоя с карты. */
static void
hyscan_gtk_map_geomark_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (gtk_layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gm_layer);
  g_clear_object (&priv->map);
}

/* Захватывает пользовательский ввод в контейнере.
 * Реализация HyScanGtkLayerInterface.grab_input(). */
static gboolean
hyscan_gtk_map_geomark_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

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
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Возвращает видимость слоя.
 * Реализация HyScanGtkLayerInterface.get_visible(). */
static gboolean
hyscan_gtk_map_geomark_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);

  return gm_layer->priv->visible;
}

static void
hyscan_gtk_map_geomark_hint_shown (HyScanGtkLayer *layer,
                                   gboolean        shown)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;

  if (!shown)
    g_clear_pointer (&priv->hover_id, g_free);
}

/* Находит метку, которая находится в точке (x, y)
 * Функция должна вызываться за g_rw_lock_reader_lock (&priv->mark_lock); */
static const HyScanGtkMapGeomarkLocation *
hyscan_gtk_map_geomark_find_hover (HyScanGtkMapGeomark *gm_layer,
                                    gdouble             x,
                                    gdouble             y,
                                    gdouble            *distance)
{
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  GHashTableIter iter;
  HyScanGeoCartesian2D value;
  HyScanGtkMapGeomarkLocation *mark;
  gdouble min_distance = G_MAXDOUBLE;
  const HyScanGtkMapGeomarkLocation *hover = NULL;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), x, y, &value.x, &value.y);
  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &mark))
    {
      gdouble mark_distance;

      if (!hyscan_cartesian_is_point_inside (&value, &mark->corner[0], &mark->corner[2]))
        continue;

      mark_distance = hyscan_cartesian_distance (&value, &mark->c2d);
      if (mark_distance > min_distance)
        continue;

      min_distance = mark_distance;
      hover = mark;
    }

  *distance = min_distance;

  return hover;
}

static gchar *
hyscan_gtk_map_geomark_hint_find (HyScanGtkLayer *layer,
                                  gdouble         x,
                                  gdouble         y,
                                  gdouble        *distance)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  const HyScanGtkMapGeomarkLocation *location;
  gchar *hint = NULL;

  g_rw_lock_reader_lock (&priv->mark_lock);

  location = hyscan_gtk_map_geomark_find_hover (gm_layer, x, y, distance);
  g_clear_pointer (&priv->hover_id, g_free);
  if (location != NULL)
    {
      priv->hover_id = g_strdup (location->mark_id);
      hint = g_strdup (location->mark->name);
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);

  return hint;
}

static gboolean
hyscan_gtk_map_geomark_load_key_file (HyScanGtkLayer *gtk_layer,
                                      GKeyFile       *key_file,
                                      const gchar    *group)
{
  HyScanGtkMapGeomark *gm_layer = HYSCAN_GTK_MAP_GEOMARK (gtk_layer);
  HyScanGtkMapGeomarkPrivate *priv = gm_layer->priv;
  gdouble width;

  width = g_key_file_get_double (key_file, group, "line-width", NULL);
  priv->line_width = width > 0 ? width : DEFAULT_LINE_WIDTH;

  hyscan_gtk_layer_load_key_file_rgba (&priv->color,          key_file, group, "color",          DEFAULT_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_hover,    key_file, group, "hover-color",    DEFAULT_HOVER_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_pending,  key_file, group, "pending-color",  DEFAULT_PENDING_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_blackout, key_file, group, "blackout-color", DEFAULT_BLACKOUT_COLOR);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_geomark_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_geomark_added;
  iface->removed = hyscan_gtk_map_geomark_removed;
  iface->grab_input = hyscan_gtk_map_geomark_grab_input;
  iface->get_visible = hyscan_gtk_map_geomark_get_visible;
  iface->set_visible = hyscan_gtk_map_geomark_set_visible;
  iface->hint_find = hyscan_gtk_map_geomark_hint_find;
  iface->hint_shown = hyscan_gtk_map_geomark_hint_shown;
  iface->load_key_file = hyscan_gtk_map_geomark_load_key_file;
}

/**
 * hyscan_gtk_map_geomark_new:
 * @model: указатель на #HyScanMarkModel
 *
 * Returns: новый объект #HyScanGtkMapGeomark
 */
HyScanGtkLayer *
hyscan_gtk_map_geomark_new (HyScanMarkModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GEOMARK,
                       "model", model, NULL);
}

/**
 * hyscan_gtk_map_geomark_mark_highlight:
 * @wfm_layer
 * @mark_id
 *
 * Выделяет метку на карте
 */
void
hyscan_gtk_map_geomark_mark_highlight (HyScanGtkMapGeomark *wfm_layer,
                                       const gchar         *mark_id)
{
  HyScanGtkMapGeomarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_GEOMARK (wfm_layer));
  priv = wfm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_writer_lock (&priv->mark_lock);
  g_free (priv->active_mark_id);
  priv->active_mark_id = g_strdup (mark_id);
  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_wfmark_mark_view:
 * @gm_layer: указатель на #HyScanGtkMapGeomark
 * @mark_id: идентификатор метки
 *
 * Перемещает область видимости карты в положение метки @mark_id и выделяет
 * эту метку.
 */
void
hyscan_gtk_map_geomark_mark_view (HyScanGtkMapGeomark *gm_layer,
                                  const gchar         *mark_id,
                                  gboolean             zoom_in)
{
  HyScanGtkMapGeomarkPrivate *priv;
  HyScanGtkMapGeomarkLocation *location;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_GEOMARK (gm_layer));
  priv = gm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  location = g_hash_table_lookup (priv->marks, mark_id);
  if (location != NULL)
    {
      gdouble x_margin, y_margin;
      gdouble prev_scale, cur_scale;

      x_margin = 0.1 * location->width;
      y_margin = 0.1 * location->height;
      gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &prev_scale, NULL);
      gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map),
                               location->corner[0].x - x_margin, location->corner[2].x + x_margin,
                               location->corner[0].y - y_margin, location->corner[2].y + y_margin);
      gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &cur_scale, NULL);

      /* Возвращаем прежний масштаб, если метка вместилась в видимую область. */
      if (cur_scale < prev_scale && !zoom_in)
        {
          gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), prev_scale, prev_scale,
                                    location->c2d.x, location->c2d.y);
        }
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
}
