#ifndef __HYSCAN_GTK_WATERFALL_MARK_H__
#define __HYSCAN_GTK_WATERFALL_MARK_H__

#include <hyscan-gtk-waterfall-layer.h>
#include <hyscan-gtk-waterfall.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_MARK             (hyscan_gtk_waterfall_mark_get_type ())
#define HYSCAN_GTK_WATERFALL_MARK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMark))
#define HYSCAN_IS_GTK_WATERFALL_MARK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))
#define HYSCAN_IS_GTK_WATERFALL_MARK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))

typedef struct _HyScanGtkWaterfallMark HyScanGtkWaterfallMark;
typedef struct _HyScanGtkWaterfallMarkPrivate HyScanGtkWaterfallMarkPrivate;
typedef struct _HyScanGtkWaterfallMarkClass HyScanGtkWaterfallMarkClass;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanGtkWaterfallMark
{
  GObject parent_instance;

  HyScanGtkWaterfallMarkPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanGtkWaterfallMarkClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                     hyscan_gtk_waterfall_mark_get_type  (void);

HYSCAN_API
HyScanGtkWaterfallMark  *hyscan_gtk_waterfall_mark_new        (HyScanGtkWaterfall   *waterfall);

//HYSCAN_API
//void                      hyscan_gtk_waterfall_mark_set_model (HyScanGtkWaterfallMark *mark,
//                                                               HyScanMarkModel         *model);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MARK_H__ */
