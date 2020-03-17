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

#include <hyscan-gtk-mark-manager.h>
#define GETTEXT_PACKAGE "hyscanfnn-evoui"
#include <glib/gi18n-lib.h>

enum
{
  PROP_MODEL_MANAGER = 1,   /* Менеджер Моделей. */
  N_PROPERTIES
};

/* Сигналы. */
enum
{
  SIGNAL_SCROLLED_HORIZONTAL, /* Прокрутка представления по горизонтали. */
  SIGNAL_SCROLLED_VERTICAL, /* Прокрутка представления по вертикали. */
  SIGNAL_LAST
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
  HyScanModelManager *model_manager;     /* Менеджер Моделей. */

  GtkWidget          *view,              /* Виджет представления. */
                     *delete_icon,       /* Виджет для иконки для кнопки "Удалить выделенное". */
                     *combo;             /* Выпадающий список для выбора типа группировки. */
  GtkToolItem        *nodes_item;        /* Развернуть/свернуть все узлы. */

  GtkIconSize         icon_size;         /* Размер иконок. */
  gulong              signal;            /* Идентификатор сигнала об изменении режима отображения всех узлов.*/
};
/* Текст пунктов выпадающего списка для выбора типа представления. */
static gchar *view_type_text[]  = {N_("Ungrouped"),
                                   N_("By types"),
                                   N_("By labels")};
/* Текст пунктов меню управления видимостью. */
static gchar *visibility_text[] = {N_(/*"Show all"*/"Select all"),
                                   N_(/*"Hide all"*/"Unselect all"),
                                   N_(/*"Show selected"*/"Toggle all"),
                                   N_(/*"Hide selected"*/"Untoggle all")};
/* Текст меток и подсказок для элементов панели инструментов. */
static gchar *tooltips_text[]   = {N_("Create new group"),
                                   N_("Delete selected"),
                                   N_("Show/hide"),
                                   N_("Grouping"),
                                   N_("Expand"),
                                   N_("Collapse")};

static void       hyscan_mark_manager_set_property              (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);

static void       hyscan_mark_manager_constructed               (GObject               *object);

static void       hyscan_mark_manager_finalize                  (GObject               *object);

