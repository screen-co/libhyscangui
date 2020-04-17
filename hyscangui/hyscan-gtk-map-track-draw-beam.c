/* hyscan-gtk-map-track-draw-beam.c
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
 * SECTION: hyscan-gtk-map-track-draw-beam
 * @Short_description: рисование галса в виде зоны покрытия
 * @Title: HyScanGtkMapTrackDrawBeam
 * @See_also: #HyScanGtkMapTrack
 *
 * Класс рисует зону покрытия галса как совокупность всех зондирований на галсе,
 * представляя каждый луч в виде прямоугольной ближней зоны и расходящейся дальней зоны.
 *
 */

#include "hyscan-gtk-map-track-draw-beam.h"
#include "hyscan-gtk-layer-param.h"
#include <hyscan-cartesian.h>
#include <gdk/gdk.h>
#include <hyscan-track-proj-quality.h>

struct _HyScanGtkMapTrackDrawBeamPrivate
{
  GdkRGBA              color;        /* Цвет покрытия. */
  GMutex               lock;         /* Мьютекс для блокировки доступа к color. */
  HyScanGtkLayerParam *param;        /* Параметры отрисовщика. */
};

static void    hyscan_gtk_map_track_draw_beam_interface_init           (HyScanGtkMapTrackDrawInterface *iface);
static void    hyscan_gtk_map_track_draw_beam_object_constructed       (GObject                        *object);
static void    hyscan_gtk_map_track_draw_beam_object_finalize          (GObject                        *object);
static void    hyscan_gtk_map_track_draw_beam_emit                     (HyScanGtkMapTrackDrawBeam      *draw_beam);
static void    hyscan_gtk_map_track_draw_beam_side                     (HyScanGtkMapTrackDrawBeam      *beam,
                                                                        HyScanTrackProjQuality         *quality_data,
                                                                        HyScanGeoCartesian2D           *from,
                                                                        HyScanGeoCartesian2D           *to,
                                                                        gdouble                         scale,
                                                                        GList                          *points,
                                                                        cairo_t                        *cairo,
                                                                        GCancellable                   *cancellable);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackDrawBeam, hyscan_gtk_map_track_draw_beam, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackDrawBeam)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, hyscan_gtk_map_track_draw_beam_interface_init))

static void
hyscan_gtk_map_track_draw_beam_class_init (HyScanGtkMapTrackDrawBeamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_track_draw_beam_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_draw_beam_object_finalize;
}

static void
hyscan_gtk_map_track_draw_beam_init (HyScanGtkMapTrackDrawBeam *gtk_map_track_draw_beam)
{
  gtk_map_track_draw_beam->priv = hyscan_gtk_map_track_draw_beam_get_instance_private (gtk_map_track_draw_beam);
}

static void
hyscan_gtk_map_track_draw_beam_object_constructed (GObject *object)
{
  HyScanGtkMapTrackDrawBeam *draw_beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (object);
  HyScanGtkMapTrackDrawBeamPrivate *priv = draw_beam->priv;

  g_mutex_init (&priv->lock);

  /* Связываем переменные с параметрами оформления трека. */
  priv->param = hyscan_gtk_layer_param_new_with_lock (&priv->lock);
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-track-beam");
  hyscan_gtk_layer_param_add_rgba (priv->param, "/beam-color", &priv->color);
  g_signal_connect_swapped (priv->param, "set", G_CALLBACK (hyscan_gtk_map_track_draw_beam_emit), draw_beam);
}

