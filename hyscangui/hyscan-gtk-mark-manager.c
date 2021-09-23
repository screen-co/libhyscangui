/* hyscan-gtk-mark-manager.c
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
 * SECTION: hyscan-gtk-mark-manager
 * @Short_description: Класс реализует виджет Журнала Меток
 * @Title: HyScanGtkMarkManager
 * @See_also: #HyScanGtkMarkManagerView, #HyScanGtkModelManager,
 *            #HyScanGtkMarkManagerCreateLabelDialog,
 *            #HyScanGtkMarkManagerSetLabelsDialog
 *
 * Виджет #HyScanMarkManager управляет представлением и реализует функции
 * для работы с объектами Журнала Меток.
 *
 * - hyscan_gtk_mark_manager_new () - создание нового виджета.
 */

#include <hyscan-gtk-mark-manager.h>
#include <hyscan-gtk-mark-manager-create-label-dialog.h>
#include <hyscan-gtk-mark-manager-set-labels-dialog.h>
#include <hyscan-gtk-mark-export.h>
#include <glib/gi18n-lib.h>

#define MAX_LABELS 32 /* Максимальное количество групп. */

enum
{
  PROP_MODEL_MANAGER = 1, /* Менеджер Моделей. */
  N_PROPERTIES
};

/* Идентификаторы текста меток и подсказок для элементов панели инструментов. */
enum
{
  CREATE_NEW_GROUP,       /* Метка и подсказка для кноки "Создать новую группу". */
  DELETE_SELECTED,        /* Метка и подсказка для кноки "Удалить выделенное". */
  ACTIONS,                /* Метка для выпадающего меню. */
  GROUPING,               /* Подсказка для выпадающего списка выбора типа представления . */
};

/* Действия (пункты выпадающего меню). */
enum
{
  EXPAND_ALL,             /* Развернуть все. */
  COLLAPSE_ALL,           /* Свернуть все. */
  TOGGLE_ALL,             /* Отметить все. */
  UNTOGGLE_ALL,           /* Снять все отметки. */
  SET_LABELS,             /* Установить группы. */
  SAVE_AS_HTML            /* Сохранить как HTML. */
};

/* Положение панели инструментов относительно виджета представления. */
typedef enum
{
  TOOLBAR_LEFT,           /* Слева. */
  TOOLBAR_RIGHT,          /* Справа. */
  TOOLBAR_BOTTOM,         /* Снизу.*/
  TOOLBAR_TOP             /* Сверху. */
}HyScanGtkMarkManagerToolbarPosition;

struct _HyScanGtkMarkManagerPrivate
{
  HyScanGtkModelManager *model_manager;       /* Менеджер Моделей. */

  GtkWidget             *view,                /* Виджет представления. */
                        *delete_icon,         /* Виджет для иконки для кнопки "Удалить выделенное". */
                        *combo,               /* Выпадающий список для выбора типа группировки. */
                        *expand_all_item,     /* Пункт выпадающего меню "Развернуть все". */
                        *collapse_all_item,   /* Пункт выпадающего меню "Свернуть все". */
                        *toggle_all_item,     /* Пункт выпадающего меню "Отметить все". */
                        *untoggle_all_item,   /* Снять все отметки. */
                        *set_labels,          /* Пункт выпадающего меню "Установить группы". */
                        *save_as_html,        /* Пункт выпадающего меню "Сохранить в формате HTML". */
                        *new_label_dialog,    /* Диалог создания новой группы. */
                        *set_labels_dialog;   /* Диалог переноса объектов в другую группу. */
  GtkToolItem           *new_label_item;      /* Кнопка "Новая группа". */
  GtkIconSize            icon_size;           /* Размер иконок. */
  gulong                 signal;              /* Идентификатор сигнала изменения типа группировки.*/
};
/* Текст пунктов выпадающего списка для выбора типа представления. */
static gchar *view_type_text[] = {N_("Ungrouped"),
                                  N_("By types"),
                                  N_("By labels")};
/* Текст пунктов меню. */
static gchar *action_text[]    = {N_("Expand all"),
                                  N_("Collapse all"),
                                  N_("Toggle all"),
                                  N_("Untoggle all"),
                                  N_("Set labels"),
                                  N_("Save as HTML")};
/* Текст меток и подсказок для элементов панели инструментов. */
static gchar *tooltips_text[]  = {N_("Create new group"),
                                  N_("Delete toggled"),
                                  N_("Actions"),
                                  N_("Grouping")};

