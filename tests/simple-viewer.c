#include <hyscan-location.h>
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>

#define max(x,y) ((x) > (y)) ? (x) : (y)
#define min(x,y) ((x) < (y)) ? (x) : (y)

HyScanTile slant_no_correction (HyScanDB         *db,
                                HyScanLocation   *location,
                                gchar            *project,
                                gchar            *track,
                                gint              board,
                                gboolean          raw,
                                gfloat          **buffer,
                                gint32           *buffer_size,
                                gboolean          show_dpt);
int
main (int argc, char **argv)
{
  cairo_surface_t *surface = NULL;
  GError *error = NULL;

  /*переменные бд*/
  HyScanCache   *cache;
  HyScanDB      *db = NULL;
  //gint32         project_id = 0;
  //gint32         track_id = 0;
  gchar         *db_uri = NULL,
                *project = NULL,
                *track = NULL;

  /*переменные обработки*/
  gdouble  ppi = 96,
           scale = 200,
           white = 0.25,
           gamma = 1.0,
           speed = 1.0;
  gint     board = 0;
  gboolean ground_dist = FALSE,
           show_dpt    = TRUE,
           use_raw_src = FALSE,
           geom        = FALSE;

  /* прочие */
  gchar  *filepath = NULL,
         *filename = NULL;
  gfloat *buffer;
  gint32  buffer_size;
  HyScanTileColor *color;
  HyScanTile tile;
  HyScanTileSurface tile_surface;
  HyScanLocation *location;
  HyScanLocationSources **list;
  HyScanLocationSourceTypes lsrc;
  gint i;

  /* Парсим аргументы*/
  {
    gchar **args;
    error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "output-path",      'o',  0,   G_OPTION_ARG_STRING,   &filepath,        "Path to output file;",                       NULL},
      { "project",          'p',  0,   G_OPTION_ARG_STRING,   &project,         "Project name;",                              NULL},
      { "track-name",       't',  0,   G_OPTION_ARG_STRING,   &track,           "Track name;",                                NULL},
      { "raw-source",       'a',  0,   G_OPTION_ARG_INT,      &use_raw_src,     "1 to use raw source, otherwise 0;",          NULL},
      { "speed",            's',  0,   G_OPTION_ARG_DOUBLE,   &speed,           "Speed of ship;",                             NULL},
      { "ppi",              'r',  0,   G_OPTION_ARG_DOUBLE,   &ppi,             "Screen PPI;",                                NULL},
      { "scale",            'm',  0,   G_OPTION_ARG_DOUBLE,   &scale,           "Scale (1:[your value]);",                    NULL},
      { "white-point",      'w',  0,   G_OPTION_ARG_DOUBLE,   &white,           "white point (0.0-1.0);",                     NULL},
      { "gamma",            'g',  0,   G_OPTION_ARG_DOUBLE,   &gamma,           "gamma correction (0.1 - 10.0);",             NULL},
      { "board",            'b',  0,   G_OPTION_ARG_INT,      &board,           "board (0 - echo, 1 - ss-star, 2 - ss-port)", NULL},
      { "ground-dist",      'v',  0,   G_OPTION_ARG_NONE,     &ground_dist,     "draw ground distance (default slant)",       NULL},
      { "show-depth",       'c',  0,   G_OPTION_ARG_NONE,     &show_dpt,        "do draw depth line",                         NULL},
      { "geom-correction",  'z',  0,   G_OPTION_ARG_NONE,     &geom,            "do geometric correction",                    NULL},
      {NULL}
    };

    #ifdef G_OS_WIN32
        args = g_win32_get_command_line ();
    #else
        args = g_strdupv (argv);
    #endif

    context = g_option_context_new ("<db-uri>\n\nSimple data viewer. Generates a full picture of a specified track and board.");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Открываем БД. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Список КД в этом галсе. *//*
  gchar **channel_list = hyscan_db_channel_list (db, track_id);
  for (i = 0; channel_list[i] != NULL; i++)
      g_printf ("channel %i: %s\n", i, channel_list[i]);*/

  /* Расчехляем кэш, колор и локейшен. */
  cache = HYSCAN_CACHE (hyscan_cached_new (700));
  color = hyscan_tile_color_new (db_uri, project, track, NULL, NULL);
  hyscan_tile_color_set_levels (color, 0, gamma, white);
  location = hyscan_location_new (db, NULL, NULL, project, track, NULL, 1);
  list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_DEPTH);

  switch (board)
    {
      case 2:
        lsrc = HYSCAN_LOCATION_SOURCE_SONAR_PORT;
        filename = g_strdup_printf ("%s%s.%s.port.png", filepath, project, track);
        break;
      case 1:
        lsrc = HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD;
        filename = g_strdup_printf ("%s%s.%s.star.png", filepath, project, track);
        break;
      case 0:
      default:
        lsrc = HYSCAN_LOCATION_SOURCE_ECHOSOUNDER;
        filename = g_strdup_printf ("%s%s.%s.echo.png", filepath, project, track);
    }

  /* Устанавливаем источник. */
  for (i = 0; list[i] != NULL; i++)
    {
      if (list[i]->source_type == lsrc)
        hyscan_location_source_set (location, list[i]->index, TRUE);
    }
  hyscan_location_source_list_free (&list);

  /* Теперь определяем, что у нас, собственно говоря, за задача. */
  if (!ground_dist && !geom)
    tile = slant_no_correction (db, location, project, track, board, use_raw_src, &buffer, &buffer_size, show_dpt);
  else
    ;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile.w, tile.h);
  tile_surface.width = cairo_image_surface_get_width (surface);
  tile_surface.height = cairo_image_surface_get_height (surface);
  tile_surface.stride = cairo_image_surface_get_stride (surface);
  tile_surface.data = cairo_image_surface_get_data (surface);

  hyscan_tile_color_add (color, &tile, buffer, buffer_size, &tile_surface);

  cairo_surface_write_to_png (surface, filename);
  cairo_surface_destroy (surface);
  g_message ("Image generated at: %s\n", filename);
  g_free (filename);
  g_free (buffer);

  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&color);
  g_clear_object (&location);

  return 0;
}

