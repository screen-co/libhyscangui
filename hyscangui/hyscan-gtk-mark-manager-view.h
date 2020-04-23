/* hyscan-gtk-mark-manager-view.h
 *
 * Copyright 2020 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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

#ifndef __HYSCAN_GTK_MARK_MANAGER_VIEW_H__
#define __HYSCAN_GTK_MARK_MANAGER_VIEW_H__

#include <gtk/gtk.h>
#include <hyscan-gtk-model-manager.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER_VIEW             (hyscan_mark_manager_view_get_type ())
#define HYSCAN_MARK_MANAGER_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerView))
#define HYSCAN_IS_MARK_MANAGER_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW))
#define HYSCAN_MARK_MANAGER_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerViewClass))
#define HYSCAN_IS_MARK_MANAGER_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER_VIEW))
#define HYSCAN_MARK_MANAGER_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerViewClass))

typedef struct _HyScanMarkManagerView        HyScanMarkManagerView;
typedef struct _HyScanMarkManagerViewPrivate HyScanMarkManagerViewPrivate;
typedef struct _HyScanMarkManagerViewClass   HyScanMarkManagerViewClass;

struct _HyScanMarkManagerView
{
  GtkScrolledWindow parent_instance;

  HyScanMarkManagerViewPrivate *priv;
};

struct _HyScanMarkManagerViewClass
{
  GtkScrolledWindowClass parent_class;
};

GType               hyscan_mark_manager_view_get_type          (void);

GtkWidget*          hyscan_mark_manager_view_new               (GtkTreeModel          *store);

void                hyscan_mark_manager_view_set_store         (HyScanMarkManagerView *self,
                                                                GtkTreeModel          *store);

void                hyscan_mark_manager_view_expand_all        (HyScanMarkManagerView *self);

void                hyscan_mark_manager_view_collapse_all      (HyScanMarkManagerView *self);

void                hyscan_mark_manager_view_unselect_all      (HyScanMarkManagerView *self);

void                hyscan_mark_manager_view_toggle_all        (HyScanMarkManagerView  *self,
                                                                gboolean                active);

void                hyscan_mark_manager_view_expand_path       (HyScanMarkManagerView  *self,
                                                                GtkTreePath            *path,
                                                                gboolean                expanded);

gchar**             hyscan_mark_manager_view_get_toggled       (HyScanMarkManagerView  *self,
                                                                ModelManagerObjectType  type);

void                hyscan_mark_manager_view_select_item       (HyScanMarkManagerView  *self,
                                                                gchar                  *id);

gboolean            hyscan_mark_manager_view_find_item_by_id   (GtkTreeModel           *model,
                                                                GtkTreeIter            *iter,
                                                                const gchar            *id);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_VIEW_H__ */
