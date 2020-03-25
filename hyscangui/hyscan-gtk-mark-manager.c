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
  SIGNAL_SCROLLED_VERTICAL,   /* Прокрутка представления по вертикали. */
  SIGNAL_LAST
};
/* Идентификаторы текста меток и подсказок для элементов панели инструментов. */
enum
{
  CREATE_NEW_GROUP, /* Метка и подсказка для кноки "Создать новую группу". */
  DELETE_SELECTED,  /* Метка и подсказка для кноки "Удалить выделенное". */
  SHOW_OR_HIDE,     /* Метка для меню управления видимостью . */
  GROUPING,         /* Подсказка для выпадающего списка выбора типа представления . */
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
  EXPAND_ALL,    /* Развернуть все. */
  COLLAPSE_ALL,  /* Свернуть все. */
  TOGGLE_ALL,    /* Отметить все. */
  UNTOGGLE_ALL   /* Снять все отметки. */
}HyScanMarkManagerViewVisibility;

struct _HyScanMarkManagerPrivate
{
  HyScanModelManager *model_manager;     /* Менеджер Моделей. */

  GtkWidget          *view,              /* Виджет представления. */
                     *delete_icon,       /* Виджет для иконки для кнопки "Удалить выделенное". */
                     *combo,             /* Выпадающий список для выбора типа группировки. */
                     *expand_all_item,   /* Пункт выпадающего меню, "Развернуть все". */
                     *collapse_all_item; /* Пункт выпадающего меню, "Свернуть все". */

  GtkIconSize         icon_size;         /* Размер иконок. */
  gulong              signal;            /* Идентификатор сигнала изменения типа группировки.*/
};
/* Текст пунктов выпадающего списка для выбора типа представления. */
static gchar *view_type_text[]  = {N_("Ungrouped"),
                                   N_("By types"),
                                   N_("By labels")};
/* Текст пунктов меню. */
static gchar *visibility_text[] = {N_("Expand all"),
                                   N_("Collapse all"),
                                   N_("Toggle all"),
                                   N_("Untoggle all")};
/* Текст меток и подсказок для элементов панели инструментов. */
static gchar *tooltips_text[]   = {N_("Create new group"),
                                   N_("Delete selected"),
                                   N_("Actions"),
                                   N_("Grouping")};

static void         hyscan_mark_manager_set_property              (GObject               *object,
                                                                   guint                  prop_id,
                                                                   const GValue          *value,
                                                                   GParamSpec            *pspec);

static void         hyscan_mark_manager_constructed               (GObject               *object);

static void         hyscan_mark_manager_finalize                  (GObject               *object);

