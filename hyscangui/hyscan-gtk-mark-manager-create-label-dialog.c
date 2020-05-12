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
 * @See_also: #HyScanGtkMarkManagerChangeLabelDialog, #HyScanGtkMarkManager, #HyScanGtkMarkManagerView, #HyScanModelManager
 *
 * Диалог #HyScanGtkMarkManagerCreateLabelDialog позволяет создать новую группу в Журнале Меток, проверяет введённые пользователем данные.
 *
 * - hyscan_gtk_mark_manager_create_label_dialog_new () - создание экземпляра диалога;
 *
 */

/* Для работы с GdkPixbuf в Prolect->Properties->C/C++ General->Path and Symbols
 * в разделе Includes добавлено "/usr/include/gdk-pixbuf-2.0".
 */
#include <hyscan-gtk-mark-manager-create-label-dialog.h>
#include <hyscan-object-data-label.h>
#include <glib/gi18n.h>

/* Проверка содержимого GtkEntry. */
#define IS_EMPTY(entry) (0 == g_strcmp0 (gtk_entry_get_text ( (entry) ), ""))
/* Размер иконок для отображения в GtkIconView. */
#define ICON_SIZE 16

enum
{
  PROP_MODEL = 1, /* Модель с данныим о группах. */
  N_PROPERTIES
};

/* Колонки для модели с иконками. */
enum
{
  COLUMN_ICON,  /* Колонка с иконкой. */
  COLUMN_NAME,  /* Колонка с названием иконки. */
  N_COLUMNS     /* Количество колонок. */
};

