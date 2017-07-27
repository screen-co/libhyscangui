#include <gtk/gtk.h>
#include <hyscan-gtk-area.h>

void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data )
{
  gtk_main_quit ();
}

int main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *area;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 500);

  area = hyscan_gtk_area_new ();

  hyscan_gtk_area_set_central (HYSCAN_GTK_AREA (area),
    gtk_image_new_from_icon_name ("emblem-important-symbolic", GTK_ICON_SIZE_DIALOG));

  hyscan_gtk_area_set_left (HYSCAN_GTK_AREA (area),
    gtk_image_new_from_icon_name ("emblem-photos-symbolic", GTK_ICON_SIZE_DIALOG));

  hyscan_gtk_area_set_right (HYSCAN_GTK_AREA (area),
    gtk_image_new_from_icon_name ("emblem-system-symbolic", GTK_ICON_SIZE_DIALOG));

  hyscan_gtk_area_set_top (HYSCAN_GTK_AREA (area),
    gtk_image_new_from_icon_name ("emblem-ok-symbolic", GTK_ICON_SIZE_DIALOG));

  hyscan_gtk_area_set_bottom (HYSCAN_GTK_AREA (area),
    gtk_image_new_from_icon_name ("emblem-shared-symbolic", GTK_ICON_SIZE_DIALOG));

  gtk_container_add (GTK_CONTAINER (window), area);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
