/*
 * hyscan-gtk-mark-manager.c
 *
 *  Created on: 14 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 *
 * В hyscancore/hyscan-core-schemas.h добавлено:
 * #define LABEL_SCHEMA_ID                        5468681196977785233
 * #define LABEL_SCHEMA_VERSION                   20200113
 * и
 * #define LABEL_SCHEMA                           "label"
 *
 * В базе данных (в файле project.prm/project.sch) добавлена схема label-ов :
 * <schema id="label">
 *  <node id="schema">
 *    <key id="id" name="Label schema id" type="integer" access="readonly">
 *      <default>5468681196977785233</default>
 *    </key>
 *    <key id="version" name="Label schema version" type="integer" access="readonly">
 *      <default>20200113</default>
 *    </key>
 *  </node>
 *  <key id="name" name="Group short name" type="string"/>
 *  <key id="description" name="Detailed description" type="string"/>
 *  <key id="operator" name="Operator name" type="string"/>
 *  <key id="label" name="Applied labels" type="integer"/>
 *  <key id="ctime" name="Creation date and time" type="integer"/>
 *  <key id="mtime" name="Modification date and time" type="integer"/>
 * </schema>
 *
 * Эту схему нужно добавить в hyscancore/project-schema.xml
 */
#include "hyscan-gtk-mark-manager.h"
#include "hyscan-gtk-mark-manager-view.h"

#define GETTEXT_PACKAGE "hyscanfnn-evoui"
#include <glib/gi18n-lib.h>

enum
{
  PROP_GEO_MARK_MODEL = 1,  /* Модель данных гео-меток. */
  PROP_WF_MARK_MODEL,       /* Модель данных водопадных меток. */
  PROP_TRACK_MODEL,         /* Модель данных галсов. */
  PROP_PROJECT_NAME,        /* Название проекта. */
  PROP_CACHE,               /* Кэш.*/
  PROP_DB,                  /* База данных. */
  N_PROPERTIES
};

/* Идентификаторы текста меток и подсказок для элементов панели инструментов. */
enum
{
  CREATE_NEW_GROUP, /* Метка и подсказка для кноки "Создать новую группу". */
  DELETE_SELECTED,  /* Метка и подсказка для кноки "Удалить выделенное". */
  SHOW_OR_HIDE,     /* Метка для меню управления видимостью . */
  GROUPING,         /* Подсказка для выпадающего списка выбора типа представления . */
  EXPAND_NODES,     /* Подсказка для кноки "Развернуть все узлы". */
  COLLAPSE_NODES    /* Подсказка для кноки "Свернуть все узлы". */
};

/* Положение панели инструментов относительно виджета представления. */
typedef enum
{
  TOOLBAR_LEFT,   /* Слева. */
  TOOLBAR_RIGHT,  /* Справа. */
  TOOLBAR_BOTTOM, /* Снизу.*/
  TOOLBAR_TOP     /* Сверху. */
}HyScanMarkManagerToolbarPosition;

/* Флаги управления видимостью. */
typedef enum
{
  SHOW_ALL,      /* Показать все. */
  HIDE_ALL,      /* Скрыть все. */
  SHOW_SELECTED, /* Показать выделенные. */
  HIDE_SELECTED  /* Скрыть выделенные. */
}HyScanMarkManagerViewVisibility;

struct _HyScanMarkManagerPrivate
{
  HyScanObjectModel  *geo_mark_model,  /* Модель данных гео-меток. */
                     *label_model;     /* Модель данных групп. */
  HyScanMarkLocModel *wf_mark_model;   /* Модель данных водопадных меток. */
  HyScanDBInfo       *track_model;     /* Модель данных галсов. */

  GtkWidget          *view,            /* Виджет представления. */
                     *delete_icon;     /* Виджет для иконки для кнопки "Удалить выделенное". */
  GtkToolItem        *nodes_item;      /* Развернуть/свернуть все узлы. */
  GtkTreeSelection   *selection;       /* Выбранные строки. */
  gchar              *project_name;    /* Название проекта. */
  HyScanCache        *cache;           /* Кэш.*/
  HyScanDB           *db;              /* База данных. */
  GHashTable         *labels,          /* Группы. */
                     *wf_marks,        /* "Водопадные" метки. */
                     *geo_marks,       /* Гео-метки. */
                     *tracks;          /* Галсы. */

