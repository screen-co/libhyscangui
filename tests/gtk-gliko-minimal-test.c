#include <stdio.h>
#include <stdlib.h>

#include <hyscan-gtk-gliko-minimal.h>

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *minimal;

  if (!gtk_init_check (&argc, &argv))
    {
      fputs ("Could not initialize GTK", stderr);
      return EXIT_FAILURE;
    }
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  minimal = hyscan_gtk_gliko_minimal_new ();
  gtk_container_add (GTK_CONTAINER (window), minimal);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);
  gtk_main ();
  return EXIT_SUCCESS;
}
