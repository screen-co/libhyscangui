/* hyscan-gtk-planner-list-bar.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-planner-list-bar
 * @Short_description: Виджет панели управления для списка плановых галсов
 * @Title: HyScanGtkPlannerListBar
 *
 * Данный виджет выводит панель управления списком схемы галсов #HyScanGtkPlannerList.
 *
 * Виджет содержит:
 * - меню для выбора видимых столбцов,
 * - кнопки для раскрытия и сворачивание всех элементов списка.
 *
 */

#include "hyscan-gtk-planner-list-bar.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_O,
  PROP_LIST
};

struct _HyScanGtkPlannerListBarPrivate
{
  GtkWidget                  *list;
  GtkWidget                  *menu;
  GtkWidget                  *button;
};

static void          hyscan_gtk_planner_list_bar_set_property             (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           const GValue            *value,
                                                                           GParamSpec              *pspec);
static void          hyscan_gtk_planner_list_bar_object_constructed       (GObject                 *object);
static void          hyscan_gtk_planner_list_bar_object_finalize          (GObject                 *object);
static void          hyscan_gtk_planner_list_bar_clicked                  (HyScanGtkPlannerListBar *list_bar);
static void          hyscan_gtk_planner_list_bar_active                   (GtkCheckMenuItem        *menu_item,
                                                                           GParamSpec              *param_spec,
                                                                           HyScanGtkPlannerListBar *list_bar);
static GtkToolItem * hyscan_gtk_planner_list_bar_create_btn               (const gchar             *icon_name,
                                                                           const gchar             *title);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerListBar, hyscan_gtk_planner_list_bar, GTK_TYPE_TOOLBAR)

static void
hyscan_gtk_planner_list_bar_class_init (HyScanGtkPlannerListBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_list_bar_set_property;

  object_class->constructed = hyscan_gtk_planner_list_bar_object_constructed;
  object_class->finalize = hyscan_gtk_planner_list_bar_object_finalize;

  g_object_class_install_property (object_class, PROP_LIST,
    g_param_spec_object ("list", "List", "HyScanGtkPlannerList widget", HYSCAN_TYPE_GTK_PLANNER_LIST,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_list_bar_init (HyScanGtkPlannerListBar *gtk_planner_list_bar)
{
  gtk_planner_list_bar->priv = hyscan_gtk_planner_list_bar_get_instance_private (gtk_planner_list_bar);
}

static void
hyscan_gtk_planner_list_bar_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  HyScanGtkPlannerListBar *gtk_planner_list_bar = HYSCAN_GTK_PLANNER_LIST_BAR (object);
  HyScanGtkPlannerListBarPrivate *priv = gtk_planner_list_bar->priv;

  switch (prop_id)
    {
    case PROP_LIST:
      priv->list = g_value_dup_object(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_list_bar_object_constructed (GObject *object)
{
  HyScanGtkPlannerListBar *list_bar = HYSCAN_GTK_PLANNER_LIST_BAR (object);
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  HyScanGtkPlannerListBarPrivate *priv = list_bar->priv;
  GtkToolItem *expand_btn, *collapse_btn;

  G_OBJECT_CLASS (hyscan_gtk_planner_list_bar_parent_class)->constructed (object);

  priv->button = GTK_WIDGET (hyscan_gtk_planner_list_bar_create_btn ("open-menu-symbolic", _("Visible Columns")));

  expand_btn = hyscan_gtk_planner_list_bar_create_btn ("zoom-in-symbolic", _("Expand All"));
  collapse_btn = hyscan_gtk_planner_list_bar_create_btn ("zoom-out-symbolic", _("Collapse All"));

  gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (priv->button), -1);
  gtk_toolbar_insert (toolbar, expand_btn, -1);
  gtk_toolbar_insert (toolbar, collapse_btn, -1);

  gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);

  priv->menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (priv->menu), priv->button, NULL);

  g_signal_connect_swapped (priv->button, "clicked", G_CALLBACK (hyscan_gtk_planner_list_bar_clicked), list_bar);
  g_signal_connect_swapped (expand_btn, "clicked", G_CALLBACK (gtk_tree_view_expand_all), priv->list);
  g_signal_connect_swapped (collapse_btn, "clicked", G_CALLBACK (gtk_tree_view_collapse_all), priv->list);
}

static void
hyscan_gtk_planner_list_bar_object_finalize (GObject *object)
{
  HyScanGtkPlannerListBar *gtk_planner_list_bar = HYSCAN_GTK_PLANNER_LIST_BAR (object);
  HyScanGtkPlannerListBarPrivate *priv = gtk_planner_list_bar->priv;

  g_object_unref (priv->list);

  G_OBJECT_CLASS (hyscan_gtk_planner_list_bar_parent_class)->finalize (object);
}

static GtkToolItem *
hyscan_gtk_planner_list_bar_create_btn (const gchar *icon_name,
                                        const gchar *title)
{
  GtkWidget *icon;
  icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
  GtkToolItem *btn;

  btn = gtk_tool_button_new (icon, title);
  gtk_widget_set_tooltip_text (GTK_WIDGET (btn), title);

  return btn;
}

static void
hyscan_gtk_planner_list_bar_active (GtkCheckMenuItem        *menu_item,
                                    GParamSpec              *param_spec,
                                    HyScanGtkPlannerListBar *list_bar)
{
  HyScanGtkPlannerListBarPrivate *priv = list_bar->priv;
  gint column_mask, item_col;

  item_col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "column"));

  column_mask = hyscan_gtk_planner_list_get_visible_cols (HYSCAN_GTK_PLANNER_LIST (priv->list));
  if (gtk_check_menu_item_get_active (menu_item))
    column_mask |= item_col;
  else
    column_mask &= ~(item_col);

  hyscan_gtk_planner_list_set_visible_cols (HYSCAN_GTK_PLANNER_LIST (priv->list), column_mask);
}

