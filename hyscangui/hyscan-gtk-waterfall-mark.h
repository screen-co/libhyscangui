/* hyscan-gtk-waterfall-mark.h
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

#ifndef __HYSCAN_GTK_WATERFALL_MARK_H__
#define __HYSCAN_GTK_WATERFALL_MARK_H__

#include "hyscan-gtk-layer.h"
#include <hyscan-mark-model.h>

G_BEGIN_DECLS

#define HYSCAN_GTK_WATERFALL_MARKS_ALL             G_MAXUINT64

#define HYSCAN_TYPE_GTK_WATERFALL_MARK             (hyscan_gtk_waterfall_mark_get_type ())
#define HYSCAN_GTK_WATERFALL_MARK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMark))
#define HYSCAN_IS_GTK_WATERFALL_MARK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))
#define HYSCAN_IS_GTK_WATERFALL_MARK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_MARK))
#define HYSCAN_GTK_WATERFALL_MARK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_MARK, HyScanGtkWaterfallMarkClass))

typedef struct _HyScanGtkWaterfallMark HyScanGtkWaterfallMark;
typedef struct _HyScanGtkWaterfallMarkPrivate HyScanGtkWaterfallMarkPrivate;
typedef struct _HyScanGtkWaterfallMarkClass HyScanGtkWaterfallMarkClass;

struct _HyScanGtkWaterfallMark
{
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallMarkPrivate *priv;
};

struct _HyScanGtkWaterfallMarkClass
{
  GInitiallyUnownedClass parent_class;
};

typedef enum
{
  HYSCAN_GTK_WATERFALL_MARKS_DRAW_DOTS,
  HYSCAN_GTK_WATERFALL_MARKS_DRAW_BORDER
} HyScanGtkWaterfallMarksDraw;

HYSCAN_API
GType                    hyscan_gtk_waterfall_mark_get_type            (void);

HYSCAN_API
HyScanGtkWaterfallMark  *hyscan_gtk_waterfall_mark_new                 (HyScanMarkModel            *markmodel);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_mark_filter     (HyScanGtkWaterfallMark     *mark,
                                                                        guint64                     filter);
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_draw_type       (HyScanGtkWaterfallMark     *mark,
                                                                        HyScanGtkWaterfallMarksDraw type);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_main_color      (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_mark_width      (HyScanGtkWaterfallMark     *mark,
                                                                        gdouble                     width);
HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_frame_color     (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_shadow_color    (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_shadow_width    (HyScanGtkWaterfallMark     *mark,
                                                                        gdouble                     width);

HYSCAN_API
void                     hyscan_gtk_waterfall_mark_set_blackout_color  (HyScanGtkWaterfallMark     *mark,
                                                                        GdkRGBA                     color);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_MARK_H__ */
