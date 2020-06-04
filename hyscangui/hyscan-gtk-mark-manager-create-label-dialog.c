/* hyscan-gtk-mark-manager-create-label-dialog.с
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
 * SECTION: hyscan-gtk-mark-manager-create-label-dialog
 * @Short_description: Класс диалога для создания новой группы в Журнале Меток.
 * @Title: HyScanGtkMarkManagerCreateLabelDialog
 * @See_also: #HyScanGtkMarkManagerChangeLabelDialog, #HyScanGtkMarkManager, #HyScanGtkMarkManagerView, #HyScanGtkModelManager
 *
 * Диалог #HyScanGtkMarkManagerCreateLabelDialog позволяет создать новую группу в Журнале Меток, проверяет введённые пользователем данные.
 *
 * - hyscan_gtk_mark_manager_create_label_dialog_new () - создание экземпляра диалога;
 *
 */

#include <hyscan-gtk-mark-manager-create-label-dialog.h>
#include <hyscan-object-data-label.h>
#include <glib/gi18n-lib.h>

#define IS_EMPTY(entry) (0 == g_strcmp0 (gtk_entry_get_text ( (entry) ), ""))
#define ICON_SIZE 16

enum
{
  PROP_MODEL = 1, /* Модель с данныим о группах. */
  N_PROPERTIES
};

/* Колонки для модели с иконками. */
enum
{
  COLUMN_ICON,    /* Колонка с иконкой. */
  N_COLUMNS       /* Количество колонок. */
};

/* Поля для вода данных GtkEntry. */
typedef enum
{
  TITLE,          /* Название группы. */
  DESCRIPTION,    /* Описание группы. */
  OPERATOR,       /* Оператор. */
  N_ENTRY         /* Количество полей. */
}EntryType;

struct _HyScanGtkMarkManagerCreateLabelDialogPrivate
{
  GtkWidget         *entry[N_ENTRY], /* Поля для ввода данных. */
                    *icon_view;      /* Виджет для отображения иконок. */
  GtkTreeModel      *icon_model;     /* Модель с иконками. */
  HyScanObjectModel *label_model;    /* Модель с данныим о группах. */
};

/* Текст для полей ввода по умолчанию. */
static gchar *entry_default_text[N_ENTRY] = {N_("New label"),
                                             N_("This is a new label"),
                                             N_("User")};
/* Текстовые метки по умолчанию.*/
static gchar *label_text[N_ENTRY] = {N_("Title"),
                                     N_("Description"),
                                     N_("Operator")};

static void       hyscan_gtk_mark_manager_create_label_dialog_set_property     (GObject      *object,
                                                                                guint         prop_id,
                                                                                const GValue *value,
                                                                                GParamSpec   *pspec);

static void       hyscan_gtk_mark_manager_create_label_dialog_constructed      (GObject      *object);

static void       hyscan_gtk_mark_manager_create_label_dialog_finalize         (GObject      *object);

static void       hyscan_gtk_mark_manager_create_label_dialog_response         (GtkWidget    *dialog,
                                                                                gint          response,
                                                                                gpointer      user_data);

static void       hyscan_gtk_mark_manager_create_label_dialog_check_entry      (GtkEntry     *entry,
                                                                                gpointer      user_data);

static guint64    hyscan_gtk_mark_manager_create_label_dialog_generate_label   (GHashTable   *table);

static gboolean   hyscan_gtk_mark_manager_create_label_dialog_validate_label   (GHashTable   *table,
                                                                                guint64       label);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMarkManagerCreateLabelDialog, hyscan_gtk_mark_manager_create_label_dialog, GTK_TYPE_DIALOG)

