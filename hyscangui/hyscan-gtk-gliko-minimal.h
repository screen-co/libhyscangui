/* hyscan-gtk-gliko_minimal.h
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
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

#ifndef __HYSCAN_GTK_GLIKO_MINIMAL_H__
#define __HYSCAN_GTK_GLIKO_MINIMAL_H__

#include <gtk/gtk.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_GLIKO_MINIMAL (hyscan_gtk_gliko_minimal_get_type ())
#define HYSCAN_GTK_GLIKO_MINIMAL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimal))
#define HYSCAN_IS_GTK_GLIKO_MINIMAL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_GLIKO_MINIMAL))
#define HYSCAN_GTK_GLIKO_MINIMAL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalClass))
#define HYSCAN_IS_GTK_GLIKO_MINIMAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_GLIKO_MINIMAL))
#define HYSCAN_GTK_GLIKO_MINIMAL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_GLIKO_MINIMAL, HyScanGtkGlikoMinimalClass))

typedef struct _HyScanGtkGlikoMinimal HyScanGtkGlikoMinimal;
typedef struct _HyScanGtkGlikoMinimalPrivate HyScanGtkGlikoMinimalPrivate;
typedef struct _HyScanGtkGlikoMinimalClass HyScanGtkGlikoMinimalClass;

struct _HyScanGtkGlikoMinimal
{
  GtkGLArea parent;
  HyScanGtkGlikoMinimalPrivate *priv;
};

struct _HyScanGtkGlikoMinimalClass
{
  GtkGLAreaClass parent_class;
};

HYSCAN_API
GType hyscan_gtk_gliko_minimal_get_type (void);

HYSCAN_API
GtkWidget *hyscan_gtk_gliko_minimal_new (void);

G_END_DECLS

#endif
