/* hyscan-gtk-planner-zeditor.c
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
 * SECTION: hyscan-gtk-planner-zeditor
 * @Short_description: Виджет редактора вершин зоны полигона
 * @Title: HyScanGtkPlannerZeditor
 *
 * Виджет HyScanGtkPlannerZeditor выводит список вершин выбранной зоны полигона и
 * позволяет их редактировать.
 *
 * Пользователь может выбрать, в каком виде выводить координаты:
 *
 * - в виде географических координат (широта, долгота)
 * - в виде топоцентрических координат (X, Y)
 *
 * Для установки режима просмотра используется функция hyscan_gtk_planner_zeditor_set_geodetic().
 *
 * При измении значений в полях редактирования параметры пересчитываются в географические координаты и сохраняются
 * в базу данных.
 *
 */

#include <glib/gi18n-lib.h>
#include "hyscan-gtk-planner-zeditor.h"

#define DATA_KEY_COLUMN "column"   /* Ключ для хранения в данных GObject-а номера столбца. */
#define KEY_DUPLICATE GDK_KEY_d    /* Клавиша для дублирования вершины. */

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
  PROP_GEODETIC,
};

enum
{
  NUMBER_COLUMN,
  LATITUDE_COLUMN,
  LONGITUDE_COLUMN,
  X_COLUMN,
  Y_COLUMN,
  N_COLUMNS
};

struct _HyScanGtkPlannerZeditorPrivate
{
  HyScanPlannerModel        *model;            /* Модель данных планировщика. */
  HyScanPlannerSelection    *selection;        /* Модель выбранных объектов. */
  HyScanGeo                 *geo;              /* Объект для перевода географических координат. */
  gboolean                   geodetic;         /* Отображать географические координаты. */
  GtkTreeStore              *store;            /* Хранилище строк GtkTreeView. */
  GHashTable                *objects;          /* Объекты планировщика. */
  gchar                     *zone_id;          /* Идентификатор редактируемой зоны. */
  gint                       vertex;           /* Номер редактируемой вершины или -1. */

  GtkTreeViewColumn         *column_x;         /* Столбец координаты X. */
  GtkTreeViewColumn         *column_y;         /* Столбец координаты Y. */
  GtkTreeViewColumn         *column_lat;       /* Столбец координаты широты. */
  GtkTreeViewColumn         *column_lon;       /* Столбец координаты долготы. */

  GtkWidget                 *context_menu;     /* Контекстное меню строки. */
  GtkWidget                 *menu_duplicate;   /* Пункт меню "Вставить до". */
  GtkWidget                 *menu_delete;      /* Пункт меню "Вставить после". */

  guint                      keycode_dup;      /* Код клавиши KEY_DUPLICATE. */
  gulong                     shandler_zone;    /* Обработчик сигнала "zone-changed". */
  gulong                     shandler_row;     /* Обработчик сигнала "changed" GtkTreeSelection. */
};

static void                 hyscan_gtk_planner_zeditor_set_property        (GObject                 *object,
                                                                            guint                    prop_id,
                                                                            const GValue            *value,
                                                                            GParamSpec              *pspec);
static void                 hyscan_gtk_planner_zeditor_get_property        (GObject                 *object,
                                                                            guint                    prop_id,
                                                                            GValue                  *value,
                                                                            GParamSpec              *pspec);
static void                 hyscan_gtk_planner_zeditor_object_constructed  (GObject                 *object);
static void                 hyscan_gtk_planner_zeditor_object_finalize     (GObject                 *object);
static GtkTreeViewColumn *  hyscan_gtk_planner_zeditor_add_renderer_double (HyScanGtkPlannerZeditor *zeditor,
                                                                            const gchar             *title,
                                                                            guint                    column);
static void                 hyscan_gtk_planner_zeditor_cell_data_func      (GtkTreeViewColumn       *tree_column,
                                                                            GtkCellRenderer         *cell,
                                                                            GtkTreeModel            *tree_model,
                                                                            GtkTreeIter             *iter,
                                                                            gpointer                 data);