static void       hyscan_mark_manager_create_new_label          (GtkToolItem           *item,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_set_grouping              (GtkComboBox           *combo,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_nodes                (HyScanMarkManager     *self,
                                                                 GtkToolItem           *item);

static void       hyscan_mark_manager_delete_selected           (GtkToolButton         *button,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_all                  (GtkMenuItem           *item,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_hide_all                  (GtkMenuItem           *item,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_show_selected             (GtkMenuItem           *item,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_hide_selected             (GtkMenuItem           *item,
                                                                 HyScanMarkManager     *self);

static void       hyscan_mark_manager_item_selected             (HyScanMarkManager     *self,
                                                                 GtkTreeSelection      *selection);

static void       hyscan_mark_manager_select_item               (HyScanMarkManager     *self);

static void       hyscan_mark_manager_item_toggled              (HyScanMarkManager     *self,
                                                                 gchar                 *id,
                                                                 gboolean               active);

static void       hyscan_mark_manager_toggle_item               (HyScanMarkManager     *self);

static void       hyscan_mark_manager_view_scrolled_horizontal  (HyScanMarkManager     *self);

static void       hyscan_mark_manager_view_scrolled_vertical    (HyScanMarkManager     *self);

static void       hyscan_mark_manager_delete_label              (GtkTreeModel          *model,
                                                                 GtkTreePath           *path,
                                                                 GtkTreeIter           *iter,
                                                                 gpointer               data);

static void       hyscan_mark_manager_grouping_changed          (HyScanMarkManager     *self);

static void       hyscan_mark_manager_expand_nodes_mode_changed (HyScanMarkManager     *self);

static void       hyscan_mark_manager_view_model_updated        (HyScanMarkManager     *self);

static void       hyscan_mark_manager_scrolled_horizontal       (GtkAdjustment         *adjustment,
                                                                 gpointer               user_data);

static void       hyscan_mark_manager_scrolled_vertical         (GtkAdjustment         *adjustment,
                                                                 gpointer               user_data);

static guint      hyscan_mark_manager_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManager, hyscan_mark_manager, GTK_TYPE_BOX)

static void
hyscan_mark_manager_class_init (HyScanMarkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_set_property;
  object_class->constructed  = hyscan_mark_manager_constructed;
  object_class->finalize     = hyscan_mark_manager_finalize;

  /* Модель данных гео-меток. */
  g_object_class_install_property (object_class, PROP_MODEL_MANAGER,
    g_param_spec_object ("model_manager", "ModelManager", "Model Manager",
                         HYSCAN_TYPE_MODEL_MANAGER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanMarkManager::h-scrolled:
   * @self: указатель на #HyScanMarkManager
   *
   * Сигнал посылается при горизонтальной прокрутке виджета.
   */
  hyscan_mark_manager_signals[SIGNAL_SCROLLED_HORIZONTAL] =
    g_signal_new ("h-scrolled", HYSCAN_TYPE_MARK_MANAGER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_ADJUSTMENT);

  /**
   * HyScanMarkManager::v-scrolled:
   * @self: указатель на #HyScanMarkManager
   *
   * Сигнал посылается при вертикальной прокрутке виджета.
   */
  hyscan_mark_manager_signals[SIGNAL_SCROLLED_VERTICAL] =
    g_signal_new ("v-scrolled", HYSCAN_TYPE_MARK_MANAGER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_ADJUSTMENT);
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
      case PROP_MODEL_MANAGER:
        {
          priv->model_manager = g_value_get_object (value);
          /* Увеличиваем счётчик ссылок на модель данных. */
          g_object_ref (priv->model_manager);
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
  GtkBox       *box                 = GTK_BOX (object);   /* Контейнер для панели инструментов и представления.*/
  GtkWidget    *toolbar             = gtk_toolbar_new (); /* Панель инструментов. */
  GtkWidget    *visibility_menu     = gtk_menu_new ();    /* Меню управления видимостью. */
  /* Пункты меню управления видимостью. */
  GtkWidget    *show_all_item       = gtk_menu_item_new_with_label (_(visibility_text[SHOW_ALL]));
  GtkWidget    *hide_all_item       = gtk_menu_item_new_with_label (_(visibility_text[HIDE_ALL]));
  GtkWidget    *show_selected_item  = gtk_menu_item_new_with_label (_(visibility_text[SHOW_SELECTED]));
  GtkWidget    *hide_selected_item  = gtk_menu_item_new_with_label (_(visibility_text[HIDE_SELECTED]));
  GtkToolItem  *new_label_item      = NULL;
  /* Кнопка для меню управления видимостью. */
  GtkToolItem  *visibility_item     = NULL;
  GtkToolItem  *separator           = gtk_separator_tool_item_new (); /* Разделитель. */
  GtkToolItem  *container           = gtk_tool_item_new (); /* Контейнер для выпадающего списка. */
  GtkTreeModel *model               = hyscan_model_manager_get_view_model (priv->model_manager);
  /* Размещаем панель инструментов сверху. */
  HyScanMarkManagerToolbarPosition toolbar_position = TOOLBAR_TOP;
  ModelManagerGrouping index,    /* Для обхода массива с пунктами меню выбора типа представления.*/
                       grouping; /* Для хранения текущего значения. */
  /* Подключаем сигнал об изменении типа группировки.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_GROUPING_CHANGED),
                            G_CALLBACK (hyscan_mark_manager_grouping_changed),
                            self);
  /* Подключаем сигнал об изменении режима отображения всех узлов.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_EXPAND_NODES_MODE_CHANGED),
                            G_CALLBACK (hyscan_mark_manager_expand_nodes_mode_changed),
                            self);
  /* Подключаем сигнал об обновлении модели представления данных.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_VIEW_MODEL_UPDATED),
                            G_CALLBACK (hyscan_mark_manager_view_model_updated),
                            self);
  /* Подключаем сигнал о выделении строки.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_ITEM_SELECTED),
                            G_CALLBACK (hyscan_mark_manager_select_item),
                            self);
  /* Подключаем сигнал об изменении состояния чек-бокса. */
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_ITEM_TOGGLED),
                            G_CALLBACK (hyscan_mark_manager_toggle_item),
                            self);
  /* Подключаем сигнал о горизонтальной прокрутке представления.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_VIEW_SCROLLED_HORIZONTAL),
                            G_CALLBACK (hyscan_mark_manager_view_scrolled_horizontal),
                            self);
  /* Подключаем сигнал о вертикальной прокрутке представления.*/
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_VIEW_SCROLLED_VERTICAL),
                            G_CALLBACK (hyscan_mark_manager_view_scrolled_vertical),
                            self);
  /* Виджет представления. */
  priv->view = hyscan_mark_manager_view_new (model);
  g_object_unref (model);
  /* Соединяем сигнал изменения выбранных элементов представления с функцией-обработчиком. */
  g_signal_connect_swapped (G_OBJECT (priv->view), "selected",
                            G_CALLBACK (hyscan_mark_manager_item_selected), self);
  g_signal_connect_swapped (G_OBJECT (priv->view), "toggled",
                            G_CALLBACK (hyscan_mark_manager_item_toggled), self);
  /* Кнопка "Создать новую группу". */
  new_label_item  = gtk_tool_button_new (NULL, _(tooltips_text[CREATE_NEW_GROUP]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (new_label_item), "insert-object");
  /* Кнопка для меню управления видимостью. */
  visibility_item = gtk_menu_tool_button_new (NULL, _(tooltips_text[DELETE_SELECTED]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (visibility_item), "edit-delete");
  gtk_widget_set_tooltip_text (GTK_WIDGET (new_label_item), _(tooltips_text[CREATE_NEW_GROUP]));
  /* Обработчик нажатия кнопки "Новая группа". */
  g_signal_connect (G_OBJECT (new_label_item), "clicked",
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
  priv->combo = gtk_combo_box_text_new ();
  for (index = UNGROUPED; index < N_VIEW_TYPES; index++)
    {
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (priv->combo), NULL, _(view_type_text[index]));
    }
  /* Обработчик выбора типа представления. */
  g_signal_connect (G_OBJECT (priv->combo), "changed",
                    G_CALLBACK (hyscan_mark_manager_set_grouping), self);
  gtk_widget_set_tooltip_text (priv->combo, _(tooltips_text[GROUPING]));
  /* Активируем текущий тип. */
  grouping = hyscan_model_manager_get_grouping (priv->model_manager);
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), grouping);
  /* Выпадающий список в контейнер. */
  gtk_container_add (GTK_CONTAINER (container), priv->combo);
  /* Кнопка "Развернуть / свернуть все узлы". */
  priv->nodes_item = gtk_toggle_tool_button_new ();
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (priv->nodes_item), "view-fullscreen");
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->nodes_item), _(tooltips_text[EXPAND_NODES]));
  priv->signal = g_signal_connect_swapped (G_OBJECT (priv->nodes_item), "clicked",
                                           G_CALLBACK (hyscan_mark_manager_show_nodes), self);
  /* Открыть / закрыть все узлы активно только для древовидного отображения. */
  if (grouping == UNGROUPED)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (priv->nodes_item), FALSE);
    }
  /* Заполняем панель инструментов. */
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), new_label_item,   -1);
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
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
    case TOOLBAR_RIGHT:
      {
        /* Панель инструментов справа. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
      }
      break;
    case TOOLBAR_BOTTOM:
      {
        /* Панель инструментов снизу. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
      }
      break;
    case TOOLBAR_TOP:
    default:
      {
        /* Панель инструментов сверху. */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
  }
  /* Подключаем сигнал горизонтальной прокрутки. */
  g_signal_connect (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->view)),
                    "value-changed",
                    G_CALLBACK (hyscan_mark_manager_scrolled_horizontal),
                    self);
  /* Подключаем сигнал вертикальной прокрутки. */
  g_signal_connect (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view)),
                    "value-changed",
                    G_CALLBACK (hyscan_mark_manager_scrolled_vertical),
                    self);
}
/* Деструктор. */
static void
hyscan_mark_manager_finalize (GObject *object)
{
  HyScanMarkManager *self = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  g_object_unref (priv->model_manager);
  priv->model_manager = NULL;

  G_OBJECT_CLASS (hyscan_mark_manager_parent_class)->finalize (object);
}
/* Обработчик нажатия кнопки "Новая группа". */
void
hyscan_mark_manager_create_new_label (GtkToolItem       *item,
                                      HyScanMarkManager *self)
{
  /* Создаём объект в базе данных. */
  HyScanMarkManagerPrivate *priv        = self->priv;
  HyScanObjectModel        *label_model = hyscan_model_manager_get_label_model (priv->model_manager);
  HyScanLabel              *label       = hyscan_label_new ();
  gint64                    time        = g_date_time_to_unix (g_date_time_new_now_local ());

  hyscan_label_set_text (label, "New label", "This is a new label", "User");
  hyscan_label_set_icon_name (label, "emblem-default");
  hyscan_label_set_label (label, 1);
  hyscan_label_set_ctime (label, time);
  hyscan_label_set_mtime (label, time);
  hyscan_object_model_add_object (label_model, (const HyScanObject*) label);

  g_object_unref (label_model);

}
/* Обработчик выбора тип представления. */
void
hyscan_mark_manager_set_grouping (GtkComboBox       *combo,
                                  HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate  *priv     = self->priv;
  ModelManagerGrouping       grouping = gtk_combo_box_get_active (combo);

  hyscan_model_manager_set_grouping (priv->model_manager, grouping);
}

