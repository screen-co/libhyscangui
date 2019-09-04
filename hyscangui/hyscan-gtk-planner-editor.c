/* hyscan-gtk-planner-editor.c
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
 * SECTION: hyscan-gtk-planner-editor
 * @Short_description: Виджет редактора параметров плановых галсов
 * @Title: HyScanGtkPlannerEditor
 *
 * Виджет HyScanGtkPlannerEditor содержит в себе поля для редактирования параметров галсов:
 * - координаты (x, y) точки начала,
 * - координаты (x, y) точки конца,
 * - длина галса,
 * - угол поворота.
 *
 * Виджет переводит географические координат галсов в декартовы (x, y) при заданных пользователем
 * координатах точки отсчёта и направлении оси OX.
 *
 * При измении значений в полях редактирования параметры галса пересчитываются в географические координаты и сохраняются
 * в базу данных.
 *
 * Если пользователь выбрал несколько галсов, то виджет позволяет отредактировать значения отдельных параметров
 * сразу у всех выбранных галсов.
 *
 */

#include <glib/gi18n-lib.h>
#include <math.h>
#include "hyscan-gtk-planner-editor.h"

#define MAX_DISTANCE 1e6   /* Максимальное значение координат X, Y концов галса, метры. */
#define DIST_STEP    1e-2  /* Шаг координат и длины галса, метры. */
#define ANGLE_STEP   1e-2  /* Шаг угла поворота, градусы. */

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

typedef struct
{
  guint    n_items;          /* Количество галсов, по которым получены значения. */

  gdouble  start_x;          /* Координата X начала галса. */
  gdouble  start_y;          /* Координата Y начала галса. */
  gdouble  end_x;            /* Координата X конца галса. */
  gdouble  end_y;            /* Координата Y конца галса. */
  gdouble  length;           /* Длина галса. */
  gdouble  angle;            /* Угол поворота галса. */

  gboolean start_x_diffs;    /* Признак того, что значение start_x варьируется. */
  gboolean start_y_diffs;    /* Признак того, что значение start_y варьируется. */
  gboolean end_x_diffs;      /* Признак того, что значение end_x варьируется. */
  gboolean end_y_diffs;      /* Признак того, что значение end_y варьируется. */
  gboolean length_diffs;     /* Признак того, что значение length варьируется. */
  gboolean angle_diffs;      /* Признак того, что значение angle варьируется. */
} HyScanGtkPlannerEditorValue;

typedef struct {
  HyScanGtkPlannerEditorPrivate *priv;        /* Указатель на приватную секцию класса. */
  guint                          i;           /* Текущий индекс выбранного элемента. */
  guint                          n_items;     /* Количество выбранных элементов. */
  const gchar                   *id;          /* Идентификатор текущего галса. */
  HyScanPlannerTrack            *track;       /* Указатель на текущий галс. */
} HyScanGtkPlannerEditorIter;

struct _HyScanGtkPlannerEditorPrivate
{
  HyScanObjectModel *model;                 /* Модель объектов планировщика. */
  GListModel        *selection;             /* Модель списка идентификаторов выбранных объектов. */
  HyScanGeo         *geo;                   /* Объект перевода координат из географических в декртовы. */
  gboolean           block_value_change;    /* Признак блокировки сигналов "value-changed" в полях ввода. */
  
  GHashTable        *objects;               /* Хэш таблица с объектами планировщика. */

  GtkWidget         *label;                 /* Виджет счётчика выбранных объектов. */
  GtkWidget         *start_x;               /* Поле ввода координаты X начала. */
  GtkWidget         *start_x_btn;           /* Галочка редактирования поля start_x. */
  GtkWidget         *start_y;               /* Поле ввода координаты Y начала. */
  GtkWidget         *start_y_btn;           /* Галочка редактирования поля start_y. */
  GtkWidget         *end_x;                 /* Поле ввода координаты X конца. */
  GtkWidget         *end_x_btn;             /* Галочка редактирования поля end_x. */
  GtkWidget         *end_y;                 /* Поле ввода координаты Y конца. */
  GtkWidget         *end_y_btn;             /* Галочка редактирования поля end_y. */
  GtkWidget         *length;                /* Поле ввода длины галса. */
  GtkWidget         *length_btn;            /* Галочка редактирования поля end_y. */
  GtkWidget         *angle;                 /* Поле ввода угла поворота галса. */
  GtkWidget         *angle_btn;             /* Галочка редактирования поля angle. */
};

static void        hyscan_gtk_planner_editor_set_property            (GObject                     *object,
                                                                      guint                        prop_id,
                                                                      const GValue                *value,
                                                                      GParamSpec                  *pspec);
