/* hyscan-gtk-map-planner.c
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
 * SECTION: hyscan-gtk-map-planner
 * @Short_description: Слой для планирования схемы галсов.
 * @Title: HyScanGtkMapPlanner
 * @See_also: #HyScanGtkLayer, #HyScanGtkMape
 *
 * Данный слой позволяет отображать и редактировать схему плановых галсов. Модель
 * плановых галсов передаётся при создании слоя функцией:
 * - hyscan_gtk_map_planner_new()
 *
 */

#include "hyscan-gtk-map-planner.h"
#include "hyscan-gtk-map.h"
#include <hyscan-planner.h>
#include <math.h>

#define HANDLE_TYPE_NAME     "map_planner"
#define HANDLE_RADIUS         3.0
#define HANDLE_HOVER_RADIUS   5.0

enum
{
  PROP_O,
  PROP_MODEL
};

typedef enum
{
  MODE_NONE,              /* Режим просмотра. */
  MODE_DRAG_START,        /* Режим редактирования начала галса. */
  MODE_DRAG_END,          /* Режим редактирования конца галса. */
} HyScanGtkMapPlannerMode;

typedef struct
{
  gchar                *id;       /* Идентификатор галса. */
  HyScanPlannerTrack   *origin;   /* Структура с исходными данными галса. */
  HyScanGeoCartesian2D  start;    /* Проекция начала галса на карту. */
  HyScanGeoCartesian2D  end;      /* Проекция конца галса на карту. */
} HyScanGtkMapPlannerTrack;

typedef struct
{
  HyScanPlannerZone    *origin;     /* Структура с исходными данными зоны полигона. */
  HyScanGeoCartesian2D *points;     /* Проекция вершин зоны на карту. */
  gsize                 points_len; /* Длина массива points. */
  GList                *tracks;     /* Список галсов HyScanGtkMapPlannerTrack, лежащих внутри этой зоны. */
} HyScanGtkMapPlannerZone;

struct _HyScanGtkMapPlannerPrivate
{
  HyScanGtkMap                      *map;            /* Виджет карты. */
  HyScanObjectModel                 *model;          /* Модель объектов планировщика. */
  gboolean                           visible;        /* Признак видимости слоя. */
  GHashTable                        *zones;          /* Таблица зон полигона. */
  GList                             *tracks;         /* Список галсов, которые не связаны ни с одной из зон. */

  HyScanGtkMapPlannerMode            cur_mode;       /* Текущий режим редактирования галса. */
  HyScanGtkMapPlannerTrack          *cur_track;      /* Копия галса, который редактируется в данный момент. */

  gchar                             *hover_track_id; /* Указатель на подсвечиваемый галс. */
  HyScanGtkMapPlannerMode            hover_mode;     /* Режим активации галса hover_track. */

  const HyScanGtkMapPlannerTrack    *found_track;    /* Указатель на галс под курсором мыши. */
  HyScanGtkMapPlannerMode            found_mode;     /* Режим активации галса found_track. */
};

static void                       hyscan_gtk_map_planner_interface_init        (HyScanGtkLayerInterface  *iface);
static void                       hyscan_gtk_map_planner_set_property          (GObject                  *object,
                                                                                guint                     prop_id,
                                                                                const GValue             *value,
                                                                                GParamSpec               *pspec);
static void                       hyscan_gtk_map_planner_object_constructed    (GObject                  *object);
static void                       hyscan_gtk_map_planner_object_finalize       (GObject                  *object);
static void                       hyscan_gtk_map_planner_model_changed         (HyScanGtkMapPlanner      *planner);
static void                       hyscan_gtk_map_planner_zone_free             (HyScanGtkMapPlannerZone  *zone);
static void                       hyscan_gtk_map_planner_track_free            (HyScanGtkMapPlannerTrack *track);
static HyScanGtkMapPlannerZone *  hyscan_gtk_map_planner_zone_create           (HyScanGtkMapPlanner      *planner,
                                                                                HyScanPlannerZone        *zone);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_create          (void);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_copy            (const HyScanGtkMapPlannerTrack *track);
static void                       hyscan_gtk_map_planner_track_project         (HyScanGtkMapPlanner      *planner,
                                                                                HyScanGtkMapPlannerTrack *track);
