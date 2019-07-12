/* hyscan-gtk-waterfall.h
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

#ifndef __HYSCAN_GTK_WATERFALL_H__
#define __HYSCAN_GTK_WATERFALL_H__

#include "hyscan-gtk-waterfall-state.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL             (hyscan_gtk_waterfall_get_type ())
#define HYSCAN_GTK_WATERFALL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfall))
#define HYSCAN_IS_GTK_WATERFALL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))
#define HYSCAN_IS_GTK_WATERFALL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))

typedef struct _HyScanGtkWaterfall HyScanGtkWaterfall;
typedef struct _HyScanGtkWaterfallPrivate HyScanGtkWaterfallPrivate;
typedef struct _HyScanGtkWaterfallClass HyScanGtkWaterfallClass;

struct _HyScanGtkWaterfall
{
  HyScanGtkWaterfallState parent_instance;

  HyScanGtkWaterfallPrivate *priv;
};

struct _HyScanGtkWaterfallClass
{
  HyScanGtkWaterfallStateClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_get_type                   (void);

HYSCAN_API
GtkWidget *             hyscan_gtk_waterfall_new                        (HyScanCache        *cache);

HYSCAN_API
void                    hyscan_gtk_waterfall_queue_draw                 (HyScanGtkWaterfall *wfall);

HYSCAN_API
void                    hyscan_gtk_waterfall_set_upsample               (HyScanGtkWaterfall *wfall,
                                                                         gint                upsample);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_colormap               (HyScanGtkWaterfall *wfall,
                                                                         HyScanSourceType    source,
                                                                         guint32            *colormap,
                                                                         guint               length,
                                                                         guint32             background);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_colormap_for_all       (HyScanGtkWaterfall *wfall,
                                                                         guint32            *colormap,
                                                                         guint               length,
                                                                         guint32             background);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_levels                 (HyScanGtkWaterfall *wfall,
                                                                         HyScanSourceType    source,
                                                                         gdouble             black,
                                                                         gdouble             gamma,
                                                                         gdouble             white);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_levels_for_all         (HyScanGtkWaterfall *wfall,
                                                                         gdouble             black,
                                                                         gdouble             gamma,
                                                                         gdouble             white);

HYSCAN_API
gint                    hyscan_gtk_waterfall_get_scale                  (HyScanGtkWaterfall *wfall,
                                                                         const gdouble     **zooms,
                                                                         gint               *num);

HYSCAN_API
gboolean                hyscan_gtk_waterfall_automove                   (HyScanGtkWaterfall *wfall,
                                                                         gboolean            automove);

HYSCAN_API
void                    hyscan_gtk_waterfall_set_automove_period        (HyScanGtkWaterfall *wfall,
                                                                         gint64              usecs);

HYSCAN_API
void                    hyscan_gtk_waterfall_set_regeneration_period    (HyScanGtkWaterfall *wfall,
                                                                         gint64              usecs);

HYSCAN_API
void                    hyscan_gtk_waterfall_set_substrate              (HyScanGtkWaterfall *wfall,
                                                                         guint32             substrate);

HYSCAN_API
GtkWidget *             hyscan_gtk_waterfall_make_grid                  (HyScanGtkWaterfall *wfall);
G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
