 /**
 * \file hyscan-gtk-waterfall-layer.h
 *
 * \brief Слои для виджета водопад
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallLayer HyScanGtkWaterfallLayer - интерфейс слоев данных
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
 * В основном слои интерактивны, то есть умеют обрабатывать пользовательский ввод.
 * Поскольку пользовательский опыт унифицирован, одному пользовательскому воздействию
 * может быть присвоена реакция в нескольких слоях, а как видно на рисунке выше,
 * слои не имеют связи между собой.
 *
 * ## Встраивание водопада ##
 *
 * Для встраивания водопада нужно знать про функцию #hyscan_gtk_waterfall_layer_grab_input.
 * Вызов этой функции автоматически разрешит изменения и передаст право ввода нужному
 * слою.
 *
 * Функция #hyscan_gtk_waterfall_layer_get_mnemonic пока что выступает в роли заглушки
 * и возвращает название иконки для кнопки, обработчик которой вызовет функцию
 * hyscan_gtk_waterfall_layer_grab_input (см waterfall-plus в тестах)
 *
 * ## Разработка новых слоёв ##
 *
 * Данный раздел посвящен разработке новых слоёв. Если соблюдать ряд правил,
 * новые слои не будут конфликтовать с существующими и предоставлять потребителям
 * схожий API и одинаковый опыт пользователям.
 *
 * ### Общие замечания ###
 *
 * Если слой что-то отображает, то рекомендуется иметь хотя бы следующий набор
 * методов для настройки цвета:
 * - new_layer_set_shadow_color - цвет подложки для рисования под элементами и текстом
 * - new_layer_set_main_color - основной цвет для элементов и текста
 * - new_layer_set_frame_color - цвет окантовки вокруг подложки под текстом
 *
 * Однако это всего лишь совет, а не требование.
 *
 * Ниже по тексту встречается понятие "хэндл". Это некий элемент интерфейса,
 * за который пользователь может "схватиться" и что-то поменять.
 *
 * Подключение к сигналам мыши и клавиатуры должно производиться ТОЛЬКО с помощью
 * g_signal_connect_after!
 *
 * Чтобы слои могли понимать, кто имеет право обработать ввод, в класс HyScanGtkWaterfallState
 * введены следующие функции:
 *
 * ### changes_allowed ###
 * Это первое разграничение, самое глобальное. Оно означает режим просмотра или
 * режим редактирования. В режиме просмотра слои не имеют права обрабатывать
 * никакой пользовательский ввод.
 * - #hyscan_gtk_waterfall_state_set_changes_allowed - разрешает или запрещает изменения
 * - #hyscan_gtk_waterfall_state_get_changes_allowed - позволяет определнить, разрешены ли изменения
 *
 * ### handle_grabbed ###
 * Это второе разграничение. Если это значение отлично от NULL, значит, кто-то
 * уже обрабатывает пользовательский ввод: создает метку, передвигает линейку и т.д.
 * Это запрещает обработку ввода всем слоям кроме собственно владельца хэндла.
 * - #hyscan_gtk_waterfall_state_set_handle_grabbed - захват хэндла
 * - #hyscan_gtk_waterfall_state_get_handle_grabbed - возвращает владельца хэндла
 *
 * ### input_owner ###
 * Последнее разграничение. Если хэндл никем не захвачен, то владелец ввода может
 * начать создание нового объекта.
 * - #hyscan_gtk_waterfall_state_set_input_owner - захват ввода
 * - #hyscan_gtk_waterfall_state_get_input_owner - возвращает владельца ввода
 *
 * ### Захват хэндлов ###
 * Сигнал "handle" класса HyScanGtkWaterfallState жизненно необходим всем желающим
 * реагировать на действия пользователя. Он эмиттируется тогда, когда пользователь
 * отпускает кнопку мыши и никто не держит хэндл. В обработчике сигнала можно
 * определить, есть ли под указателем хэндл и вернуть идентификатор класса
 * (указатель на себя) если хэндл найден и NULL в противном случае.
 * Эмиссия сигнала прекращается как только кто-то вернул отличное от NULL значение.
 *
 * \code
 * gconstpointer
 * handle_cb (HyScanGtkWaterfallState *state,
 *            GdkEventButton          *event,
 *            gpointer                *userdata)
 * \endcode
 *
 * Однако как было сказано выше, разделение на слои носит частично искуственный характер, а потому есть
 * ряд специальных случаев.
 * 1. Слой управления водопадом (HyScanGtkWaterfallControl) имеет право ввода \b всегда. <br>
 *    так сделано для того, чтобы пользователь всегда имел возможность подвинуть область отображения.
 *     Достигается это тем, что WaterfallControl при захвате ввода передает NULL.
 *
 * Пока что всё.
 *
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
  void         (*set_visible)  (HyScanGtkWaterfallLayer *layer,
                                gboolean                 visible);
  const gchar *(*get_mnemonic) (HyScanGtkWaterfallLayer *layer);
};

HYSCAN_API
GType           hyscan_gtk_waterfall_layer_get_type     (void);

/**
 * Функция захватывает ввод.
 *
 * \param layer указатель на объект, имплементирующий \link HyScanGtkWaterfallLayer \endlink
 */
HYSCAN_API
void            hyscan_gtk_waterfall_layer_grab_input   (HyScanGtkWaterfallLayer *layer);

/**
 * Функция позволяет полностью скрыть слой.
 *
 * \param layer указатель на объект, имплементирующий \link HyScanGtkWaterfallLayer \endlink
 * \param visible TRUE для показа слоя, FALSE для скрытия
 */
HYSCAN_API
void            hyscan_gtk_waterfall_layer_set_visible (HyScanGtkWaterfallLayer *layer,
                                                        gboolean                 visible);

/**
 * Функция возвращает название иконки для слоя.
 *
 * \param layer указатель на объект, имплементирующий \link HyScanGtkWaterfallLayer \endlink
 *
 * \return название иконки.
 */
HYSCAN_API
const gchar *   hyscan_gtk_waterfall_layer_get_mnemonic (HyScanGtkWaterfallLayer *layer);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_LAYER_H__ */