  GtkIconSize         icon_size;       /* Размер иконок. */
};
/* Текст пунктов выпадающего списка для выбора типа представления. */
static gchar *view_type_text[]  = {N_("Ungrouped"),
                                   N_("By labels"),
                                   N_("By types")};
/* Текст пунктов меню управления видимостью. */
static gchar *visibility_text[] = {N_("Show all"),
                                   N_("Hide all"),
                                   N_("Show selected"),
                                   N_("Hide selected")};
/* Текст меток и подсказок для элементов панели инструментов. */
static gchar *tooltips_text[]   = {N_("Create new group"),
                                   N_("Delete selected"),
                                   N_("Show/hide"),
                                   N_("Grouping"),
                                   N_("Expand"),
                                   N_("Collapse")};

static void       hyscan_mark_manager_set_property         (GObject               *object,
                                                            guint                  prop_id,
                                                            const GValue          *value,
                                                            GParamSpec            *pspec);

static void       hyscan_mark_manager_constructed          (GObject               *object);

static void       hyscan_mark_manager_finalize             (GObject               *object);

static void       hyscan_mark_manager_create_new_label     (GtkToolItem           *item,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_set_view_type        (GtkComboBox           *combo,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_nodes           (HyScanMarkManager     *self,
                                                            GtkToolItem           *item);

static void       hyscan_mark_manager_delete_selected      (GtkToolButton         *button,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_all             (GtkMenuItem           *item,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_hide_all             (GtkMenuItem           *item,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_selected        (GtkMenuItem           *item,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_hide_selected        (GtkMenuItem           *item,
                                                            HyScanMarkManager     *self);

static void       hyscan_mark_manager_labels_changed       (HyScanMarkManager     *self);

static void       hyscan_mark_manager_geo_marks_changed    (HyScanMarkManager     *self);

static void       hyscan_mark_manager_wf_marks_changed     (HyScanMarkManager     *self);

static void       hyscan_mark_manager_tracks_changed       (HyScanMarkManager     *self);

static void       hyscan_mark_manager_item_selected        (HyScanMarkManager     *self,
                                                            GtkTreeSelection      *selection);

static void       hyscan_mark_manager_delete_label         (GtkTreeModel          *model,
                                                            GtkTreePath           *path,
                                                            GtkTreeIter           *iter,
                                                            gpointer               data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManager, hyscan_mark_manager, GTK_TYPE_BOX)

static void
hyscan_mark_manager_class_init (HyScanMarkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_set_property;
  object_class->constructed  = hyscan_mark_manager_constructed;
  object_class->finalize     = hyscan_mark_manager_finalize;

  /* Модель данных гео-меток. */
  g_object_class_install_property (object_class, PROP_GEO_MARK_MODEL,
    g_param_spec_object ("geo_mark_model", "Geo", "Geo mark model",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Модель данных водопадных меток. */
  g_object_class_install_property (object_class, PROP_WF_MARK_MODEL,
    g_param_spec_object ("wf_mark_model", "Wf", "Waterfall mark model",
                         HYSCAN_TYPE_MARK_LOC_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Модель данных галсов. */
  g_object_class_install_property (object_class, PROP_TRACK_MODEL,
    g_param_spec_object ("track_model", "Db_info", "Data base information",
                       HYSCAN_TYPE_DB_INFO,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Название проекта. */
  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project_name", "Project_name", "Project name",
                         "",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Кэш.*/
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The link to main cache with frequency used stafs",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* База данных. */
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Data base",
                         "The link to data base",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_mark_manager_init (HyScanMarkManager *self)
{
  self->priv = hyscan_mark_manager_get_instance_private (self);
  /* Размеры иконок.
  GTK_ICON_SIZE_INVALID            Invalid size.
  GTK_ICON_SIZE_MENU               Size appropriate for menus (16px).
  GTK_ICON_SIZE_SMALL_TOOLBAR      Size appropriate for small toolbars (16px).
  GTK_ICON_SIZE_LARGE_TOOLBAR      Size appropriate for large toolbars (24px)
  GTK_ICON_SIZE_BUTTON             Size appropriate for buttons (16px)
  GTK_ICON_SIZE_DND                Size appropriate for drag and drop (32px)
  GTK_ICON_SIZE_DIALOG             Size appropriate for dialogs (48px)
*/
  self->priv->icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
}
static void
hyscan_mark_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanMarkManager *self        = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
      /* Модель данных гео-меток. */
      case PROP_GEO_MARK_MODEL:
        {
          priv->geo_mark_model = g_value_get_object (value);
          /* Увеличиваем счётчик ссылок на модель данных. */
          g_object_ref (priv->geo_mark_model);
        }
      break;
      /* Модель данных водопадных меток. */
      case PROP_WF_MARK_MODEL:
        {
          priv->wf_mark_model = g_value_get_object (value);
          /* Увеличиваем счётчик ссылок на модель данных. */
          g_object_ref (priv->wf_mark_model);
        }
      break;
      /* Модель данных галсов. */
      case PROP_TRACK_MODEL:
        {
          priv->track_model = g_value_get_object (value);
          /* Увеличиваем счётчик ссылок на модель данных. */
          g_object_ref (priv->track_model);
        }
      break;
      /* Название проекта */
      case PROP_PROJECT_NAME:
        {
          priv->project_name = g_value_dup_string (value);
        }
      break;
      /* Кэш.*/
      case PROP_CACHE:
        {
          priv->cache  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на кэш. */
          g_object_ref (priv->cache);
        }
      break;
      /* База данных. */
      case PROP_DB:
        {
          priv->db  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на базу данных. */
          g_object_ref (priv->db);
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
hyscan_mark_manager_constructed (GObject *object)
{
  HyScanMarkManager        *self    = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv    = self->priv;
  GtkBox      *box                  = GTK_BOX (object); /* Контейнер для панели инструментов и представления.*/
  GtkWidget   *toolbar              = gtk_toolbar_new (); /* Панель инструментов. */
  GtkWidget   *visibility_menu      = gtk_menu_new ();  /* Меню управления видимостью. */
  /* Пункты меню управления видимостью. */
  GtkWidget   *show_all_item        = gtk_menu_item_new_with_label (_(visibility_text[SHOW_ALL]));
  GtkWidget   *hide_all_item        = gtk_menu_item_new_with_label (_(visibility_text[HIDE_ALL]));
  GtkWidget   *show_selected_item   = gtk_menu_item_new_with_label (_(visibility_text[SHOW_SELECTED]));
  GtkWidget   *hide_selected_item   = gtk_menu_item_new_with_label (_(visibility_text[HIDE_SELECTED]));
  GtkWidget   *combo                = gtk_combo_box_text_new ();
  GtkToolItem *new_group_item       = NULL;
  /* Кнопка для меню управления видимостью. */
  GtkToolItem *visibility_item      = NULL;
  GtkToolItem *separator            = gtk_separator_tool_item_new (); /* Разделитель. */
  GtkToolItem *container            = gtk_tool_item_new (); /* Контейнер для выпадающего списка. */
  /* Размещаем панель инструментов сверху. */
  HyScanMarkManagerToolbarPosition toolbar_position = TOOLBAR_TOP;
  HyScanMarkManagerGrouping index,    /* Для обхода массива с пунктами меню выбора типа представления.*/
                            grouping; /* Для хранения текущего значения. */
  /* Создаём модель с данными о группах. */
  priv->label_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_LABEL);
  hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
  /* Подключаем сигнал об изменении данных о группах в моделе. */
  g_signal_connect_swapped (G_OBJECT (priv->label_model),    "changed",
                            G_CALLBACK (hyscan_mark_manager_labels_changed),    self);
  /* Подключаем сигнал об изменении данных о гео-метках в моделе. */
  g_signal_connect_swapped (G_OBJECT (priv->geo_mark_model), "changed",
                            G_CALLBACK (hyscan_mark_manager_geo_marks_changed), self);
  /* Подключаем сигнал об изменении данных о "водопадных" метках в моделе. */
  g_signal_connect_swapped (G_OBJECT (priv->wf_mark_model),  "changed",
                            G_CALLBACK (hyscan_mark_manager_wf_marks_changed),  self);
  /* Подключаем сигнал об изменении данных о галсах в моделе. */
  g_signal_connect_swapped (G_OBJECT (priv->track_model),    "tracks-changed",
                            G_CALLBACK (hyscan_mark_manager_tracks_changed),    self);
  /* Виджет представления. */
  priv->view = hyscan_mark_manager_view_new ();
  /* Соединяем сигнал изменения выбранных элементов представления с функцией-обработчиком. */
  g_signal_connect_swapped (G_OBJECT (priv->view), "selected",
                            G_CALLBACK (hyscan_mark_manager_item_selected), self);
  /* Кнопка "Создать новую группу". */
  new_group_item  = gtk_tool_button_new (NULL, _(tooltips_text[CREATE_NEW_GROUP]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (new_group_item), "insert-object");
  /* Кнопка для меню управления видимостью. */
  visibility_item = gtk_menu_tool_button_new (NULL, _(tooltips_text[DELETE_SELECTED]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (visibility_item), "edit-delete");
  grouping = hyscan_mark_manager_view_get_grouping (HYSCAN_MARK_MANAGER_VIEW (priv->view));
  gtk_widget_set_tooltip_text (GTK_WIDGET (new_group_item), _(tooltips_text[CREATE_NEW_GROUP]));
  /* Обработчик нажатия кнопки "Новая группа". */
  g_signal_connect (G_OBJECT (new_group_item), "clicked",
                    G_CALLBACK (hyscan_mark_manager_create_new_label), self);
  /* Иконка для кнопки "Удалить выделенное". */
  priv->delete_icon = gtk_image_new_from_icon_name ("edit-delete", priv->icon_size);
  /* Делаем неактивной кнопку "Удалить выделенное". */
  gtk_widget_set_sensitive (priv->delete_icon, FALSE);
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (visibility_item), priv->delete_icon);
  gtk_widget_set_tooltip_text (priv->delete_icon, _(tooltips_text[DELETE_SELECTED]));
  /* Меню управления видимостью. */
  gtk_menu_shell_append (GTK_MENU_SHELL (visibility_menu), show_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (visibility_menu), hide_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (visibility_menu), show_selected_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (visibility_menu), hide_selected_item);
  /* Отображаем пункты меню управления видимостью. */
  gtk_widget_show (show_all_item);
  gtk_widget_show (hide_all_item);
  gtk_widget_show (show_selected_item);
  gtk_widget_show (hide_selected_item);
  /* Устанавливаем меню на кнопку в панели инструментов. */
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (visibility_item), visibility_menu);
  gtk_menu_tool_button_set_arrow_tooltip_text (GTK_MENU_TOOL_BUTTON (visibility_item),
                                              _(tooltips_text[SHOW_OR_HIDE]));
  /* Обработчик нажатия кнопки "Удалить выделенное" */
  g_signal_connect (G_OBJECT (visibility_item),    "clicked",
                    G_CALLBACK (hyscan_mark_manager_delete_selected), self);
  /* Обработчик выбора пункта меню "Показать всё". */
  g_signal_connect (G_OBJECT (show_all_item),      "activate",
                    G_CALLBACK (hyscan_mark_manager_show_all),        self);
  /* Обработчик выбора пункта меню "Скрыть всё". */
  g_signal_connect (G_OBJECT (hide_all_item),      "activate",
                    G_CALLBACK (hyscan_mark_manager_hide_all),        self);
  /* Обработчик выбора пункта меню "Показать выделенное". */
  g_signal_connect (G_OBJECT (show_selected_item), "activate",
                    G_CALLBACK (hyscan_mark_manager_show_selected),   self);
  /* Обработчик выбора пункта меню "Скрыть выделенное". */
  g_signal_connect (G_OBJECT (hide_selected_item), "activate",
                    G_CALLBACK (hyscan_mark_manager_hide_selected),   self);
  /* Отступ. */
  /*gtk_container_set_border_width (GTK_CONTAINER (toolbar), 20);*/
  /*gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);*/
  /* Наполняем выпадающий список. */
  for (index = UNGROUPED; index < N_VIEW_TYPES; index++)
    {
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), NULL, _(view_type_text[index]));
    }
  /* Активируем текущий тип. */
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), grouping);
  gtk_widget_set_tooltip_text (combo, _(tooltips_text[GROUPING]));
  /* Обработчик выбора типа представления. */
  g_signal_connect (G_OBJECT (combo), "changed",
                    G_CALLBACK (hyscan_mark_manager_set_view_type), self);
  /* Выпадающий список в контейнер. */
  gtk_container_add (GTK_CONTAINER (container), combo);
  /* Кнопка "Открыть / закрыть все узлы". */
  priv->nodes_item = gtk_toggle_tool_button_new ();
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (priv->nodes_item), "view-fullscreen");
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->nodes_item), _(tooltips_text[EXPAND_NODES]));
  g_signal_connect_swapped (G_OBJECT (priv->nodes_item), "clicked",
                            G_CALLBACK (hyscan_mark_manager_show_nodes), self);
  /* Открыть / закрыть все узлы активно только для древовидного отображения. */
  if (grouping == UNGROUPED)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (priv->nodes_item), FALSE);
    }
  /* Заполняем панель инструментов. */
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), new_group_item,   -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), visibility_item,  -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), container,        -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), separator,        -1);
  /* Добавляем кнопку "Развернуть/свернуть все узлы.". */
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->nodes_item, -1);
  /* Добавляем панель инструментов и представление с учётом взаимного расположения. */
  switch (toolbar_position)
  {
    case TOOLBAR_LEFT:
      {
        /* Панель инструментов слева. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (box, toolbar, TRUE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
    case TOOLBAR_RIGHT:
      {
        /* Панель инструментов справа. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, TRUE, TRUE, 0);
      }
      break;
    case TOOLBAR_BOTTOM:
      {
        /* Панель инструментов снизу. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, TRUE, TRUE, 0);
      }
      break;
    case TOOLBAR_TOP:
    default:
      {
        /* Панель инструментов сверху. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, toolbar, TRUE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
  }
}
/* Деструктор. */
static void
hyscan_mark_manager_finalize (GObject *object)
{
  HyScanMarkManager *self = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */
  /*  Группы. */
  if (priv->labels != NULL)
    {
      g_hash_table_remove_all (priv->labels);
      g_hash_table_unref (priv->labels);
      priv->labels = NULL;
    }
  /* Модель групп. */
  if (priv->label_model != NULL)
    {
      g_object_unref (priv->label_model);
      priv->label_model = NULL;
    }
  g_object_unref (priv->geo_mark_model);
  priv->geo_mark_model = NULL;
  g_object_unref (priv->wf_mark_model);
  priv->wf_mark_model = NULL;
  g_object_unref (priv->track_model);
  priv->track_model = NULL;
  g_free (priv->project_name);
  priv->project_name = NULL;
  g_object_unref (priv->cache);
  priv->cache = NULL;
  g_object_unref (priv->db);
  priv->db = NULL;

  G_OBJECT_CLASS (hyscan_mark_manager_parent_class)->finalize (object);
}
/* Обработчик нажатия кнопки "Новая группа". */
void
hyscan_mark_manager_create_new_label (GtkToolItem       *item,
                                      HyScanMarkManager *self)
{
  /* Создаём объект в базе данных. */
  HyScanMarkManagerPrivate *priv  = self->priv;
  HyScanLabel              *label = hyscan_label_new ();
  gint64 time = g_date_time_to_unix (g_date_time_new_now_local ());
  /* gint64 time = g_get_real_time (); */
  g_print ("New label\n");
  hyscan_label_set_text (label, "New label", "This is a new label", "Adm");
  hyscan_label_set_label (label, 1);
  hyscan_label_set_ctime (label, time);
  hyscan_label_set_mtime (label, time);
  hyscan_object_model_add_object (priv->label_model, (const HyScanObject*) label);

}
/* Обработчик выбора тип представления. */
void
hyscan_mark_manager_set_view_type (GtkComboBox       *combo,
                                   HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate  *priv     = self->priv;
  HyScanMarkManagerGrouping  grouping = gtk_combo_box_get_active (combo);

  g_print ("Set view type\n");
  gtk_widget_set_sensitive (priv->delete_icon, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET(priv->nodes_item), grouping);
  hyscan_mark_manager_view_set_grouping (HYSCAN_MARK_MANAGER_VIEW (priv->view), grouping);
  /* Проверяем нужно ли развернуть узлы. */
  if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (priv->nodes_item)))
    hyscan_mark_manager_view_expand_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
}

