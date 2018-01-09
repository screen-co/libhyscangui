#ifndef __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__
#define __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__

#include <hyscan-gtk-waterfall-state.h>

G_BEGIN_DECLS

typedef struct
{
  HyScanWaterfallDisplayType  display_type;
  HyScanSourceType            lsource;
  HyScanSourceType            rsource;
  gboolean                    sources_changed;

  HyScanTileType              tile_type;
  gboolean                    tile_type_changed;

  HyScanCache                *cache;
  gchar                      *prefix;
  gboolean                    cache_changed;

  HyScanDB                   *db;
  gchar                      *project;
  gchar                      *track;
  gboolean                    raw;
  gboolean                    track_changed;

  gfloat                      ship_speed;
  gboolean                    speed_changed;

  GArray                     *velocity;
  gboolean                    velocity_changed;

  HyScanSourceType            depth_source;
  guint                       depth_channel;
  gboolean                    depth_source_changed;

  gulong                      depth_time;
  guint                       depth_size;
  gboolean                    depth_params_changed;

  guint64                     mark_filter;
  gboolean                    mark_filter_changed;
} _HyScanGtkWaterfallMayerState;

void hyscan_gtk_waterfall_mayer_trigger          (HyScanGtkWaterfallMayer *mayer);
void hyscan_gtk_waterfall_mayer_connect_signals  (HyScanGtkWaterfallMayer *mayer,
                                                  guint64                  mask);
void hyscan_gtk_waterfall_mayer_create_thread    (HyScanGtkWaterfallMayer *mayer,
                                                  HyScanGtkWaterfallMayerCheckFunc *check_func,
                                                  HyScanGtkWaterfallMayerThreadFunc *thread_func,
                                                  HyScanGtkWaterfallMayerStopFunc *stop_func,
                                                  gpointer       thread_data,
                                                  const gchar   *thread_name);

void hyscan_gtk_waterfall_mayer_kill_thread (HyScanGtkWaterfallMayer *mayer);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MAYER_PRIVATE_H__ */
