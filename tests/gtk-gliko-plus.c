#include <hyscan-gtk-area.h>
#include <hyscan-tile-color.h>

#include <hyscan-acoustic-data.h>
#include <hyscan-data-player.h>
#include <hyscan-gtk-gliko-view.h>
#include <hyscan-nmea-data.h>

#include <glib.h>
#include <gtk/gtk.h>
//#include <libxml/parser.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef G_OS_WIN32
#define PARSE_SEPARATOR "\\"
#define COMPOSE_FORMAT "%s\\"
#else
#define PARSE_SEPARATOR "/"
#define COMPOSE_FORMAT "/%s"
#endif

enum
{
  POINTER_NONE = 0,
  POINTER_CENTER,
  POINTER_RULER_FROM,
  POINTER_RULER_TO,
  POINTER_RULER_LINE
};

static void destroy_cb (GtkWidget *object, gpointer user_data);
void reopen_clicked (GtkButton *button,
                     gpointer user_data);
void zoom_clicked (GtkButton *button,
                   gpointer user_data);
void scale_changed (GtkRange *range, gpointer user_data);
void player_changed (GtkRange *range, gpointer user_data);

static void pause_cb (GtkToggleButton *togglebutton, gpointer user_data);
static void x10_cb (GtkToggleButton *togglebutton, gpointer user_data);

void color_changed (GtkColorButton *chooser);
// void       player_changed   (GtkScale                 *player_scale,
// HyScanGtkWaterfallPlayer *player);
// void       player_stop      (HyScanGtkWaterfallPlayer *player,
// GtkScale                 *scale);*/

gchar *uri_composer (gchar **path,
                     gchar *prefix,
                     guint cut);

GtkWidget *make_menu (gdouble white,
                      gdouble gamma);
gboolean slant_ground (GtkSwitch *widget,
                       gboolean state,
                       gpointer udata);
void open_db (HyScanDB **db,
              gchar **old_uri,
              gchar *new_uri);

static void player_ready_cb (HyScanDataPlayer *player, gint64 time, gpointer user_data);
static void player_range_cb (HyScanDataPlayer *player, gint64 min, gint64 max, gpointer user_data);

static void ruler_changed_cb (HyScanGtkGlikoView *view, gdouble az, gdouble ar, gdouble bz, gdouble br);

static GtkWidget *scale_white;
static GtkWidget *scale_bright;
static GtkWidget *scale_contrast;
static GtkWidget *scale_gamma;
static GtkWidget *scale_balance;
static GtkWidget *scale_bottom;
static GtkWidget *scale_rotation;
static GtkWidget *scale_turn;
static GtkWidget *scale_player;
static GtkWidget *pause_button;
static GtkWidget *x10_button;

static GtkWidget *label_a;
static GtkWidget *label_b;
static GtkWidget *label_ab;
static GtkWidget *label_ba;

static HyScanDB *db;
static gchar *db_uri;
static gchar *project_dir;
static GtkWidget *window;

static GtkWidget *window;
static GtkWidget *gliko;

static HyScanDB *db = NULL;
HyScanNMEAData *nmea_data = NULL;
static HyScanDataPlayer *player;

static gint32 num_azimuthes = 1024;

static GtkWidget *area;

gdouble speed = 1.0;

gint64 player_min = 0;
int player_position_changed = 0;

