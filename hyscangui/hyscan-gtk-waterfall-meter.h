/**
 * \file hyscan-gtk-waterfall-meter.h
 *
 * \brief Измерения для водопада
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallMeter HyScanGtkWaterfallMeter - слой измерений
 *
 * Данный слой позволяет производить простые линейные измерения на водопаде.
 * Он поддерживает бесконечное число линеек, каждая из которых состоит из
 * двух точек.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_METER_H__
#define __HYSCAN_GTK_WATERFALL_METER_H__

#include <hyscan-gtk-waterfall-layer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_METER             (hyscan_gtk_waterfall_meter_get_type ())
#define HYSCAN_GTK_WATERFALL_METER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeter))
#define HYSCAN_IS_GTK_WATERFALL_METER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))
#define HYSCAN_IS_GTK_WATERFALL_METER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))

typedef struct _HyScanGtkWaterfallMeter HyScanGtkWaterfallMeter;
typedef struct _HyScanGtkWaterfallMeterPrivate HyScanGtkWaterfallMeterPrivate;
typedef struct _HyScanGtkWaterfallMeterClass HyScanGtkWaterfallMeterClass;

struct _HyScanGtkWaterfallMeter
{
  GObject parent_instance;

  HyScanGtkWaterfallMeterPrivate *priv;
};

struct _HyScanGtkWaterfallMeterClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                           hyscan_gtk_waterfall_meter_get_type            (void);

/**
 *
 * Функция создает новый слой HyScanGtkWaterfallMeter.
 *
 * \param waterfall указатель на виджет \link HyScanGtkWaterfall \endlink;
 *
 * \return новый слой HyScanGtkWaterfallMeter.
 */
HYSCAN_API
HyScanGtkWaterfallMeter        *hyscan_gtk_waterfall_meter_new                 (HyScanGtkWaterfall      *waterfall);

/**
 *
 * Функция задает основной цвет.
 * Влияет на цвет линеек и текста.
 *
 * \param meter указатель на \link HyScanGtkWaterfallMeter \endlink;
 * \param color основной цвет.
 */
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_main_color      (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);
/**
 *
 * Функция устанавливает цвет подложки.
 * Подложки рисуются под линейками и текстом.
 *
 * \param meter указатель на \link HyScanGtkWaterfallMeter \endlink;
 * \param color цвет подложки.
 */
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_color    (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);
/**
 *
 * Функция задает цвет рамки.
 * Рамки рисуются вокруг подложки под текстом.
 *
 * \param meter указатель на \link HyScanGtkWaterfallMeter \endlink;
 * \param color цвет рамок.
 */
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_frame_color     (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);
/**
 *
 * Функция задает толщину основных линий.
 *
 * \param meter указатель на \link HyScanGtkWaterfallMeter \endlink;
 * \param width толщина основных линий.
 */
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_meter_width     (HyScanGtkWaterfallMeter *meter,
                                                                                gdouble                  width);
/**
 *
 * Функция задает толщину подложки.
 *
 * \param meter указатель на \link HyScanGtkWaterfallMeter \endlink;
 * \param width толщина подложки.
 */
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_width    (HyScanGtkWaterfallMeter *meter,
                                                                                gdouble                  width);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_METER_H__ */