/* Обработчик изменения состояния кнопки "Развернуть/свернуть все узлы." */
void
hyscan_mark_manager_show_nodes (HyScanMarkManager *self,
                                GtkToolItem       *item)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Show/hide all nodes\n");
  if (gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (item)))
    {
      g_print ("Toggle button is active\n");
      gtk_tool_button_set_icon_name   (GTK_TOOL_BUTTON (item), "view-restore");
      gtk_widget_set_tooltip_text (GTK_WIDGET (item), _(tooltips_text[COLLAPSE_NODES]));
      hyscan_mark_manager_view_expand_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
    }
  else
    {
      g_print ("Toggle button is not active\n");
      gtk_tool_button_set_icon_name   (GTK_TOOL_BUTTON (item), "view-fullscreen");
      gtk_widget_set_tooltip_text (GTK_WIDGET (item), _(tooltips_text[EXPAND_NODES]));
      hyscan_mark_manager_view_collapse_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
    }
}

/* Обработчик нажатия кнопки "Удалить выделенное". */
void
hyscan_mark_manager_delete_selected (GtkToolButton     *button,
                                     HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  gboolean sensivity = gtk_widget_get_sensitive (
                           GTK_WIDGET ( gtk_tool_button_get_icon_widget (button)));
  HyScanMarkManagerView *view = HYSCAN_MARK_MANAGER_VIEW (priv->view);
  if (sensivity && hyscan_mark_manager_view_has_selected (view))
    {
       GtkTreeSelection *selection = hyscan_mark_manager_view_get_selection (view);
       g_print ("Delete selected\n");
       /* Удаляем объект из базы данных. */
       gtk_tree_selection_selected_foreach(selection, hyscan_mark_manager_delete_label, self);
       gtk_widget_set_sensitive(priv->delete_icon, FALSE);
    }
}

