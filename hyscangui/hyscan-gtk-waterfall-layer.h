#ifndef __HYSCAN_GTK_WATERFALL_LAYER_H__
#define __HYSCAN_GTK_WATERFALL_LAYER_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_LAYER            (hyscan_gtk_waterfall_layer_get_type ())
#define HYSCAN_GTK_WATERFALL_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_LAYER, HyScanGtkWaterfallLayer))
#define HYSCAN_IS_GTK_WATERFALL_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_LAYER))
#define HYSCAN_GTK_WATERFALL_LAYER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_WATERFALL_LAYER, HyScanGtkWaterfallLayerInterface))

typedef struct _HyScanGtkWaterfallLayer HyScanGtkWaterfallLayer;
typedef struct _HyScanGtkWaterfallLayerInterface HyScanGtkWaterfallLayerInterface;

struct _HyScanGtkWaterfallLayerInterface
{
  GTypeInterface       g_iface;

  void         (*grab_input)   (HyScanGtkWaterfallLayer *layer);

  const gchar *(*get_mnemonic) (HyScanGtkWaterfallLayer *layer);
};

HYSCAN_API
GType           hyscan_gtk_waterfall_layer_get_type     (void);

HYSCAN_API
void            hyscan_gtk_waterfall_layer_grab_input   (HyScanGtkWaterfallLayer *layer);

HYSCAN_API
const gchar    *hyscan_gtk_waterfall_layer_get_mnemonic (HyScanGtkWaterfallLayer *layer);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_LAYER_H__ */
