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
#define MAX_SPEED    1e2   /* Максимальное значение координат X, Y концов галса, метры. */
#define DIST_STEP    1e-2  /* Шаг координат и длины галса, метры. */
#define ANGLE_STEP   1e-2  /* Шаг угла поворота, градусы. */
#define SPEED_STEP   1e-2  /* Шаг угла поворота, градусы. */

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

typedef struct
{
  gboolean origin_set;       /* Признак того, что задана точка отсчёта. */
  guint    n_items;          /* Количество галсов, по которым получены значения. */

  gdouble  start_x;          /* Координата X начала галса. */
  gdouble  start_y;          /* Координата Y начала галса. */
  gdouble  end_x;            /* Координата X конца галса. */
  gdouble  end_y;            /* Координата Y конца галса. */
  gdouble  length;           /* Длина галса. */
  gdouble  angle;            /* Угол поворота галса. */
  gdouble  speed;            /* Скорость движения по галсу. */

  gboolean start_x_diffs;    /* Признак того, что значение start_x варьируется. */
  gboolean start_y_diffs;    /* Признак того, что значение start_y варьируется. */
  gboolean end_x_diffs;      /* Признак того, что значение end_x варьируется. */
  gboolean end_y_diffs;      /* Признак того, что значение end_y варьируется. */
  gboolean length_diffs;     /* Признак того, что значение length варьируется. */
  gboolean angle_diffs;      /* Признак того, что значение angle варьируется. */
  gboolean speed_diffs;      /* Признак того, что значение speed варьируется. */
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
  HyScanPlannerModel     *model;                 /* Модель объектов планировщика. */
  HyScanPlannerSelection *selection;             /* Модель списка идентификаторов выбранных объектов. */
  HyScanGeo              *geo;                   /* Объект перевода координат из географических в декртовы. */
  gboolean                block_value_change;    /* Признак блокировки сигналов "value-changed" в полях ввода. */

  gchar                 **selected_tracks;       /* Список выбранных галсов. */
  GHashTable             *objects;               /* Хэш таблица с объектами планировщика. */

  GtkWidget              *label;                 /* Виджет счётчика выбранных объектов. */
  GtkWidget              *start_x;               /* Поле ввода координаты X начала. */
  GtkWidget              *start_x_btn;           /* Галочка редактирования поля start_x. */
  GtkWidget              *start_y;               /* Поле ввода координаты Y начала. */
  GtkWidget              *start_y_btn;           /* Галочка редактирования поля start_y. */
  GtkWidget              *end_x;                 /* Поле ввода координаты X конца. */
  GtkWidget              *end_x_btn;             /* Галочка редактирования поля end_x. */
  GtkWidget              *end_y;                 /* Поле ввода координаты Y конца. */
  GtkWidget              *end_y_btn;             /* Галочка редактирования поля end_y. */
  GtkWidget              *length;                /* Поле ввода длины галса. */
  GtkWidget              *length_btn;            /* Галочка редактирования поля end_y. */
  GtkWidget              *angle;                 /* Поле ввода угла поворота галса. */
  GtkWidget              *angle_btn;             /* Галочка редактирования поля angle. */
  GtkWidget              *speed;                 /* Поле ввода скорости движения по галсу. */
  GtkWidget              *speed_btn;             /* Галочка редактирования поля speed. */
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
static void        hyscan_gtk_planner_editor_tracks_changed          (HyScanGtkPlannerEditor      *editor,
                                                                      gchar                      **tracks);
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
static void        hyscan_gtk_planner_editor_speed_changed           (HyScanGtkPlannerEditor      *editor);
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
    g_param_spec_object ("model", "HyScanPlannerModel", "HyScanPlannerModel with planner data",
                         HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanPlannerSelection", "HyScanPlannerSelection with selected objects",
                         HYSCAN_TYPE_PLANNER_SELECTION,
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

  GtkGrid *grid = GTK_GRID (object);

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->constructed (object);

  priv->geo = hyscan_planner_model_get_geo (priv->model);

  priv->label = gtk_label_new (NULL);
  priv->start_x = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->start_y = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->end_x = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->end_y = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->angle = gtk_spin_button_new_with_range (-360.0, 360.0, ANGLE_STEP);
  priv->length = gtk_spin_button_new_with_range (-MAX_DISTANCE, MAX_DISTANCE, DIST_STEP);
  priv->speed = gtk_spin_button_new_with_range (0, MAX_SPEED, SPEED_STEP);
  priv->start_x_btn = gtk_check_button_new ();
  priv->start_y_btn = gtk_check_button_new ();
  priv->end_x_btn = gtk_check_button_new ();
  priv->end_y_btn = gtk_check_button_new ();
  priv->length_btn = gtk_check_button_new ();
  priv->angle_btn = gtk_check_button_new ();
  priv->speed_btn = gtk_check_button_new ();

  gtk_grid_set_row_spacing (grid, 3);
  gtk_grid_set_column_spacing (grid, 6);
  gtk_widget_set_halign (priv->label, GTK_ALIGN_START);
  gtk_grid_attach (grid, priv->label, 0, ++i, 2, 1);

  hyscan_gtk_planner_editor_attach (grid, _("Speed"), priv->speed, priv->speed_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_speed_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Length"), priv->length, priv->length_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_length_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Angle"), priv->angle, priv->angle_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_angle_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Start X"), priv->start_x, priv->start_x_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_start_changed));
  hyscan_gtk_planner_editor_attach (grid, _("Start Y"), priv->start_y, priv->start_y_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_start_changed));
  hyscan_gtk_planner_editor_attach (grid, _("End X"), priv->end_x, priv->end_x_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_end_changed));
  hyscan_gtk_planner_editor_attach (grid, _("End Y"), priv->end_y, priv->end_y_btn, ++i,
                                    G_CALLBACK (hyscan_gtk_planner_editor_end_changed));

  g_signal_connect_swapped (priv->selection, "tracks-changed",
                            G_CALLBACK (hyscan_gtk_planner_editor_tracks_changed), editor);
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
  g_clear_object (&priv->geo);

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
  GtkWidget *label_widget;

  label_widget = gtk_label_new (label);
  gtk_widget_set_halign (label_widget, GTK_ALIGN_END);

  gtk_grid_attach (grid, label_widget, 0, top, 1, 1);
  gtk_grid_attach (grid, entry,        1, top, 1, 1);
  gtk_grid_attach (grid, checkbox,     2, top, 1, 1);

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

  HyScanGtkPlannerEditorIter iter;

  memset (value, 0, sizeof (*value));
  value->origin_set = (priv->geo != NULL);

  if (priv->objects == NULL || priv->geo == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      HyScanGeoCartesian2D start;
      HyScanGeoCartesian2D end;
      gdouble length, angle, speed;

      if (!hyscan_geo_geo2topoXY (priv->geo, &start, track->start) ||
          !hyscan_geo_geo2topoXY (priv->geo, &end, track->end))
        {
          g_warning ("HyScanGtkPlannerEditor: failed to convert geo coordinates to (x, y)");
          continue;
        }

      speed = track->speed;
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
          value->speed = speed;
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

          if (ABS (value->speed - speed) > SPEED_STEP / 2.0)
            value->speed_diffs = TRUE;
        }

      value->n_items++;
    }
}

