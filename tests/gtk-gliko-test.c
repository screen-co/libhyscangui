/*
Программа тестирования индикатора кругового обзора

Параметры командной строки для просмотра данных, предварительно
сгенерированных программой "gen-gliko-test-data":

./gtk-gliko-test -d file:///tmp/ko -p testko -t testko -n 1024


Управление параметрами просмотра в программе:

- изменение масштаба - вращение колесика мыши;
- исходный масштаб/центр - нажатие колесика мыши;
- изменение центра - левая кнопка мыши + перемещение;
- изменение контрастности - клавиша "Ctrl" + вращение колесика мыши;
- исходная контрастность - клавиша "Ctrl" + нажатие колесика мыши
- изменение яркости - клавиша "Shift" + вращение колесика мыши;
- исходная яркость - клавиша "Shift" + нажатие колесика мыши.
- изменение нелинейности - одновременное нажатие клавиш "Ctrl" и "Shift" + вращение колесика мыши;
- сброс нелинейности - одновременное нажатие клавиш "Ctrl" и "Shift" + нажатие колесика мыши.
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <hyscan-acoustic-data.h>
#include <hyscan-data-player.h>
#include <hyscan-gtk-gliko.h>
#include <hyscan-nmea-data.h>

static GtkWidget *window;
static GtkWidget *overlay;
static GtkWidget *gliko;
static GtkWidget *label;

static HyScanDB *db = NULL;
HyScanNMEAData *nmea_data = NULL;
static HyScanDataPlayer *player;

static gint32 num_azimuthes = 1024;

static int center_move = 0;
static gdouble sxc0 = 0.f, syc0 = 0.f;
static gdouble mx0 = 0.f, my0 = 0.f;
static gdouble delta = 0.01f;

static void
update_title ()
{
  gdouble b, c, g;
  gchar t[256];
  gchar q[256];

  b = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (gliko));
  c = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (gliko));
  g = hyscan_gtk_gliko_get_gamma_value (HYSCAN_GTK_GLIKO (gliko));

  sprintf (t, "B%.2lf C%.2lf G%.2lf", b, c, g);
  gtk_window_set_title (GTK_WINDOW (window), t);

  //sprintf( q, "<b><span font='20' background='#ffffff' foreground='#404040'>%s</span></b>", t );
  sprintf (q, "<b><span font='20' foreground='#FFFFFF'>%s</span></b>", t);
  gtk_label_set_text (GTK_LABEL (label), q);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
}

static void
button_cb (GtkWidget *widget, GdkEventButton *event, void *w)
{
  if (event->button == 1)
    {
      if (event->type == GDK_BUTTON_PRESS)
        {
          center_move = 1;
          hyscan_gtk_gliko_get_center (HYSCAN_GTK_GLIKO (gliko), &sxc0, &syc0);
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
      if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
        {
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), 0.0);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), 0.0);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), 1.0);
          update_title ();
        }
      else
        {
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), 1.0);
          hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (gliko), 0.0, 0.0);
        }
    }
}

static void
motion_cb (GtkWidget *widget, GdkEventMotion *event, void *w)
{
  GtkAllocation allocation;
  int baseline;
  gdouble s, x, y;

  gtk_widget_get_allocated_size (GTK_WIDGET (gliko), &allocation, &baseline);

  if (center_move)
    {
      s = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
      x = sxc0 - s * (mx0 - (gdouble) event->x) / allocation.height;
      y = syc0 - s * ((gdouble) event->y - my0) / allocation.height;
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (gliko), x, y);
    }
}

static void
scroll_cb (GtkWidget *widget, GdkEventScroll *event, void *w)
{
  gdouble f;

  if (event->direction == GDK_SCROLL_UP)
    {
      if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
        {
          f = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (gliko));
          f += delta;
          if (f > 1.0)
            {
              f = 1.0;
            }
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), f);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), f + 5.0 * delta);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          f = hyscan_gtk_gliko_get_gamma_value (HYSCAN_GTK_GLIKO (gliko));
          f -= 0.01;
          if (f < 0.0)
            {
              f = 0.0;
            }
          hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), f);
          update_title ();
        }
      else
        {
          f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), 0.875 * f);
        }
    }
  if (event->direction == GDK_SCROLL_DOWN)
    {
      if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
        {
          f = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (gliko));
          f -= delta;
          if (f < -1.0)
            {
              f = -1.0;
            }
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), f);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), f - 5.0 * delta);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          f = hyscan_gtk_gliko_get_gamma_value (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), f + 0.01);
          update_title ();
        }
      else
        {
          f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), 1.125 * f);
          update_title ();
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
  gchar *source_name1 = "ss-starboard";
  gchar *source_name2 = "ss-port";
  gchar *output_file = NULL;
  gdouble speed = 1.0;
  gdouble white_point = 1.0;
  gdouble black_point = 0.0;
  gdouble gamma_value = 1.0;
  gint32 angular_source = 1;
  guint32 color1 = 0xFF00FF80;
  guint32 color2 = 0xFFFF8000;
  guint32 background = 0x00404040;

  guint32 fps = 25;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
        {
          { "db", 'd', 0, G_OPTION_ARG_STRING, &db_uri, "DB uri", NULL },
          { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
          { "angular", 'a', 0, G_OPTION_ARG_INT, &angular_source, "Angular source channel's number (default 1)", NULL },
          { "track", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track name", NULL },
          { "starboard", 's', 0, G_OPTION_ARG_STRING, &source_name1, "Starboard name", NULL },
          { "port", 'r', 0, G_OPTION_ARG_STRING, &source_name2, "Port name", NULL },
          { "output", 'o', 0, G_OPTION_ARG_STRING, &output_file, "Output image file name", NULL },
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

  white_point = CLAMP (white_point, 0.0, 1.0);
  black_point = CLAMP (black_point, 0.0, 1.0);
  gamma_value = CLAMP (gamma_value, 0.0, 2.0);

  //delta = 0.1 * (white_point - black_point);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  overlay = gtk_overlay_new ();
  label = gtk_label_new ("");
  gliko = hyscan_gtk_gliko_new ();

  gtk_container_add (GTK_CONTAINER (window), overlay);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), gliko);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);

  gtk_widget_set_events (overlay,
                         GDK_EXPOSURE_MASK |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_BUTTON_MOTION_MASK |
                             GDK_SCROLL_MASK);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (overlay, "button_press_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (overlay, "button_release_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (overlay, "motion_notify_event", G_CALLBACK (motion_cb), NULL);
  g_signal_connect (overlay, "scroll_event", G_CALLBACK (scroll_cb), NULL);

  /* Подключение к базе данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    {
      g_error ("can't connect to db '%s'", db_uri);
      return EXIT_FAILURE;
    }

  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 0, &color1, 1, background);
  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 1, &color2, 1, background);

  hyscan_gtk_gliko_set_angular_source (HYSCAN_GTK_GLIKO (gliko), angular_source);
  hyscan_gtk_gliko_set_source_name (HYSCAN_GTK_GLIKO (gliko), 0, source_name1);
  hyscan_gtk_gliko_set_source_name (HYSCAN_GTK_GLIKO (gliko), 1, source_name2);
  hyscan_gtk_gliko_set_rotation (HYSCAN_GTK_GLIKO (gliko), 0.0);

  player = hyscan_data_player_new ();

  hyscan_gtk_gliko_set_player (HYSCAN_GTK_GLIKO (gliko), player);

  hyscan_gtk_gliko_set_black_point (HYSCAN_GTK_GLIKO (gliko), black_point);
  hyscan_gtk_gliko_set_white_point (HYSCAN_GTK_GLIKO (gliko), white_point);
  hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), gamma_value);

  hyscan_data_player_set_fps (player, fps);
  hyscan_data_player_set_track (player, db, project_name, track_name);

  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);

  hyscan_data_player_play (player, speed);

  gtk_widget_show_all (window);

  update_title ();

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