static void         hyscan_gtk_mark_manager_set_property                (GObject              *object,
                                                                         guint                 prop_id,
                                                                         const GValue         *value,
                                                                         GParamSpec           *pspec);

static void         hyscan_gtk_mark_manager_constructed                 (GObject              *object);

static void         hyscan_gtk_mark_manager_finalize                    (GObject              *object);

static void         hyscan_gtk_mark_manager_create_new_label            (GtkToolItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_release_new_label_dialog    (GtkWidget            *dialog,
                                                                         gpointer              user_data);

static void         hyscan_gtk_mark_manager_set_grouping                (GtkComboBox          *combo,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_delete_toggled              (GtkToolButton        *button,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_expand_all                  (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_collapse_all                (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_toggle_all                  (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_untoggle_all                (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_set_labels_dialog_create    (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_release_set_labels_dialog   (GtkWidget            *dialog,
                                                                         gpointer              user_data);

static void         hyscan_gtk_mark_manager_toggled_items_set_labels    (HyScanGtkMarkManager *self,
                                                                         gint64                labels);

static void         hyscan_gtk_mark_manager_item_selected               (HyScanGtkMarkManager *self,
                                                                         gchar                *id);

static void         hyscan_gtk_mark_manager_select_item                 (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_unselect                    (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_unselect_all                (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_labels_changed              (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_item_toggled                (HyScanGtkMarkManager *self,
                                                                         gchar                *id,
                                                                         gboolean              active);

static void         hyscan_gtk_mark_manager_toggle_item                 (HyScanGtkMarkManager *self,
                                                                         gchar                *id,
                                                                         gboolean              active);

static void         hyscan_gtk_mark_manager_view_scrolled_horizontal    (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_view_scrolled_vertical      (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_grouping_changed            (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_view_model_updated          (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_scrolled_horizontal         (GtkAdjustment        *adjustment,
                                                                         gpointer              user_data);

static void         hyscan_gtk_mark_manager_scrolled_vertical           (GtkAdjustment        *adjustment,
                                                                         gpointer              user_data);

static void         hyscan_gtk_mark_manager_item_expanded               (HyScanGtkMarkManager *self,
                                                                         gchar                *id,
                                                                         gboolean              expanded);

static void         hyscan_gtk_mark_manager_expand_item                 (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_collapse_item               (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_expand_current_item         (HyScanGtkMarkManager *self,
                                                                         gboolean              expanded);

static void         hyscan_gtk_mark_manager_expand_items                (HyScanGtkMarkManager *self,
                                                                         gboolean              expanded);

static void         hyscan_gtk_mark_manager_expand_all_items            (HyScanGtkMarkManager *self);

static void         hyscan_gtk_mark_manager_toggled_items_save_as_html  (GtkMenuItem          *item,
                                                                         HyScanGtkMarkManager *self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMarkManager, hyscan_gtk_mark_manager, GTK_TYPE_BOX)

static void
hyscan_gtk_mark_manager_class_init (HyScanGtkMarkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_mark_manager_set_property;
  object_class->constructed  = hyscan_gtk_mark_manager_constructed;
  object_class->finalize     = hyscan_gtk_mark_manager_finalize;

  g_object_class_install_property (object_class, PROP_MODEL_MANAGER,
    g_param_spec_object ("model_manager", "ModelManager", "Model Manager",
                         HYSCAN_TYPE_GTK_MODEL_MANAGER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_mark_manager_init (HyScanGtkMarkManager *self)
{
  self->priv = hyscan_gtk_mark_manager_get_instance_private (self);
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
hyscan_gtk_mark_manager_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanGtkMarkManager *self        = HYSCAN_GTK_MARK_MANAGER (object);
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_MODEL_MANAGER:
      priv->model_manager = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_mark_manager_constructed (GObject *object)
{
  HyScanGtkMarkManager        *self = HYSCAN_GTK_MARK_MANAGER (object);
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  GtkBox       *box                 = GTK_BOX (object);   /* Контейнер для панели инструментов и представления.*/
  GtkWidget    *toolbar             = gtk_toolbar_new (); /* Панель инструментов. */
  GtkWidget    *action_menu         = gtk_menu_new ();    /* Меню действий. */
  GtkToolItem  *action_item         = NULL;
  GtkToolItem  *separator           = gtk_separator_tool_item_new (); /* Разделитель. */
  GtkToolItem  *container           = gtk_tool_item_new (); /* Контейнер для выпадающего списка. */
  GtkTreeModel *model               = hyscan_gtk_model_manager_get_view_model (priv->model_manager);
  /* Размещаем панель инструментов сверху. */
  HyScanGtkMarkManagerToolbarPosition toolbar_position = TOOLBAR_TOP;
  ModelManagerGrouping grouping;  /* Для хранения текущего значения. */

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_parent_class)->constructed (object);

  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_GROUPING_CHANGED),
                            G_CALLBACK (hyscan_gtk_mark_manager_grouping_changed),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_VIEW_MODEL_UPDATED),
                            G_CALLBACK (hyscan_gtk_mark_manager_view_model_updated),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_ITEM_SELECTED),
                            G_CALLBACK (hyscan_gtk_mark_manager_select_item),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_ITEM_TOGGLED),
                            G_CALLBACK (hyscan_gtk_mark_manager_toggle_item),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_ITEM_EXPANDED),
                            G_CALLBACK (hyscan_gtk_mark_manager_expand_item),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_ITEM_COLLAPSED),
                            G_CALLBACK (hyscan_gtk_mark_manager_collapse_item),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_VIEW_SCROLLED_HORIZONTAL),
                            G_CALLBACK (hyscan_gtk_mark_manager_view_scrolled_horizontal),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_VIEW_SCROLLED_VERTICAL),
                            G_CALLBACK (hyscan_gtk_mark_manager_view_scrolled_vertical),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_UNSELECT_ALL),
                            G_CALLBACK (hyscan_gtk_mark_manager_unselect_all),
                            self);
  g_signal_connect_swapped (priv->model_manager,
                            hyscan_gtk_model_manager_get_signal_title (priv->model_manager,
                                                                       SIGNAL_LABELS_CHANGED),
                            G_CALLBACK (hyscan_gtk_mark_manager_labels_changed),
                            self);

  priv->view = hyscan_gtk_mark_manager_view_new (model);
  g_object_unref (model);

  g_signal_connect_swapped (G_OBJECT (priv->view), "selected",
                            G_CALLBACK (hyscan_gtk_mark_manager_item_selected), self);
  g_signal_connect_swapped (G_OBJECT (priv->view), "unselect",
                            G_CALLBACK (hyscan_gtk_mark_manager_unselect), self);
  g_signal_connect_swapped (G_OBJECT (priv->view), "toggled",
                            G_CALLBACK (hyscan_gtk_mark_manager_item_toggled), self);
  g_signal_connect_swapped (G_OBJECT (priv->view), "expanded",
                            G_CALLBACK (hyscan_gtk_mark_manager_item_expanded), self);

  priv->new_label_item = gtk_tool_button_new (NULL, _(tooltips_text[CREATE_NEW_GROUP]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (priv->new_label_item), "insert-object");
  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->new_label_item), _(tooltips_text[CREATE_NEW_GROUP]));
  g_signal_connect (G_OBJECT (priv->new_label_item), "clicked",
                    G_CALLBACK (hyscan_gtk_mark_manager_create_new_label), self);
  /* Если создано более MAX_LABELS групп, делаем кнопку "Новая группа" неактивной. */
  hyscan_gtk_mark_manager_labels_changed (self);

  action_item = gtk_menu_tool_button_new (NULL, _(tooltips_text[DELETE_SELECTED]));
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (action_item), "edit-delete");

  priv->delete_icon = gtk_image_new_from_icon_name ("edit-delete", priv->icon_size);
  gtk_widget_set_sensitive (priv->delete_icon, FALSE);
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (action_item), priv->delete_icon);
  gtk_widget_set_tooltip_text (priv->delete_icon, _(tooltips_text[DELETE_SELECTED]));
  /* Создаём пункты выпадающего меню.*/
  priv->expand_all_item   = gtk_menu_item_new_with_label (_(action_text[EXPAND_ALL]));
  priv->collapse_all_item = gtk_menu_item_new_with_label (_(action_text[COLLAPSE_ALL]));
  priv->toggle_all_item   = gtk_menu_item_new_with_label (_(action_text[TOGGLE_ALL]));
  priv->untoggle_all_item = gtk_menu_item_new_with_label (_(action_text[UNTOGGLE_ALL]));
  priv->set_labels        = gtk_menu_item_new_with_label (_(action_text[SET_LABELS]));
  priv->save_as_html      = gtk_menu_item_new_with_label (_(action_text[SAVE_AS_HTML]));

  gtk_widget_set_sensitive (priv->set_labels,   FALSE);
  gtk_widget_set_sensitive (priv->save_as_html, FALSE);

  grouping = hyscan_gtk_model_manager_get_grouping (priv->model_manager);

  if (grouping == UNGROUPED)
    {
      gtk_widget_set_sensitive (priv->expand_all_item,   FALSE);
      gtk_widget_set_sensitive (priv->collapse_all_item, FALSE);
    }
  gtk_widget_set_sensitive (priv->untoggle_all_item, FALSE);

  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->expand_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->collapse_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->toggle_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->untoggle_all_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->set_labels);
  gtk_menu_shell_append (GTK_MENU_SHELL (action_menu), priv->save_as_html);

  gtk_widget_show (priv->expand_all_item);
  gtk_widget_show (priv->collapse_all_item);
  gtk_widget_show (priv->toggle_all_item);
  gtk_widget_show (priv->untoggle_all_item);
  gtk_widget_show (priv->set_labels);
  gtk_widget_show (priv->save_as_html);

  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (action_item), action_menu);
  gtk_menu_tool_button_set_arrow_tooltip_text (GTK_MENU_TOOL_BUTTON (action_item),
                                              _(tooltips_text[ACTIONS]));

  g_signal_connect (G_OBJECT (action_item),             "clicked",
                    G_CALLBACK (hyscan_gtk_mark_manager_delete_toggled), self);
  g_signal_connect (G_OBJECT (priv->expand_all_item),   "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_expand_all),     self);
  g_signal_connect (G_OBJECT (priv->collapse_all_item), "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_collapse_all),   self);
  g_signal_connect (G_OBJECT (priv->toggle_all_item),   "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_toggle_all),     self);
  g_signal_connect (G_OBJECT (priv->untoggle_all_item), "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_untoggle_all),   self);
  g_signal_connect (G_OBJECT (priv->set_labels),      "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_set_labels_dialog_create), self);
  g_signal_connect (G_OBJECT (priv->save_as_html),      "activate",
                    G_CALLBACK (hyscan_gtk_mark_manager_toggled_items_save_as_html), self);

  priv->combo = gtk_combo_box_text_new ();
  for (ModelManagerGrouping index = UNGROUPED; index < N_VIEW_TYPES; index++)
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (priv->combo), NULL, _(view_type_text[index]));
  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), grouping);

  priv->signal = g_signal_connect (G_OBJECT (priv->combo), "changed",
                    G_CALLBACK (hyscan_gtk_mark_manager_set_grouping), self);
  gtk_widget_set_tooltip_text (priv->combo, _(tooltips_text[GROUPING]));
  gtk_container_add (GTK_CONTAINER (container), priv->combo);

  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), priv->new_label_item, -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), action_item,          -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), container,            -1);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), separator,            -1);

  switch (toolbar_position)
  {
    case TOOLBAR_LEFT:
      {
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
    case TOOLBAR_RIGHT:
      {
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
      }
      break;
    case TOOLBAR_BOTTOM:
      {
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
      }
      break;
    case TOOLBAR_TOP:
    default:
      {
        gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (box, toolbar, FALSE, TRUE, 0);
        gtk_box_pack_start (box, priv->view, TRUE, TRUE, 0);
      }
      break;
  }

  g_signal_connect (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->view)),
                    "value-changed",
                    G_CALLBACK (hyscan_gtk_mark_manager_scrolled_horizontal),
                    self);
  g_signal_connect (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view)),
                    "value-changed",
                    G_CALLBACK (hyscan_gtk_mark_manager_scrolled_vertical),
                    self);
}

