/* hyscan-gtk-planner-editor.c
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
 * SECTION: hyscan-gtk-planner-editor
 * @Short_description: Виджет редактора параметров плановых галсов
 * @Title: HyScanGtkPlannerEditor
 *
 * Виджет HyScanGtkPlannerEditor содержит в себе поля для редактирования параметров галсов:
 * - скорость и длина галса,
 * - курс движения,
 * - координаты точек начала и конца,
 * - угол поворота.
 *
 * Виджет переводит географические координаты галсов в декартовы (x, y), если пользователь задал #HyScanPlannerOrigin -
 * параметры декартовой СК (точку отсчёта и направление оси OX).
 *
 * При измении значений в полях редактирования параметры галса пересчитываются в географические координаты и сохраняются
 * в базу данных.
 *
 * Если пользователь выбрал несколько галсов, то виджет позволяет отредактировать значения отдельных параметров
 * сразу у всех выбранных галсов.
 *
 */

#include "hyscan-gtk-planner-editor.h"
#include <glib/gi18n-lib.h>
#include <math.h>

#define MAX_DISTANCE 1e6   /* Максимальное значение координат X, Y концов галса, метры. */
#define MAX_SPEED    1e2   /* Максимальное значение координат X, Y концов галса, метры. */
#define DIST_STEP    1e-2  /* Шаг координат и длины галса, метры. */
#define ANGLE_STEP   1e-2  /* Шаг угла поворота, градусы. */
#define SPEED_STEP   1e-2  /* Шаг скорости, м/с. */
#define LL_STEP      1e-6  /* Шаг широты и долготы, градусы. */

#define NORMALIZE_DEG(x) ((x) < 0 ? (x) + 360.0 : (x))

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
  gdouble  angle;            /* Угол галса к оси OX. */
  gdouble  speed;            /* Скорость движения по галсу. */
  gdouble  course;           /* Курс галса. */
  gdouble  start_lat;        /* Широта начала галса. */
  gdouble  start_lon;        /* Долгота начала галса. */
  gdouble  end_lat;          /* Широта конца галса. */
  gdouble  end_lon;          /* Долгота конца галса. */

  gboolean start_x_diffs;    /* Признак того, что значение start_x варьируется. */
  gboolean start_y_diffs;    /* Признак того, что значение start_y варьируется. */
  gboolean end_x_diffs;      /* Признак того, что значение end_x варьируется. */
  gboolean end_y_diffs;      /* Признак того, что значение end_y варьируется. */
  gboolean length_diffs;     /* Признак того, что значение length варьируется. */
  gboolean angle_diffs;      /* Признак того, что значение angle варьируется. */
  gboolean speed_diffs;      /* Признак того, что значение speed варьируется. */
  gdouble  course_diffs;     /* Признак того, что значение track варьируется. */
  gdouble  start_lat_diffs;  /* Признак того, что значение start_lat варьируется. */
  gdouble  start_lon_diffs;  /* Признак того, что значение start_lon варьируется. */
  gdouble  end_lat_diffs;    /* Признак того, что значение end_lat варьируется. */
  gdouble  end_lon_diffs;    /* Признак того, что значение end_lon варьируется. */
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
  HyScanGeo              *geo;                   /* Объект перевода координат из географических в декартовы. */
  gboolean                block_value_change;    /* Признак блокировки сигналов "value-changed" в полях ввода. */

  gchar                 **selected_tracks;       /* Список выбранных галсов. */
  GHashTable             *objects;               /* Хэш таблица с объектами планировщика. */

  GtkWidget              *label;                 /* Виджет счётчика выбранных объектов. */
  GtkWidget              *topo_label;            /* Подпись топографических полeй. */
  GtkWidget              *topo_box;              /* Контейнер топографических полeй. */
  GtkWidget              *start_x;               /* Поле ввода координаты X начала. */
  GtkWidget              *start_x_btn;           /* Галочка редактирования поля start_x. */
  GtkWidget              *start_y;               /* Поле ввода координаты Y начала. */
  GtkWidget              *start_y_btn;           /* Галочка редактирования поля start_y. */
  GtkWidget              *end_x;                 /* Поле ввода координаты X конца. */
  GtkWidget              *end_x_btn;             /* Галочка редактирования поля end_x. */
  GtkWidget              *end_y;                 /* Поле ввода координаты Y конца. */
  GtkWidget              *end_y_btn;             /* Галочка редактирования поля end_y. */
  GtkWidget              *start_lat;             /* Поле ввода координаты широты начала. */
  GtkWidget              *start_lat_btn;         /* Галочка редактирования поля start_lat. */
  GtkWidget              *start_lon;             /* Поле ввода координаты долготы начала. */
  GtkWidget              *start_lon_btn;         /* Галочка редактирования поля start_lon. */
  GtkWidget              *end_lat;               /* Поле ввода координаты широты конца. */
  GtkWidget              *end_lat_btn;           /* Галочка редактирования поля end_lat. */
  GtkWidget              *end_lon;               /* Поле ввода координаты долготы конца. */
  GtkWidget              *end_lon_btn;           /* Галочка редактирования поля end_lon. */
  GtkWidget              *length;                /* Поле ввода длины галса. */
  GtkWidget              *length_btn;            /* Галочка редактирования поля end_y. */
  GtkWidget              *angle;                 /* Поле ввода угла поворота галса. */
  GtkWidget              *angle_btn;             /* Галочка редактирования поля angle. */
  GtkWidget              *track;                 /* Поле ввода курса галса. */
  GtkWidget              *track_btn;             /* Галочка редактирования поля track. */
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
                                                                      HyScanGeoPoint              *value);
