#ifndef __HYSCAN_GTK_WATERFALL_TOOLS_H__
#define __HYSCAN_GTK_WATERFALL_TOOLS_H__

#include <hyscan-api.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHADOW_DEFAULT "rgba (0, 0, 0, 0.75)"
#define FRAME_DEFAULT "rgba (255, 255, 255, 0.25)"
#define HANDLE_RADIUS 9

typedef struct
{
  gdouble x;
  gdouble y;
} HyScanCoordinates;

HYSCAN_API
gdouble                 hyscan_gtk_waterfall_tools_distance            (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

HYSCAN_API
gdouble                 hyscan_gtk_waterfall_tools_angle               (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

HYSCAN_API
HyScanCoordinates       hyscan_gtk_waterfall_tools_middle              (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_line_in_square      (HyScanCoordinates *line_start,
                                                                        HyScanCoordinates *line_end,
                                                                        HyScanCoordinates *square_start,
                                                                        HyScanCoordinates *square_end);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_line_k_b_calc       (HyScanCoordinates *line_start,
                                                                        HyScanCoordinates *line_end,
                                                                        gdouble           *k,
                                                                        gdouble           *b);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_point_in_square     (HyScanCoordinates *point,
                                                                        HyScanCoordinates *square_start,
                                                                        HyScanCoordinates *square_end);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_point_in_square_w   (HyScanCoordinates *point,
                                                                        HyScanCoordinates *square_start,
                                                                        gdouble            dx,
                                                                        gdouble            dy);

HYSCAN_API
cairo_pattern_t*        hyscan_gtk_waterfall_tools_make_handle_pattern (gdouble            radius,
                                                                        GdkRGBA            inner,
                                                                        GdkRGBA            outer);

HYSCAN_API
void             hyscan_cairo_set_source_gdk_rgba                      (cairo_t           *cr,
                                                                        GdkRGBA           *rgba);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_TOOLS_H__ */
