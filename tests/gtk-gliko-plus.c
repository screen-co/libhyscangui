#include <hyscan-gtk-area.h>
#include <hyscan-tile-color.h>

#include <hyscan-acoustic-data.h>
#include <hyscan-data-player.h>
#include <hyscan-gtk-gliko-minimal.h>
#include <hyscan-gtk-gliko.h>
#include <hyscan-nmea-data.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
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

void reopen_clicked (GtkButton *button,
                     gpointer user_data);
void zoom_clicked (GtkButton *button,
                   gpointer user_data);
void scale_changed (GtkRange *range, gpointer user_data);

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

static void button_cb (GtkWidget *widget, GdkEventButton *event, void *w);
static void motion_cb (GtkWidget *widget, GdkEventMotion *event, void *w);
static void scroll_cb (GtkWidget *widget, GdkEventScroll *event, void *w);
static gboolean draw_ruler_cb (GtkWidget *widget, cairo_t *cr, gpointer data);

static void update_ruler ();

static GtkWidget *scale_white;
static GtkWidget *scale_bright;
static GtkWidget *scale_contrast;
static GtkWidget *scale_gamma;
static GtkWidget *scale_bottom;
static GtkWidget *scale_rotation;
static GtkWidget *scale_turn;
static GtkWidget *scale_player;

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
static GtkWidget *overlay;
//static GtkWidget *label;

static HyScanDB *db = NULL;
HyScanNMEAData *nmea_data = NULL;
static HyScanDataPlayer *player;

static gint32 num_azimuthes = 1024;

static int pointer_move = POINTER_NONE;
static gdouble sxc0 = 0.f, syc0 = 0.f;
static gdouble mx0 = 0.f, my0 = 0.f;
static gdouble mx1, my1, mx2, my2;
//static gdouble delta = 0.01f;
static GtkWidget *area;
static GtkWidget *ruler;

static const int ruler_size = 6;
gdouble point_a_z = 30.0;
gdouble point_a_r = 75.0;
gdouble point_b_z = 60.0;
gdouble point_b_r = 50.0;
gdouble point_ab_z = 0.0;
gdouble point_ba_z = 0.0;

PangoLayout *pango_layout = NULL;

