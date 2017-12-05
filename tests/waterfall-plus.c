#include <hyscan-gtk-waterfall.h>
#include <hyscan-gtk-waterfall-control.h>
#include <hyscan-gtk-waterfall-grid.h>
#include <hyscan-gtk-waterfall-mark.h>
#include <hyscan-gtk-waterfall-meter.h>
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

enum
{
  DIALOG_CANCEL,
  DIALOG_OPEN_RAW,
  DIALOG_OPEN_COMPUTED
};

void       reopen_clicked   (GtkButton *button,
                             gpointer   user_data);
void       zoom_clicked     (GtkButton *button,
                             gpointer   user_data);
void       level_changed    (GtkScale  *white,
                             GtkScale  *gamma);

void       color_changed    (GtkColorButton  *chooser);

gchar     *uri_composer     (gchar    **path,
                             gchar     *prefix,
                             guint      cut);

GtkWidget *make_layer_btn   (HyScanGtkWaterfallLayer *layer,
                             GtkWidget          *from);
GtkWidget *make_overlay     (HyScanGtkWaterfall *wf,
                             gdouble             white,
                             gdouble             gamma);

void       open_db          (HyScanDB **db,
                             gchar    **old_uri,
                             gchar     *new_uri);

static HyScanGtkWaterfallState   *wf_state;
static HyScanGtkWaterfall        *wf;
static HyScanGtkWaterfallGrid    *wf_grid;
static HyScanGtkWaterfallControl *wf_ctrl;
static HyScanGtkWaterfallMark    *wf_mark;
static HyScanGtkWaterfallMeter   *wf_metr;
static HyScanDB                  *db;
static gchar                     *db_uri;
static gchar                     *project_dir;
static GtkWidget                 *window;

int
main (int    argc,
      char **argv)
{
  GtkWidget *overlay;
  GtkWidget *wf_widget;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gdouble speed = 1.0;
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));
  HyScanCache *cache2 = HYSCAN_CACHE (hyscan_cached_new (512));

  gtk_init (&argc, &argv);

  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name;",          NULL},
      { "track",       't', 0, G_OPTION_ARG_STRING, &track_name,   "Track name;",            NULL},
      { "speed",       's', 0, G_OPTION_ARG_DOUBLE, &speed,        "Speed of ship;",         NULL},
      { "white",       'w', 0, G_OPTION_ARG_DOUBLE, &white,        "white level (0.0-1.0);", NULL},
      { "gamma",       'g', 0, G_OPTION_ARG_DOUBLE, &gamma,        "gamma level (0.0-1.0);", NULL},
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

  /* Основное окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_set_title (GTK_WINDOW (window), "Waterfall+");
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  /* Водопад. */
  wf_widget = hyscan_gtk_waterfall_new ();
  /* Для удобства приведем типы. */
  wf  = HYSCAN_GTK_WATERFALL (wf_widget);
  wf_state  = HYSCAN_GTK_WATERFALL_STATE (wf_widget);

  wf_ctrl = hyscan_gtk_waterfall_control_new (wf);
  wf_grid = hyscan_gtk_waterfall_grid_new (wf);
  wf_metr = hyscan_gtk_waterfall_meter_new (wf);
  wf_mark = hyscan_gtk_waterfall_mark_new (wf);

  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  hyscan_gtk_waterfall_state_set_cache (wf_state, cache, cache2, "prefix");

  hyscan_gtk_waterfall_state_set_depth_source (wf_state, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);
  hyscan_gtk_waterfall_state_set_depth_filter_size (wf_state, 2);
  hyscan_gtk_waterfall_state_set_depth_time (wf_state, 100000);

  hyscan_gtk_waterfall_set_upsample (wf, 2);

  /* Устанавливаем цветовую схему и подложку. */
  hyscan_gtk_waterfall_set_levels_for_all (wf, 0.0, gamma, white);
  hyscan_gtk_waterfall_set_substrate (wf, hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0));

  /* Кладем виджет в основное окно. */
  overlay = make_overlay (wf, white, gamma);

  gtk_container_add (GTK_CONTAINER (window), overlay);
  gtk_widget_show_all (window);

  if (db != NULL && project_name != NULL && track_name != NULL)
    hyscan_gtk_waterfall_state_set_track (wf_state, db, project_name, track_name, TRUE);

  /* Начинаем работу. */
  gtk_main ();

  g_clear_object (&cache);
  g_clear_object (&cache2);
  g_clear_object (&db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);

  g_clear_object (&wf_state);
  g_clear_object (&wf_grid);
  g_clear_object (&wf_ctrl);
  g_clear_object (&wf_mark);
  g_clear_object (&wf_metr);

  xmlCleanupParser ();

  return 0;
}