int
main (int argc,
      char **argv)
{
  GtkWidget *left;
  GtkWidget *central;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gchar *source_name1 = "around-port";      // левый борт
  gchar *source_name2 = "around-starboard"; // правый борт
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  gdouble bottom = 0.0;
  gdouble speed = 1.0;
  gint32 angular_source = 3;
  guint32 color1 = 0xFF00FF80;
  guint32 color2 = 0xFFFF8000;
  guint32 background = 0x00404040;
  guint32 fps = 25;

  gtk_init (&argc, &argv);

  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
      { "track", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track name", NULL },
      { "angular", 'a', 0, G_OPTION_ARG_INT, &angular_source, "Angular source channel's number (default 3)", NULL },
      { "port", 'r', 0, G_OPTION_ARG_STRING, &source_name1, "Port data channel", NULL },
      { "starboard", 's', 0, G_OPTION_ARG_STRING, &source_name2, "Starboard data channel", NULL },
      { "bottom", 'b', 0, G_OPTION_ARG_DOUBLE, &bottom, "Bottom", NULL },
      { "white-point", 'W', 0, G_OPTION_ARG_DOUBLE, &white, "White point", NULL },
      { "gamma", 'g', 0, G_OPTION_ARG_DOUBLE, &gamma, "Gamma", NULL },
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

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) == 2)
      db_uri = g_strdup (args[1]);

    g_option_context_free (context);

    g_strfreev (args);
  }

  open_db (&db, &db_uri, db_uri);

  /* Основное окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_set_title (GTK_WINDOW (window), "Around+");
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  /* Кладем виджет в основное окно. */
  area = hyscan_gtk_area_new ();

  left = make_menu (white, gamma);
  hyscan_gtk_area_set_left (HYSCAN_GTK_AREA (area), left);

  gliko = GTK_WIDGET (hyscan_gtk_gliko_view_new ());
  central = gliko;

  hyscan_gtk_area_set_central (HYSCAN_GTK_AREA (area), central);

  gtk_container_add (GTK_CONTAINER (window), area);

  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 0, &color1, 1, background);
  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 1, &color2, 1, background);

  hyscan_gtk_gliko_set_angular_source (HYSCAN_GTK_GLIKO (gliko), angular_source);
  hyscan_gtk_gliko_set_source_name (HYSCAN_GTK_GLIKO (gliko), 0, source_name1);
  hyscan_gtk_gliko_set_source_name (HYSCAN_GTK_GLIKO (gliko), 1, source_name2);
  hyscan_gtk_gliko_set_rotation (HYSCAN_GTK_GLIKO (gliko), 0.0);
  hyscan_gtk_gliko_set_bottom (HYSCAN_GTK_GLIKO (gliko), bottom);

  player = hyscan_data_player_new ();

  g_object_ref (G_OBJECT (gliko));
  hyscan_gtk_gliko_set_player (HYSCAN_GTK_GLIKO (gliko), player);

  hyscan_gtk_gliko_set_white_point (HYSCAN_GTK_GLIKO (gliko), white);
  hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), gamma);

  hyscan_data_player_set_fps (player, fps);
  g_signal_connect (player, "range", G_CALLBACK (player_range_cb), NULL);
  g_signal_connect (player, "ready", G_CALLBACK (player_ready_cb), NULL);
  g_signal_connect (gliko, "ruler-changed", G_CALLBACK (ruler_changed_cb), NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), NULL);

  gtk_widget_show_all (window);

  if (db != NULL && project_name != NULL && track_name != NULL)
    {
      hyscan_data_player_set_track (player, db, project_name, track_name);
      hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_angular_source (HYSCAN_GTK_GLIKO (gliko)), 1, 0);
      //hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
      //hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);
      hyscan_data_player_play (player, speed);
    }

  /* Начинаем работу. */
  gtk_main ();

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);

  //xmlCleanupParser ();

  return 0;
}

static void
destroy_cb (GtkWidget *object, gpointer user_data)
{
  hyscan_data_player_shutdown (player);
  g_object_unref (G_OBJECT (gliko));
  g_object_unref (G_OBJECT (player));
  g_clear_object (&db);
}

