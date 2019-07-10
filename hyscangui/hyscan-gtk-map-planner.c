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
 * @Short_description: Слой карты с запланированными галсами
 * @Title: HyScanGtkMapPlanner
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Данный слой планировать галсы для будущих миссий
 */

#include "hyscan-gtk-map-planner.h"
#include <hyscan-gtk-map.h>
#include <math.h>

#define HOVER_DISTANCE 8                   /* Расстояние до точки, при котором ее можно ухватить. */
#define NEW_TRACK_ID   "new-magic-id"      /* Ключ в таблице галсов, соответствующий новому галсу. */

#define LINE_COLOR "#C67C67"               /* Цвет линий по умолчанию. */
#define LINE_WIDTH 2.0                     /* Толщина линий по умолчанию. */

enum
{
  PROP_O,
  PROP_PLANNER
};

enum 
{
  MODE_NONE,
  MODE_DRAG
};

typedef enum
{
  POINT_NONE,
  POINT_START,
  POINT_END,
} HyScanGtkMapPlannerPoint;

typedef struct
{
  HyScanPlannerTrack   *orig;
  HyScanGeoCartesian2D  start;
  HyScanGeoCartesian2D  end;
} HyScanGtkMapPlannerTrack;

struct _HyScanGtkMapPlannerPrivate
{
  HyScanGtkMap                      *map;           /* Карта. */
  HyScanPlanner                     *planner;       /* Планировщик. */
  guint                              mode;          /* Текущий режим работы. */

  gboolean                           visible;       /* Признак видимости слоя. */

  GMutex                             lock;          /* Блокировка доступа к следующим переменным. */
  GHashTable                        *tracks;        /* Таблица запланированных галсов. */
  HyScanGtkMapPlannerTrack          *active_track;  /* Активный галс. */
  HyScanGtkMapPlannerPoint           active_point;  /* Активная точка активного галса. */

  /* Стиль оформления. */
  GdkRGBA                            line_color;    /* Цвет линий. */
  gdouble                            line_width;    /* Толщина линий. */
};

static void    hyscan_gtk_map_planner_interface_init           (HyScanGtkLayerInterface  *iface);
static void    hyscan_gtk_map_planner_set_property             (GObject                  *object,
                                                                guint                     prop_id,
                                                                const GValue             *value,
                                                                GParamSpec               *pspec);
static void    hyscan_gtk_map_planner_object_constructed       (GObject                  *object);
static void    hyscan_gtk_map_planner_object_finalize          (GObject                  *object);
static void    hyscan_gtk_map_planner_update                   (HyScanGtkMapPlanner      *planner_layer);
static void    hyscan_gtk_map_planner_track_free               (HyScanGtkMapPlannerTrack *track);

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

  g_object_class_install_property (object_class, PROP_PLANNER,
    g_param_spec_object ("planner", "Planner",
                         "The HyScanPlanner containing mission information", HYSCAN_TYPE_PLANNER,
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
    case PROP_PLANNER:
      priv->planner = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_planner_object_constructed (GObject *object)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);
  priv->line_width = LINE_WIDTH;
  gdk_rgba_parse (&priv->line_color, LINE_COLOR);
  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                        (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_signal_connect_swapped (priv->planner, "changed", G_CALLBACK (hyscan_gtk_map_planner_update), gtk_map_planner);
}

static void
hyscan_gtk_map_planner_object_finalize (GObject *object)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  g_mutex_clear (&priv->lock);
  g_hash_table_unref (priv->tracks);
  g_object_unref (priv->planner);

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->finalize (object);
}

static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_track_new (void)
{
  return g_slice_new (HyScanGtkMapPlannerTrack);
}

static void
hyscan_gtk_map_planner_track_free (HyScanGtkMapPlannerTrack *track)
{
  hyscan_planner_track_free (track->orig);
  g_slice_free (HyScanGtkMapPlannerTrack, track);
}

static gboolean
hyscan_gtk_map_planner_steal_track (gchar              *key,
                                    HyScanPlannerTrack *orig,
                                    GHashTable         *tracks)
{
  HyScanGtkMapPlannerTrack *track;

  track = hyscan_gtk_map_planner_track_new ();
  track->orig = orig;

  g_hash_table_insert (tracks, key, track);

  return TRUE;
}

static void
hyscan_gtk_map_planner_project_track (HyScanGtkMapPlanner      *planner_layer,
                                      HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  hyscan_gtk_map_geo_to_value (priv->map, track->orig->start, &track->start);
  hyscan_gtk_map_geo_to_value (priv->map, track->orig->end, &track->end);
}