/* Обработчик изменения типа группировки. */
void
hyscan_mark_manager_grouping_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate  *priv = self->priv;
  ModelManagerGrouping       grouping = hyscan_model_manager_get_grouping (priv->model_manager);

  gtk_combo_box_set_active (GTK_COMBO_BOX(priv->combo), grouping);
  gtk_widget_set_sensitive (priv->delete_icon, FALSE);

  if (priv->nodes_item != NULL)
    gtk_widget_set_sensitive (GTK_WIDGET(priv->nodes_item), grouping);

  /* Проверяем нужно ли развернуть узлы. */
  if (hyscan_model_manager_get_expand_nodes_mode (priv->model_manager))
    {
      hyscan_mark_manager_view_expand_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
    }
}

/* Обработчик изменения состояния кнопки "Развернуть/свернуть все узлы." */
void
hyscan_mark_manager_show_nodes (HyScanMarkManager *self,
                                GtkToolItem       *item)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  gboolean toggle = !hyscan_model_manager_get_expand_nodes_mode (priv->model_manager);

  hyscan_model_manager_set_expand_nodes_mode (priv->model_manager, toggle);
}

/* Обработчик изменения режима отображения всех узлов. */
void
hyscan_mark_manager_expand_nodes_mode_changed (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  gboolean toggle = hyscan_model_manager_get_expand_nodes_mode (priv->model_manager);

  /* Отключаем обработчик изменения состояния кнопки "Развернуть/свернуть все узлы.",
   * чтобы не было повторного вызова после gtk_toggle_tool_button_set_active.
   * */
  g_signal_handler_block (G_OBJECT (priv->nodes_item), priv->signal);
  /* Устанавливаем режим отображения всех узлов. */
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->nodes_item), toggle);
  /* Включаем обработчик изменения состояния кнопки "Развернуть/свернуть все узлы.",
   * чтобы снова его обрабатывать.
   * */
  g_signal_handler_unblock (G_OBJECT (priv->nodes_item), priv->signal);

  if (toggle)
    {
      gtk_tool_button_set_icon_name   (GTK_TOOL_BUTTON (priv->nodes_item), "view-restore");
      gtk_widget_set_tooltip_text (GTK_WIDGET (priv->nodes_item), _(tooltips_text[COLLAPSE_NODES]));
      hyscan_mark_manager_view_expand_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
    }
  else
    {
      gtk_tool_button_set_icon_name   (GTK_TOOL_BUTTON (priv->nodes_item), "view-fullscreen");
      gtk_widget_set_tooltip_text (GTK_WIDGET (priv->nodes_item), _(tooltips_text[EXPAND_NODES]));
      hyscan_mark_manager_view_collapse_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
    }
}

