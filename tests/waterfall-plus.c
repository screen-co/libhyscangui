#include <hyscan-gtk-waterfall.h>
#include <hyscan-gtk-waterfall-control.h>
#include <hyscan-gtk-waterfall-grid.h>
#include <hyscan-gtk-waterfall-mark.h>
#include <hyscan-tile-color.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>
#include <string.h>

void       reopen_clicked   (GtkButton *button,
                             gpointer   user_data);
void       zoom_clicked     (GtkButton *button,
                             gpointer   user_data);
void       white_changed    (GtkScale  *scale,
                             gpointer   user_data);
void       gamma_changed    (GtkScale  *scale,
                             gpointer   user_data);

void       color_changed    (GtkColorButton  *chooser,
                             gpointer         user_data);

gchar     *uri_composer     (gchar    **path,
                             gchar     *prefix,
                             guint      cut);

GtkWidget *make_overlay (HyScanGtkWaterfall *wf,
                         gdouble             white,
                         gdouble             gamma);

void       open_db          (HyScanDB **db,
                             gchar    **old_uri,
                             gchar     *new_uri);


static HyScanGtkWaterfallState *wf_state;
static HyScanGtkWaterfall      *wf;
static HyScanGtkWaterfallGrid     *wf_grid;
static HyScanGtkWaterfallControl  *wf_ctrl;
static HyScanGtkWaterfallMark  *wf_mark;
static HyScanDB                *db;
static gchar                   *db_uri;
static gchar                   *project_dir;
static GtkWidget               *window;

int
main (int    argc,
      char **argv)
{
  GtkWidget *overlay;
  GtkWidget *wf_widget;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gchar *cache_prefix = "wf-test";

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
      {
        db_uri = g_strdup (args[1]);
      }

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
  wf_mark = hyscan_gtk_waterfall_mark_new (wf);

  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  hyscan_gtk_waterfall_state_set_cache (wf_state, cache, cache2, cache_prefix);

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

  xmlCleanupParser ();

  return 0;
}

GtkWidget*
make_overlay (HyScanGtkWaterfall *wf,
              gdouble             white,
              gdouble             gamma)
{
  GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *zoom_btn_in = gtk_button_new_from_icon_name ("gtk-zoom-in", GTK_ICON_SIZE_BUTTON);
  GtkWidget *zoom_btn_out = gtk_button_new_from_icon_name ("gtk-zoom-out", GTK_ICON_SIZE_BUTTON);
  GtkWidget *btn_reopen = gtk_button_new_from_icon_name ("folder-open", GTK_ICON_SIZE_BUTTON);
  GtkWidget *scale_white = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.001, 1.0, 0.005);
  GtkWidget *scale_gamma = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.5, 2.0, 0.005);

  GtkWidget *lay_ctrl = gtk_button_new_with_label ("ctrl");
  GtkWidget *lay_mark = gtk_button_new_with_label ("mark");
  GtkWidget *lay_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);

  GtkWidget *color_chooser = gtk_color_button_new ();
  GdkRGBA    rgba;

  gdk_rgba_parse (&rgba, "#00FFFF");
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_chooser), &rgba);
  g_object_set (color_chooser, "width-request", 2, NULL);

  /* Настраиваем прокрутки. */
  gtk_scale_set_value_pos (GTK_SCALE (scale_white), GTK_POS_LEFT);
  gtk_scale_set_value_pos (GTK_SCALE (scale_gamma), GTK_POS_LEFT);
  gtk_widget_set_size_request (scale_white, 150, 1);
  gtk_widget_set_size_request (scale_gamma, 150, 1);
  gtk_range_set_value (GTK_RANGE (scale_white), white);
  gtk_range_set_value (GTK_RANGE (scale_gamma), gamma);

  /* Layer control. */
  gtk_button_box_set_layout (GTK_BUTTON_BOX (lay_box), GTK_BUTTONBOX_EXPAND);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_ctrl, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (lay_box), lay_mark, TRUE, TRUE, 0);

  /* Кладём в коробку. */
  gtk_box_pack_start (GTK_BOX (box), zoom_btn_in,  FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), zoom_btn_out, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), btn_reopen,   FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), color_chooser,FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), scale_white,  FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), scale_gamma,  FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (box), lay_box,      FALSE, FALSE, 2);

  gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (wf));
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), box);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_END);
  // gtk_widget_set_margin_start (box, 5);
  // gtk_widget_set_margin_bottom (box, 5);

  g_signal_connect (btn_reopen, "clicked", G_CALLBACK (reopen_clicked), NULL);
  g_signal_connect (zoom_btn_in, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (1));
  g_signal_connect (zoom_btn_out, "clicked", G_CALLBACK (zoom_clicked), GINT_TO_POINTER (0));
  g_signal_connect (scale_white, "value-changed", G_CALLBACK (white_changed), scale_gamma);
  g_signal_connect (scale_gamma, "value-changed", G_CALLBACK (gamma_changed), scale_white);
  g_signal_connect (color_chooser, "color-set", G_CALLBACK (color_changed), NULL);
  g_signal_connect_swapped (lay_ctrl, "clicked", G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input), HYSCAN_GTK_WATERFALL_LAYER (wf_ctrl));
  g_signal_connect_swapped (lay_mark, "clicked", G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input), HYSCAN_GTK_WATERFALL_LAYER (wf_mark));


  gtk_widget_set_size_request (GTK_WIDGET (wf), 800, 600);
  return overlay;
}

