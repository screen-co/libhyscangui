/**
 * \file hyscan-gtk-waterfall-control.h
 *
 * \brief Управление видимой областью водопада
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallControl HyScanGtkWaterfallControl - управление видимой областью
 *
 * Слой HyScanGtkWaterfallControl предназначен для управления видимой областью.
 * Это включает в себя зуммирование и перемещение.
 *
 * Класс умеет обрабатывать движения мыши, прокрутку колеса и нажатия кнопок
 * клавиатуры: стрелки, +/-, PgUp, PgDown, Home, End.
 *
 * - #hyscan_gtk_waterfall_control_new создание объекта;
 * - #hyscan_gtk_waterfall_control_set_wheel_behaviour настройка колесика мыши;
 * - #hyscan_gtk_waterfall_control_zoom масштабирование изображения.
 *
 */
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
GType                       hyscan_gtk_waterfall_control_get_type              (void);

/**
 *
 * Функция создает новый объект.
 *
 * \param waterfall родительский виджет.
 *
 * \return указатель на объект HyScanGtkWaterfallControl.
 *
 */
HYSCAN_API
HyScanGtkWaterfallControl  *hyscan_gtk_waterfall_control_new                   (HyScanGtkWaterfall        *waterfall);

/**
 *
 * Функция задает поведение колесика мыши.
 *
 * Колесико мыши может быть настроено на прокрутку или зуммирование.
 * Альтернативное действие будет выполняться с нажатой клавишей Ctrl.
 *
 * \param control указатель на объект HyScanGtkWaterfallControl;
 * \param scroll_without_ctrl TRUE, чтобы колесо мыши отвечало за прокрутку, FALSE,
 * чтобы колесо мыши отвечало за зуммирование.
 *
 */
HYSCAN_API
void                        hyscan_gtk_waterfall_control_set_wheel_behaviour   (HyScanGtkWaterfallControl *control,
                                                                                gboolean                   scroll_without_ctrl);
/**
 *
 * Функция масштабирует изображение относительно центра.
 *
 * \param wfall указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param zoom_in направление масштабирования (TRUE - увеличение, FALSE - уменьшение).
 *
 */
HYSCAN_API
void                        hyscan_gtk_waterfall_control_zoom                  (HyScanGtkWaterfallControl *wfall,
                                                                                gboolean                   zoom_in);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_CONTROL_H__ */
