/*
 * hyscan-gtk-mark-manager-create-label-dialog.с
 *
 *  Created on: 10 апр. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 *
 * Для работы с GdkPixbuf в Prolect->Properties->C/C++ General->Path and Symbols
 * в разделе Includes добавлено "/usr/include/gdk-pixbuf-2.0".
 */
#include <hyscan-gtk-mark-manager-create-label-dialog.h>
#include <hyscan-object-data-label.h>
/* Проверка содержимого GtkEntry. */
#define IS_EMPTY(entry) (0 == g_strcmp0 (gtk_entry_get_text ( (entry) ), ""))
/* Размер иконок для отображения в GtkIconView. */
#define ICON_SIZE 16
#define GETTEXT_PACKAGE "hyscanfnn-evoui"
#include <glib/gi18n-lib.h>

enum
{
  PROP_PARENT = 1,  /* Родительское окно. */
  PROP_MODEL,       /* Модель с данныим о группах. */
  N_PROPERTIES
};

/* Поля для вода данных GtkEntry. */
enum
{
  TITLE,       /* Название группы. */
  DESCRIPTION, /* Описание группы. */
  OPERATOR,    /* Оператор. */
  N_ENTRY      /* Количество полей. */
};

/* Колонки для модели с иконками. */
enum
{
  COLUMN_ICON,  /* Колонка с иконкой. */
  COLUMN_NAME,  /* Колонка с названием иконки. */
  N_COLUMNS     /* Количество колонок. */
};

struct _HyScanMarkManagerCreateLabelDialogPrivate
{
  GtkWindow         *parent;         /* Родительское окно. */
  GtkWidget         *content,        /* Контейнер для размещения виджетов диалога.  */
                    *entry[N_ENTRY], /* Поля для ввода данных. */
                    *icon_view,      /* Виджет для отображения иконок. */
                    *ok_button;      /* Кнопка "Ok". */
  GtkTreeModel      *icon_model;     /* Модель с иконками. */
  HyScanObjectModel *label_model;          /* Модель с данныим о группах. */
};

static void       hyscan_mark_manager_create_label_dialog_set_property   (GObject      *object,
                                                                          guint         prop_id,
                                                                          const GValue *value,
                                                                          GParamSpec   *pspec);

static void       hyscan_mark_manager_create_label_dialog_constructed    (GObject      *object);

static void       hyscan_mark_manager_create_label_dialog_finalize       (GObject      *object);

static GtkWidget* hyscan_mark_manager_create_label_dialog_add_item       (GtkBox      *content,
                                                                          const gchar *label_text,
                                                                          const gchar *entry_text);

static void       hyscan_mark_manager_create_label_dialog_response       (GtkWidget   *dialog,
                                                                          gint         response,
                                                                          gpointer     user_data);

static void       hyscan_mark_manager_create_label_dialog_check_entry    (GtkEntry    *entry,
                                                                          gpointer     user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManagerCreateLabelDialog, hyscan_mark_manager_create_label_dialog, GTK_TYPE_DIALOG)

