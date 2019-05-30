/* navigation-model-test.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
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

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <hyscan-navigation-model.h>
#include <hyscan-nmea-file-device.h>

#define DEVICE_NAME "nav-device"

int usage (const gchar *prg_name)
{
  g_print ("Usage: %s filename\n", prg_name);
  return -1;
}

void
on_model_changed (HyScanNavigationModel *model,
                  gdouble                time,
                  HyScanGeoGeodetic     *geo)
{
  g_print ("Model changed: %12.2f sec: %10.6f, %10.6f\n", time, geo->lat, geo->lon);
}

int main (int    argc,
          char **argv)
{
  GMainLoop *loop;

  HyScanNmeaFileDevice *device;
  HyScanNavigationModel *model;

  const gchar *filename;

  if (argc < 2)
    return usage (argv[0]);

  filename = argv[1];

  loop = g_main_loop_new (NULL, FALSE);

  device = hyscan_nmea_file_device_new (DEVICE_NAME, filename);
  model = hyscan_navigation_model_new ();
  hyscan_navigation_model_set_sensor (model, HYSCAN_SENSOR (device));
  hyscan_navigation_model_set_sensor_name (model, DEVICE_NAME);

  g_signal_connect_swapped (device, "finish", G_CALLBACK (g_main_loop_quit), loop);
  g_signal_connect (model, "changed", G_CALLBACK (on_model_changed), NULL);

  hyscan_sensor_set_enable (HYSCAN_SENSOR (device), DEVICE_NAME, TRUE);

  g_main_loop_run (loop);

  hyscan_device_disconnect (HYSCAN_DEVICE (device));

  g_object_unref (device);
  g_object_unref (model);

  g_main_loop_unref (loop);

  return 0;
}
