#ifndef __HYSCAN_GTK_WATERFALL_CONTROL_H__
#define __HYSCAN_GTK_WATERFALL_CONTROL_H__

#include <hyscan-gtk-waterfall-layer.h>
#include <hyscan-gtk-waterfall.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_CONTROL             (hyscan_gtk_waterfall_control_get_type ())
#define HYSCAN_GTK_WATERFALL_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_CONTROL, HyScanGtkWaterfallControl))
#define HYSCAN_IS_GTK_WATERFALL_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_CONTROL))
#define HYSCAN_GTK_WATERFALL_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_CONTROL, HyScanGtkWaterfallControlClass))
#define HYSCAN_IS_GTK_WATERFALL_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_CONTROL))
#define HYSCAN_GTK_WATERFALL_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_CONTROL, HyScanGtkWaterfallControlClass))

typedef struct _HyScanGtkWaterfallControl HyScanGtkWaterfallControl;
typedef struct _HyScanGtkWaterfallControlPrivate HyScanGtkWaterfallControlPrivate;
typedef struct _HyScanGtkWaterfallControlClass HyScanGtkWaterfallControlClass;

struct _HyScanGtkWaterfallControl
{
  GObject parent_instance;

  HyScanGtkWaterfallControlPrivate *priv;
};

struct _HyScanGtkWaterfallControlClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                       hyscan_gtk_waterfall_control_get_type (void);

HYSCAN_API
HyScanGtkWaterfallControl  *hyscan_gtk_waterfall_control_new      (HyScanGtkWaterfall   *waterfall);

HYSCAN_API
void                        hyscan_gtk_waterfall_control_set_wheel_behaviour (HyScanGtkWaterfallControl *control,
                                                                              gboolean                   scroll_without_ctrl);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_CONTROL_H__ */
