/* hyscan-gtk-gliko-area.h
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

#ifndef __HYSCAN_GTK_GLIKO_AREA_H__
#define __HYSCAN_GTK_GLIKO_AREA_H__

#include "hyscan-gtk-gliko-layer.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_GLIKO_AREA             (hyscan_gtk_gliko_area_get_type ())
#define HYSCAN_GTK_GLIKO_AREA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoArea))
#define HYSCAN_IS_GTK_GLIKO_AREA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_GLIKO_AREA))
#define HYSCAN_GTK_GLIKO_AREA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaClass))
#define HYSCAN_IS_GTK_GLIKO_AREA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_GLIKO_AREA))
#define HYSCAN_GTK_GLIKO_AREA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_GLIKO_AREA, HyScanGtkGlikoAreaClass))

typedef struct _HyScanGtkGlikoArea HyScanGtkGlikoArea;
typedef struct _HyScanGtkGlikoAreaPrivate HyScanGtkGlikoAreaPrivate;
typedef struct _HyScanGtkGlikoAreaClass HyScanGtkGlikoAreaClass;

struct _HyScanGtkGlikoArea
{
  GObject parent_instance;

  HyScanGtkGlikoAreaPrivate *priv;
};

struct _HyScanGtkGlikoAreaClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_gtk_gliko_area_get_type (void);

HYSCAN_API
HyScanGtkGlikoArea *hyscan_gtk_gliko_area_new (void);

HYSCAN_API
void hyscan_gtk_gliko_area_init_dimension (HyScanGtkGlikoArea *instance,
                                           const int na,
                                           const int nd);

HYSCAN_API
void hyscan_gtk_gliko_area_set_data (HyScanGtkGlikoArea *instance,
                                     const int channel,
                                     const int azimuth,
                                     const gfloat *data);

HYSCAN_API
void hyscan_gtk_gliko_area_fade (HyScanGtkGlikoArea *instance);

G_END_DECLS

#endif
