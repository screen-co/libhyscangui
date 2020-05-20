/* hyscan-gtk-mark-manager.h
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

#ifndef __HYSCAN_GTK_MARK_MANAGER_H__
#define __HYSCAN_GTK_MARK_MANAGER_H__

#include <gtk/gtk.h>
#include <hyscan-gtk-mark-manager-view.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MARK_MANAGER             (hyscan_gtk_mark_manager_get_type ())
#define HYSCAN_GTK_MARK_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER, HyScanGtkMarkManager))
#define HYSCAN_IS_GTK_MARK_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER))
#define HYSCAN_GTK_MARK_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MARK_MANAGER, HyScanGtkMarkManagerClass))
#define HYSCAN_IS_GTK_MARK_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MARK_MANAGER))
#define HYSCAN_GTK_MARK_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER, HyScanGtkMarkManagerClass))

typedef struct _HyScanGtkMarkManager        HyScanGtkMarkManager;
typedef struct _HyScanGtkMarkManagerPrivate HyScanGtkMarkManagerPrivate;
typedef struct _HyScanGtkMarkManagerClass   HyScanGtkMarkManagerClass;

struct _HyScanGtkMarkManager
{
  GtkBox parent_instance;

  HyScanGtkMarkManagerPrivate *priv;
};

struct _HyScanGtkMarkManagerClass
{
  GtkBoxClass parent_class;
};

GType        hyscan_gtk_mark_manager_get_type   (void);

GtkWidget*   hyscan_gtk_mark_manager_new        (HyScanGtkModelManager *model_manager);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_H__ */
