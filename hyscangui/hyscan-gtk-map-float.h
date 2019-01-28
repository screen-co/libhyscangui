#ifndef __HYSCAN_GTK_MAP_FLOAT_H__
#define __HYSCAN_GTK_MAP_FLOAT_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_FLOAT             (hyscan_gtk_map_float_get_type ())
#define HYSCAN_GTK_MAP_FLOAT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_FLOAT, HyScanGtkMapFloat))
#define HYSCAN_IS_GTK_MAP_FLOAT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_FLOAT))
#define HYSCAN_GTK_MAP_FLOAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_FLOAT, HyScanGtkMapFloatClass))
#define HYSCAN_IS_GTK_MAP_FLOAT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_FLOAT))
#define HYSCAN_GTK_MAP_FLOAT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_FLOAT, HyScanGtkMapFloatClass))

typedef struct _HyScanGtkMapFloat HyScanGtkMapFloat;
typedef struct _HyScanGtkMapFloatPrivate HyScanGtkMapFloatPrivate;
typedef struct _HyScanGtkMapFloatClass HyScanGtkMapFloatClass;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanGtkMapFloat
{
  GObject parent_instance;

  HyScanGtkMapFloatPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanGtkMapFloatClass
{
  GObjectClass parent_class;
};

GType                 hyscan_gtk_map_float_get_type (void);

HyScanGtkMapFloat *   hyscan_gtk_map_float_new      (HyScanGtkMap *map);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_FLOAT_H__ */