static void
hyscan_gtk_planner_list_bar_clicked (HyScanGtkPlannerListBar *list_bar)
{
  HyScanGtkPlannerListBarPrivate *priv = list_bar->priv;
  guint i;
  gint column_mask;
  GList *children, *link;

  static struct
  {
    const gchar             *title;
    HyScanGtkPlannerListCol  value;
  } columns[] = {
    { N_("Progress done"), HYSCAN_GTK_PLANNER_LIST_PROGRESS},
    { N_("Quality"), HYSCAN_GTK_PLANNER_LIST_QUALITY},
    { N_("Length"), HYSCAN_GTK_PLANNER_LIST_LENGTH},
    { N_("Time"), HYSCAN_GTK_PLANNER_LIST_TIME},
    { N_("Velocity"), HYSCAN_GTK_PLANNER_LIST_VELOCITY},
    { N_("Track"), HYSCAN_GTK_PLANNER_LIST_TRACK},
    { N_("Velocity Std. Dev."), HYSCAN_GTK_PLANNER_LIST_VELOCITY_SD},
    { N_("Track Std. Dev."), HYSCAN_GTK_PLANNER_LIST_TRACK_SD},
    { N_("Y Std. Dev"), HYSCAN_GTK_PLANNER_LIST_Y_SD},
  };

  /* Удаляем старые пункты. */
  children = gtk_container_get_children (GTK_CONTAINER (priv->menu));
  for (link = children; link != NULL; link = link->next)
    gtk_container_remove (GTK_CONTAINER (priv->menu), link->data);
  g_list_free (children);

  /* Добавляем новые пункты. */
  column_mask = hyscan_gtk_planner_list_get_visible_cols (HYSCAN_GTK_PLANNER_LIST (priv->list));
  for (i = 0; i < G_N_ELEMENTS (columns); i++)
    {
      GtkWidget *menu_item;

      menu_item = gtk_check_menu_item_new_with_label (_(columns[i].title));
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), columns[i].value & column_mask);
      gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), menu_item);
      gtk_widget_show_all (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "column", GINT_TO_POINTER (columns[i].value));
      g_signal_connect (menu_item, "notify::active", G_CALLBACK (hyscan_gtk_planner_list_bar_active), list_bar);
    }

  gtk_menu_popup_at_widget (GTK_MENU (priv->menu), priv->button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}

GtkWidget *
hyscan_gtk_planner_list_bar_new (HyScanGtkPlannerList *planner_list)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_LIST_BAR,
                       "list", planner_list,
                       NULL);
}
