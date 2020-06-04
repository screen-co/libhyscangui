/* hyscan-gtk-mark-manager-change-label-dialog.с
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

/**
 * SECTION: hyscan-gtk-mark-manager-change-label-dialog
 * @Short_description: Класс диалога для переноса объектов в выбранную группы.
 * @Title: HyScanGtkMarkManagerChangeLabelDialog
 * @See_also: #HyScanGtkMarkManagerCreateLabelDialog, #HyScanGtkMarkManager, #HyScanGtkMarkManagerView, #HyScanGtkModelManager
 *
 * Диалог #HyScanGtkMarkManagerChangeLabelDialog позволяет выбрать группу в Журнале Меток и перенести в неё выбранные объекты.
 *
 * - hyscan_gtk_mark_manager_change_label_dialog_new () - создание экземпляра диалога;
 *
 */
#include <hyscan-gtk-mark-manager-change-label-dialog.h>
#include <hyscan-object-data-label.h>
#include <glib/gi18n-lib.h>

enum
{
  PROP_MODEL = 1,      /* Модель с данныим о группах. */
  N_PROPERTIES
};

/* Поля для вода данных GtkEntry. */
enum
{
  TITLE,               /* Название группы. */
  DESCRIPTION,         /* Описание группы. */
  OPERATOR,            /* Оператор. */
  N_ENTRY              /* Количество полей. */
};

/* Колонки для модели с данными групп. */
enum
{
  COLUMN_ICON,         /* Колонка с иконкой. */
  COLUMN_NAME,         /* Колонка с названием группы. */
  COLUMN_ID,           /* Идентификатор в базе данных. */
  COLUMN_DESCRIPTION,  /* Описание группы. */
  COLUMN_OPERATOR,     /* Оператор. */
  COLUMN_CTIME,        /* Дата и время создания. */
  COLUMN_MTIME,        /* Дата и время изменения. */
  N_COLUMNS            /* Количество колонок. */
};

/* Сигнал. */
enum
{
  SIGNAL_CHANGE_LABEL,  /* Выбрана группа. */
  SIGNAL_LAST           /* Количество сигналов. */
};

struct _HyScanGtkMarkManagerChangeLabelDialogPrivate
{
  GtkWidget         *tree_view;      /* Виджет для отображения иконок. */
  GtkTreeModel      *model;          /* Модель с группами. */
  HyScanObjectModel *label_model;    /* Модель с данныим о группах. */
};

static void       hyscan_gtk_mark_manager_change_label_dialog_set_property   (GObject      *object,
                                                                              guint         prop_id,
                                                                              const GValue *value,
                                                                              GParamSpec   *pspec);

static void       hyscan_gtk_mark_manager_change_label_dialog_constructed    (GObject      *object);

static void       hyscan_gtk_mark_manager_change_label_dialog_finalize       (GObject      *object);

static void       hyscan_gtk_mark_manager_change_label_dialog_response       (GtkWidget   *dialog,
                                                                              gint         response,
                                                                              gpointer     user_data);

static guint      hyscan_gtk_mark_manager_change_label_dialog_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMarkManagerChangeLabelDialog, hyscan_gtk_mark_manager_change_label_dialog, GTK_TYPE_DIALOG)

