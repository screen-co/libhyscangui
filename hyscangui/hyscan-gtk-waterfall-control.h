#ifndef __HYSCAN_GTK_WATERFALL_CONTROL_H__
#define __HYSCAN_GTK_WATERFALL_CONTROL_H__

#include <glib-object.h>

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
  HyScanGtkWaterfall parent_instance;

  HyScanGtkWaterfallControlPrivate *priv;
};

struct _HyScanGtkWaterfallControlClass
{
  HyScanGtkWaterfallClass parent_class;
};

GType                   hyscan_gtk_waterfall_control_get_type  (void);

HYSCAN_API
GtkWidget              *hyscan_gtk_waterfall_control_new       (void);

/**
 *
 * Функция позволяет установить частоту кадров.
 * Данная функция влияет на плавность в режиме автосдвижки.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param fps - количество кадров в секунду.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_control_set_fps   (HyScanGtkWaterfallControl *control,
                                                                guint                      fps);

/**
 *
 * Функция включает режим автосдвига данных.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param on - TRUE: вкл, FALSE: выкл.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_control_automove  (HyScanGtkWaterfallControl *control,
                                                                gboolean                   on);



G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_CONTROL_H__ */
