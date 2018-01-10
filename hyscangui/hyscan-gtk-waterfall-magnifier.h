/**
 * \file hyscan-gtk-waterfall-magnifier.h
 *
 * \brief Лупа для виджета водопад
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallMagnifier HyScanGtkWaterfallMagnifier - лупа для водопада
 *
 * Виджет создается методом #hyscan_gtk_waterfall_magnifier_new.
 *
 * Виджет обрабатывает сигнал "visible-draw" от \link GtkCifroArea \endlink.
 * В этом обработчике он рисует окошко с увеличенной частью отображаемой области.
 *
 * - #hyscan_gtk_waterfall_magnifier_set_size задает размер увеличиваемой части
 * - #hyscan_gtk_waterfall_magnifier_set_zoom задает степень увеличения
 * - #hyscan_gtk_waterfall_magnifier_set_position задает положение окошка
 * - #hyscan_gtk_waterfall_magnifier_set_frame_color задает цвет рамки
 * - #hyscan_gtk_waterfall_magnifier_set_frame_width задает толщину рамки
 *
 * Класс не потокобезопасен и должен использоваться в главном потоке.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_MAGNIFIER_H__
#define __HYSCAN_GTK_WATERFALL_MAGNIFIER_H__

#include <hyscan-gtk-waterfall-layer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER             (hyscan_gtk_waterfall_magnifier_get_type ())
#define HYSCAN_GTK_WATERFALL_MAGNIFIER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER, HyScanGtkWaterfallMagnifier))
#define HYSCAN_IS_GTK_WATERFALL_MAGNIFIER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER))
#define HYSCAN_GTK_WATERFALL_MAGNIFIER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER, HyScanGtkWaterfallMagnifierClass))
#define HYSCAN_IS_GTK_WATERFALL_MAGNIFIER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER))
#define HYSCAN_GTK_WATERFALL_MAGNIFIER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER, HyScanGtkWaterfallMagnifierClass))

typedef struct _HyScanGtkWaterfallMagnifier HyScanGtkWaterfallMagnifier;
typedef struct _HyScanGtkWaterfallMagnifierPrivate HyScanGtkWaterfallMagnifierPrivate;
typedef struct _HyScanGtkWaterfallMagnifierClass HyScanGtkWaterfallMagnifierClass;

struct _HyScanGtkWaterfallMagnifier
{
  GObject parent_instance;

  HyScanGtkWaterfallMagnifierPrivate *priv;
};

struct _HyScanGtkWaterfallMagnifierClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_gtk_waterfall_magnifier_get_type           (void);

/**
 *
 * Функция создает новый объект \link HyScanGtkWaterfallMagnifier \endlink
 *
 * \param waterfall указатель на виджет \link HyScanGtkWaterfall \endlink;
 *
 * \return новый объект \link HyScanGtkWaterfallMagnifier \endlink
 */
HYSCAN_API
HyScanGtkWaterfallMagnifier *hyscan_gtk_waterfall_magnifier_new (HyScanGtkWaterfall *waterfall);

/**
 *
 * Функция задает степень увеличения.
 *
 * \param magnifier указатель на объект \link HyScanGtkWaterfallMagnifier \endlink;
 * \param zoom степень увеличения; 0 отключает зуммирование.
 */
HYSCAN_API
void  hyscan_gtk_waterfall_magnifier_set_zoom           (HyScanGtkWaterfallMagnifier *magnifier,
                                                         guint                        zoom);

/**
 *
 * Функция задает размеры области под указатем, которая зуммируется.
 *
 * \param magnifier указатель на объект \link HyScanGtkWaterfallMagnifier \endlink;
 * \param width ширина области;
 * \param height высота области;
 */
HYSCAN_API
void  hyscan_gtk_waterfall_magnifier_set_size           (HyScanGtkWaterfallMagnifier *magnifier,
                                                         gdouble                      width,
                                                         gdouble                      height);
/**
 *
 * Функция задает начальную координату (левый верхний угол) окошка с увеличенным изображением.
 *
 * \param magnifier указатель на объект \link HyScanGtkWaterfallMagnifier \endlink;
 * \param x горизонтальная координата;
 * \param y вертикальная координата.
 */
HYSCAN_API
void  hyscan_gtk_waterfall_magnifier_set_position       (HyScanGtkWaterfallMagnifier *magnifier,
                                                         gdouble                      x,
                                                         gdouble                      y);

/**
 *
 * Функция задает цвет рамки.
 *
 * \param magnifier указатель на объект \link HyScanGtkWaterfallMagnifier \endlink;
 * \param color цвет рамки.
 */
HYSCAN_API
void  hyscan_gtk_waterfall_magnifier_set_frame_color    (HyScanGtkWaterfallMagnifier *magnifier,
                                                         GdkRGBA                      color);

/**
 *
 * Функция задает ширину рамки.
 *
 * \param magnifier указатель на объект \link HyScanGtkWaterfallMagnifier \endlink;
 * \param width ширина рамки.
 */
HYSCAN_API
void  hyscan_gtk_waterfall_magnifier_set_frame_width    (HyScanGtkWaterfallMagnifier *magnifier,
                                                         gdouble                      width);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MAGNIFIER_H__ */
