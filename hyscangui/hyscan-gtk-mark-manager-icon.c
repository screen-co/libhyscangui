/* hyscan-gtk-mark-manager-icon.c
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

/**
 * SECTION: hyscan-gtk-mark-manager-icon
 * @Short_description: Набор функций для работы со структурами иконок Журнала Меток.
 * @Title: HyScanGtkMarkManagerIcon
 * @See_also: #HyScanGtkMarkManager
 *
 * Иконки используются для отображения в графическом виде принадлежности объектов Журнала Меток к группам.
 *
 * Информация об иконке хранится в структуре #HyScanGtkMarkManagerIcon.
 *
 * Для работы с иконками используются функции:
 *
 * - hyscan_gtk_mark_manager_icon_get_type () - регистрация типа Активная иконка;
 * - hyscan_gtk_mark_manager_icon_new () - создание Активной иконки;
 * - hyscan_gtk_mark_manager_icon_free () - удаление Активной иконки;
 * - hyscan_gtk_mark_manager_icon_copy () - копирование Активной иконки;
 * - hyscan_gtk_mark_manager_icon_get_tooltips () - получение списка подсказок для иконок;
 * - hyscan_gtk_mark_manager_icon_get_icon () - получение общей иконки;
 * - hyscan_gtk_mark_manager_icon_add () - добавление данных.
 *
 */
#include <hyscan-gtk-mark-manager-icon.h>

/**
 * hyscan_gtk_mark_manager_icon_get_type:
 *
 * Регистрирует новоый тип Иконка для объектов Журнала Меток.
 */
GType
hyscan_gtk_mark_manager_icon_get_type (void)
{
  static GType hyscan_gtk_mark_manager_icon_type = 0;
  /* Регистрация нового типа (только при первом вызове). */
  if (hyscan_gtk_mark_manager_icon_type == 0)
    {
      hyscan_gtk_mark_manager_icon_type = g_boxed_type_register_static ("HyScanGtkMarkManagerIcon",
                                            (GBoxedCopyFunc) hyscan_gtk_mark_manager_icon_copy,
                                            (GBoxedFreeFunc) hyscan_gtk_mark_manager_icon_free);
    }
  return hyscan_gtk_mark_manager_icon_type;
}

/**
 * hyscan_gtk_mark_manager_icon_new:
 * @self: указатель на иконку
 * @icon: картинка, используемая в качестве иконки
 * @tooltip: текст всплывающей подсказки
 *
 * Returns: создаёт новый объект #HyScanMarkManagerIcon или
 * освобождает уже созданный и заполняет его новыми
 * данными. Когда иконка больше не нужна, необходимо
 * использовать #hyscan_gtk_mark_manager_icon_free ().
 */
HyScanGtkMarkManagerIcon*
hyscan_gtk_mark_manager_icon_new (HyScanGtkMarkManagerIcon *self,
                                  GdkPixbuf                *icon,
                                  const gchar              *tooltip)
{
  if (self == NULL)
    self = g_new0 (HyScanGtkMarkManagerIcon, 1);
  else
    hyscan_gtk_mark_manager_icon_free (self);

  self->data = g_new0 (HyScanGtkMarkManagerIconData, 2);
  /*self->data[0].icon    = gdk_pixbuf_copy (icon);*/
  self->data[0].icon    = (GdkPixbuf*)icon;
  self->data[0].tooltip = g_strdup (tooltip);

  self->length = 1;

  return self;
}

/**
 * hyscan_gtk_mark_manager_icon_free:
 * @self: указатель на иконку
 *
 * Удаляет иконку.
 */
void
hyscan_gtk_mark_manager_icon_free (HyScanGtkMarkManagerIcon *self)
{
  if (self == NULL)
    return;

  for (guint index = 0; index < self->length; index++)
    {
      g_object_unref (self->data[index].icon);
      g_free (self->data[index].tooltip);
      self->data[index].icon    = NULL;
      self->data[index].tooltip = NULL;
    }

  g_free (self->data);
  self->data   = NULL;
  self->length = 0;
}