static void
hyscan_gtk_map_track_draw_beam_object_finalize (GObject *object)
{
  HyScanGtkMapTrackDrawBeam *gtk_map_track_draw_beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (object);
  HyScanGtkMapTrackDrawBeamPrivate *priv = gtk_map_track_draw_beam->priv;

  g_object_unref (priv->param);
  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_gtk_map_track_draw_beam_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_track_draw_beam_emit (HyScanGtkMapTrackDrawBeam *draw_beam)
{
  g_signal_emit_by_name (draw_beam, "param-changed");
}

/* Рисует часть галса по точкам points. */
static void
hyscan_gtk_map_track_draw_beam_side (HyScanGtkMapTrackDrawBeam *beam,
                                     HyScanTrackProjQuality    *quality_data,
                                     HyScanGeoCartesian2D      *from,
                                     HyScanGeoCartesian2D      *to,
                                     gdouble                    scale,
                                     GList                     *points,
                                     cairo_t                   *cairo,
                                     GCancellable              *cancellable)
{
  HyScanGtkMapTrackDrawBeamPrivate *priv = beam->priv;
  GList *point_l;
  HyScanMapTrackPoint *point;
  HyScanGeoCartesian2D start, nf, ff1, ff2;

  if (points == NULL)
    return;

  cairo_save (cairo);

  /* Делаем не наложение цвета, а замену (это сохранит полупрозрачность цвета при наложении изображений). */
  cairo_set_operator (cairo, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_rgba (cairo, &priv->color);

  /* Чтобы соседние полосы точно совпадали, убираем сглаживание. */
  cairo_set_antialias (cairo, CAIRO_ANTIALIAS_NONE);

  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      gdouble nr_part, dx1, dy1, dx2, dy2, dx_nf, dy_nf;

      point = point_l->data;

      const HyScanTrackCovSection *quality_values;
      gsize quality_len;

      hyscan_track_proj_quality_get (quality_data, point->index, &quality_values, &quality_len);
      if (point->b_dist <= 0 || quality_len == 0)
        continue;

      if (g_cancellable_is_cancelled (cancellable))
        break;

      /* Координаты точки на поверхности cairo. */
      hyscan_gtk_map_track_draw_scale (&point->start_c2d, from, to, scale, &start);
      hyscan_gtk_map_track_draw_scale (&point->fr1_c2d, from, to, scale, &ff1);
      hyscan_gtk_map_track_draw_scale (&point->fr2_c2d, from, to, scale, &ff2);
      hyscan_gtk_map_track_draw_scale (&point->nr_c2d, from, to, scale, &nf);

      nr_part = point->nr_length_m / point->b_length_m;
      dx1 = ff1.x - start.x;
      dy1 = ff1.y - start.y;
      dx2 = ff2.x - start.x;
      dy2 = ff2.y - start.y;
      dx_nf = (nf.x - start.x) / nr_part;
      dy_nf = (nf.y - start.y) / nr_part;
      for (guint i = 0; i < quality_len - 1; i++)
        {
          const HyScanTrackCovSection *section = &quality_values[i];
          gdouble s0 = section->start;
          gdouble s1 = quality_values[i + 1].start;

          if (section->quality == 0.0)
            continue;

          /* Ближняя зона. */
          if (s0 < nr_part)
            {
              HyScanGeoCartesian2D pt_nr[2];
              gdouble nr0 = s0, nr1 = MIN (nr_part, s1);

              pt_nr[0].x = start.x + nr0 * dx_nf;
              pt_nr[0].y = start.y + nr0 * dy_nf;
              pt_nr[1].x = start.x + nr1 * dx_nf;
              pt_nr[1].y = start.y + nr1 * dy_nf;

              cairo_move_to (cairo, pt_nr[0].x, pt_nr[0].y);
              cairo_line_to (cairo, pt_nr[1].x, pt_nr[1].y);
              cairo_set_line_width (cairo, point->aperture / scale);
              cairo_stroke (cairo);
            }

          /* Дальняя зона. */
          if (s1 > nr_part)
            {
              HyScanGeoCartesian2D pt_fr[4];
              gdouble fr0 = MAX (s0, nr_part), fr1 = s1;

              pt_fr[0].x = start.x + fr0 * dx1;
              pt_fr[0].y = start.y + fr0 * dy1;
              pt_fr[1].x = start.x + fr1 * dx1;
              pt_fr[1].y = start.y + fr1 * dy1;
              pt_fr[2].x = start.x + fr1 * dx2;
              pt_fr[2].y = start.y + fr1 * dy2;
              pt_fr[3].x = start.x + fr0 * dx2;
              pt_fr[3].y = start.y + fr0 * dy2;

              cairo_move_to (cairo, pt_fr[0].x, pt_fr[0].y);
              cairo_line_to (cairo, pt_fr[1].x, pt_fr[1].y);
              cairo_line_to (cairo, pt_fr[2].x, pt_fr[2].y);
              cairo_line_to (cairo, pt_fr[3].x, pt_fr[3].y);
              cairo_close_path (cairo);
              cairo_fill (cairo);
            }
        }
    }

  cairo_restore (cairo);
}

static void
hyscan_gtk_map_track_draw_beam_draw_region (HyScanGtkMapTrackDraw     *track_draw,
                                            HyScanTrackProjQuality    *quality_data,
                                            HyScanMapTrackData       *data,
                                            cairo_t                   *cairo,
                                            gdouble                    scale,
                                            HyScanGeoCartesian2D      *from,
                                            HyScanGeoCartesian2D      *to,
                                            GCancellable              *cancellable)
{
  HyScanGtkMapTrackDrawBeam *beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (track_draw);

  hyscan_gtk_map_track_draw_beam_side (beam, quality_data, from, to, scale, data->port, cairo, cancellable);
  hyscan_gtk_map_track_draw_beam_side (beam, quality_data, from, to, scale, data->starboard, cairo, cancellable);
}

static HyScanParam *
hyscan_gtk_map_track_draw_beam_get_param (HyScanGtkMapTrackDraw *track_draw)
{
  HyScanGtkMapTrackDrawBeam *draw_beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (track_draw);
  HyScanGtkMapTrackDrawBeamPrivate *priv = draw_beam->priv;

  return g_object_ref (HYSCAN_PARAM (priv->param));
}

static void
hyscan_gtk_map_track_draw_beam_interface_init (HyScanGtkMapTrackDrawInterface *iface)
{
  iface->draw_region = hyscan_gtk_map_track_draw_beam_draw_region;
  iface->get_param = hyscan_gtk_map_track_draw_beam_get_param;
}

/**
 * hyscan_gtk_map_track_draw_beam_new:
 *
 * Функция создаёт объект для рисования покрытия галса.
 *
 * Returns: (transfer full): новый объект #HyScanGtkMapTrackDraw, для удаления g_object_unref().
 */
HyScanGtkMapTrackDraw *
hyscan_gtk_map_track_draw_beam_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW_BEAM, NULL);
}