GtkWidget*
make_layer_btn (HyScanGtkWaterfallLayer *layer,
                GtkWidget               *from)
{
  GtkWidget *new;
  const gchar *icon;

  icon = hyscan_gtk_waterfall_layer_get_mnemonic (layer);
  new = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (from));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (new), FALSE);
  gtk_button_set_image (GTK_BUTTON (new), gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));

  return new;
}

GtkWidget*
make_overlay (HyScanGtkWaterfall *wf,
              gdouble             white,
              gdouble             gamma)
{
  GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *zoom_btn_in = gtk_button_new_from_icon_name ("zoom-in-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *zoom_btn_out = gtk_button_new_from_icon_name ("zoom-out-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *btn_reopen = gtk_button_new_from_icon_name ("folder-symbolic", GTK_ICON_SIZE_BUTTON);
  GtkWidget *scale_white = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.000, 1.0, 0.001);
  GtkWidget *scale_gamma = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.5, 2.0, 0.005);

  GtkWidget *lay_ctrl = make_layer_btn (HYSCAN_GTK_WATERFALL_LAYER (wf_ctrl), NULL);
  GtkWidget *lay_mark = make_layer_btn (HYSCAN_GTK_WATERFALL_LAYER (wf_mark), lay_ctrl);
  GtkWidget *lay_metr = make_layer_btn (HYSCAN_GTK_WATERFALL_LAYER (wf_metr), lay_mark);

  GtkWidget *lay_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *track_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  /* Делаем симпатичные кнопки. */
  gtk_style_context_add_class (gtk_widget_get_style_context (lay_box), "linked");
  gtk_style_context_add_class (gtk_widget_get_style_context (track_box), "linked");

  GtkWidget *color_chooser = gtk_color_button_new ();
  GdkRGBA    rgba;

  gdk_rgba_parse (&rgba, "#00FFFF");
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_chooser), &rgba);

  /* Настраиваем прокрутки. */
  gtk_scale_set_value_pos (GTK_SCALE (scale_white), GTK_POS_LEFT);
  gtk_scale_set_value_pos (GTK_SCALE (scale_gamma), GTK_POS_LEFT);
  gtk_widget_set_size_request (scale_white, 150, 1);
  gtk_widget_set_size_request (scale_gamma, 150, 1);
  gtk_range_set_value (GTK_RANGE (scale_white), white);
  gtk_range_set_value (GTK_RANGE (scale_gamma), gamma);

  /* Layer control. */
  gtk_box_pack_start (GTK_BOX (lay_box), lay_ctrl, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_mark, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_metr, FALSE, TRUE, 0);

  /* Кладём в коробку. */
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_in,  FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), zoom_btn_out, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (track_box), btn_reopen,   FALSE, FALSE, 2);

  gtk_box_pack_start (GTK_BOX (box), track_box,   FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), color_chooser,FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), scale_white,  FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), scale_gamma,  FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), lay_box,      FALSE, FALSE, 2);

  g_signal_connect (btn_reopen, "clicked", G_CALLBACK (reopen_clicked), NULL);
  g_signal_connect (zoom_btn_in, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (1));
  g_signal_connect (zoom_btn_out, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (0));
  g_signal_connect (scale_white, "value-changed", G_CALLBACK (level_changed), scale_gamma);
  g_signal_connect_swapped (scale_gamma, "value-changed", G_CALLBACK (level_changed), scale_white);
  g_signal_connect (color_chooser, "color-set", G_CALLBACK (color_changed), NULL);
  g_signal_connect_swapped (lay_ctrl, "clicked", G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input), HYSCAN_GTK_WATERFALL_LAYER (wf_ctrl));
  g_signal_connect_swapped (lay_mark, "clicked", G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input), HYSCAN_GTK_WATERFALL_LAYER (wf_mark));
  g_signal_connect_swapped (lay_metr, "clicked", G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input), HYSCAN_GTK_WATERFALL_LAYER (wf_metr));

  hyscan_gtk_waterfall_layer_grab_input (HYSCAN_GTK_WATERFALL_LAYER (wf_ctrl));
  gtk_widget_set_size_request (GTK_WIDGET (wf), 800, 600);

  gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (wf));

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), box);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_END);
  gtk_widget_set_margin_bottom (box, 5);

  return overlay;
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
  gboolean rawness;
  guint len;
  gint res;

  dialog = gtk_file_chooser_dialog_new ("Select track", GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        "Cancel",   DIALOG_CANCEL,
                                        "Raw",      DIALOG_OPEN_RAW,
                                        "Computed", DIALOG_OPEN_COMPUTED,
                                        NULL);
  if (project_dir != NULL)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), project_dir);

  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (res == DIALOG_OPEN_RAW)
    rawness = TRUE;
  else if (res == DIALOG_OPEN_COMPUTED)
    rawness = FALSE;
  else
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


  hyscan_gtk_waterfall_state_set_track (wf_state, db, project, track, rawness);

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