static void        hyscan_gtk_planner_editor_spin_changed            (GtkWidget                   *spin_button,
                                                                      HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_model_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_end_changed             (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_start_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_start_geo_changed       (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_end_geo_changed         (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_length_changed          (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_angle_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_speed_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_track_changed           (HyScanGtkPlannerEditor      *editor);
static void        hyscan_gtk_planner_editor_update_view             (HyScanGtkPlannerEditor      *editor);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerEditor, hyscan_gtk_planner_editor, GTK_TYPE_BOX)

static void
hyscan_gtk_planner_editor_class_init (HyScanGtkPlannerEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_editor_set_property;
  object_class->constructed = hyscan_gtk_planner_editor_object_constructed;
  object_class->finalize = hyscan_gtk_planner_editor_object_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/hyscan/gtk/hyscan-gtk-planner-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, label);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, topo_box);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, topo_label);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_x);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_y);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_x);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_y);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_lat);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_lon);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_lat);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_lon);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, track);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, angle);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, length);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, speed);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_x_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_y_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_x_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_y_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_lat_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, start_lon_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_lat_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, end_lon_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, track_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, angle_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, length_btn);
  gtk_widget_class_bind_template_child_private (widget_class, HyScanGtkPlannerEditor, speed_btn);

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
  gtk_widget_init_template (GTK_WIDGET (editor));
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
  guint i;
  struct
  {
    GtkWidget     *spin_btn;  /* Поле ввода. */
    GtkWidget     *checkbox;  /* Галочка включения поля ввода. */
    gdouble        lower;     /* Минимальное значение параметра. */
    gdouble        upper;     /* Максимальное значение параметра. */
    gdouble        step;      /* Шаг изменения. */
    gint           digits;    /* Число знаков после запятой. */
  } ranges[] = {
    { priv->start_x,     priv->start_x_btn,   -MAX_DISTANCE, MAX_DISTANCE,  DIST_STEP, 2 },
    { priv->start_y,     priv->start_y_btn,   -MAX_DISTANCE, MAX_DISTANCE,  DIST_STEP, 2 },
    { priv->end_x,       priv->end_x_btn,     -MAX_DISTANCE, MAX_DISTANCE,  DIST_STEP, 2 },
    { priv->end_y,       priv->end_y_btn,     -MAX_DISTANCE, MAX_DISTANCE,  DIST_STEP, 2 },
    { priv->angle,       priv->angle_btn,            -360.0,        360.0, ANGLE_STEP, 2 },
    { priv->track,       priv->track_btn,            -360.0,        360.0, ANGLE_STEP, 2 },
    { priv->length,      priv->length_btn,    -MAX_DISTANCE, MAX_DISTANCE,  DIST_STEP, 2 },
    { priv->speed,       priv->speed_btn,                 0,    MAX_SPEED, SPEED_STEP, 2 },
    { priv->start_lat,   priv->start_lat_btn,         -90.0,         90.0,    LL_STEP, 6 },
    { priv->start_lon,   priv->start_lon_btn,        -180.0,        180.0,    LL_STEP, 6 },
    { priv->end_lat,     priv->end_lat_btn,           -90.0,         90.0,    LL_STEP, 6 },
    { priv->end_lon,     priv->end_lon_btn,          -180.0,        180.0,    LL_STEP, 6 },
  };

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->constructed (object);

  priv->geo = hyscan_planner_model_get_geo (priv->model);
  for (i = 0; i < G_N_ELEMENTS (ranges); i++)
    {
      GtkAdjustment *adjustment;

      adjustment = gtk_adjustment_new (0, ranges[i].lower, ranges[i].upper, ranges[i].step, 0, 0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (ranges[i].spin_btn), ranges[i].digits);
      gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (ranges[i].spin_btn), adjustment);
      g_object_bind_property (ranges[i].checkbox, "active", ranges[i].spin_btn, "sensitive", G_BINDING_SYNC_CREATE);
      g_signal_connect (ranges[i].spin_btn, "value-changed", G_CALLBACK (hyscan_gtk_planner_editor_spin_changed), editor);
    }

  g_signal_connect_swapped (priv->selection, "tracks-changed",
                            G_CALLBACK (hyscan_gtk_planner_editor_tracks_changed), editor);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_editor_model_changed), editor);

  priv->selected_tracks = hyscan_planner_selection_get_tracks (priv->selection);
  hyscan_gtk_planner_editor_model_changed (editor);
}