static void
hyscan_gtk_mark_manager_finalize (GObject *object)
{
  HyScanGtkMarkManager *self = HYSCAN_GTK_MARK_MANAGER (object);
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  g_object_unref (priv->model_manager);
  priv->model_manager = NULL;

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_parent_class)->finalize (object);
}
/* Обработчик нажатия кнопки "Новая группа". */
static void
hyscan_gtk_mark_manager_create_new_label (GtkToolItem          *item,
                                          HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  HyScanObjectModel *label_model;
  GtkWindow *parent;

  if (priv->new_label_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->new_label_dialog));
      return;
    }
  label_model = hyscan_gtk_model_manager_get_label_model (priv->model_manager);
  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));

  priv->new_label_dialog = hyscan_gtk_mark_manager_create_label_dialog_new (parent, label_model);

  g_signal_connect (priv->new_label_dialog, "destroy",
                    G_CALLBACK (hyscan_gtk_mark_manager_release_new_label_dialog), self);

  g_object_unref (label_model);
}

/* Функция-обработчик сигнала закрытия диалога создания новой группы.*/
static void
hyscan_gtk_mark_manager_release_new_label_dialog (GtkWidget *dialog,
                                                  gpointer   user_data)
{
  HyScanGtkMarkManager *self;
  HyScanGtkMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER (user_data));

  self = HYSCAN_GTK_MARK_MANAGER (user_data);
  priv = self->priv;

  priv->new_label_dialog = NULL;
}