/* Обновляет значения в полях ввода. */
static void
hyscan_gtk_planner_editor_update_view (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorValue value;
  gboolean value_set;

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
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->speed), value.speed);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (priv->start_x), "");
      gtk_entry_set_text (GTK_ENTRY (priv->start_y), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_x), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_y), "");
      gtk_entry_set_text (GTK_ENTRY (priv->angle), "");
      gtk_entry_set_text (GTK_ENTRY (priv->length), "");
      gtk_entry_set_text (GTK_ENTRY (priv->speed), "");
    }

  gtk_widget_set_sensitive (GTK_WIDGET (editor), value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_x_btn), !value.start_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_y_btn), !value.start_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_x_btn), !value.end_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_y_btn), !value.end_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->angle_btn), !value.angle_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->length_btn), !value.length_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->speed_btn), !value.speed_diffs && value_set);

  priv->block_value_change = FALSE;

  if (value.origin_set)
    {
      gchar *label_text;

      label_text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d track selected", "%d tracks selected", value.n_items),
                                    value.n_items);
      gtk_label_set_text (GTK_LABEL (priv->label), label_text);
      g_free (label_text);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->label), _("The origin of coordinate system is not set."));
    }
}

static void
hyscan_gtk_planner_editor_tracks_changed (HyScanGtkPlannerEditor  *editor,
                                          gchar                  **tracks)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_clear_pointer (&priv->selected_tracks, g_strfreev);
  priv->selected_tracks = g_strdupv (tracks);

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
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  iter->priv = priv;
  iter->i = 0;
  iter->n_items = priv->selected_tracks == NULL ? 0 : g_strv_length (priv->selected_tracks);
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

      selected_id = priv->selected_tracks[iter->i];
      found = g_hash_table_lookup_extended (priv->objects, selected_id,
                                            (gpointer *) &iter->id, (gpointer *) &iter->track);

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

  if (priv->block_value_change || priv->objects == NULL || priv->geo == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->start_x, priv->start_y, &iter.track->start);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_end_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  if (priv->block_value_change || priv->objects == NULL || priv->geo == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->end_x, priv->end_y, &iter.track->end);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_length_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;
  gdouble length;

  if (priv->block_value_change || priv->objects == NULL || priv->geo == NULL)
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
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (HyScanObject *) track);
    }
}

static void
hyscan_gtk_planner_editor_angle_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble angle;

  if (priv->block_value_change || priv->objects == NULL || priv->geo == NULL)
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
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (HyScanObject *) track);
    }
}

static void
hyscan_gtk_planner_editor_speed_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble speed;

  if (priv->block_value_change || priv->objects == NULL || priv->geo == NULL)
    return;

  speed = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->speed));

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      track->speed = speed;
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (const HyScanObject *) track);
    }
}

static void
hyscan_gtk_planner_editor_model_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  priv->objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));
  g_clear_object (&priv->geo);
  priv->geo = hyscan_planner_model_get_geo (priv->model);

  hyscan_gtk_planner_editor_update_view (editor);
}

/**
 * hyscan_gtk_planner_editor_new:
 * @model: модель объектов планировщика #HyScanPlannerModel
 * @selection: модель данных выбранных объектов
 *
 * Функция создаёт виджет для редактирования параметров выбранных галсов.
 *
 * Returns: указатель на виджет
 */
GtkWidget *
hyscan_gtk_planner_editor_new (HyScanPlannerModel     *model,
                               HyScanPlannerSelection *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_EDITOR,
                       "model", model,
                       "selection", selection,
                       NULL);
}
