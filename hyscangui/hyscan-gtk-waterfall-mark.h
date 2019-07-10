/**
 * \file hyscan-gtk-waterfall-state.h
 *
 * \brief Виджет "водопад"
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallState HyScanGtkWaterfallState - хранение параметров водопада
 *
 * Данный класс решает две задачи. Первая - хранение основных параметров водопада.
 * Вторая - хранения прав доступа к пользовательским воздействиям.
 *
 * Хранение параметров водопада.
 * С помощью функций-сеттеров устанавливаются параметры и в этот же момент
 * эмиттируется детализированный сигнал "changed".
 * \code
 * void
 * changed_cb (HyScanGtkWaterfallState *state,
 *             gpointer                 userdata);
 *
 * \endcode
 *
 * Сигнал "handle" интересен только разработчикам новых слоёв и более подробно
 * описан в документации к интерфейсу \link HyScanGtkWaterfallLayer \endlink
 * \code
 * gconstpointer
 * handle_cb (HyScanGtkWaterfallState *state,
 *            GdkEventButton          *event,
 *            gpointer                *userdata)
 * \endcode
 *
 * - Тип тайла (наклонная / горизонтальная дальность):
 *     \link hyscan_gtk_waterfall_state_set_tile_type сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_tile_type геттер, \endlink
 *     сигнал "changed::tile-type"
 * - Система кэширования:
 *     \link hyscan_gtk_waterfall_state_set_cache сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_cache геттер, \endlink
 *     сигнал "changed::profile"
 * - Текущие БД, проект и галс:
 *     \link hyscan_gtk_waterfall_state_set_track сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_track геттер, \endlink
 *     сигнал "changed::track"
 * - Скорость судна:
 *     \link hyscan_gtk_waterfall_state_set_ship_speed сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_ship_speed геттер, \endlink
 *     сигнал "changed::speed"
 * - Профиль скорости звука:
 *     \link hyscan_gtk_waterfall_state_set_sound_velocity сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_sound_velocity геттер, \endlink
 *     сигнал "changed::velocity"
 * - Источник данных глубины:
 *     \link hyscan_gtk_waterfall_state_set_depth_source сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_depth_source геттер, \endlink
 *     сигнал "changed::depth-source"
 * - Окно валидности данных глубины:
 *     \link hyscan_gtk_waterfall_state_set_depth_time сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_depth_time геттер, \endlink
 *     сигнал "changed::depth-params"
 * - Размер фильтра для глубины:
 *     \link hyscan_gtk_waterfall_state_set_depth_filter_size сеттер, \endlink
 *     \link hyscan_gtk_waterfall_state_get_depth_filter_size геттер, \endlink
 *     сигнал "changed::cache"
 *
 * Отдельно стоит функция задания типа отображения (водопад и эхолот):
 * - #hyscan_gtk_waterfall_state_echosounder задает режим отображения "эхолот"
 *   (один борт, более поздние данные справа)
 * - #hyscan_gtk_waterfall_state_sidescan задает режим отображения "ГБО"
 *   (два борта, более поздние данные сверху)
 * - #hyscan_gtk_waterfall_state_get_sources возвращает режим отображения и
 *   выбранные источники.
 * Сигнал - "changed::sources"
 *
 * Разграничение доступа.
 *
 * Некоторые слои (см. HyScanGtkWaterfallLayer) могут уметь взаимодействовать
 * с пользователем. Поскольку пользовательский опыт унифицирован, на одно и то же
 * воздействие (щелчок кнопкой мыши, нажатие кнопки) могут отреагировать несколько
 * слоёв сразу. А потому введены понятия "владелец ввода" и "держатель хэндла (ручки)".
 * Программисту, встраивающему водопад в свою программу, эти методы не интересны.
 * Для разработки новых слоев надо обратиться к документации HyScanGtkWaterfallLayer,
 * где и описано разграничение доступа.
 *
 * - #hyscan_gtk_waterfall_state_set_input_owner
 * - #hyscan_gtk_waterfall_state_get_input_owner
 * - #hyscan_gtk_waterfall_state_set_handle_grabbed
 * - #hyscan_gtk_waterfall_state_get_handle_grabbed
 * - #hyscan_gtk_waterfall_state_set_changes_allowed
 * - #hyscan_gtk_waterfall_state_get_changes_allowed
 *
 * Класс не является потокобезопасным и предназначен для использования в главном потоке.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_MARK_H__
#define __HYSCAN_GTK_WATERFALL_MARK_H__

#include "hyscan-gtk-layer.h"
#include <hyscan-mark-model.h>

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
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallMarkPrivate *priv;
};

struct _HyScanGtkWaterfallMarkClass
{
  GInitiallyUnownedClass parent_class;
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
HyScanGtkWaterfallMark  *hyscan_gtk_waterfall_mark_new                 (HyScanMarkModel            *markmodel);

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