/* Обработчик сигнала обновления модели представления данных. */
void
hyscan_mark_manager_view_model_updated (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv           = self->priv;
  HyScanMarkManagerView    *view           = HYSCAN_MARK_MANAGER_VIEW (priv->view);
  GtkTreeModel             *model          = hyscan_model_manager_get_view_model (priv->model_manager);
  GtkTreeSelection         *selected_items = hyscan_model_manager_get_selected_items (priv->model_manager);

  if (model != NULL)
    {
      hyscan_mark_manager_view_set_store (view, model);
      g_object_unref (model);
    }
  if (selected_items)
    hyscan_mark_manager_view_set_selection (view, selected_items);
}

/* Обработчик сигнала горизонтальной прокрутки представления.
 * Сигнал приходит из #HyScanModelManagerView.*/
void
hyscan_mark_manager_scrolled_horizontal (GtkAdjustment *adjustment,
                                         gpointer       user_data)
{
  HyScanMarkManager *self;
  HyScanMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (user_data));

  self = HYSCAN_MARK_MANAGER (user_data);
  priv = self->priv;

  hyscan_model_manager_set_horizontal_adjustment (priv->model_manager, adjustment);
}

/* Обработчик сигнала вертикальной прокрутки представления.
 * Сигнал приходит из #HyScanModelManagerView.*/