/* Функция должна вызываться за g_mutex_lock (&priv->lock); */
static void
hyscan_gtk_map_planner_update_tracks (HyScanGtkMapPlanner *planner_layer)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  HyScanGtkMapPlannerTrack *track;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    hyscan_gtk_map_planner_project_track (planner_layer, track);
}

static void
hyscan_gtk_map_planner_update (HyScanGtkMapPlanner *planner_layer)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  GHashTable *orig_tracks;

  orig_tracks = hyscan_planner_get (priv->planner);

  g_mutex_lock (&priv->lock);

  /* Копируем к себе список всех галсов. */
  g_hash_table_remove_all (priv->tracks);
  g_hash_table_foreach_steal (orig_tracks, (GHRFunc) hyscan_gtk_map_planner_steal_track, priv->tracks);
  hyscan_gtk_map_planner_update_tracks (planner_layer);

  priv->mode = MODE_NONE;
  priv->active_track = NULL;
  priv->active_point = POINT_NONE;

  g_mutex_unlock (&priv->lock);

  g_hash_table_unref (orig_tracks);
}

static void
hyscan_gtk_map_planner_draw (GtkCifroArea *cifro,
                             cairo_t      *cairo,
                             gpointer      user_data)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (user_data);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapPlannerTrack *track;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner_layer)))
    return;

  /* Стили оформления. */
  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);

  /* Рисуем все запланированные галсы. */
  g_mutex_lock (&priv->lock);
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      gdouble x0, y0, x1, y1;
      gboolean active;

      /* Переводим координаты концов галса в СК виджета. */
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x0, &y0,
                                             track->start.x, track->start.y);
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x1, &y1,
                                             track->end.x, track->end.y);

      cairo_new_path (cairo);
      cairo_move_to (cairo, x0, y0);
      cairo_line_to (cairo, x1, y1);
      cairo_stroke (cairo);

      active = (priv->active_track == track && priv->active_point == POINT_START);
      cairo_arc (cairo, x0, y0, active ? 7.0 : 3.0, 0, 2 * G_PI);
      cairo_fill (cairo);

      active = (priv->active_track == track && priv->active_point == POINT_END);
      cairo_arc (cairo, x1, y1, active ? 7.0 : 3.0, 0, 2 * G_PI);
      cairo_fill (cairo);
    }
  g_mutex_unlock (&priv->lock);
}

static void
hyscan_gtk_map_planner_proj_notify (HyScanGtkMapPlanner *planner_layer,
                                    GParamSpec          *pspec)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  g_mutex_lock (&priv->lock);
  hyscan_gtk_map_planner_update_tracks (planner_layer);
  g_mutex_unlock (&priv->lock);
}

/* Находит точку на слое в окрестности указанных координат виджета.
 * Функцию необходимо вызывать за g_mutex_lock (&priv->lock); */
static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_get_point_at (HyScanGtkMapPlanner      *planner_layer,
                                     gdouble                   x,
                                     gdouble                   y,
                                     HyScanGtkMapPlannerPoint *point)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  GHashTableIter iter;
  HyScanGtkMapPlannerTrack *track;

  gdouble x_val, y_val;

  gdouble threshold;
  gdouble scale;
  HyScanGtkMapPlannerPoint active_point;
  HyScanGtkMapPlannerTrack *active_track;

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
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner_layer)))
    return NULL;

  /* Определяем координаты и размеры в логической СК. */
  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), x, y, &x_val, &y_val);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
  threshold = HOVER_DISTANCE * scale;

  /* Ищем точку под курсором. */
  active_track = NULL;
  active_point = POINT_NONE;
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track) && active_track == NULL)
    {
      /* Галсы, которых пока нет в модели, мы не добавляем. */
      if (track->orig->id == NULL)
        continue;

      if (fabs (track->start.x - x_val) + fabs (track->start.y - y_val) < threshold)
        {
          active_point = POINT_START;
          active_track = track;
        }
      else if (fabs (track->end.x - x_val) + fabs (track->end.y - y_val) < threshold)
        {
          active_point = POINT_END;
          active_track = track;
        }
    }

  *point = active_point;

  return active_track;
}

/* Выделяет точку под курсором мыши, если она находится на отрезке. */
static void
hyscan_gtk_map_planner_motion_hover (HyScanGtkMapPlanner *planner_layer,
                                     GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  HyScanGtkMapPlannerPoint active_point;
  HyScanGtkMapPlannerTrack *active_track;

  g_mutex_lock (&priv->lock);

  /* Ищем точку под курсором. */
  active_track = hyscan_gtk_map_planner_get_point_at (planner_layer, event->x, event->y, &active_point);

  if (active_track != NULL || active_track != priv->active_track)
    {
      priv->active_track = active_track;
      priv->active_point = active_point;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }

  g_mutex_unlock (&priv->lock);
}

