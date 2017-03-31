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
 * а потому не представляет пользы.
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
 * Класс не является потокобезопасным.
 *
 * Однако дочерние классы организованы таким образом, что
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
 * по нажатию на клавиши home, end, PgUp и PgDown.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_H__
#define __HYSCAN_GTK_WATERFALL_H__

#include <gtk-cifro-area.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-tile-common.h>
#include <hyscan-depth.h>

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

  void (*open)                                                 (HyScanGtkWaterfall *waterfall,
                                                                HyScanDB           *db,
                                                                const gchar        *project,
                                                                const gchar        *track,
                                                                gboolean            raw);
  void (*close)                                                (HyScanGtkWaterfall *waterfall);

};

GType                   hyscan_gtk_waterfall_get_type          (void);

HYSCAN_API
void                    hyscan_gtk_waterfall_echosounder       (HyScanGtkWaterfall *waterfall,
                                                                HyScanSourceType    source);
HYSCAN_API
void                    hyscan_gtk_waterfall_sidescan          (HyScanGtkWaterfall *waterfall,
                                                                HyScanSourceType    left,
                                                                HyScanSourceType    right);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_cache         (HyScanGtkWaterfall *waterfall,
                                                                HyScanCache        *cache,
                                                                HyScanCache        *cache2,
                                                                const gchar        *prefix);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_tile_type     (HyScanGtkWaterfall *waterfall,
                                                                HyScanTileType      type);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_ship_speed    (HyScanGtkWaterfall  *waterfall,
                                                                gfloat               ship_speed);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_sound_velocity(HyScanGtkWaterfall  *waterfall,
                                                                GArray              *velocity);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_source  (HyScanGtkWaterfall  *waterfall,
                                                                HyScanSourceType     source,
                                                                guint                channel);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_filter_size (HyScanGtkWaterfall *waterfall,
                                                                    guint               size);
HYSCAN_API
void                    hyscan_gtk_waterfall_set_depth_time    (HyScanGtkWaterfall  *waterfall,
                                                                gulong               usecs);
HYSCAN_API
void                    hyscan_gtk_waterfall_open              (HyScanGtkWaterfall *waterfall,
                                                                HyScanDB           *db,
                                                                const gchar        *project,
                                                                const gchar        *track,
                                                                gboolean            raw);
HYSCAN_API
void                    hyscan_gtk_waterfall_close             (HyScanGtkWaterfall *waterfall);

HYSCAN_API
void                    hyscan_gtk_waterfall_zoom              (HyScanGtkWaterfall *waterfall,
                                                                gboolean            zoom_in);
HYSCAN_API
void                    hyscan_gtk_waterfall_wheel_behaviour   (HyScanGtkWaterfall *waterfall,
                                                                gboolean            scroll);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