/* Обработчик выбора тип представления. */
static void
hyscan_gtk_mark_manager_set_grouping (GtkComboBox          *combo,
                                      HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  ModelManagerGrouping     grouping = gtk_combo_box_get_active (combo);

  hyscan_gtk_mark_manager_view_block_signal_selected (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), TRUE);
  hyscan_gtk_model_manager_set_grouping (priv->model_manager, grouping);
  hyscan_gtk_mark_manager_view_block_signal_selected (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), FALSE);
}

/* Обработчик изменения типа группировки. */
static void
hyscan_gtk_mark_manager_grouping_changed (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  ModelManagerGrouping grouping = hyscan_gtk_model_manager_get_grouping (priv->model_manager);
  gboolean has_toggled = hyscan_gtk_model_manager_has_toggled (priv->model_manager);

  if (priv->signal != 0)
    {
      g_signal_handler_block (G_OBJECT (priv->combo), priv->signal);
      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), grouping);
      g_signal_handler_unblock (G_OBJECT (priv->combo), priv->signal);
    }

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

  gtk_widget_set_sensitive (priv->delete_icon,       has_toggled);
  gtk_widget_set_sensitive (priv->untoggle_all_item, has_toggled);
  gtk_widget_set_sensitive (priv->set_labels,        has_toggled);
  gtk_widget_set_sensitive (priv->save_as_html,      has_toggled);

  hyscan_gtk_mark_manager_select_item (self);
}