static void                 hyscan_gtk_planner_zeditor_edited              (GtkCellRendererText     *cell,
                                                                            gchar                   *path_string,
                                                                            gchar                   *new_text,
                                                                            gpointer                 user_data);
static void                 hyscan_gtk_planner_zeditor_zone_changed        (HyScanGtkPlannerZeditor *zeditor);
static void                 hyscan_gtk_planner_zeditor_model_changed       (HyScanGtkPlannerZeditor *zeditor);
static void                 hyscan_gtk_planner_zeditor_row_changed         (HyScanGtkPlannerZeditor *zeditor);
static void                 hyscan_gtk_planner_zeditor_set_zone            (HyScanGtkPlannerZeditor *zeditor);
static gboolean             hyscan_gtk_planner_zeditor_key_press           (GtkWidget               *widget,
                                                                            GdkEventKey             *event);
static gboolean             hyscan_gtk_planner_zeditor_button_press        (GtkWidget               *widget,
                                                                            GdkEventButton          *event);
static void                 hyscan_gtk_planner_zeditor_realize             (GtkWidget               *widget);
static void                 hyscan_gtk_planner_zeditor_menu_activate       (GtkButton               *button,
                                                                            gpointer                 user_data);
static GtkWidget *          hyscan_gtk_planner_zeditor_menu_add            (HyScanGtkPlannerZeditor *zeditor,
                                                                            const gchar             *title);
static void                 hyscan_gtk_planner_zeditor_vertex_duplicate    (HyScanGtkPlannerZeditor *zeditor);
static void                 hyscan_gtk_planner_zeditor_vertex_delete       (HyScanGtkPlannerZeditor *zeditor);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerZeditor, hyscan_gtk_planner_zeditor, GTK_TYPE_TREE_VIEW);

static void
hyscan_gtk_planner_zeditor_class_init (HyScanGtkPlannerZeditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_zeditor_set_property;
  object_class->get_property = hyscan_gtk_planner_zeditor_get_property;
  object_class->constructed = hyscan_gtk_planner_zeditor_object_constructed;
  object_class->finalize = hyscan_gtk_planner_zeditor_object_finalize;

  widget_class->key_press_event = hyscan_gtk_planner_zeditor_key_press;
  widget_class->button_press_event = hyscan_gtk_planner_zeditor_button_press;
  widget_class->realize = hyscan_gtk_planner_zeditor_realize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("planner-model", "Planner Model", "Planner model with zones information",
                         HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanListStore", "Selection containing selected zones",
                         HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_GEODETIC,
    g_param_spec_boolean ("geodetic", "Use geodetic coordinates", "Whether use geodetic coordinates or topocentric",
                          FALSE,
                          G_PARAM_READWRITE));
}

static void
hyscan_gtk_planner_zeditor_init (HyScanGtkPlannerZeditor *gtk_planner_zeditor)
{
  gtk_planner_zeditor->priv = hyscan_gtk_planner_zeditor_get_instance_private (gtk_planner_zeditor);
}

