#include <stdio.h>
#include <stdlib.h>

#include <hyscan-gtk-gliko-area.h>
#include <hyscan-gtk-gliko-grid.h>
#include <hyscan-gtk-gliko-overlay.h>

typedef float sample_t;

gint32 num_azimuthes;
guint32 iko_length;
float rotation;
float scale;
float cx;
float cy;
float contrast;
float brightness;
float fade_coef;
sample_t *buffer;

static void
fill_buffer (sample_t *buffer, const sample_t value, const int length)
{
  int i;

  for (i = 0; i < length; i++)
    buffer[i] = value;
}

static gboolean
update_timeout (gpointer iko)
{
  static int a = 0;
  static int d = 0;

  fill_buffer (buffer, 0.5f, iko_length);
  buffer[(d + 0) % iko_length] = 1.0f;
  buffer[(d + 1) % iko_length] = 1.0f;
  buffer[(d + 2) % iko_length] = 1.0f;
  buffer[(d + 3) % iko_length] = 1.0f;
  hyscan_gtk_gliko_area_set_data (HYSCAN_GTK_GLIKO_AREA (iko), 0, a % num_azimuthes, buffer);
  if ((a & 0xF) == 0)
    {
      hyscan_gtk_gliko_area_fade (HYSCAN_GTK_GLIKO_AREA (iko));
    }
  a++;
  d++;
  return TRUE;
}

static void
destroy_timeout (GtkWidget *widget, gpointer pv)
{
  g_source_remove (*((guint *) pv));
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *overlay;
  HyScanGtkGlikoArea *iko;
  HyScanGtkGlikoGrid *grid;
  guint timer_id;

  num_azimuthes = 1024;
  iko_length = 1024;
  rotation = 0.0f;
  scale = 1.0f;
  cx = 0.0f;
  cy = 0.0f;
  contrast = 0.0f;
  brightness = 0.0f;
  fade_coef = 0.98f;
  buffer = g_malloc0 (iko_length * sizeof (unsigned char));

  if (!gtk_init_check (&argc, &argv))
    {
      fputs ("Could not initialize GTK", stderr);
      return EXIT_FAILURE;
    }
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  overlay = hyscan_gtk_gliko_overlay_new ();
  iko = hyscan_gtk_gliko_area_new ();
  grid = hyscan_gtk_gliko_grid_new ();

  hyscan_gtk_gliko_overlay_set_layer (HYSCAN_GTK_GLIKO_OVERLAY (overlay), 0, HYSCAN_GTK_GLIKO_LAYER (iko));
  hyscan_gtk_gliko_overlay_enable_layer (HYSCAN_GTK_GLIKO_OVERLAY (overlay), 0, 1);
  hyscan_gtk_gliko_overlay_set_layer (HYSCAN_GTK_GLIKO_OVERLAY (overlay), 1, HYSCAN_GTK_GLIKO_LAYER (grid));
  hyscan_gtk_gliko_overlay_enable_layer (HYSCAN_GTK_GLIKO_OVERLAY (overlay), 1, 1);

  g_object_set (iko, "gliko-color1-rgb", "#00FF80", NULL);
  g_object_set (iko, "gliko-color1-alpha", 1.0f, NULL);
  g_object_set (iko, "gliko-color2-rgb", "#FF8000", NULL);
  g_object_set (iko, "gliko-color2-alpha", 1.0f, NULL);
  g_object_set (iko, "gliko-background-rgb", "#404040", NULL);
  g_object_set (iko, "gliko-background-alpha", 0.25f, NULL);

  g_object_set (iko, "gliko-fade-coef", fade_coef, NULL);
  g_object_set (iko, "gliko-scale", scale, NULL);
  g_object_set (grid, "gliko-scale", scale, NULL);

  hyscan_gtk_gliko_area_init_dimension (HYSCAN_GTK_GLIKO_AREA (iko), num_azimuthes, iko_length);

  gtk_container_add (GTK_CONTAINER (window), overlay);
  gtk_widget_set_events (overlay, GDK_EXPOSURE_MASK);
  timer_id = g_timeout_add (10, update_timeout, iko);
  g_signal_connect (window, "destroy", G_CALLBACK (destroy_timeout), &timer_id);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);
  gtk_main ();
  return EXIT_SUCCESS;
}
