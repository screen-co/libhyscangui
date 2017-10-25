/**
 *
 * Сигналы:
 * - changed::sources
 * - changed::tile-type
 * - changed::profile
 * - changed::track
 * - changed::speed
 * - changed::velocity
 * - changed::depth-source
 * - changed::depth-params
 * - changed::cache
 */
#ifndef __HYSCAN_GTK_WATERFALL_STATE_H__
#define __HYSCAN_GTK_WATERFALL_STATE_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-core-types.h>
#include <hyscan-tile-common.h>
#include <gtk-cifro-area.h>

G_BEGIN_DECLS

/** \brief Типы отображения. */
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
GType                  hyscan_gtk_waterfall_state_get_type                  (void);

HYSCAN_API
HyScanGtkWaterfallState  *hyscan_gtk_waterfall_state_new                       (void);

/**
 *
 * Функция позволяет слою захватить ввод.
 *
 * Идентификатор может быть абсолютно любым, кроме NULL.
 * Рекомедуется использовать адрес вызывающего эту функцию слоя.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param owner - идентификатор объекта, желающего захватить ввод.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_input_owner                 (HyScanGtkWaterfallState *state,
                                                                              gconstpointer       instance);
/**
 *
 * Функция возвращает владельца ввода.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца ввода или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
gconstpointer           hyscan_gtk_waterfall_state_get_input_owner                  (HyScanGtkWaterfallState *state);
/**
 *
 * Функция позволяет слою захватить ввод.
 *
 * Идентификатор может быть абсолютно любым, кроме NULL.
 * Рекомедуется использовать адрес вызывающего эту функцию слоя.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfallState \endlink;
 * \param owner - идентификатор объекта, желающего захватить ввод.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_handle_grabbed                  (HyScanGtkWaterfallState *state,
                                                                                  gconstpointer       instance);
/**
 *
 * Функция возвращает владельца ввода.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца ввода или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
gconstpointer           hyscan_gtk_waterfall_state_get_handle_grabbed                   (HyScanGtkWaterfallState *state);

/**
 *
 * Функция возвращает владельца ввода.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца ввода или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_state_set_changes_allowed                  (HyScanGtkWaterfallState *state,
                                                                                   gboolean            allowed);
/**
 *
 * Функция возвращает владельца ввода.
 *
 * \param state - указатель на объект \link HyScanGtkWaterfallState \endlink;
 *
 * \return идентификатор владельца ввода или NULL, если владелец не установлен.
 *
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_state_get_changes_allowed                  (HyScanGtkWaterfallState *state);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_echosounder               (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      source);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_sidescan                  (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      lsource,
                                                                         HyScanSourceType      rsource);
void                   hyscan_gtk_waterfall_set_tile_type                   (HyScanGtkWaterfallState *state,
                                                                         HyScanTileType        type);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_profile               (HyScanGtkWaterfallState *state,
                                                                         const gchar          *profile);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_cache                 (HyScanGtkWaterfallState *state,
                                                                         HyScanCache          *cache,
                                                                         HyScanCache          *cache2,
                                                                         const gchar          *prefix);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_track                 (HyScanGtkWaterfallState *state,
                                                                         HyScanDB             *db,
                                                                         const gchar          *project,
                                                                         const gchar          *track,
                                                                         gboolean              raw);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_ship_speed            (HyScanGtkWaterfallState *state,
                                                                         gfloat                speed);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_sound_velocity        (HyScanGtkWaterfallState *state,
                                                                         GArray               *velocity);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_depth_source          (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      source,
                                                                         guint                 channel);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_depth_time            (HyScanGtkWaterfallState *state,
                                                                         gulong                usecs);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_set_depth_filter_size     (HyScanGtkWaterfallState *state,
                                                                         guint                 size);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_sources               (HyScanGtkWaterfallState       *state,
                                                                         HyScanWaterfallDisplayType *type,
                                                                         HyScanSourceType           *lsource,
                                                                         HyScanSourceType           *rsource);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_tile_type             (HyScanGtkWaterfallState *state,
                                                                         HyScanTileType       *type);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_profile               (HyScanGtkWaterfallState *state,
                                                                         gchar               **profile);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_cache                 (HyScanGtkWaterfallState *state,
                                                                         HyScanCache         **cache,
                                                                         HyScanCache         **cache2,
                                                                         gchar               **prefix);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_track                 (HyScanGtkWaterfallState *state,
                                                                         HyScanDB            **db,
                                                                         gchar               **project,
                                                                         gchar               **track,
                                                                         gboolean             *raw);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_ship_speed            (HyScanGtkWaterfallState *state,
                                                                         gfloat               *speed);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_sound_velocity        (HyScanGtkWaterfallState *state,
                                                                         GArray              **velocity);

HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_depth_source          (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType     *source,
                                                                         guint                *channel);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_depth_time            (HyScanGtkWaterfallState *state,
                                                                         gulong               *usecs);
HYSCAN_API
void                   hyscan_gtk_waterfall_state_get_depth_filter_size     (HyScanGtkWaterfallState *state,
                                                                         guint                *size);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_STATE_H__ */