static void
hyscan_gtk_planner_zeditor_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (object);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    case PROP_GEODETIC:
      hyscan_gtk_planner_zeditor_set_geodetic (zeditor, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_zeditor_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  HyScanGtkPlannerZeditor *gtk_planner_zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (object);
  HyScanGtkPlannerZeditorPrivate *priv = gtk_planner_zeditor->priv;

  switch (prop_id)
    {
    case PROP_GEODETIC:
      g_value_set_boolean (value, priv->geodetic);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_zeditor_object_constructed (GObject *object)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (object);
  GtkTreeView *tree_view = GTK_TREE_VIEW (zeditor);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *num_col;

  G_OBJECT_CLASS (hyscan_gtk_planner_zeditor_parent_class)->constructed (object);

  priv->store = gtk_tree_store_new (N_COLUMNS,
                                    G_TYPE_UINT,        /* NUMBER_COLUMN.    Порядковый номер вершины. */
                                    G_TYPE_DOUBLE,      /* LATITUDE_COLUMN.  Широта. */
                                    G_TYPE_DOUBLE,      /* LONGITUDE_COLUMN. Долгота. */
                                    G_TYPE_DOUBLE,      /* X_COLUMN.         Координата по оси X. */
                                    G_TYPE_DOUBLE);     /* Y_COLUMN.         Координата по оси Y. */

  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (priv->store));

  renderer = gtk_cell_renderer_text_new ();
  num_col = gtk_tree_view_column_new_with_attributes (_("N"), renderer,
                                                      "text", NUMBER_COLUMN,
                                                      NULL);

  priv->column_lat = hyscan_gtk_planner_zeditor_add_renderer_double (zeditor, _("Latitude"), LATITUDE_COLUMN);
  priv->column_lon = hyscan_gtk_planner_zeditor_add_renderer_double (zeditor, _("Longitude"), LONGITUDE_COLUMN);
  priv->column_x = hyscan_gtk_planner_zeditor_add_renderer_double (zeditor, _("X"),   X_COLUMN);
  priv->column_y = hyscan_gtk_planner_zeditor_add_renderer_double (zeditor, _("Y"),   Y_COLUMN);

  gtk_tree_view_append_column (tree_view, num_col);
  gtk_tree_view_append_column (tree_view, priv->column_lat);
  gtk_tree_view_append_column (tree_view, priv->column_lon);
  gtk_tree_view_append_column (tree_view, priv->column_x);
  gtk_tree_view_append_column (tree_view, priv->column_y);

  priv->context_menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (priv->context_menu), GTK_WIDGET (zeditor), NULL);
  priv->menu_duplicate = hyscan_gtk_planner_zeditor_menu_add (zeditor, _("Duplicate"));
  priv->menu_delete = hyscan_gtk_planner_zeditor_menu_add (zeditor, _("Delete"));

  priv->shandler_zone = g_signal_connect_swapped (priv->selection, "zone-changed",
                                                  G_CALLBACK (hyscan_gtk_planner_zeditor_zone_changed), zeditor);
  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_planner_zeditor_model_changed), zeditor);
  priv->shandler_row = g_signal_connect_swapped (gtk_tree_view_get_selection (tree_view), "changed",
                                                 G_CALLBACK (hyscan_gtk_planner_zeditor_row_changed), zeditor);

  /* Загружаем актуальный список объектов. */
  hyscan_gtk_planner_zeditor_model_changed (zeditor);
  hyscan_gtk_planner_zeditor_set_geodetic (zeditor, priv->geodetic);
}

static void
hyscan_gtk_planner_zeditor_object_finalize (GObject *object)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (object);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  g_signal_handlers_disconnect_by_data (priv->model, zeditor);
  g_signal_handlers_disconnect_by_data (priv->selection, zeditor);

  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_clear_object (&priv->store);
  g_clear_object (&priv->geo);
  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_free (priv->zone_id);

  G_OBJECT_CLASS (hyscan_gtk_planner_zeditor_parent_class)->finalize (object);
}

static gboolean
hyscan_gtk_planner_zeditor_key_press (GtkWidget   *widget,
                                      GdkEventKey *event)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (widget);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  /* Обрабатываем только нажатие Delete. */
  if (event->keyval == GDK_KEY_Delete)
    {
      hyscan_gtk_planner_zeditor_vertex_delete (zeditor);

      return GDK_EVENT_STOP;
    }

  else if (event->hardware_keycode == priv->keycode_dup && event->state & GDK_CONTROL_MASK)
    {
      hyscan_gtk_planner_zeditor_vertex_duplicate (zeditor);

      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (hyscan_gtk_planner_zeditor_parent_class)->key_press_event (widget, event);
}