void
hyscan_mark_manager_scrolled_vertical (GtkAdjustment *adjustment,
                                         gpointer       user_data)
{
  HyScanMarkManager *self;
  HyScanMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (user_data));

  self = HYSCAN_MARK_MANAGER (user_data);
  priv = self->priv;

  hyscan_model_manager_set_vertical_adjustment (priv->model_manager, adjustment);
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

  hyscan_mark_manager_view_select_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), TRUE);
  gtk_widget_set_sensitive(priv->delete_icon, TRUE);
}

/* Обработчик выбора пункта меню "Скрыть всё". */
void
hyscan_mark_manager_hide_all (GtkMenuItem       *item,
                              HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  hyscan_mark_manager_view_select_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), FALSE);
  gtk_widget_set_sensitive(priv->delete_icon, FALSE);
}

/* Обработчик выбора пункта меню "Показать выделенное". */
void
hyscan_mark_manager_show_selected (GtkMenuItem       *item,
                                   HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Show selected\n");
  hyscan_mark_manager_view_toggle_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), TRUE);
}

/* Обработчик выбора пункта меню "Скрыть выделенное". */
void
hyscan_mark_manager_hide_selected (GtkMenuItem       *item,
                                   HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Hide selected\n");
  hyscan_mark_manager_view_toggle_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), FALSE);
}

/* Функция-обработчик выделения объектов MarkManagerView. */
void
hyscan_mark_manager_item_selected (HyScanMarkManager *self,
                                   GtkTreeSelection  *selection)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  hyscan_model_manager_set_selection (priv->model_manager, selection);
}