static void         hyscan_mark_manager_create_new_label          (GtkToolItem           *item,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_set_grouping              (GtkComboBox           *combo,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_delete_selected           (GtkToolButton         *button,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_expand_all                (GtkMenuItem           *item,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_collapse_all              (GtkMenuItem           *item,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_toggle_all                (GtkMenuItem           *item,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_untoggle_all              (GtkMenuItem           *item,
                                                                   HyScanMarkManager     *self);

static void         hyscan_mark_manager_item_selected             (HyScanMarkManager     *self,
                                                                   gchar                 *id);

static void         hyscan_mark_manager_select_item               (HyScanMarkManager     *self);

static void         hyscan_mark_manager_item_toggled              (HyScanMarkManager     *self,
                                                                   gchar                 *id,
                                                                   gboolean               active);

static void         hyscan_mark_manager_toggle_item               (HyScanMarkManager     *self);

static void         hyscan_mark_manager_view_scrolled_horizontal  (HyScanMarkManager     *self);

static void         hyscan_mark_manager_view_scrolled_vertical    (HyScanMarkManager     *self);

static void         hyscan_mark_manager_delete_label              (GtkTreeModel          *model,
                                                                   GtkTreePath           *path,
                                                                   GtkTreeIter           *iter,
                                                                   gpointer               data);

static void         hyscan_mark_manager_grouping_changed          (HyScanMarkManager     *self);

static void         hyscan_mark_manager_view_model_updated        (HyScanMarkManager     *self);

static void         hyscan_mark_manager_scrolled_horizontal       (GtkAdjustment         *adjustment,
                                                                   gpointer               user_data);

static void         hyscan_mark_manager_scrolled_vertical         (GtkAdjustment         *adjustment,
                                                                   gpointer               user_data);

static void         hyscan_mark_manager_item_expanded             (HyScanMarkManager     *self,
                                                                   gchar                 *id,
                                                                   gboolean               expanded);

static void         hyscan_mark_manager_expand_item               (HyScanMarkManager     *self);

static void         hyscan_mark_manager_collapse_item             (HyScanMarkManager     *self);

static void         hyscan_mark_manager_expand_current_item       (HyScanMarkManager     *self,
                                                                   gboolean               expanded);

static void         hyscan_mark_manager_expand_items              (HyScanMarkManager     *self,
                                                                   gboolean               expanded);

static void         hyscan_mark_manager_expand_all_items          (HyScanMarkManager     *self);

static GtkTreeIter* hyscan_mark_manager_find_item                 (GtkTreeModel          *model,
                                                                   const gchar           *id);

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
    g_signal_new ("h-scrolled", HYSCAN_TYPE_MARK_MANAGER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_ADJUSTMENT);

  /**
   * HyScanMarkManager::v-scrolled:
   * @self: указатель на #HyScanMarkManager
   *
   * Сигнал посылается при вертикальной прокрутке виджета.
   */
  hyscan_mark_manager_signals[SIGNAL_SCROLLED_VERTICAL] =
    g_signal_new ("v-scrolled", HYSCAN_TYPE_MARK_MANAGER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
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
  GtkWidget    *action_menu         = gtk_menu_new ();        /* Меню действий. */
  /* Пункты меню управления видимостью. */
  GtkWidget    *toggle_all_item     = gtk_menu_item_new_with_label (_(visibility_text[TOGGLE_ALL]));
  GtkWidget    *untoggle_all_item   = gtk_menu_item_new_with_label (_(visibility_text[UNTOGGLE_ALL]));
  GtkToolItem  *new_label_item      = NULL;
  /* Кнопка для меню управления видимостью. */
  GtkToolItem  *action_item     = NULL;
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
  /* Подключаем сигнал о разворачивании узла древовидного представления. */
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_ITEM_EXPANDED),
                            G_CALLBACK (hyscan_mark_manager_expand_item),
                            self);
  /* Подключаем сигнал о сворачивании узла древовидного представления. */
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_model_manager_get_signal_title (priv->model_manager,
                                                                   SIGNAL_ITEM_COLLAPSED),
                            G_CALLBACK (hyscan_mark_manager_collapse_item),
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
  /* Соединяем сигнал изменения состояния чек-боксов с функцией-обработчиком. */
  g_signal_connect_swapped (G_OBJECT (priv->view), "toggled",
                            G_CALLBACK (hyscan_mark_manager_item_toggled), self);
  /* Соединяем сигнал разворачивания узла древовидного представления с функцией-обработчиком. */
  g_signal_connect_swapped (G_OBJECT (priv->view), "expanded",
                            G_CALLBACK (hyscan_mark_manager_item_expanded), self);
  /* Кнопка "Создать новую группу". */
  new_label_item  = gtk_tool_button_new (NULL, _(tooltips_text[CREATE_NEW_GROUP]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (new_label_item), "insert-object");
  /* Кнопка для меню управления видимостью. */
  action_item = gtk_menu_tool_button_new (NULL, _(tooltips_text[DELETE_SELECTED]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (action_item), "edit-delete");
  gtk_widget_set_tooltip_text (GTK_WIDGET (new_label_item), _(tooltips_text[CREATE_NEW_GROUP]));
  /* Обработчик нажатия кнопки "Новая группа". */
  g_signal_connect (G_OBJECT (new_label_item), "clicked",
                    G_CALLBACK (hyscan_mark_manager_create_new_label), self);
  /* Иконка для кнопки "Удалить выделенное". */
  priv->delete_icon = gtk_image_new_from_icon_name ("edit-delete", priv->icon_size);
  /* Делаем неактивной кнопку "Удалить выделенное". */
  gtk_widget_set_sensitive (priv->delete_icon, FALSE);
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (action_item), priv->delete_icon);
  gtk_widget_set_tooltip_text (priv->delete_icon, _(tooltips_text[DELETE_SELECTED]));
  /* Создаём пункты выпадающего меню "Развернуть все" и "Свернуть все".  */
  priv->expand_all_item   = gtk_menu_item_new_with_label (_(visibility_text[EXPAND_ALL]));
  priv->collapse_all_item = gtk_menu_item_new_with_label (_(visibility_text[COLLAPSE_ALL]));
  /* Активируем текущий тип группировки. */
  grouping = hyscan_model_manager_get_grouping (priv->model_manager);
  /* Если представление табличное, то делаем их неактивными.*/
  if (grouping == UNGROUPED)
    {
      gtk_widget_set_sensitive (priv->expand_all_item,   FALSE);
      gtk_widget_set_sensitive (priv->collapse_all_item, FALSE);
    }
  /* Меню управления видимостью. */
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->expand_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->collapse_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), toggle_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), untoggle_all_item);
  /* Отображаем пункты меню управления видимостью. */
  gtk_widget_show (priv->expand_all_item);
  gtk_widget_show (priv->collapse_all_item);
  gtk_widget_show (toggle_all_item);
  gtk_widget_show (untoggle_all_item);

  /* Устанавливаем меню на кнопку в панели инструментов. */
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (action_item), action_menu);
  gtk_menu_tool_button_set_arrow_tooltip_text (GTK_MENU_TOOL_BUTTON (action_item),
                                              _(tooltips_text[SHOW_OR_HIDE]));
  /* Обработчик нажатия кнопки "Удалить выделенное" */
  g_signal_connect (G_OBJECT (action_item),             "clicked",
                    G_CALLBACK (hyscan_mark_manager_delete_selected), self);
  /* Обработчик выбора пункта меню "Развернуть все". */
  g_signal_connect (G_OBJECT (priv->expand_all_item),   "activate",
                    G_CALLBACK (hyscan_mark_manager_expand_all),      self);
  /* Обработчик выбора пункта меню "Свернуть все". */
  g_signal_connect (G_OBJECT (priv->collapse_all_item), "activate",
                    G_CALLBACK (hyscan_mark_manager_collapse_all),    self);
  /* Обработчик выбора пункта меню "Отметить все". */
  g_signal_connect (G_OBJECT (toggle_all_item),         "activate",
                    G_CALLBACK (hyscan_mark_manager_toggle_all),      self);
  /* Обработчик выбора пункта меню "Снять все отметки". */
  g_signal_connect (G_OBJECT (untoggle_all_item),       "activate",
                    G_CALLBACK (hyscan_mark_manager_untoggle_all),    self);
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
  priv->signal = g_signal_connect (G_OBJECT (priv->combo), "changed",
                    G_CALLBACK (hyscan_mark_manager_set_grouping), self);
  gtk_widget_set_tooltip_text (priv->combo, _(tooltips_text[GROUPING]));
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), grouping);
  /* Выпадающий список в контейнер. */
  gtk_container_add (GTK_CONTAINER (container), priv->combo);
  /* Заполняем панель инструментов. */
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), new_label_item,   -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), action_item,      -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), container,        -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), separator,        -1);
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
  ModelManagerGrouping   grouping = hyscan_model_manager_get_grouping (priv->model_manager);

  if (priv->signal != 0)
    {
      /* Отключаем сигнал. */
      g_signal_handler_block (G_OBJECT (priv->combo), priv->signal);
      /* Устанавливаем тип группировки. */
      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), grouping);
      /* Включаем сигнал. */
      g_signal_handler_unblock (G_OBJECT (priv->combo), priv->signal);
    }

  /* Если представление табличное, то делаем их неактивными.*/
  if (grouping == UNGROUPED)
    {
      gtk_widget_set_sensitive (priv->expand_all_item,   FALSE);
      gtk_widget_set_sensitive (priv->collapse_all_item, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->expand_all_item,   TRUE);
      gtk_widget_set_sensitive (priv->collapse_all_item, TRUE);
    }
  /* Выделяем выбранное */
  hyscan_mark_manager_select_item (self);
  /* Чтобы правильно отобразить выделенное нужно установить фокус на GtkTreeView. */
  /*hyscan_mark_manager_view_set_focus (HYSCAN_MARK_MANAGER_VIEW (priv->view));*/
}

