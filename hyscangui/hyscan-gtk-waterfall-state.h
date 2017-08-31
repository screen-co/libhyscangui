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

GType                  hyscan_gtk_waterfall_state_get_type                  (void);

HyScanGtkWaterfallState  *hyscan_gtk_waterfall_state_new                       (void);

void                   hyscan_gtk_waterfall_state_echosounder               (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      source);
void                   hyscan_gtk_waterfall_state_sidescan                  (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      lsource,
                                                                         HyScanSourceType      rsource);
void                   hyscan_gtk_waterfall_set_tile_type                   (HyScanGtkWaterfallState *state,
                                                                         HyScanTileType        type);

void                   hyscan_gtk_waterfall_state_set_profile               (HyScanGtkWaterfallState *state,
                                                                         const gchar          *profile);

void                   hyscan_gtk_waterfall_state_set_cache                 (HyScanGtkWaterfallState *state,
                                                                         HyScanCache          *cache,
                                                                         HyScanCache          *cache2,
                                                                         const gchar          *prefix);

void                   hyscan_gtk_waterfall_state_set_track                 (HyScanGtkWaterfallState *state,
                                                                         HyScanDB             *db,
                                                                         const gchar          *project,
                                                                         const gchar          *track,
                                                                         gboolean              raw);

void                   hyscan_gtk_waterfall_state_set_ship_speed            (HyScanGtkWaterfallState *state,
                                                                         gfloat                speed);
void                   hyscan_gtk_waterfall_state_set_sound_velocity        (HyScanGtkWaterfallState *state,
                                                                         GArray               *velocity);

void                   hyscan_gtk_waterfall_state_set_depth_source          (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType      source,
                                                                         guint                 channel);
void                   hyscan_gtk_waterfall_state_set_depth_time            (HyScanGtkWaterfallState *state,
                                                                         gulong                usecs);
void                   hyscan_gtk_waterfall_state_set_depth_filter_size     (HyScanGtkWaterfallState *state,
                                                                         guint                 size);

void                   hyscan_gtk_waterfall_state_get_sources               (HyScanGtkWaterfallState       *state,
                                                                         HyScanWaterfallDisplayType *type,
                                                                         HyScanSourceType           *lsource,
                                                                         HyScanSourceType           *rsource);
void                   hyscan_gtk_waterfall_state_get_tile_type             (HyScanGtkWaterfallState *state,
                                                                         HyScanTileType       *type);
void                   hyscan_gtk_waterfall_state_get_profile               (HyScanGtkWaterfallState *state,
                                                                         gchar               **profile);

void                   hyscan_gtk_waterfall_state_get_cache                 (HyScanGtkWaterfallState *state,
                                                                         HyScanCache         **cache,
                                                                         HyScanCache         **cache2,
                                                                         gchar               **prefix);

void                   hyscan_gtk_waterfall_state_get_track                 (HyScanGtkWaterfallState *state,
                                                                         HyScanDB            **db,
                                                                         gchar               **project,
                                                                         gchar               **track,
                                                                         gboolean             *raw);

void                   hyscan_gtk_waterfall_state_get_ship_speed            (HyScanGtkWaterfallState *state,
                                                                         gfloat               *speed);
void                   hyscan_gtk_waterfall_state_get_sound_velocity        (HyScanGtkWaterfallState *state,
                                                                         GArray              **velocity);

void                   hyscan_gtk_waterfall_state_get_depth_source          (HyScanGtkWaterfallState *state,
                                                                         HyScanSourceType     *source,
                                                                         guint                *channel);
void                   hyscan_gtk_waterfall_state_get_depth_time            (HyScanGtkWaterfallState *state,
                                                                         gulong               *usecs);
void                   hyscan_gtk_waterfall_state_get_depth_filter_size     (HyScanGtkWaterfallState *state,
                                                                         guint                *size);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_STATE_H__ */