/* Обработчик сигнала обновления модели представления данных.
 * Сигнал приходит из #HyScanGtkModelManager. */
static void
hyscan_gtk_mark_manager_view_model_updated (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv  = self->priv;
  HyScanGtkMarkManagerView *view  = HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view);
  GtkTreeModel             *model = hyscan_gtk_model_manager_get_view_model (priv->model_manager);
  gchar                    *id    = hyscan_gtk_model_manager_get_selected_item (priv->model_manager);

  if (model != NULL)
    {
      hyscan_gtk_mark_manager_view_set_store (view, model);
      g_object_unref (model);
    }

  hyscan_gtk_mark_manager_expand_all_items (self);

  if (id != NULL)
    hyscan_gtk_mark_manager_view_select_item (view, id);
}

/* Обработчик сигнала горизонтальной прокрутки представления.
 * Сигнал приходит из #HyScanGtkMarkManagerView.*/
static void
hyscan_gtk_mark_manager_scrolled_horizontal (GtkAdjustment *adjustment,
                                             gpointer       user_data)
{
  HyScanGtkMarkManager *self;
  HyScanGtkMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER (user_data));

  self = HYSCAN_GTK_MARK_MANAGER (user_data);
  priv = self->priv;

  hyscan_gtk_model_manager_set_horizontal_adjustment (priv->model_manager,
                                                      gtk_adjustment_get_value (adjustment));
}

/* Обработчик сигнала вертикальной прокрутки представления.
 * Сигнал приходит из #HyScanGtkModelManagerView.*/
static void
hyscan_gtk_mark_manager_scrolled_vertical (GtkAdjustment *adjustment,
                                           gpointer       user_data)
{
  HyScanGtkMarkManager *self;
  HyScanGtkMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER (user_data));

  self = HYSCAN_GTK_MARK_MANAGER (user_data);
  priv = self->priv;

  hyscan_gtk_model_manager_set_vertical_adjustment (priv->model_manager,
                                                    gtk_adjustment_get_value (adjustment));
}