HyScanTile
slant_no_correction (HyScanDB         *db,
                     HyScanLocation   *location,
                     gchar            *project,
                     gchar            *track,
                     gint              board,
                     gboolean          raw,
                     gfloat          **buffer,
                     gint32           *buffer_size,
                     gboolean          show_dpt)
{
  HyScanRawData *dchannel;
  HyScanLocationData loca;
  HyScanRawDataInfo dchannel_info;
  gint64 itime;
  HyScanTile     tile = {0};
  const gchar   *channel_name;
  guint32        lindex,
                 rindex;
  guint32 vcount, vmaxcount;
  gfloat  *values;
  guint32 iu, ju;

  switch (board)
    {
      case 2:
        channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_SIDE_SCAN_PORT, raw, 1);
        dchannel = hyscan_raw_data_new (db, project, track, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1);
        break;
      case 1:
        channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, raw, 1);
        dchannel = hyscan_raw_data_new (db, project, track, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);
        break;
      case 0:
      default:
        channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_ECHOSOUNDER, raw, 1);
        dchannel = hyscan_raw_data_new (db, project, track, HYSCAN_SOURCE_ECHOSOUNDER, 1);
    }

  /* Открываем КД. */
  dchannel_info = hyscan_raw_data_get_info (dchannel);

  /* Смотрим, сколько всего есть данных.*/
  if (!hyscan_raw_data_get_range	(dchannel, &lindex, &rindex))
    return tile;
  g_message ("range: %i %i", lindex, rindex);
  /* Теперь смотрим, какая строка самая длинная. */
  vmaxcount = hyscan_raw_data_get_values_count (dchannel, rindex);
  for (iu = lindex; iu < rindex; iu++)
    {
      vcount = hyscan_raw_data_get_values_count (dchannel, iu);
      vmaxcount = (vcount > vmaxcount) ? vcount : vmaxcount;
    }

  tile.w = vmaxcount;
  tile.h = (rindex - lindex + 1);

  /* Теперь создаем массив и шлепаем в него строки. */
  *buffer_size = tile.w * tile.h;
  *buffer = g_malloc0 (*buffer_size * sizeof (gfloat));
  values = g_malloc0 (vmaxcount * sizeof (gfloat));

  for (iu = 0; iu < rindex - lindex + 1; iu++)
    {
      vcount = vmaxcount;
      hyscan_raw_data_get_amplitude_values (dchannel, iu+lindex, values, &vcount, NULL);
      for (ju = 0; ju < vcount; ju++)
        (*buffer)[iu*vmaxcount + ju] = values[ju];
      for (; ju < vmaxcount; ju++)
        (*buffer)[iu*vmaxcount + ju] = -1.0;
      if (show_dpt)
        {
          itime = hyscan_raw_data_get_time(dchannel, iu+lindex);
          loca = hyscan_location_get (location, HYSCAN_LOCATION_PARAMETER_DEPTH, itime, 0, 0, 0, 0, 0, 0, 0);
          (*buffer)[iu*vmaxcount + (gint)round (loca.depth * dchannel_info.data.rate / 750.0)] = 1.0;
        }
    }

  g_clear_object (&dchannel);
  g_free (values);
  return tile;
}
