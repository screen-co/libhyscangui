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

#ifndef __HYSCAN_GTK_WATERFALL_STATE_H__
#define __HYSCAN_GTK_WATERFALL_STATE_H__

#include <hyscan-db.h>
#include <hyscan-tile-common.h>
#include <hyscan-amplitude-factory.h>
#include <hyscan-depth-factory.h>
#include <gtk-cifro-area.h>

G_BEGIN_DECLS

/** \brief Типы отображения */
typedef enum
{
  HYSCAN_WATERFALL_DISPLAY_SIDESCAN      = 100,   /**< ГБО. */
  HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER   = 101,   /**< Эхолот. */
} HyScanWaterfallDisplayType;

#define HYSCAN_TYPE_GTK_WATERFALL_STATE             (hyscan_gtk_waterfall_state_get_type ())
#define HYSCAN_GTK_WATERFALL_STATE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallState))
#define HYSCAN_IS_GTK_WATERFALL_STATE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE))
#define HYSCAN_GTK_WATERFALL_STATE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallStateClass))
#define HYSCAN_IS_GTK_WATERFALL_STATE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_STATE))
#define HYSCAN_GTK_WATERFALL_STATE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_STATE, HyScanGtkWaterfallStateClass))

typedef struct _HyScanGtkWaterfallState HyScanGtkWaterfallState;
typedef struct _HyScanGtkWaterfallStatePrivate HyScanGtkWaterfallStatePrivate;
typedef struct _HyScanGtkWaterfallStateClass HyScanGtkWaterfallStateClass;

struct _HyScanGtkWaterfallState
{
  GtkCifroArea parent_instance;

  HyScanGtkWaterfallStatePrivate *priv;
};

struct _HyScanGtkWaterfallStateClass
{
  GtkCifroAreaClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_state_get_type                    (void);

/**
 *
 * Функция позволяет слою захватить ввод.
 *
 * Идентификатор может быть абсолютно любым, кроме NULL.
 * Рекомедуется использовать адрес вызывающего эту функцию слоя.
 *
 * \param state указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param instance идентификатор объекта, желающего захватить ввод.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_input_owner             (HyScanGtkWaterfallState *state,
                                                                                gconstpointer            instance);
/**
 *
 * Функция возвращает владельца ввода.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца ввода или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
gconstpointer           hyscan_gtk_waterfall_state_get_input_owner             (HyScanGtkWaterfallState *state);
/**
 *
 * Функция позволяет слою захватить хэндл.
 *
 * Идентификатор может быть абсолютно любым, кроме NULL.
 * Рекомедуется использовать адрес вызывающего эту функцию слоя.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param instance идентификатор объекта, желающего захватить хэндл.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_handle_grabbed          (HyScanGtkWaterfallState *state,
                                                                                gconstpointer            instance);
/**
 *
 * Функция возвращает владельца хэндла.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца хэндла или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
gconstpointer           hyscan_gtk_waterfall_state_get_handle_grabbed          (HyScanGtkWaterfallState *state);

/**
 *
 * Функция позволяет запретить или разрешить изменения.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param allowed TRUE, чтобы разрешить изменения.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_changes_allowed         (HyScanGtkWaterfallState *state,
                                                                                gboolean                 allowed);
/**
 *
 * Функция позволяет узнать, запрещены изменения или нет.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return TRUE, если разрешены, FALSE, если запрещены.
 *
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_state_get_changes_allowed         (HyScanGtkWaterfallState *state);

/**
 *
 * Функция устанавливает режим отображения эхолот.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param source отображаемый тип данных.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_echosounder                 (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType         source);
/**
 *
 * Функция устанавливает режим отображения водопад.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param lsource тип данных, отображаемый слева;
 * \param rsource тип данных, отображаемый справа.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_sidescan                    (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType         lsource,
                                                                                HyScanSourceType         rsource);
/**
 *
 * Функция устанавливает тип тайла (наклонная или горизонтальная дальность).
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param type тип тайла.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_tile_flags              (HyScanGtkWaterfallState *state,
                                                                                HyScanTileFlags          flags);

/**
 *
 * Функция задает БД, проект и галс
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project имя проекта;
 * \param track имя галса;
 * \param raw использовать ли сырые данные.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_track                   (HyScanGtkWaterfallState *state,
                                                                                HyScanDB                *db,
                                                                                const gchar             *project,
                                                                                const gchar             *track);

/**
 *
 * Функция устанавливает скорость судна.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param speed скорость судна в м/с.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_ship_speed              (HyScanGtkWaterfallState *state,
                                                                                gfloat                   speed);
/**
 *
 * Функция устанавливает скорость звука.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param velocity профиль скорости звука.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_sound_velocity          (HyScanGtkWaterfallState *state,
                                                                                GArray                  *velocity);

/**
 *
 * Функция возвращает тип отображения и источники, как противоположность
 * функциям hyscan_gtk_waterfall_state_echosounder и hyscan_gtk_waterfall_state_sidescan.
 * В случае режима эхолот источники справа и слева будут идентичны.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param lsource тип данных слева;
 * \param rsource тип данных справа.
 *
 */
HYSCAN_API
HyScanWaterfallDisplayType  hyscan_gtk_waterfall_state_get_sources             (HyScanGtkWaterfallState *state,
                                                                                HyScanSourceType        *lsource,
                                                                                HyScanSourceType        *rsource);
/**
 *
 * Функция возвращает тип тайла.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param type тип тайла.
 *
 */
HYSCAN_API
HyScanTileFlags         hyscan_gtk_waterfall_state_get_tile_flags              (HyScanGtkWaterfallState *state);

/**
 *
 * Функция возвращает систему кэширования.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param cache указатель на объект \link HyScanCache \endlink;
 *
 */
HYSCAN_API
HyScanAmplitudeFactory * hyscan_gtk_waterfall_state_get_amp_factory            (HyScanGtkWaterfallState *state);

/**
 *
 * Функция возвращает систему кэширования.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param cache указатель на объект \link HyScanCache \endlink;
 *
 */
HYSCAN_API
HyScanDepthFactory *    hyscan_gtk_waterfall_state_get_dpt_factory            (HyScanGtkWaterfallState *state);

/**
 *
 * Функция возвращает систему кэширования.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param cache указатель на объект \link HyScanCache \endlink;
 *
 */
HYSCAN_API
HyScanCache *           hyscan_gtk_waterfall_state_get_cache                   (HyScanGtkWaterfallState *state);
/**
 *
 * Функция возвращает БД, проект и галс.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param db указатель на \link HyScanDB \endlink;
 * \param project имя проекта;
 * \param track имя галса;
 * \param raw использовать ли сырые данные.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_get_track                   (HyScanGtkWaterfallState *state,
                                                                                HyScanDB               **db,
                                                                                gchar                  **project,
                                                                                gchar                  **track);

/**
 *
 * Функция возвращает скорость судна.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param speed скорость судна.
 *
 */
HYSCAN_API
gfloat                  hyscan_gtk_waterfall_state_get_ship_speed              (HyScanGtkWaterfallState *state);
/**
 *
 * Функция возвращает профиль скорости звука.
 *
 * \param state указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param velocity профиль скорости звука.
 *
 */
HYSCAN_API
GArray *                hyscan_gtk_waterfall_state_get_sound_velocity          (HyScanGtkWaterfallState *state);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_STATE_H__ */
