/* hyscan-gtk-planner-import.c
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

/**
 * SECTION: hyscan-gtk-planner-import
 * @Short_description: Виджет редактора параметров плановых галсов
 * @Title: HyScanGtkPlannerImport
 *
 * Виджет HyScanGtkPlannerImport предназначен для импорта данных планировщика из
 * XML-файла в проект базы данных. База данных и проект указываются при создании
 * виджета: hyscan_gtk_planner_import_new().
 *
 * Виджет позволяет выбрать файл для импорта и настроить параметры импорта.
 * Если пользователь установил валидный файл для импорта, то свойство #HyScanGtkPlannerImport:ready
 * будет установлено в %TRUE.
 *
 * Для непосредственно записи данных в БД необходимо вызвать функцию
 * hyscan_gtk_planner_import_start().
 *
 * Предполагается, что пользователь виджета самостоятельно добавит кнопку для
 * импорта:
 *
 * |[<!-- language="C" -->
 *   GtkWidget *import, *button;
 *
 *   import = hyscan_gtk_planner_import_new (db, project_name);
 *   button = gtk_button_new_with_label ("Import");
 *
 *   g_signal_connect_swapped (button, "clicked", G_CALLBACK (hyscan_gtk_planner_import_start), import);
 *   g_object_bind_property (import, "ready", button, "sensitive", G_BINDING_SYNC_CREATE);
 * ]|
 *
 */

#include "hyscan-gtk-planner-import.h"
#include <glib/gi18n-lib.h>
#include <hyscan-planner-export.h>
#include <hyscan-object-data.h>
#include <hyscan-planner.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_READY,
};

struct _HyScanGtkPlannerImportPrivate
{
  HyScanDB              *db;                 /* База данных. */
  gchar                 *project;            /* Имя проекта. */
  GCancellable          *cancellable;        /* Объект для отмены задачи импорта. */
  gboolean               ready;              /* Признак того, что можно импортировать. */
  gboolean               running;            /* Признак того, что идёт импорт. */

  GHashTable            *objects;            /* Объекты планировщика для импорта*/
  gboolean               replace;            /* Признак того, что необходимо удалить данных планировщика в БД. */

  GtkWidget             *label;              /* GtkLabel со статусом импорта. */
  GtkWidget             *file_chooser;       /* GtkFileChooserButton кнопка для выбора файла. */
  GtkWidget             *replace_chkbox;     /* GtkCheckButton кнопка для установки параметра replace. */
};

static void        hyscan_gtk_planner_import_set_property          (GObject                *object,
                                                                    guint                   prop_id,
                                                                    const GValue           *value,
                                                                    GParamSpec             *pspec);
static void        hyscan_gtk_planner_import_get_property          (GObject                *object,
                                                                    guint                   prop_id,
                                                                    GValue                 *value,
                                                                    GParamSpec             *pspec);
static void        hyscan_gtk_planner_import_object_constructed    (GObject                *object);
static void        hyscan_gtk_planner_import_object_finalize       (GObject                *object);
static void        hyscan_gtk_planner_import_file_set              (HyScanGtkPlannerImport *import);

static GParamSpec *hyscan_gtk_planner_import_pspec_ready;

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerImport, hyscan_gtk_planner_import, GTK_TYPE_BOX)

static void
hyscan_gtk_planner_import_class_init (HyScanGtkPlannerImportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_import_set_property;
  object_class->get_property = hyscan_gtk_planner_import_get_property;

  object_class->constructed = hyscan_gtk_planner_import_object_constructed;
  object_class->finalize = hyscan_gtk_planner_import_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Database", "HyScanDB object to import into", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name to import into", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_gtk_planner_import_pspec_ready =
    g_param_spec_boolean ("ready", "Ready to import", "Whether the import can be started", FALSE,
                          G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_READY, hyscan_gtk_planner_import_pspec_ready);
}

static void
hyscan_gtk_planner_import_init (HyScanGtkPlannerImport *gtk_planner_import)
{
  gtk_planner_import->priv = hyscan_gtk_planner_import_get_instance_private (gtk_planner_import);
}