void
reopen_clicked (GtkButton *button,
                gpointer   user_data)
{
  GtkWidget *dialog;
  GtkFileChooserAction act = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  gint result;
  gchar *path = NULL;
  gchar *track = NULL;
  gchar *project = NULL;
  gchar **split = NULL;
  guint len;

  dialog = gtk_file_chooser_dialog_new ("Select track", GTK_WINDOW (window), act,
                                        "Cancel", GTK_RESPONSE_CANCEL,
                                        "Open",   GTK_RESPONSE_ACCEPT,
                                        NULL);

  if (project_dir != NULL)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), project_dir);

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  if (result != GTK_RESPONSE_ACCEPT)
    goto cleanup;

  path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));

  #ifdef G_OS_WIN32
    split = g_strsplit (path, "\\", -1);
  #else
    split = g_strsplit (path, "/", -1);
  #endif

  len = g_strv_length (split);
  if (len < 2)
    goto cleanup;

  g_free (project_dir);
  project_dir = uri_composer (split, NULL, 1);

  open_db (&db, &db_uri, uri_composer (split, "file://", 2));

  project = split[len - 2];
  track = split[len - 1];

  hyscan_gtk_waterfall_state_set_track (wf_state, db, project, track, TRUE);

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
white_changed (GtkScale  *scale,
               gpointer   user_data)
{
  GtkWidget *other = user_data;
  gdouble white = gtk_range_get_value (GTK_RANGE (scale)),
          gamma = gtk_range_get_value (GTK_RANGE (other));

  hyscan_gtk_waterfall_set_levels_for_all (wf, 0.0, gamma, white);
}
void
gamma_changed (GtkScale  *scale,
               gpointer   user_data)
{
  GtkWidget *other = user_data;
  gdouble white = gtk_range_get_value (GTK_RANGE (other)),
          gamma = gtk_range_get_value (GTK_RANGE (scale));

  hyscan_gtk_waterfall_set_levels_for_all (wf, 0.0, gamma, white);
}

void
color_changed (GtkColorButton *chooser,
               gpointer        user_data)
{
  guint32 background, colors[2], *colormap;
  guint cmap_len;
  GdkRGBA rgba;

  if (chooser != NULL)
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (chooser), &rgba);
  else
    rgba = *(GdkRGBA*)user_data;

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

        #ifdef G_OS_WIN32
          written = g_snprintf (puri, 2048 - index, "%s\\", *path);
        #else
          written = g_snprintf (puri, 2048 - index, "/%s", *path);
        #endif
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
  if (new_uri == NULL)
    return;
  if (old_uri == NULL)
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
