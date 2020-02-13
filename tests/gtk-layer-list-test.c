/* gtk-map-config-test.c
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

#include <hyscan-profile-map.h>
#include <hyscan-gtk-map-control.h>
#include <hyscan-gtk-param-tree.h>
#include <hyscan-data-schema-builder.h>
#include <hyscan-gtk-map-pin.h>
#include <hyscan-gtk-layer-list.h>
#include <hyscan-gtk-map-ruler.h>
#include <hyscan-gtk-map-base.h>
#include <hyscan-cached.h>

static GtkWidget *
create_tools (const gchar *label)
{
  GtkWidget *widget;

  widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (widget), gtk_label_new ("This is"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (widget), gtk_label_new ("a sample"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (widget), gtk_label_new (label), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (widget), gtk_label_new ("widget"), FALSE, FALSE, 0);

  return widget;
}

int
main (int    argc,
      char **argv)
{
  static GtkWidget *map;
  static HyScanProfileMap *profile;
  GtkWidget *layer_list, *window;
  HyScanGeoGeodetic center = { 59.9934843, 29.7034145, 0 };
  GtkWidget *paned;
  HyScanGtkLayer *pin, *control, *ruler, *base;
  HyScanCache *cache;

  gdouble *scales;
  gint scales_len;

  gtk_init (&argc, &argv);

  {
    gchar **args;

    #ifdef G_OS_WIN32
      args = g_win32_get_command_line ();
    #else
      args = g_strdupv (argv);
    #endif

    g_strfreev (args);
  }

  map = hyscan_gtk_map_new (center);
  gtk_widget_set_size_request (map, 300, -1);
  scales = hyscan_gtk_map_create_scales2 (0.01, 1000, 1, &scales_len);
  hyscan_gtk_map_set_scales_meter (HYSCAN_GTK_MAP (map), scales, scales_len);
  g_free (scales);

  cache = HYSCAN_CACHE (hyscan_cached_new (100));
  base = hyscan_gtk_map_base_new (cache);
  g_object_unref (cache);

  pin = hyscan_gtk_map_pin_new ();
  ruler = hyscan_gtk_map_ruler_new ();
  control = hyscan_gtk_map_control_new ();

  layer_list = hyscan_gtk_layer_list_new (HYSCAN_GTK_LAYER_CONTAINER (map));
  g_object_set (layer_list, "margin", 6, NULL);
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), base,    "base");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), control, "control");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), pin,     "pin");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), ruler,   "ruler");

  /* Добавляем элементы в список инструментов. */
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), base,  "base",  "Base map");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), NULL,  "tools", "Some settings");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), pin,   "pin",   "Pin");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), ruler, "ruler", "Ruler");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), NULL,  "empty", "Useless button");

  /* Для отдельных элементов добавляем настройки. */
  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (layer_list), "tools", create_tools ("settings"));
  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (layer_list), "ruler", create_tools ("ruler"));

  profile = hyscan_profile_map_new_default (NULL);
  hyscan_profile_read (HYSCAN_PROFILE (profile));
  hyscan_profile_map_apply (profile, HYSCAN_GTK_MAP (map), "base");
  hyscan_gtk_layer_set_visible (base, TRUE);

  paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_wide_handle (GTK_PANED (paned), TRUE);
  gtk_paned_add1 (GTK_PANED (paned), layer_list);
  gtk_paned_add2 (GTK_PANED (paned), map);

  /* Окно с картой. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  gtk_container_add (GTK_CONTAINER (window), paned);
  g_signal_connect_swapped (window, "destroy", G_CALLBACK (gtk_main_quit), window);

  gtk_widget_show_all (window);

  gtk_main ();

  g_object_unref (profile);
}
