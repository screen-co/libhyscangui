/* hyscan-gtk-map-mark-list.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-map-mark-list
 * @Short_description: виджет списка меток на карте
 * @Title: HyScanGtkMapMarkList
 *
 * Виджет показывает список акустических и географических меток на карте.
 *
 * Активация метки (двойной клик или Enter) переводит видимую область карты на метку.
 *
 */

#include "hyscan-gtk-map-mark-list.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_O,
  PROP_MODEL_WFMARK,
  PROP_MODEL_GEOMARK,
  PROP_LAYER_WFMARK,
  PROP_LAYER_GEOMARK,
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST,
};

/* Столбцы в GtkTreeView списка меток. */
enum
{
  MARK_ID_COLUMN,
  MARK_NAME_COLUMN,
  MARK_MTIME_COLUMN,
  MARK_MTIME_SORT_COLUMN,
  MARK_TYPE_COLUMN,
  MARK_TYPE_NAME_COLUMN,
  MARK_OPERATOR_COLUMN,
  MARK_DESCRIPTION_COLUMN,
  MARK_LAT_COLUMN,
  MARK_LON_COLUMN,
  MARK_N_COLUMNS,
};

struct _HyScanGtkMapMarkListPrivate
{
  gint                         prop_a;
  GtkListStore                *mark_store;
  HyScanMark                   selected_mark;
  HyScanGeoPoint               selected_location;
  gchar                       *selected_id;
  HyScanMarkLocModel          *model_wfmark;
  HyScanObjectModel           *model_geomark;
  HyScanGtkMapWfmark          *layer_wfmark;
  HyScanGtkMapGeomark         *layer_geomark;
};

static void         hyscan_gtk_map_mark_list_set_property        (GObject               *object,
                                                                  guint                  prop_id,
                                                                  const GValue          *value,
                                                                  GParamSpec            *pspec);
static void         hyscan_gtk_map_mark_list_object_constructed  (GObject               *object);
static void         hyscan_gtk_map_mark_list_object_finalize     (GObject               *object);
static void         hyscan_gtk_map_mark_list_activated           (HyScanGtkMapMarkList  *mark_list,
                                                                  GtkTreePath           *path,
                                                                  GtkTreeViewColumn     *col);
static void         hyscan_gtk_map_mark_list_changed             (HyScanGtkMapMarkList  *mark_list);
static void         hyscan_gtk_map_mark_list_selected            (GtkTreeSelection      *selection,
                                                                  HyScanGtkMapMarkList  *mark_list);
static void         hyscan_gtk_map_mark_list_append              (HyScanGtkMapMarkList  *mark_list,
                                                                  GtkTreeIter           *reuse_iter,
                                                                  const gchar           *mark_id,
                                                                  HyScanMark            *mark,
                                                                  HyScanGeoPoint         location);
static GHashTable * hyscan_gtk_map_mark_list_filter_iters        (HyScanGtkMapMarkList  *mark_list,
                                                                  GType                  filter_type,
                                                                  GHashTable            *marks);

static guint   hyscan_gtk_map_mark_list_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapMarkList, hyscan_gtk_map_mark_list, GTK_TYPE_TREE_VIEW)