static void                       hyscan_gtk_map_planner_zone_project          (HyScanGtkMapPlanner      *planner,
                                                                                HyScanGtkMapPlannerZone  *zone);
static void                       hyscan_gtk_map_planner_draw                  (HyScanGtkMapPlanner      *planner,
                                                                                cairo_t                  *cairo);
static void                       hyscan_gtk_map_planner_zone_draw             (HyScanGtkMapPlanner      *planner,
                                                                                HyScanGtkMapPlannerZone  *zone,
                                                                                cairo_t                  *cairo);
static void                       hyscan_gtk_map_planner_track_draw            (HyScanGtkMapPlanner      *planner,
                                                                                HyScanGtkMapPlannerTrack *track,
                                                                                gboolean                  skip_current,
                                                                                cairo_t                  *cairo);
static gboolean                   hyscan_gtk_map_planner_motion_notify         (HyScanGtkMapPlanner      *planner,
                                                                                GdkEventMotion           *event);
static gboolean                   hyscan_gtk_map_planner_key_press             (HyScanGtkMapPlanner      *planner,
                                                                                GdkEventKey              *event);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPlanner, hyscan_gtk_map_planner, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapPlanner)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_planner_interface_init))

static void
hyscan_gtk_map_planner_class_init (HyScanGtkMapPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_planner_set_property;

  object_class->constructed = hyscan_gtk_map_planner_object_constructed;
  object_class->finalize = hyscan_gtk_map_planner_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Planner model", HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_planner_init (HyScanGtkMapPlanner *gtk_map_planner)
{
  gtk_map_planner->priv = hyscan_gtk_map_planner_get_instance_private (gtk_map_planner);
}

static void
hyscan_gtk_map_planner_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

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
hyscan_gtk_map_planner_object_constructed (GObject *object)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->constructed (object);

  priv->zones = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_planner_zone_free);

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_map_planner_model_changed), planner);
}

static void
hyscan_gtk_map_planner_object_finalize (GObject *object)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  g_clear_object (&priv->model);
  g_free (priv->hover_track_id);
  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_hash_table_destroy (priv->zones);

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->finalize (object);
}

/* Обработчик сигнала "changed" модели.
 * Копирует информацию о схеме плановых галсов во внутренние структуры слоя. */
static void
hyscan_gtk_map_planner_model_changed (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  GHashTable *objects;
  gchar *key;
  HyScanPlannerObject *object;
  gchar *found_id;   /* Идентификатор найденного и подсвеченных галсов. */

  /* Получаем список всех объектов планировщика. */
  objects = hyscan_object_model_get (priv->model);

  /* Загружаем зоны. */
  g_hash_table_remove_all (priv->zones);
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanGtkMapPlannerZone *zone;

      if (object->type != HYSCAN_PLANNER_ZONE)
        continue;

      /* Воруем ключ и объект из одной хэш-таблицы и добавляем их в другую. */
      g_hash_table_iter_steal (&iter);
      zone = hyscan_gtk_map_planner_zone_create (planner, &object->zone);
      g_hash_table_insert (priv->zones, key, zone);
    }

  /* Загружаем плановые галсы по каждой зоне. */
  /* Запоминаем ИД интересных галсов. */
  found_id = priv->found_track != NULL ? g_strdup (priv->found_track->id) : NULL;
  priv->found_track = NULL;

  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->tracks = NULL;
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanGtkMapPlannerTrack *track;
      HyScanGtkMapPlannerZone *zone = NULL;
      GList **tracks;

      if (object->type != HYSCAN_PLANNER_TRACK)
        continue;

      /* Находим зону, из которой этот галс. */
      if (object->track.zone_id != NULL)
        zone = g_hash_table_lookup (priv->zones, object->track.zone_id);

      /* Если зона не найдена, то добавляем галс в общий список. */
      if (zone == NULL)
        tracks = &priv->tracks;
      else
        tracks = &zone->tracks;

      /* Воруем ключ и объект из хэш-таблицы - ключ удаляем, а объект помещаем в зону. */
      g_hash_table_iter_steal (&iter);
      track = hyscan_gtk_map_planner_track_create ();
      track->id = key;
      track->origin = &object->track;
      hyscan_gtk_map_planner_track_project (planner, track);
      *tracks = g_list_append (*tracks, track);

      /* Восстанавливаем указатели на интересные галсы. */
      if (g_strcmp0 (found_id, track->id))
        priv->found_track = track;
    }

  g_hash_table_unref (objects);
  g_free (found_id);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_track_free (HyScanGtkMapPlannerTrack *track)
{
  hyscan_planner_track_free (track->origin);
  g_free (track->id);
  g_slice_free (HyScanGtkMapPlannerTrack, track);
}

