/* hyscan-gtk-area.h
 *
 * Copyright 2015 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanGui.
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

#ifndef __HYSCAN_GTK_AREA_H__
#define __HYSCAN_GTK_AREA_H__

#include <gtk/gtk.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_AREA             (hyscan_gtk_area_get_type ())
#define HYSCAN_GTK_AREA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_AREA, HyScanGtkArea))
#define HYSCAN_IS_GTK_AREA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_AREA))
#define HYSCAN_GTK_AREA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_AREA, HyScanGtkAreaClass))
#define HYSCAN_IS_GTK_AREA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_AREA))
#define HYSCAN_GTK_AREA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_AREA, HyScanGtkAreaClass))

typedef struct _HyScanGtkArea HyScanGtkArea;
typedef struct _HyScanGtkAreaClass HyScanGtkAreaClass;

struct _HyScanGtkAreaClass
{
  GtkGridClass parent_class;
};

HYSCAN_API
GType hyscan_gtk_area_get_type (void);

HYSCAN_API
GtkWidget *    hyscan_gtk_area_new                     (void);

HYSCAN_API
void           hyscan_gtk_area_set_central             (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

HYSCAN_API
void           hyscan_gtk_area_set_left                (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

HYSCAN_API
void           hyscan_gtk_area_set_right               (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

HYSCAN_API
void           hyscan_gtk_area_set_top                 (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

HYSCAN_API
void           hyscan_gtk_area_set_bottom              (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

HYSCAN_API
void           hyscan_gtk_area_set_left_visible        (HyScanGtkArea         *area,
                                                        gboolean               visible);

HYSCAN_API
gboolean       hyscan_gtk_area_is_left_visible         (HyScanGtkArea         *area);

HYSCAN_API
void           hyscan_gtk_area_set_right_visible       (HyScanGtkArea         *area,
                                                        gboolean               visible);

HYSCAN_API
gboolean       hyscan_gtk_area_is_right_visible        (HyScanGtkArea         *area);

HYSCAN_API
void           hyscan_gtk_area_set_top_visible         (HyScanGtkArea         *area,
                                                        gboolean               visible);

HYSCAN_API
gboolean       hyscan_gtk_area_is_top_visible          (HyScanGtkArea         *area);

HYSCAN_API
void           hyscan_gtk_area_set_bottom_visible      (HyScanGtkArea         *area,
                                                        gboolean               visible);

HYSCAN_API
gboolean       hyscan_gtk_area_is_bottom_visible       (HyScanGtkArea         *area);

HYSCAN_API
void           hyscan_gtk_area_set_all_visible         (HyScanGtkArea         *area,
                                                        gboolean               visible);

G_END_DECLS

#endif /* __HYSCAN_GTK_AREA_H__ */