int
main (int argc,
      char **argv)
{
  GtkWidget *left;
  GtkWidget *central;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gchar *source_name1 = "ss-starboard";
  gchar *source_name2 = "ss-port";
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  gdouble bottom = 0.0;
  gdouble speed = 1.0;
  gint32 angular_source = 1;
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
      { "angular", 'a', 0, G_OPTION_ARG_INT, &angular_source, "Angular source channel's number (default 1)", NULL },
      { "starboard", 's', 0, G_OPTION_ARG_STRING, &source_name1, "Starboard name", NULL },
      { "port", 'r', 0, G_OPTION_ARG_STRING, &source_name2, "Port name", NULL },
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

  overlay = gtk_overlay_new ();
  ruler = gtk_drawing_area_new ();
  //label = gtk_gl_area_new();//gtk_label_new ("");
  gliko = hyscan_gtk_gliko_new ();
  //label = hyscan_gtk_gliko_minimal_new ();

  g_signal_connect (G_OBJECT (ruler), "draw", G_CALLBACK (draw_ruler_cb), NULL);
  //g_signal_connect_swapped (G_OBJECT (ruler), "configure-event", G_CALLBACK (configure_cb), NULL);
  pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (ruler), "A");

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), gliko);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), ruler);

  central = overlay;
  //central = gliko;

  gtk_widget_set_events (central,
                         GDK_EXPOSURE_MASK |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_BUTTON_MOTION_MASK |
                             GDK_SCROLL_MASK);

  g_signal_connect (central, "button_press_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (central, "button_release_event", G_CALLBACK (button_cb), NULL);
  g_signal_connect (central, "motion_notify_event", G_CALLBACK (motion_cb), NULL);
  g_signal_connect (central, "scroll_event", G_CALLBACK (scroll_cb), NULL);

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

  gtk_widget_show_all (window);

  if (db != NULL && project_name != NULL && track_name != NULL)
    {
      hyscan_data_player_set_track (player, db, project_name, track_name);
      hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
      hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);
      hyscan_data_player_play (player, speed);
    }

  update_ruler ();

  /* Начинаем работу. */
  gtk_main ();

  hyscan_data_player_shutdown (player);
  g_object_unref (G_OBJECT (gliko));
  g_object_unref (G_OBJECT (player));
  g_clear_object (&db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
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
  scale_white = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.001, 1.0, 0.001);
  scale_bright = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -1.0, 1.0, 0.01);
  scale_contrast = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -1.0, 1.0, 0.01);
  scale_gamma = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.1, 2.0, 0.005);
  scale_bottom = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 50.0, 0.01);
  scale_rotation = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -180, 180.0, 1.0);
  scale_turn = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -180, 180.0, 1.0);
  scale_player = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -10.0, 10.0, 0.1);

  GtkWidget *track_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

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
  gtk_scale_set_value_pos (GTK_SCALE (scale_gamma), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_player), GTK_POS_TOP);
  gtk_widget_set_size_request (scale_white, 150, 1);
  gtk_widget_set_size_request (scale_bright, 150, 1);
  gtk_widget_set_size_request (scale_contrast, 150, 1);
  gtk_widget_set_size_request (scale_gamma, 150, 1);
  gtk_widget_set_size_request (scale_player, 150, 1);
  gtk_range_set_value (GTK_RANGE (scale_white), white);
  gtk_range_set_value (GTK_RANGE (scale_bright), 0.0);
  gtk_range_set_value (GTK_RANGE (scale_contrast), 0.0);
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
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Дно"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_bottom, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Поворот развертки"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_rotation, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Поворот изображения"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_turn, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Плеер"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_player, FALSE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX (box), ruler_grid, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);

  g_signal_connect (btn_reopen, "clicked", G_CALLBACK (reopen_clicked), NULL);
  g_signal_connect (zoom_btn_in, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (1));
  g_signal_connect (zoom_btn_out, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (0));
  g_signal_connect (scale_gamma, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_bright, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_contrast, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_white, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_bottom, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_rotation, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (scale_turn, "value-changed", G_CALLBACK (scale_changed), NULL);
  g_signal_connect (color_chooser, "color-set", G_CALLBACK (color_changed), NULL);

  // g_signal_connect (wf_play, "player-stop", G_CALLBACK (player_stop), scale_player);

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
  gdouble speed;

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

  hyscan_data_player_pause (player);
  hyscan_data_player_set_track (player, db, project, track);
  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 0), 1, HYSCAN_CHANNEL_DATA);
  hyscan_data_player_add_channel (player, hyscan_gtk_gliko_get_source (HYSCAN_GTK_GLIKO (gliko), 1), 2, HYSCAN_CHANNEL_DATA);
  speed = 1.0; //gtk_range_get_value (GTK_RANGE (scale_player));
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
  hyscan_gtk_gliko_set_bottom (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_bottom)));
  hyscan_gtk_gliko_set_rotation (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_rotation)));
  hyscan_gtk_gliko_set_full_rotation (HYSCAN_GTK_GLIKO (gliko), gtk_range_get_value (GTK_RANGE (scale_turn)));
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

static void
update_title ()
{
}

static gdouble range360( const gdouble a )
{
  guint i;
  gdouble d;

  d = a * 65536.0 / 360.0;
  i = (int)d;
  d = (i & 0xFFFF);
  return d * 360.0 / 65536.0;
}

static void
update_ruler ()
{
  gchar tmpz[32], tmpr[32], tmp[64];
  gdouble x1, y1, x2, y2, d;
  const gdouble radians_to_degrees = 180.0 / G_PI;
  const gdouble degrees_to_radians = G_PI / 180.0;
  gdouble a = hyscan_gtk_gliko_get_full_rotation (HYSCAN_GTK_GLIKO (gliko));

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_a_z, point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_b_z, point_b_r, &x2, &y2);

  gtk_widget_queue_draw (ruler);

  point_ab_z = range360 (atan2 (x2 - x1, y1 - y2) * radians_to_degrees - a);
  point_ba_z = range360 (atan2 (x1 - x2, y2 - y1) * radians_to_degrees - a);

  x1 = point_a_r * sin (point_a_z * degrees_to_radians);
  y1 = point_a_r * cos (point_a_z * degrees_to_radians);

  x2 = point_b_r * sin (point_b_z * degrees_to_radians);
  y2 = point_b_r * cos (point_b_z * degrees_to_radians);

  d = sqrt ((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));

  g_ascii_formatd (tmpz, 32, "%.0lf", point_a_z);
  g_ascii_formatd (tmpr, 32, "%.1lf", point_a_r);
  sprintf (tmp, "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_a), tmp);

  g_ascii_formatd (tmpz, 32, "%.0lf", point_b_z);
  g_ascii_formatd (tmpr, 32, "%.1lf", point_b_r);
  sprintf (tmp, "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_b), tmp);

  g_ascii_formatd (tmpz, 32, "%.0lf", point_ab_z);
  g_ascii_formatd (tmpr, 32, "%.1lf", d);
  sprintf (tmp, "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_ab), tmp);

  g_ascii_formatd (tmpz, 32, "%.0lf", point_ba_z);
  sprintf (tmp, "%s\302\260/%s м", tmpz, tmpr);
  gtk_label_set_text (GTK_LABEL (label_ba), tmp);
}

static int
ruler_node_catch (const gdouble a, const gdouble r, const gdouble x, const gdouble y)
{
  gdouble xn, yn;

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), a, r, &xn, &yn);
  if (x >= (xn - ruler_size) &&
      x < (xn + ruler_size) &&
      y >= (yn - ruler_size) &&
      y < (yn + ruler_size))
    {
      return 1;
    }
  return 0;
}

