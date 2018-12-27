#include <hyscan-gtk-spin-button.h>

GtkWidget * hex;
GtkWidget * bin;
GtkWidget * dec;
gboolean show_prefix;

static void
switch_show_prefix (GtkButton *button)
{
  show_prefix = !show_prefix;

  hyscan_gtk_spin_button_set_show_prefix (HYSCAN_GTK_SPIN_BUTTON (bin), show_prefix);
  hyscan_gtk_spin_button_set_show_prefix (HYSCAN_GTK_SPIN_BUTTON (dec), show_prefix);
  hyscan_gtk_spin_button_set_show_prefix (HYSCAN_GTK_SPIN_BUTTON (hex), show_prefix);
}

static void
value_changed (GtkSpinButton *spin,
               GtkButton     *button)
{
  gdouble value = gtk_spin_button_get_value (spin);
  gchar *text = g_strdup_printf ("%f", value);
  gtk_button_set_label (button, text);
  g_free (text);
}

int
main (int argc, char **argv)
{
  GtkAdjustment * adj;
  GtkWidget * window;
  GtkWidget * button;
  GtkWidget * box;

  gtk_init (&argc, &argv);

  adj = gtk_adjustment_new (0, -65536, 65537, 1, 1, 1);

  bin = g_object_new (HYSCAN_TYPE_GTK_SPIN_BUTTON, "adjustment", adj, "base", 2, NULL);
  dec = g_object_new (HYSCAN_TYPE_GTK_SPIN_BUTTON, "adjustment", adj, "base", 10, NULL);
  hex = g_object_new (HYSCAN_TYPE_GTK_SPIN_BUTTON, "adjustment", adj, "base", 16, NULL);

  {
    GtkEntryBuffer * buffer = gtk_entry_buffer_new (NULL, -1);
    gtk_entry_set_buffer (GTK_ENTRY (hex), buffer);
  }

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  button = gtk_button_new ();
  g_signal_connect (bin, "value-changed", G_CALLBACK (value_changed), button);
  g_signal_connect (hex, "value-changed", G_CALLBACK (value_changed), button);
  g_signal_connect (dec, "value-changed", G_CALLBACK (value_changed), button);
  g_signal_connect (button, "clicked", G_CALLBACK (switch_show_prefix), NULL);

  gtk_box_pack_start (GTK_BOX (box), bin, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (box), dec, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (box), hex, TRUE, TRUE, 2);
  gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 2);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 150, -1);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