static void
hyscan_gtk_map_mark_list_class_init (HyScanGtkMapMarkListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_mark_list_set_property;
  object_class->constructed = hyscan_gtk_map_mark_list_object_constructed;
  object_class->finalize = hyscan_gtk_map_mark_list_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL_GEOMARK,
    g_param_spec_object ("model-geomark", "HyScanObjectModel", "HyScanObjectModel of HyScanMarkGeo",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MODEL_WFMARK,
    g_param_spec_object ("model-wfmark", "HyScanMarkLocModel", "Waterfall marks location model",
                         HYSCAN_TYPE_MARK_LOC_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_LAYER_GEOMARK,
    g_param_spec_object ("layer-geomark", "HyScanGtkMapGeomark", "HyScanGtkMapGeomark layer",
                         HYSCAN_TYPE_GTK_MAP_GEOMARK,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_LAYER_WFMARK,
    g_param_spec_object ("layer-wfmark", "HyScanGtkMapWfmark", "HyScanGtkMapWfmark layer",
                         HYSCAN_TYPE_GTK_MAP_WFMARK,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanGtkMapMarkList::changed:
   * @mark_list: указатель на #HyScanGtkMapMarkList
   *
   * Сигнал посылается при изменении выбранной метки.
   */
  hyscan_gtk_map_mark_list_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_GTK_MAP_MARK_LIST, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_gtk_map_mark_list_init (HyScanGtkMapMarkList *gtk_map_mark_list)
{
  gtk_map_mark_list->priv = hyscan_gtk_map_mark_list_get_instance_private (gtk_map_mark_list);
}

static void
hyscan_gtk_map_mark_list_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkMapMarkList *gtk_map_mark_list = HYSCAN_GTK_MAP_MARK_LIST (object);
  HyScanGtkMapMarkListPrivate *priv = gtk_map_mark_list->priv;

  switch (prop_id)
    {
    case PROP_MODEL_GEOMARK:
      priv->model_geomark = g_value_dup_object (value);
      break;

    case PROP_MODEL_WFMARK:
      priv->model_wfmark = g_value_dup_object (value);
      break;

    case PROP_LAYER_GEOMARK:
      priv->layer_geomark = g_value_dup_object (value);
      break;

    case PROP_LAYER_WFMARK:
      priv->layer_wfmark = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_mark_list_object_constructed (GObject *object)
{
  HyScanGtkMapMarkList *mark_list = HYSCAN_GTK_MAP_MARK_LIST (object);
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;
  GtkTreeView *tree_view = GTK_TREE_VIEW (object);
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *type_column, *name_column, *date_column;
  GtkTreeSelection *tree_selection;

  G_OBJECT_CLASS (hyscan_gtk_map_mark_list_parent_class)->constructed (object);

  priv->mark_store = gtk_list_store_new (MARK_N_COLUMNS,
                                         G_TYPE_STRING,  /* MARK_ID_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_NAME_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_MTIME_COLUMN */
                                         G_TYPE_INT64,   /* MARK_MTIME_SORT_COLUMN */
                                         G_TYPE_GTYPE,   /* MARK_TYPE_COLUMN */
                                         G_TYPE_STRING,  /* MARK_TYPE_NAME_COLUMN */
                                         G_TYPE_STRING,  /* MARK_OPERATOR_COLUMN */
                                         G_TYPE_STRING,  /* MARK_DESCRIPTION_COLUMN */
                                         G_TYPE_DOUBLE,  /* MARK_LAT_COLUMN */
                                         G_TYPE_DOUBLE,  /* MARK_LON_COLUMN */
                                         -1);
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (priv->mark_store));

  /* Тип метки. */
  renderer = gtk_cell_renderer_text_new ();
  type_column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", MARK_TYPE_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (type_column, MARK_TYPE_COLUMN);

  /* Название метки. */
  renderer = gtk_cell_renderer_text_new ();
  name_column = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", MARK_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (name_column, MARK_NAME_COLUMN);
  gtk_tree_view_column_set_expand (name_column, TRUE);
  gtk_tree_view_column_set_expand (name_column, TRUE);
  gtk_tree_view_column_set_resizable (name_column, TRUE);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  /* Время последнего изменения метки. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer, "text", MARK_MTIME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, MARK_MTIME_SORT_COLUMN);

  /* Добавляем столбцы. */
  gtk_tree_view_set_search_column (tree_view, MARK_NAME_COLUMN);
  gtk_tree_view_append_column (tree_view, name_column);
  gtk_tree_view_append_column (tree_view, date_column);
  gtk_tree_view_append_column (tree_view, type_column);

  gtk_tree_view_set_grid_lines (tree_view, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

  tree_selection = gtk_tree_view_get_selection (tree_view);
  g_signal_connect (tree_selection, "changed", G_CALLBACK (hyscan_gtk_map_mark_list_selected), mark_list);
  g_signal_connect (mark_list, "row-activated", G_CALLBACK (hyscan_gtk_map_mark_list_activated), mark_list);
  g_signal_connect_swapped (priv->model_wfmark, "changed", G_CALLBACK (hyscan_gtk_map_mark_list_changed), mark_list);
  g_signal_connect_swapped (priv->model_geomark, "changed", G_CALLBACK (hyscan_gtk_map_mark_list_changed), mark_list);
}

static void
hyscan_gtk_map_mark_list_object_finalize (GObject *object)
{
  HyScanGtkMapMarkList *gtk_map_mark_list = HYSCAN_GTK_MAP_MARK_LIST (object);
  HyScanGtkMapMarkListPrivate *priv = gtk_map_mark_list->priv;

  g_object_unref (priv->mark_store);

  G_OBJECT_CLASS (hyscan_gtk_map_mark_list_parent_class)->finalize (object);
}

/* Добавляет метку в список меток. */
static void
hyscan_gtk_map_mark_list_append (HyScanGtkMapMarkList *mark_list,
                                 GtkTreeIter          *reuse_iter,
                                 const gchar          *mark_id,
                                 HyScanMark           *mark,
                                 HyScanGeoPoint        location)
{
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;

  GtkTreeIter tree_iter;

  GDateTime *local;
  gchar *time_str;
  gchar *type_name;

  if (mark->type == HYSCAN_TYPE_MARK_WATERFALL && ((HyScanMarkWaterfall *) mark)->track == NULL)
    return;

  /* Добавляем в список меток. */
  local = g_date_time_new_from_unix_local (mark->mtime / G_TIME_SPAN_SECOND);
  time_str = g_date_time_format (local, "%d.%m %H:%M");

  if (mark->type == HYSCAN_TYPE_MARK_WATERFALL)
    type_name = "W";
  else if (mark->type == HYSCAN_TYPE_MARK_GEO)
    type_name = "G";
  else
    type_name = "?";

  if (reuse_iter == NULL)
    {
      gtk_list_store_append (priv->mark_store, &tree_iter);
      reuse_iter = &tree_iter;
    }

  gtk_list_store_set (priv->mark_store, reuse_iter,
                      MARK_ID_COLUMN, mark_id,
                      MARK_NAME_COLUMN, mark->name,
                      MARK_MTIME_COLUMN, time_str,
                      MARK_MTIME_SORT_COLUMN, mark->mtime,
                      MARK_TYPE_COLUMN, mark->type,
                      MARK_TYPE_NAME_COLUMN, type_name,
                      MARK_LAT_COLUMN, location.lat,
                      MARK_LON_COLUMN, location.lon,
                      -1);

  g_free (time_str);
  g_date_time_unref (local);
}

/* Удаляет из таблицы меток строки пропавших меток и возвращает хэш таблицу остальных строк. */
static GHashTable *
hyscan_gtk_map_mark_list_filter_iters (HyScanGtkMapMarkList *mark_list,
                                       GType                 filter_type,
                                       GHashTable           *marks)
{
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;
  GHashTable *gtk_iters;
  GtkTreeIter gtk_iter;
  gboolean valid;

  /* Удаляем строки, которых больше нет. */
  gtk_iters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);
  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->mark_store), &gtk_iter);
  while (valid)
    {
      GType mark_type;
      gchar *mark_id;

      /* Обрабатываем только метки указанного типа. */
      gtk_tree_model_get (GTK_TREE_MODEL (priv->mark_store), &gtk_iter, MARK_TYPE_COLUMN, &mark_type, -1);
      if (mark_type != filter_type)
        {
          valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->mark_store), &gtk_iter);
          continue;
        }

      gtk_tree_model_get (GTK_TREE_MODEL (priv->mark_store), &gtk_iter, MARK_ID_COLUMN, &mark_id, -1);
      if (g_hash_table_contains (marks, mark_id))
        {
          g_hash_table_insert (gtk_iters, mark_id, gtk_tree_iter_copy (&gtk_iter));
          valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->mark_store), &gtk_iter);
        }
      else
        {
          g_free (mark_id);
          valid = gtk_list_store_remove (priv->mark_store, &gtk_iter);
          continue;
        }
    }

  return gtk_iters;
}

