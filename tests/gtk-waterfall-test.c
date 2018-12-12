#include <hyscan-gtk-waterfall.h>
#include <hyscan-gtk-waterfall-control.h>
#include <hyscan-gtk-waterfall-grid.h>
#include <hyscan-gtk-waterfall-mark.h>
#include <hyscan-gtk-waterfall-magnifier.h>
#include <hyscan-tile-color.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>

#define MNEMONIC(x) hyscan_gtk_waterfall_layer_get_mnemonic (HYSCAN_GTK_WATERFALL_LAYER (x))

gdouble speed = 1.0;

gboolean
spdch (HyScanGtkWaterfallState *state)
{
  speed *= 0.9;
  hyscan_gtk_waterfall_state_set_ship_speed (state, speed);
  return FALSE;
}

GtkWidget*
make_layer_btn (HyScanGtkWaterfallLayer *layer,
                const gchar             *text)
{
  GtkWidget *button;
  GtkWidget *image;
  const gchar *icon;

  icon = hyscan_gtk_waterfall_layer_get_mnemonic (layer);
  image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON);

  button = gtk_button_new ();

  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_button_set_label (GTK_BUTTON (button), text);

  return button;
}

int
main (int    argc,
      char **argv)
{

  GtkWidget *window = NULL;
  GtkWidget *wf_widget = NULL;
  GtkWidget *gtkgrid = NULL;
  HyScanGtkWaterfall *wf = NULL;
  HyScanGtkWaterfallState *wf_state = NULL;
  HyScanGtkWaterfallControl *control = NULL;
  HyScanGtkWaterfallGrid *grid = NULL;
  HyScanGtkWaterfallMark *mark = NULL;
  HyScanGtkWaterfallMagnifier *loopa = NULL;

  HyScanMarkModel *markmodel;

  gchar *project_name;
  gchar *track_name;
  gchar *db_uri;
  gchar *title;
  gboolean use_computed = FALSE;
  HyScanDB *db;
  gdouble black = 0.0;
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));

  guint32 background, colors[2], *colormap;
  guint cmap_len;

  gtk_init (&argc, &argv);
  {
    GtkAdjustment * adj =  gtk_adjustment_new (0, 0, 1, 2, 3, 4);
    GtkWidget * sb =  gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adj);

    GValue gv = {0};
    g_value_init (&gv, G_TYPE_INT);
    g_message ("%s", g_type_name (G_TYPE_INT));

    gtk_widget_style_get_property (sb, "slider-width", &gv);

    g_message ("%i", g_value_get_int (&gv));

  }
  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name;",          NULL},
      { "track",       't', 0, G_OPTION_ARG_STRING, &track_name,   "Track name;",            NULL},
      { "computed",    'c', 0, G_OPTION_ARG_NONE  , &use_computed, "Use computed data;",     NULL},
      { "speed",       's', 0, G_OPTION_ARG_DOUBLE, &speed,        "Speed of ship;",         NULL},
      { "black",       'b', 0, G_OPTION_ARG_DOUBLE, &black,        "black level (0.0-1.0);", NULL},
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

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  markmodel = hyscan_mark_model_new ();


  /* Водопад. */
  wf_widget = hyscan_gtk_waterfall_new (cache);
  wf_state = HYSCAN_GTK_WATERFALL_STATE (wf_widget);
  wf = HYSCAN_GTK_WATERFALL (wf_widget);

  control = hyscan_gtk_waterfall_control_new (wf);
  grid = hyscan_gtk_waterfall_grid_new (wf);
  mark = hyscan_gtk_waterfall_mark_new (wf, markmodel);
  loopa = hyscan_gtk_waterfall_magnifier_new (wf);

  hyscan_gtk_waterfall_layer_grab_input (HYSCAN_GTK_WATERFALL_LAYER (control));

  hyscan_gtk_waterfall_state_sidescan (wf_state, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  hyscan_gtk_waterfall_state_echosounder (wf_state, HYSCAN_SOURCE_ECHOSOUNDER);
  hyscan_gtk_waterfall_state_set_ship_speed (wf_state, speed);

  hyscan_gtk_waterfall_set_upsample (wf, 2);

  background = hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0);
  colors[0]  = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
  colors[1]  = hyscan_tile_color_converter_d2i (0.0, 1.0, 1.0, 1.0);
  colormap   = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);

  /* Устанавливаем цветовую схему и подложку. */
  hyscan_gtk_waterfall_set_colormap_for_all (wf, colormap, cmap_len, background);
  hyscan_gtk_waterfall_set_levels_for_all (wf, black, gamma, white);
  hyscan_gtk_waterfall_set_substrate (wf, background);

  /* Основное окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  title = g_strdup_printf ("%s/%s/%s", db_uri, project_name, track_name);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  {
    GtkWidget *bbox;
    GtkWidget *l_control = make_layer_btn (HYSCAN_GTK_WATERFALL_LAYER (control), "control");
    GtkWidget *l_mark = make_layer_btn (HYSCAN_GTK_WATERFALL_LAYER (mark), "mark");
    g_signal_connect_swapped (l_control, "clicked",
                              G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input),
                              HYSCAN_GTK_WATERFALL_LAYER (control));
    g_signal_connect_swapped (l_mark, "clicked",
                              G_CALLBACK (hyscan_gtk_waterfall_layer_grab_input),
                              HYSCAN_GTK_WATERFALL_LAYER (mark));


    gtkgrid = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_CENTER);

    gtk_box_pack_start (GTK_BOX (bbox), l_mark, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), l_control, TRUE, FALSE, 0);

    GtkWidget *spd = gtk_button_new_with_label ("speed * 0.9");
    g_signal_connect_swapped (spd, "clicked", G_CALLBACK (spdch), wf_state);
    gtk_box_pack_end (GTK_BOX (bbox), spd, TRUE, FALSE, 0);

    gtk_widget_set_size_request (wf_widget, 800, 600);

    gtk_box_pack_start (GTK_BOX (gtkgrid), wf_widget, TRUE, TRUE, 2);
    gtk_box_pack_end (GTK_BOX (gtkgrid), bbox, FALSE, FALSE, 2);
  }
  // gtk_container_add (GTK_CONTAINER (window), wf_widget);
  gtk_container_add (GTK_CONTAINER (window), gtkgrid);
  gtk_widget_show_all (window);



  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  /* !use_computed == use_raw. */
  hyscan_gtk_waterfall_state_set_track (wf_state, db, project_name, track_name);
  hyscan_mark_model_set_project (markmodel, db, project_name);

  /* Начинаем работу. */
  gtk_main ();

  g_clear_object (&control);
  g_clear_object (&grid);
  g_clear_object (&loopa);

  g_clear_object (&cache);
  g_clear_object (&markmodel);
  g_clear_object (&db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);
  g_free (title);
  g_free (colormap);

  xmlCleanupParser ();

  return 0;
}
