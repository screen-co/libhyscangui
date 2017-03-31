#ifndef __HYSCAN_GTK_WATERFALL_PRIVATE_H__
#define __HYSCAN_GTK_WATERFALL_PRIVATE_H__

G_BEGIN_DECLS

HyScanGtkWaterfallType  hyscan_gtk_waterfall_get_sources           (HyScanGtkWaterfall *waterfall,
                                                                    HyScanSourceType   *left,
                                                                    HyScanSourceType   *right);

HyScanCache            *hyscan_gtk_waterfall_get_cache             (HyScanGtkWaterfall *waterfall);
HyScanCache            *hyscan_gtk_waterfall_get_cache2            (HyScanGtkWaterfall *waterfall);
const gchar            *hyscan_gtk_waterfall_get_cache_prefix      (HyScanGtkWaterfall *waterfall);

HyScanTileType          hyscan_gtk_waterfall_get_tile_type         (HyScanGtkWaterfall *waterfall);

gfloat                  hyscan_gtk_waterfall_get_ship_speed        (HyScanGtkWaterfall *waterfall);
GArray                 *hyscan_gtk_waterfall_get_sound_velocity    (HyScanGtkWaterfall *waterfall);

HyScanSourceType        hyscan_gtk_waterfall_get_depth_source      (HyScanGtkWaterfall *waterfall,
                                                                    guint              *channel);
                                                                    
guint                   hyscan_gtk_waterfall_get_depth_filter_size (HyScanGtkWaterfall *waterfall);
gulong                  hyscan_gtk_waterfall_get_depth_time        (HyScanGtkWaterfall *waterfall);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_PRIVATE_H__ */