/* Обработчик изменения состояния кнопки "Развернуть/свернуть все узлы." */
/*void
hyscan_mark_manager_show_nodes (HyScanMarkManager *self,
                                GtkToolItem       *item)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  gboolean toggle = !hyscan_model_manager_get_expand_nodes_mode (priv->model_manager);

  hyscan_model_manager_set_expand_nodes_mode (priv->model_manager, toggle);
}*/

/* Обработчик сигнала обновления модели представления данных. */
void
hyscan_mark_manager_view_model_updated (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv  = self->priv;
  HyScanMarkManagerView    *view  = HYSCAN_MARK_MANAGER_VIEW (priv->view);
  GtkTreeModel             *model = hyscan_model_manager_get_view_model (priv->model_manager);
  gchar                    *id    = hyscan_model_manager_get_selected_item (priv->model_manager);

  if (model != NULL)
    {
      hyscan_mark_manager_view_set_store (view, model);
      g_object_unref (model);
    }

  hyscan_mark_manager_expand_all_items (self);

  if (id != NULL)
    hyscan_mark_manager_view_set_selection (view, id);
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
                           GTK_WIDGET (gtk_tool_button_get_icon_widget (button)));
  HyScanMarkManagerView *view = HYSCAN_MARK_MANAGER_VIEW (priv->view);

  if (sensivity && hyscan_mark_manager_view_has_selected (view))
    {
       GtkTreeSelection *selection = hyscan_mark_manager_view_get_selection (view);
       /* Удаляем объект из базы данных. */
       gtk_tree_selection_selected_foreach (selection, hyscan_mark_manager_delete_label, self);
       gtk_widget_set_sensitive (priv->delete_icon, FALSE);
    }
}

