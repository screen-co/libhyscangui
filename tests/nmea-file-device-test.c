/* nmea-device-virtual-test.c
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

#include <hyscan-nmea-file-device.h>
#include <hyscan-buffer.h>

#define DEVICE_NAME "nmea-test-device"

GCond cond;
GMutex mutex;
gboolean device_done;

/* Обработчик сигнала "sensor-data". */
void
on_data_received (HyScanNmeaFileDevice *device,
                  const gchar          *name,
                  HyScanSourceType      source,
                  gint64                time,
                  HyScanBuffer         *data,
                  guint                *count)
{
  const gchar *nmea_string;
  guint32 size;
  HyScanDataType data_type;

  nmea_string = hyscan_buffer_get (data, &data_type, &size);
  g_assert (data_type == HYSCAN_DATA_STRING);

  g_print ("%14" G_GINT64_FORMAT " %s", time, nmea_string);

  ++(*count);
}

/* Обработчик сигнала "finish". */
static void
on_finish (HyScanNmeaFileDevice *device,
           gpointer              user_data)
{
  g_mutex_lock (&mutex);
  device_done = TRUE;
  g_cond_signal (&cond);
  g_mutex_unlock (&mutex);
}

/* Создает виртуальный девайс и эмитит сигнал c NMEA строками раз в секунду. */
int main (int    argc,
          char **argv)
{
  HyScanNmeaFileDevice *device = NULL;

  const gchar *file_name;
  guint received_lines = 0;

  if (argc < 2)
    {
      g_print ("Usage: %s file_name\n", argv[0]);
      return -1;
    }

  g_cond_init (&cond);
  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);

  file_name = argv[1];
  device = hyscan_nmea_file_device_new (DEVICE_NAME, file_name);

  g_signal_connect (device, "sensor-data", G_CALLBACK (on_data_received), &received_lines);
  g_signal_connect (device, "finish", G_CALLBACK (on_finish), NULL);

  g_assert_true (hyscan_sensor_set_enable (HYSCAN_SENSOR (device), DEVICE_NAME, TRUE));

  while (!device_done)
    g_cond_wait (&cond, &mutex);
  g_mutex_unlock (&mutex);

  hyscan_device_disconnect (HYSCAN_DEVICE (device));
  g_object_unref (device);

  g_assert (received_lines > 0);

  return 0;
}
