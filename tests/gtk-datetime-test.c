#include <hyscan-gtk-datetime.h>

GtkWidget *hs_date;
GtkWidget *hs_time;
GtkWidget *hs_both;

/* Функция задает текущее время. */
void
now (GtkButton *button)
{
  GDateTime *dt = g_date_time_new_now_local ();
  gint64 unixtime = g_date_time_to_unix (dt);

  hyscan_gtk_datetime_set_time (HYSCAN_GTK_DATETIME (hs_date), unixtime);
  hyscan_gtk_datetime_set_time (HYSCAN_GTK_DATETIME (hs_time), unixtime);
  hyscan_gtk_datetime_set_time (HYSCAN_GTK_DATETIME (hs_both), unixtime);

  g_date_time_unref (dt);
}

/* Уведомление об изменении времени. */
void
notify_time (GObject    *object,
             GParamSpec *pspec)
{
  gint64 val = -1;

  g_object_get (object, pspec->name, &val, NULL);
  g_message ("\"notify::time\": %p, %li", object, val);
}

/* Функция создает виджет и упаковывает его в бокс. */
GtkWidget *
create_and_pack (HyScanGtkDateTimeMode  mode,
                 GtkWidget             *box)
{
  GtkWidget *widget = hyscan_gtk_datetime_new (mode);
  GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

  g_signal_connect (widget, "notify::time", G_CALLBACK (notify_time), NULL);
  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), separator, TRUE, TRUE, 0);

  return widget;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *box, *button;

  gtk_init (&argc, &argv);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  hs_date = create_and_pack (HYSCAN_GTK_DATETIME_DATE, box);
  hs_time = create_and_pack (HYSCAN_GTK_DATETIME_TIME, box);
  hs_both = create_and_pack (HYSCAN_GTK_DATETIME_BOTH, box);

  button = gtk_button_new_with_label ("Set current time");
  g_signal_connect (button, "clicked", G_CALLBACK (now), NULL);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