static void
hyscan_gtk_planner_import_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerImport *gtk_planner_import = HYSCAN_GTK_PLANNER_IMPORT (object);
  HyScanGtkPlannerImportPrivate *priv = gtk_planner_import->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_import_get_property (GObject      *object,
                                        guint         prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerImport *gtk_planner_import = HYSCAN_GTK_PLANNER_IMPORT (object);
  HyScanGtkPlannerImportPrivate *priv = gtk_planner_import->priv;

  switch (prop_id)
    {
    case PROP_READY:
      g_value_set_boolean (value, priv->ready);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_import_object_constructed (GObject *object)
{
  HyScanGtkPlannerImport *import = HYSCAN_GTK_PLANNER_IMPORT (object);
  HyScanGtkPlannerImportPrivate *priv = import->priv;

  GtkBox *box = GTK_BOX (import);

  guint i;
  const gchar *filters[][2] = {
    { N_("XML Files"), "*.xml" },
    { N_("All Files"), "*" },
  };

  G_OBJECT_CLASS (hyscan_gtk_planner_import_parent_class)->constructed (object);

  priv->label = gtk_label_new (_("Select file to import"));
  priv->replace_chkbox = gtk_check_button_new_with_label (_("Delete current project plan"));
  priv->file_chooser = gtk_file_chooser_button_new (_("Track plan"), GTK_FILE_CHOOSER_ACTION_OPEN);
  for (i = 0; i < G_N_ELEMENTS (filters); i++)
    {
      GtkFileFilter *filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, _(filters[i][0]));
      gtk_file_filter_add_pattern (filter, filters[i][1]);
      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file_chooser), filter);
    }

  g_signal_connect_swapped (priv->file_chooser, "file-set", G_CALLBACK (hyscan_gtk_planner_import_file_set), import);

  gtk_box_set_spacing (box, 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
  gtk_box_pack_start (box, priv->label, FALSE, TRUE, 12);
  gtk_box_pack_start (box, priv->file_chooser, FALSE, TRUE, 0);
  gtk_box_pack_start (box, priv->replace_chkbox, FALSE, TRUE, 0);
}

static void
hyscan_gtk_planner_import_object_finalize (GObject *object)
{
  HyScanGtkPlannerImport *gtk_planner_import = HYSCAN_GTK_PLANNER_IMPORT (object);
  HyScanGtkPlannerImportPrivate *priv = gtk_planner_import->priv;

  g_clear_object (&priv->db);
  g_free (priv->project);

  G_OBJECT_CLASS (hyscan_gtk_planner_import_parent_class)->finalize (object);
}

static void
hyscan_gtk_planner_import_update_label (HyScanGtkPlannerImport *import)
{
  HyScanGtkPlannerImportPrivate *priv = import->priv;
  GHashTableIter iter;
  HyScanObject *object;
  gchar *text;
  gint count_zone = 0, count_tracks = 0, has_origin = FALSE;

  if (priv->objects == NULL)
    {
      gtk_label_set_text (GTK_LABEL (priv->label), _("Invalid file format, no objects has been loaded."));
      return;
    }

  g_hash_table_iter_init (&iter, priv->objects);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &object))
    {
      if (HYSCAN_IS_PLANNER_ZONE (object))
        count_zone++;
      else if (HYSCAN_IS_PLANNER_TRACK (object))
        count_tracks++;
      else if (HYSCAN_IS_PLANNER_ORIGIN (object))
        has_origin = TRUE;
    }

  text = g_strdup_printf (_("Found %d %s and %d %s. %s."),
                          count_zone,
                          g_dngettext (GETTEXT_PACKAGE, "zone", "zones", count_zone),
                          count_tracks,
                          g_dngettext (GETTEXT_PACKAGE, "track", "tracks", count_tracks),
                          has_origin ? _("Origin is set") : _("Origin is not set"));
  gtk_label_set_text (GTK_LABEL (priv->label), text);
  g_free (text);

}

