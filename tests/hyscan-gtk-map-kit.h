#ifndef __HYSCAN_GTK_MAP_KIT_H__
#define __HYSCAN_GTK_MAP_KIT_H__

#include <hyscan-gtk-map.h>
#include <hyscan-db.h>
#include <hyscan-sensor.h>

typedef struct _HyScanGtkMapKitPrivate HyScanGtkMapKitPrivate;

typedef struct
{
  /* Виджеты. */
  GtkWidget *map;                 /* Виджет карты. */
  GtkWidget *control;             /* Виджет с кнопками управления карты. */

  /* ## Private. ## */
  HyScanGtkMapKitPrivate *priv;

  HyScanGeoGeodetic center;       /* Географические координаты для виджета навигации. */

  GtkWidget *layer_tool_stack;    /* GtkStack с виджетами настроек каждого слоя. */

  /* Слои. */
  HyScanGtkLayer *track_layer;    /* Слой просмотра галсов. */
  HyScanGtkLayer *wfmark_layer;   /* Слой с метками водопада. */
  HyScanGtkLayer *map_grid;       /* Слой координатной сетки. */
  HyScanGtkLayer *ruler;          /* Слой линейки. */
  HyScanGtkLayer *pin_layer;      /* Слой географических отметок. */
  HyScanGtkLayer *way_layer;      /* Слой текущего положения по GPS-датчику. */

} HyScanGtkMapKit;

HyScanGtkMapKit * hyscan_gtk_map_kit_new   (HyScanGeoGeodetic *center,
                                            HyScanDB          *db,
                                            const gchar       *project_name,
                                            const gchar       *profile_dir,
                                            HyScanSensor      *sensor,
                                            const gchar       *sensor_name);

void              hyscan_gtk_map_kit_free  (HyScanGtkMapKit   *kit);

#endif /* __HYSCAN_GTK_MAP_KIT_H__ */
