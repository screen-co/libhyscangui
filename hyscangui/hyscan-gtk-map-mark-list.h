/* hyscan-gtk-map-mark-list.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_GTK_MAP_MARK_LIST_H__
#define __HYSCAN_GTK_MAP_MARK_LIST_H__

#include <hyscan-gtk-map-wfmark.h>
#include <hyscan-gtk-map-geomark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_MARK_LIST             (hyscan_gtk_map_mark_list_get_type ())
#define HYSCAN_GTK_MAP_MARK_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_MARK_LIST, HyScanGtkMapMarkList))
#define HYSCAN_IS_GTK_MAP_MARK_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_MARK_LIST))
#define HYSCAN_GTK_MAP_MARK_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_MARK_LIST, HyScanGtkMapMarkListClass))
#define HYSCAN_IS_GTK_MAP_MARK_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_MARK_LIST))
#define HYSCAN_GTK_MAP_MARK_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_MARK_LIST, HyScanGtkMapMarkListClass))

typedef struct _HyScanGtkMapMarkList HyScanGtkMapMarkList;
typedef struct _HyScanGtkMapMarkListPrivate HyScanGtkMapMarkListPrivate;
typedef struct _HyScanGtkMapMarkListClass HyScanGtkMapMarkListClass;

struct _HyScanGtkMapMarkList
{
  GtkTreeView parent_instance;

  HyScanGtkMapMarkListPrivate *priv;
};

struct _HyScanGtkMapMarkListClass
{
  GtkTreeViewClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_mark_list_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_mark_list_new              (HyScanObjectModel    *model_geomark,
                                                                  HyScanMarkLocModel   *model_wfmark,
                                                                  HyScanGtkMapWfmark   *layer_wfmark,
                                                                  HyScanGtkMapGeomark  *layer_geomark);

HYSCAN_API
gchar *                hyscan_gtk_map_mark_list_get_selected     (HyScanGtkMapMarkList *mark_list,
                                                                  HyScanMark           *mark,
                                                                  HyScanGeoPoint       *location);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_MARK_LIST_H__ */
