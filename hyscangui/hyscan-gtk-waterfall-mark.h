#ifndef __HYSCAN_GTK_WATERFALL_MARK_H__
#define __HYSCAN_GTK_WATERFALL_MARK_H__

#include <hyscan-gtk-waterfall-layer.h>
#include <hyscan-gtk-waterfall.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_GTK_WATERFALL_MARKS_ALL             G_MAXUINT64

#define HYSCAN_TYPE_GTK_WATERFALL_MARK             (hyscan_gtk_waterfall_mark_get_type ())
#define HYSCAN_GTK_WATERFALL_MARK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMark))
#define HYSCAN_IS_GTK_WATERFALL_MARK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))
#define HYSCAN_IS_GTK_WATERFALL_MARK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))

typedef struct _HyScanGtkWaterfallMark HyScanGtkWaterfallMark;
typedef struct _HyScanGtkWaterfallMarkPrivate HyScanGtkWaterfallMarkPrivate;
typedef struct _HyScanGtkWaterfallMarkClass HyScanGtkWaterfallMarkClass;

struct _HyScanGtkWaterfallMark
{
  GObject parent_instance;

  HyScanGtkWaterfallMarkPrivate *priv;
};

struct _HyScanGtkWaterfallMarkClass
{
  GObjectClass parent_class;
};

typedef enum
{
  HYSCAN_GTK_WATERFALL_MARKS_DRAW_DOTS,
  HYSCAN_GTK_WATERFALL_MARKS_DRAW_BORDER
} HyScanGtkWaterfallMarksDraw;

HYSCAN_API
GType                    hyscan_gtk_waterfall_mark_get_type            (void);

/**
 *
 * Функция создает новый объект \link HyScanGtkWaterfallMark \endlink.
 *
 * \param waterfall указатель на виджет \link HyScanGtkWaterfall \endlink;
 *
 * \return новый объект HyScanGtkWaterfallMark.
 *
 */
HYSCAN_API
HyScanGtkWaterfallMark  *hyscan_gtk_waterfall_mark_new                 (HyScanGtkWaterfall         *waterfall);

/**
 *
 * Функция задает фильтр меток.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param filter
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_mark_filter     (HyScanGtkWaterfallMark     *mark,
                                                                        guint64                     filter);
/**
 *
 * Функция задает тип отображения и пока что не работает.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param type
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_draw_type       (HyScanGtkWaterfallMark     *mark,
                                                                        HyScanGtkWaterfallMarksDraw type);

/**
 *
 * Функция задает основной цвет.
 * Влияет на цвет меток и подписей.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param color основной цвет.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_main_color      (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);
/**
 *
 * Функция задает толщину основных линий.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param width толщина в пикселях.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_mark_width      (HyScanGtkWaterfallMark     *mark,
                                                                        gdouble                     width);
/**
 *
 * Функция задает цвет рамки вокруг текста.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param color цвет рамки.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_frame_color     (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

/**
 *
 * Функция задает цвет подложки.
 * Подложка рисуется под текстом и линиями метки.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param color цвет подложки.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_shadow_color    (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);
/**
 *
 * Функция задает ширину линий подложки.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param width ширина линий.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_shadow_width    (HyScanGtkWaterfallMark     *mark,
                                                                        gdouble                     width);
/**
 *
 * Функция задает цвет затемнения.
 * Затемнение рисуется в момент создания метки.
 *
 * \param mark указатель на объект \link HyScanGtkWaterfallMark \endlink;
 * \param color цвет затемнения.
 *
 */
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_blackout_color  (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MARK_H__ */