static void
hyscan_gtk_mark_manager_create_label_dialog_class_init (HyScanGtkMarkManagerCreateLabelDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_mark_manager_create_label_dialog_set_property;
  object_class->constructed  = hyscan_gtk_mark_manager_create_label_dialog_constructed;
  object_class->finalize     = hyscan_gtk_mark_manager_create_label_dialog_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Model with data about labels",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_mark_manager_create_label_dialog_init (HyScanGtkMarkManagerCreateLabelDialog *self)
{
  self->priv = hyscan_gtk_mark_manager_create_label_dialog_get_instance_private (self);
}

static void
hyscan_gtk_mark_manager_create_label_dialog_set_property (GObject      *object,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec)
{
  HyScanGtkMarkManagerCreateLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv = self->priv;

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
hyscan_gtk_mark_manager_create_label_dialog_constructed (GObject *object)
{
  HyScanGtkMarkManagerCreateLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv = self->priv;
  GtkIconTheme *icon_theme;
  GtkDialog    *dialog  = GTK_DIALOG (object);
  GtkWindow    *window  = GTK_WINDOW (dialog);
  GtkWidget    *left    = gtk_box_new (GTK_ORIENTATION_VERTICAL,   0),
               *right   = gtk_box_new (GTK_ORIENTATION_VERTICAL,   0),
               *box     = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0),
               *content = gtk_dialog_get_content_area (dialog),
               *scroll  = gtk_scrolled_window_new (NULL, NULL),
               *label[N_ENTRY];
  GtkIconView  *view    = GTK_ICON_VIEW (priv->icon_view);
  GtkTreePath  *path    = gtk_tree_path_new_first ();
  GtkTreeIter   iter;
  GList        *images  = NULL,
               *ptr     = NULL;
  EntryType     type;

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_create_label_dialog_parent_class)->constructed (object);

  gtk_window_set_title (window, _("New label"));
  gtk_window_set_modal (window, FALSE);
  gtk_window_set_destroy_with_parent (window, TRUE);

  gtk_widget_set_size_request (GTK_WIDGET (dialog), 600, 400);
  
  gtk_dialog_add_button (dialog, _("OK"),     GTK_RESPONSE_OK);
  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);

  /* Создаём и размещаем текстовые метки и поля ввода. */
  for (type = TITLE; type < N_ENTRY; type++)
    {
      label[type] = gtk_label_new (_(label_text[type]));
      gtk_widget_set_halign (label[type], GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (left), label[type], TRUE, TRUE, 0);

      priv->entry[type] = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (priv->entry[type]), _(entry_default_text[type]));
      gtk_box_pack_start (GTK_BOX (right), priv->entry[type], TRUE, TRUE, 0);

      g_signal_connect (priv->entry[type],
                        "changed",
                        G_CALLBACK (hyscan_gtk_mark_manager_create_label_dialog_check_entry),
                        self);
    }

  gtk_box_pack_start (GTK_BOX (box), left,  FALSE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (box), right, TRUE,  TRUE, 0);

  gtk_box_pack_start (GTK_BOX (content), box, FALSE, TRUE, 0);

  icon_theme = gtk_icon_theme_get_default ();
  images = gtk_icon_theme_list_icons (icon_theme, NULL);

  if (images == NULL)
    return;

  ptr = g_list_first (images);

  priv->icon_model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, GDK_TYPE_PIXBUF));

  do
    {
      GError      *error     = NULL;
      const gchar *icon_name = (const gchar*)ptr->data;
      GdkPixbuf   *pixbuf    = gtk_icon_theme_load_icon (icon_theme, icon_name, ICON_SIZE, 0, &error);

      if (pixbuf != NULL)
        {
          GtkListStore *store = GTK_LIST_STORE (priv->icon_model);

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter, COLUMN_ICON, pixbuf, -1);
          g_object_unref (pixbuf);
        }
      else
        {
          g_warning (_("Couldn't load icon \"%s\".\n Error: %s."), icon_name, error->message);
          g_error_free (error);
        }
      ptr = g_list_next (ptr);
    }
  while (ptr != NULL);

  g_list_free (images);

  priv->icon_view = gtk_icon_view_new_with_model (priv->icon_model);
  view = GTK_ICON_VIEW (priv->icon_view);

  gtk_icon_view_set_pixbuf_column (view, COLUMN_ICON);
  gtk_icon_view_set_text_column (view, -1);
  gtk_icon_view_set_columns (view, -1);
  gtk_icon_view_set_item_width (view, ICON_SIZE);
  gtk_icon_view_set_selection_mode (view, GTK_SELECTION_BROWSE);
  gtk_icon_view_select_path (view, path);
  gtk_tree_path_free (path);
  gtk_container_add (GTK_CONTAINER (scroll), priv->icon_view);
  gtk_box_pack_start (GTK_BOX (content), gtk_label_new (_("Choose icon")), FALSE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (content), GTK_WIDGET (scroll), TRUE, TRUE, 0);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (hyscan_gtk_mark_manager_create_label_dialog_response),
                    self);

  gtk_widget_show_all (GTK_WIDGET (dialog));
}

static void
hyscan_gtk_mark_manager_create_label_dialog_finalize (GObject *object)
{
  HyScanGtkMarkManagerCreateLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv = self->priv;

  g_object_unref (priv->label_model);
  g_object_unref (priv->icon_model);

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_create_label_dialog_parent_class)->finalize (object);
}