static void        hyscan_gtk_planner_editor_object_constructed      (GObject                     *object);
static void        hyscan_gtk_planner_editor_object_finalize         (GObject                     *object);
static void        hyscan_gtk_planner_editor_iter_init               (HyScanGtkPlannerEditor      *editor,
                                                                      HyScanGtkPlannerEditorIter  *iter);
static gboolean    hyscan_gtk_planner_editor_iter_next               (HyScanGtkPlannerEditorIter  *iter);
static void        hyscan_gtk_planner_editor_selection_changed       (HyScanGtkPlannerEditor      *editor,
                                                                      guint                        position,
                                                                      guint                        removed,
                                                                      guint                        added);
static void        hyscan_gtk_planner_editor_get_value               (HyScanGtkPlannerEditor      *editor,
                                                                      HyScanGtkPlannerEditorValue *value);
static void        hyscan_gtk_planner_editor_convert_point           (HyScanGtkPlannerEditor      *editor,
                                                                      GtkWidget                   *widget_x,
                                                                      GtkWidget                   *widget_y,
                                                                      HyScanGeoGeodetic           *value);
static void        hyscan_gtk_planner_editor_model_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_end_changed             (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_start_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_length_changed          (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_angle_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_update_view             (HyScanGtkPlannerEditor      *editor);
static inline void hyscan_gtk_planner_editor_attach                  (GtkGrid                     *grid,
                                                                      const gchar                 *label,
                                                                      GtkWidget                   *entry,
                                                                      GtkWidget                   *checkbox,
                                                                      gint                         top,
                                                                      GCallback                    handler);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerEditor, hyscan_gtk_planner_editor, GTK_TYPE_GRID)

static void
hyscan_gtk_planner_editor_class_init (HyScanGtkPlannerEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_editor_set_property;
  object_class->constructed = hyscan_gtk_planner_editor_object_constructed;
  object_class->finalize = hyscan_gtk_planner_editor_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanObjectModel", "HyScanObjectModel with planner data",
                         HYSCAN_TYPE_OBJECT_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "GListModel", "GListModel with C-string ID of objects",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_editor_init (HyScanGtkPlannerEditor *editor)
{
  editor->priv = hyscan_gtk_planner_editor_get_instance_private (editor);
}

static void
hyscan_gtk_planner_editor_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      if (g_list_model_get_item_type (priv->selection) != G_TYPE_STRING)
        {
          g_warning ("HyScanGtkPlannerEditor: GListModel must contain items of type G_TYPE_STRING");
          g_clear_object (&priv->selection);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_editor_object_constructed (GObject *object)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  gsize i = 0;
  HyScanGeoGeodetic origin;

  GtkGrid *grid = GTK_GRID (object);

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->constructed (object);

  origin.lat = 55.5692;
  origin.lon = 38.0982;
  origin.h = 90;
  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  gtk_grid_set_row_spacing (grid, 3);
  gtk_grid_set_column_spacing (grid, 6);

  priv->label = gtk_label_new (NULL);
  priv->start_x = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->start_y = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->end_x = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->end_y = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->angle = gtk_spin_button_new_with_range (-360.0, 360.0, ANGLE_STEP);
  priv->length = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->start_x_btn = gtk_check_button_new ();
  priv->start_y_btn = gtk_check_button_new ();
  priv->end_x_btn = gtk_check_button_new ();
  priv->end_y_btn = gtk_check_button_new ();
  priv->length_btn = gtk_check_button_new ();
  priv->angle_btn = gtk_check_button_new ();

  gtk_grid_attach (grid, priv->label, 0, ++i, 2, 1);

  hyscan_gtk_planner_editor_attach (grid, _("Start X"), priv->start_x, priv->start_x_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_start_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Start Y"), priv->start_y, priv->start_y_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_start_changed));
  hyscan_gtk_planner_editor_attach (grid, _("End X"), priv->end_x, priv->end_x_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_end_changed));
  hyscan_gtk_planner_editor_attach (grid, _("End Y"), priv->end_y, priv->end_y_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_end_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Length"), priv->length, priv->length_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_length_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Angle"), priv->angle, priv->angle_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_angle_changed));

  g_signal_connect_swapped (priv->selection, "items-changed", G_CALLBACK (hyscan_gtk_planner_editor_selection_changed),
                            editor);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_editor_model_changed), editor);

  hyscan_gtk_planner_editor_update_view (editor);
}

static void
hyscan_gtk_planner_editor_object_finalize (GObject *object)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_signal_handlers_disconnect_by_data (priv->selection, editor);
  g_signal_handlers_disconnect_by_data (priv->model, editor);
  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->finalize (object);
}

