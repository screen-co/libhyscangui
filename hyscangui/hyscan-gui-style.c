/* hyscan-gui-style.c
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

#include "hyscan-gui-style.h"
#include <gtk/gtk.h>

/**
 * hyscan_gui_style_init:
 *
 * Функция регистрирует стили оформления hyscan-gui.css и иконки библиотеки HyScanGui.
 * Функцию следует вызывать при создании класса виджета, который использует
 * эти стили или иконки.
 *
 * Повторный вызов функции ничего не делает.
 */
void
hyscan_gui_style_init (void)
{
  static gboolean initialized = FALSE;

  GdkScreen *screen;
  GtkStyleProvider *themes_style_provider;

  if (initialized)
    return;

  screen = gdk_screen_get_default ();
  g_return_if_fail (screen != NULL);

  /* Подключаем стили. */
  themes_style_provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_style_context_add_provider_for_screen (screen, themes_style_provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (themes_style_provider), "/org/hyscan/gtk/hyscan-gui.css");
  g_object_unref(themes_style_provider);

  /* Подключаем иконки. */
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default(), "/org/hyscan/icons");

  initialized = TRUE;
}
