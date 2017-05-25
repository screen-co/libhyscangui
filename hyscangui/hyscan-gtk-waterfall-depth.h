#ifndef __HYSCAN_GTK_WATERFALL_DEPTH_H__
#define __HYSCAN_GTK_WATERFALL_DEPTH_H__

#include <hyscan-gtk-waterfall-grid.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_DEPTH             (hyscan_gtk_waterfall_depth_get_type ())
#define HYSCAN_GTK_WATERFALL_DEPTH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_DEPTH, HyScanGtkWaterfallDepth))
#define HYSCAN_IS_GTK_WATERFALL_DEPTH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_DEPTH))
#define HYSCAN_GTK_WATERFALL_DEPTH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_DEPTH, HyScanGtkWaterfallDepthClass))
#define HYSCAN_IS_GTK_WATERFALL_DEPTH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_DEPTH))
#define HYSCAN_GTK_WATERFALL_DEPTH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_DEPTH, HyScanGtkWaterfallDepthClass))

typedef struct _HyScanGtkWaterfallDepth HyScanGtkWaterfallDepth;
typedef struct _HyScanGtkWaterfallDepthPrivate HyScanGtkWaterfallDepthPrivate;
typedef struct _HyScanGtkWaterfallDepthClass HyScanGtkWaterfallDepthClass;

struct _HyScanGtkWaterfallDepth
{
  HyScanGtkWaterfallGrid parent_instance;

  HyScanGtkWaterfallDepthPrivate *priv;
};

struct _HyScanGtkWaterfallDepthClass
{
  HyScanGtkWaterfallGridClass parent_class;
};

HYSCAN_API
GType                    hyscan_gtk_waterfall_depth_get_type         (void);

HYSCAN_API
GtkWidget               *hyscan_gtk_waterfall_depth_new              (void);

HYSCAN_API
void                     hyscan_gtk_waterfall_depth_show             (HyScanGtkWaterfallDepth *wfdepth,
                                                                      gboolean                 show);

HYSCAN_API
void                     hyscan_gtk_waterfall_depth_set_color        (HyScanGtkWaterfallDepth *wfdepth,
                                                                      guint32                  color);

HYSCAN_API
void                     hyscan_gtk_waterfall_depth_set_width        (HyScanGtkWaterfallDepth *wfdepth,
                                                                      gdouble                  width);
G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_DEPTH_H__ */
