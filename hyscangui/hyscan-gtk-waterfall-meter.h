/* hyscan-gtk-waterfall-meter.h
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

#ifndef __HYSCAN_GTK_WATERFALL_METER_H__
#define __HYSCAN_GTK_WATERFALL_METER_H__

#include "hyscan-gtk-layer.h"
#include "hyscan-gtk-waterfall.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_METER             (hyscan_gtk_waterfall_meter_get_type ())
#define HYSCAN_GTK_WATERFALL_METER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeter))
#define HYSCAN_IS_GTK_WATERFALL_METER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))
#define HYSCAN_IS_GTK_WATERFALL_METER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))

typedef struct _HyScanGtkWaterfallMeter HyScanGtkWaterfallMeter;
typedef struct _HyScanGtkWaterfallMeterPrivate HyScanGtkWaterfallMeterPrivate;
typedef struct _HyScanGtkWaterfallMeterClass HyScanGtkWaterfallMeterClass;

struct _HyScanGtkWaterfallMeter
{
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallMeterPrivate *priv;
};

struct _HyScanGtkWaterfallMeterClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                           hyscan_gtk_waterfall_meter_get_type            (void);


HYSCAN_API
HyScanGtkWaterfallMeter        *hyscan_gtk_waterfall_meter_new                 (void);


HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_main_color      (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);

HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_color    (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);

HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_frame_color     (HyScanGtkWaterfallMeter *meter,
                                                                                GdkRGBA                  color);

HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_meter_width     (HyScanGtkWaterfallMeter *meter,
                                                                                gdouble                  width);

HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_width    (HyScanGtkWaterfallMeter *meter,
                                                                                gdouble                  width);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_METER_H__ */