static void
hyscan_mark_manager_create_label_dialog_class_init (HyScanMarkManagerCreateLabelDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_create_label_dialog_set_property;
  object_class->constructed  = hyscan_mark_manager_create_label_dialog_constructed;
  object_class->finalize     = hyscan_mark_manager_create_label_dialog_finalize;

  /* Родительское окно. */
  g_object_class_install_property (object_class, PROP_PARENT,
    g_param_spec_object ("parent", "Parent", "Parent window",
                         GTK_TYPE_WINDOW,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Модель с данныим о группах. */
  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Model with data about labels",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_mark_manager_create_label_dialog_init (HyScanMarkManagerCreateLabelDialog *self)
{
  self->priv = hyscan_mark_manager_create_label_dialog_get_instance_private (self);
}
static void
hyscan_mark_manager_create_label_dialog_set_property (GObject      *object,
                                                      guint         prop_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec)
{
  HyScanMarkManagerCreateLabelDialog *self = HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanMarkManagerCreateLabelDialogPrivate *priv = self->priv;

  switch (prop_id)
    {
    /* Родительское окно. */
    case PROP_PARENT:
      {
        priv->parent = g_value_dup_object (value);
      }
      break;
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
hyscan_mark_manager_create_label_dialog_constructed (GObject *object)
{
  HyScanMarkManagerCreateLabelDialog *self = HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanMarkManagerCreateLabelDialogPrivate *priv = self->priv;
  GtkIconTheme *icon_theme;                /* Тема оформленя, откуда беуться иконки. */
  GtkDialog *dialog = GTK_DIALOG (object); /* Диалог. */
  GtkWindow *window = GTK_WINDOW (dialog); /* Тот же диалог, но GtkWindow. */
  GList     *images;                       /* Список иконок из темы. */
  /* Дефолтный конструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_mark_manager_create_label_dialog_parent_class)->constructed (object);
  /* Устанавливаем заголовок диаога. */
  gtk_window_set_title (window, "New label");
  /* Устанавливаем родительское окно. */
  gtk_window_set_transient_for (window, priv->parent);
  /* Создаём немодальный диалог. */
  gtk_window_set_modal (window, FALSE);
  /* Закрывать вместе с родительским окном. */
  gtk_window_set_destroy_with_parent (window, TRUE);
  /* Добавляем кнопки "ОК" и "Cancel". */
  priv->ok_button = gtk_dialog_add_button (dialog, _("OK"), GTK_RESPONSE_OK);
  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
  /* Получаем контейнер для размещения виджетов диалога. */
  priv->content = gtk_dialog_get_content_area (dialog);
  /* Название группы. */
  priv->entry[TITLE] = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                         "Title",
                                                                         "New label");
  g_signal_connect (priv->entry[TITLE],
                    "changed",
                    G_CALLBACK (hyscan_mark_manager_create_label_dialog_check_entry),
                    self);
  /* Описание группы. */
  priv->entry[DESCRIPTION] = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                               "Description",
                                                                               "This is a new label");
  g_signal_connect (priv->entry[DESCRIPTION],
                    "changed",
                    G_CALLBACK (hyscan_mark_manager_create_label_dialog_check_entry),
                    self);
  /* Оператор. */
  priv->entry[OPERATOR] = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                            "Operator",
                                                                            "User");
  g_signal_connect (priv->entry[OPERATOR],
                    "changed",
                    G_CALLBACK (hyscan_mark_manager_create_label_dialog_check_entry),
                    self);
  /* Получаем тему. */
  icon_theme = gtk_icon_theme_get_default();
  /* Получаем список иконок. */
  images = gtk_icon_theme_list_icons(icon_theme, NULL);
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

        /* Создаём IconView с моделью заполненой данными. */
      priv->icon_view = gtk_icon_view_new_with_model(priv->icon_model);
    }

  if (priv->icon_view != NULL)
    {
      GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
      GtkIconView *view = GTK_ICON_VIEW (priv->icon_view);
      /* Связываем колонку с иконкой в модели с IconView. */
      gtk_icon_view_set_pixbuf_column (view, COLUMN_ICON);
      /* Связываем колонку с названием иконки в одели с IconView. */
      gtk_icon_view_set_text_column (view, -1);
      gtk_icon_view_set_columns (view, 0);
      /* Заполняем иконками всё пространство виджета автоматически. */
      gtk_icon_view_set_columns (view, -1);
      /* Устанавливаем ширину каждого элемента. */
      gtk_icon_view_set_item_width (view, ICON_SIZE);
      /* Хотя бы один элемент должен быть выбран.*/
      gtk_icon_view_set_selection_mode (view, GTK_SELECTION_BROWSE);
      /* Выделяем первую иконку. */
      gtk_icon_view_select_path (view, gtk_tree_path_new_first ());
      /* Виджет для выбора иконки в GtkScrolledWindow.*/
      gtk_container_add(GTK_CONTAINER(scroll), priv->icon_view);
      /* Добавляем метку "Выбрать иконку". */
      gtk_box_pack_start (GTK_BOX (priv->content), gtk_label_new ("Choose icon"), FALSE, TRUE, 10);
      /* Помещаем GtkScrolledWindow в диалог. */
      gtk_box_pack_start (GTK_BOX (priv->content), GTK_WIDGET (scroll), TRUE, TRUE, 0);
    }
  /* Подключаем сигнал для обработки результата диалога. */
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (hyscan_mark_manager_create_label_dialog_response),
                    self);
  /* Отображаем диалог. */
  gtk_widget_show_all (GTK_WIDGET (dialog));
}
/* Деструктор. */
static void
hyscan_mark_manager_create_label_dialog_finalize (GObject *object)
{
  HyScanMarkManagerCreateLabelDialog *self = HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG (object);
  HyScanMarkManagerCreateLabelDialogPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */
  g_object_unref (priv->parent);
  g_object_unref (priv->label_model);
  g_object_unref (priv->icon_model);

  G_OBJECT_CLASS (hyscan_mark_manager_create_label_dialog_parent_class)->finalize (object);
}

