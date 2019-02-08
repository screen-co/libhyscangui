#ifndef __HYSCAN_GTK_MAP_H__
#define __HYSCAN_GTK_MAP_H__

#include <gtk/gtk.h>
#include <gtk-cifro-area.h>
#include <hyscan-geo-projection.h>

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
};

HYSCAN_API
GType                  hyscan_gtk_map_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_new              (HyScanGeoProjection *projection);

HYSCAN_API
void                   hyscan_gtk_map_move_to          (HyScanGtkMap        *map,
                                                        HyScanGeoGeodetic    center);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale        (HyScanGtkMap        *map);

HYSCAN_API
void                   hyscan_gtk_map_value_to_geo     (HyScanGtkMap        *map,
                                                        HyScanGeoGeodetic   *coords,
                                                        gdouble              x_val,
                                                        gdouble              y_val);

HYSCAN_API
gconstpointer          hyscan_gtk_map_get_howner       (HyScanGtkMap        *map);

HYSCAN_API
void                   hyscan_gtk_map_set_howner       (HyScanGtkMap        *map,
                                                        gconstpointer        howner);

HYSCAN_API
void                   hyscan_gtk_map_release_input   (HyScanGtkMap        *map,
                                                       gconstpointer        howner);

HYSCAN_API
gboolean               hyscan_gtk_map_grab_input      (HyScanGtkMap        *map,
                                                       gconstpointer        howner);

HYSCAN_API
gboolean               hyscan_gtk_map_is_howner       (HyScanGtkMap        *map,
                                                       gconstpointer        howner);



G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_H__ */