static void
hyscan_gtk_map_planner_zone_free (HyScanGtkMapPlannerZone *zone)
{
  if (zone == NULL)
    return;

  g_free (zone->points);
  hyscan_planner_zone_free (zone->origin);
  g_list_free_full (zone->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_slice_free (HyScanGtkMapPlannerZone, zone);
}

static HyScanGtkMapPlannerZone *
hyscan_gtk_map_planner_zone_create (HyScanGtkMapPlanner *planner,
                                    HyScanPlannerZone   *zone)
{
  HyScanGtkMapPlannerZone *gtk_zone;

  gtk_zone = g_slice_new (HyScanGtkMapPlannerZone);
  gtk_zone->points = g_new (HyScanGeoCartesian2D, zone->points_len);
  gtk_zone->points_len = zone->points_len;
  gtk_zone->origin = zone;
  gtk_zone->tracks = NULL;

  hyscan_gtk_map_planner_zone_project (planner, gtk_zone);

  return gtk_zone;
}

static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_track_create (void)
{
  return g_slice_new0 (HyScanGtkMapPlannerTrack);
}

static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_track_copy (const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerTrack *copy;

  copy = hyscan_gtk_map_planner_track_create ();
  copy->id = g_strdup (track->id);
  copy->origin = hyscan_planner_track_copy (track->origin);
  copy->start = track->start;
  copy->end = track->end;

  return copy;
}

/* Проецирует плановый галс на карту. */
static void
hyscan_gtk_map_planner_track_project (HyScanGtkMapPlanner       *planner,
                                      HyScanGtkMapPlannerTrack  *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->map == NULL || track == NULL)
    return;

  hyscan_gtk_map_geo_to_value (priv->map, track->origin->start, &track->start);
  hyscan_gtk_map_geo_to_value (priv->map, track->origin->end, &track->end);
}

/* Проецирует зону полигона на карту. */
static void
hyscan_gtk_map_planner_zone_project (HyScanGtkMapPlanner     *planner,
                                     HyScanGtkMapPlannerZone *zone)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gsize i;

  if (priv->map == NULL)
    return;

  for (i = 0; i < zone->points_len; ++i)
    hyscan_gtk_map_geo_to_value (priv->map, zone->origin->points[i], &zone->points[i]);
}

/* Обработчик события "visible-draw".
 * Рисует слой планировщика. */
static void
hyscan_gtk_map_planner_draw (HyScanGtkMapPlanner *planner,
                             cairo_t             *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  gchar *key;
  HyScanGtkMapPlannerZone *zone;
  GList *list;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    hyscan_gtk_map_planner_zone_draw (planner, zone, cairo);

  for (list = priv->tracks; list != NULL; list = list->next)
    hyscan_gtk_map_planner_track_draw (planner, list->data, TRUE, cairo);

  /* Рисуем текущий редактируемый галс. */
  if (priv->cur_track != NULL)
    hyscan_gtk_map_planner_track_draw (planner, priv->cur_track, FALSE, cairo);
}

/* Обработка события "motion-notify".
 * Перемещает галс в новую точку. */