static void
hyscan_gtk_map_mark_list_changed (HyScanGtkMapMarkList *mark_list)
{
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;
  GHashTable *marks;
  GHashTableIter hash_iter;
  gchar *mark_id;

  /* Добавляем метки из всех моделей. */
  if (priv->model_wfmark != NULL && (marks = hyscan_mark_loc_model_get (priv->model_wfmark)) != NULL)
    {
      HyScanMarkLocation *location;
      GHashTable *gtk_iters;

      /* Фильтруем строки, которые уже есть. */
      gtk_iters = hyscan_gtk_map_mark_list_filter_iters (mark_list, HYSCAN_TYPE_MARK_WATERFALL, marks);

      g_hash_table_iter_init (&hash_iter, marks);
      while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &location))
        {
          GtkTreeIter *reuse_iter = g_hash_table_lookup (gtk_iters, mark_id);
          hyscan_gtk_map_mark_list_append (mark_list, reuse_iter, mark_id, (HyScanMark *) location->mark,
                                           location->mark_geo);
        }

      g_hash_table_unref (gtk_iters);
      g_hash_table_unref (marks);
    }

  if (priv->model_geomark != NULL && (marks = hyscan_object_model_get (priv->model_geomark)) != NULL)
    {
      HyScanMarkGeo *mark;
      GHashTable *gtk_iters;

      /* Фильтруем строки, которые уже есть. */
      gtk_iters = hyscan_gtk_map_mark_list_filter_iters (mark_list, HYSCAN_TYPE_MARK_GEO, marks);

      g_hash_table_iter_init (&hash_iter, marks);
      while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &mark))
        {
          GtkTreeIter *reuse_iter = g_hash_table_lookup (gtk_iters, mark_id);
          hyscan_gtk_map_mark_list_append (mark_list, reuse_iter, mark_id, (HyScanMark *) mark, mark->center);
        }

      g_hash_table_unref (gtk_iters);
      g_hash_table_unref (marks);
    }
}

