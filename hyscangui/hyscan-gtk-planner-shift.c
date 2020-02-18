/* hyscan-gtk-planner-shift.c
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
 * SECTION: hyscan-gtk-planner-shift
 * @Short_description: Виджет для создания параллельных плановых галсов
 * @Title: HyScanGtkPlannerShift
 *
 * Виджет HyScanGtkPlannerOrigin содержит в себе текстовые поля для редактирования
 * начала координат планировщика:
 * - широты,
 * - долготы,
 * - направления оси OX.
 *
 * При измении значений в полях информация записывается в базу данных.
 *
 */

#include <glib/gi18n-lib.h>
#include <hyscan-cartesian.h>
#include "hyscan-gtk-planner-shift.h"

#define MAX_TRACKS 200   /* Максимальное количество генерируемых галсов. */

enum
{
  PROP_O,
  PROP_TRACK
};

struct _HyScanGtkPlannerShiftPrivate
{
  HyScanPlannerTrack          *track;           /* Опорный план галса. */
  HyScanGeo                   *geo;             /* Объект перевода геокоординат. */
  HyScanGeoCartesian2D         track_start;     /* Координаты начала галса в прямоугольной СК. */
  HyScanGeoCartesian2D         track_end;       /* Координаты конца галса в прямоугольной СК. */
  HyScanGeoCartesian2D        *vertices;        /* Координаты вершин ограничивающего периметра. */
  gsize                        vertices_len;    /* Число вершин. */
  HyScanGtkMapPlanner         *viewer;          /* Слой карты для предпросмотра сгенерированных галсов. */
  GList                       *tracks;          /* Сгенерированные галсы. */

  GtkWidget                   *distance;        /* Поле ввода расстояния меджу галсами. */
  GtkWidget                   *alternate;       /* Чекбокс для включения чередования направления галсов. */
  GtkWidget                   *adjust_len;      /* Чекбокс для подгонки длины галсов до пересечения с периметром. */
  GtkWidget                   *num_left;        /* Поле ввода количества галсов слева от опорного. */
  GtkWidget                   *num_right;       /* Поле ввода количества галсов справа от опорного. */
  GtkWidget                   *fill_zone;       /* Чекбокс для заполнения периметра. */
};

static void                   hyscan_gtk_planner_shift_set_property             (GObject                  *object,
                                                                                 guint                     prop_id,
                                                                                 const GValue             *value,
                                                                                 GParamSpec               *pspec);
static void                   hyscan_gtk_planner_shift_object_constructed       (GObject                  *object);
static void                   hyscan_gtk_planner_shift_object_finalize          (GObject                  *object);
static GtkWidget *            hyscan_gtk_planner_shift_make_label               (const gchar              *mnemonic,
                                                                                 GtkWidget                *target);
static void                   hyscan_gtk_planner_shift_update                   (HyScanGtkPlannerShift    *shift);
static void                   hyscan_gtk_planner_shift_fill_zone                (HyScanGtkPlannerShift    *shift);
static gboolean               hyscan_gtk_planner_track_add                      (HyScanGtkPlannerShift    *shift,
                                                                                 const HyScanPlannerTrack *track_tpl,
                                                                                 gint                      index,
                                                                                 gboolean                  alternate,
                                                                                 gboolean                  adjust_len,
                                                                                 gdouble                   distance);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerShift, hyscan_gtk_planner_shift, GTK_TYPE_GRID)

