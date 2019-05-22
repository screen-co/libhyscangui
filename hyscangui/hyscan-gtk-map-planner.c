#include "hyscan-gtk-map-planner.h"
#include <hyscan-gtk-map.h>
#include <math.h>

#define HOVER_DISTANCE 8  /* Расстояние до точки, при котором ее можно ухватить. */

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
  HyScanGtkMap                      *map;
  HyScanPlanner                     *planner;
  guint                              mode;

  gboolean                           visible;

  GMutex                             lock;
  GHashTable                        *tracks;

  HyScanGtkMapPlannerTrack          *active_track;
  HyScanGtkMapPlannerPoint           active_point;
};

static void    hyscan_gtk_map_planner_interface_init           (HyScanGtkLayerInterface  *iface);
static void    hyscan_gtk_map_planner_set_property             (GObject                  *object,
                                                                guint                     prop_id,
                                                                const GValue             *value,
                                                                GParamSpec               *pspec);
static void    hyscan_gtk_map_planner_object_constructed       (GObject                  *object);
static void    hyscan_gtk_map_planner_object_finalize          (GObject                  *object);
static void    hyscan_gtk_map_planner_changed                  (HyScanGtkMapPlanner      *planner_layer);
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
  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                        (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_signal_connect_swapped (priv->planner, "changed", G_CALLBACK (hyscan_gtk_map_planner_changed), gtk_map_planner);
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

/* Функция должна вызываться за g_mutex_lock (&priv->lock); */
static void
hyscan_gtk_map_planner_update_tracks (HyScanGtkMapPlanner *planner_layer)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  HyScanGtkMapPlannerTrack *track;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      hyscan_gtk_map_geo_to_value (priv->map, track->orig->start, &track->start);
      hyscan_gtk_map_geo_to_value (priv->map, track->orig->end, &track->end);
    }
}

static void
hyscan_gtk_map_planner_changed (HyScanGtkMapPlanner *planner_layer)
{
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;
  GHashTable *orig_tracks;

  orig_tracks = hyscan_planner_get (priv->planner);

  g_mutex_lock (&priv->lock);
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

  /* Стили оформления.. */
  cairo_set_line_width (cairo, 2.0);
  cairo_set_source_rgba (cairo, 1.0, 0.0, 0.0, 0.8);

  /* Рисуем все запланированные галсы. */
  g_mutex_lock (&priv->lock);
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      gdouble x0, y0, x1, y1;
      gboolean active;

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

  /* Если какой-то хэндл захвачен или редактирование запрещено, то не реагируем на точки. */
  howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
  if (howner != NULL)
    return NULL;

  if (!hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (priv->map)))
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
      g_warning ("HyScanGtkMapPinLayer: failed to start drag - handle is grabbed");
      return NULL;
    }
  if (priv->mode == MODE_DRAG)
    {
      g_warning ("HyScanGtkMapPinLayer: failed to start drag - already in drag mode");
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
hyscan_gtk_map_pin_layer_handle_release (HyScanGtkLayerContainer *container,
                                         GdkEventMotion          *event,
                                         HyScanGtkMapPlanner    *layer)
{
  HyScanGtkMapPlannerPrivate *priv = layer->priv;
  HyScanPlannerTrack *track_copy;

  if (hyscan_gtk_layer_container_get_handle_grabbed (container) != layer)
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

static void
hyscan_gtk_map_planner_added (HyScanGtkLayer          *layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPlanner *planner_layer = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  
    /* Сигналы контейнера. */
  g_signal_connect (container, "handle-grab", G_CALLBACK (hyscan_gtk_map_planner_handle_grab), planner_layer);
  g_signal_connect (container, "handle-release", G_CALLBACK (hyscan_gtk_map_pin_layer_handle_release), planner_layer);
  
  g_signal_connect_after (priv->map, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_planner_draw), planner_layer);
  g_signal_connect_swapped (priv->map, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_planner_motion_notify), planner_layer);
  g_signal_connect_swapped (priv->map, "notify::projection",
                            G_CALLBACK (hyscan_gtk_map_planner_proj_notify), planner_layer);
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

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_planner_added;
  iface->removed = hyscan_gtk_map_planner_removed;
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
}


HyScanGtkLayer *
hyscan_gtk_map_planner_new (HyScanPlanner *planner)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PLANNER,
                       "planner", planner, NULL);
}