/* Обработчик выбора пункта меню "Показать всё". */
void
hyscan_mark_manager_show_all (GtkMenuItem       *item,
                              HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Show all\n");
  hyscan_mark_manager_view_select_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
  gtk_widget_set_sensitive(priv->delete_icon, TRUE);
}

/* Обработчик выбора пункта меню "Скрыть всё". */
void
hyscan_mark_manager_hide_all (GtkMenuItem       *item,
                              HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Hide all\n");
  hyscan_mark_manager_view_unselect_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
  gtk_widget_set_sensitive(priv->delete_icon, FALSE);
}

/* Обработчик выбора пункта меню "Показать выделенное". */
void
hyscan_mark_manager_show_selected (GtkMenuItem       *item,
                                   HyScanMarkManager *self)
{
  g_print ("Show selected\n");
}

/* Обработчик выбора пункта меню "Скрыть выделенное". */
void
hyscan_mark_manager_hide_selected (GtkMenuItem       *item,
                                   HyScanMarkManager *self)
{
  g_print ("Hide selected\n");
}

/* Обработчик сигнала об изменении данных о группах в моделе. */
void
hyscan_mark_manager_labels_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Label model changed\n");
  if (priv->labels != NULL)
    g_hash_table_remove_all (priv->labels);
  priv->labels = hyscan_object_model_get (priv->label_model);
  if (priv->labels != NULL)
    {
      HyScanLabel *object;
      GHashTableIter table_iter;
      gchar *id;
      g_print ("labels %p\n", priv->labels);
      g_hash_table_iter_init (&table_iter, priv->labels);
      while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
        {
          g_print ("%s\n", object->name);
        }
      hyscan_mark_manager_view_update_labels (HYSCAN_MARK_MANAGER_VIEW (priv->view), priv->labels);
    }
}