static gboolean
hyscan_gtk_map_planner_motion_notify (HyScanGtkMapPlanner *planner,
                                      GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track = priv->cur_track;
  HyScanGeoCartesian2D cursor;
  HyScanGeoCartesian2D *point_2d, *start_2d;
  HyScanGeoGeodetic *point_geo;

  if (priv->cur_mode == MODE_DRAG_END)
    {
      start_2d = &track->start;
      point_2d = &track->end;
      point_geo = &track->origin->end;
    }
  else if (priv->cur_mode == MODE_DRAG_START)
    {
      start_2d = &track->end;
      point_2d = &track->start;
      point_geo = &track->origin->start;
    }
  else
    {
      return GDK_EVENT_PROPAGATE;
    }

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  /* Сохраняем длину. */
  if (event->state & GDK_CONTROL_MASK)
    {
      gdouble angle;
      gdouble length;

      angle = atan2 (cursor.y - start_2d->y, cursor.x - start_2d->x);
      length = hypot (point_2d->y - start_2d->y, point_2d->x - start_2d->x);
      point_2d->x = start_2d->x + length * cos (angle);
      point_2d->y = start_2d->y + length * sin (angle);
    }

  /* Сохраняем направление. */
  else if (event->state & GDK_SHIFT_MASK)
    {
      gdouble angle;
      gdouble cursor_angle;
      gdouble d_angle;
      gdouble length;
      gdouble dx, dy, cursor_dx, cursor_dy;

      dy = point_2d->y - start_2d->y;
      dx = point_2d->x - start_2d->x;
      cursor_dy = cursor.y - start_2d->y;
      cursor_dx = cursor.x - start_2d->x;
      angle = atan2 (dy, dx);
      cursor_angle = atan2 (cursor_dy, cursor_dx);
      length = hypot (cursor_dx, cursor_dy);

      /* Проверяем, не сменилось ли направление. */
      d_angle = ABS (cursor_angle - angle);
      if (d_angle > G_PI_2 && d_angle < G_PI + G_PI_2)
        length = -length;

      point_2d->x = start_2d->x + length * cos (angle);
      point_2d->y = start_2d->y + length * sin (angle);
    }

  /* Свободно перемещаем. */
  else
    {
      *point_2d = cursor;
    }

  /* Переносим конец галса в новую точку. */
  hyscan_gtk_map_value_to_geo (priv->map, point_geo, *point_2d);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_planner_track_remove (HyScanGtkMapPlanner *planner,
                                     const gchar         *track_id)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *link;

  /* Удаляем из модели. */
  hyscan_object_model_remove_object (priv->model, track_id);

  /* Удаляем из внутреннего списка. */
  for (link = priv->tracks; link != NULL; link = link->next)
    {
      HyScanGtkMapPlannerTrack *track = link->data;

      if (g_strcmp0 (track->id, priv->cur_track->id) != 0)
        continue;

      priv->tracks = g_list_remove_link (priv->tracks, link);
      return;
    }
}

/* Обработка события "key-press-event". */
static gboolean
hyscan_gtk_map_planner_key_press (HyScanGtkMapPlanner *planner,
                                  GdkEventKey         *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->cur_mode == MODE_NONE || priv->cur_track == NULL)
    return GDK_EVENT_PROPAGATE;

  /* Удаляем активный галс. */
  if (event->keyval == GDK_KEY_Delete)
    {
      hyscan_gtk_map_planner_track_remove (planner, priv->cur_track->id);

      g_clear_pointer (&priv->cur_track, hyscan_gtk_map_planner_track_free);
      priv->cur_mode = MODE_NONE;
      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);

      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  /* Отменяем текущее изменение. */
  else if (event->keyval == GDK_KEY_Escape)
    {
      g_clear_pointer (&priv->cur_track, hyscan_gtk_map_planner_track_free);
      priv->cur_mode = MODE_NONE;
      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);

      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Рисует зону полигона и галсы внутри неё. */
static void
hyscan_gtk_map_planner_zone_draw (HyScanGtkMapPlanner     *planner,
                                  HyScanGtkMapPlannerZone *zone,
                                  cairo_t                 *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *list;
  gsize i;

  cairo_new_path (cairo);

  for (i = 0; i < zone->points_len; ++i)
    {
      gdouble x, y;

      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, zone->points[i].x, zone->points[i].y);
      cairo_line_to (cairo, x, y);
    }

  cairo_close_path (cairo);

  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.0);
  cairo_stroke (cairo);

  for (list = zone->tracks; list != NULL; list = list->next)
    hyscan_gtk_map_planner_track_draw (planner, (HyScanGtkMapPlannerTrack *) list->data, TRUE, cairo);
}