static inline void
hyscan_gtk_planner_editor_attach (GtkGrid     *grid,
                                  const gchar *label,
                                  GtkWidget   *entry,
                                  GtkWidget   *checkbox,
                                  gint         top,
                                  GCallback    handler)
{
  gtk_grid_attach (grid, gtk_label_new (label), 0, top, 1, 1);
  gtk_grid_attach (grid, entry,                 1, top, 1, 1);
  gtk_grid_attach (grid, checkbox,              2, top, 1, 1);

  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (entry), FALSE);
  g_object_bind_property (checkbox, "active", entry, "sensitive", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (entry, "value-changed", handler, grid);
}

/* Определяет значения параметров выбранных галсов. */
static void
hyscan_gtk_planner_editor_get_value (HyScanGtkPlannerEditor      *editor,
                                     HyScanGtkPlannerEditorValue *value)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  guint i, n_items;

  memset (value, 0, sizeof (*value));

  if (priv->objects == NULL)
    return;

  n_items = g_list_model_get_n_items (priv->selection);
  for (i = 0; i < n_items; ++i)
    {
      HyScanPlannerObject *object;
      HyScanPlannerTrack *track;
      HyScanGeoCartesian2D start;
      HyScanGeoCartesian2D end;
      gdouble length, angle;
      gchar *id;

      id = g_list_model_get_item (priv->selection, i);
      object = g_hash_table_lookup (priv->objects, id);
      if (object == NULL || object->type != HYSCAN_PLANNER_TRACK)
        goto next;

      track = &object->track;

      if (!hyscan_geo_geo2topoXY (priv->geo, &start, track->start) ||
          !hyscan_geo_geo2topoXY (priv->geo, &end, track->end))
        {
          g_warning ("HyScanGtkPlannerEditor: failed to convert geo coordinates to (x, y)");
          goto next;
        }

      length = hypot (end.x - start.x, end.y - start.y);
      angle = atan2 (end.x - start.x, end.y - start.y) / G_PI * 180.0;
      if (angle < 0)
        angle += 360.0;

      if (value->n_items == 0)
        {
          value->start_x = start.x;
          value->start_y = start.y;
          value->end_x = end.x;
          value->end_y = end.y;
          value->angle = angle;
          value->length = length;
        }
      else
        {
          if (ABS (value->start_x - start.x) > DIST_STEP / 2.0)
            value->start_x_diffs = TRUE;

          if (ABS (value->start_y - start.y) > DIST_STEP / 2.0)
            value->start_y_diffs = TRUE;

          if (ABS (value->end_x - end.x) > DIST_STEP / 2.0)
            value->end_x_diffs = TRUE;

          if (ABS (value->end_y - end.y) > DIST_STEP / 2.0)
            value->end_y_diffs = TRUE;

          if (ABS (value->angle - angle) > ANGLE_STEP / 2.0)
            value->angle_diffs = TRUE;

          if (ABS (value->length - length) > DIST_STEP / 2.0)
            value->length_diffs = TRUE;
        }

      value->n_items++;

    next:
      g_free (id);
    }
}

/* Обновляет значения в полях ввода. */
static void
hyscan_gtk_planner_editor_update_view (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorValue value;
  gboolean value_set;

  gchar *label_text;

  hyscan_gtk_planner_editor_get_value (editor, &value);

  priv->block_value_change = TRUE;

  value_set = (value.n_items > 0);

  if (value_set)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_x), value.start_x);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_y), value.start_y);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_x), value.end_x);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_y), value.end_y);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->angle), value.angle);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->length), value.length);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (priv->start_x), "");
      gtk_entry_set_text (GTK_ENTRY (priv->start_y), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_x), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_y), "");
      gtk_entry_set_text (GTK_ENTRY (priv->angle), "");
      gtk_entry_set_text (GTK_ENTRY (priv->length), "");
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_x_btn), !value.start_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_y_btn), !value.start_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_x_btn), !value.end_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_y_btn), !value.end_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->angle_btn), !value.angle_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->length_btn), !value.length_diffs && value_set);

  priv->block_value_change = FALSE;

  label_text = g_strdup_printf (_("%d items selected"), value.n_items);
  gtk_label_set_text (GTK_LABEL (priv->label), label_text);

  g_free (label_text);
}

static void
hyscan_gtk_planner_editor_selection_changed (HyScanGtkPlannerEditor *editor,
                                             guint                   position,
                                             guint                   removed,
                                             guint                   added)
{
  hyscan_gtk_planner_editor_update_view (editor);
}