GtkWidget *
make_menu (gdouble white,
           gdouble gamma)
{
  GtkWidget *w;
  // GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *zoom_btn_in = gtk_button_new_from_icon_name ("zoom-in-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *zoom_btn_out = gtk_button_new_from_icon_name ("zoom-out-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *btn_reopen = gtk_button_new_from_icon_name ("folder-symbolic", GTK_ICON_SIZE_BUTTON);
  pause_button = gtk_toggle_button_new ();
  x10_button = gtk_toggle_button_new_with_label ("x10");
  gtk_button_set_image (GTK_BUTTON (pause_button), gtk_image_new_from_icon_name ("media-playback-pause", GTK_ICON_SIZE_BUTTON));

  scale_white = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.001, 1.0, 0.001);
  scale_bright = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -1.0, 1.0, 0.01);
  scale_contrast = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -1.0, 1.0, 0.01);
  scale_gamma = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.1, 2.0, 0.005);
  scale_balance = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -1.0, 1.0, 0.01);
  scale_bottom = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 50.0, 0.01);
  scale_rotation = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -180, 180.0, 1.0);
  scale_turn = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -180, 180.0, 1.0);
  scale_player = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);

  GtkWidget *track_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *player_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  /* Делаем симпатичные кнопки. */
  gtk_style_context_add_class (gtk_widget_get_style_context (track_box), "linked");

  GtkWidget *color_chooser = gtk_color_button_new ();
  GdkRGBA rgba;

  gdk_rgba_parse (&rgba, "#00FFFF");
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_chooser), &rgba);

  /* Настраиваем прокрутки. */
  gtk_scale_set_value_pos (GTK_SCALE (scale_white), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_bright), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_contrast), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_balance), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_gamma), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_player), GTK_POS_TOP);
  gtk_widget_set_size_request (scale_white, 150, 1);
  gtk_widget_set_size_request (scale_bright, 150, 1);
  gtk_widget_set_size_request (scale_contrast, 150, 1);
  gtk_widget_set_size_request (scale_balance, 150, 1);
  gtk_widget_set_size_request (scale_gamma, 150, 1);
  gtk_widget_set_size_request (scale_player, 150, 1);
  gtk_range_set_value (GTK_RANGE (scale_white), white);
  gtk_range_set_value (GTK_RANGE (scale_bright), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_contrast), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_balance), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_gamma), gamma);
  gtk_range_set_value (GTK_RANGE (scale_rotation), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_turn), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_player), 0.0);

  gtk_scale_add_mark (GTK_SCALE (scale_player), 0.0, GTK_POS_TOP, NULL);
  gtk_scale_set_has_origin (GTK_SCALE (scale_player), FALSE);

  GtkWidget *ruler_grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (ruler_grid), 4);
  gtk_grid_attach (GTK_GRID (ruler_grid), gtk_label_new ("Измерение"), 0, 0, 2, 1);
  gtk_grid_attach (GTK_GRID (ruler_grid), w = gtk_label_new ("A"), 0, 1, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (ruler_grid), label_a = gtk_label_new (""), 1, 1, 1, 1);
  gtk_widget_set_halign (label_a, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (ruler_grid), w = gtk_label_new ("B"), 0, 2, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (ruler_grid), label_b = gtk_label_new (""), 1, 2, 1, 1);
  gtk_widget_set_halign (label_b, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (ruler_grid), w = gtk_label_new ("A→B"), 0, 3, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (ruler_grid), label_ab = gtk_label_new (""), 1, 3, 1, 1);
  gtk_widget_set_halign (label_b, GTK_ALIGN_END);
  gtk_grid_attach (GTK_GRID (ruler_grid), w = gtk_label_new ("B→A"), 0, 4, 1, 1);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (ruler_grid), label_ba = gtk_label_new (""), 1, 4, 1, 1);
  gtk_widget_set_halign (label_b, GTK_ALIGN_END);

  /* Кладём в коробку. */
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_in, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_out, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), btn_reopen, TRUE, TRUE, 2);

  gtk_box_pack_start (GTK_BOX (box), track_box, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Цвет"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), color_chooser, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Диапазон"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_white, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Яркость (Shift+Scroll)"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_bright, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Контраст (Ctrl+Scroll)"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_contrast, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Нелинейность"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_gamma, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Баланс"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_balance, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Дно"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_bottom, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Поворот развертки"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_rotation, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Поворот изображения"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_turn, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Плеер"), FALSE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (player_box), pause_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (player_box), x10_button, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (player_box), scale_player, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), player_box, FALSE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX (box), ruler_grid, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);

  g_signal_connect (btn_reopen, "clicked", G_CALLBACK (reopen_clicked), NULL);
  g_signal_connect (zoom_btn_in, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (1));
  g_signal_connect (zoom_btn_out, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (0));
  g_signal_connect (scale_gamma, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_bright, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_contrast, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_balance, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_white, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_bottom, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_rotation, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_turn, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (color_chooser, "color-set", G_CALLBACK (color_changed), NULL);
  g_signal_connect (scale_player, "value-changed", G_CALLBACK (player_changed), NULL);

  // g_signal_connect (wf_play, "player-stop", G_CALLBACK (player_stop), scale_player);
  g_signal_connect (pause_button, "toggled", G_CALLBACK (pause_cb), NULL);
  g_signal_connect (x10_button, "toggled", G_CALLBACK (x10_cb), NULL);

  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_end (box, 12);

  return box;
}