static void
hyscan_gtk_planner_import_update_ready (HyScanGtkPlannerImport *import)
{
  HyScanGtkPlannerImportPrivate *priv = import->priv;
  gboolean ready;

  ready = priv->objects != NULL && !priv->running;

  if (ready == priv->ready)
    return;

  priv->ready = ready;
  g_object_notify_by_pspec (G_OBJECT (import), hyscan_gtk_planner_import_pspec_ready);
}

static void
hyscan_gtk_planner_import_file_set (HyScanGtkPlannerImport *import)
{
  HyScanGtkPlannerImportPrivate *priv = import->priv;
  gchar *filename;

  if (priv->running)
    return;

  g_clear_pointer (&priv->objects, g_hash_table_unref);

  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->file_chooser));
  
  if (filename != NULL)
    priv->objects = hyscan_planner_import_xml_from_file (filename);

  hyscan_gtk_planner_import_update_ready (import);
  hyscan_gtk_planner_import_update_label (import);

  g_free (filename);
}

/* Обработчик завершения задания по импорту. */
static void
hyscan_gtk_planner_import_ready (HyScanGtkPlannerImport *import,
                                 GTask                  *task,
                                 gpointer                user_data)
{
  HyScanGtkPlannerImportPrivate *priv = import->priv;
  gboolean result;

  /* Снимаем блокировку виджета. */
  priv->running = FALSE;
  g_clear_object (&priv->cancellable);
  hyscan_gtk_planner_import_update_ready (import);

  result = g_task_propagate_boolean (task, NULL);
  if (result)
    gtk_label_set_text (GTK_LABEL (priv->label), _("Import finished successfully"));
  else
    gtk_label_set_text (GTK_LABEL (priv->label), _("Failed to import"));
}

/* Функция выполняет импорт, выполняется в фоновом потоке. */
static void
hyscan_gtk_planner_import_run (GTask                  *task,
                               HyScanGtkPlannerImport *import,
                               GHashTable             *objects,
                               GCancellable           *cancellable)
{
  HyScanGtkPlannerImportPrivate *priv = import->priv;
  gboolean result;

  result = hyscan_planner_import_to_db (priv->db, priv->project, objects, priv->replace);
  g_task_return_boolean (task, result);
}

/**
 * hyscan_gtk_planner_import_new:
 * @db: указатель на базу данных #HyScanDB
 * @project: название проекта
 *
 * Функция создает виджет для импорта XML-файла планировщика в базу данных.
 *
 * Returns: виджет импорта данных планировщика
 */
GtkWidget *
hyscan_gtk_planner_import_new (HyScanDB    *db,
                               const gchar *project)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_IMPORT,
                       "db", db,
                       "project", project,
                       NULL);
}

/**
 * hyscan_gtk_planner_import_start:
 * @import: указатель на виджет #HyScanGtkPlannerImport
 *
 * Функция запускает задание импорта данных в БД.
 */
void
hyscan_gtk_planner_import_start (HyScanGtkPlannerImport *import)
{
  HyScanGtkPlannerImportPrivate *priv;
  GTask *task;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_IMPORT (import));
  priv = import->priv;

  if (priv->objects == NULL || priv->running)
    return;

  /* Блокирум виджет на время импорта. */
  priv->running = TRUE;
  priv->cancellable = g_cancellable_new ();

  priv->replace = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->replace_chkbox));
  task = g_task_new (import, priv->cancellable, (GAsyncReadyCallback) hyscan_gtk_planner_import_ready, NULL);
  g_task_set_task_data (task, g_hash_table_ref (priv->objects), (GDestroyNotify) g_hash_table_unref);
  g_task_run_in_thread (task, (GTaskThreadFunc) hyscan_gtk_planner_import_run);
  g_object_unref (task);
}

/**
 * hyscan_gtk_planner_import_stop:
 * @import: указатель на виджет #HyScanGtkPlannerImport
 *
 * Функция останавливает импорт данных в БД, если он выполняется.
 */
void
hyscan_gtk_planner_import_stop (HyScanGtkPlannerImport *import)
{
  HyScanGtkPlannerImportPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_IMPORT (import));
  priv = import->priv;

  if (priv->cancellable != NULL)
    g_cancellable_cancel (priv->cancellable);
}
