#ifndef __HYSCAN_GTK_WATERFALL_TOOLS_H__
#define __HYSCAN_GTK_WATERFALL_TOOLS_H__

#include <hyscan-api.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
  gdouble x;
  gdouble y;
} HyScanCoordinates;

HYSCAN_API
gdouble
hyscan_gtk_waterfall_tools_radius (HyScanCoordinates *start,
                                   HyScanCoordinates *end);

HYSCAN_API
gdouble
hyscan_gtk_waterfall_tools_angle (HyScanCoordinates *start,
                                  HyScanCoordinates *end);

HYSCAN_API
HyScanCoordinates
hyscan_gtk_waterfall_tools_middle (HyScanCoordinates *start,
                                   HyScanCoordinates *end);

HYSCAN_API
gboolean
hyscan_gtk_waterfall_tools_line_in_square (HyScanCoordinates *line_start,
                                           HyScanCoordinates *line_end,
                                           HyScanCoordinates *square_start,
                                           HyScanCoordinates *square_end);

gboolean
hyscan_gtk_waterfall_tools_line_k_b_calc  (HyScanCoordinates *line_start,
                                           HyScanCoordinates *line_end,
                                           gdouble           *k,
                                           gdouble           *b);
HYSCAN_API
gboolean
hyscan_gtk_waterfall_tools_point_in_square (HyScanCoordinates *point,
                                            HyScanCoordinates *square_start,
                                            HyScanCoordinates *square_end);
HYSCAN_API
gboolean
hyscan_gtk_waterfall_tools_point_in_square_w (HyScanCoordinates *point,
                                              HyScanCoordinates *square_start,
                                              gdouble            dx,
                                              gdouble            dy);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_TOOLS_H__ */
