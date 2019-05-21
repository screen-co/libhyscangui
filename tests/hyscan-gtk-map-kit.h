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

  /* ## Private. ## */
  HyScanGtkMapKitPrivate *priv;
} HyScanGtkMapKit;

HyScanGtkMapKit * hyscan_gtk_map_kit_new   (HyScanGeoGeodetic *center,
                                            HyScanDB          *db,
                                            const gchar       *project_name,
                                            const gchar       *profile_dir,
                                            HyScanSensor      *sensor,
                                            const gchar       *sensor_name);

void              hyscan_gtk_map_kit_free  (HyScanGtkMapKit   *kit);

#endif /* __HYSCAN_GTK_MAP_KIT_H__ */