/* Поля для вода данных GtkEntry. */
typedef enum
{
  TITLE,       /* Название группы. */
  DESCRIPTION, /* Описание группы. */
  OPERATOR,    /* Оператор. */
  N_ENTRY      /* Количество полей. */
} EntryType;

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

  /* Модель с данныим о группах. */
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
    /* Модель с данныим о группах. */
    case PROP_MODEL:
      {
        priv->label_model = g_value_dup_object (value);
      }
      break;
    /* Что-то ещё... */
    default:
      {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
    }
}
/* Конструктор. */
static void
hyscan_gtk_mark_manager_create_label_dialog_constructed (GObject *object)
{
  HyScanGtkMarkManagerCreateLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv = self->priv;
  GtkIconTheme *icon_theme;                 /* Тема оформленя, откуда беуться иконки. */
  GtkDialog *dialog  = GTK_DIALOG (object); /* Диалог. */
  GtkWindow *window  = GTK_WINDOW (dialog); /* Тот же диалог, но GtkWindow. */
  GtkWidget *left    = gtk_box_new (GTK_ORIENTATION_VERTICAL,   0), /* Контейнер для меток. */
            *right   = gtk_box_new (GTK_ORIENTATION_VERTICAL,   0), /* Контейнер для полей ввода. */
            *box     = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0), /* Общий контейнер. */
            *content = gtk_dialog_get_content_area (dialog),
            *label[N_ENTRY];                /* Текстовые метки. */
  GList     *images;                        /* Список иконок из темы. */
  EntryType  type;
  /* Дефолтный конструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_gtk_mark_manager_create_label_dialog_parent_class)->constructed (object);
  /* Устанавливаем заголовок диаога. */
  gtk_window_set_title (window, _("New label"));
  /* Создаём немодальный диалог. */
  gtk_window_set_modal (window, FALSE);
  /* Закрывать вместе с родительским окном. */
  gtk_window_set_destroy_with_parent (window, TRUE);
  /* Размер диалога. */
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 600, 400);
  /* Добавляем кнопки "ОК" и "Cancel". */
  gtk_dialog_add_button (dialog, _("OK"),     GTK_RESPONSE_OK);
  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
  /* Создаём и размещаем текстовые метки и поля ввода. */
  for (type = TITLE; type < N_ENTRY; type++)
    {
      /* Текстовая метка. */
      label[type] = gtk_label_new (_(label_text[type]));
      /* Текстовую метку в контейнер слева. */
      gtk_widget_set_halign (label[type], GTK_ALIGN_START);
      /* Метки в контейнер.  */
      gtk_box_pack_start (GTK_BOX (left), label[type], TRUE, TRUE, 0);
      /* Поле ввода. */
      priv->entry[type] = gtk_entry_new ();
      /* Текст поля ввода по умолчанию. */
      gtk_entry_set_text (GTK_ENTRY (priv->entry[type]), _(entry_default_text[type]));
      /* Поле ввода в контейнер справа. */
      gtk_box_pack_start (GTK_BOX (right), priv->entry[type], TRUE, TRUE, 0);
      /* Подключаем сигнал для проверки заполнения поля ввода. */
      g_signal_connect (priv->entry[type],
                        "changed",
                        G_CALLBACK (hyscan_gtk_mark_manager_create_label_dialog_check_entry),
                        self);
    }
  /* Метки и поля ввода в контейнер. */
  gtk_box_pack_start (GTK_BOX (box), left,  FALSE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (box), right, TRUE,  TRUE, 0);
  /* Контейнер с метками и полями ввода в диалог. */
  gtk_box_pack_start (GTK_BOX (content), box, FALSE, TRUE, 0);
  /* Получаем тему. */
  icon_theme = gtk_icon_theme_get_default ();
  /* Получаем список иконок. */
  images = gtk_icon_theme_list_icons (icon_theme, NULL);
  /* Заполняем IconView иконками из темы. */
  if (images != NULL)
    {
      GtkTreeIter iter;
      /* Создаём модель с иконками. */
      priv->icon_model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING));
      GList *ptr = g_list_first (images);

      do
        {
          GError *error = NULL;
          const gchar *icon_name = (const gchar*)ptr->data;
          GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name, ICON_SIZE, 0, &error);

          if (pixbuf != NULL)
            {
              /* Добавляем иконку в IconView. */
              GtkListStore *store = GTK_LIST_STORE (priv->icon_model);
              /* Добавляем новую строку в модель */
              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter,
                                  COLUMN_ICON, pixbuf,
                                  COLUMN_NAME, icon_name,
                                  -1);
              /* Освобождаем pixbuf. */
              g_object_unref (pixbuf);
            }
          else
            {
              g_warning ("Couldn't load icon \"%s\".\n Error: %s.", icon_name, error->message);
              g_error_free (error);
            }
          ptr = g_list_next (ptr);
        }
      while (ptr != NULL);

      g_list_free (images);
        /* Создаём IconView с моделью заполненой данными. */
      priv->icon_view = gtk_icon_view_new_with_model (priv->icon_model);
    }

  if (priv->icon_view != NULL)
    {
      GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
      GtkIconView *view = GTK_ICON_VIEW (priv->icon_view);
      GtkTreePath *path = gtk_tree_path_new_first ();
      /* Связываем колонку с иконкой в модели с IconView. */
      gtk_icon_view_set_pixbuf_column (view, COLUMN_ICON);
      /* Связываем колонку с названием иконки в одели с IconView. */
      gtk_icon_view_set_text_column (view, -1);
      /*gtk_icon_view_set_columns (view, 0);*/
      /* Заполняем иконками всё пространство виджета автоматически. */
      gtk_icon_view_set_columns (view, -1);
      /* Устанавливаем ширину каждого элемента. */
      gtk_icon_view_set_item_width (view, ICON_SIZE);
      /* Хотя бы один элемент должен быть выбран.*/
      gtk_icon_view_set_selection_mode (view, GTK_SELECTION_BROWSE);
      /* Выделяем первую иконку. */
      gtk_icon_view_select_path (view, path);
      /* path - больше не нужен. */
      gtk_tree_path_free (path);
      /* Виджет для выбора иконки в GtkScrolledWindow.*/
      gtk_container_add (GTK_CONTAINER (scroll), priv->icon_view);
      /* Добавляем метку "Выбрать иконку". */
      gtk_box_pack_start (GTK_BOX (content), gtk_label_new (_("Choose icon")), FALSE, TRUE, 10);
      /* Помещаем GtkScrolledWindow в диалог. */
      gtk_box_pack_start (GTK_BOX (content), GTK_WIDGET (scroll), TRUE, TRUE, 0);
    }
  /* Подключаем сигнал для обработки результата диалога. */
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (hyscan_gtk_mark_manager_create_label_dialog_response),
                    self);
  /* Отображаем диалог. */
  gtk_widget_show_all (GTK_WIDGET (dialog));
}
/* Деструктор. */
static void
hyscan_gtk_mark_manager_create_label_dialog_finalize (GObject *object)
{
  HyScanGtkMarkManagerCreateLabelDialog *self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */
  g_object_unref (priv->label_model);
  g_object_unref (priv->icon_model);

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_create_label_dialog_parent_class)->finalize (object);
}

