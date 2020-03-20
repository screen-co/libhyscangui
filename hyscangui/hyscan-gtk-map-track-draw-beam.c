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
                                     HyScanGeoCartesian2D      *from,
                                     HyScanGeoCartesian2D      *to,
                                     gdouble                    scale,
                                     GList                     *points,
                                     cairo_t                   *cairo,
                                     GCancellable              *cancellable)
{
  HyScanGtkMapTrackDrawBeamPrivate *priv = beam->priv;
  GList *point_l;
  HyScanGtkMapTrackPoint *point;
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
      point = point_l->data;

      if (point->b_dist <= 0)
        continue;

      if (g_cancellable_is_cancelled (cancellable))
        break;

      /* Координаты точки на поверхности cairo. */
      hyscan_gtk_map_track_draw_scale (&point->start_c2d, from, to, scale, &start);
      hyscan_gtk_map_track_draw_scale (&point->fr1_c2d, from, to, scale, &ff1);
      hyscan_gtk_map_track_draw_scale (&point->fr2_c2d, from, to, scale, &ff2);
      hyscan_gtk_map_track_draw_scale (&point->nr_c2d, from, to, scale, &nf);

      /* Дальняя зона. */
      cairo_move_to (cairo, start.x, start.y);
      cairo_line_to (cairo, ff1.x, ff1.y);
      cairo_line_to (cairo, ff2.x, ff2.y);
      cairo_close_path (cairo);
      cairo_fill (cairo);

      /* Ближняя зона. */
      cairo_move_to (cairo, start.x, start.y);
      cairo_line_to (cairo, nf.x, nf.y);
      cairo_set_line_width (cairo, point->aperture / scale);
      cairo_stroke (cairo);
    }

  cairo_restore (cairo);
}

static void
hyscan_gtk_map_track_draw_beam_draw_region (HyScanGtkMapTrackDraw     *track_draw,
                                            HyScanGtkMapTrackDrawData *data,
                                            cairo_t                   *cairo,
                                            gdouble                    scale,
                                            HyScanGeoCartesian2D      *from,
                                            HyScanGeoCartesian2D      *to)
{
  HyScanGtkMapTrackDrawBeam *beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (track_draw);

  hyscan_gtk_map_track_draw_beam_side (beam, from, to, scale, data->port, cairo);
  hyscan_gtk_map_track_draw_beam_side (beam, from, to, scale, data->starboard, cairo);
}

static HyScanParam *
hyscan_gtk_map_track_draw_beam_get_param (HyScanGtkMapTrackDraw *track_draw)
{
  HyScanGtkMapTrackDrawBeam *draw_beam = HYSCAN_GTK_MAP_TRACK_DRAW_BEAM (track_draw);
  HyScanGtkMapTrackDrawBeamPrivate *priv = draw_beam->priv;

  return g_object_ref (priv->param);
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
