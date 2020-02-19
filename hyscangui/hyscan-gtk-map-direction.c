#include <glib/gi18n-lib.h>
#include "hyscan-gtk-map-direction.h"

enum
{
  PROP_O,
  PROP_MAP,
  PROP_SELECTION,
  PROP_NAV_MODEL,
};

typedef enum
{
  MODE_MANUAL,
  MODE_SHIP,
  MODE_TRACK,
} HyScanGtkMapDirectionMode;

struct _HyScanGtkMapDirectionPrivate
{
  HyScanGtkMap                *map;
  HyScanPlannerSelection      *selection;
  HyScanObjectModel           *obj_model;
  HyScanNavModel              *nav_model;

  HyScanGtkMapDirectionMode    mode;
  gboolean                     follow_ship;

  gboolean                    nav_data_set;
  HyScanNavModelData          nav_data;
  gdouble                     track_angle;

  GtkWidget                   *btn_follow;
  GtkWidget                   *btn_manual;
  GtkWidget                   *btn_track;
  GtkWidget                   *btn_ship;
};

static void    hyscan_gtk_map_direction_set_property         (GObject               *object,
                                                              guint                  prop_id,
                                                              const GValue          *value,
                                                              GParamSpec            *pspec);
static void    hyscan_gtk_map_direction_object_constructed   (GObject               *object);
static void    hyscan_gtk_map_direction_object_finalize      (GObject               *object);
static void    hyscan_gtk_map_direction_mode_toggled         (GtkToggleButton       *button,
                                                              HyScanGtkMapDirection *direction);
static void    hyscan_gtk_map_direction_follow_toggled       (GtkToggleButton       *button,
                                                              HyScanGtkMapDirection *direction);
static void    hyscan_gtk_map_direction_update_angle         (HyScanGtkMapDirection *direction);
static void    hyscan_gtk_map_direction_nav_chgd             (HyScanGtkMapDirection *direction,
                                                              HyScanNavModelData    *nav_data);
static void    hyscan_gtk_map_direction_trk_chgd             (HyScanGtkMapDirection *direction);
static void    hyscan_gtk_map_direction_update_pos           (HyScanGtkMapDirection *direction);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapDirection, hyscan_gtk_map_direction, GTK_TYPE_BUTTON_BOX)

static void
hyscan_gtk_map_direction_class_init (HyScanGtkMapDirectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_direction_set_property;

  object_class->constructed = hyscan_gtk_map_direction_object_constructed;
  object_class->finalize = hyscan_gtk_map_direction_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "Map", "Map", HYSCAN_TYPE_GTK_MAP,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "Planner selection", "Planner selection", HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_NAV_MODEL,
    g_param_spec_object ("nav-model", "Navigation model", "Navigation model", HYSCAN_TYPE_NAV_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_direction_init (HyScanGtkMapDirection *gtk_map_direction)
{
  gtk_map_direction->priv = hyscan_gtk_map_direction_get_instance_private (gtk_map_direction);
}

static void
hyscan_gtk_map_direction_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkMapDirection *gtk_map_direction = HYSCAN_GTK_MAP_DIRECTION (object);
  HyScanGtkMapDirectionPrivate *priv = gtk_map_direction->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    case PROP_NAV_MODEL:
      priv->nav_model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_direction_object_constructed (GObject *object)
{
  HyScanGtkMapDirection *gtk_map_direction = HYSCAN_GTK_MAP_DIRECTION (object);
  HyScanGtkMapDirectionPrivate *priv = gtk_map_direction->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_direction_parent_class)->constructed (object);

  priv->btn_manual = gtk_radio_button_new_with_mnemonic_from_widget (NULL, _("_User"));
  priv->btn_track  = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (priv->btn_manual), _("_Track"));
  priv->btn_ship   = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (priv->btn_manual), _("_Ship"));
  priv->btn_follow = gtk_check_button_new_with_mnemonic(_("_Follow"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_manual), priv->mode == MODE_MANUAL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_track), priv->mode == MODE_TRACK);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_ship), priv->mode == MODE_SHIP);

  gtk_container_add (GTK_CONTAINER (object), priv->btn_manual);
  gtk_container_add (GTK_CONTAINER (object), priv->btn_track);
  gtk_container_add (GTK_CONTAINER (object), priv->btn_ship);
  gtk_container_add (GTK_CONTAINER (object), priv->btn_follow);

  g_signal_connect (priv->btn_manual, "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_track,  "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_ship,   "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_follow, "toggled", G_CALLBACK (hyscan_gtk_map_direction_follow_toggled), object);

  priv->obj_model = HYSCAN_OBJECT_MODEL (hyscan_planner_selection_get_model (priv->selection));

  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_direction_nav_chgd), object);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_direction_trk_chgd), object);
  g_signal_connect_swapped (priv->obj_model, "changed", G_CALLBACK (hyscan_gtk_map_direction_trk_chgd), object);
}

