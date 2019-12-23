/* hyscan-gtk-waterfall-coord.h
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

#ifndef __HYSCAN_GTK_WATERFALL_COORD_H__
#define __HYSCAN_GTK_WATERFALL_COORD_H__

#include "hyscan-gtk-layer.h"
#include <hyscan-object-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_COORD             (hyscan_gtk_waterfall_coord_get_type ())
#define HYSCAN_GTK_WATERFALL_COORD(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_COORD, HyScanGtkWaterfallCoord))
#define HYSCAN_IS_GTK_WATERFALL_COORD(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_COORD))
#define HYSCAN_GTK_WATERFALL_COORD_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_COORD, HyScanGtkWaterfallCoordClass))
#define HYSCAN_IS_GTK_WATERFALL_COORD_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_COORD))
#define HYSCAN_GTK_WATERFALL_COORD_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_COORD, HyScanGtkWaterfallCoordClass))

typedef struct _HyScanGtkWaterfallCoord HyScanGtkWaterfallCoord;
typedef struct _HyScanGtkWaterfallCoordPrivate HyScanGtkWaterfallCoordPrivate;
typedef struct _HyScanGtkWaterfallCoordClass HyScanGtkWaterfallCoordClass;

struct _HyScanGtkWaterfallCoord
{
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallCoordPrivate *priv;
};

struct _HyScanGtkWaterfallCoordClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                    hyscan_gtk_waterfall_coord_get_type            (void);

HYSCAN_API
HyScanGtkWaterfallCoord *hyscan_gtk_waterfall_coord_new                 (void);

HYSCAN_API
void                     hyscan_gtk_waterfall_coord_set_coord_filter     (HyScanGtkWaterfallCoord     *coord,
                                                                        guint64                     filter);
HYSCAN_API
void                     hyscan_gtk_waterfall_coord_set_main_color      (HyScanGtkWaterfallCoord     *coord,
                                                                        GdkRGBA                     color);

HYSCAN_API
void                     hyscan_gtk_waterfall_coord_set_main_width      (HyScanGtkWaterfallCoord     *coord,
                                                                         gdouble                     width);
HYSCAN_API

HYSCAN_API
void                     hyscan_gtk_waterfall_coord_set_shadow_color    (HyScanGtkWaterfallCoord     *coord,
                                                                        GdkRGBA                     color);

HYSCAN_API
void                     hyscan_gtk_waterfall_coord_set_shadow_width    (HyScanGtkWaterfallCoord     *coord,
                                                                        gdouble                     width);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_COORD_H__ */