/* Обработчик выбора пункта меню "Развернуть всё". */
void
hyscan_mark_manager_expand_all (GtkMenuItem       *item,
                                HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
/*
  hyscan_mark_manager_view_select_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), TRUE);
  gtk_widget_set_sensitive(priv->delete_icon, TRUE);
*/
  hyscan_mark_manager_view_expand_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
}

/* Обработчик выбора пункта меню "Свернуть всё". */
void
hyscan_mark_manager_collapse_all (GtkMenuItem       *item,
                                  HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
/*
  hyscan_mark_manager_view_select_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), FALSE);
  gtk_widget_set_sensitive(priv->delete_icon, FALSE);
*/
  hyscan_mark_manager_view_collapse_all (HYSCAN_MARK_MANAGER_VIEW (priv->view));
}

/* Обработчик выбора пункта меню "Отметить все". */
void
hyscan_mark_manager_toggle_all (GtkMenuItem       *item,
                                   HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Show selected\n");
  hyscan_mark_manager_view_toggle_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), TRUE);
}

/* Обработчик выбора пункта меню "Снять все отметки". */
void
hyscan_mark_manager_untoggle_all (GtkMenuItem       *item,
                                HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  g_print ("Hide selected\n");
  hyscan_mark_manager_view_toggle_all (HYSCAN_MARK_MANAGER_VIEW (priv->view), FALSE);
}

/* Функция-обработчик выделения объектов MarkManagerView. */
void
hyscan_mark_manager_item_selected (HyScanMarkManager *self,
                                   gchar             *id)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  hyscan_model_manager_set_selection (priv->model_manager, id);
}

