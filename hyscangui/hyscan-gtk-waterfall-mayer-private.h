#ifndef __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__
#define __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__

#include <hyscan-gtk-waterfall-state.h>

G_BEGIN_DECLS

struct _HyScanGtkWaterfallMayerPrivate
{
  HyScanGtkWaterfallState     *wf_state;
  HyScanGtkWaterfall          *wfall;
  GtkCifroArea                *carea;
};

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__ */