/* Функция-обработчик результата диалога создания новой группы. */
void
hyscan_gtk_mark_manager_create_label_dialog_response (GtkWidget *dialog,
                                                      gint       response,
                                                      gpointer   user_data)
{
  HyScanGtkMarkManagerCreateLabelDialog *self;
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;

  switch (response)
    {
    case GTK_RESPONSE_OK:
      {
        HyScanLabel *label = hyscan_label_new ();
        GHashTable  *table = hyscan_object_model_get (priv->label_model);
        GList       *images;
        gint64  time  = g_date_time_to_unix (g_date_time_new_now_local ());
        guint64 size  = hyscan_gtk_mark_manager_create_label_dialog_generate_label (table);
        g_print ("OK\n");
        g_hash_table_destroy (table);
        /* Заполняем данные о группе. */
        hyscan_label_set_text (label,
                               gtk_entry_get_text (GTK_ENTRY (priv->entry[TITLE])),
                               gtk_entry_get_text (GTK_ENTRY (priv->entry[DESCRIPTION])),
                               gtk_entry_get_text (GTK_ENTRY (priv->entry[OPERATOR])));
        images = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (priv->icon_view));
        if (g_list_length (images) == 0)
          {
            hyscan_label_set_icon_name (label, "emblem-default");
          }
        else
          {
            GList *ptr = g_list_first (images);
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter (priv->icon_model, &iter, (GtkTreePath*)ptr->data))
              {
                gchar *icon_name = NULL;
                gtk_tree_model_get (priv->icon_model, &iter, COLUMN_NAME, &icon_name, -1);
                hyscan_label_set_icon_name (label, icon_name);
                g_free (icon_name);
              }
            else
              {
                g_list_free (images);
                break;
              }
          }
        g_list_free (images);
        hyscan_label_set_label (label, size);
        hyscan_label_set_ctime (label, time);
        hyscan_label_set_mtime (label, time);
        /* Добавляем группу в базу данных. */
        hyscan_object_model_add_object (priv->label_model, (const HyScanObject*) label);
        /* Освобождаем label. */
        hyscan_label_free (label);
      }
      break;
    case GTK_RESPONSE_CANCEL:
      g_print ("CANCEL\n");
      break;
    case GTK_RESPONSE_DELETE_EVENT:
      g_print ("DELETE EVENT\n");
      break;
    default: break;
    }
  /* Удаляем диалог.*/
  gtk_widget_destroy (dialog);
}

/* Проверка ввода. */
void
hyscan_gtk_mark_manager_create_label_dialog_check_entry (GtkEntry  *entry,
                                                         gpointer   user_data)
{
  HyScanGtkMarkManagerCreateLabelDialog *self;
  HyScanGtkMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;
  /* Проверяем все ли поля имеют данные. */
  if (IS_EMPTY (GTK_ENTRY (priv->entry[TITLE]))       ||
      IS_EMPTY (GTK_ENTRY (priv->entry[DESCRIPTION])) ||
      IS_EMPTY (GTK_ENTRY (priv->entry[OPERATOR])))
    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);
  else
    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, TRUE);
}

/* Генерация идентификатора группы. */
guint64
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

/* Проверка идентификатора группы. */
gboolean
hyscan_gtk_mark_manager_create_label_dialog_validate_label (GHashTable *table,
                                                             guint64     label)
{
  GHashTableIter  table_iter;
  HyScanLabel    *object;
  gchar          *id;

  g_hash_table_iter_init (&table_iter, table);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      if (object != NULL)
        {
          if (object->label == label)
            return FALSE;
        }
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