/* Передвигает точку point под курсор. */
static void
hyscan_gtk_map_planner_motion_drag (HyScanGtkMapPlanner *planner_layer,
                                    GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGeoCartesian2D *point;

  switch (priv->active_point)
    {
    case POINT_START:
      point = &priv->active_track->start;
      break;

    case POINT_END:
      point = &priv->active_track->end;
      break;

    default:
      g_return_if_reached ();
    }

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->x, &point->y);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обрабатывает сигнал "motion-notify-event". */
static gboolean
hyscan_gtk_map_planner_motion_notify (HyScanGtkMapPlanner *planner_layer,
                                      GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_planner_motion_drag (planner_layer, event);
  else
    hyscan_gtk_map_planner_motion_hover (planner_layer, event);

  return FALSE;
}

/* Инициирует перетаскивание точки.
 * Функцию необходимо вызывать за g_mutex_lock (&priv->lock); */
static gconstpointer
hyscan_gtk_map_planner_start_drag (HyScanGtkMapPlanner      *layer,
                                   HyScanGtkMapPlannerTrack *track,
                                   HyScanGtkMapPlannerPoint  point)
{
  HyScanGtkMapPlannerPrivate *priv = layer->priv;
  gconstpointer howner;

  howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));

  /* Проверяем, что хэндл свободен и мы ещё не в режиме перемещения. */
  if (howner != NULL)
    {
      g_warning ("HyScanGtkMapPlanner: failed to start drag - handle is grabbed");
      return NULL;
    }
  if (priv->mode == MODE_DRAG)
    {
      g_warning ("HyScanGtkMapPlanner: failed to start drag - already in drag mode");
      return NULL;
    }

  priv->mode = MODE_DRAG;
  priv->active_point = point;
  priv->active_track = track;

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return layer;
}

/* Обработчик сигнала "handle-grab". Захватывает хэндл под указателем мыши. */
static gconstpointer
hyscan_gtk_map_planner_handle_grab (HyScanGtkLayerContainer *container,
                                    GdkEventMotion          *event,
                                    HyScanGtkMapPlanner     *layer)
{
  HyScanGtkMapPlannerPrivate *priv = layer->priv;
  HyScanGtkMapPlannerTrack *track;
  HyScanGtkMapPlannerPoint point;
  gconstpointer result = NULL;

  g_mutex_lock (&priv->lock);

  track = hyscan_gtk_map_planner_get_point_at (layer, event->x, event->y, &point);
  if (track != NULL)
    result = hyscan_gtk_map_planner_start_drag (layer, track, point);

  g_mutex_unlock (&priv->lock);

  return result;
}

/* Обработчик сигнала "handle-release".
 * Возвращает %TRUE, если мы разрешаем отпустить хэндл. */
static gboolean
hyscan_gtk_map_planner_handle_release (HyScanGtkLayerContainer *container,
                                       GdkEventMotion          *event,
                                       gconstpointer            howner,
                                       HyScanGtkMapPlanner     *layer)
{
  HyScanGtkMapPlannerPrivate *priv = layer->priv;
  HyScanPlannerTrack *track_copy;

  if (howner != layer)
    return FALSE;

  if (priv->mode != MODE_DRAG)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Определяем новые параметры плана галса. */
  track_copy = hyscan_planner_track_copy (priv->active_track->orig);
  if (priv->active_point == POINT_START)
    hyscan_gtk_map_value_to_geo (priv->map, &track_copy->start, priv->active_track->start);
  else if (priv->active_point == POINT_END)
    hyscan_gtk_map_value_to_geo (priv->map, &track_copy->end, priv->active_track->end);
  else
    g_warn_if_reached ();

  /* Устанавливаем состояние в MODE_NONE. */
  priv->mode = MODE_NONE;
  priv->active_track = NULL;
  priv->active_point = POINT_NONE;

  g_mutex_unlock (&priv->lock);

  /* Обновляем модель данных. */
  hyscan_planner_update (priv->planner, track_copy);
  hyscan_planner_track_free (track_copy);

  return TRUE;
}

/* Обработка "button-release-event" на слое.
 * Создаёт новую точку в том месте, где пользователь кликнул мышью. */
