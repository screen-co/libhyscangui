/**
 *
 * \file hyscan-tile-manager.h
 *
 * \brief Менеджер тайлов.
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanTileManager HyScanTileManager - менеджер тайлов.
 *
 * HyScanTileManager - менеджер тайлов.
 *
 * Задача этого класса - реализация логики поиска и сравнения тайлов и заполнения cairo_surface_t.
 *
 * Объект создается методом #hyscan_tile_manager_new. Объект не будет работать до
 * тех пор, пока не будут вызываны функции #hyscan_tile_manager_set_cache и
 * #hyscan_tile_manager_open.
 *
 * Всякий раз в начале обработчика события перерисовки видимой области
 * необходимо вызывать #hyscan_tile_manager_view_start, чтобы установить новый view_id,
 * потом вызвать #hyscan_tile_manager_get_tile для каждого тайла, а в конце вызывать
 * #hyscan_tile_manager_view_end. При вызове #hyscan_tile_manager_get_tile класс сам
 * определит, какой тайл и откуда взять и отдаст его на генерацию (при необходимости),
 * при этом в этот момент фактическая генерация не начинается.
 * При вызове #hyscan_tile_manager_view_end тайлы действительно начинают генерироваться.
 *
 * Поскольку класс внутри себя содержит объекты
 * \link HyScanTileColor\endlink и \link HyScanTileQueue\endlink, предоставлены интерфейсы для
 * настройки параметров генерации тайлов: функции #hyscan_tile_manager_set_colormap, #hyscan_tile_manager_set_levels и
 * #hyscan_tile_manager_update_tilequeue.
 *
 * - #hyscan_tile_manager_new - создает новый объект \link HyScanTileManager \endlink;
 * - #hyscan_tile_manager_open - открывает БД, проект, галс;
 * - #hyscan_tile_manager_set_cache - устанавливает кэш;
 * - #hyscan_tile_manager_view_start - начало view;
 * - #hyscan_tile_manager_get_tile - отдает тайл;
 * - #hyscan_tile_manager_view_end - конец view;
 * - #hyscan_tile_manager_set_colormap - обновление параметров \link HyScanTileColor \endlink;
 * - #hyscan_tile_manager_set_levels - обновление параметров \link HyScanTileColor \endlink;
 * - #hyscan_tile_manager_update_tilequeue - обновление параметров \link HyScanTileQueue \endlink;
 hyscan_tile_manager_new
 hyscan_tile_manager_set_cache
 hyscan_tile_manager_setup_depth
 hyscan_tile_manager_set_speeds
 hyscan_tile_manager_update_tilequeue
 hyscan_tile_manager_open
 hyscan_tile_manager_close
 hyscan_tile_manager_view_start
 hyscan_tile_manager_get_tile
 hyscan_tile_manager_view_end
 hyscan_tile_manager_set_colormap
 hyscan_tile_manager_set_levels
 hyscan_tile_manager_track_params
 */

#ifndef __HYSCAN_TILE_MANAGER_H__
#define __HYSCAN_TILE_MANAGER_H__

#include <gtk/gtk.h>
#include <hyscan-cache.h>
#include <hyscan-depth.h>
#include <hyscan-tile-common.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TILE_MANAGER             (hyscan_tile_manager_get_type ())
#define HYSCAN_TILE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TILE_MANAGER, HyScanTileManager))
#define HYSCAN_IS_TILE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TILE_MANAGER))
#define HYSCAN_TILE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TILE_MANAGER, HyScanTileManagerClass))
#define HYSCAN_IS_TILE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TILE_MANAGER))
#define HYSCAN_TILE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TILE_MANAGER, HyScanTileManagerClass))

typedef struct _HyScanTileManager HyScanTileManager;
typedef struct _HyScanTileManagerPrivate HyScanTileManagerPrivate;
typedef struct _HyScanTileManagerClass HyScanTileManagerClass;

struct _HyScanTileManager
{
  GObject parent_instance;

  HyScanTileManagerPrivate *priv;
};

struct _HyScanTileManagerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_tile_manager_get_type          (void);

/**
 *
 * Функция создает новый объект \link HyScanTileManager \endlink.
 *
 * \return указатель на объект \link HyScanTileManager \endlink
 */
HYSCAN_API
HyScanTileManager      *hyscan_tile_manager_new               (void);