/* Рисует галс. */
static void
hyscan_gtk_map_planner_track_draw (HyScanGtkMapPlanner      *planner,
                                   HyScanGtkMapPlannerTrack *track,
                                   gboolean                  skip_current,
                                   cairo_t                  *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble start_x, start_y, end_x, end_y;
  gdouble start_size, end_size;

  /* Пропускаем активный галс. */
  if (skip_current && priv->cur_track != NULL && g_strcmp0 (priv->cur_track->id, track->id) == 0)
    return;

  /* Определяем размеры хэндлов. */
  start_size = HANDLE_RADIUS;
  end_size = HANDLE_RADIUS;
  if (priv->hover_track_id != NULL && g_strcmp0 (priv->hover_track_id, track->id) == 0)
    {
      if (priv->hover_mode == MODE_DRAG_START)
        start_size = HANDLE_HOVER_RADIUS;
      else if (priv->hover_mode == MODE_DRAG_END)
        end_size = HANDLE_HOVER_RADIUS;
    }

  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &start_x, &start_y, track->start.x, track->start.y);
  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &end_x, &end_y, track->end.x, track->end.y);

  cairo_set_source_rgb (cairo, 0.5, 0.0, 0.0);

  /* Точка в начале галса. */
  cairo_arc (cairo, start_x, start_y, start_size, -G_PI, G_PI);
  cairo_fill (cairo);

  /* Стрелка в конце галса. */
  cairo_save (cairo);
  cairo_translate (cairo, end_x, end_y);
  cairo_rotate (cairo, atan2 (start_y - end_y, start_x - end_x) - G_PI_2);
  cairo_move_to (cairo, 0, 0);
  cairo_line_to (cairo,  end_size, 2 * end_size);
  cairo_line_to (cairo, -end_size, 2 * end_size);
  cairo_fill (cairo);
  cairo_restore (cairo);

  cairo_move_to (cairo, start_x, start_y);
  cairo_line_to (cairo, end_x, end_y);
  cairo_stroke (cairo);
}

static const HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_handle_at (HyScanGtkMapPlanner     *planner,
                                  gdouble                  x,
                                  gdouble                  y,
                                  HyScanGtkMapPlannerMode *mode)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *list;
  gdouble scale_x, scale_y;
  gdouble max_x, max_y;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, &scale_y);
  max_x = HANDLE_HOVER_RADIUS * scale_x;
  max_y = HANDLE_HOVER_RADIUS * scale_y;

  for (list = priv->tracks; list != NULL; list = list->next)
    {
      HyScanGtkMapPlannerTrack *track = list->data;

      if (ABS (track->start.x - x) < max_x && ABS (track->start.y - y) < max_y)
        {
          *mode = MODE_DRAG_START;
          return track;
        }

      else if (ABS (track->end.x - x) < max_x && ABS (track->end.y - y) < max_y)
        {
          *mode = MODE_DRAG_END;
          return track;
        }
    }

  *mode = MODE_NONE;

  return NULL;
}

static gboolean
hyscan_gtk_map_planner_handle_create_add (HyScanGtkMapPlanner *planner,
                                          gdouble              x,
                                          gdouble              y)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack track;
  HyScanGtkMapPlannerTrack *track_proj;

  /* Создаём новый галс только в слое, в БД пока не добавляем. */
  track.type = HYSCAN_PLANNER_TRACK;
  track.number = 0;
  track.name = "Track";
  track.zone_id = NULL;
  track.speed = 1.0;

  track_proj = hyscan_gtk_map_planner_track_create ();
  track_proj->origin = hyscan_planner_track_copy (&track);
  track_proj->start.x = x;
  track_proj->start.y = y;
  track_proj->end = track_proj->start;

  hyscan_gtk_map_value_to_geo (priv->map, &track_proj->origin->start, track_proj->start);
  track_proj->origin->end = track_proj->origin->start;

  /* И начинаем перетаскивание конца галса. */
  priv->cur_mode = MODE_DRAG_END;
  priv->cur_track = track_proj;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), planner);

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_handle_find (HyScanGtkLayer       *layer,
                                    gdouble               x,
                                    gdouble               y,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  const HyScanGeoCartesian2D *point;

  priv->found_track = hyscan_gtk_map_planner_handle_at (planner, x, y, &priv->found_mode);
  if (priv->found_track == NULL)
    return FALSE;

  if (priv->found_mode == MODE_DRAG_START)
    point = &priv->found_track->start;
  else if (priv->found_mode == MODE_DRAG_END)
    point = &priv->found_track->end;
  else
    return FALSE;

  handle->val_x = point->x;
  handle->val_y = point->y;
  handle->type_name = HANDLE_TYPE_NAME;

  return TRUE;
}

