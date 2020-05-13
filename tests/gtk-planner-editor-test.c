/* gtk-planner-editor-test.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#include <hyscan-gtk-planner-editor.h>
#include <hyscan-data-writer.h>
#include <hyscan-gtk-planner-list.h>
#include <hyscan-cached.h>

#define PROJECT_NAME "gtk-planner-editor"

static gchar *
create_project (HyScanDB *db)
{
  GDateTime *date_time;
  gchar *project;
  HyScanDataWriter *writer;

  /* Создаём проект. */
  date_time = g_date_time_new_now_local ();
  project = g_strdup_printf (PROJECT_NAME"-%ld", g_date_time_to_unix (date_time));

  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  hyscan_data_writer_start (writer, project, project, HYSCAN_TRACK_SURVEY, NULL, -1);
  hyscan_data_writer_stop (writer);

  g_object_unref (writer);
  g_date_time_unref (date_time);

  return project;
}

static void
create_tracks (HyScanPlannerModel *model)
{
  HyScanPlannerTrack track = { .type = HYSCAN_TYPE_PLANNER_TRACK };
  HyScanGeoPoint center = { 50.0, 50.0 };
  gint i;

  for (i = 0; i < 5; i++)
    {
      track.number = i;
      track.plan.speed = 1.0;
      track.plan.start = center;
      track.plan.start.lat += 1e-4 * i;
      track.plan.start.lon += 1e-4 * i;

      track.plan.end = center;
      track.plan.end.lat += 1e-4 * (i + 1);
      track.plan.end.lon += 1e-4 * (i + 1);

      hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (model), (const HyScanObject *) &track);
    }
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window, *box, *editor, *list;

  gchar *db_uri;
  gchar *project_name;
  HyScanDB *db;

  HyScanCache *cache;
  HyScanDBInfo *db_info;
  HyScanPlannerSelection *selection;
  HyScanPlannerModel *model;
  HyScanPlannerStats *stats;
  HyScanTrackStats *track_stats;

  gtk_init (&argc, &argv);

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Failed to connect db");

  project_name = create_project (db);

  /* Создаём модели. */
  model = hyscan_planner_model_new ();
  db_info = hyscan_db_info_new (db);
  cache = HYSCAN_CACHE (hyscan_cached_new (100));
  track_stats = hyscan_track_stats_new (db_info, NULL, cache);
  stats = hyscan_planner_stats_new (track_stats, model);
  selection = hyscan_planner_selection_new (model);

  hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (model), db, project_name);
  hyscan_db_info_set_project (db_info, project_name);

  create_tracks (model);

  list = hyscan_gtk_planner_list_new (model, selection, stats, NULL);
  editor = hyscan_gtk_planner_editor_new (model, selection);
  g_object_set (editor, "margin", 6, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (box), list, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), editor, TRUE, TRUE, 0);

  /* Главное окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect_swapped (window, "destroy", G_CALLBACK (gtk_main_quit), window);

  gtk_widget_show_all (window);

  gtk_main ();

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_free (db_uri);
  g_free (project_name);
  g_object_unref (db);
  g_object_unref (db_info);
  g_object_unref (model);
  g_object_unref (track_stats);
  g_object_unref (stats);
  g_object_unref (cache);
  g_object_unref (selection);
}