static void
hyscan_gtk_planner_zeditor_realize (GtkWidget *widget)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (widget);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  GdkDisplay *display;
  GdkKeymap *keymap;

  GdkKeymapKey *keys;
  gint n_keys;

  GTK_WIDGET_CLASS (hyscan_gtk_planner_zeditor_parent_class)->realize (widget);

  display = gtk_widget_get_display (widget);
  keymap = gdk_keymap_get_for_display (display);

  gdk_keymap_get_entries_for_keyval (keymap, KEY_DUPLICATE, &keys, &n_keys);
  if (n_keys > 0)
    priv->keycode_dup = keys[0].keycode;

  g_free (keys);
}

/* Обработчи события "button-press-event". */
static gboolean
hyscan_gtk_planner_zeditor_button_press (GtkWidget      *widget,
                                         GdkEventButton *event)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (widget);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  if (event->button == GDK_BUTTON_SECONDARY)
    gtk_menu_popup_at_pointer (GTK_MENU (priv->context_menu), (const GdkEvent *) event);

  return GTK_WIDGET_CLASS (hyscan_gtk_planner_zeditor_parent_class)->button_press_event (widget, event);
}

/* Добавляет пункт в контекстное меню. */
static GtkWidget *
hyscan_gtk_planner_zeditor_menu_add (HyScanGtkPlannerZeditor *zeditor,
                                     const gchar             *title)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  GtkWidget *menu_item;

  menu_item = gtk_menu_item_new_with_label (title);
  g_signal_connect (menu_item, "activate", G_CALLBACK (hyscan_gtk_planner_zeditor_menu_activate), zeditor);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->context_menu), menu_item);

  return menu_item;
}

/* Дублирует вершину с индексом index. */
static void
hyscan_gtk_planner_zeditor_vertex_duplicate (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  HyScanPlannerZone *zone;

  if (priv->vertex < 0 || priv->zone_id == NULL)
    return;

  zone = g_hash_table_lookup (priv->objects, priv->zone_id);
  g_return_if_fail (HYSCAN_IS_PLANNER_ZONE (zone));

  hyscan_planner_zone_vertex_dup (zone, priv->vertex);
  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->model), priv->zone_id, (const HyScanObject *) zone);
}

/* Удаляет текущую вершину. */
static void
hyscan_gtk_planner_zeditor_vertex_delete (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  HyScanPlannerZone *zone;

  if (priv->vertex < 0 || priv->zone_id == NULL)
    return;

  zone = g_hash_table_lookup (priv->objects, priv->zone_id);
  g_return_if_fail (HYSCAN_IS_PLANNER_ZONE (zone));

  if (zone->points_len > 3)
    {
      hyscan_planner_zone_vertex_remove (zone, priv->vertex);
      hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->model), priv->zone_id, (const HyScanObject *) zone);
      hyscan_planner_selection_set_zone (priv->selection, priv->zone_id, MAX (0, priv->vertex - 1));
    }
  else
    {
      hyscan_object_store_remove (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_ZONE, priv->zone_id);
      g_clear_pointer (&priv->zone_id, g_free);
    }
}

/* Обработчик выбора пункта контекстного меню. */
static void
hyscan_gtk_planner_zeditor_menu_activate (GtkButton *button,
                                          gpointer   user_data)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (user_data);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  if (GTK_WIDGET (button) == priv->menu_duplicate)
    hyscan_gtk_planner_zeditor_vertex_duplicate (zeditor);

  else if (GTK_WIDGET (button) == priv->menu_delete)
    hyscan_gtk_planner_zeditor_vertex_delete (zeditor);
}

/* Обработчик сигнала "zone-changed". Устанавливает выбранную зону в виджет. */
static void
hyscan_gtk_planner_zeditor_zone_changed (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  g_clear_pointer (&priv->zone_id, g_free);
  priv->zone_id = hyscan_planner_selection_get_zone (priv->selection, &priv->vertex);
  hyscan_gtk_planner_zeditor_set_zone (zeditor);
}

