#include <hyscan-gtk-area.h>
#include <hyscan-gtk-waterfall.h>
#include <hyscan-gtk-waterfall-control.h>
#include <hyscan-object-model.h>
#include <hyscan-gtk-waterfall-grid.h>
#include <hyscan-gtk-waterfall-mark.h>
#include <hyscan-gtk-waterfall-meter.h>
#include <hyscan-gtk-waterfall-shadowm.h>
#include <hyscan-gtk-waterfall-player.h>
#include <hyscan-gtk-waterfall-coord.h>
#include <hyscan-tile-color.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>
#include <string.h>

#ifdef G_OS_WIN32
  #define PARSE_SEPARATOR "\\"
  #define COMPOSE_FORMAT "%s\\"
#else
  #define PARSE_SEPARATOR "/"
  #define COMPOSE_FORMAT "/%s"
#endif

void       reopen_clicked   (GtkButton *button,
                             gpointer   user_data);
void       zoom_clicked     (GtkButton *button,
                             gpointer   user_data);
void       level_changed    (GtkScale  *white,
                             GtkScale  *gamma);

void       color_changed    (GtkColorButton  *chooser);
// void       player_changed   (GtkScale                 *player_scale,
                             // HyScanGtkWaterfallPlayer *player);
// void       player_stop      (HyScanGtkWaterfallPlayer *player,
                             // GtkScale                 *scale);*/

gchar     *uri_composer     (gchar    **path,
                             gchar     *prefix,
                             guint      cut);

GtkWidget *make_layer_btn   (HyScanGtkLayer *layer,
                             GtkWidget      *from,
                             const gchar    *text);
GtkWidget *make_menu        (HyScanGtkWaterfall *wf,
                             gdouble             white,
                             gdouble             gamma);
gboolean   slant_ground     (GtkSwitch *widget,
                             gboolean   state,
                             gpointer   udata);
void       open_db          (HyScanDB **db,
                             gchar    **old_uri,
                             gchar     *new_uri);

static HyScanGtkWaterfallState   *wf_state;
static HyScanGtkWaterfall        *wf;
static HyScanGtkWaterfallControl *wf_ctrl;
static HyScanGtkWaterfallGrid    *wf_grid;
static void    *wf_mark;
// static HyScanGtkWaterfallMark    *wf_mark;
static HyScanGtkWaterfallMeter   *wf_metr;
static HyScanGtkWaterfallShadowm *wf_shad;
static HyScanGtkWaterfallCoord   *wf_coor;
static void  *wf_play;
// static HyScanGtkWaterfallPlayer  *wf_play;

static HyScanDB                  *db;
static gchar                     *db_uri;
static gchar                     *project_dir;
static HyScanObjectModel         *markmodel;
static GtkWidget                 *window;

int
main (int    argc,
      char **argv)
{
  GtkWidget *area;
  GtkWidget *left;
  GtkWidget *central;
  GtkWidget *wf_widget;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gdouble speed = 1.0;
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (2048));

  gtk_init (&argc, &argv);

  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name",          NULL},
      { "track",       't', 0, G_OPTION_ARG_STRING, &track_name,   "Track name",            NULL},
      { "speed",       's', 0, G_OPTION_ARG_DOUBLE, &speed,        "Speed of ship",         NULL},
      { "white",       'w', 0, G_OPTION_ARG_DOUBLE, &white,        "white level (0.0-1.0)", NULL},
      { "gamma",       'g', 0, G_OPTION_ARG_DOUBLE, &gamma,        "gamma level (0.0-1.0)", NULL},
      {NULL}
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
  markmodel = hyscan_object_model_new ();
  hyscan_object_model_set_types (markmodel, 1, HYSCAN_TYPE_OBJECT_DATA_WFMARK);

  /* Основное окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_set_title (GTK_WINDOW (window), "Waterfall+");
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  /* Водопад. */
  wf_widget = hyscan_gtk_waterfall_new (cache);
  /* Для удобства приведем типы. */
  wf  = HYSCAN_GTK_WATERFALL (wf_widget);
  wf_state  = HYSCAN_GTK_WATERFALL_STATE (wf_widget);

  wf_grid = hyscan_gtk_waterfall_grid_new ();
  wf_ctrl = hyscan_gtk_waterfall_control_new ();
  wf_metr = hyscan_gtk_waterfall_meter_new ();
  wf_shad = hyscan_gtk_waterfall_shadowm_new ();
  wf_mark = hyscan_gtk_waterfall_mark_new (markmodel);
  wf_play = hyscan_gtk_waterfall_player_new ();
  wf_coor = hyscan_gtk_waterfall_coord_new ();

  hyscan_gtk_waterfall_state_set_ship_speed (wf_state, speed);

  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_grid), "grid");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_ctrl), "control");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_metr), "meter");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_mark), "mark");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_shad), "shadowmeter");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_play), "player");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (wf), HYSCAN_GTK_LAYER (wf_coor), "coordinates");
  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  hyscan_gtk_waterfall_set_upsample (wf, 2);

  /* Устанавливаем цветовую схему и подложку. */
  hyscan_gtk_waterfall_set_levels_for_all (wf, 0.0, gamma, white);
  hyscan_gtk_waterfall_set_substrate (wf, hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0));

  /* Кладем виджет в основное окно. */
  area = hyscan_gtk_area_new ();

  left = make_menu (wf, white, gamma);
  hyscan_gtk_area_set_left (HYSCAN_GTK_AREA (area), left);

  central = hyscan_gtk_waterfall_make_grid (wf);
  hyscan_gtk_area_set_central (HYSCAN_GTK_AREA (area), central);

  gtk_container_add (GTK_CONTAINER (window), area);
  gtk_widget_show_all (window);

  if (db != NULL && project_name != NULL && track_name != NULL)
    {
      hyscan_gtk_waterfall_state_set_track (wf_state, db, project_name, track_name);
      hyscan_object_model_set_project (markmodel, db, project_name);
    }

  /* Начинаем работу. */
  gtk_main ();
  g_clear_object (&cache);
  g_clear_object (&db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}

