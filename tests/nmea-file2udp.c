/* nmea-file2udp.c
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
#include <gio/gio.h>

#include <string.h>
#include <stdio.h>
#include <hyscan-buffer.h>

void
on_data_received (HyScanNmeaFileDevice *device,
                  const gchar          *name,
                  HyScanSourceType      source,
                  gint64                time,
                  HyScanBuffer         *data,
                  gpointer              user_data)
{
  const gchar *nmea;
  guint size;

  GSocket *socket = user_data;
  gdouble dtime = time / 1000000.0;

  nmea = hyscan_buffer_get (data, NULL, &size);
  g_socket_send (socket, nmea, size, NULL, NULL);

  g_print ("Send to UDP: time %.03fs\n%s\n", dtime, nmea);
}

int
main (int    argc,
      char **argv)
{
  GSocket *socket;
  GSocketAddress *address;
  HyScanNmeaFileDevice *device;

  gchar *host = NULL;
  gchar *filename = NULL;
  gint port = 0;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "filename", 'f', 0, G_OPTION_ARG_STRING, &filename,  "Path to nmea file",      NULL },
        { "host",     'h', 0, G_OPTION_ARG_STRING, &host,      "Destination ip address", NULL },
        { "port",     'p', 0, G_OPTION_ARG_INT,    &port,      "Destination udp port",   NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((filename == NULL) || (host == NULL) || (port < 1024) || (port > 65535))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  address = g_inet_socket_address_new_from_string (host, port);
  if (address == NULL)
    g_error ("unknown host address");

  socket = g_socket_new (g_socket_address_get_family (address),
                         G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         NULL);
  if (socket == NULL)
    g_error ("can't create socket");

  if (!g_socket_connect (socket, address, NULL, NULL))
    g_error ("can't connect to %s:%d", host, port);

  device = hyscan_nmea_file_device_new ("test-device", filename);
  hyscan_sensor_set_enable (HYSCAN_SENSOR (device), "test-device", TRUE);
  g_signal_connect (device, "sensor-data", G_CALLBACK (on_data_received), socket);

  g_message ("Press [Enter] to terminate test...");
  getchar ();

  g_object_unref (device);
  g_object_unref (socket);
  g_object_unref (address);

  return 0;
}
