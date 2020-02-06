#include <hyscan-data-writer.h>
#include "hyscan-gtk-map-kit.h"

typedef struct
{
  HyScanSourceType  source;           /* Источник данных. */
  guint             channel;          /* Номер канала. */
  gint32            write_channel_id; /* Идентификатор канала. */
  gint32            read_channel_id;  /* Идентификатор канала. */
  guint32           index;            /* Текущий индекс чтения данных. */
  gint64            time;             /* Метка времени для текущего индекса. */
  guint32           last;             /* Последний индекс. */
} Channel;

typedef struct
{
  HyScanDB         *db;                     /* База данных. */
  HyScanBuffer     *buffer;                 /* Буфер копирования данных. */
  gint64            first_record_time;      /* Время первой записи в каналах данных. */
  gint64            time_offset;            /* Разница между g_get_monotonic_time() и времени записи данных. */
  guint             tag;                    /* Тэг функции, которая копирует. */

  gint32            read_project_id;        /* Ид проекта для чтения. */
  gint32            read_track_id;          /* Ид галса для чтения. */
  gint32            write_project_id;       /* Ид проекта для записи. */
  gint32            write_track_id;         /* Ид галса для записи. */

  GArray           *channels;               /* Копируемые каналы данных. */
} DataTranslator;

/* Копирует данные канала channel до момента времени current_time. */
static void
copy_channel (DataTranslator *translator,
              gint64          current_time,
              Channel        *channel)
{
  if (channel->time > current_time || channel->index > channel->last)
    return;

  while (channel->time <= current_time)
    {
      gboolean status;

      status = hyscan_db_channel_get_data (translator->db,
                                           channel->read_channel_id, channel->index,
                                           translator->buffer, NULL);

      if (!status)
        g_error ("Failed to get data");

      if (!hyscan_db_channel_add_data (translator->db, channel->write_channel_id, channel->time, translator->buffer, NULL))
        g_error ("Failed to add data");

      if (++channel->index > channel->last)
        break;

      channel->time = hyscan_db_channel_get_data_time (translator->db, channel->read_channel_id, channel->index);
    }
}

gboolean
write_db (gpointer user_data)
{
  DataTranslator *translator = user_data;
  GArray *channels = translator->channels;
  guint i;

  gint64 current_time;

  /* Инициализируем начало отсчета. */
  if (translator->time_offset == 0)
    translator->time_offset = g_get_monotonic_time () - g_array_index (channels, Channel, 0).time;

  current_time = g_get_monotonic_time () - translator->time_offset;
  for (i = 0; i < channels->len; ++i)
    copy_channel (translator, current_time, &g_array_index (channels, Channel, i));

  return G_SOURCE_CONTINUE;
}

static void
close_channel (DataTranslator *translator,
               Channel        *channel)
{
  hyscan_db_close (translator->db, channel->write_channel_id);
  hyscan_db_close (translator->db, channel->read_channel_id);
}

static void
write_channel_params (DataTranslator  *translator,
                      gint32           channel_id,
                      HyScanParamList *param_list)
{
  gint32 write_param_id;

  write_param_id = hyscan_db_channel_param_open (translator->db, channel_id);
  hyscan_db_param_set (translator->db, write_param_id, NULL, param_list);
  hyscan_db_close (translator->db, write_param_id);
}

static HyScanParamList *
read_channel_params (DataTranslator   *translator,
                     gint32            channel_id,
                     gchar           **schema_id)
{
  HyScanParamList *param_list;
  gint32 read_param_id;
  HyScanDataSchema *schema;
  gint i;
  const gchar * const * keys;

  /* Считаываем параметры канала данных. */
  read_param_id = hyscan_db_channel_param_open (translator->db, channel_id);

  schema = hyscan_db_param_object_get_schema (translator->db, read_param_id, NULL);
  *schema_id = g_strdup (hyscan_data_schema_get_id (schema));

  if (schema == NULL)
    g_error ("Schema is NULL");
  param_list = hyscan_param_list_new ();
  keys = hyscan_data_schema_list_keys (schema);
  for (i = 0; keys[i] != NULL; ++i)
    {
      if (hyscan_data_schema_key_get_access (schema, keys[i]) & HYSCAN_DATA_SCHEMA_ACCESS_WRITE)
        hyscan_param_list_add (param_list, keys[i]);
    }

  hyscan_db_param_get (translator->db, read_param_id, NULL, param_list);

  hyscan_db_close (translator->db, read_param_id);

  return param_list;
}

static Channel
open_channel (DataTranslator *translator,
              const gchar    *channel_name)
{
  HyScanChannelType type;
  HyScanParamList *param_list;
  gchar *schema_id;
  Channel channel;

  hyscan_channel_get_types_by_id (channel_name, &channel.source, &type, &channel.channel);
  channel.read_channel_id = hyscan_db_channel_open (translator->db, translator->read_track_id, channel_name);
  hyscan_db_channel_get_data_range (translator->db, channel.read_channel_id, &channel.index, &channel.last);
  channel.time = hyscan_db_channel_get_data_time (translator->db, channel.read_channel_id, channel.index);

  /* Копируем параметры этого канала данных. */
  param_list = read_channel_params (translator, channel.read_channel_id, &schema_id);
  channel.write_channel_id = hyscan_db_channel_create (translator->db, translator->write_track_id,
                                                        channel_name, schema_id);
  write_channel_params (translator, channel.write_channel_id, param_list);

  g_free (schema_id);
  g_object_unref (param_list);

  return channel;
}

