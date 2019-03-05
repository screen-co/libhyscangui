#ifndef __HYSCAN_GTK_MAP_H__
#define __HYSCAN_GTK_MAP_H__

#include <hyscan-geo-projection.h>
#include <hyscan-gtk-layer-container.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP             (hyscan_gtk_map_get_type ())
#define HYSCAN_GTK_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP, HyScanGtkMap))
#define HYSCAN_IS_GTK_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP))
#define HYSCAN_GTK_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP, HyScanGtkMapClass))
#define HYSCAN_IS_GTK_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP))
#define HYSCAN_GTK_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP, HyScanGtkMapClass))

typedef struct _HyScanGtkMap HyScanGtkMap;
typedef struct _HyScanGtkMapPrivate HyScanGtkMapPrivate;
typedef struct _HyScanGtkMapClass HyScanGtkMapClass;

/* Координаты точки. */
typedef struct
{
  HyScanGeoGeodetic geo;
  gdouble           x;
  gdouble           y;
} HyScanGtkMapPoint;

struct _HyScanGtkMap
{
  HyScanGtkLayerContainer parent_instance;

  HyScanGtkMapPrivate *priv;
};

struct _HyScanGtkMapClass
{
  HyScanGtkLayerContainerClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_new              (HyScanGeoProjection    *projection);

HYSCAN_API
void                   hyscan_gtk_map_set_projection   (HyScanGtkMap           *map,
                                                        HyScanGeoProjection    *projection);

HYSCAN_API
void                   hyscan_gtk_map_move_to          (HyScanGtkMap           *map,
                                                        HyScanGeoGeodetic       center);

HYSCAN_API
void                   hyscan_gtk_map_set_scale        (HyScanGtkMap           *map,
                                                        gdouble                 scale);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale        (HyScanGtkMap           *map);

HYSCAN_API
void                   hyscan_gtk_map_point_to_geo     (HyScanGtkMap           *map,
                                                        HyScanGeoGeodetic      *coords,
                                                        gdouble                 x,
                                                        gdouble                 y);

HYSCAN_API
void                   hyscan_gtk_map_value_to_geo     (HyScanGtkMap           *map,
                                                        HyScanGeoGeodetic      *coords,
                                                        gdouble                 x_val,
                                                        gdouble                 y_val);

HYSCAN_API
void                   hyscan_gtk_map_geo_to_value     (HyScanGtkMap           *map,
                                                        HyScanGeoGeodetic       coords,
                                                        gdouble                *x_val,
                                                        gdouble                *y_val);

HYSCAN_API
HyScanGtkMapPoint *    hyscan_gtk_map_point_copy       (HyScanGtkMapPoint      *point);

HYSCAN_API
void                   hyscan_gtk_map_point_free       (HyScanGtkMapPoint      *point);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_H__ */