GtkWidget*
make_layer_btn (HyScanGtkLayer *layer,
                GtkWidget      *from,
                const gchar    *text)
{
  GtkWidget *button;
  const gchar *icon;

  icon = hyscan_gtk_layer_get_icon_name (layer);
  button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (from));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));
  gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
  gtk_button_set_label (GTK_BUTTON (button), text);

  return button;
}

GtkWidget*
make_menu (HyScanGtkWaterfall *wf,
           gdouble             white,
           gdouble             gamma)
{
  // GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *zoom_btn_in = gtk_button_new_from_icon_name ("zoom-in-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *zoom_btn_out = gtk_button_new_from_icon_name ("zoom-out-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *btn_reopen = gtk_button_new_from_icon_name ("folder-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *scale_white = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.000, 1.0, 0.001);
  GtkWidget *scale_gamma = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.5, 2.0, 0.005);
  GtkWidget *scale_player = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -10.0, 10.0, 0.1);
  GtkWidget *slant_ground_switch = gtk_switch_new ();

  GtkWidget *lay_ctrl = make_layer_btn (HYSCAN_GTK_LAYER (wf_ctrl), NULL, "control");
  GtkWidget *lay_mark = make_layer_btn (HYSCAN_GTK_LAYER (wf_mark), lay_ctrl, "marks");
  GtkWidget *lay_metr = make_layer_btn (HYSCAN_GTK_LAYER (wf_metr), lay_mark, "meter");
  GtkWidget *lay_shad = make_layer_btn (HYSCAN_GTK_LAYER (wf_shad), lay_metr, "shadowmeter");
  GtkWidget *lay_coor = make_layer_btn (HYSCAN_GTK_LAYER (wf_coor), lay_shad, "coordinates");

  GtkWidget *lay_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *track_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  /* Делаем симпатичные кнопки. */
  gtk_style_context_add_class (gtk_widget_get_style_context (lay_box), "linked");
  gtk_style_context_add_class (gtk_widget_get_style_context (track_box), "linked");

  GtkWidget *color_chooser = gtk_color_button_new ();
  GdkRGBA    rgba;

  gdk_rgba_parse (&rgba, "#00FFFF");
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_chooser), &rgba);

  /* Настраиваем прокрутки. */
  gtk_scale_set_value_pos (GTK_SCALE (scale_white), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_gamma), GTK_POS_TOP);
  gtk_scale_set_value_pos (GTK_SCALE (scale_player), GTK_POS_TOP);
  gtk_widget_set_size_request (scale_white, 150, 1);
  gtk_widget_set_size_request (scale_gamma, 150, 1);
  gtk_widget_set_size_request (scale_player, 150, 1);
  gtk_range_set_value (GTK_RANGE (scale_white), white);
  gtk_range_set_value (GTK_RANGE (scale_gamma), gamma);
  gtk_range_set_value (GTK_RANGE (scale_player), 0.0);

  gtk_scale_add_mark (GTK_SCALE (scale_player), 0.0, GTK_POS_TOP, NULL);
  gtk_scale_set_has_origin (GTK_SCALE (scale_player), FALSE);

  /* Layer control. */
  gtk_box_pack_start (GTK_BOX (lay_box), lay_ctrl, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_mark, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_metr, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_shad, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_coor, TRUE, TRUE, 0);

  /* Кладём в коробку. */
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_in,  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_out, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), btn_reopen,   TRUE, TRUE, 2);

  gtk_box_pack_start (GTK_BOX (box), track_box,    FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Изображение"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), color_chooser,FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_white,  FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_gamma,  FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Слой"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), lay_box,      FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Плеер"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scale_player, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Накл/гор"), FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), slant_ground_switch, FALSE, TRUE, 0);

  g_signal_connect (btn_reopen, "clicked", G_CALLBACK (reopen_clicked), NULL);
  g_signal_connect (zoom_btn_in, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (1));
  g_signal_connect (zoom_btn_out, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (0));
  g_signal_connect (scale_white, "value-changed", G_CALLBACK (level_changed), scale_gamma);
  g_signal_connect_swapped (scale_gamma, "value-changed", G_CALLBACK (level_changed), scale_white);
  ///*g_signal_connect (scale_player, "value-changed", G_CALLBACK (player_changed), wf_play);*/
  g_signal_connect (color_chooser, "color-set", G_CALLBACK (color_changed), NULL);
  g_signal_connect_swapped (lay_ctrl, "clicked", G_CALLBACK (hyscan_gtk_layer_grab_input), HYSCAN_GTK_LAYER (wf_ctrl));
  g_signal_connect_swapped (lay_mark, "clicked", G_CALLBACK (hyscan_gtk_layer_grab_input), HYSCAN_GTK_LAYER (wf_mark));
  g_signal_connect_swapped (lay_metr, "clicked", G_CALLBACK (hyscan_gtk_layer_grab_input), HYSCAN_GTK_LAYER (wf_metr));
  g_signal_connect_swapped (lay_shad, "clicked", G_CALLBACK (hyscan_gtk_layer_grab_input), HYSCAN_GTK_LAYER (wf_shad));
  g_signal_connect_swapped (lay_coor, "clicked", G_CALLBACK (hyscan_gtk_layer_grab_input), HYSCAN_GTK_LAYER (wf_coor));
  g_signal_connect (slant_ground_switch, "state-set", G_CALLBACK (slant_ground), wf);

  // g_signal_connect (wf_play, "player-stop", G_CALLBACK (player_stop), scale_player);

  hyscan_gtk_layer_grab_input (HYSCAN_GTK_LAYER (wf_ctrl));
  gtk_widget_set_size_request (GTK_WIDGET (wf), 800, 600);

  // gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (wf));

  // gtk_overlay_add_overlay (GTK_OVERLAY (overlay), box);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_end (box, 12);

  return box;
}