DataTranslator *
translator_new (HyScanDB    *db,
                const gchar *read_project_name,
                const gchar *write_project_name,
                const gchar *track_name)
{
  DataTranslator *translator;
  HyScanDataWriter *writer;
  gchar **channel_list, **channel_name;

  translator = g_new0 (DataTranslator, 1);
  translator->db = g_object_ref (db);
  translator->buffer = hyscan_buffer_new ();

  /* Создаём проект и галс через HyScanDataWriter. */
  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, translator->db);
  if (!hyscan_data_writer_start (writer, write_project_name, track_name, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error ("Can't start data writer. May be track with the name <%s> exists", track_name);
  hyscan_data_writer_stop (writer);
  g_object_unref (writer);

  /* Открываем галс для чтения. */
  translator->read_project_id = hyscan_db_project_open (translator->db, read_project_name);
  translator->read_track_id = hyscan_db_track_open (translator->db, translator->read_project_id, track_name);
  if (translator->read_track_id < 0)
    g_error ("Failed to open track \"%s\", project \"%s\"", track_name, read_project_name);

  /* Открываем галс для записи. */
  translator->write_project_id = hyscan_db_project_open (translator->db, write_project_name);
  translator->write_track_id = hyscan_db_track_open (translator->db, translator->write_project_id, track_name);
  if (translator->read_track_id < 0)
    g_error ("Failed to open track \"%s\", project \"%s\"", track_name, write_project_name);

  /* Открываем каналы данных. */
  channel_list = hyscan_db_channel_list (db, translator->read_track_id);
  translator->channels = g_array_new (FALSE, FALSE, sizeof (Channel));
  for (channel_name = channel_list; *channel_name != NULL; channel_name++)
    {
      Channel channel = open_channel (translator, *channel_name);
      g_array_append_val (translator->channels, channel);
    }
  g_strfreev (channel_list);

  /* Запускаем копирование данных в новый галс по таймеру. */
  translator->tag = g_timeout_add (100, write_db, translator);

  return translator;
}

static void
translator_free (DataTranslator *translator)
{
  guint i;

  g_source_remove (translator->tag);

  for (i = 0; i < translator->channels->len; ++i)
    close_channel (translator, &g_array_index (translator->channels, Channel, i));
  g_array_free (translator->channels, TRUE);

  hyscan_db_close (translator->db, translator->read_project_id);
  hyscan_db_close (translator->db, translator->read_track_id);
  g_object_unref (translator->db);
  g_object_unref (translator->buffer);
  g_free (translator);
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *grid;

  /* Параметры CLI. */
  gchar *db_uri = NULL;        /* Ссылка на базу данных. */
  gchar *project_read = NULL;  /* Проекты для чтения. */
  gchar *project_write = NULL; /* Проекты для записи. */
  gchar *track_name = NULL;    /* Имя галса. */
  gchar *profile_dir = NULL;   /* Путь к каталогу, где хранятся профили карты. */
  gchar *origin = NULL;        /* Координаты центра карты. */

  HyScanDB *db;
  HyScanUnits *units;
  DataTranslator *translator;
  HyScanGeoGeodetic center = {.lat = 55.571, .lon = 38.103};

  HyScanGtkMapKit *kit;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        {"map-profiles",  'm', 0, G_OPTION_ARG_STRING, &profile_dir,   "Path to dir with map profiles", NULL},
        {"db-uri",        'd', 0, G_OPTION_ARG_STRING, &db_uri,        "Database uri",                  NULL},
        {"project-read",  'r', 0, G_OPTION_ARG_STRING, &project_read,  "Source project name",           NULL},
        {"project-write", 'w', 0, G_OPTION_ARG_STRING, &project_write, "Destination project name",      NULL},
        {"track",         't', 0, G_OPTION_ARG_STRING, &track_name,    "Track name",                    NULL},
        {"origin",        '0', 0, G_OPTION_ARG_STRING, &origin,        "Map origin, lat,lon",           NULL},
        {NULL}
      };

    context = g_option_context_new ("");
    g_option_context_set_summary (context, "Test HyScanGtkMapTrack with modifying channel data.\n\n"
                                           "Program reads data from the source project and writes it to the \n"
                                           "destination. HyScanGtkMapTrack reads data from the destination project, \n"
                                           "thus simulating real-time write.");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message ("%s", error->message);
        return -1;
      }

    if (db_uri == NULL || track_name == NULL || project_read == NULL || project_write == NULL)
      {
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

  /* Окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  /* Grid-виджет. */
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 20);

  db = hyscan_db_new (db_uri);
  units = hyscan_units_new ();
  translator = translator_new (db, project_read, project_write, track_name);
  kit = hyscan_gtk_map_kit_new (&center, db, units, "/tmp/tile-cache");
  hyscan_gtk_map_kit_set_project (kit, project_write);
  hyscan_gtk_map_kit_load_profiles (kit, profile_dir);

  gtk_grid_attach (GTK_GRID (grid), kit->navigation, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), kit->map,        1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), kit->control,    2, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);

  gtk_widget_show_all (window);

  /* Main loop. */
  gtk_main ();

  /* Cleanup. */
  g_clear_object (&units);
  g_clear_object (&db);
  hyscan_gtk_map_kit_free (kit);
  translator_free (translator);

  return 0;
}
