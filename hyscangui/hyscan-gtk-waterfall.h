/**
 * \file hyscan-gtk-waterfall.h
 *
 * \brief Виджет "водопад".
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfall HyScanGtkWaterfall - виджет "водопад".
 *
 * Данный виджет является родительским для всех виджетов, которые требуются для
 * отображения данных в режиме "водопад".
 * В нём реализуется логика установки пользователем и хранения параметров водопада,
 * а также управление прокруткой и зуммированием с помощью клавиатуры и мыши.
 *
 * Метод _new отсутствует, так как сам по себе этот виджет ничего не отрисовывает,
 * а потому не представляет пользы для пользователя.
 *
 * Все методы делятся на четыре категории:
 *
 * 1. Управление типом отображения:
 * - #hyscan_gtk_waterfall_echosounder - отображение данных эхолота;
 * - #hyscan_gtk_waterfall_sidescan - отображение данных ГБО;
 *
 * 2. Установка параметров обработки, системы кэширования:
 * - #hyscan_gtk_waterfall_set_cache - установка системы кэширования;
 * - #hyscan_gtk_waterfall_set_tile_type - установка типа тайла;
 * - #hyscan_gtk_waterfall_set_ship_speed - установка скорости судна;
 * - #hyscan_gtk_waterfall_set_sound_velocity - установка скорости звука;
 * - #hyscan_gtk_waterfall_set_depth_source - настройка \link HyScanDepthometer \endlink;
 * - #hyscan_gtk_waterfall_set_depth_filter_size - настройка \link HyScanDepthometer \endlink;
 * - #hyscan_gtk_waterfall_set_depth_time - настройка \link HyScanDepthometer \endlink;
 *
 * 3. API для зуммирования и настройки поведения колеса мыши (прокрутка/зум):
 * - #hyscan_gtk_waterfall_zoom - программное зуммирование;
 * - #hyscan_gtk_waterfall_wheel_behaviour - настройка поведения колеса мыши;
 *
 * 4. Открытие и закрытие галса:
 * - #hyscan_gtk_waterfall_open - открывает БД, проект и галс;
 * - #hyscan_gtk_waterfall_close - закрывает БД, проект и галс;
 *
 * Помимо этого есть две виртуальные функции (open и close), вызывающиеся, соответственно,
 * в методах hyscan_gtk_waterfall_open и hyscan_gtk_waterfall_close.
 *
 * Класс не является потокобезопасным. Однако дочерние классы организованы таким образом, что
 * считывание установленных параметров происходит только во внутренних переопределенных
 * методах _open и _close. Таким образом, установка параметров после вызова метода _open
 * приведет к изменениям только после повторного вызова метода _open.
 *
 * Функция hyscan_gtk_waterfall_open помимо вызова виртуальной функции open вызывает и close,
 * поэтому во-первых, в дочерних классах не обязательно вызывать close в обработчике open,
 * а во-вторых, при необходимости открыть галс без смены параметров отображения можно
 * не вызывать метод hyscan_gtk_waterfall_close.
 *
 * На уровне этого класса режимы ГБО и эхолот отличаются только направлением сдвижки
 * при нажатии на клавиатуру или прокрутке мышью.
 *
 * В классе определено два сигнала. Они являются чисто внутренними и не представляют
 * интереса для пользователя, однако могут быть полезны для программиста.
 * Важно понимать, что эти сигналы отражают только намерения пользователя совершить какие-то
 * действия. Видимая область может сдвигаться или зуммироваться из нижележащих классов,
 * тогда сигналы эмиттированы не будут.
 * - Сигнал "waterfall-user-zoom" эмиттируется, когда класс собирается вызвать функцию gtk_cifro_area_zoom ().
 *   При этом передается направление зуммирования.
 * - Сигнал "waterfall-user-move" эмиттируется, когда класс собирается сдвинуть видимую область.
 *   Передается величина сдвижки по вертикальной и горизонтальной оси.
 *
 * \code
 * void waterfall_zoom_cb (HyScanGtkWaterfall   *wfall,
 *                         GtkCifroAreaZoomType  zoom);
 *
 * void waterfall_move_cb (HyScanGtkWaterfall   *wfall,
 *                         gdouble               vertical,
 *                         gdouble               horisontal);
 * \endcode
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_H__
#define __HYSCAN_GTK_WATERFALL_H__

#include <gtk-cifro-area.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-tile-common.h>
#include <hyscan-depth.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

/** \brief Типы тайлов по источнику данных. */
typedef enum
{
  HYSCAN_GTK_WATERFALL_SIDESCAN      = 100,   /**< ГБО. */
  HYSCAN_GTK_WATERFALL_ECHOSOUNDER   = 101,   /**< Эхолот. */
} HyScanGtkWaterfallType;

#define HYSCAN_TYPE_GTK_WATERFALL             (hyscan_gtk_waterfall_get_type ())
#define HYSCAN_GTK_WATERFALL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfall))
#define HYSCAN_IS_GTK_WATERFALL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))
#define HYSCAN_IS_GTK_WATERFALL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))