/* Обработчик нажатия кнопки "Удалить выбранное". */
static void
hyscan_gtk_mark_manager_delete_toggled (GtkToolButton        *button,
                                        HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  GtkWidget *box, *image, *label, *dialog;

  if (!gtk_widget_get_sensitive (priv->delete_icon))
    return;

  box    = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  image  = gtk_image_new_from_icon_name ("dialog-question", GTK_ICON_SIZE_DIALOG);
  label  = gtk_label_new (_("Toggled objects will delete irrevocably.\nContunue?"));
  dialog = gtk_dialog_new_with_buttons (_("Delete toggled"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Ok"),     GTK_RESPONSE_OK,
                                        NULL);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 10);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), box);
  gtk_widget_show_all (dialog);

  if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      /* Удаляем объект из базы данных. */
      hyscan_gtk_model_manager_delete_toggled_items (priv->model_manager);

      gtk_widget_set_sensitive (priv->delete_icon,       FALSE);
      gtk_widget_set_sensitive (priv->untoggle_all_item, FALSE);
      gtk_widget_set_sensitive (priv->set_labels,        FALSE);
      gtk_widget_set_sensitive (priv->save_as_html,      FALSE);
    }
  gtk_widget_destroy (dialog);
}

/* Обработчик выбора пункта меню "Развернуть всё". */
static void
hyscan_gtk_mark_manager_expand_all (GtkMenuItem          *item,
                                    HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_mark_manager_view_expand_all (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view));
}

/* Обработчик выбора пункта меню "Свернуть всё". */
static void
hyscan_gtk_mark_manager_collapse_all (GtkMenuItem          *item,
                                      HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_mark_manager_view_collapse_all (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view));
}

/* Обработчик выбора пункта меню "Отметить все". */
static void
hyscan_gtk_mark_manager_toggle_all (GtkMenuItem          *item,
                                    HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  hyscan_gtk_mark_manager_view_toggle_all (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), TRUE);
}

/* Обработчик выбора пункта меню "Снять все отметки". */
static void
hyscan_gtk_mark_manager_untoggle_all (GtkMenuItem          *item,
                                      HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  hyscan_gtk_mark_manager_view_toggle_all (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), FALSE);
}

/* Обработчик выбора пункта меню "Перенести в группу". */
static void
hyscan_gtk_mark_manager_set_labels_dialog_create (GtkMenuItem          *item,
                                                  HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  HyScanObjectModel *label_model;
  GtkWindow *parent;
  guint64 labels,        /* Битовая маска общих групп. */
          inconsistents; /* Битовая маска групп с неопределённым статусом. */

  if (priv->set_labels_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->set_labels_dialog));
      return;
    }
  label_model = hyscan_gtk_model_manager_get_label_model (priv->model_manager);
  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));

  hyscan_gtk_model_manager_toggled_items_get_bit_masks (priv->model_manager, &labels, &inconsistents);

  priv->set_labels_dialog = hyscan_gtk_mark_manager_set_labels_dialog_new (parent,
                                                                           label_model,
                                                                           labels,
                                                                           inconsistents);
  g_signal_connect (priv->set_labels_dialog, "destroy",
                    G_CALLBACK (hyscan_gtk_mark_manager_release_set_labels_dialog), self);
  g_signal_connect_swapped (priv->set_labels_dialog, "set-labels",
                            G_CALLBACK (hyscan_gtk_mark_manager_toggled_items_set_labels), self);

  g_object_unref (label_model);
}

/* Функция-обработчик сигнала закрытия диалога установки групп объектам. */
static void
hyscan_gtk_mark_manager_release_set_labels_dialog (GtkWidget *dialog,
                                                   gpointer   user_data)
{
  HyScanGtkMarkManager *self;
  HyScanGtkMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER (user_data));

  self = HYSCAN_GTK_MARK_MANAGER (user_data);
  priv = self->priv;

  priv->set_labels_dialog = NULL;
}

/* Функция-обработчик сигнала установки групп.
 * Устанавливает группы у выбранных объектов по полученным
 * из #HyScanGtkMarkManagerSetLabelsDialog битовым маскам.
 * */
static void
hyscan_gtk_mark_manager_toggled_items_set_labels (HyScanGtkMarkManager *self,
                                                  gint64                labels)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  gint64 inconsistents =
         hyscan_gtk_mark_manager_set_labels_dialog_get_inconsistents (priv->set_labels_dialog);

  hyscan_gtk_model_manager_toggled_items_set_labels (priv->model_manager, labels, inconsistents);
}