/* Устанавливает геокоординаты value из виджетов widget_x и widget_y.*/
static void
hyscan_gtk_planner_editor_convert_point (HyScanGtkPlannerEditor *editor,
                                         GtkWidget              *widget_x,
                                         GtkWidget              *widget_y,
                                         HyScanGeoGeodetic      *value)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGeoGeodetic geodetic;
  HyScanGeoCartesian2D cartesian;

  geodetic = *value;
  hyscan_geo_geo2topoXY (priv->geo, &cartesian, geodetic);

  if (gtk_widget_get_sensitive (widget_x))
    cartesian.x = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget_x));

  if (gtk_widget_get_sensitive (widget_y))
    cartesian.y = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget_y));

  hyscan_geo_topoXY2geo (priv->geo, value, cartesian, 0);
}

/* Инициализирует итератор выбранных галсов. */
static void
hyscan_gtk_planner_editor_iter_init (HyScanGtkPlannerEditor     *editor,
                                     HyScanGtkPlannerEditorIter *iter)
{
  iter->priv = editor->priv;
  iter->i = 0;
  iter->n_items = g_list_model_get_n_items (editor->priv->selection);
  iter->id = NULL;
  iter->track = NULL;
}

/* Возвращает следующий выбранный галс итератора. */
static gboolean
hyscan_gtk_planner_editor_iter_next (HyScanGtkPlannerEditorIter *iter)
{
  HyScanGtkPlannerEditorPrivate *priv = iter->priv;

  for (; iter->i < iter->n_items; ++iter->i)
    {
      gchar *selected_id;
      gboolean found;

      selected_id = g_list_model_get_item (priv->selection, iter->i);
      found = g_hash_table_lookup_extended (priv->objects, selected_id,
                                            (gpointer *) &iter->id, (gpointer *) &iter->track);
      g_free (selected_id);

      if (!found || iter->track->type != HYSCAN_PLANNER_TRACK)
        continue;

      ++iter->i;

      return TRUE;
    }

  return FALSE;
}

static void
hyscan_gtk_planner_editor_start_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->start_x, priv->start_y, &iter.track->start);
      hyscan_object_model_modify_object (priv->model, iter.id, iter.track);
    }
}

static void
hyscan_gtk_planner_editor_end_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->end_x, priv->end_y, &iter.track->end);
      hyscan_object_model_modify_object (priv->model, iter.id, iter.track);
    }
}

static void
hyscan_gtk_planner_editor_length_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;
  gdouble length;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  length = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->length));

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      HyScanGeoCartesian2D start_2d, end_2d;
      gdouble scale;
      gdouble dx, dy;

      hyscan_geo_geo2topoXY (priv->geo, &start_2d, track->start);
      hyscan_geo_geo2topoXY (priv->geo, &end_2d, track->end);
      dx = end_2d.x - start_2d.x;
      dy = end_2d.y - start_2d.y;
      scale = length / hypot (dx, dy);
      end_2d.x = start_2d.x + scale * dx;
      end_2d.y = start_2d.y + scale * dy;

      hyscan_geo_topoXY2geo (priv->geo, &track->end, end_2d, 0);
      hyscan_object_model_modify_object (priv->model, id, track);
    }
}

static void
hyscan_gtk_planner_editor_angle_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble angle;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->angle));
  angle = - angle / 180.0 * G_PI - G_PI - G_PI_2;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      HyScanGeoCartesian2D start_2d, end_2d;
      gdouble length;
      gdouble dx, dy;

      hyscan_geo_geo2topoXY (priv->geo, &start_2d, track->start);
      hyscan_geo_geo2topoXY (priv->geo, &end_2d, track->end);
      dx = end_2d.x - start_2d.x;
      dy = end_2d.y - start_2d.y;
      length = hypot (dx, dy);
      end_2d.x = start_2d.x + length * cos (angle);
      end_2d.y = start_2d.y + length * sin (angle);

      hyscan_geo_topoXY2geo (priv->geo, &track->end, end_2d, 0);
      hyscan_object_model_modify_object (priv->model, id, track);
    }
}

static void
hyscan_gtk_planner_editor_model_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  priv->objects = hyscan_object_model_get (priv->model);

  hyscan_gtk_planner_editor_update_view (editor);
}

/**
 * hyscan_gtk_planner_editor_new:
 * @model: модель объектов планировщика
 * @selection: модель данных выбранных объектов
 *
 * Функция создаёт виджет для редактирования параметров выбранных галсов.
 *
 * Returns: указатель на виджет
 */
GtkWidget *
hyscan_gtk_planner_editor_new (HyScanObjectModel *model,
                               GListModel        *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_EDITOR,
                       "model", model,
                       "selection", selection,
                       NULL);
}
