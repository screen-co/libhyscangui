/**
 * \file hyscan-gtk-waterfall.h
 *
 * \brief Виджет "водопад".
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallLayer HyScanGtkWaterfallLayer - интерфейс слоев данных.
 *
 * Чтобы разгрузить основной класс было введено понятие слоя. Слой - это сущность,
 * рисующая какой-то предопределенный тип объектов. Например, сетка, метки, линейка.
 * Указатель на виджет передается слою в конструкторе.
 * \code
 *             WaterfallState
 *                   |
 *               Waterfall
 *              /    |    \
 *           Grid  Mark  Depth
 * \endcode
 *
 * Установлено, что есть режимы просмотра (когда )
 * В основном слои интерактивны, то есть умеют обрабатывать пользовательский ввод.
 * Однако одному пользовательскому воздействию может быть присвоена реакция в нескольких
 * слоях, а как видно на рисунке выше, слои не имеют связи между собой.
 * Чтобы слои могли понимать, кто сейчас обрабатывает ввод, в общем предке введены функции
 * \link hyscan_gtk_waterfall_set_input_owner захвата \endlink и
 * \link hyscan_gtk_waterfall_get_input_owner проверки обладателя \endlink ввода.
 *
 * Для удобства в интерфейсе есть функция #hyscan_gtk_waterfall_layer_grab_input, захватывающая
 * ввод.
 *
 * Ещё одно разделение - это возможность пользователя повлиять на объекты.
 * - Режим просмотра делает невозможным какое-либо влияние, пользователь может только прокручивать
 *   и зуммировать изображение.
 * - Режим редактирования позволяет дополнительно к прокрутке создавать новые объекты,
 *   удалять или изменять уже существующие,
 *
 * Однако как было сказано выше, разделение на слои носит частично искуственный характер, а потому есть
 * ряд специальных случаев.
 * 1. Слой управления водопадом (HyScanGtkWaterfallControl) имеет право ввода \b всегда. <br>
 *    так сделано для того, чтобы пользователь всегда имел возможность подвинуть область отображения.
 *     Достигается это тем, что WaterfallControl при захвате ввода передает NULL.
 * 2. Двойное нажатие мыши всегда отрабатывается слоем меток как установка "быстрой метки".
 *
 * Пока что всё.
 */



#ifndef __HYSCAN_GTK_WATERFALL_LAYER_H__
#define __HYSCAN_GTK_WATERFALL_LAYER_H__

#include <hyscan-gtk-waterfall.h>

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