static gconstpointer
hyscan_gtk_map_planner_handle_grab (HyScanGtkLayer       *layer,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_return_val_if_fail (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0, NULL);
  if (priv->found_track == NULL)
    return NULL;

  /* В момент захвата хэндла мы предполагаем, что текущий хэндл не выбран. Иначе это ошибка в логике программы. */
  g_assert (priv->cur_track == NULL);

  priv->cur_mode = priv->found_mode;
  priv->cur_track = hyscan_gtk_map_planner_track_copy (priv->found_track);

  return planner;
}

static void
hyscan_gtk_map_planner_handle_show (HyScanGtkLayer       *layer,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  const gchar *hover_id;
  gboolean id_changed;
  HyScanGtkMapPlannerMode mode;

  g_return_if_fail (handle == NULL || g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0);

  if (handle != NULL && priv->found_track != NULL)
    {
      mode = priv->found_mode;
      hover_id = priv->found_track->id;
    }
  else
    {
      mode = MODE_NONE;
      hover_id = NULL;
    }

  id_changed = g_strcmp0 (hover_id, priv->hover_track_id) != 0;
  if (mode != priv->hover_mode || id_changed)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  priv->hover_mode = mode;
  if (id_changed)
    {
      g_clear_pointer (&priv->hover_track_id, g_free);
      priv->hover_track_id = g_strdup (hover_id);
    }
}

static gboolean
hyscan_gtk_map_planner_handle_create (HyScanGtkLayer *layer,
                                      GdkEventButton *event)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble x, y;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &x, &y);

  return hyscan_gtk_map_planner_handle_create_add (planner, x, y);
}

static gboolean
hyscan_gtk_map_planner_handle_release (HyScanGtkLayer *layer,
                                       gconstpointer   howner)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *cur_track;

  /* Проверяем, что хэндл у нас. */
  if (howner != planner)
    return FALSE;

  /* По какой-то причине хэндл у нас, но нет активного галса. Вернём хэндл, но ругнёмся. */
  g_return_val_if_fail (priv->cur_track != NULL, TRUE);
  cur_track = priv->cur_track;

  if (cur_track->id == NULL)
    hyscan_object_model_add_object (priv->model, cur_track->origin);
  else
    hyscan_object_model_modify_object (priv->model, cur_track->id, cur_track->origin);

  priv->cur_mode = MODE_NONE;
  g_clear_pointer (&priv->cur_track, hyscan_gtk_map_planner_track_free);

  return TRUE;
}

static void
hyscan_gtk_map_planner_set_visible (HyScanGtkLayer *layer,
                                    gboolean        visible)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  priv->visible = visible;
  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_planner_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  return priv->visible;
}

static void
hyscan_gtk_map_planner_added (HyScanGtkLayer          *layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);

  g_signal_connect_swapped (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_planner_draw), planner);
  g_signal_connect_swapped (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_planner_motion_notify), planner);
  g_signal_connect_swapped (priv->map, "key-press-event", G_CALLBACK (hyscan_gtk_map_planner_key_press), planner);
}

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->handle_find = hyscan_gtk_map_planner_handle_find;
  iface->handle_grab = hyscan_gtk_map_planner_handle_grab;
  iface->handle_release = hyscan_gtk_map_planner_handle_release;
  iface->handle_create = hyscan_gtk_map_planner_handle_create;
  iface->handle_show = hyscan_gtk_map_planner_handle_show;
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
  iface->added = hyscan_gtk_map_planner_added;
}

/**
 * hyscan_gtk_map_planner_new:
 * @model: модель объектов планировщика
 *
 * Создаёт слой планировщика схемы галсов.
 *
 * Returns: указатель на слой планировщика
 */
HyScanGtkLayer *
hyscan_gtk_map_planner_new (HyScanObjectModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PLANNER,
                       "model", model,
                       NULL);
}