static int
ruler_line_catch (const gdouble x, const gdouble y)
{
  gdouble x1, y1, x2, y2, d1, d2, d;

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_a_z, point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_b_z, point_b_r, &x2, &y2);

  d = sqrt ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  d1 = sqrt ((x - x1) * (x - x1) + (y - y1) * (y - y1));
  d2 = sqrt ((x - x2) * (x - x2) + (y - y2) * (y - y2));

  if (d1 > d || d2 > d || d < 1.0f)
    return 0;

  // расстояние от точки до отразка
  d = fabs ((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1) / d;
  if (d >= (0.5f * ruler_size))
    return 0;
  return 1;
}

static void
button_cb (GtkWidget *widget, GdkEventButton *event, void *w)
{
  if (event->button == 1)
    {
      if (event->type == GDK_BUTTON_PRESS)
        {
          if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == 0)
            {
              if (ruler_node_catch (point_a_z, point_a_r, event->x, event->y))
                {
                  pointer_move = POINTER_RULER_FROM;
                }
              else if (ruler_node_catch (point_b_z, point_b_r, event->x, event->y))
                {
                  pointer_move = POINTER_RULER_TO;
                }
              else if (ruler_line_catch (event->x, event->y))
                {
                  pointer_move = POINTER_RULER_LINE;
                  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_a_z, point_a_r, &mx1, &my1);
                  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_b_z, point_b_r, &mx2, &my2);
                  mx0 = event->x;
                  my0 = event->y;
                }
              else
                {
                  pointer_move = POINTER_CENTER;
                  hyscan_gtk_gliko_get_center (HYSCAN_GTK_GLIKO (gliko), &sxc0, &syc0);
                  mx0 = event->x;
                  my0 = event->y;
                }
            }
        }
      else if (event->type == GDK_BUTTON_RELEASE)
        {
          pointer_move = 0;
        }
    }
  else if (event->button == 2)
    {
      if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
        {
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), 0.0);
          gtk_range_set_value (GTK_RANGE (scale_contrast), 0.0);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), 0.0);
          gtk_range_set_value (GTK_RANGE (scale_bright), 0.0);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), 1.0);
          gtk_range_set_value (GTK_RANGE (scale_gamma), 1.0);
          update_title ();
        }
      else
        {
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), 1.0);
          hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (gliko), 0.0, 0.0);
          update_title ();
        }
    }
}