static void
hyscan_gtk_mark_manager_change_label_dialog_class_init (HyScanGtkMarkManagerChangeLabelDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_mark_manager_change_label_dialog_set_property;
  object_class->constructed  = hyscan_gtk_mark_manager_change_label_dialog_constructed;
  object_class->finalize     = hyscan_gtk_mark_manager_change_label_dialog_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Model with data about labels",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /**
   * HyScanGtkMarkManagerChangeLabelDialog::change-label:
   * @self: указатель на #HyScanMarkManager
   * @id: идентификатор группы в базе данных
   *
   * Сигнал посылается при выборе группы для переноса объектов Журнала Меток.
   */
  hyscan_gtk_mark_manager_change_label_dialog_signals[SIGNAL_CHANGE_LABEL] =
             g_signal_new ("change-label",
                           HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG,
                           G_SIGNAL_RUN_LAST,
                           0, NULL, NULL,
                           g_cclosure_marshal_VOID__STRING,
                           G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
hyscan_gtk_mark_manager_change_label_dialog_init (HyScanGtkMarkManagerChangeLabelDialog *self)
{
  self->priv = hyscan_gtk_mark_manager_change_label_dialog_get_instance_private (self);
}

static void
hyscan_gtk_mark_manager_change_label_dialog_set_property (GObject      *object,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec)
{
  HyScanGtkMarkManagerChangeLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerChangeLabelDialogPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->label_model = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_mark_manager_change_label_dialog_constructed (GObject *object)
{
  HyScanGtkMarkManagerChangeLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerChangeLabelDialogPrivate *priv = self->priv;
  GtkDialog  *dialog  = GTK_DIALOG (object);
  GtkWindow  *window  = GTK_WINDOW (dialog);
  GtkWidget  *content = gtk_dialog_get_content_area (dialog);
  GHashTable *table   = hyscan_object_model_get (priv->label_model);

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_change_label_dialog_parent_class)->constructed (object);

  gtk_window_set_title (window, _("Choose label"));
  gtk_window_set_modal (window, FALSE);
  gtk_window_set_destroy_with_parent (window, TRUE);

  gtk_widget_set_size_request (GTK_WIDGET (dialog), 600, 400);

  gtk_dialog_add_button (dialog, _("OK"),     GTK_RESPONSE_OK);
  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);

  /* Заполняем модель данными. */
  if (table != NULL)
    {
      GHashTableIter  table_iter;
      GtkTreeIter     store_iter;
      GtkListStore   *store = gtk_list_store_new (N_COLUMNS,
                                                  GDK_TYPE_PIXBUF, /* Иконки. */
                                                  G_TYPE_STRING,   /* Название группы. */
                                                  G_TYPE_STRING,   /* Идентификатор в базе данных. */
                                                  G_TYPE_STRING,   /* Описание группы. */
                                                  G_TYPE_STRING,   /* Оператор. */
                                                  G_TYPE_STRING,   /* Дата и время создания. */
                                                  G_TYPE_STRING);  /* Дата и время изменения. */
      GtkIconTheme   *icon_theme = gtk_icon_theme_get_default ();
      HyScanLabel    *object;
      gchar          *id;

      g_hash_table_iter_init (&table_iter, table);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL)
            {
              GdkPixbuf *icon = NULL;
              if (gtk_icon_theme_has_icon (icon_theme, object->icon_data))
                {
                  icon = gtk_icon_theme_load_icon (icon_theme, object->icon_data, 16, 0, NULL);
                }
              else
                {
                  GInputStream *stream = g_memory_input_stream_new ();
                  GError *error = NULL;
                  guchar *buf;
                  gsize length;

                  buf = g_base64_decode ((const gchar*)object->icon_data, &length);
                  stream = g_memory_input_stream_new_from_data ((const void*)buf, (gssize)length, g_free);
                  icon = gdk_pixbuf_new_from_stream (stream, NULL, &error);

                  g_object_unref (stream);
                }

              if (icon != NULL)
                {
                  gtk_list_store_append (store,           &store_iter);
                  gtk_list_store_set (store,              &store_iter,
                                      COLUMN_ICON,         icon,                  /* Иконки. */
                                      COLUMN_NAME,         object->name,          /* Название группы. */
                                      COLUMN_ID,           id,                    /* Идентификатор в базе данных. */
                                      COLUMN_DESCRIPTION,  object->description,   /* Описание группы. */
                                      COLUMN_OPERATOR,     object->operator_name, /* Оператор. */
                                      /* Дата и время создания. */
                                      COLUMN_CTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (object->ctime),
                                                             "%d.%m.%Y %H:%M:%S"),
                                      /* Дата и время изменения. */
                                      COLUMN_MTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (object->mtime),
                                                             "%d.%m.%Y %H:%M:%S"),
                                      -1);
                  g_object_unref (icon);
                }
            }
        }

      priv->model = GTK_TREE_MODEL (store);
      g_hash_table_destroy (table);

      priv->tree_view = gtk_tree_view_new_with_model (priv->model);

      if (priv->tree_view != NULL)
        {
          GtkWidget         *scroll        = gtk_scrolled_window_new (NULL, NULL);
          GtkCellRenderer   *renderer      = gtk_cell_renderer_text_new (),
                            *icon_renderer = gtk_cell_renderer_pixbuf_new ();
          GtkTreeView       *tree_view     = GTK_TREE_VIEW (priv->tree_view);

          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_ICON,
                                                       _("Icon"), icon_renderer,
                                                       "pixbuf", COLUMN_ICON,
                                                       NULL);
          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_NAME,
                                                       _("Title"), renderer,
                                                       "text", COLUMN_NAME,
                                                       NULL);
          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_DESCRIPTION,
                                                       _("Description"), renderer,
                                                       "text", COLUMN_DESCRIPTION,
                                                       NULL);
          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_OPERATOR,
                                                       _("Operator"), renderer,
                                                       "text", COLUMN_OPERATOR,
                                                       NULL);
          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_CTIME,
                                                       _("Created"), renderer,
                                                       "text", COLUMN_CTIME,
                                                       NULL);
          gtk_tree_view_insert_column_with_attributes (tree_view,
                                                       COLUMN_MTIME,
                                                       _("Modified"), renderer,
                                                       "text", COLUMN_MTIME,
                                                       NULL);
          gtk_tree_view_set_headers_visible (tree_view, TRUE);
          gtk_tree_view_set_enable_tree_lines (tree_view, FALSE);
          gtk_tree_selection_set_mode (gtk_tree_view_get_selection (tree_view), GTK_SELECTION_BROWSE);

          gtk_container_add (GTK_CONTAINER (scroll), priv->tree_view);
          gtk_box_pack_start (GTK_BOX (content), scroll, TRUE, TRUE, 0);
        }
      else
        {
          gtk_container_add (GTK_CONTAINER (content),
                             gtk_label_new (_("There is no GtkTreeView\n"
                                              "to show list of labels.")));
          gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
        }
    }

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (hyscan_gtk_mark_manager_change_label_dialog_response),
                    self);

  gtk_widget_show_all (GTK_WIDGET (dialog));
}

