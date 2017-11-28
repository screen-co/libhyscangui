#include <gtk/gtk.h>
#include <gtk-cifro-area.h>

static GtkWidget *window;
static GtkWidget *da;
static GtkWidget *r0;
static GtkWidget *r1;
static GtkWidget *s0;
static GtkWidget *s1;
static GtkWidget *s2;
static GtkWidget *ra;

static GtkWidget *cb0;
static GtkWidget *cb1;

static gdouble vr0;
static gdouble vr1;
static gdouble vs0;
static gdouble vs1;
static gdouble vs2;
static gdouble vra;

GdkRGBA vcb0, vcb1;

#define X (500)
#define X2 (X/2.0)

void v_changed (void);
void c_changed (void);
gboolean drw (GtkWidget *widget,
              cairo_t   *cr,
              gpointer   ud);

int
main (int    argc,
      char **argv)
{
  gtk_init (&argc, &argv);


  GtkWidget *box = gtk_grid_new ();
  // gtk_grid_set_row_homogeneous (GTK_GRID (box), TRUE);

  da = gtk_cifro_area_new ();

  gtk_widget_set_size_request (da, X, X);

  r0 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05);
  gtk_scale_set_value_pos (GTK_SCALE (r0), GTK_POS_LEFT);
  r1 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05);
  gtk_scale_set_value_pos (GTK_SCALE (r1), GTK_POS_LEFT);
  s0 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05);
  gtk_scale_set_value_pos (GTK_SCALE (s0), GTK_POS_LEFT);
  s1 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05);
  gtk_scale_set_value_pos (GTK_SCALE (s1), GTK_POS_LEFT);
  s2 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1.0, 0.05);
  gtk_scale_set_value_pos (GTK_SCALE (s2), GTK_POS_LEFT);
  ra = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, X2, 0.5);
  gtk_scale_set_value_pos (GTK_SCALE (ra), GTK_POS_LEFT);

  cb0 = gtk_color_button_new ();
  cb1 = gtk_color_button_new ();
  gdk_rgba_parse (&vcb0, "yellow");
  gdk_rgba_parse (&vcb1, "black");
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cb0), &vcb0);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cb1), &vcb1);

  gtk_grid_attach (GTK_GRID (box), da, 0/*left*/, 0/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("da"), 1/*left*/, 0/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), r0, 0/*left*/, 1/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("r0"), 1/*left*/, 1/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), r1, 0/*left*/, 2/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("r1"), 1/*left*/, 2/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), s0, 0/*left*/, 3/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("s0"), 1/*left*/, 3/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), s1, 0/*left*/, 4/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("s1"), 1/*left*/, 4/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), s2, 0/*left*/, 5/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("s2"), 1/*left*/, 5/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), ra, 0/*left*/, 6/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("ra"), 1/*left*/, 6/*top*/, 1, 1);

  gtk_grid_attach (GTK_GRID (box), cb0, 0/*left*/, 7/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("cb1"), 1/*left*/, 7/*top*/, 1, 1);
  gtk_grid_attach (GTK_GRID (box), cb1, 0/*left*/, 8/*top*/, 1, 1);
    gtk_grid_attach (GTK_GRID (box), gtk_label_new ("cb1"), 1/*left*/, 8/*top*/, 1, 1);

  g_signal_connect (r0, "value-changed", G_CALLBACK (v_changed), NULL);
  g_signal_connect (r1, "value-changed", G_CALLBACK (v_changed), NULL);
  g_signal_connect (s0, "value-changed", G_CALLBACK (v_changed), NULL);
  g_signal_connect (s1, "value-changed", G_CALLBACK (v_changed), NULL);
  g_signal_connect (s2, "value-changed", G_CALLBACK (v_changed), NULL);
  g_signal_connect (ra, "value-changed", G_CALLBACK (v_changed), NULL);

  g_signal_connect (cb0, "color-set", G_CALLBACK (c_changed), NULL);
  g_signal_connect (cb1, "color-set", G_CALLBACK (c_changed), NULL);

  g_signal_connect (da, "visible-draw", G_CALLBACK (drw), NULL);

  /* Основное окно программы. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);

  /* Начинаем работу. */
  gtk_main ();

  return 0;
}

void
v_changed (void)
{
  vra = gtk_range_get_value (GTK_RANGE (ra));

  vr0 = gtk_range_get_value (GTK_RANGE (r0)) * vra;
  vr1 = gtk_range_get_value (GTK_RANGE (r1)) * vra;
  vs0 = gtk_range_get_value (GTK_RANGE (s0));
  vs1 = gtk_range_get_value (GTK_RANGE (s1));
  vs2 = gtk_range_get_value (GTK_RANGE (s2));

  gtk_widget_queue_draw (da);
}

void
c_changed (void)
{
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cb0), &vcb0);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cb1), &vcb1);
  gtk_widget_queue_draw (da);
}


void draw_gradient1(cairo_t *cr)
{
  cairo_pattern_t *pattern;

  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_set_line_width (cr, 12);
  cairo_move_to (cr, 10, 10);

  pattern = cairo_pattern_create_radial(X2, X2, vr0, X2, X2, vr1);

  cairo_pattern_add_color_stop_rgba (pattern, vs0, vcb0.red, vcb0.green, vcb0.blue, vcb0.alpha);
  cairo_pattern_add_color_stop_rgba (pattern, vs1, vcb1.red, vcb1.green, vcb1.blue, vcb1.alpha);
  cairo_pattern_add_color_stop_rgba (pattern, vs2, vcb1.red, vcb1.green, vcb1.blue, 0.0);

  cairo_set_source (cr, pattern);
  cairo_arc (cr, X2, X2, vra, 0, G_PI * 2);
  cairo_fill (cr);

  cairo_pattern_destroy(pattern);
}

gboolean drw (GtkWidget *widget,
              cairo_t   *cr,
              gpointer   ud)
{
  draw_gradient1 (cr);
  return TRUE;
}
