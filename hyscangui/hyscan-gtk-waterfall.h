/**
 *
 * \file hyscan-gtk-waterfall.h
 *
 * \brief Виджет "водопад".
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfall HyScanGtkWaterfall - виджет "водопад".
 *
 * Виджет создается методом #hyscan_gtk_waterfall_new. Виджет не будет работать до
 * тех пор, пока не будет вызывана функция #hyscan_gtk_waterfall_open. Такая логика
 * нужна для того, чтобы не пересоздавать виджет при любых изменениях.
 *
 * Виджет обрабатывает сигнал "visible-draw" от \link GtkCifroArea \endlink.
 * В этом обработчике он разбивает видимую область на тайлы и делает запрос к
 * \link HyScanTileManager \endlink. Помимо этого виджет обрабатывает GdkEventConfigure,
 * для определения PPI экрана.
 *
 * Виджет поддерживает работу в режиме водопад-онлайн, когда наиболее актуальные
 * данные находятся вверху экрана и автоматически сдвигаются вниз. При этом если конечный
 * пользователь сдвинет мышкой изображение, режим автосдвижки выключится и будет
 * эмиттирован сигнал "automove-state".
 * Обработчик этого сигнала должен выглядеть следюущим образом:
 *
 * \code
 * void automove_state_cb (HyScanGtkWaterfall *waterfall,
 *                         gboolean            state);
 * \endcode
 *
 * - #hyscan_gtk_waterfall_new - создание объекта;
 * - #hyscan_gtk_waterfall_open - открытие галса;
 * - #hyscan_gtk_waterfall_set_cache - установка кэша;
 * - #hyscan_gtk_waterfall_set_refresh_time - установка времени обновления экрана;
 * - #hyscan_gtk_waterfall_set_fps - установка количества кадров в секунду;
 * - #hyscan_gtk_waterfall_automove - включение и отключение автосдвижки;
 * - #hyscan_gtk_waterfall_set_colormap - установка цветовой схемы;
 * - #hyscan_gtk_waterfall_set_levels - установка уровней;
 * - #hyscan_gtk_waterfall_update_tile_param - установка параметров тайлов;
 * - #hyscan_gtk_waterfall_get_scale - текущий масштаб;
 * - #hyscan_gtk_waterfall_zoom - зуммирование;
 * - #hyscan_gtk_waterfall_track_params - параметры галса;
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_H__
#define __HYSCAN_GTK_WATERFALL_H__

#include <gtk/gtk.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <gtk-cifro-area.h>
#include <hyscan-tile-common.h>
#include <hyscan-depth.h>

G_BEGIN_DECLS

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
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_get_type         (void);

/**
 *
 * Функция создает новый виджет \link HyScanGtkWaterfall \endlink
 *
 */
GtkWidget              *hyscan_gtk_waterfall_new              (void);

/**
 *
 * Функция позволяет установить систему кэширования.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param cache - указатель на объект \link HyScanCache \endlink;
 * \param cache2 - указатель на объект \link HyScanCache \endlink;
 * \param cache_prefix - префикс для системы кэширования;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_cache        (HyScanGtkWaterfall *waterfall,
                                                               HyScanCache        *cache,
                                                               HyScanCache        *cache2,
                                                               const gchar        *cache_prefix);

HYSCAN_API
void                    hyscan_gtk_waterfall_set_speeds       (HyScanGtkWaterfall *waterfall,
                                                               gfloat                  ship_speed,
                                                               GArray                 *sound_speed);

HYSCAN_API
void                    hyscan_gtk_waterfall_setup_depth      (HyScanGtkWaterfall *waterfall,
                                                               HyScanDepthSource       source,
                                                               gint                    size,
                                                               gulong                  microseconds);
/**
 *
 * Функция обновляет параметры генерации тайла.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param type - тип генерируемого тайла (см \link HyScanTileCommon \endlink);
 * \param upsample - желаемая величина передискретизации;
 *
 * \return TRUE, если параметры успешно установлены.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_update_tile_param (HyScanGtkWaterfall *waterfall,
                                                                HyScanTileType      type,
                                                                gint                upsample);

/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 * Фактически, это обертка над \link hyscan_tile_color_set_colormap \endlink.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param сolormap - массив значений цветов точек;
 * \param length - размер массива;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_colormap     (HyScanGtkWaterfall *waterfall,
                                                               guint32            *colormap,
                                                               gint                length,
                                                               guint32             background);

/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 * Это обертка над \link hyscan_tile_color_set_levels \endlink.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_levels       (HyScanGtkWaterfall *waterfall,
                                                               gdouble             black,
                                                               gdouble             gamma,
                                                               gdouble             white);

/**
 *
 * Функция позволяет открыть или переоткрыть БД, проект, галс.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param db - указатель на объект \link HyScanDB \endlink;
 * \param project_name - имя проекта;
 * \param track_name - имя галса;
 * \param ship_speed - скорость движения;
 * \param sound_speed - скорость звука;
 * \param raw - использовать сырые данные;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_open             (HyScanGtkWaterfall *waterfall,
                                                               HyScanDB           *db,
                                                               const gchar        *project_name,
                                                               const gchar        *track_name,
                                                               HyScanTileSource    source,
                                                               gboolean            raw);

HYSCAN_API
void                    hyscan_gtk_waterfall_close            (HyScanGtkWaterfall *waterfall);
/**
 *
 * Внутри виджета есть функция, которая проверяет, сгенерирован ли новый тайл.
 * Эта функция позволяет установить интервал опроса. Фактически, это время между
 * сигналом готовности тайла и его появлением на экране. Увеличение интервала
 * приведет к тому, что тайлы будут отрисовываться позднее, но может уменьшиться
 * нагрузка на процессор.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param interval - интервал опроса.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_refresh_time (HyScanGtkWaterfall *waterfall,
                                                               guint               interval);
// /**
//  *
//  * Функция позволяет установить частоту кадров.
//  * Данная функция влияет на плавность в режиме автосдвижки.
//  *
//  * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
//  * \param fps - количество кадров в секунду.
//  *
//  */
// HYSCAN_API
// void                    hyscan_gtk_waterfall_set_fps          (HyScanGtkWaterfall *waterfall,
//                                                                guint               fps);
//
// /**
//  *
//  * Функция включает режим автосдвига данных.
//  *
//  * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
//  * \param on - TRUE: вкл, FALSE: выкл.
//  *
//  */
// HYSCAN_API
// void                    hyscan_gtk_waterfall_automove         (HyScanGtkWaterfall *waterfall,
//                                                                gboolean            on);
//
/**
 *
 * Функция возвращает масштабы и индекс текущего масштаба.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param zooms - указатель на массив масштабов.
 * \param num - число масштабов.
 *
 * \return номер масштаба; -1 в случае ошибки.
 */
HYSCAN_API
gint                    hyscan_gtk_waterfall_get_scale         (HyScanGtkWaterfall *waterfall,
                                                                const gdouble     **zooms,
                                                                gint               *num);

/**
 *
 * Функция зуммирует изображение.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param zoom_in - TRUE, чтобы приблизить, FALSE, чтобы отдалить.
 *
 * \return номер масштаба; -1 в случае ошибки.
 */
HYSCAN_API
gint                    hyscan_gtk_waterfall_zoom              (HyScanGtkWaterfall *waterfall,
                                                                gboolean            zoom_in);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
