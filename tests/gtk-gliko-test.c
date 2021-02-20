#include <hyscan-acoustic-data.h>
#include <hyscan-data-player.h>
#include <hyscan-gtk-gliko.h>
#include <hyscan-nmea-data.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *window;
static GtkWidget *gliko;

static HyScanDB *db = NULL;
HyScanNMEAData *nmea_data = NULL;
static HyScanDataPlayer *player;

static gint32 num_azimuthes = 1024;

static gdouble white_point = 1.0;
static gdouble black_point = 0.0;
static gdouble gamma_value = 1.0;

static int iko_update = 0;
static int grid_update = 0;

static int center_move = 0;
static gdouble sxc0 = 0.f, syc0 = 0.f;
static gdouble mx0 = 0.f, my0 = 0.f;

static void
button_cb (GtkWidget *widget, GdkEventButton *event, void *w)
{
  if (event->button == 1)
    {
      if (event->type == GDK_BUTTON_PRESS)
        {
          center_move = 1;
          hyscan_gtk_gliko_get_center (HYSCAN_GTK_GLIKO (widget), &sxc0, &syc0);
          mx0 = (gdouble) event->x;
          my0 = (gdouble) event->y;
        }
      else if (event->type == GDK_BUTTON_RELEASE)
        {
          center_move = 0;
        }
    }
  else if (event->button == 2)
    {
      if (event->state & GDK_CONTROL_MASK)
        {
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (widget), 0.0);
          iko_update = 1;
        }
      else if (event->state & GDK_SHIFT_MASK)
        {
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (widget), 0.0);
          iko_update = 1;
        }
      else
        {
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (widget), 1.0);
          hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (widget), 0.0, 0.0);
          grid_update = iko_update = 1;
        }
    }
}

static void
motion_cb (GtkWidget *widget, GdkEventMotion *event, void *w)
{
  GtkAllocation allocation;
  int baseline;
  gdouble s, x, y;

  gtk_widget_get_allocated_size (GTK_WIDGET (widget), &allocation, &baseline);

  if (center_move)
    {
      s = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (widget));
      x = sxc0 - s * (mx0 - (gdouble) event->x) / allocation.height;
      y = syc0 - s * ((gdouble) event->y - my0) / allocation.height;
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (widget), x, y);
      grid_update = iko_update = 1;
    }
}

static void
scroll_cb (GtkWidget *widget, GdkEventScroll *event, void *w)
{
  gdouble f;

  if (event->direction == GDK_SCROLL_UP)
    {
      if (event->state & GDK_CONTROL_MASK)
        {
          f = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (widget));
          f += 0.05;
          if (f > 0.99)
            f = 0.99;
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (widget), f);
          iko_update = 1;
        }
      else if (event->state & GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (widget));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (widget), f + 0.05);
          iko_update = 1;
        }
      else
        {
          f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (widget));
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (widget), 0.875 * f);
          grid_update = iko_update = 1;
        }
    }
  if (event->direction == GDK_SCROLL_DOWN)
    {
      if (event->state & GDK_CONTROL_MASK)
        {
          f = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (widget));
          f -= 0.05f;
          if (f < -0.99f)
            f = -0.99f;
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (widget), f);
          iko_update = 1;
        }
      else if (event->state & GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (widget));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (widget), f - 0.05);
          iko_update = 1;
        }
      else
        {
          f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (widget));
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (widget), 1.125 * f);
          grid_update = iko_update = 1;
        }
    }
}

int
main (int argc,
      char **argv)
{
  gchar *db_uri = NULL;
  gchar *project_name = NULL;
  gchar *track_name = NULL;
  //gchar *source_name1 = "ss-starboard";
  //gchar *source_name2 = "ss-port";
  gchar *output_file = NULL;
  gdouble along_scale = 1.0;
  gdouble across_scale = 0.04;
  gdouble speed = 1.0;

  guint32 fps = 25;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
        {
          { "db", 'd', 0, G_OPTION_ARG_STRING, &db_uri, "DB uri", NULL },
          { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
          { "track", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track name", NULL },
          //{ "starboard", 's', 0, G_OPTION_ARG_STRING, &source_name, "Starboard name", NULL },
          //{ "port", 'r', 0, G_OPTION_ARG_STRING, &source_name, "Port name", NULL },
          { "output", 'o', 0, G_OPTION_ARG_STRING, &output_file, "Output image file name", NULL },
          { "along-scale", 'l', 0, G_OPTION_ARG_DOUBLE, &along_scale, "Scale along track", NULL },
          { "across-scale", 'c', 0, G_OPTION_ARG_DOUBLE, &across_scale, "Scale across track", NULL },
          { "white-point", 'w', 0, G_OPTION_ARG_DOUBLE, &white_point, "White point", NULL },
          { "black-point", 'b', 0, G_OPTION_ARG_DOUBLE, &black_point, "Black point", NULL },
          { "gamma", 'g', 0, G_OPTION_ARG_DOUBLE, &gamma_value, "Gamma", NULL },
          { "fps", 'f', 0, G_OPTION_ARG_INT, &fps, "Time between signaller calls", NULL },
          { "speed", 'S', 0, G_OPTION_ARG_DOUBLE, &speed, "Play speed coeff", NULL },
          { "num-azimuthes", 'n', 0, G_OPTION_ARG_INT, &num_azimuthes, "Number of azimuthes", NULL },
          { NULL }
        };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    if (!gtk_init_check (&argc, &argv))
      {
        fputs ("Could not initialize GTK", stderr);
        return EXIT_FAILURE;
      }

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((db_uri == NULL) || (project_name == NULL) || (track_name == NULL) /*|| (source_name == NULL)*/)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  if (along_scale < 0.1)
    {
      along_scale = 0.1;
      g_warning ("along scale clamped to %f", along_scale);
    }

  if (across_scale < 0.01)
    {
      across_scale = 0.1;
      g_warning ("across scale clamped to %f", along_scale);
    }

  along_scale = CLAMP (along_scale, 0.1, 100.0);
  across_scale = CLAMP (across_scale, 0.01, 1.0);

  white_point = CLAMP (white_point, 0.0, 1.0);
  black_point = CLAMP (black_point, 0.0, 1.0);
  gamma_value = CLAMP (gamma_value, 0.0, 2.0);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gliko = hyscan_gtk_gliko_new ();
  gtk_container_add (GTK_CONTAINER (window), gliko);

  gtk_widget_set_events (gliko,
                         GDK_EXPOSURE_MASK |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_BUTTON_MOTION_MASK |
                             GDK_SCROLL_MASK);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (gliko, "button_press_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (gliko, "button_release_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (gliko, "motion_notify_event", G_CALLBACK (motion_cb), NULL);
  g_signal_connect (gliko, "scroll_event", G_CALLBACK (scroll_cb), NULL);

  /* Подключение к базе данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    {
      g_error ("can't connect to db '%s'", db_uri);
      return EXIT_FAILURE;
    }

  player = hyscan_data_player_new ();

  hyscan_gtk_gliko_set_player (HYSCAN_GTK_GLIKO (gliko), player);

  hyscan_data_player_set_fps (player, fps);
  hyscan_data_player_set_track (player, db, project_name, track_name);

  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);

  hyscan_data_player_play (player, speed);

  gtk_widget_show_all (window);

  // Enter GTK event loop:
  gtk_main ();

  //g_clear_object (&channel[0].acoustic_data);
  //g_clear_object (&channel[0].acoustic_data);
  g_clear_object (&db);

  g_free (db_uri);
  g_free (project_name);
  g_free (track_name);
  //g_free (source_name);

  return EXIT_SUCCESS;
}
