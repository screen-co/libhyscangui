#include "hyscan-gtk-map-kit.h"
#include <hyscan-gtk-map.h>
#include <hyscan-db.h>
#include <hyscan-nmea-file-device.h>
#include <hyscan-driver.h>
#include <hyscan-hw-connector.h>

#define GPS_SENSOR_NAME "nmea"
#define INI_GROUP       "planner"

static gchar   *db_uri;                        /* Ссылка на базу данных. */
static gchar   *project_name;                  /* Ссылка на базу данных. */
static gchar   *profile_dir;                   /* Путь к каталогу, где хранятся профили карты. */
static gchar   *mission_ini;                   /* Путь к файлу со списком запланированных галсов. */
static gchar   *origin;                        /* Координаты центра карты. */
static gdouble  delay_time;                    /* Время задержки навигационных данных. */
static gchar   *driver_path = NULL;
static gchar   *config_file = NULL;
static gchar   *hw_profile = NULL;


void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
}

static void
start_stop (GtkSwitch   *widget,
            GParamSpec  *pspec,
            HyScanSonar *sonar)
{
  gboolean active;

  active = gtk_switch_get_active (widget);

  if (active)
    {
      gchar *track_name;
      GDateTime *now;

      now = g_date_time_new_now_local ();
      track_name = g_strdup_printf ("%02d%02d%02d",
                                    g_date_time_get_hour (now),
                                    g_date_time_get_minute (now),
                                    g_date_time_get_second (now));
      hyscan_sonar_start (sonar, project_name, track_name, HYSCAN_TRACK_SURVEY);
      g_free (track_name);
    }
  else
    {
      hyscan_sonar_stop (sonar);
    }


}

static GtkWidget *
start_stop_switch_create (HyScanSonar *sonar)
{
  GtkWidget *rec_switch;

  rec_switch = gtk_switch_new ();

  g_signal_connect (rec_switch, "notify::active", G_CALLBACK (start_stop), sonar);

  return rec_switch;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;
  GtkWidget *grid;

  HyScanDB *db = NULL;
  HyScanHWConnector *connector = NULL;
  HyScanControl *control = NULL;
  HyScanGeoGeodetic center = {.lat = 55.571, .lon = 38.103};

  gchar **driver_paths = NULL;

  HyScanGtkMapKit *kit = NULL;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "driver-path", 'p', 0, G_OPTION_ARG_STRING, &driver_path,  "Paths to sonar drivers", NULL },
        { "hw-profile",  'w', 0, G_OPTION_ARG_STRING, &hw_profile,   "Hardware profile name", NULL },
        { "config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file,  "Path to configuration file", NULL },
        { "map-dir",     'd', 0, G_OPTION_ARG_STRING, &profile_dir,  "Path to dir with map profiles", NULL },
        { "db-uri",      'D', 0, G_OPTION_ARG_STRING, &db_uri,       "Database uri", NULL},
        { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL},
        { "origin",      '0', 0, G_OPTION_ARG_STRING, &origin,       "Map origin, lat,lon", NULL},
        { "mission",     'm', 0, G_OPTION_ARG_STRING, &mission_ini,  "Path to mission planner ini-file", NULL},
        { "nav-delay",     0, 0, G_OPTION_ARG_DOUBLE, &delay_time,   "Delay in navigation data to smooth real time track", NULL},
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message (error->message);
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

  /* Загрузка параметров из конфигурационного файла. */
  if (config_file != NULL)
    {
      GKeyFile *key_file;
      gchar *value;
      gdouble double_value;
      GError *error = NULL;

      key_file = g_key_file_new ();
      g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_NONE, NULL);

      value = g_key_file_get_string (key_file, INI_GROUP, "db-uri", NULL);
      if (value != NULL)
        db_uri = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "project", NULL);
      if (value != NULL)
        project_name = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "driver-path", NULL);
      if (value != NULL)
        driver_path = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "hw-profile", NULL);
      if (value != NULL)
        hw_profile = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "origin", NULL);
      if (value != NULL)
        origin = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "map-dir", NULL);
      if (value != NULL)
        profile_dir = value;

      value = g_key_file_get_string (key_file, INI_GROUP, "mission", NULL);
      if (value != NULL)
        mission_ini = value;

      double_value = g_key_file_get_double (key_file, INI_GROUP, "nav-delay", &error);
      if (error == NULL)
        delay_time = double_value;
      else
        g_clear_error (&error);
    }

  /* Подключение к устройству. */
  connector = hyscan_hw_connector_new ();
  driver_paths = g_strsplit (driver_path, ";", -1);
  hyscan_hw_connector_set_driver_paths (connector, (const gchar**)driver_paths);
  if (!hyscan_hw_connector_load_profile (connector, hw_profile))
    goto fail;

  control = hyscan_hw_connector_connect (connector);
  if (control == NULL)
    goto fail;
  hyscan_sensor_set_enable (HYSCAN_SENSOR (control), GPS_SENSOR_NAME, TRUE);

  /* База данных. */
  if (db_uri != NULL)
    {
      db = hyscan_db_new (db_uri);
      if (db == NULL)
        goto fail;

      hyscan_control_writer_set_db (control, db);
    }

  /* Grid-виджет. */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 0);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
  gtk_widget_set_margin_start (grid, 20);
  gtk_widget_set_margin_end (grid, 20);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 0);

  /* Добавляем виджет карты в окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 600);

  kit = hyscan_gtk_map_kit_new (&center, db);

  hyscan_gtk_map_kit_set_project (kit, project_name);
  hyscan_gtk_map_kit_load_profiles (kit, profile_dir);
  hyscan_gtk_map_kit_add_planner (kit, mission_ini);
  hyscan_gtk_map_kit_add_nav (kit, HYSCAN_SENSOR (control), GPS_SENSOR_NAME, delay_time);

  /* Левый столбец существует только при наличии подключения к бд.*/
  if (db != NULL)
    {
      GtkWidget *left_col;

      left_col = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add (GTK_CONTAINER (left_col), gtk_label_new ("Запись"));
      gtk_container_add (GTK_CONTAINER (left_col), start_stop_switch_create (HYSCAN_SONAR (control)));
      gtk_container_add (GTK_CONTAINER (left_col), kit->navigation);

      gtk_grid_attach (GTK_GRID (grid), left_col,        0, 0, 1, 1);
    }

  gtk_grid_attach (GTK_GRID (grid), kit->map,        1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), kit->control,    2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), kit->status_bar, 1, 1, 2, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

  gtk_widget_show_all (window);

  /* Main loop. */
  gtk_main ();

  /* Cleanup. */
fail:
  g_clear_object (&db);
  g_clear_pointer (&kit, hyscan_gtk_map_kit_free);

  return 0;
}
