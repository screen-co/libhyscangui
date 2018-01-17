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
                     const gchar * subtitle);

int
main (int argc, char **argv)
{
  HyScanDataBox *data_box;
  GType view_type;
  gchar *file = NULL;
  gchar *id = NULL;
  gchar *view = g_strdup ("list");
  gint   n_windows = 1;

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
    make_window (HYSCAN_PARAM (data_box), view_type, file, id);

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
             const gchar * subtitle)
{
  GtkWidget *window, *header;
  GtkWidget *frontend;

  /* Виджет отображения. */
  frontend = g_object_new (view_type, "param", backend, NULL);
  hyscan_gtk_param_set_watch_period (HYSCAN_GTK_PARAM (frontend), 1000);

  /* Хэдербар. */
  header = g_object_new (GTK_TYPE_HEADER_BAR,
                         "title", title,
                         "subtitle", subtitle,
                         "show-close-button", TRUE,
                         NULL);

  /* Настраиваем окошко. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 500);

  /* Показываем всё. */
  gtk_window_set_titlebar (GTK_WINDOW (window), header);
  gtk_container_add (GTK_CONTAINER (window), frontend);
  gtk_widget_show_all (window);
}

