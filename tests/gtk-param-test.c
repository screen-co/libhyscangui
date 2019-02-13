#include <hyscan-data-schema.h>
#include <hyscan-data-box.h>
#include <libxml/parser.h>
#include <hyscan-gtk-param-tree.h>
#include <hyscan-gtk-param-list.h>
#include <hyscan-gtk-param-cc.h>

GType find_function (const gchar * view);
void  make_window   (HyScanParam * backend,
                     GType         view_type,
                     const gchar * title,
                     const gchar * subtitle,
                     gboolean      hidden,
                     const gchar * root);
void  make_buttons  (HyScanGtkParam * frontend,
                     GtkGrid        * container);
void  save_and_exit (HyScanGtkParam *param);
void  reset_and_exit (HyScanGtkParam *param);

int
main (int argc, char **argv)
{
  HyScanDataBox *data_box;
  GType view_type;
  gchar *file = NULL;
  gchar *id = NULL;
  gchar *view = g_strdup ("list");
  gint   n_windows = 1;
  gboolean hidden = FALSE;
  gchar *root = NULL;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "id",     'i', 0, G_OPTION_ARG_STRING, &id,   "Schema id to show", NULL },
        { "view",   'v', 0, G_OPTION_ARG_STRING, &view, "View type (list, tree, cc)", NULL },
        { "unhide", 'u', 0, G_OPTION_ARG_NONE,   &hidden, "Make hidden keys visible", NULL },
        { "root",   'r', 0, G_OPTION_ARG_STRING, &root, "Root node", NULL },
        { "windows",'n', 0, G_OPTION_ARG_INT,    &n_windows, "Number of windows", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("file");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }
    if (g_strv_length (args) < 2)
      {
        g_print ("%s\n", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    file = g_strdup (args[1]);

    g_option_context_free (context);
    g_strfreev (args);
  }

  /* Определяем, как отображать схему. */
  view_type = find_function (view);
  if (view_type == G_TYPE_INVALID)
    {
      g_print ("\"%s\" is not a valid view type.\n", view);
      return -1;
    }

  /* Создаем HyScanParam. */
  data_box = hyscan_data_box_new_from_file (file, id);

  /* Создаем окна. */
  for (; n_windows > 0; --n_windows)
    make_window (HYSCAN_PARAM (data_box), view_type, file, id, hidden, root);

  gtk_main ();

  g_object_unref (data_box);
  xmlCleanupParser ();
  g_free (file);
  g_free (id);
  g_free (view);

  return 0;
}

/* Функция определяет тип виджета. */
GType
find_function (const gchar * view)
{
  if (g_strcmp0 (view, "cc") == 0)
    return HYSCAN_TYPE_GTK_PARAM_CC;
  else if (g_strcmp0 (view, "list") == 0)
    return HYSCAN_TYPE_GTK_PARAM_LIST;
  else if (g_strcmp0 (view, "tree") == 0)
    return HYSCAN_TYPE_GTK_PARAM_TREE;
  else
    return G_TYPE_INVALID;
}

/* Функция создает окно с виджетом. */
void
make_window (HyScanParam * backend,
             GType         view_type,
             const gchar * title,
             const gchar * subtitle,
             gboolean      hidden,
             const gchar * root)
{
  GtkWidget *window, *header, *grid;
  GtkWidget *frontend;

  /* Виджет отображения. */
  frontend = g_object_new (view_type,
                           "param", backend,
                           "hidden", hidden,
                           "root", root,
                           NULL);
  hyscan_gtk_param_set_watch_period (HYSCAN_GTK_PARAM (frontend), 1000);

  /* Хэдербар. */
  header = g_object_new (GTK_TYPE_HEADER_BAR,
                         "title", title,
                         "subtitle", subtitle,
                         "show-close-button", TRUE,
                         NULL);

  /* Сетка, в которую мы всё упакуем. */
  grid = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (grid), frontend, 0, 0, 4, 1);
  make_buttons (HYSCAN_GTK_PARAM (frontend), GTK_GRID (grid));


  /* Настраиваем окошко. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 500);

  /* Показываем всё. */
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);
}

void
make_buttons (HyScanGtkParam * frontend,
              GtkGrid        * container)
{
  GtkWidget *abar;
  GtkWidget *apply, *discard, *ok, *exit;
  GtkSizeGroup * size;

  apply = gtk_button_new_with_label ("Применить");
  discard = gtk_button_new_with_label ("Откатить");
  ok = gtk_button_new_with_label ("Ок");
  exit = gtk_button_new_with_label ("Выйти");

  g_signal_connect_swapped (apply, "clicked", G_CALLBACK (hyscan_gtk_param_apply), frontend);
  g_signal_connect_swapped (discard, "clicked", G_CALLBACK (hyscan_gtk_param_discard), frontend);
  g_signal_connect_swapped (ok, "clicked", G_CALLBACK (save_and_exit), frontend);
  g_signal_connect_swapped (exit, "clicked", G_CALLBACK (reset_and_exit), frontend);

  abar = gtk_action_bar_new ();

  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), apply);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), discard);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), ok);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), exit);

  gtk_grid_attach (container, abar, 0, 1, 4, 1);

  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), apply);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), discard);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), ok);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), exit);
}

void
save_and_exit (HyScanGtkParam *param)
{
  hyscan_gtk_param_apply (param);
  gtk_main_quit ();
}

void
reset_and_exit (HyScanGtkParam *param)
{
  hyscan_gtk_param_discard (param);
  gtk_main_quit ();
}