typedef struct _HyScanGtkWaterfall HyScanGtkWaterfall;
typedef struct _HyScanGtkWaterfallPrivate HyScanGtkWaterfallPrivate;
typedef struct _HyScanGtkWaterfallClass HyScanGtkWaterfallClass;

struct _HyScanGtkWaterfall
{
  GtkCifroArea parent_instance;

  HyScanGtkWaterfallPrivate *priv;
};

struct _HyScanGtkWaterfallClass
{
  GtkCifroAreaClass parent_class;

  void (*open)   (HyScanGtkWaterfall *waterfall,
                  HyScanDB           *db,
                  const gchar        *project,
                  const gchar        *track,
                  gboolean            raw);
  void (*close)  (HyScanGtkWaterfall *waterfall);

};

HYSCAN_API
GType                   hyscan_gtk_waterfall_get_type          (void);

/**
 *
 * Функция устанавливает режим отображения "эхолот" (один борт, акустические строки
 * направлены вниз, свежие данные справа).
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param source тип КД.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_echosounder       (HyScanGtkWaterfall *waterfall,
                                                                HyScanSourceType    source);
/**
 *
 * Функция устанавливает режим отображения "ГБО" (два борта, ак. строки направлены
 * вправо и влево, свежие данные сверху).
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param left тип КД левого борта
 * \param right тип КД правого борта
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_sidescan          (HyScanGtkWaterfall *waterfall,
                                                                HyScanSourceType    left,
                                                                HyScanSourceType    right);
/**
 *
 * Функция устанавливает систему кэширования.
 * Поддерживается две системы кэширования. Во вторую складываются только разукрашенные
 * тайлы, в первую всё остальное.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param cache указатель на интерфейс \link HyScanCache \endlink
 * \param cache2 указатель на интерфейс \link HyScanCache \endlink (NULL или cache для
 * использования одного кэша)
 * \param prefix префикс систем кэширования
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_cache         (HyScanGtkWaterfall *waterfall,
                                                                HyScanCache        *cache,
                                                                HyScanCache        *cache2,
                                                                const gchar        *prefix);
/**
 *
 * Функция устанавливает тип отображения: наклонная или горизонтальная дальность.
 * Эта настройка работает только для отображения ГБО (#hyscan_gtk_waterfall_sidescan).
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param type
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_tile_type     (HyScanGtkWaterfall *waterfall,
                                                                HyScanTileType      type);
/**
 *
 * Функция устанавливает скорость движения судна.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param ship_speed скорость судна в м/с
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_ship_speed    (HyScanGtkWaterfall  *waterfall,
                                                                gfloat               ship_speed);
/**
 *
 * Функция устанавливает профиль скорости звука.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param velocity профиль скорости звука, см \link HyScanSoundVelocity \endlink
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_sound_velocity(HyScanGtkWaterfall  *waterfall,
                                                                GArray              *velocity);
/**
 *
 * Функция устанавливает тип и номер канала для определения глубины.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param source тип КД
 * \param channel номер канала
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_source  (HyScanGtkWaterfall  *waterfall,
                                                                HyScanSourceType     source,
                                                                guint                channel);
/**
 *
 * Функция устанавливает размер фильтра для определения глубины.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param size размер фильтра
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_filter_size (HyScanGtkWaterfall *waterfall,
                                                                    guint               size);
/**
 *
 * Функция устанавливает окно валидности данных для глубины.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param usecs окно валидности в микросекундах
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_time    (HyScanGtkWaterfall  *waterfall,
                                                                gulong               usecs);
/**
 *
 * Функция отрывает БД, проект и галс.
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param db указатель на интерфейс \link HyScanDB \endlink
 * \param project название проекта
 * \param track название галса
 * \param raw использовать ли сырые данные
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_open              (HyScanGtkWaterfall *waterfall,
                                                                HyScanDB           *db,
                                                                const gchar        *project,
                                                                const gchar        *track,
                                                                gboolean            raw);
/**
 *
 * Функция закрывает БД, проект и галс
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_close             (HyScanGtkWaterfall *waterfall);

/**
 *
 * Функция зуммирует изображение
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param zoom_in TRUE для приближения, FALSE для отдаления
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_zoom              (HyScanGtkWaterfall *waterfall,
                                                                gboolean            zoom_in);
/**
 *
 * По многочисленным просьбам трудящихся функция настраивает поведения колесика мыши.
 *
 * Действие        |Scroll = TRUE |Scroll = FALSE
 * :--------------:|:------------:|:------------:
 * Прокрутка       | Прокрутка    | Зум
 * CTRL+прокрутка  | Зум          | Прокрутка
 *
 * \param waterfall указатель на объект \link HyScanGtkWaterfall \endlink
 * \param scroll поведение колесика мыши
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_wheel_behaviour   (HyScanGtkWaterfall *waterfall,
                                                                gboolean            scroll);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
