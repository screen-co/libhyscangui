/* hyscan-gtk-map-wfmark.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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


#ifndef __HYSCAN_GTK_MAP_WFMARK_H__
#define __HYSCAN_GTK_MAP_WFMARK_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-mark-loc-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_WFMARK             (hyscan_gtk_map_wfmark_get_type ())
#define HYSCAN_GTK_MAP_WFMARK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK, HyScanGtkMapWfmark))
#define HYSCAN_IS_GTK_MAP_WFMARK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK))
#define HYSCAN_GTK_MAP_WFMARK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_WFMARK, HyScanGtkMapWfmarkClass))
#define HYSCAN_IS_GTK_MAP_WFMARK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_WFMARK))
#define HYSCAN_GTK_MAP_WFMARK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK, HyScanGtkMapWfmarkClass))

typedef struct _HyScanGtkMapWfmark HyScanGtkMapWfmark;
typedef struct _HyScanGtkMapWfmarkPrivate HyScanGtkMapWfmarkPrivate;
typedef struct _HyScanGtkMapWfmarkClass HyScanGtkMapWfmarkClass;

struct _HyScanGtkMapWfmark
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapWfmarkPrivate *priv;
};

struct _HyScanGtkMapWfmarkClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType            hyscan_gtk_map_wfmark_get_type       (void);

HYSCAN_API
HyScanGtkLayer * hyscan_gtk_map_wfmark_new            (HyScanMarkLocModel    *model,
                                                       HyScanDB              *db,
                                                       HyScanCache           *cache);
                                                                             
HYSCAN_API                                                                   
void             hyscan_gtk_map_wfmark_mark_highlight (HyScanGtkMapWfmark    *wfm_layer,
                                                       const gchar           *mark_id);
                                                                             
HYSCAN_API                                                                   
void             hyscan_gtk_map_wfmark_mark_view      (HyScanGtkMapWfmark    *wfm_layer,
                                                       const gchar           *mark_id,
                                                       gboolean               zoom_in);

HYSCAN_API
void             hyscan_gtk_map_wfmark_set_project    (HyScanGtkMapWfmark    *wfm_layer,
                                                       const gchar           *project_name);

HYSCAN_API
void             hyscan_gtk_map_wfmark_set_show_mode  (HyScanGtkMapWfmark    *wfm_layer,
                                                       gint                   mode);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_WFMARK_H__ */