/* Обработчик сигнала об изменении данных о гео-метках в моделе. */
void
hyscan_mark_manager_geo_marks_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Geo mark model changed\n");
  if (priv->geo_marks != NULL)
    g_hash_table_remove_all (priv->geo_marks);
  priv->geo_marks = hyscan_object_model_get (priv->geo_mark_model);
  if (priv->geo_marks != NULL)
    {
      hyscan_mark_manager_view_update_geo_marks (HYSCAN_MARK_MANAGER_VIEW (priv->view), priv->geo_marks);
    }
}

/* Обработчик сигнала об изменении данных о "водопадных" метках в моделе. */
void
hyscan_mark_manager_wf_marks_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Waterfall mark model changed\n");
  if (priv->wf_marks != NULL)
    g_hash_table_remove_all (priv->wf_marks);
  priv->wf_marks = hyscan_mark_loc_model_get (priv->wf_mark_model);
  if (priv->wf_marks != NULL)
    {
      hyscan_mark_manager_view_update_wf_marks (HYSCAN_MARK_MANAGER_VIEW (priv->view), priv->wf_marks);
    }
}

/* Обработчик сигнала об изменении данных о галсах в моделе. */
void
hyscan_mark_manager_tracks_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Track model changed\n");
  if (priv->tracks != NULL)
    g_hash_table_remove_all (priv->tracks);
  priv->tracks = hyscan_db_info_get_tracks (priv->track_model);
  if (priv->tracks != NULL)
    {
      hyscan_mark_manager_view_update_tracks (HYSCAN_MARK_MANAGER_VIEW (priv->view), priv->tracks);
    }
}

