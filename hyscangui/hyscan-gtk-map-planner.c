#include "hyscan-gtk-map-planner.h"
#include "hyscan-gtk-map.h"
#include <hyscan-planner.h>

enum
{
  PROP_O,
  PROP_MODEL
};

typedef struct
{
  HyScanPlannerTrack   *origin;
  HyScanGeoCartesian2D  start;
  HyScanGeoCartesian2D  end;
} HyScanGtkMapPlannerTrack;

typedef struct
{
  HyScanPlannerZone    *origin;
  HyScanGeoCartesian2D *points;
  gsize                 points_len;
  GList                *tracks;
} HyScanGtkMapPlannerZone;

struct _HyScanGtkMapPlannerPrivate
{
  HyScanGtkMap                *map;
  HyScanObjectModel           *model;
  gboolean                     visible;
  GHashTable                  *zones;
  GList                       *tracks;
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
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_create          (HyScanGtkMapPlanner      *planner,
                                                                                HyScanPlannerTrack       *track);
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
                                                                                cairo_t                  *cairo);
static gboolean                   hyscan_gtk_map_planner_handle_create         (HyScanGtkMapPlanner      *planner,
                                                                                GdkEventButton           *event);

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

  g_hash_table_destroy (priv->zones);

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_planner_model_changed (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  GHashTable *objects;
  gchar *key;
  HyScanPlannerObject *object;

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
      g_free (key);
      track = hyscan_gtk_map_planner_track_create (planner, &object->track);
      *tracks = g_list_append (*tracks, track);
    }

  g_hash_table_unref (objects);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_track_free (HyScanGtkMapPlannerTrack *track)
{
  hyscan_planner_track_free (track->origin);
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
hyscan_gtk_map_planner_track_create (HyScanGtkMapPlanner *planner,
                                     HyScanPlannerTrack  *track)
{
  HyScanGtkMapPlannerTrack *gtk_track;

  gtk_track = g_slice_new (HyScanGtkMapPlannerTrack);
  gtk_track->origin = track;

  hyscan_gtk_map_planner_track_project (planner, gtk_track);

  return gtk_track;
}

static void
hyscan_gtk_map_planner_track_project (HyScanGtkMapPlanner       *planner,
                                      HyScanGtkMapPlannerTrack  *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->map == NULL)
    return;

  hyscan_gtk_map_geo_to_value (priv->map, track->origin->start, &track->start);
  hyscan_gtk_map_geo_to_value (priv->map, track->origin->end, &track->end);
}

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
    hyscan_gtk_map_planner_track_draw (planner, list->data, cairo);
}

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
    hyscan_gtk_map_planner_track_draw (planner, (HyScanGtkMapPlannerTrack *) list->data, cairo);
}

static void
hyscan_gtk_map_planner_track_draw (HyScanGtkMapPlanner      *planner,
                                   HyScanGtkMapPlannerTrack *track,
                                   cairo_t                  *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble start_x, start_y, end_x, end_y;

  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &start_x, &start_y, track->start.x, track->start.y);
  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &end_x, &end_y, track->end.x, track->end.y);

  cairo_move_to (cairo, start_x, start_y);
  cairo_line_to (cairo, end_x, end_y);
  cairo_set_source_rgb (cairo, 0.5, 0.0, 0.0);
  cairo_stroke (cairo);
}

static gboolean
hyscan_gtk_map_planner_handle_create (HyScanGtkMapPlanner *planner,
                                      GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack track;
  HyScanGeoCartesian2D start, end;

  track.type = HYSCAN_PLANNER_TRACK;
  track.number = 0;
  track.name = "Track";
  track.zone_id = NULL;
  track.speed = 1.0;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &start.x, &start.y);
  hyscan_gtk_map_value_to_geo (priv->map, &track.start, start);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x + 20, event->y + 20, &end.x, &end.y);
  hyscan_gtk_map_value_to_geo (priv->map, &track.end, end);

  hyscan_object_model_add_object (priv->model, &track);

  return TRUE;
}

static void
hyscan_gtk_map_planner_set_visible (HyScanGtkLayer *layer,
                                    gboolean        visible)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  priv->visible = visible;
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
  g_signal_connect_swapped (priv->map, "handle-create", G_CALLBACK (hyscan_gtk_map_planner_handle_create), planner);
}

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
  iface->added = hyscan_gtk_map_planner_added;
}

HyScanGtkLayer *
hyscan_gtk_map_planner_new (HyScanObjectModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PLANNER,
                       "model", model,
                       NULL);
}
