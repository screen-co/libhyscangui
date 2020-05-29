/* hyscan-gtk-mark-export.h
 *
 * Copyright 2019 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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

#ifndef __HYSCAN_GTK_MARK_EXPORT_H__
#define __HYSCAN_GTK_MARK_EXPORT_H__

#include <gtk/gtk.h>
#include <hyscan-mark-loc-model.h>
#include <hyscan-object-model.h>
#include <hyscan-tile-queue.h>
#include <hyscan-gtk-model-manager.h>

HYSCAN_API
void
hyscan_gtk_mark_export_save_as_csv       (GtkWindow             *window,
                                          HyScanMarkLocModel    *ml_model,
                                          HyScanObjectModel     *mark_geo_model,
                                          gchar                 *project_name);

HYSCAN_API
void
hyscan_gtk_mark_export_copy_to_clipboard (HyScanMarkLocModel    *ml_model,
                                          HyScanObjectModel     *mark_geo_model,
                                          gchar                 *project_name);

HYSCAN_API
gchar*
hyscan_gtk_mark_export_to_str            (HyScanMarkLocModel    *ml_model,
                                          HyScanObjectModel     *mark_geo_model,
                                          gchar                 *project_name);

HYSCAN_API
void
hyscan_gtk_mark_export_save_as_html      (HyScanGtkModelManager *model_manager,
                                          GtkWindow             *toplevel,
                                          gboolean               toggled);

#endif /* __HYSCAN_GTK_MARK_EXPORT_H__ */

