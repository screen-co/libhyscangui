/* hyscan-gtk-gliko.h
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
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

#ifndef __HYSCAN_GTK_GLIKO_H__
#define __HYSCAN_GTK_GLIKO_H__

#include <gtk/gtk.h>
#include <hyscan-api.h>
#include <hyscan-data-player.h>

G_BEGIN_DECLS


#define HYSCAN_TYPE_GTK_GLIKO             (hyscan_gtk_gliko_get_type ())
#define HYSCAN_GTK_GLIKO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGliko))
#define HYSCAN_IS_GTK_GLIKO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_GLIKO))
#define HYSCAN_GTK_GLIKO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoClass))
#define HYSCAN_IS_GTK_GLIKO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_GLIKO))
#define HYSCAN_GTK_GLIKO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoClass))

typedef struct _HyScanGtkGliko HyScanGtkGliko;
typedef struct _HyScanGtkGlikoPrivate HyScanGtkGlikoPrivate;
typedef struct _HyScanGtkGlikoClass HyScanGtkGlikoClass;

HYSCAN_API
GType          hyscan_gtk_gliko_get_type               (void);

HYSCAN_API
GtkWidget *    hyscan_gtk_gliko_new                    (void);

HYSCAN_API
void           hyscan_gtk_gliko_set_num_azimuthes      (HyScanGtkGliko *instance,
                                                        const guint num_azimuthes);

HYSCAN_API
guint          hyscan_gtk_gliko_get_num_azimuthes      (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_player             (HyScanGtkGliko *instance,
                                                        HyScanDataPlayer *player);

HYSCAN_API
HyScanDataPlayer *hyscan_gtk_gliko_get_player          (HyScanGtkGliko *instance);


HYSCAN_API
void           hyscan_gtk_gliko_set_fade_period       (HyScanGtkGliko *instance,
                                                       const gdouble seconds );

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_fade_period       (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_fade_koeff        (HyScanGtkGliko *instance,
                                                       const gdouble koef );

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_fade_koeff        (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_scale             (HyScanGtkGliko *instance,
                                                       const gdouble scale);

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_scale             (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_center            (HyScanGtkGliko *instance,
                                                       const gdouble cx,
                                                       const gdouble cy);

HYSCAN_API
void           hyscan_gtk_gliko_get_center            (HyScanGtkGliko *instance,
                                                       gdouble *cx,
                                                       gdouble *cy);

HYSCAN_API
void           hyscan_gtk_gliko_set_rotation          (HyScanGtkGliko *instance,
                                                       const gdouble alpha);

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_rotation          (HyScanGtkGliko *instance);


HYSCAN_API
void           hyscan_gtk_gliko_set_black_point       (HyScanGtkGliko *instance,
                                                       const gdouble black);
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_black_point       (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_white_point       (HyScanGtkGliko *instance,
                                                       const gdouble white);
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_white_point       (HyScanGtkGliko *instance);

HYSCAN_API
void           hyscan_gtk_gliko_set_gamma_value       (HyScanGtkGliko *instance,
                                                       const gdouble gamma);
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_gamma_value       (HyScanGtkGliko *instance);


HYSCAN_API
HyScanSourceType hyscan_gtk_gliko_get_source          (HyScanGtkGliko *instance,
                                                       const gint channel);

G_END_DECLS

#endif
