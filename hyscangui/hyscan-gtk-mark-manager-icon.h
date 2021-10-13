/* hyscan-gtk-mark-manager-icon.h
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

#ifndef __HYSCAN_GTK_MARK_MANAGER_ICON_H__
#define __HYSCAN_GTK_MARK_MANAGER_ICON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MARK_MANAGER_ICON hyscan_gtk_mark_manager_icon_get_type ()
#define HAS_MANY_ICONS(icon) ( ( (icon)->length > 1) ? TRUE : FALSE)

/* Данные для иконки. */
typedef struct
{
  GdkPixbuf *icon;    /* Иконка. */
  gchar     *tooltip; /* Текст всплывающей подсказки. */
}HyScanGtkMarkManagerIconData;

/* Иконка. */
typedef struct
{
  HyScanGtkMarkManagerIconData *data;   /* Указатиель на массив с данными иконок. */
  guint                         length; /* Количество элементов в массиве. */
}HyScanGtkMarkManagerIcon;

GType                     hyscan_gtk_mark_manager_icon_get_type     (void);

HyScanGtkMarkManagerIcon* hyscan_gtk_mark_manager_icon_new          (HyScanGtkMarkManagerIcon  *self,
                                                                     GdkPixbuf                 *icon,
                                                                     const gchar               *tooltip);

void                      hyscan_gtk_mark_manager_icon_free         (HyScanGtkMarkManagerIcon  *self);

HyScanGtkMarkManagerIcon* hyscan_gtk_mark_manager_icon_copy         (HyScanGtkMarkManagerIcon  *self);

GStrv                     hyscan_gtk_mark_manager_icon_get_tooltips (HyScanGtkMarkManagerIcon  *self);

GdkPixbuf*                hyscan_gtk_mark_manager_icon_get_icon     (HyScanGtkMarkManagerIcon  *self);

gboolean                  hyscan_gtk_mark_manager_icon_add          (HyScanGtkMarkManagerIcon  *self,
                                                                     GdkPixbuf                 *icon,
                                                                     const gchar               *tooltip);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_ICON_H__ */