static void
hyscan_gtk_planner_shift_class_init (HyScanGtkPlannerShiftClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_shift_set_property;

  object_class->constructed = hyscan_gtk_planner_shift_object_constructed;
  object_class->finalize = hyscan_gtk_planner_shift_object_finalize;

  g_object_class_install_property (object_class, PROP_TRACK,
    g_param_spec_pointer ("track", "Track", "Original planner track",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_shift_init (HyScanGtkPlannerShift *gtk_planner_shift)
{
  gtk_planner_shift->priv = hyscan_gtk_planner_shift_get_instance_private (gtk_planner_shift);
}

static void
hyscan_gtk_planner_shift_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkPlannerShift *gtk_planner_shift = HYSCAN_GTK_PLANNER_SHIFT (object);
  HyScanGtkPlannerShiftPrivate *priv = gtk_planner_shift->priv;

  switch (prop_id)
    {
    case PROP_TRACK:
      priv->track = hyscan_planner_track_copy (g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_shift_object_constructed (GObject *object)
{
  HyScanGtkPlannerShift *shift = HYSCAN_GTK_PLANNER_SHIFT (object);
  HyScanGtkPlannerShiftPrivate *priv = shift->priv;
  GtkGrid *grid = GTK_GRID (object);
  gint i;

  G_OBJECT_CLASS (hyscan_gtk_planner_shift_parent_class)->constructed (object);

  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);

  priv->distance = gtk_spin_button_new_with_range (0.1, 500, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->distance), 20);

  priv->alternate = gtk_check_button_new_with_mnemonic (_("_Alternate direction"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->alternate), TRUE);

  priv->fill_zone = gtk_check_button_new_with_mnemonic (_("_Fill full zone"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->fill_zone), FALSE);

  priv->adjust_len = gtk_check_button_new_with_mnemonic (_("Adjust length to _zone size"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->adjust_len), TRUE);

  priv->num_left = gtk_spin_button_new_with_range (0, MAX_TRACKS, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->num_left), 1);
  priv->num_right = gtk_spin_button_new_with_range (0, MAX_TRACKS, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->num_right), 0);

  g_signal_connect_swapped (priv->distance, "value-changed", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  g_signal_connect_swapped (priv->num_left, "value-changed", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  g_signal_connect_swapped (priv->num_right, "value-changed", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  g_signal_connect_swapped (priv->fill_zone, "notify::active", G_CALLBACK (hyscan_gtk_planner_shift_fill_zone), shift);
  g_signal_connect_swapped (priv->fill_zone, "notify::active", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  g_signal_connect_swapped (priv->alternate, "notify::active", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  g_signal_connect_swapped (priv->adjust_len, "notify::active", G_CALLBACK (hyscan_gtk_planner_shift_update), shift);
  hyscan_gtk_planner_shift_update (shift);

  i = -1;
  gtk_grid_attach (grid, hyscan_gtk_planner_shift_make_label (_("_Distance between, m"), priv->distance), 0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->distance,                                                                  1,   i, 1, 1);
  gtk_grid_attach (grid, hyscan_gtk_planner_shift_make_label (_("Tracks on the left"), priv->num_left),   0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->num_left,                                                                  1,   i, 1, 1);
  gtk_grid_attach (grid, hyscan_gtk_planner_shift_make_label (_("Tracks on the right"), priv->num_right), 0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->num_right,                                                                 1,   i, 1, 1);
  gtk_grid_attach (grid, priv->adjust_len,                                                                0, ++i, 2, 1);
  gtk_grid_attach (grid, priv->fill_zone,                                                                 0, ++i, 2, 1);
  gtk_grid_attach (grid, priv->alternate,                                                                 0, ++i, 2, 1);
}

static void
hyscan_gtk_planner_shift_object_finalize (GObject *object)
{
  HyScanGtkPlannerShift *shift = HYSCAN_GTK_PLANNER_SHIFT (object);
  HyScanGtkPlannerShiftPrivate *priv = shift->priv;

  g_free (priv->vertices);
  hyscan_gtk_planner_shift_set_viewer (shift, NULL);
  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_planner_track_free);
  hyscan_planner_track_free (priv->track);
  g_clear_object (&priv->geo);

  G_OBJECT_CLASS (hyscan_gtk_planner_shift_parent_class)->finalize (object);
}

/* Генерирует один план галса на указанном расстоянии от опорного. */
static gboolean
hyscan_gtk_planner_track_add (HyScanGtkPlannerShift    *shift,
                              const HyScanPlannerTrack *track_tpl,
                              gint                      index,
                              gboolean                  alternate,
                              gboolean                  adjust_len,
                              gdouble                   distance)
{
  HyScanGtkPlannerShiftPrivate *priv = shift->priv;

  HyScanPlannerTrack *new_track;
  HyScanGeoCartesian2D *start, *end;
  HyScanGeoCartesian2D new_start, new_end;
  gdouble invert;
  gboolean added = FALSE;

  invert = alternate && (index % 2 != 0);
  start = invert ? &priv->track_end : &priv->track_start;
  end = invert ? &priv->track_start : &priv->track_end;

  new_start.x = start->x;
  new_start.y = start->y + distance * index;
  new_end.x = end->x;
  new_end.y = end->y + distance * index;

  /* Если пользователь выбрал подгонять длину галса под размеры зоны и зона вогнутая, то может оказаться, что галс
   * разбивается на несколько. */
  if (adjust_len)
    {
      HyScanGeoCartesian2D *cross_pts;
      guint j, cross_pts_len;
      cross_pts = hyscan_cartesian_polygon_cross (priv->vertices, priv->vertices_len, &new_start, &new_end, &cross_pts_len);

      for (j = 0; j + 1 < cross_pts_len; j += 2)
        {
          new_track = hyscan_planner_track_copy (track_tpl);
          hyscan_geo_topoXY2geo (priv->geo, &new_track->plan.start, cross_pts[j], 0.0);
          hyscan_geo_topoXY2geo (priv->geo, &new_track->plan.end, cross_pts[j + 1], 0.0);

          priv->tracks = g_list_append (priv->tracks, new_track);
          added = TRUE;
        }

      g_free (cross_pts);
    }

  /* Без подгонки сохраняем копию галса в первозданном виде. */
  else
    {
      new_track = hyscan_planner_track_copy (track_tpl);
      hyscan_geo_topoXY2geo (priv->geo, &new_track->plan.start, new_start, 0.0);
      hyscan_geo_topoXY2geo (priv->geo, &new_track->plan.end, new_end, 0.0);

      priv->tracks = g_list_append (priv->tracks, new_track);
      added = TRUE;
    }

  return added;
}

/* Обработчик галочки "Заполнить зону". */
static void
hyscan_gtk_planner_shift_fill_zone (HyScanGtkPlannerShift *shift)
{
  HyScanGtkPlannerShiftPrivate *priv = shift->priv;
  gboolean sensitive;

  sensitive = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->fill_zone));
  gtk_widget_set_sensitive (priv->adjust_len, sensitive);
  gtk_widget_set_sensitive (priv->num_left, sensitive);
  gtk_widget_set_sensitive (priv->num_right, sensitive);
}

/* Генерирует галсы согласно установленным настройкам. */
static void
hyscan_gtk_planner_shift_update (HyScanGtkPlannerShift *shift)
{
  HyScanGtkPlannerShiftPrivate *priv = shift->priv;
  HyScanPlannerTrack track_tpl = {0};

  gdouble distance;
  gint num_right, num_left;
  gboolean alternate;
  gboolean adjust_len;
  gboolean fill_zone;

  gint i;

  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_planner_track_free);
  priv->tracks = NULL;

  distance = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->distance));
  num_left = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->num_left));
  num_right = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->num_right));
  alternate = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->alternate));
  adjust_len = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->adjust_len));
  fill_zone = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->fill_zone));

  if (priv->track == NULL || (num_right == 0 && num_left == 0 && !fill_zone) || distance <= 0)
    goto exit;

  track_tpl.zone_id = priv->track->zone_id;
  track_tpl.plan.velocity = priv->track->plan.velocity;

  if (fill_zone)
    {
      gboolean add_left = TRUE, add_right = TRUE;

      /* Если заполняем зону, то длину всегда подгоняем. */
      adjust_len = TRUE;
      i = 1;
      do
        {
          add_left &= hyscan_gtk_planner_track_add (shift, &track_tpl, i, alternate, adjust_len, distance);
          add_right &= hyscan_gtk_planner_track_add (shift, &track_tpl, -i, alternate, adjust_len, distance);
          i++;
        } while ((add_left || add_right) && i < MAX_TRACKS);
    }
  else
    {
      for (i = 1; i <= num_left; ++i)
        hyscan_gtk_planner_track_add (shift, &track_tpl, i, alternate, adjust_len, distance);

      for (i = -1; i >= -num_right; --i)
        hyscan_gtk_planner_track_add (shift, &track_tpl, i, alternate, adjust_len, distance);
    }