void
reopen_clicked (GtkButton *button,
                gpointer   user_data)
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
                                        "Cancel",   GTK_RESPONSE_CANCEL,
                                        "_Open",    GTK_RESPONSE_OK,
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


  hyscan_gtk_waterfall_state_set_track (wf_state, db, project, track);
  hyscan_object_model_set_project (markmodel, db, project);

  {
    gchar * title = g_strdup_printf ("Waterfall+ %s, %s", project, track);
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
              gpointer   user_data)
{
  gint direction = GPOINTER_TO_INT (user_data);
  gboolean in = (direction == 1) ? TRUE : FALSE;
  hyscan_gtk_waterfall_control_zoom (wf_ctrl, in);
}

void
level_changed (GtkScale  *white_scale,
               GtkScale  *gamma_scale)
{
  gdouble white = gtk_range_get_value (GTK_RANGE (white_scale));
  gdouble gamma = gtk_range_get_value (GTK_RANGE (gamma_scale));

  hyscan_gtk_waterfall_set_levels_for_all (wf, 0.0, gamma, (white > 0.0) ? white : 0.001);
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
  colors[0]  = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
  colors[1]  = hyscan_tile_color_converter_d2i (rgba.red, rgba.green, rgba.blue, 1.0);

  colormap   = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);

  hyscan_gtk_waterfall_set_colormap_for_all (wf, colormap, cmap_len, background);

  g_free (colormap);
}

gchar*
uri_composer (gchar **path,
              gchar  *prefix,
              guint   cut)
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

gboolean
slant_ground (GtkSwitch *widget,
              gboolean   state,
              gpointer   udata)
{
  HyScanTileFlags flags = 0;
  HyScanTileFlags gnd_flag = HYSCAN_TILE_GROUND;
  HyScanGtkWaterfallState *wf = udata;
  HyScanGtkWaterfallState *wf_state = HYSCAN_GTK_WATERFALL_STATE (wf);

  flags = hyscan_gtk_waterfall_state_get_tile_flags (wf_state);

  if (state)
    flags |= gnd_flag;
  else
    flags &= ~gnd_flag;

  hyscan_gtk_waterfall_state_set_tile_flags (wf_state, flags);

  gtk_switch_set_state (GTK_SWITCH (widget), state);
  return TRUE;
}

void
open_db (HyScanDB **db,
         gchar    **old_uri,
         gchar     *new_uri)
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