/* Функция-обработчик выделения cтроки. Сигнал отправляет ModelManager. */
void
hyscan_mark_manager_select_item (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  gchar *id = hyscan_model_manager_get_selected_item (priv->model_manager);

  if (id != NULL)
    {
      gtk_widget_set_sensitive (priv->delete_icon, id);

      hyscan_mark_manager_view_set_selection (HYSCAN_MARK_MANAGER_VIEW (priv->view), id);
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
  /*GtkTreePath *tmp;*/

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (data));

  priv = HYSCAN_MARK_MANAGER (data)->priv;
  label_model = hyscan_model_manager_get_label_model (priv->model_manager);
  /* Получаем идентификатор удаляемого объекта из нулевой колонки модели (COLUMN_ID) . */
  gtk_tree_model_get (model, iter, COLUMN_ID, &id, -1);
  /* Пока что ничего не удаляем. */
  /*hyscan_object_model_remove_object (label_model, id);*/

  g_object_unref (label_model);

  /*tmp = gtk_tree_path_copy (path);
  if (gtk_tree_path_up (tmp))
    {
      hyscan_mark_manager_view_expand_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), tmp);
    }
  gtk_tree_path_free (tmp);*/
  hyscan_mark_manager_view_expand_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), path, TRUE);
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanModelManagerView. */
void
hyscan_mark_manager_item_expanded (HyScanMarkManager *self,
                                   gchar             *id,
                                   gboolean           expanded)
{
  HyScanMarkManagerPrivate *priv = self->priv;

  hyscan_model_manager_expand_item (priv->model_manager, id, expanded);
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanModelManager-а.*/
void
hyscan_mark_manager_expand_item (HyScanMarkManager *self)
{
  g_print ("EXPANDING SIGNAL FROM MODEL MANAGER CATCHED\n");
  hyscan_mark_manager_expand_current_item (self, TRUE);
}

/* Функция-обработчик сигнала о cворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanModelManager-а.*/
void
hyscan_mark_manager_collapse_item (HyScanMarkManager *self)
{
  g_print ("COLLAPSING SIGNAL FROM MODEL MANAGER CATCHED\n");
  hyscan_mark_manager_expand_current_item (self, FALSE);
}

void
hyscan_mark_manager_expand_current_item (HyScanMarkManager *self,
                                         gboolean           expanded)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  GtkTreeModel *model = hyscan_model_manager_get_view_model (priv->model_manager);
  gchar        *id    = hyscan_model_manager_get_current_id (priv->model_manager);
  GtkTreeIter  *iter  = hyscan_mark_manager_find_item (model, id);

  if (iter != NULL)
    {
      GtkTreePath *path = gtk_tree_model_get_path (model, iter);

      g_print ("ID: %s\nPath: %s\n", id, gtk_tree_path_to_string (path));

      hyscan_mark_manager_view_expand_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), path, expanded);

      gtk_tree_path_free (path);
    }

  g_object_unref (model);
  if (iter != NULL)
    gtk_tree_iter_free (iter);
}

/* Обновляет состояние всех узлов древовидного представления.
 * Разворачивает те узлы, которые нужно развернуть и
 * сворачивает те узлы, которые нужно свернуть.
 * */
void
hyscan_mark_manager_expand_all_items (HyScanMarkManager *self)
{
  hyscan_mark_manager_expand_items (self, FALSE);
  hyscan_mark_manager_expand_items (self, TRUE);
}

/* Разворачивает или сворачивает узлы древовидного
 * представления в зависимости от заданного
 * аргумента expanded:
 * TRUE  - разворачивает, те что нужно развернуть;
 * FALSE - сворачивает, те что нужно свернуть.
 * */
void
hyscan_mark_manager_expand_items (HyScanMarkManager *self,
                                 gboolean           expanded)
{
  HyScanMarkManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      gchar **list = hyscan_model_manager_get_expanded_items (priv->model_manager, type, expanded);

      if (list != NULL)
        {
          gint i;

          for (i = 0; list[i] != NULL; i++)
            {
              GtkTreeModel *model = hyscan_model_manager_get_view_model (priv->model_manager);
              GtkTreeIter  *iter  = hyscan_mark_manager_find_item (model, list[i]);

              if (iter != NULL)
                {
                  GtkTreePath *path = gtk_tree_model_get_path (model, iter);

                  hyscan_mark_manager_view_expand_path (HYSCAN_MARK_MANAGER_VIEW (priv->view), path, expanded);

                  gtk_tree_path_free (path);
                }

              g_object_unref (model);
              if (iter != NULL)
                gtk_tree_iter_free (iter);
            }

          g_strfreev (list);
        }
    }
}

/* Функция ищет в модели запись по заданному идентификатору и
 * возвращает итератор для этой записи. Когда итератор больше не нужен,
 * необходимо использовать #gtk_tree_iter_free ().
 * */
GtkTreeIter*
hyscan_mark_manager_find_item (GtkTreeModel *model,
                               const gchar  *id)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          if (hyscan_mark_manager_view_find_item_by_id (model, &iter, id))
            {
              return gtk_tree_iter_copy (&iter);
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
  return NULL;
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