exit:
  if (priv->viewer)
    hyscan_gtk_map_planner_set_preview (priv->viewer, priv->tracks);
}

/* Создаёт подпись для поля ввода target. */
static GtkWidget *
hyscan_gtk_planner_shift_make_label (const gchar *mnemonic,
                                     GtkWidget   *target)
{
  GtkWidget *label;

  label = gtk_label_new_with_mnemonic (mnemonic);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), target);
  gtk_widget_set_halign (label, GTK_ALIGN_START);

  return label;
}

/**
 * hyscan_gtk_planner_shift_new:
 *
 * Создает виджет для генерации параллельных галсов.
 *
 * Returns: новый виджет #HyScanGtkPlannerShift
 */
GtkWidget *
hyscan_gtk_planner_shift_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_SHIFT, NULL);
}

/**
 * hyscan_gtk_planner_shift_set_viewer:
 * @shift: указатель на #HyScanGtkPlannerShift
 * @viewer: указатель на слой #HyScanGtkMapPlanner для предпросмотра генерируемых галсов
 *
 * Устанавливает слой, в котором будет происходить предварительный просмотр генерируемых галсов.
 */
void
hyscan_gtk_planner_shift_set_viewer (HyScanGtkPlannerShift    *shift,
                                     HyScanGtkMapPlanner      *viewer)
{
  HyScanGtkPlannerShiftPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_SHIFT (shift));
  priv = shift->priv;

  if (priv->viewer != NULL)
    {
      hyscan_gtk_map_planner_set_preview (priv->viewer, NULL);
      g_clear_object (&priv->viewer);
    }

 if (viewer == NULL)
   return;

  priv->viewer = g_object_ref (viewer);
  hyscan_gtk_map_planner_set_preview (priv->viewer, priv->tracks);
}

