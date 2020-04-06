#ifndef __HYSCAN_GTK_MAP_KIT_H__
#define __HYSCAN_GTK_MAP_KIT_H__

#include <hyscan-gtk-map.h>
#include <hyscan-units.h>
#include <hyscan-db.h>
#include <hyscan-sensor.h>
#include <hyscan-sonar-recorder.h>

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

HyScanGtkMapKit * hyscan_gtk_map_kit_new              (HyScanGeoPoint    *center,
                                                       HyScanDB          *db,
                                                       HyScanUnits       *units,
                                                       const gchar       *cache_dir);

void              hyscan_gtk_map_kit_set_project      (HyScanGtkMapKit   *kit,
                                                       const gchar       *project_name);

gboolean          hyscan_gtk_map_kit_set_profile_name (HyScanGtkMapKit   *kit,
                                                       const gchar       *profile_name);

gchar *           hyscan_gtk_map_kit_get_profile_name (HyScanGtkMapKit   *kit);

void              hyscan_gtk_map_kit_load_profiles    (HyScanGtkMapKit   *kit,
                                                       const gchar       *profile_dir);

void              hyscan_gtk_map_kit_add_nav          (HyScanGtkMapKit           *kit,
                                                       HyScanSensor              *sensor,
                                                       const gchar               *sensor_name,
                                                       HyScanSonarRecorder       *recorder,
                                                       const HyScanAntennaOffset *offset,
                                                       gdouble                    delay_time);

void              hyscan_gtk_map_kit_add_marks_wf     (HyScanGtkMapKit   *kit);

void              hyscan_gtk_map_kit_add_planner      (HyScanGtkMapKit   *kit);

void              hyscan_gtk_map_kit_add_marks_geo    (HyScanGtkMapKit   *kit);

void              hyscan_gtk_map_kit_free             (HyScanGtkMapKit   *kit);

#endif /* __HYSCAN_GTK_MAP_KIT_H__ */
