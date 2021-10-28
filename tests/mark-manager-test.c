/* mark-manager-test.c
 *
 * Copyright 2020 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */
#include <gtk/gtk.h>
#ifdef G_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <hyscan-cached.h>
#include <hyscan-gtk-mark-manager.h>
/* Экземпляры Журнала Меток. */
typedef enum
{
  FIRST,      /* Первый. */
  SECOND,     /* Второй. */
  THIRD,      /* Третий. */
  INSTANCES   /* Количество экземпляров. */
}MarkManagerInstance;
/* Функция-обработчик сигнала о двойном клике по объекту Журнала Меток. */
static void
on_double_click (HyScanGtkModelManager *model_manager,
                 gchar                 *id,
                 guint                  type,
                 gpointer              *user_data)
{
  g_print ("Double click on object [id: %s, type: %u].\n", id, type);
}
/* Главная функция.*/
int
main (int    argc,
      char **argv)
{
  HyScanDB *db = NULL;
  HyScanCache *cache = NULL;
  HyScanGtkModelManager *model_manager;
  GtkWidget *widget,
            *layer,
            *mark_manager[INSTANCES];
  GError *error = NULL;
  GOptionContext *context;
  gchar *db_uri = NULL,
        *project_name = NULL,
        *desktop_dir;
  GOptionEntry entries[] = {
    {"db-uri",       'd', 0, G_OPTION_ARG_STRING, &db_uri,       "Database uri", NULL},
    {"project-name", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL},
    {NULL}
  };
  /* Инициализация GTK. */
  gtk_init (&argc, &argv);
  /* Разбор аргументов командной строки. */
  context = g_option_context_new ("");
  g_option_context_set_help_enabled (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_ignore_unknown_options (context, FALSE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_message ("%s", error->message);
      return -1;
    }
  /* Проверка пути к базе данных. */
  if (db_uri == NULL)
    {
      g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
      return -1;
    }
  /* Проверка названия проекта. */
  if (project_name == NULL)
    {
      g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
      return -1;
    }

  g_option_context_free (context);
  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);

  if (db == NULL)
    {
      g_error ("can't open db at: %s", db_uri);
      goto terminate;
    }

  g_print ("DB opened seccessfully.\nProject %s opened\n", project_name);

  cache = HYSCAN_CACHE (hyscan_cached_new (255));
  desktop_dir = (gchar*)g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  model_manager = hyscan_gtk_model_manager_new (project_name, db, cache, desktop_dir);

  g_print ("Testing...\n");

  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (widget), 900, 600);
  gtk_window_set_title (GTK_WINDOW (widget), "Mark Manager Test");

  layer = gtk_grid_new ();
  gtk_grid_set_row_homogeneous    (GTK_GRID (layer), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (layer), TRUE);
  gtk_grid_set_row_spacing        (GTK_GRID (layer), 10);
  gtk_grid_set_column_spacing     (GTK_GRID (layer), 10);

  mark_manager[FIRST]  = hyscan_gtk_mark_manager_new (model_manager);
  gtk_grid_attach (GTK_GRID (layer), mark_manager[FIRST], 0, 0, 1, 2);

  mark_manager[SECOND] = hyscan_gtk_mark_manager_new (model_manager);
  gtk_grid_attach (GTK_GRID (layer), mark_manager[SECOND], 0, 2, 1, 1);

  mark_manager[THIRD]  = hyscan_gtk_mark_manager_new (model_manager);
  gtk_grid_attach (GTK_GRID (layer), mark_manager[THIRD], 2, 0, 2, 3);

  gtk_container_set_border_width (GTK_CONTAINER (widget), 10);
  gtk_container_add (GTK_CONTAINER (widget), layer);

  g_signal_connect (G_OBJECT (widget), "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (model_manager,
                    hyscan_gtk_model_manager_get_signal_title (model_manager,
                                                               HYSCAN_MODEL_MANAGER_SIGNAL_SHOW_OBJECT),
                    G_CALLBACK (on_double_click),
                    NULL);

  gtk_widget_show_all (widget);

  gtk_main ();

  g_object_unref (cache);
  g_object_unref (model_manager);
  g_print ("Done!\n");

terminate:
  g_free (project_name);
  g_free (db_uri);

  return 0;
}

#ifdef G_OS_WIN32
/* Точка входа для ОС Windows. */
int WINAPI
WinMain (HINSTANCE hInt,
         HINSTANCE hPrevInst,
         LPSTR     lpCmdLine,
         int       nCmdShow)
{
  /* Аргументы командной строки. */
  gchar **argv = g_win32_get_command_line ();
  guint   argc = g_strv_length (argv);
  /* Запуск главной функции. */
  gint result = main (argc, argv);
  /* Освобождаем аргументы командной строки. */
  g_strfreev (argv);
  /* Завершение. */
  return result;
}
#endif
