#include <hyscan-gtk-map.h>
#include <hyscan-nmea-file-device.h>
#include <hyscan-driver.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <hyscan-gtk-area.h>
#include <hyscan-control.h>
#include <hyscan-control-model.h>
#include <hyscan-gtk-map-builder.h>
#include <hyscan-cached.h>

#define GPS_SENSOR_NAME "my-nmea-sensor"

#ifndef HYSCAN_LOCALEDIR
#define HYSCAN_LOCALEDIR "./locale"
#endif

static gchar *db_uri;                        /* Ссылка на базу данных. */
static gchar *project_name;                  /* Ссылка на базу данных. */
static gchar *track_file;                    /* Путь к файлу с NMEA-строками. */
static gchar *origin;                        /* Координаты центра карты. */
static gchar *udp_host;                      /* Хост для подключения к GPS-приемнику. */
static gint udp_port;                        /* Порт для подключения к GPS-приемнику. */
static gdouble delay_time;                   /* Время задержки навигационных данных. */

void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
}

/* Подключается к GPS-приемнику по UDP. */
HyScanDevice *
load_nmea_udp_device (const gchar *nmea_udp_host,
                      gint         nmea_udp_port)
{
  const gchar *uri = "nmea://udp";

  HyScanDevice *device;

  HyScanDriver *nmea_driver;
  HyScanDataSchema *connect;
  HyScanParamList *params;

  const gchar *address_enum_id;
  GList *addresses, *address;

  nmea_driver = hyscan_driver_new (".", "nmea");
  if (nmea_driver == NULL)
    return NULL;

  /* Устанавливаем параметры подключения.  */
  params = hyscan_param_list_new ();

  /* 1. Адрес. */
  connect = hyscan_discover_config (HYSCAN_DISCOVER (nmea_driver), uri);
  address_enum_id = hyscan_data_schema_key_get_enum_id (connect, "/udp/address");
  addresses = address = hyscan_data_schema_enum_get_values (connect, address_enum_id);
  while (address != NULL)
    {
      HyScanDataSchemaEnumValue *value = address->data;

      if (g_strcmp0 (value->name, nmea_udp_host) == 0)
        hyscan_param_list_set_enum (params, "/udp/address", value->value);

      address = g_list_next (address);
    }
  g_list_free (addresses);

  /* 2. Порт. */
  if (nmea_udp_port != 0)
    hyscan_param_list_set_integer (params, "/udp/port", nmea_udp_port);

  /* 3. Имя датчика. */
  hyscan_param_list_set_string (params, "/dev-id", GPS_SENSOR_NAME);

  g_message ("Connecting to %s:%d", nmea_udp_host, nmea_udp_port);

  device = hyscan_discover_connect (HYSCAN_DISCOVER (nmea_driver), uri, params);
  if (device == NULL)
    {
      g_warning ("NMEA device: connection failed");
    }
  else if (!HYSCAN_IS_SENSOR (device))
    {
      g_clear_object (&device);
      g_warning ("NMEA device is not a HyScanSensor");
    }

  g_object_unref (connect);
  g_object_unref (params);
  g_object_unref (nmea_driver);

  return device;
}