static void
motion_cb (GtkWidget *widget, GdkEventMotion *event, void *w)
{
  GtkAllocation allocation;
  int baseline;
  gdouble s, x, y;

  if (pointer_move == POINTER_CENTER)
    {
      gtk_widget_get_allocated_size (GTK_WIDGET (gliko), &allocation, &baseline);

      s = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
      x = sxc0 - s * (mx0 - (gdouble) event->x) / allocation.height;
      y = syc0 - s * ((gdouble) event->y - my0) / allocation.height;
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (gliko), x, y);
    }
  else if (pointer_move == POINTER_RULER_FROM)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (gliko), event->x, event->y, &point_a_z, &point_a_r);
      update_ruler ();
    }
  else if (pointer_move == POINTER_RULER_TO)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (gliko), event->x, event->y, &point_b_z, &point_b_r);
      update_ruler ();
    }
  else if (pointer_move == POINTER_RULER_LINE)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (gliko), mx1 + (event->x - mx0), my1 + (event->y - my0), &point_a_z, &point_a_r);
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (gliko), mx2 + (event->x - mx0), my2 + (event->y - my0), &point_b_z, &point_b_r);
      update_ruler ();
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
          f += 0.01;
          if (f > 1.0)
            {
              f = 1.0;
            }
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), f);
          gtk_range_set_value (GTK_RANGE (scale_contrast), f);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), f + 0.01);
          gtk_range_set_value (GTK_RANGE (scale_bright), f);
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
          gtk_range_set_value (GTK_RANGE (scale_gamma), f);
          update_title ();
        }
      else
        {
          f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (gliko), 0.875 * f);
          update_title ();
        }
    }
  if (event->direction == GDK_SCROLL_DOWN)
    {
      if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
        {
          f = hyscan_gtk_gliko_get_contrast (HYSCAN_GTK_GLIKO (gliko));
          f -= 0.01;
          if (f < -1.0)
            {
              f = -1.0;
            }
          hyscan_gtk_gliko_set_contrast (HYSCAN_GTK_GLIKO (gliko), f);
          gtk_range_set_value (GTK_RANGE (scale_contrast), f);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_SHIFT_MASK)
        {
          f = hyscan_gtk_gliko_get_brightness (HYSCAN_GTK_GLIKO (gliko));
          hyscan_gtk_gliko_set_brightness (HYSCAN_GTK_GLIKO (gliko), f - 0.01);
          gtk_range_set_value (GTK_RANGE (scale_bright), f);
          update_title ();
        }
      else if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          f = hyscan_gtk_gliko_get_gamma_value (HYSCAN_GTK_GLIKO (gliko)) + 0.01;
          hyscan_gtk_gliko_set_gamma_value (HYSCAN_GTK_GLIKO (gliko), f);
          gtk_range_set_value (GTK_RANGE (scale_gamma), f);
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

static void ruler_caption ( cairo_t *cr, const double x1, const double y1, const double a, const char *caption, const int length )
{
  double z, w, h, x, y;
  PangoRectangle inc_rect, logical_rect;
  const double margin = 4;
  gdouble b = hyscan_gtk_gliko_get_full_rotation (HYSCAN_GTK_GLIKO (gliko));

  pango_layout_set_text (pango_layout, caption, length);
  pango_layout_get_pixel_extents (pango_layout, &inc_rect, &logical_rect);
  w = logical_rect.width;
  h = logical_rect.height;

  z = range360 (a - 45.0 + b);

  switch( (int)(z / 90.0) )
  {
  case 0: // угол 45-135, надпись слева
    x = x1 - ruler_size - w - margin;
    y = y1 - 0.5 * h;
    break;
  case 1: // угол 135-225, надпись сверху
    x = x1 - 0.5 * w;
    y = y1 - ruler_size - h - margin;
    break;
  case 2: // угол 225-315, надпись справа
    x = x1 + ruler_size + margin;
    y = y1 - 0.5 * h;
    break;
  default: // угол 315-45, надпись снизу
    x = x1 - 0.5 * w;
    y = y1 + ruler_size + margin;
    break;
  }
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);
  cairo_rectangle (cr, x, y, w, h);
  cairo_fill (cr);
  cairo_stroke (cr);
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  pango_cairo_show_layout (cr, pango_layout);
  cairo_stroke (cr);
}

static gboolean
draw_ruler_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  guint width, height;
  gint b = ruler_size * 2 / 3;
  //GdkRGBA color;
  GtkStyleContext *context;
  gdouble x1, y1, x2, y2;

  context = gtk_widget_get_style_context (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_a_z, point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (gliko), point_b_z, point_b_r, &x2, &y2);

  // рисуем затенение
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);

  cairo_set_line_width (cr, 4.0);
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_stroke (cr);

  cairo_rectangle (cr, x1 - ruler_size, y1 - ruler_size, ruler_size + ruler_size, ruler_size + ruler_size);
  cairo_fill (cr);
  cairo_stroke (cr);

  cairo_rectangle (cr, x2 - ruler_size, y2 - ruler_size, ruler_size + ruler_size, ruler_size + ruler_size);
  cairo_fill (cr);
  cairo_stroke (cr);

  // рисуем отрезок
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);

  cairo_set_line_width (cr, 2.0);
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_stroke (cr);

  cairo_rectangle (cr, x1 - b, y1 - b, b + b, b + b);
  cairo_fill (cr);
  cairo_stroke (cr);

  cairo_rectangle (cr, x2 - b, y2 - b, b + b, b + b);
  cairo_fill (cr);
  cairo_stroke (cr);

  ruler_caption (cr, x1, y1, point_ab_z, "A", 1);
  ruler_caption (cr, x2, y2, point_ba_z, "B", 1);

  /*

  gtk_style_context_get_color (context,
                               gtk_style_context_get_state (context),
                               &color);
  gdk_cairo_set_source_rgba (cr, &color);
  */

  return FALSE;
}
