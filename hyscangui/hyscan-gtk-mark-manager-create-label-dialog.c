/*
 * hyscan-gtk-mark-manager-create-label-dialog.с
 *
 *  Created on: 10 апр. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include <hyscan-gtk-mark-manager-create-label-dialog.h>
#include <hyscan-object-data-label.h>
#define GETTEXT_PACKAGE "hyscanfnn-evoui"
#include <glib/gi18n-lib.h>

enum
{
  PROP_PARENT = 1,  /* Родительское окно. */
  PROP_MODEL,       /* Модель с данныим о группах. */
  N_PROPERTIES
};

struct _HyScanMarkManagerCreateLabelDialogPrivate
{
  GtkWindow         *parent;      /* Родительское окно. */
  GtkWidget         *content,     /* Контейнер для размещения виджетов диалога.  */
                    *title,       /* Название группы. */
                    *description, /* Описание группы. */
                    *operator;    /* Оператор. */
  HyScanObjectModel *model;       /* Модель с данныим о группах. */
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
        priv->model = g_value_dup_object (value);
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
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkWindow *window = GTK_WINDOW (dialog);
  /* Remove this call then class is derived from GObject.
     This call is strongly needed then class is derived from GtkWidget. */
  G_OBJECT_CLASS (hyscan_mark_manager_create_label_dialog_parent_class)->constructed (object);
  /* Устанавливаем заголовок диаога. */
  gtk_window_set_title (window, "New label");
  /* Устанавливаем родительское окно. */
  gtk_window_set_transient_for (window, priv->parent);
  /* Создаём немодальный диалог. */
  gtk_window_set_modal (window, FALSE);
  /* Закрывать вместе с родительским окном. */
  gtk_window_set_destroy_with_parent (window, TRUE);

  /* Получаем контейнер для размещения виджетов диалога. */
  priv->content = gtk_dialog_get_content_area (dialog);
  /* Название группы. */
  priv->title = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                  "Title",
                                                                  "New label");
  /* Описание группы. */
  priv->description = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                        "Description",
                                                                        "This is a new label");
  /* Оператор. */
  priv->operator = hyscan_mark_manager_create_label_dialog_add_item (GTK_BOX (priv->content),
                                                                     "Operator",
                                                                     "User");

  /* Добавляем кнопки "ОК" и "Cancel". */
  gtk_dialog_add_button (dialog, _("OK"),     GTK_RESPONSE_OK);
  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
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
  /*g_object_unref (priv->parent);*/
  g_object_unref (priv->model);

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
  gtk_box_pack_start (content, GTK_WIDGET (box), TRUE, TRUE, 0);
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
        gint64       time  = g_date_time_to_unix (g_date_time_new_now_local ());
        guint size = g_hash_table_size (hyscan_object_model_get (priv->model));
        g_print ("OK\n");
        /* Заполняем данные о группе. */
        hyscan_label_set_text (label,
                               gtk_entry_get_text (GTK_ENTRY (priv->title)),
                               gtk_entry_get_text (GTK_ENTRY (priv->description)),
                               gtk_entry_get_text (GTK_ENTRY (priv->operator)));
        hyscan_label_set_icon_name (label, "emblem-default");
        hyscan_label_set_label (label, size);
        hyscan_label_set_ctime (label, time);
        hyscan_label_set_mtime (label, time);
        /* Добавляем группу в базу данных. */
        hyscan_object_model_add_object (priv->model, (const HyScanObject*) label);
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