/**
 * hyscan_gtk_planner_shift_set_track:
 * @shift: указатель на #HyScanGtkPlannerShift
 * @track: (nullable): опорный галс
 * @zone: (nullable): зона #HyScanPlannerZone
 *
 * Устанавливает опорный план галса, для которого надо создавать параллельные,
 * и зону, в периметре которой должны находиться сгенерированные галсы.
 */
void
hyscan_gtk_planner_shift_set_track (HyScanGtkPlannerShift    *shift,
                                    const HyScanPlannerTrack *track,
                                    const HyScanPlannerZone  *zone)
{
  HyScanGtkPlannerShiftPrivate *priv;
  HyScanTrackPlan *plan;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_SHIFT (shift));
  priv = shift->priv;

  g_clear_object (&priv->geo);
  g_clear_pointer (&priv->track, hyscan_planner_track_free);
  g_clear_pointer (&priv->vertices, g_free);
  priv->vertices_len = 0;

  if (track == NULL)
    goto exit;

  priv->track = hyscan_planner_track_copy (track);
  plan = &priv->track->plan;

  priv->geo = hyscan_planner_track_geo (plan, NULL);
  hyscan_geo_geo2topoXY (priv->geo, &priv->track_start, plan->start);
  hyscan_geo_geo2topoXY (priv->geo, &priv->track_end, plan->end);

  if (zone == NULL)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->fill_zone), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->adjust_len), FALSE);
    }
  gtk_widget_set_sensitive (priv->adjust_len, zone != NULL);
  gtk_widget_set_sensitive (priv->fill_zone, zone != NULL);

  if (zone != NULL)
    {
      gsize i;
      priv->vertices_len = zone->points_len;
      priv->vertices = g_new (HyScanGeoCartesian2D, priv->vertices_len);
      for (i = 0; i < priv->vertices_len; ++i)
        hyscan_geo_geo2topoXY (priv->geo, &priv->vertices[i], zone->points[i]);
    }

exit:
  hyscan_gtk_planner_shift_update (shift);
}

/**
 * hyscan_gtk_planner_shift_save:
 * @shift: указатель на #HyScanGtkPlannerShift
 * @model: указатель на #HyScanObjectModel
 *
 * Записывает сгененрированные галсы в базу данных
 */
void
hyscan_gtk_planner_shift_save (HyScanGtkPlannerShift *shift,
                               HyScanObjectModel     *model)
{
  HyScanGtkPlannerShiftPrivate *priv;
  GList *link;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_SHIFT (shift));
  priv = shift->priv;

  for (link = priv->tracks; link != NULL; link = link->next)
    hyscan_object_model_add_object (model, link->data);
}