void
reopen_clicked (GtkButton *button,
                gpointer user_data)
{
  GtkWidget *dialog;
  gchar *path = NULL;
  gchar *track = NULL;
  gchar *project = NULL;
  gchar **split = NULL;
  guint len;
  gint res;

  dialog = gtk_file_chooser_dialog_new ("Select track", GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        "Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_OK,
                                        NULL);
  if (project_dir != NULL)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), project_dir);

  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (res == GTK_RESPONSE_CANCEL)
    goto cleanup;

  path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
  split = g_strsplit (path, PARSE_SEPARATOR, -1);
  len = g_strv_length (split);
  if (len < 2)
    goto cleanup;

  g_free (project_dir);
  project_dir = uri_composer (split, NULL, 1);

  open_db (&db, &db_uri, uri_composer (split, "file://", 2));

  project = split[len - 2];
  track = split[len - 1];

  printf ("db_uri \"%s\" project \"%s\" track \"%s\"\n", db_uri, project, track);

  hyscan_data_player_stop (player);
  hyscan_data_player_clear_channels (player);
  hyscan_data_player_set_track (player, db, project, track);
  hyscan_data_player_add_channel (player, HYSCAN_SOURCE_NMEA, hyscan_gtk_gliko_get_angular_source (HYSCAN_GTK_GLIKO (gliko)), HYSCAN_CHANNEL_DATA);
  //hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
  //hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);
  hyscan_data_player_play (player, speed);

  {
    gchar *title = g_strdup_printf ("Around+ %s, %s", project, track);
    gtk_window_set_title (GTK_WINDOW (window), title);
    g_free (title);
  }

cleanup:

  g_clear_pointer (&split, g_strfreev);
  g_clear_pointer (&path, g_free);
  gtk_widget_destroy (dialog);
}

void
zoom_clicked (GtkButton *button,
              gpointer user_data)
{
  gint direction = GPOINTER_TO_INT (user_data);
  gboolean in = (direction == 1) ? TRUE : FALSE;
  gdouble f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
  hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), f * (in ? 0.875 : 1.125));
}

void
scale_changed (GtkRange *range, gpointer user_data)
{
  hyscan_gtk_gliko_set_white_point (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_white)));
  hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_bright)));
  hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_contrast)));
  hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_gamma)));
  hyscan_gtk_gliko_set_balance (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_balance)));
  hyscan_gtk_gliko_set_bottom (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_bottom)));
  hyscan_gtk_gliko_set_rotation (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_rotation)));
  hyscan_gtk_gliko_set_full_rotation (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_turn)));
}

void
player_changed (GtkRange *range, gpointer user_data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pause_button)))
    {
      hyscan_data_player_seek (player, player_min + (gint64) (1000000 * gtk_range_get_value (range)));
      player_position_changed = 1;
    }
}

/*
void
player_changed (GtkScale                 *player_scale,
                HyScanGtkWaterfallPlayer *player)
{
  gdouble speed = gtk_range_get_value (GTK_RANGE (player_scale));

  gtk_range_set_fill_level (GTK_RANGE (player_scale), G_MAXDOUBLE);
  gtk_range_set_show_fill_level (GTK_RANGE (player_scale), speed != 0);

  // hyscan_gtk_waterfall_player_set_speed (player, speed);
}

void
player_stop (HyScanGtkWaterfallPlayer *player,
             GtkScale                 *scale)
{
  gtk_range_set_value (GTK_RANGE (scale), 0.0);
}*/

void
color_changed (GtkColorButton *chooser)
{
  guint32 background, colors[2], *colormap;
  guint cmap_len;
  GdkRGBA rgba;

  if (chooser != NULL)
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (chooser), &rgba);

  background = hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0);
  colors[0] = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
  colors[1] = hyscan_tile_color_converter_d2i (rgba.red, rgba.green, rgba.blue, 1.0);

  colormap = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);

  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 0, colormap, cmap_len, background);
  hyscan_gtk_gliko_set_colormap (HYSCAN_GTK_GLIKO (gliko), 1, colormap, cmap_len, background);

  g_free (colormap);
}

gchar *
uri_composer (gchar **path,
              gchar *prefix,
              guint cut)
{
  guint len = g_strv_length (path) - cut;
  gchar *uri, *puri;
  gsize index = 0, written;

  uri = g_malloc0 (2048 * sizeof (gchar));
  puri = uri;

  if (prefix != NULL)
    {
      written = g_snprintf (puri, 2048 - index, "%s", prefix);
      index += written;
      puri += written;
    }

  for (; *path != NULL && len > 0; path++, len--)
    {
      if (**path == '\0')
        continue;

      written = g_snprintf (puri, 2048 - index, COMPOSE_FORMAT, *path);
      index += written;
      puri += written;
    }

  return uri;
}