/* Объект управления датчиками. */
HyScanControlModel *
create_control (HyScanDB *db)
{
  HyScanControl *control;
  HyScanControlModel *control_model;
  HyScanDevice *device = NULL;

  if (track_file != NULL)
    device = HYSCAN_DEVICE (hyscan_nmea_file_device_new (GPS_SENSOR_NAME, track_file));
  else if (udp_host != NULL && udp_port > 0)
    device = load_nmea_udp_device (udp_host, udp_port);

  if (device == NULL)
    return NULL;

  control = hyscan_control_new ();
  hyscan_control_device_add (control, device);
  hyscan_control_device_bind (control);
  hyscan_control_writer_set_db (control, db);

  hyscan_sensor_set_enable (HYSCAN_SENSOR (control), GPS_SENSOR_NAME, TRUE);

  control_model = hyscan_control_model_new (control);

  g_object_unref (device);
  g_object_unref (control);

  return control_model;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;

  HyScanDB *db = NULL;
  HyScanControlModel *sonar;
  HyScanGeoPoint center = {.lat = 55.571, .lon = 38.103};
  HyScanCache *cache;
  HyScanGtkModelManager *model_manager;

  HyScanGtkMapBuilder *map_builder;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "udp-host",        'h', 0, G_OPTION_ARG_STRING, &udp_host,          "Listen to UDP host", NULL},
        { "udp-port",        'P', 0, G_OPTION_ARG_INT,    &udp_port,          "Listen to UDP port", NULL},
        { "track-file",      't', 0, G_OPTION_ARG_STRING, &track_file,        "GPS-track file with NMEA-sentences", NULL },
        { "db-uri",          'D', 0, G_OPTION_ARG_STRING, &db_uri,            "Database uri", NULL},
        { "project-name",    'p', 0, G_OPTION_ARG_STRING, &project_name,      "Project name", NULL},
        { "origin",          '0', 0, G_OPTION_ARG_STRING, &origin,            "Map origin, lat,lon", NULL},
        { "delay",             0, 0, G_OPTION_ARG_DOUBLE, &delay_time,        "Delay in navigation data to smooth real time track", NULL},
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message ("%s", error->message);
        return -1;
      }

    if (db_uri == NULL)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    if (udp_host != NULL && track_file != NULL)
      {
        g_warning ("udp-host and track-file are mutually exclusive options.");
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    if (origin != NULL)
      {
        gdouble lat, lon;

        if (sscanf (origin, "%lf,%lf", &lat, &lon) == 2)
          {
            center.lat = lat;
            center.lon = lon;
          }
      }

    g_option_context_free (context);
  }

  /* Перевод. */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, HYSCAN_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Добавляем виджет карты в окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), project_name);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 600);

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Failed to connect to db: %s", db_uri);

  cache = HYSCAN_CACHE (hyscan_cached_new (255));
  model_manager = hyscan_gtk_model_manager_new (project_name, db, cache, NULL);
  map_builder = hyscan_gtk_map_builder_new (model_manager);

  sonar = create_control (db);
  if (sonar != NULL)
    hyscan_gtk_map_builder_add_nav (map_builder, sonar, FALSE);

  if (db != NULL)
    {
      hyscan_gtk_map_builder_add_tracks (map_builder);
      hyscan_gtk_map_builder_add_marks (map_builder);
      hyscan_gtk_map_builder_add_grid (map_builder);
      hyscan_gtk_map_builder_add_pin (map_builder);
      hyscan_gtk_map_builder_add_ruler (map_builder);
      hyscan_gtk_map_builder_add_planner (map_builder, TRUE);
      hyscan_gtk_map_builder_add_planner_list (map_builder);
    }

  hyscan_gtk_map_move_to (hyscan_gtk_map_builder_get_map (map_builder), center);

  /* Стоим интерфейс. */
  {
    GtkWidget *area;
    GtkWidget *map_box;

    area = hyscan_gtk_area_new ();
    gtk_widget_set_margin_top (area, 20);
    gtk_grid_set_column_spacing (GTK_GRID (area), 10);

    map_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_pack_start (GTK_BOX (map_box), hyscan_gtk_map_builder_get_central (map_builder), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (map_box), hyscan_gtk_map_builder_get_bar (map_builder), FALSE, TRUE, 0);

    hyscan_gtk_area_set_central (HYSCAN_GTK_AREA (area), map_box);
    hyscan_gtk_area_set_left (HYSCAN_GTK_AREA (area), hyscan_gtk_map_builder_get_left_col (map_builder));
    hyscan_gtk_area_set_right (HYSCAN_GTK_AREA (area), hyscan_gtk_map_builder_get_right_col (map_builder));

    gtk_container_add (GTK_CONTAINER (window), area);
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);
  }

  gtk_widget_show_all (window);

  /* Main loop. */
  gtk_main ();

  /* Cleanup. */
  g_clear_object (&cache);
  g_clear_object (&sonar);
  g_clear_object (&db);
  g_object_unref (map_builder);

  return 0;
}