/* Обработчик сигнала HyScanObjectModel "changed". Обновляет данные объектов планировщика. */
static void
hyscan_gtk_planner_zeditor_model_changed (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_clear_object (&priv->geo);
  priv->objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_ZONE);
  priv->geo = hyscan_planner_model_get_geo (priv->model);

  hyscan_gtk_planner_zeditor_set_zone (zeditor);
}

/* Обработчик сигнала GtkTreeSelection "changed". Обновляет номер выбранной вершины. */
static void
hyscan_gtk_planner_zeditor_row_changed (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  GtkTreeSelection *tree_selection;
  GtkTreeIter iter;

  tree_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (zeditor));

  if (gtk_tree_selection_get_selected (tree_selection, NULL, &iter))
    {
      guint number;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, NUMBER_COLUMN, &number, -1);
      priv->vertex = number - 1;
    }
  else
    {
      priv->vertex = -1;
    }

  g_signal_handler_block (priv->selection, priv->shandler_zone);
  hyscan_planner_selection_set_zone (priv->selection, priv->zone_id, priv->vertex);
  g_signal_handler_unblock (priv->selection, priv->shandler_zone);
}

/* Загружает информацию из текущей зоны. */
static void
hyscan_gtk_planner_zeditor_set_zone (HyScanGtkPlannerZeditor *zeditor)
{
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  GtkTreeIter iter;
  gsize i;
  HyScanPlannerZone *zone;
  GtkTreeViewColumn *focus_column = NULL;
  GtkTreePath *path = NULL;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (zeditor));

  g_signal_handler_block (selection, priv->shandler_row);
  gtk_tree_view_get_cursor (GTK_TREE_VIEW (zeditor), NULL, &focus_column);

  /* Убираем предыдущие значения. */
  gtk_tree_store_clear (priv->store);

  if (priv->zone_id == NULL)
    goto exit;

  zone = g_hash_table_lookup (priv->objects, priv->zone_id);
  if (zone == NULL)
    {
      g_clear_pointer (&priv->zone_id, g_free);
      goto exit;
    }

  for (i = 0; i < zone->points_len; i++)
    {
      HyScanGeoPoint *vertex = &zone->points[i];
      HyScanGeoCartesian2D cartesian;
      guint number;

      if (priv->geo != NULL)
        hyscan_geo_geo2topoXY0 (priv->geo, &cartesian, *vertex);
      else
        cartesian.x = cartesian.y = 0.0;

      number = i + 1;

      gtk_tree_store_append (priv->store, &iter, NULL);
      gtk_tree_store_set (priv->store, &iter,
                          X_COLUMN, cartesian.x,
                          Y_COLUMN, cartesian.y,
                          LATITUDE_COLUMN, vertex->lat,
                          LONGITUDE_COLUMN, vertex->lon,
                          NUMBER_COLUMN, number,
                          -1);

      if (priv->vertex == (gint) i)
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
    }

  if (path != NULL)
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (zeditor), path, focus_column, FALSE);
      gtk_tree_path_free (path);
    }

exit:
  g_signal_handler_unblock (selection, priv->shandler_row);
}

/* Устанавливает свойства рендерера в зависимости от текущих данных. */
static void
hyscan_gtk_planner_zeditor_cell_data_func (GtkTreeViewColumn *tree_column,
                                           GtkCellRenderer   *cell,
                                           GtkTreeModel      *tree_model,
                                           GtkTreeIter       *iter,
                                           gpointer           data)
{
  gchar *text;
  gdouble value;
  guint column;

  column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), DATA_KEY_COLUMN));

  gtk_tree_model_get (tree_model, iter, column, &value, -1);

  if (column == X_COLUMN || column == Y_COLUMN)
    text = g_strdup_printf ("%.2f", value);
  else
    text = g_strdup_printf ("%.6f", value);

  g_object_set (G_OBJECT (cell), "text", text, NULL);
  g_free (text);
}