void
open_db (HyScanDB **db,
         gchar **old_uri,
         gchar *new_uri)
{
  if (new_uri == NULL || old_uri == NULL)
    return;

  if (g_strcmp0 (new_uri, *old_uri) == 0 && *db != NULL)
    return;

  if (*db != NULL)
    g_clear_object (db);

  *db = hyscan_db_new (new_uri);

  if (*old_uri != new_uri)
    g_free (*old_uri);

  *old_uri = new_uri;
}

static gdouble
range360 (const gdouble a)
{
  guint i;
  gdouble d;

  d = a * 65536.0 / 360.0;
  i = (int) d;
  d = (i & 0xFFFF);
  return d * 360.0 / 65536.0;
}

static void
ruler_changed_cb (HyScanGtkGlikoView *view, gdouble point_a_z, gdouble point_a_r, gdouble point_b_z, gdouble point_b_r)
{
  gchar tmpz[32], tmpr[32], tmp[64];
  gdouble x1, y1, x2, y2, d;
  gdouble point_ab_z;
  gdouble point_ba_z;
  const gdouble radians_to_degrees = 180.0 / G_PI;
  const gdouble degrees_to_radians = G_PI / 180.0;
  gdouble a = hyscan_gtk_gliko_get_full_rotation (HYSCAN_GTK_GLIKO (gliko));

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_a_z, point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_b_z, point_b_r, &x2, &y2);

  point_ab_z = range360 (atan2 (x2 - x1, y1 - y2) * radians_to_degrees - a);
  point_ba_z = range360 (atan2 (x1 - x2, y2 - y1) * radians_to_degrees - a);

  x1 = point_a_r * sin (point_a_z * degrees_to_radians);
  y1 = point_a_r * cos (point_a_z * degrees_to_radians);

  x2 = point_b_r * sin (point_b_z * degrees_to_radians);
  y2 = point_b_r * cos (point_b_z * degrees_to_radians);

  d = sqrt ((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));

  g_ascii_formatd (tmpz, sizeof (tmpz), "%.0f", point_a_z);
  g_ascii_formatd (tmpr, sizeof (tmpr), "%.1f", point_a_r);
  g_snprintf (tmp, sizeof (tmp), "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_a), tmp);

  g_ascii_formatd (tmpz, sizeof (tmpz), "%.0f", point_b_z);
  g_ascii_formatd (tmpr, sizeof (tmpr), "%.1f", point_b_r);
  g_snprintf (tmp, sizeof (tmp), "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_b), tmp);

  g_ascii_formatd (tmpz, sizeof (tmpz), "%.0f", point_ab_z);
  g_ascii_formatd (tmpr, sizeof (tmpr), "%.1f", d);
  g_snprintf (tmp, sizeof (tmp), "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_ab), tmp);

  g_ascii_formatd (tmpz, sizeof (tmpz), "%.0f", point_ba_z);
  g_snprintf (tmp, sizeof (tmp), "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_ba), tmp);
}

static void
player_range_cb (HyScanDataPlayer *player, gint64 min, gint64 max, gpointer user_data)
{
  player_min = min;
  gtk_range_set_range (GTK_RANGE (scale_player), 0.0, 0.000001 * (max - min));
}

static void
player_ready_cb (HyScanDataPlayer *player, gint64 time, gpointer user_data)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pause_button)))
    {
      gtk_range_set_value (GTK_RANGE (scale_player), 0.000001 * (time - player_min));
    }
}

static void
pause_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pause_button)))
    {
      hyscan_data_player_pause (player);
      gtk_widget_set_sensitive (GTK_WIDGET (scale_player), TRUE);
      hyscan_gtk_gliko_set_playback (HYSCAN_GTK_GLIKO (gliko), 0);
      player_position_changed = 0;
    }
  else
    {
      hyscan_gtk_gliko_set_playback (HYSCAN_GTK_GLIKO (gliko), player_position_changed ? 2 : 1);
      player_position_changed = 0;
      hyscan_data_player_play (player, speed);
      gtk_widget_set_sensitive (GTK_WIDGET (scale_player), FALSE);
    }
}

static void
x10_cb (GtkToggleButton *togglebutton, gpointer user_data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (x10_button)))
    {
      speed = 10.0;
    }
  else
    {
      speed = 1.0;
    }
  hyscan_data_player_play (player, speed);
}