static void
hyscan_gtk_planner_editor_object_finalize (GObject *object)
{
  HyScanGtkPlannerEditor *editor = HYSCAN_GTK_PLANNER_EDITOR (object);
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  g_signal_handlers_disconnect_by_data (priv->selection, editor);
  g_signal_handlers_disconnect_by_data (priv->model, editor);
  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_clear_pointer (&priv->selected_tracks, g_strfreev);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_clear_object (&priv->geo);

  G_OBJECT_CLASS (hyscan_gtk_planner_editor_parent_class)->finalize (object);
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

  if (priv->objects == NULL)
    return;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      HyScanTrackPlan *plan;
      HyScanGeoCartesian2D start = {0};
      HyScanGeoCartesian2D end = {0};
      gdouble length, angle = 0.0, angle_north, speed;

      plan = &track->plan;
      speed = plan->speed;

      length = hyscan_planner_track_length (plan);
      angle_north = hyscan_planner_track_angle (track) / G_PI * 180.0;
      angle_north = NORMALIZE_DEG (angle_north);

      if (priv->geo != NULL &&
           hyscan_geo_geo2topoXY0 (priv->geo, &start, plan->start) &&
           hyscan_geo_geo2topoXY0 (priv->geo, &end, plan->end))
        {
          angle = atan2 (end.y - start.y, end.x - start.x) / G_PI * 180.0;
          angle = NORMALIZE_DEG (angle);
        }

      if (value->n_items == 0)
        {
          value->course = angle_north;
          value->start_lat = plan->start.lat;
          value->start_lon = plan->start.lon;
          value->end_lat = plan->end.lat;
          value->end_lon = plan->end.lon;
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
          if (ABS (value->start_lat - plan->start.lat) > LL_STEP / 2.0)
            value->start_lat_diffs = TRUE;

          if (ABS (value->start_lon - plan->start.lon) > LL_STEP / 2.0)
            value->start_lon_diffs = TRUE;

          if (ABS (value->end_lat - plan->end.lat) > LL_STEP / 2.0)
            value->end_lat_diffs = TRUE;

          if (ABS (value->end_lon - plan->end.lon) > LL_STEP / 2.0)
            value->end_lon_diffs = TRUE;

          if (ABS (value->course - angle_north) > ANGLE_STEP / 2.0)
            value->course_diffs = TRUE;

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
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_lat), value.start_lat);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_lon), value.start_lon);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_lat), value.end_lat);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_lon), value.end_lon);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->track), value.course);
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
      gtk_entry_set_text (GTK_ENTRY (priv->start_lat), "");
      gtk_entry_set_text (GTK_ENTRY (priv->start_lon), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_lat), "");
      gtk_entry_set_text (GTK_ENTRY (priv->end_lon), "");
      gtk_entry_set_text (GTK_ENTRY (priv->track), "");
    }

  gtk_widget_set_sensitive (GTK_WIDGET (editor), value_set);
  gtk_widget_set_sensitive (priv->topo_box, value.origin_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_x_btn), !value.start_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_y_btn), !value.start_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_x_btn), !value.end_x_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_y_btn), !value.end_y_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->angle_btn), !value.angle_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->length_btn), !value.length_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->speed_btn), !value.speed_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_lat_btn), !value.start_lat_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->start_lon_btn), !value.start_lon_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_lat_btn), !value.end_lat_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->end_lon_btn), !value.end_lon_diffs && value_set);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->track_btn), !value.course_diffs && value_set);

  priv->block_value_change = FALSE;

  gchar *label_text;

  if (value.n_items == 0)
    {
      gtk_label_set_text (GTK_LABEL (priv->label), _("Select track to edit its properties"));
    }
  else
    {
      label_text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d track selected", "%d tracks selected", value.n_items),
                                    value.n_items);
      gtk_label_set_text (GTK_LABEL (priv->label), label_text);
      g_free (label_text);
    }

  gtk_widget_set_visible (priv->topo_label, !value.origin_set);
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
                                         HyScanGeoPoint         *value)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGeoPoint geodetic;
  HyScanGeoCartesian2D cartesian;

  geodetic = *value;
  hyscan_geo_geo2topoXY0 (priv->geo, &cartesian, geodetic);

  if (gtk_widget_get_sensitive (widget_x))
    cartesian.x = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget_x));

  if (gtk_widget_get_sensitive (widget_y))
    cartesian.y = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget_y));

  hyscan_geo_topoXY2geo0 (priv->geo, value, cartesian);
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

      selected_id = priv->selected_tracks[iter->i];
      g_hash_table_lookup_extended (priv->objects, selected_id,
                                    (gpointer *) &iter->id, (gpointer *) &iter->track);

      if (!HYSCAN_IS_PLANNER_TRACK (iter->track))
        continue;

      ++iter->i;

      return TRUE;
    }

  return FALSE;
}