/* Функция-обработчик результата диалога создания новой группы. */
static void
hyscan_gtk_mark_manager_create_label_dialog_response (GtkWidget *dialog,
                                                      gint       response,
                                                      gpointer   user_data)
{
  HyScanGtkMarkManagerCreateLabelDialog *self;
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;

  if (response == GTK_RESPONSE_OK)
    {
      HyScanLabel *label  = hyscan_label_new ();
      GdkPixbuf   *pixbuf = NULL;
      GHashTable  *table  = hyscan_object_model_get (priv->label_model);
      GList       *images;
      GDateTime   *dt     = g_date_time_new_now_local ();
      gint64       time   = g_date_time_to_unix (dt);
      guint64      id     = hyscan_gtk_mark_manager_create_label_dialog_generate_label (table);

      g_date_time_unref (dt);
      g_hash_table_destroy (table);

      hyscan_label_set_text (label,
                             gtk_entry_get_text (GTK_ENTRY (priv->entry[TITLE])),
                             gtk_entry_get_text (GTK_ENTRY (priv->entry[DESCRIPTION])),
                             gtk_entry_get_text (GTK_ENTRY (priv->entry[OPERATOR])));
      images = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (priv->icon_view));

      if (g_list_length (images) == 0)
        {
          GtkWidget *image = gtk_image_new_from_resource ("/org/hyscan/icons/emblem-default.png");
          pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (image));
          gtk_widget_destroy (image);
        }
      else
        {
          GList *ptr = g_list_first (images);
          GtkTreeIter iter;
          if (gtk_tree_model_get_iter (priv->icon_model, &iter, (GtkTreePath*)ptr->data))
            {
              gtk_tree_model_get (priv->icon_model, &iter, COLUMN_ICON, &pixbuf, -1);
            }
        }

      g_list_free (images);

      if (pixbuf != NULL)
        {
          GOutputStream *stream = g_memory_output_stream_new_resizable ();

          if (gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, NULL, NULL))
            {
              gpointer data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));

              gsize length = g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (stream));
              gchar *str = g_base64_encode ((const guchar*)data, length);

              hyscan_label_set_icon_data (label, (const gchar*)str);

              g_object_unref (pixbuf);
              g_free (str);
            }

          g_object_unref (stream);

          hyscan_label_set_label (label, id);
          hyscan_label_set_ctime (label, time);
          hyscan_label_set_mtime (label, time);
          /* Добавляем группу в базу данных. */
          hyscan_object_model_add_object (priv->label_model, (const HyScanObject*) label);
        }

      hyscan_label_free (label);
    }

  gtk_widget_destroy (dialog);
}

/* Проверка ввода. */
static void
hyscan_gtk_mark_manager_create_label_dialog_check_entry (GtkEntry  *entry,
                                                         gpointer   user_data)
{
  HyScanGtkMarkManagerCreateLabelDialog *self;
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;

  if (IS_EMPTY (GTK_ENTRY (priv->entry[TITLE]))       ||
      IS_EMPTY (GTK_ENTRY (priv->entry[DESCRIPTION])) ||
      IS_EMPTY (GTK_ENTRY (priv->entry[OPERATOR])))
    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);
  else
    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, TRUE);
}

/* Генерация идентификатора группы. */
static guint64
hyscan_gtk_mark_manager_create_label_dialog_generate_label (GHashTable *table)
{
  guint64 label = g_random_int ();
  label = label << 32 | g_random_int ();
  while (!hyscan_gtk_mark_manager_create_label_dialog_validate_label (table, label))
    {
      label = g_random_int ();
      label = label << 32 | g_random_int ();
    }
  return label;
}

/* Проверка идентификатора группы на валидность. */
static gboolean
hyscan_gtk_mark_manager_create_label_dialog_validate_label (GHashTable *table,
                                                            guint64     label)
{
  GHashTableIter  table_iter;
  HyScanLabel    *object;
  gchar          *id;

  g_hash_table_iter_init (&table_iter, table);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      if (object == NULL)
        continue;
      if (object->label == label)
        return FALSE;
    }
  return TRUE;
}

/**
 * hyscan_gtk_mark_manager_create_label_dialog_new:
 * @parent: родительское окно
 * @model: модель с данныим о группах
 *
 * Returns: cоздаёт новый объект #HyScanGtkMarkManagerCreateLabelDialog
 */
GtkWidget*
hyscan_gtk_mark_manager_create_label_dialog_new (GtkWindow         *parent,
                                                 HyScanObjectModel *model)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG,
                                   "transient-for", parent,
                                   "model",         model,
                                   NULL));
}