static void
hyscan_gtk_map_direction_object_finalize (GObject *object)
{
  HyScanGtkMapDirection *gtk_map_direction = HYSCAN_GTK_MAP_DIRECTION (object);
  HyScanGtkMapDirectionPrivate *priv = gtk_map_direction->priv;

  g_object_unref (priv->selection);
  g_object_unref (priv->obj_model);
  g_object_unref (priv->map);
  g_object_unref (priv->nav_model);

  G_OBJECT_CLASS (hyscan_gtk_map_direction_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_direction_nav_chgd (HyScanGtkMapDirection *direction,
                                   HyScanNavModelData    *nav_data)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;

  priv->nav_data_set = TRUE;
  priv->nav_data = *nav_data;
  if (priv->mode == MODE_SHIP)
    hyscan_gtk_map_direction_update_angle (direction);

  if (priv->follow_ship)
    hyscan_gtk_map_direction_update_pos (direction);
}

static void
hyscan_gtk_map_direction_trk_chgd (HyScanGtkMapDirection *direction)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;
  gchar *active_id;
  HyScanPlannerTrack *track;

  active_id = hyscan_planner_selection_get_active_track (priv->selection);
  if (active_id == NULL)
    return;

  track = (HyScanPlannerTrack *) hyscan_object_model_get_id (priv->obj_model, active_id);
  g_free (active_id);
  if (track == NULL || track->type != HYSCAN_PLANNER_TRACK)
    return;

  priv->track_angle = hyscan_planner_track_angle (track);
  if (priv->mode == MODE_TRACK)
    hyscan_gtk_map_direction_update_angle (direction);

  hyscan_planner_track_free (track);
}

static void
hyscan_gtk_map_direction_update_pos (HyScanGtkMapDirection *direction)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;
  HyScanGeoCartesian2D center;

  if (!priv->follow_ship || !priv->nav_data_set)
    return;

  hyscan_gtk_map_geo_to_value (priv->map, priv->nav_data.coord, &center);
  gtk_cifro_area_set_view_center (GTK_CIFRO_AREA (priv->map), center.x, center.y);
}


static void
hyscan_gtk_map_direction_update_angle (HyScanGtkMapDirection *direction)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;
  gdouble angle;

  switch (priv->mode)
  {
    case MODE_TRACK:
      angle = priv->track_angle;
      break;

    case MODE_SHIP:
      angle = priv->nav_data.heading;
      break;

    case MODE_MANUAL:
    default:
      angle = 0.0;
  }

  gtk_cifro_area_set_angle (GTK_CIFRO_AREA (priv->map), -angle);
}

/* Обрабатывает нажатие кнопки переключения режима. */
static void
hyscan_gtk_map_direction_mode_toggled (GtkToggleButton       *button,
                                       HyScanGtkMapDirection *direction)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;

  if (!gtk_toggle_button_get_active (button))
    return;

  if (button == GTK_TOGGLE_BUTTON (priv->btn_manual))
    priv->mode = MODE_MANUAL;
  else if (button == GTK_TOGGLE_BUTTON (priv->btn_ship))
    priv->mode = MODE_SHIP;
  else if (button == GTK_TOGGLE_BUTTON (priv->btn_track))
    priv->mode = MODE_TRACK;

  hyscan_gtk_map_direction_update_angle (direction);
}

/* Обрабатывает нажатие кнопки переключения режима. */
static void
hyscan_gtk_map_direction_follow_toggled (GtkToggleButton       *button,
                                         HyScanGtkMapDirection *direction)
{
  HyScanGtkMapDirectionPrivate *priv = direction->priv;

  priv->follow_ship = gtk_toggle_button_get_active (button);
  if (priv->follow_ship)
    hyscan_gtk_map_direction_update_pos (direction);
}

GtkWidget *
hyscan_gtk_map_direction_new (HyScanGtkMap           *map,
                              HyScanPlannerSelection *selection,
                              HyScanNavModel         *nav_model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_DIRECTION,
                       "map", map,
                       "selection", selection,
                       "nav-model", nav_model,
                       NULL);
}
