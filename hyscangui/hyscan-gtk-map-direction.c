/* hyscan-gtk-map-direction.c
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
 * SECTION: hyscan-gtk-map-direaction
 * @Short_description: Виджет слежения за объектом на карте
 * @Title: HyScanGtkMapDirection
 *
 * Виджет предназначен для установки положения карты при навигации. Пользователю
 * доступны три радиокнопки, определяющие ориентацию карты, и один чекбокс для
 * слежения за судном.
 *
 * Доступные режимы ориентации карты:
 *
 * - вручную,
 * - по курсу движения судна,
 * - по направлению активного плана галса.
 *
 * В режиме слежения за судном видимая область карты перемещается вслед за судном.
 *
 */

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
  HyScanGtkMap                *map;           /* Виджет карты. */
  HyScanPlannerSelection      *selection;     /* Выбранные плановые галсы. */
  HyScanObjectModel           *obj_model;     /* Модель объектов планировщика. */
  HyScanNavState              *nav_model;     /* Модель навигационных данных. */

  HyScanGtkMapDirectionMode    mode;          /* Режим ориентации карты. */
  gboolean                     follow_ship;   /* Следить за судном? */

  gboolean                    nav_data_set;   /* Положение судна получено. */
  HyScanNavStateData          nav_data;       /* Положение судна. */
  gdouble                     track_angle;    /* Направление активного плана галса. */

  GtkWidget                   *btn_follow;    /* Чекбокс "Следить за судном". */
  GtkWidget                   *btn_manual;    /* Радиокнопка ориентации вручную. */
  GtkWidget                   *btn_track;     /* Радиокнопка ориентации по активному плану галса. */
  GtkWidget                   *btn_ship;      /* Радиокнопка ориентации по курсу судна. */
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
                                                              HyScanNavStateData    *nav_data);
static void    hyscan_gtk_map_direction_trk_chgd             (HyScanGtkMapDirection *direction);
static void    hyscan_gtk_map_direction_update_pos           (HyScanGtkMapDirection *direction);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapDirection, hyscan_gtk_map_direction, GTK_TYPE_BOX)

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
    g_param_spec_object ("nav-state", "Navigation model", "Navigation model", HYSCAN_TYPE_NAV_STATE,
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
  GtkWidget *line1;

  G_OBJECT_CLASS (hyscan_gtk_map_direction_parent_class)->constructed (object);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

  priv->btn_manual = gtk_radio_button_new_with_mnemonic_from_widget (NULL, _("_User"));
  priv->btn_track  = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (priv->btn_manual), _("_Track"));
  priv->btn_ship   = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (priv->btn_manual), _("_Ship"));
  priv->btn_follow = gtk_check_button_new_with_mnemonic(_("_Follow ship"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_manual), priv->mode == MODE_MANUAL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_track), priv->mode == MODE_TRACK);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->btn_ship), priv->mode == MODE_SHIP);

  line1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (line1), gtk_label_new (_("Orientation")), TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (line1), priv->btn_manual, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (line1), priv->btn_track, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (line1), priv->btn_ship, TRUE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (object), line1, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (object), priv->btn_follow, TRUE, TRUE, 0);

  g_signal_connect (priv->btn_manual, "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_track,  "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_ship,   "toggled", G_CALLBACK (hyscan_gtk_map_direction_mode_toggled), object);
  g_signal_connect (priv->btn_follow, "toggled", G_CALLBACK (hyscan_gtk_map_direction_follow_toggled), object);

  priv->obj_model = HYSCAN_OBJECT_MODEL (hyscan_planner_selection_get_model (priv->selection));

  g_signal_connect_swapped (priv->nav_model, "nav-changed", G_CALLBACK (hyscan_gtk_map_direction_nav_chgd), object);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_direction_trk_chgd), object);
  g_signal_connect_swapped (priv->obj_model, "changed", G_CALLBACK (hyscan_gtk_map_direction_trk_chgd), object);
}

static void
hyscan_gtk_map_direction_object_finalize (GObject *object)
{
  HyScanGtkMapDirection *gtk_map_direction = HYSCAN_GTK_MAP_DIRECTION (object);
  HyScanGtkMapDirectionPrivate *priv = gtk_map_direction->priv;

  g_signal_handlers_disconnect_by_data (priv->nav_model, object);
  g_signal_handlers_disconnect_by_data (priv->selection, object);
  g_signal_handlers_disconnect_by_data (priv->obj_model, object);

  g_object_unref (priv->selection);
  g_object_unref (priv->obj_model);
  g_object_unref (priv->map);
  g_object_unref (priv->nav_model);

  G_OBJECT_CLASS (hyscan_gtk_map_direction_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_direction_nav_chgd (HyScanGtkMapDirection *direction,
                                   HyScanNavStateData    *nav_data)
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

  track = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (priv->obj_model),
                                                          HYSCAN_TYPE_PLANNER_TRACK,
                                                          active_id);
  g_free (active_id);
  if (!HYSCAN_IS_PLANNER_TRACK (track))
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

/**
 * hyscan_gtk_map_direction_new:
 * @map: карта #HyScanGtkMap
 * @selection: модель выбранных объектов планировщика
 * @nav_model: модель навигационных данных
 *
 * Функция создаёт виджет для управления положением карты.
 *
 * Returns: новый виджет для управления положением карты
 */
GtkWidget *
hyscan_gtk_map_direction_new (HyScanGtkMap           *map,
                              HyScanPlannerSelection *selection,
                              HyScanNavState         *nav_model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_DIRECTION,
                       "map", map,
                       "selection", selection,
                       "nav-state", nav_model,
                       NULL);
}