static void
hyscan_gtk_mark_manager_change_label_dialog_finalize (GObject *object)
{
  HyScanGtkMarkManagerChangeLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerChangeLabelDialogPrivate *priv = self->priv;

  g_object_unref (priv->label_model);
  g_object_unref (priv->model);

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_change_label_dialog_parent_class)->finalize (object);
}

/* Функция-обработчик результата диалога создания новой группы. */
static void
hyscan_gtk_mark_manager_change_label_dialog_response (GtkWidget *dialog,
                                                      gint       response,
                                                      gpointer   user_data)
{
  HyScanGtkMarkManagerChangeLabelDialog *self;
  HyScanGtkMarkManagerChangeLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG (user_data);
  priv = self->priv;

  if (response == GTK_RESPONSE_OK)
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
      GtkTreeIter iter;

      if (gtk_tree_selection_get_selected (selection, &priv->model, &iter))
        {
          gchar *id = NULL;
          gtk_tree_model_get (priv->model, &iter, COLUMN_ID, &id, -1);
          g_signal_emit (self, hyscan_gtk_mark_manager_change_label_dialog_signals[SIGNAL_CHANGE_LABEL], 0, id);
          g_free (id);
        }
    }

  gtk_widget_destroy (dialog);
}

/**
 * hyscan_gtk_mark_manager_change_label_dialog_new:
 * @parent: родительское окно
 * @model: модель с данныим о группах
 *
 * Returns: cоздаёт новый объект #HyScanGtkMarkManagerChangeLabelDialog
 */
GtkWidget*
hyscan_gtk_mark_manager_change_label_dialog_new (GtkWindow         *parent,
                                                 HyScanObjectModel *model)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG,
                                   "transient-for", parent,
                                   "model",         model,
                                   NULL));
}