static void
hyscan_gtk_map_mark_list_activated (HyScanGtkMapMarkList *mark_list,
                                    GtkTreePath          *path,
                                    GtkTreeViewColumn    *col)
{
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;
  GtkTreeModel *model = GTK_TREE_MODEL (priv->mark_store);
  GtkTreeIter iter;
  gchar *mark_id;
  GType mark_type;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, MARK_ID_COLUMN, &mark_id, MARK_TYPE_COLUMN, &mark_type, -1);

  if (mark_type == HYSCAN_TYPE_MARK_WATERFALL && priv->layer_wfmark != NULL)
    hyscan_gtk_map_wfmark_mark_view (HYSCAN_GTK_MAP_WFMARK (priv->layer_wfmark), mark_id, FALSE);

  else if (mark_type == HYSCAN_TYPE_MARK_GEO && priv->layer_geomark != NULL)
    hyscan_gtk_map_geomark_mark_view (HYSCAN_GTK_MAP_GEOMARK (priv->layer_geomark), mark_id, FALSE);

  g_free (mark_id);
}

/* Обработчик выделения строки в списке меток:
 * устанавливает выбранную метку в редактор меток. */
static void
hyscan_gtk_map_mark_list_selected (GtkTreeSelection     *selection,
                                   HyScanGtkMapMarkList *mark_list)
{
  HyScanGtkMapMarkListPrivate *priv = mark_list->priv;
  HyScanMark *mark = &priv->selected_mark;
  HyScanGeoPoint *location = &priv->selected_location;
  GtkTreeIter iter;

  g_clear_pointer (&mark->name, g_free);
  g_clear_pointer (&mark->description, g_free);
  g_clear_pointer (&mark->operator_name, g_free);

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    goto exit;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->mark_store), &iter,
                      MARK_ID_COLUMN, &priv->selected_id,
                      MARK_NAME_COLUMN, &mark->name,
                      MARK_OPERATOR_COLUMN, &mark->operator_name,
                      MARK_DESCRIPTION_COLUMN, &mark->description,
                      MARK_LAT_COLUMN, &location->lat,
                      MARK_LON_COLUMN, &location->lon,
                      -1);

  /* Выделяем метку на карте. */
  if (priv->layer_wfmark != NULL)
    hyscan_gtk_map_wfmark_mark_highlight (priv->layer_wfmark, priv->selected_id);

  if (priv->layer_geomark != NULL)
    hyscan_gtk_map_geomark_mark_highlight (priv->layer_geomark, priv->selected_id);

exit:
  g_signal_emit (mark_list, hyscan_gtk_map_mark_list_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_gtk_map_mark_list_new:
 * @model_geomark: (nullable): указатель модель геометок #HyScanObjectModel
 * @model_wfmark: (nullable): указатель на модель положения акустических меток #HyScanMarkLocModel
 * @layer_wfmark: (nullable): слой акустических меток карты
 * @layer_geomark: (nullable): слой географических меток карты
 *
 * Функция создаёт виджет списка меток.
 *
 * Returns: новый виджет списка меток #HyScanGtkMapMarkList.
 */
GtkWidget *
hyscan_gtk_map_mark_list_new (HyScanObjectModel   *model_geomark,
                              HyScanMarkLocModel  *model_wfmark,
                              HyScanGtkMapWfmark  *layer_wfmark,
                              HyScanGtkMapGeomark *layer_geomark)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_MARK_LIST,
                       "model-geomark", model_geomark,
                       "model-wfmark", model_wfmark,
                       "layer-wfmark", layer_wfmark,
                       "layer-geomark", layer_geomark,
                       NULL);
}

/**
 * hyscan_gtk_map_mark_list_get_selected:
 * @mark_list: указатель на #HyScanGtkMapMarkList
 * @mark: (out): метка
 * @location: (out): местоположение метки
 *
 * Функция возвращает информацию о выбранной в виджете метке.
 *
 * Returns: (transfer full): идентификатор выбранной метки или %NULL, для удаления g_free().
 */
gchar *
hyscan_gtk_map_mark_list_get_selected (HyScanGtkMapMarkList *mark_list,
                                       HyScanMark           *mark,
                                       HyScanGeoPoint       *location)
{
  HyScanGtkMapMarkListPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_MARK_LIST (mark_list), NULL);
  priv = mark_list->priv;

  if (mark != NULL)
    {
      mark->name = g_strdup (priv->selected_mark.name);
      mark->operator_name = g_strdup (priv->selected_mark.operator_name);
      mark->description = g_strdup (priv->selected_mark.description);
    }

  if (location != NULL)
    *location = priv->selected_location;

  return priv->selected_id;
}
