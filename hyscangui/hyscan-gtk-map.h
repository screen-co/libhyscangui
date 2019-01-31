#ifndef __HYSCAN_GTK_MAP_H__
#define __HYSCAN_GTK_MAP_H__

#include <gtk/gtk.h>
#include <gtk-cifro-area.h>
#include <hyscan-geo.h>

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


struct _HyScanGtkMap
{
  GtkCifroArea parent_instance;

  HyScanGtkMapPrivate *priv;
};

struct _HyScanGtkMapClass
{
  GtkCifroAreaClass parent_class;

  void (*tile_to_point) (HyScanGtkMap       *map,
                         gdouble            *x,
                         gdouble            *y,
                         gdouble             x_tile,
                         gdouble             y_tile);

  void (*point_to_tile) (HyScanGtkMap       *map,
                         gdouble             x,
                         gdouble             y,
                         gdouble            *x_tile,
                         gdouble            *y_tile);
};

HYSCAN_API
GType                  hyscan_gtk_map_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_new              (void);

HYSCAN_API
void                   hyscan_gtk_map_move_to          (HyScanGtkMap       *map,
                                                        HyScanGeoGeodetic   center);

HYSCAN_API
void                   hyscan_gtk_map_set_zoom         (HyScanGtkMap       *map,
                                                        guint               zoom);

HYSCAN_API
guint                  hyscan_gtk_map_get_zoom         (HyScanGtkMap       *map);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale        (HyScanGtkMap       *map);

HYSCAN_API
void                   hyscan_gtk_map_tile_to_point    (HyScanGtkMap       *map,
                                                        gdouble            *x,
                                                        gdouble            *y,
                                                        gdouble             x_tile,
                                                        gdouble             y_tile);
HYSCAN_API
void                   hyscan_gtk_map_point_to_tile    (HyScanGtkMap       *map,
                                                        gdouble             x,
                                                        gdouble             y,
                                                        gdouble            *x_tile,
                                                        gdouble            *y_tile);


HYSCAN_API
void                   hyscan_gtk_map_get_tile_view_i  (HyScanGtkMap       *map,
                                                        guint              *from_tile_x,
                                                        guint              *to_tile_x,
                                                        guint              *from_tile_y,
                                                        guint              *to_tile_y);

HYSCAN_API
void                   hyscan_gtk_map_geo_to_tile      (guint               zoom,
                                                        HyScanGeoGeodetic   coords,
                                                        gdouble            *tile_x,
                                                        gdouble            *tile_y);

HYSCAN_API
void                   hyscan_gtk_map_tile_to_geo      (guint               zoom,
                                                        HyScanGeoGeodetic  *coords,
                                                        gdouble             tile_x,
                                                        gdouble             tile_y);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_H__ */
