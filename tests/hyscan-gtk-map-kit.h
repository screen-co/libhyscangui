#ifndef __HYSCAN_GTK_MAP_KIT_H__
#define __HYSCAN_GTK_MAP_KIT_H__

#include <hyscan-gtk-map.h>
#include <hyscan-db.h>
#include <hyscan-sensor.h>


typedef struct _HyScanGtkMapKitPrivate HyScanGtkMapKitPrivate;

typedef struct
{

  GtkWidget *navigation;          /* Виджет со списком меток и галсов. */
  GtkWidget *map;                 /* Виджет карты. */
  GtkWidget *control;             /* Виджет с кнопками управления карты. */
  GtkWidget *status_bar;          /* Виджет со статусом. */

  /* ## Private. ## */
  HyScanGtkMapKitPrivate *priv;
} HyScanGtkMapKit;

HYSCAN_API
HyScanGtkMapKit * hyscan_gtk_map_kit_new              (HyScanGeoGeodetic *center,
                                                       HyScanDB          *db,
                                                       const gchar       *cache_dir);

HYSCAN_API
void              hyscan_gtk_map_kit_set_project      (HyScanGtkMapKit   *kit,
                                                       const gchar       *project_name);

HYSCAN_API
gboolean          hyscan_gtk_map_kit_set_profile_name (HyScanGtkMapKit   *kit,
                                                       const gchar       *profile_name);

HYSCAN_API
gchar *           hyscan_gtk_map_kit_get_profile_name (HyScanGtkMapKit   *kit);

HYSCAN_API
void              hyscan_gtk_map_kit_load_profiles    (HyScanGtkMapKit   *kit,
                                                       const gchar       *profile_dir);

HYSCAN_API
void              hyscan_gtk_map_kit_add_planner      (HyScanGtkMapKit   *kit,
                                                       const gchar       *planner_ini);

HYSCAN_API
void              hyscan_gtk_map_kit_add_nav          (HyScanGtkMapKit   *kit,
                                                       HyScanSensor      *sensor,
                                                       const gchar       *sensor_name,
                                                       gdouble            delay_time);

HYSCAN_API
void              hyscan_gtk_map_kit_add_marks_wf     (HyScanGtkMapKit   *kit);

HYSCAN_API
void              hyscan_gtk_map_kit_add_marks_geo    (HyScanGtkMapKit   *kit);

HYSCAN_API
void              hyscan_gtk_map_kit_free             (HyScanGtkMapKit   *kit);

#endif /* __HYSCAN_GTK_MAP_KIT_H__ */
