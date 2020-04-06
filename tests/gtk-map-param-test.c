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
#include <hyscan-gtk-param-cc.h>
#include <hyscan-gtk-map-base.h>
#include <hyscan-cached.h>

static GtkWidget *map;
static HyScanProfileMap *profile;
static gchar *file_name;

static void
map_config_apply (HyScanGtkParam *param_tree)
{
  hyscan_gtk_param_apply (param_tree);

  if (file_name != NULL)
    hyscan_profile_map_write (profile, HYSCAN_GTK_MAP (map), file_name);
}

static GtkWidget *
map_config_new (void)
{
  GtkWidget *box, *gtk_param, *button, *abar, *window;
  HyScanParam *param;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  param = hyscan_gtk_layer_container_get_param (HYSCAN_GTK_LAYER_CONTAINER (map));
  gtk_param = hyscan_gtk_param_cc_new (param, "/", FALSE);
  button = gtk_button_new_with_label ("Apply");

  g_signal_connect_swapped (button, "clicked", map_config_apply, gtk_param);

  abar = gtk_action_bar_new ();
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), button);

  gtk_box_pack_start (GTK_BOX (box), gtk_param, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), abar, FALSE, TRUE, 0);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);

  g_object_unref (param);

  return window;
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *layer_list, *window, *config_window;
  HyScanGeoPoint center = { 59.9934843, 29.7034145 };
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

    if (argc == 2)
      file_name = g_strdup (args[1]);

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

  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), base,  "base",  "Base map");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), pin,   "pin",   "Pin");
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (layer_list), ruler, "ruler", "Ruler");

  if (file_name != NULL)
    profile = hyscan_profile_map_new (NULL, file_name);
  else
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

  /* Окно с настрйками стиля. */
  config_window = map_config_new ();
  gtk_window_set_default_size (GTK_WINDOW (config_window), 600, 500);
  gtk_window_set_transient_for (GTK_WINDOW (config_window), GTK_WINDOW (window));

  gtk_widget_show_all (config_window);
  gtk_widget_show_all (window);

  gtk_main ();

  g_object_unref (profile);
}
