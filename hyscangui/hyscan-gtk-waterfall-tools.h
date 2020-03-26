/* hyscan-gtk-waterfall-tools.h
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */
#ifndef __HYSCAN_GTK_WATERFALL_TOOLS_H__
#define __HYSCAN_GTK_WATERFALL_TOOLS_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHADOW_DEFAULT "rgba (0, 0, 0, 0.75)"
#define FRAME_DEFAULT "rgba (255, 255, 255, 0.25)"
#define HANDLE_RADIUS 9

/**
 * HyScanCoordinates:
 * Функция определяет расстояние между точками.
 *
 * @x: x
 * @y: y
 *
 * Структура с координатами.
 */
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
cairo_pattern_t *       hyscan_gtk_waterfall_tools_make_handle_pattern (gdouble            radius,
                                                                        GdkRGBA            inner,
                                                                        GdkRGBA            outer);

HYSCAN_API
void                    hyscan_cairo_set_source_gdk_rgba               (cairo_t           *cr,
                                                                        GdkRGBA           *rgba);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_TOOLS_H__ */