/**
 *
 * Функция позволяет установить систему кэширования.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param cache - указатель на объект \link HyScanCache \endlink;
 * \param cache2 - указатель на объект \link HyScanCache \endlink;
 * \param cache_prefix - префикс для системы кэширования;
 *
 */
HYSCAN_API
void                    hyscan_tile_manager_set_cache         (HyScanTileManager      *tilemgr,
                                                               HyScanCache            *cache,
                                                               HyScanCache            *cache2,
                                                               const gchar            *cache_prefix);

HYSCAN_API
void                    hyscan_tile_manager_setup_depth       (HyScanTileManager      *tilemgr,
                                                               HyScanDepthSource       source,
                                                               gint                    size,
                                                               gulong                  microseconds);

/**
 *
 * Функция устанавливает скорость звука и судна.
 *
 * Возмонжно установить только одну из скоростей, передав нулевое или
 * отрицательное значение другой.
 *
 * \param tilemgr - указатель на \link HyScanTileManager \endlink;
 * \param ship_speed - скорость движения;
 * \param sound_speed - скорость звука;
 *
 * \return TRUE, всегда.
 */
HYSCAN_API
void                    hyscan_tile_manager_set_speeds        (HyScanTileManager      *tilemgr,
                                                               gfloat                  ship_speed,
                                                               GArray                 *sound_speed);
/**
 *
 * Функция позволяет открыть или переоткрыть БД, проект, галс.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param db - указатель на объект \link HyScanDB \endlink;
 * \param project_name - имя проекта;
 * \param track_name - имя галса;
 * \param location - указатель на объект \link HyScanLocation \endlink;
 * \param preferred - предпочитаемый тип данных;
 *
 */

/**
 *
 * Функция обновляет параметры генерации тайла.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param type - тип генерируемого тайла (см \link HyScanTileCommon \endlink);
 * \param upsample - желаемая величина передискретизации;
 *
 * \return TRUE, если параметры успешно установлены.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_update_tilequeue  (HyScanTileManager      *tilemgr,
                                                               HyScanTileType          type,
                                                               guint                   upsample);

/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 * Фактически, это обертка над \link hyscan_tile_color_set_colormap \endlink.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param сolormap - массив значений цветов точек;
 * \param levels - число градаций;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_set_colormap      (HyScanTileManager      *tilemgr,
                                                               guint32                *colormap,
                                                               gint                    levels,
                                                               guint32                 background);

/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 * Это обертка над \link hyscan_tile_color_set_levels \endlink.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_set_levels        (HyScanTileManager      *tilemgr,
                                                               gdouble                 black,
                                                               gdouble                 gamma,
                                                               gdouble                 white);

HYSCAN_API
void                    hyscan_tile_manager_open              (HyScanTileManager      *tilemgr,
                                                               HyScanDB               *db,
                                                               const gchar            *project_name,
                                                               const gchar            *track_name,
                                                               HyScanTileSource        source,
                                                               gboolean                raw);

HYSCAN_API
void                    hyscan_tile_manager_close            (HyScanTileManager      *tilemgr);



/**
 *
 * Функция начинает новый view.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 *
 * \return TRUE, всегда.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_view_start        (HyScanTileManager      *tilemgr);

/**
 *
 * Функция позволяет получить тайл из кэша.
 *
 * При этом функция сама разберется, откуда взять наиболее актуальный тайл и требуется
 * ли перегенерировать его. Если функция вернула FALSE, то нужно отрисовать заглушку.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink;
 * \param tile - запрашиваемый тайл;
 * \param tile_surface - cairo_surface_t, в которой нужно отрисовать тайл;
 *
 * \return TRUE, если тайл найден. FALSE, если не найден.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_get_tile          (HyScanTileManager      *tilemgr,
                                                               HyScanTile             *tile,
                                                               cairo_surface_t       **tile_surface);

/**
 *
 * Функция завершает view и отправляет тайлы на генерацию.
 *
 * \param tilemgr - указатель на объект \link HyScanTileManager \endlink.
 *
 * \return TRUE, если тайлы успешно отправлены на генерацию.
 */
HYSCAN_API
gboolean                hyscan_tile_manager_view_end          (HyScanTileManager      *tilemgr);

HYSCAN_API
gboolean                hyscan_tile_manager_track_params     (HyScanTileManager      *tilemgr,
                                                               gdouble                *lwidth,
                                                               gdouble                *rwidth,
                                                               gdouble                *length);



G_END_DECLS

#endif /* __HYSCAN_TILE_MANAGER_H__ */