/**
 * hyscan_gtk_mark_manager_icon_copy:
 * @self: указатель на иконку
 *
 * Returns: копию иконки. Когда иконка больше не нужна, необходимо
 * использовать #hyscan_gtk_mark_manager_icon_free ().
 */
HyScanGtkMarkManagerIcon*
hyscan_gtk_mark_manager_icon_copy (HyScanGtkMarkManagerIcon *self)
{
  HyScanGtkMarkManagerIcon *copy;

  if (self == NULL)
    return NULL;

  copy = g_new0 (HyScanGtkMarkManagerIcon, 1);
  copy->length = self->length;
  copy->data = g_new0 (HyScanGtkMarkManagerIconData, copy->length);

  for (guint index = 0; index < self->length; index++)
    {
      copy->data[index].icon    = gdk_pixbuf_copy (self->data[index].icon);
      copy->data[index].tooltip = g_strdup (self->data[index].tooltip);
    }

  return copy;
}

/**
 * hyscan_gtk_mark_manager_icon_get_tooltips:
 * @self: указатель на иконку
 *
 * Returns: NULL-терминированный список подсказок для иконок.
 * Когда список больше не нужен, необходимо использовать #g_strfreev ().
 */
GStrv
hyscan_gtk_mark_manager_icon_get_tooltips (HyScanGtkMarkManagerIcon *self)
{
  GStrv tooltips;

  if (self == NULL)
    return NULL;

  tooltips = g_new0 (gchar*, self->length + 1);

  for (guint index = 0; index < self->length; index++)
    tooltips[index] = g_strdup (self->data[index].tooltip);

  return tooltips;
}

/**
 * hyscan_gtk_mark_manager_icon_get_icon:
 * @self: указатель на иконку
 *
 * Returns: иконку. Когда иконка больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
GdkPixbuf*
hyscan_gtk_mark_manager_icon_get_icon (HyScanGtkMarkManagerIcon *self)
{
  GdkPixbuf *icon;

  if (self == NULL)
    return NULL;

  icon = gdk_pixbuf_copy (self->data[0].icon);

  for (guint index = 1; index < self->length; index++)
    {
      cairo_t         *cr;
      cairo_surface_t *surface;
      cairo_format_t   format;
      gint             width;
      gint             height;

      format = (gdk_pixbuf_get_has_alpha (icon)) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
      width  = gdk_pixbuf_get_width (icon) + gdk_pixbuf_get_width (self->data[index].icon);
      height = gdk_pixbuf_get_height (icon);

      surface = cairo_image_surface_create (format, width, height);

      cr = cairo_create (surface);

      gdk_cairo_set_source_pixbuf (cr, icon, 0, 0);
      cairo_paint (cr);

      gdk_cairo_set_source_pixbuf (cr, self->data[index].icon, gdk_pixbuf_get_width (icon), 0);
      cairo_paint (cr);

      icon = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);

      cairo_surface_destroy (surface);
      cairo_destroy (cr);
    }

  return icon;
}

/**
 * hyscan_gtk_mark_manager_icon_add:
 * @self: указатель на иконку
 * @icon: картинка, используемая в качестве иконки
 * @tooltip: текст всплывающей подсказки
 *
 * Returns: TRUE  - данные успешно добавлены,
 *          FALSE - данные не добавлены.
 */
gboolean
hyscan_gtk_mark_manager_icon_add (HyScanGtkMarkManagerIcon *self,
                                  GdkPixbuf                *icon,
                                  const gchar              *tooltip)
{
  if (self == NULL)
    return FALSE;

  if (icon == NULL)
    return FALSE;

  if (tooltip == NULL)
    return FALSE;

  self->data = (HyScanGtkMarkManagerIconData*)g_realloc (self->data,
                                                         (self->length + 2) * sizeof (HyScanGtkMarkManagerIcon));

  /*self->data[self->length].icon    = gdk_pixbuf_copy (icon);*/
  self->data[self->length].icon    = icon;
  self->data[self->length].tooltip = g_strdup (tooltip);

  self->length++;

  return TRUE;
}