/* Функция-обработчик выделения объектов MarkManagerView. */
static void
hyscan_gtk_mark_manager_item_selected (HyScanGtkMarkManager *self,
                                       gchar                *id)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_model_manager_set_selected_item (priv->model_manager, id);
}

/* Функция-обработчик выделения cтроки. Сигнал отправляет ModelManager. */
static void
hyscan_gtk_mark_manager_select_item (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  gchar *id = hyscan_gtk_model_manager_get_selected_item (priv->model_manager);

  if (id != NULL)
    hyscan_gtk_mark_manager_view_select_item (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), id);
}

/* Функция-обработчик сигнала о снятии выделения.
 * Сигнал отправляет MarkManagerView. */
static void
hyscan_gtk_mark_manager_unselect (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_model_manager_unselect_all (priv->model_manager);
}

/* Функция-обработчик сигнала о снятии выделения.
 * Приходит из Model Manager-а. */
static void
hyscan_gtk_mark_manager_unselect_all (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_mark_manager_view_unselect_all (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view));
}

/* Функция-обработчик сигнала об изменении данных в модели групп.
 * Приходит из Model Manager-а. */
static void
hyscan_gtk_mark_manager_labels_changed (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  HyScanObjectModel *label_model;
  GHashTable *table;
  guint size;
  /* Если групп более MAX_LABELS, делаем кнопку "Новая группа" неактивной. */
  label_model = hyscan_gtk_model_manager_get_label_model (priv->model_manager);
  table = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (label_model), HYSCAN_TYPE_LABEL);
  size = g_hash_table_size (table);
  gtk_widget_set_sensitive (GTK_WIDGET (priv->new_label_item), size < MAX_LABELS);
  g_hash_table_destroy (table);
  g_object_unref (label_model);
}

/* Функция-обработчик изменения состояния чек-боксов в MarkManagerView. */
static void
hyscan_gtk_mark_manager_item_toggled (HyScanGtkMarkManager *self,
                                      gchar                *id,
                                      gboolean              active)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_model_manager_toggle_item (priv->model_manager, id, active);
}

/* Функция-обработчик сигнала об изменении состояния чек-бокса.
 * Приходит из Model Manager-а. */
static void
hyscan_gtk_mark_manager_toggle_item (HyScanGtkMarkManager *self,
                                     gchar                *id,
                                     gboolean              active)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  gboolean has_toggled = hyscan_gtk_model_manager_has_toggled (priv->model_manager);

  hyscan_gtk_mark_manager_view_toggle_item (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), id, active);

  gtk_widget_set_sensitive (priv->delete_icon,       has_toggled);
  gtk_widget_set_sensitive (priv->untoggle_all_item, has_toggled);
  gtk_widget_set_sensitive (priv->set_labels,        has_toggled);
  gtk_widget_set_sensitive (priv->save_as_html,      has_toggled);
}

/* Функция-обработчик сигнал горизонтальной полосы прокрутки представления.
 * Сигнал приходит из #HyScanGtkMarkManager-а. */
static void
hyscan_gtk_mark_manager_view_scrolled_horizontal (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  GtkAdjustment *adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->view));
  gdouble value = hyscan_gtk_model_manager_get_horizontal_adjustment (priv->model_manager);

  gtk_adjustment_set_value (adjustment, value);
}

/* Функция-обработчик сигнал вертикальной полосы прокрутки представления.
 * Сигнал приходит из #HyScanGtkMarkManager-а.*/
static void
hyscan_gtk_mark_manager_view_scrolled_vertical (HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;;
  GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->view));
  gdouble value = hyscan_gtk_model_manager_get_vertical_adjustment (priv->model_manager);

  gtk_adjustment_set_value (adjustment, value);
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanGtkModelManagerView. */
static void
hyscan_gtk_mark_manager_item_expanded (HyScanGtkMarkManager *self,
                                       gchar                *id,
                                       gboolean              expanded)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;

  hyscan_gtk_model_manager_expand_item (priv->model_manager, id, expanded);
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanGtkModelManager-а.*/
static void
hyscan_gtk_mark_manager_expand_item (HyScanGtkMarkManager *self)
{
  hyscan_gtk_mark_manager_expand_current_item (self, TRUE);
}