static void
hyscan_gtk_planner_editor_spin_changed (GtkWidget              *spin_button,
                                        HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;

  if (priv->block_value_change || priv->objects == NULL)
    return;

  if (spin_button == priv->start_x || spin_button == priv->start_y)
    hyscan_gtk_planner_editor_start_changed (editor);

  else if (spin_button == priv->end_x || spin_button == priv->end_y)
    hyscan_gtk_planner_editor_end_changed (editor);

  else if (spin_button == priv->end_lat || spin_button == priv->end_lon)
    hyscan_gtk_planner_editor_end_geo_changed (editor);

  else if (spin_button == priv->start_lat || spin_button == priv->start_lon)
    hyscan_gtk_planner_editor_start_geo_changed (editor);

  else if (spin_button == priv->track)
    hyscan_gtk_planner_editor_track_changed (editor);

  else if (spin_button == priv->length)
    hyscan_gtk_planner_editor_length_changed (editor);

  else if (spin_button == priv->angle)
    hyscan_gtk_planner_editor_angle_changed (editor);

  else if (spin_button == priv->speed)
    hyscan_gtk_planner_editor_speed_changed (editor);

}

static void
hyscan_gtk_planner_editor_start_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->start_x, priv->start_y, &iter.track->plan.start);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_end_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      hyscan_gtk_planner_editor_convert_point (editor, priv->end_x, priv->end_y, &iter.track->plan.end);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_start_geo_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      if (gtk_widget_get_sensitive (priv->start_lat))
        iter.track->plan.start.lat = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->start_lat));
      if (gtk_widget_get_sensitive (priv->start_lon))
        iter.track->plan.start.lon = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->start_lon));
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_end_geo_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      if (gtk_widget_get_sensitive (priv->end_lat))
        iter.track->plan.end.lat = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->end_lat));
      if (gtk_widget_get_sensitive (priv->end_lon))
        iter.track->plan.end.lon = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->end_lon));
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), iter.id, (HyScanObject *) iter.track);
    }
}