/* Функция-обработчик выделения cтроки. Сигнал отправляет ModelManager. */
void
hyscan_mark_manager_select_item (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  GtkTreeSelection *selection = hyscan_model_manager_get_selected_items (priv->model_manager);

  if (selection != NULL)
    {
      gtk_widget_set_sensitive (priv->delete_icon, gtk_tree_selection_count_selected_rows (selection));

      hyscan_mark_manager_view_set_selection (HYSCAN_MARK_MANAGER_VIEW (priv->view), selection);
    }
}

/* Функция-обработчик изменения состояния чек-боксов в MarkManagerView. */
void
hyscan_mark_manager_item_toggled (HyScanMarkManager *self,
                                  gchar             *id,
                                  gboolean           active)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  hyscan_model_manager_toggle_item (priv->model_manager, id, active);
}

/* Функция-обработчик сигнала об изменении состояния чек-бокса.
 * Приходит из Model Manager-а. */
void
hyscan_mark_manager_toggle_item (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  g_print ("***\n");
  for (type = LABEL; type < TYPES; type++)
    {
      gchar **list = hyscan_model_manager_get_toggled_items (priv->model_manager, type);

      if (list != NULL)
        {
          gint i;

          switch (type)
            {
            case LABEL:
              g_print ("Labels:\n");
              break;
            case GEO_MARK:
              g_print ("Geo-marks:\n");
              break;
            case ACOUSTIC_MARK:
              g_print ("Acoustic marks:\n");
              break;
            case TRACK:
              g_print ("Tracks:\n");
              break;
            default:
              g_print ("Unknown objects:\n");
              break;
            }

          for (i = 0; list[i] != NULL; i++)
            {
              g_print ("Item #%i: %s\n", i + 1, list[i]);
            }

          g_strfreev (list);
        }
    }
  g_print ("***\n");
}

/* Функция-обработчик сигнал горизонтальной полосы прокрутки представления.
 * Сигнал приходит из #HyScanModelManager-а. */
void
hyscan_mark_manager_view_scrolled_horizontal (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (priv->view),
                                       hyscan_model_manager_get_horizontal_adjustment (priv->model_manager));
}

/* Функция-обработчик сигнал вертикальной полосы прокрутки представления.
 * Сигнал приходит из #HyScanModelManager-а.*/
void
hyscan_mark_manager_view_scrolled_vertical (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (priv->view),
                                       hyscan_model_manager_get_vertical_adjustment (priv->model_manager));
}

/* Функция удаляет объект*/
void hyscan_mark_manager_delete_label (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  HyScanMarkManagerPrivate *priv;
  HyScanObjectModel *label_model;
  gchar *id;
  GtkTreePath *tmp;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (data));

  priv = HYSCAN_MARK_MANAGER (data)->priv;
  label_model = hyscan_model_manager_get_label_model (priv->model_manager);
  /* Получаем идентификатор удаляемого объекта из нулевой колонки модели (COLUMN_ID) . */
  gtk_tree_model_get (model, iter, COLUMN_ID, &id, -1);
  hyscan_object_model_remove_object (label_model, id);

  g_object_unref (label_model);

  /*tmp = gtk_tree_path_copy (path);
  if (gtk_tree_path_up (tmp))
    {
      hyscan_mark_manager_view_expand_to_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), tmp);
    }*/
  hyscan_mark_manager_view_expand_to_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), path);
}

/**
 * hyscan_mark_manager_new:
 * @model_manager: Указатель на Менеджер Моделей
 *
 * Returns: cоздаёт новый объект #HyScanMarkManager
 */
GtkWidget*
hyscan_mark_manager_new (HyScanModelManager *model_manager)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_MARK_MANAGER,
                                   "model_manager",  model_manager,
                                   NULL));
}