/* Функция-обработчик выделения объектов MarkManagerView. */
void
hyscan_mark_manager_item_selected (HyScanMarkManager *self,
                                   GtkTreeSelection  *selection)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print (">>> SIGNAL CAUGHT\n");
  if (gtk_widget_get_sensitive (priv->delete_icon) == FALSE)
    {
      gtk_widget_set_sensitive(priv->delete_icon, TRUE);
    }
}
/* Функция удаляет объект*/
void hyscan_mark_manager_delete_label (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  HyScanMarkManagerPrivate *priv;
  gchar *id;
  GtkTreePath *tmp;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (data));

  priv = HYSCAN_MARK_MANAGER (data)->priv;
  gtk_tree_model_get (model, iter, 0/*COLUMN_ID*/, &id, -1);
  g_print ("*** id: %s\n", id);
  hyscan_object_model_remove_object (priv->label_model, id);
  /*tmp = gtk_tree_path_copy (path);
  if (gtk_tree_path_up (tmp))
    {
      hyscan_mark_manager_view_expand_to_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), tmp);
    }*/
  hyscan_mark_manager_view_expand_to_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), path);
}

/**
 * hyscan_mark_manager_new:
 *
 * Returns: cоздаёт новый объект #HyScanMarkManager
 */
GtkWidget*
hyscan_mark_manager_new (HyScanObjectModel  *geo_mark_model,
                         HyScanMarkLocModel *wf_mark_model,
                         HyScanDBInfo       *track_model,
                         gchar              *project_name,
                         HyScanCache        *cache,
                         HyScanDB           *db)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_MARK_MANAGER,
                                   "geo_mark_model", geo_mark_model,
                                   "wf_mark_model",  wf_mark_model,
                                   "track_model",    track_model,
                                   "project_name",   project_name,
                                   "cache",          cache,
                                   "db",             db,
                                   NULL));
}

