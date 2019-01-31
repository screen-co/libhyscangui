#ifndef __HYSCAN_GTK_CRTS_MAP_H__
#define __HYSCAN_GTK_CRTS_MAP_H__

#include <glib-object.h>
#include <hyscan-gtk-map.h>
#include <hyscan-geo.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_CRTS_MAP             (hyscan_gtk_crts_map_get_type ())
#define HYSCAN_GTK_CRTS_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_CRTS_MAP, HyScanGtkCrtsMap))
#define HYSCAN_IS_GTK_CRTS_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_CRTS_MAP))
#define HYSCAN_GTK_CRTS_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_CRTS_MAP, HyScanGtkCrtsMapClass))
#define HYSCAN_IS_GTK_CRTS_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_CRTS_MAP))
#define HYSCAN_GTK_CRTS_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_CRTS_MAP, HyScanGtkCrtsMapClass))

typedef struct _HyScanGtkCrtsMap HyScanGtkCrtsMap;
typedef struct _HyScanGtkCrtsMapPrivate HyScanGtkCrtsMapPrivate;
typedef struct _HyScanGtkCrtsMapClass HyScanGtkCrtsMapClass;

struct _HyScanGtkCrtsMap
{
  HyScanGtkMap parent_instance;

  HyScanGtkCrtsMapPrivate *priv;
};

struct _HyScanGtkCrtsMapClass
{
  HyScanGtkMapClass parent_class;
};

GType                  hyscan_gtk_crts_map_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_crts_map_new              (HyScanGeo *geo);

G_END_DECLS

#endif /* __HYSCAN_GTK_CRTS_MAP_H__ */