/* Функция-обработчик сигнала о cворачивании узла древовидного представления.
 * Сигнал приходит из #HyScanGtkModelManager-а.*/
static void
hyscan_gtk_mark_manager_collapse_item (HyScanGtkMarkManager *self)
{
  hyscan_gtk_mark_manager_expand_current_item (self, FALSE);
}

/* Разворачивает или сворачивает узел древовидного представления. */
static void
hyscan_gtk_mark_manager_expand_current_item (HyScanGtkMarkManager *self,
                                             gboolean              expanded)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  GtkTreeModel *model = hyscan_gtk_model_manager_get_view_model (priv->model_manager);
  GtkTreeIter  *array = NULL, iter;
  guint         index = 0;
  gchar        *id;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    {
      g_object_unref (model);
      return;
    }

  array = (GtkTreeIter*)g_realloc (NULL, sizeof (GtkTreeIter));
  id    = hyscan_gtk_model_manager_get_current_id (priv->model_manager);

  do
    array = hyscan_gtk_mark_manager_view_find_items_by_id (model, &iter, array, &index, id);
  while (gtk_tree_model_iter_next (model, &iter));

  while (index != 0)
    {
      GtkTreePath *path = NULL;;

      index--;

      path = gtk_tree_model_get_path (model, &array[index]);

      hyscan_gtk_mark_manager_view_expand_path (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), path, expanded);

      gtk_tree_path_free (path);
    }
  g_object_unref (model);
  g_free (array);
}

/* Обновляет состояние всех узлов древовидного представления.
 * Разворачивает те узлы, которые нужно развернуть и
 * сворачивает те узлы, которые нужно свернуть.
 * */
static void
hyscan_gtk_mark_manager_expand_all_items (HyScanGtkMarkManager *self)
{
  hyscan_gtk_mark_manager_expand_items (self, FALSE);
  hyscan_gtk_mark_manager_expand_items (self, TRUE);
}

/* Разворачивает или сворачивает узлы древовидного
 * представления в зависимости от заданного
 * аргумента expanded:
 * TRUE  - разворачивает, те что нужно развернуть;
 * FALSE - сворачивает, те что нужно свернуть.
 * */
static void
hyscan_gtk_mark_manager_expand_items (HyScanGtkMarkManager *self,
                                      gboolean              expanded)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  GtkTreeModel *model = hyscan_gtk_model_manager_get_view_model (priv->model_manager);

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      gchar **list = hyscan_gtk_model_manager_get_expanded_items (priv->model_manager, type, expanded);

      if (list == NULL)
        continue;

      for (gint i = 0; list[i] != NULL; i++)
        {
          GtkTreeIter *array = (GtkTreeIter*)g_realloc (NULL, sizeof (GtkTreeIter)), iter;
          guint        index = 0;

          if (!gtk_tree_model_get_iter_first (model, &iter))
            {
              g_free (array);
              continue;
            }

          do
            array = hyscan_gtk_mark_manager_view_find_items_by_id (model, &iter, array, &index, list[i]);
          while (gtk_tree_model_iter_next (model, &iter));

          while (index != 0)
            {
              GtkTreePath *path = NULL;

              index--;

              path = gtk_tree_model_get_path (model, &array[index]);

              hyscan_gtk_mark_manager_view_expand_path (HYSCAN_GTK_MARK_MANAGER_VIEW (priv->view), path, expanded);

              gtk_tree_path_free (path);
            }
          g_free (array);
        }
      g_strfreev (list);
    }
  g_object_unref (model);
}

/* Обработчик выбора пункта меню "Сохранить как HTML". */
static void
hyscan_gtk_mark_manager_toggled_items_save_as_html (GtkMenuItem          *item,
                                                    HyScanGtkMarkManager *self)
{
  HyScanGtkMarkManagerPrivate *priv = self->priv;
  GtkWindow *toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));

  hyscan_gtk_mark_export_save_as_html (priv->model_manager, toplevel, TRUE);
}

/**
 * hyscan_gtk_mark_manager_new:
 * @model_manager: Указатель на Менеджер Моделей
 *
 * Returns: cоздаёт новый объект #HyScanMarkManager
 */
GtkWidget*
hyscan_gtk_mark_manager_new (HyScanGtkModelManager *model_manager)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_GTK_MARK_MANAGER,
                                   "model_manager",  model_manager,
                                   NULL));
}
