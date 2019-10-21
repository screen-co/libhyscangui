#include "hyscan-gtk-planner-status.h"
#include <glib/gi18n-lib.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_GTK_PLANNER
};

struct _HyScanGtkPlannerStatusPrivate
{
  HyScanPlannerModel   *model;
  HyScanGtkMapPlanner  *gtk_planner;

  GtkWidget            *length;
  GtkWidget            *label;
};

static void    hyscan_gtk_planner_status_set_property             (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   const GValue             *value,
                                                                   GParamSpec               *pspec);
static void    hyscan_gtk_planner_status_object_constructed       (GObject                  *object);
static void    hyscan_gtk_planner_status_object_finalize          (GObject                  *object);
static void    hyscan_gtk_planner_status_track_changed            (HyScanGtkPlannerStatus   *p_status,
                                                                   const HyScanPlannerTrack *track);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerStatus, hyscan_gtk_planner_status, GTK_TYPE_LABEL)

static void
hyscan_gtk_planner_status_class_init (HyScanGtkPlannerStatusClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_status_set_property;
  object_class->constructed = hyscan_gtk_planner_status_object_constructed;
  object_class->finalize = hyscan_gtk_planner_status_object_finalize;

  g_object_class_install_property (object_class, PROP_GTK_PLANNER,
    g_param_spec_object ("gtk-planner", "HyScanGtkMapPlanner", "Map planner layer", HYSCAN_TYPE_GTK_MAP_PLANNER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_status_init (HyScanGtkPlannerStatus *gtk_planner_status)
{
  gtk_planner_status->priv = hyscan_gtk_planner_status_get_instance_private (gtk_planner_status);
}

static void
hyscan_gtk_planner_status_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerStatus *gtk_planner_status = HYSCAN_GTK_PLANNER_STATUS (object);
  HyScanGtkPlannerStatusPrivate *priv = gtk_planner_status->priv;

  switch (prop_id)
    {
    case PROP_GTK_PLANNER:
      priv->gtk_planner = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_status_object_constructed (GObject *object)
{
  HyScanGtkPlannerStatus *p_status = HYSCAN_GTK_PLANNER_STATUS (object);
  HyScanGtkPlannerStatusPrivate *priv = p_status->priv;

  G_OBJECT_CLASS (hyscan_gtk_planner_status_parent_class)->constructed (object);

  priv->model = g_object_ref (hyscan_gtk_map_planner_get_model (priv->gtk_planner));
  g_signal_connect_swapped (priv->gtk_planner, "track-changed",
                            G_CALLBACK (hyscan_gtk_planner_status_track_changed), p_status);

  hyscan_gtk_planner_status_track_changed (p_status, NULL);
}

static void
hyscan_gtk_planner_status_object_finalize (GObject *object)
{
  HyScanGtkPlannerStatus *gtk_planner_status = HYSCAN_GTK_PLANNER_STATUS (object);
  HyScanGtkPlannerStatusPrivate *priv = gtk_planner_status->priv;

  g_clear_object (&priv->model);
  g_clear_object (&priv->gtk_planner);

  G_OBJECT_CLASS (hyscan_gtk_planner_status_parent_class)->finalize (object);
}

static void
hyscan_gtk_planner_status_track_changed (HyScanGtkPlannerStatus   *p_status,
                                         const HyScanPlannerTrack *track)
{
  HyScanGtkPlannerStatusPrivate *priv = p_status->priv;
  GtkLabel *label = GTK_LABEL (p_status);
  HyScanGeo *geo;
  HyScanGeoCartesian2D start, end;

  if (track == NULL)
    {
      gtk_label_set_text (label,  _("No active track"));

      return;
    }

  geo = hyscan_planner_model_get_geo (HYSCAN_PLANNER_MODEL (priv->model));
  if (geo == NULL)
    {
      gtk_label_set_text (label, _("Coordinate system is not available"));

      return;
    }

  if (!hyscan_geo_geo2topoXY (geo, &start, track->start) || !hyscan_geo_geo2topoXY (geo, &end, track->end))
    {
      gtk_label_set_text (label, _("Wrong value"));
    }
  else
    {
      gdouble angle, length;
      gchar *text;

      length = hypot (end.x - start.x, end.y - start.y);
      angle = atan2 (end.y - start.y, end.x - start.x);
      angle = angle / G_PI * 180.0;
      if (angle < 0)
        angle += 360.0;

      text = g_strdup_printf (_("Angle %.2f°, Length %.2f %s"), angle, length, _("m"));
      gtk_label_set_text (label, text);

      g_free (text);
    }

  g_clear_object (&geo);
}

GtkWidget *
hyscan_gtk_planner_status_new (HyScanGtkMapPlanner *planner)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_STATUS,
                       "gtk-planner", planner,
                       NULL);
}