static void
hyscan_gtk_planner_editor_length_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;
  gdouble length;

  length = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->length));

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanGeo *geo;
      HyScanPlannerTrack *track = iter.track;
      HyScanTrackPlan *plan = &track->plan;
      const gchar *id = iter.id;

      HyScanGeoCartesian2D start_2d, end_2d;
      gdouble scale;
      gdouble dx, dy;

      geo = hyscan_planner_track_geo (plan, NULL);
      hyscan_geo_geo2topoXY0 (geo, &start_2d, plan->start);
      hyscan_geo_geo2topoXY0 (geo, &end_2d, plan->end);
      dx = end_2d.x - start_2d.x;
      dy = end_2d.y - start_2d.y;
      scale = length / hypot (dx, dy);
      end_2d.x = start_2d.x + scale * dx;
      end_2d.y = start_2d.y + scale * dy;

      hyscan_geo_topoXY2geo0 (geo, &plan->end, end_2d);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (HyScanObject *) track);
      g_object_unref (geo);
    }
}

static void
hyscan_gtk_planner_editor_angle_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble angle;

  angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->angle));
  angle = angle / 180.0 * G_PI;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      HyScanGeoCartesian2D start_2d, end_2d;
      gdouble length;
      gdouble dx, dy;

      /* Сохраняем точку начала галса и его длину. */
      hyscan_geo_geo2topoXY0 (priv->geo, &start_2d, track->plan.start);
      hyscan_geo_geo2topoXY0 (priv->geo, &end_2d, track->plan.end);
      dx = end_2d.x - start_2d.x;
      dy = end_2d.y - start_2d.y;
      length = hypot (dx, dy);
      end_2d.x = start_2d.x + length * cos (angle);
      end_2d.y = start_2d.y + length * sin (angle);

      hyscan_geo_topoXY2geo0 (priv->geo, &track->plan.end, end_2d);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (HyScanObject *) track);
    }
}

static void
hyscan_gtk_planner_editor_track_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble angle;

  /* Новое значение курса. */
  angle = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->track));
  angle = angle / 180.0 * G_PI;

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      HyScanGeoCartesian2D start_2d, end_2d;
      gdouble length;
      gdouble dx, dy;
      gint i;

      /* Повторяем поворот до тех пор, пока не достигнем нужной точности. */
      for (i = 0; i < 10; i++)
        {
          HyScanGeo *geo;
          gdouble current_angle;
          gdouble d_angle;

          current_angle = hyscan_planner_track_angle (track);
          d_angle = remainder (current_angle - angle, 2 * G_PI);
          if (ABS (d_angle) < ANGLE_STEP / 180.0 * G_PI / 2.0)
            break;

          /* Сохраняем точку начала галса и его длину. */
          geo = hyscan_planner_track_geo (&track->plan, NULL);
          hyscan_geo_geo2topoXY0 (geo, &start_2d, track->plan.start);
          hyscan_geo_geo2topoXY0 (geo, &end_2d, track->plan.end);
          dx = end_2d.x - start_2d.x;
          dy = end_2d.y - start_2d.y;
          length = hypot (dx, dy);
          end_2d.x = start_2d.x + length * cos (d_angle);
          end_2d.y = start_2d.y + length * sin (d_angle);

          hyscan_geo_topoXY2geo0 (geo, &track->plan.end, end_2d);
          g_object_unref (geo);
        }

      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), id, (HyScanObject *) track);
    }
}

static void
hyscan_gtk_planner_editor_speed_changed (HyScanGtkPlannerEditor *editor)
{
  HyScanGtkPlannerEditorPrivate *priv = editor->priv;
  HyScanGtkPlannerEditorIter iter;

  gdouble speed;

  speed = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->speed));

  hyscan_gtk_planner_editor_iter_init (editor, &iter);
  while (hyscan_gtk_planner_editor_iter_next (&iter))
    {
      HyScanPlannerTrack *track = iter.track;
      const gchar *id = iter.id;

      track->plan.speed = speed;
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