/* Создаёт метку и поле ввода для одного элемента диалога создания новой группы. */
GtkWidget*
hyscan_mark_manager_create_label_dialog_add_item (GtkBox      *content,
                                                  const gchar *label_text,
                                                  const gchar *entry_text)
{
  GtkBox    *box   = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10));
  GtkWidget *entry = gtk_entry_new ();
  /* Название группы. */
  gtk_box_pack_start (box, gtk_label_new (label_text), FALSE, TRUE, 0);
  /* Поле для ввода названия группы. */
  gtk_entry_set_text (GTK_ENTRY (entry), entry_text);
  gtk_box_pack_start (box, entry, TRUE, TRUE, 0);
  /* Помещаем метку и поле ввода в контейнер. */
  gtk_box_pack_start (content, GTK_WIDGET (box), FALSE, TRUE, 0);
  return entry;
}

/* Функция-обработчик результата диалога создания новой группы. */
void
hyscan_mark_manager_create_label_dialog_response (GtkWidget *dialog,
                                                  gint       response,
                                                  gpointer   user_data)
{
  HyScanMarkManagerCreateLabelDialog *self;
  HyScanMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;

  switch (response)
    {
    case GTK_RESPONSE_OK:
      {
        HyScanLabel *label = hyscan_label_new ();
        GList       *images;
        gint64       time  = g_date_time_to_unix (g_date_time_new_now_local ());
        guint size = g_hash_table_size (hyscan_object_model_get (priv->label_model));
        g_print ("OK\n");
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
              }
            else
              {
                break;
              }
          }
        hyscan_label_set_label (label, size);
        hyscan_label_set_ctime (label, time);
        hyscan_label_set_mtime (label, time);
        /* Добавляем группу в базу данных. */
        hyscan_object_model_add_object (priv->label_model, (const HyScanObject*) label);
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
  /* Удаляем диалог и обнуляем указатель на диалог.*/
  gtk_widget_destroy (dialog);
}

/* Проверка ввода. */
void
hyscan_mark_manager_create_label_dialog_check_entry (GtkEntry  *entry,
                                                     gpointer   user_data)
{
  HyScanMarkManagerCreateLabelDialog *self;
  HyScanMarkManagerCreateLabelDialogPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data));

  self = HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG (user_data);
  priv = self->priv;
  /* Проверяем все ли поля имеют данные. */
  if (IS_EMPTY (GTK_ENTRY (priv->entry[TITLE]))       ||
      IS_EMPTY (GTK_ENTRY (priv->entry[DESCRIPTION])) ||
      IS_EMPTY (GTK_ENTRY (priv->entry[OPERATOR])))
    gtk_widget_set_sensitive (priv->ok_button, FALSE);
  else
    gtk_widget_set_sensitive (priv->ok_button, TRUE);
}

/**
 * hyscan_mark_manager_create_label_dialog_new:
 * @title: название
 * @active: состояние чек-бокса
 *
 * Returns: cоздаёт новый объект #HyScanMarkManagerCreateLabelDialog
 */
GtkWidget*
hyscan_mark_manager_create_label_dialog_new (GtkWindow         *parent,
                                             HyScanObjectModel *model)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG,
                                   "parent", parent,
                                   "model", model,
                                   NULL));
}