static gboolean
hyscan_gtk_map_planner_button_release (GtkWidget      *widget,
                                       GdkEventButton *event,
                                       HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  HyScanGtkLayerContainer *container;
  HyScanGtkMapPlannerTrack *track_proj;
  HyScanPlannerTrack track;
  HyScanGeoCartesian2D point;

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

  /* Создаем новый галс. */
  track.id = NULL;
  track.name = NULL;
  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.x, &point.y);
  hyscan_gtk_map_value_to_geo (priv->map, &track.start, point);
  track.end = track.start;

  /* Обертываем галс в HyScanGtkMapPlannerTrack и проецируем на карту. */
  track_proj = hyscan_gtk_map_planner_track_new ();
  track_proj->orig = hyscan_planner_track_copy (&track);
  hyscan_gtk_map_planner_project_track (planner_layer, track_proj);

  g_mutex_lock (&priv->lock);

  /* Добавляем галс в таблицу. */
  g_hash_table_insert (priv->tracks, NEW_TRACK_ID, track_proj);

  /* Начинаем его перетаскивать. */
  priv->active_point = POINT_END;
  priv->mode = MODE_DRAG;
  priv->active_track = track_proj;

  g_mutex_unlock (&priv->lock);

  hyscan_gtk_layer_container_set_handle_grabbed (container, layer);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

static gboolean
hyscan_gtk_map_planner_key_press (HyScanGtkMapPlanner *planner_layer,
                                  GdkEventKey         *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  gchar *id;

  if (event->keyval != GDK_KEY_Delete || priv->mode != MODE_DRAG)
    return GDK_EVENT_PROPAGATE;

  g_mutex_lock (&priv->lock);

  /* Получаем id текущего галса. */
  id = g_strdup (priv->active_track->orig->id);

  /* Завершаем перетаскивание и возвращаемся в обычный режим. */
  priv->mode = MODE_NONE;
  priv->active_point = POINT_NONE;
  priv->active_track = NULL;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);

  g_mutex_unlock (&priv->lock);

  /* Галс с id есть в модели данных. Надо его оттуда удалить. */
  if (id != NULL)
    {
      hyscan_planner_delete (priv->planner, id);
    }
  /* Галс без id существует только в слое. Проще всего его удалить, загрузив актуальные метки. */
  else
    {
      hyscan_gtk_map_planner_update (planner_layer);
    }

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
  g_free (id);

  return GDK_EVENT_STOP;
}

static void
hyscan_gtk_map_planner_added (HyScanGtkLayer          *layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  
  /* Сигналы контейнера. */
  g_signal_connect (container, "handle-grab", G_CALLBACK (hyscan_gtk_map_planner_handle_grab), planner_layer);
  g_signal_connect (container, "handle-release", G_CALLBACK (hyscan_gtk_map_planner_handle_release), planner_layer);

  g_signal_connect_after (container, "button-release-event",
                          G_CALLBACK (hyscan_gtk_map_planner_button_release),planner_layer);
  g_signal_connect_swapped (container, "key-press-event",
                            G_CALLBACK (hyscan_gtk_map_planner_key_press), planner_layer);

  g_signal_connect_after (priv->map, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_planner_draw), planner_layer);
  g_signal_connect_swapped (priv->map, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_planner_motion_notify), planner_layer);
  g_signal_connect_swapped (priv->map, "notify::projection",
                            G_CALLBACK (hyscan_gtk_map_planner_proj_notify), planner_layer);

  /* Загружаем данные из модели. */
  hyscan_gtk_map_planner_update (planner_layer);
}

static void
hyscan_gtk_map_planner_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  /* Отключаемся от карты. */
  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
}

static void
hyscan_gtk_map_planner_set_visible (HyScanGtkLayer *layer,
                                    gboolean        visible)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  priv->visible = visible;
  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_planner_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  return priv->visible;
}

/* Захватывает пользовательский ввод в контейнере.
 * Реализация HyScanGtkLayerInterface.grab_input(). */
static gboolean
hyscan_gtk_map_planner_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  if (priv->map == NULL)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_load_key_file (HyScanGtkLayer *gtk_layer,
                                      GKeyFile       *key_file,
                                      const gchar    *group)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (gtk_layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  gdouble width;

  width = g_key_file_get_double (key_file, group, "line-width", NULL);
  priv->line_width = width > 0 ? width : LINE_WIDTH;

  hyscan_gtk_layer_load_key_file_rgba (&priv->line_color, key_file, group, "line-color", LINE_COLOR);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_planner_added;
  iface->removed = hyscan_gtk_map_planner_removed;
  iface->grab_input = hyscan_gtk_map_planner_grab_input;
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
  iface->load_key_file = hyscan_gtk_map_planner_load_key_file;
}


HyScanGtkLayer *
hyscan_gtk_map_planner_new (HyScanPlanner *planner)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PLANNER,
                       "planner", planner, NULL);
}
