#ifndef __HYSCAN_GTK_MAP_CONTROL_H__
#define __HYSCAN_GTK_MAP_CONTROL_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_CONTROL             (hyscan_gtk_map_control_get_type ())
#define HYSCAN_GTK_MAP_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_CONTROL, HyScanGtkMapControl))
#define HYSCAN_IS_GTK_MAP_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_CONTROL))
#define HYSCAN_GTK_MAP_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_CONTROL, HyScanGtkMapControlClass))
#define HYSCAN_IS_GTK_MAP_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_CONTROL))
#define HYSCAN_GTK_MAP_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_CONTROL, HyScanGtkMapControlClass))

typedef struct _HyScanGtkMapControl HyScanGtkMapControl;
typedef struct _HyScanGtkMapControlPrivate HyScanGtkMapControlPrivate;
typedef struct _HyScanGtkMapControlClass HyScanGtkMapControlClass;

struct _HyScanGtkMapControl
{
  GObject parent_instance;

  HyScanGtkMapControlPrivate *priv;
};

struct _HyScanGtkMapControlClass
{
  GObjectClass parent_class;
};

GType                  hyscan_gtk_map_control_get_type         (void);

HYSCAN_API
HyScanGtkMapControl *  hyscan_gtk_map_control_new              (void);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_CONTROL_H__ */
