#ifndef __HYSCAN_GTK_WATERFALL_MAYER_H__
#define __HYSCAN_GTK_WATERFALL_MAYER_H__

#include <hyscan-gtk-waterfall.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_MAYER             (hyscan_gtk_waterfall_mayer_get_type ())
#define HYSCAN_GTK_WATERFALL_MAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAYER, HyScanGtkWaterfallMayer))
#define HYSCAN_IS_GTK_WATERFALL_MAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAYER))
#define HYSCAN_GTK_WATERFALL_MAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_MAYER, HyScanGtkWaterfallMayerClass))
#define HYSCAN_IS_GTK_WATERFALL_MAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_MAYER))
#define HYSCAN_GTK_WATERFALL_MAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAYER, HyScanGtkWaterfallMayerClass))

typedef struct _HyScanGtkWaterfallMayer HyScanGtkWaterfallMayer;
typedef struct _HyScanGtkWaterfallMayerPrivate HyScanGtkWaterfallMayerPrivate;
typedef struct _HyScanGtkWaterfallMayerClass HyScanGtkWaterfallMayerClass;
typedef struct _HyScanGtkWaterfallMayerState HyScanGtkWaterfallMayerState;

struct _HyScanGtkWaterfallMayer
{
  GObject parent_instance;

  HyScanGtkWaterfallMayerPrivate *priv;
};

struct _HyScanGtkWaterfallMayerClass
{
  /* Виртуальные функции, доступные извне. */
  void         (*grab_input)   (HyScanGtkWaterfallMayer *layer);
  void         (*set_visible)  (HyScanGtkWaterfallMayer *layer,
                                gboolean                 visible);
  const gchar *(*get_mnemonic) (HyScanGtkWaterfallMayer *layer);

  GObjectClass parent_class;
};

GType                  hyscan_gtk_waterfall_mayer_get_type              (void);

void                   hyscan_gtk_waterfall_mayer_grab_input            (HyScanGtkWaterfallMayer *mayer);
void                   hyscan_gtk_waterfall_mayer_set_visible           (HyScanGtkWaterfallMayer *mayer,
                                                                         gboolean                 visible);
const gchar*           hyscan_gtk_waterfall_mayer_get_mnemonic          (HyScanGtkWaterfallMayer *mayer);



G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MAYER_H__ */