/* Рендерер редактируемых ячеек с типом данных G_TYPE_DOUBLE. */
static GtkTreeViewColumn *
hyscan_gtk_planner_zeditor_add_renderer_double (HyScanGtkPlannerZeditor *zeditor,
                                                const gchar             *title,
                                                guint                    column)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *view_column;

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  g_object_set_data (G_OBJECT (renderer), DATA_KEY_COLUMN, GUINT_TO_POINTER (column));

  view_column = gtk_tree_view_column_new_with_attributes (title, renderer, NULL);
  gtk_tree_view_column_set_expand (view_column, TRUE);
  gtk_tree_view_column_set_cell_data_func (view_column, renderer, hyscan_gtk_planner_zeditor_cell_data_func, NULL, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (hyscan_gtk_planner_zeditor_edited), zeditor);

  return view_column;
}

/* Обработчик сигнала "edited" отдельной ячейки. */
static void
hyscan_gtk_planner_zeditor_edited (GtkCellRendererText *cell,
                                   gchar               *path_string,
                                   gchar               *new_text,
                                   gpointer             user_data)
{
  HyScanGtkPlannerZeditor *zeditor = HYSCAN_GTK_PLANNER_ZEDITOR (user_data);
  HyScanGtkPlannerZeditorPrivate *priv = zeditor->priv;
  gdouble new_value;
  GtkTreeIter iter;
  guint column;
  HyScanPlannerZone *zone;
  HyScanGeoPoint *vertex;

  guint number;

  if (priv->objects == NULL)
    return;

  zone = g_hash_table_lookup (priv->objects, priv->zone_id);
  if (zone == NULL)
    return;

  gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (priv->store), &iter, path_string);

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, NUMBER_COLUMN, &number, -1);
  if (zone->points_len < number)
    return;

  vertex = &zone->points[number - 1];
  column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), DATA_KEY_COLUMN));
  new_value = g_strtod (new_text, NULL);

  if (column == LATITUDE_COLUMN)
    {
      vertex->lat = new_value;
    }
  else if (column == LONGITUDE_COLUMN)
    {
      vertex->lon = new_value;
    }
  else if (priv->geo != NULL && (column == X_COLUMN || column == Y_COLUMN))
    {
      HyScanGeoCartesian2D cartesian;

      hyscan_geo_geo2topoXY0 (priv->geo, &cartesian, *vertex);

      (column == X_COLUMN) ? (cartesian.x = new_value) : 0;
      (column == Y_COLUMN) ? (cartesian.y = new_value) : 0;

      hyscan_geo_topoXY2geo0 (priv->geo, vertex, cartesian);
    }

  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->model), priv->zone_id, (HyScanObject *) zone);
}

/**
 * hyscan_gtk_planner_zeditor_new:
 * @model: модель объектов планировщика #HyScanPlannerModel
 * @selection: хранилище выбранных объектов #HyScanListStore
 *
 * Returns: новый виджет #HyScanGtkPlannerZeditor
 */
GtkWidget *
hyscan_gtk_planner_zeditor_new (HyScanPlannerModel     *model,
                                HyScanPlannerSelection *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_ZEDITOR,
                       "planner-model", model,
                       "selection", selection,
                       NULL);
}

/**
 * hyscan_gtk_planner_zeditor_set_geodetic:
 * @zeditor: указатель на #HyScanGtkPlannerEditor
 * @geodetic: показывать координаты в виде геодезических или нет
 *
 * Устанавливает режим отображения координат вершин в виде геодезических (широта, долгота)
 * или топоцентрических (X, Y).
 */
void
hyscan_gtk_planner_zeditor_set_geodetic (HyScanGtkPlannerZeditor *zeditor,
                                         gboolean                 geodetic)
{
  HyScanGtkPlannerZeditorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_ZEDITOR (zeditor));
  priv = zeditor->priv;

  priv->geodetic = geodetic;

  gtk_tree_view_column_set_visible (priv->column_lat, priv->geodetic);
  gtk_tree_view_column_set_visible (priv->column_lon, priv->geodetic);
  gtk_tree_view_column_set_visible (priv->column_x, !priv->geodetic);
  gtk_tree_view_column_set_visible (priv->column_y, !priv->geodetic);
}
