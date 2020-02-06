/* hyscan-gtk-map-track-draw-bar.c
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
 * SECTION: hyscan-gtk-map-track-draw-bar
 * @Short_description: рисование галса в виде линий дальности
 * @Title: HyScanGtkMapTrackDrawBar
 * @See_also: #HyScanGtkMapTrack
 *
 * Класс для рисования галса на карте в виде линий дальности и траектории движения
 * судна.
 *
 */

#include <hyscan-cartesian.h>
#include <math.h>
#include "hyscan-gtk-map-track-draw-bar.h"
#include "hyscan-gtk-layer-param.h"

#define DEFAULT_COLOR_SHADOW          "rgba(150, 150, 150, 0.5)"    /* Цвет затенения. */
#define DEFAULT_LINE_WIDTH            1                             /* Толщина линии движения. */
#define DEFAULT_STROKE_WIDTH          1.0                           /* Толщина обводки. */

typedef struct
{
  GdkRGBA                             color_left;                   /* Цвет левого борта. */
  GdkRGBA                             color_right;                  /* Цвет правого борта. */
  GdkRGBA                             color_track;                  /* Цвет линии движения. */
  GdkRGBA                             color_shadow;                 /* Цвет затенения (полупрозрачный чёрный). */
  gdouble                             bar_width;                    /* Толщина линии дальности. */
  gdouble                             bar_margin;                   /* Расстояние между соседними линиями дальности. */
  gdouble                             line_width;                   /* Толщина линии движения. */
  gdouble                             stroke_width;                 /* Толщина линии обводки. */
} HyScanGtkMapTrackDrawBarStyle;

struct _HyScanGtkMapTrackDrawBarPrivate
{
  HyScanGtkMapTrackDrawBarStyle       style;                        /* Стиль оформления. */
  GMutex                              lock;                         /* Мьютекс для доступа к полю style. */
  HyScanGtkLayerParam                *param;                        /* HyScanParam для стилей оформления. */
};

static void    hyscan_gtk_map_track_draw_bar_interface_init           (HyScanGtkMapTrackDrawInterface *iface);
static void    hyscan_gtk_map_track_draw_bar_object_constructed       (GObject                        *object);
static void    hyscan_gtk_map_track_draw_bar_object_finalize          (GObject                        *object);
static void    hyscan_gtk_map_track_draw_bar_emit                     (HyScanGtkMapTrackDrawBar       *draw_bar);
static void    hyscan_gtk_map_track_draw_bar_path                     (HyScanGeoCartesian2D           *from,
                                                                       HyScanGeoCartesian2D           *to,
                                                                       gdouble                         scale,
                                                                       GList                          *points,
                                                                       cairo_t                        *cairo,
                                                                       HyScanGtkMapTrackDrawBarStyle  *style);
static void    hyscan_gtk_map_track_draw_bar_side                     (HyScanGeoCartesian2D           *from,
                                                                       HyScanGeoCartesian2D           *to,
                                                                       gdouble                         scale,
                                                                       GList                          *points,
                                                                       cairo_t                        *cairo,
                                                                       HyScanGtkMapTrackDrawBarStyle  *style);
static void    hyscan_gtk_map_track_draw_bar_start                    (HyScanGeoCartesian2D           *from,
                                                                       HyScanGeoCartesian2D           *to,
                                                                       gdouble                         scale,
                                                                       GList                          *points,
                                                                       cairo_t                        *cairo,
                                                                       HyScanGtkMapTrackDrawBarStyle  *style);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackDrawBar, hyscan_gtk_map_track_draw_bar, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackDrawBar)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, hyscan_gtk_map_track_draw_bar_interface_init))

static void
hyscan_gtk_map_track_draw_bar_class_init (HyScanGtkMapTrackDrawBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_track_draw_bar_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_draw_bar_object_finalize;
}

static void
hyscan_gtk_map_track_draw_bar_init (HyScanGtkMapTrackDrawBar *gtk_map_track_draw_bar)
{
  gtk_map_track_draw_bar->priv = hyscan_gtk_map_track_draw_bar_get_instance_private (gtk_map_track_draw_bar);
}

static void
hyscan_gtk_map_track_draw_bar_object_constructed (GObject *object)
{
  HyScanGtkMapTrackDrawBar *draw_bar = HYSCAN_GTK_MAP_TRACK_DRAW_BAR (object);
  HyScanGtkMapTrackDrawBarPrivate *priv = draw_bar->priv;
  HyScanGtkMapTrackDrawBarStyle *style = &priv->style;

  G_OBJECT_CLASS (hyscan_gtk_map_track_draw_bar_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);

  /* Оформление трека по умолчанию. */
  style->line_width = DEFAULT_LINE_WIDTH;
  style->stroke_width = DEFAULT_STROKE_WIDTH;
  gdk_rgba_parse (&style->color_shadow, DEFAULT_COLOR_SHADOW);

  priv->param = hyscan_gtk_layer_param_new_with_lock (&priv->lock);
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-track");
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (priv->param), "/bar-width", &style->bar_width);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (priv->param), "/bar-margin", &style->bar_margin);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/track-color", &style->color_track);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/port-color", &style->color_left);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/starboard-color", &style->color_right);
  g_signal_connect_swapped (priv->param, "set", G_CALLBACK (hyscan_gtk_map_track_draw_bar_emit), draw_bar);
}

static void
hyscan_gtk_map_track_draw_bar_object_finalize (GObject *object)
{
  HyScanGtkMapTrackDrawBar *gtk_map_track_draw_bar = HYSCAN_GTK_MAP_TRACK_DRAW_BAR (object);
  HyScanGtkMapTrackDrawBarPrivate *priv = gtk_map_track_draw_bar->priv;

  g_object_unref (priv->param);
  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_gtk_map_track_draw_bar_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_track_draw_bar_emit (HyScanGtkMapTrackDrawBar *draw_bar)
{
  g_signal_emit_by_name (draw_bar, "param-changed");
}

/* Рисует часть галса по точкам points. */
static void
hyscan_gtk_map_track_draw_bar_path (HyScanGeoCartesian2D          *from,
                                    HyScanGeoCartesian2D          *to,
                                    gdouble                        scale,
                                    GList                         *points,
                                    cairo_t                       *cairo,
                                    HyScanGtkMapTrackDrawBarStyle *style)
{
  GList *point_l;
  HyScanGtkMapTrackPoint *point;
  HyScanGeoCartesian2D coord;

  /* Рисуем линию движения. */
  gdk_cairo_set_source_rgba (cairo, &style->color_track);
  cairo_set_line_width (cairo, style->line_width);
  cairo_new_path (cairo);
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      /* Координаты точки на поверхности cairo. */
      hyscan_gtk_map_track_draw_scale (&point->ship_c2d, from, to, scale, &coord);

      cairo_line_to (cairo, coord.x, coord.y);
    }
  cairo_stroke (cairo);
}

/* Рисует точки одного из бортов по точкам points. */
static void
hyscan_gtk_map_track_draw_bar_side (HyScanGeoCartesian2D          *from,
                                    HyScanGeoCartesian2D          *to,
                                    gdouble                        scale,
                                    GList                         *points,
                                    cairo_t                       *cairo,
                                    HyScanGtkMapTrackDrawBarStyle *style)
{
  GList *point_l;
  HyScanGtkMapTrackPoint *point, *next_point;
  GdkRGBA *fill_color;

  gdouble threshold;

  /* Делим весь отрезок на зоны длины threshold. В каждой зоне одна полоса. */
  threshold = scale * (style->bar_margin + style->bar_width);

  /* Рисуем полосы от бортов. */
  cairo_set_line_width (cairo, style->bar_width);
  cairo_new_path (cairo);
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGeoCartesian2D ship, start, end;

      point = point_l->data;
      next_point = point_l->next ? point_l->next->data : NULL;

      /* Пропускаем полосы на криволинейных участках. */
      // if (!point->straight)
      //   continue;

      if (point->b_dist <= 0)
        continue;

      /* Рисуем полосу, только если следующая полоса лежит в другой зоне. */
      if (next_point == NULL || round (next_point->dist_along / threshold) == round (point->dist_along / threshold))
        continue;

      hyscan_gtk_map_track_draw_scale (&point->ship_c2d, from, to, scale, &ship);
      hyscan_gtk_map_track_draw_scale (&point->start_c2d, from, to, scale, &start);
      hyscan_gtk_map_track_draw_scale (&point->fr_c2d, from, to, scale, &end);

      if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT)
        fill_color = &style->color_left;
      else
        fill_color = &style->color_right;

      /* Линия дальности. */
      cairo_move_to (cairo, start.x, start.y);
      cairo_line_to (cairo, end.x, end.y);
      gdk_cairo_set_source_rgba (cairo, fill_color);
      cairo_set_line_width (cairo, style->bar_width);
      cairo_stroke (cairo);

      /* Линия, соединяющая центр судна с антенной. */
      cairo_move_to (cairo, start.x, start.y);
      cairo_line_to (cairo, ship.x, ship.y);
      gdk_cairo_set_source_rgba (cairo, &style->color_shadow);
      cairo_set_line_width (cairo, style->bar_width);
      cairo_stroke (cairo);
    }
}

/* Рисует точку начала. */
static void
hyscan_gtk_map_track_draw_bar_start (HyScanGeoCartesian2D          *from,
                                     HyScanGeoCartesian2D          *to,
                                     gdouble                        scale,
                                     GList                         *points,
                                     cairo_t                       *cairo,
                                     HyScanGtkMapTrackDrawBarStyle *style)
{
  HyScanGtkMapTrackPoint *point;
  HyScanGeoCartesian2D coord;

  if (points == NULL)
    return;

  point = points->data;

  if (!hyscan_cartesian_is_point_inside (&point->ship_c2d, from, to))
    return;

  /* Координаты точки на поверхности cairo. */
  hyscan_gtk_map_track_draw_scale (&point->ship_c2d, from, to, scale, &coord);

  cairo_arc (cairo, coord.x, coord.y, 2.0 * style->line_width, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &style->color_track);
  cairo_fill (cairo);
}

static void
hyscan_gtk_map_track_draw_bar_draw_region (HyScanGtkMapTrackDraw      *track_draw,
                                           HyScanGtkMapTrackDrawData  *data,
                                           cairo_t                    *cairo,
                                           gdouble                     scale,
                                           HyScanGeoCartesian2D       *from,
                                           HyScanGeoCartesian2D       *to)
{
  HyScanGtkMapTrackDrawBar *draw_bar = HYSCAN_GTK_MAP_TRACK_DRAW_BAR (track_draw);
  HyScanGtkMapTrackDrawBarPrivate *priv = draw_bar->priv;
  HyScanGtkMapTrackDrawBarStyle style;

  g_mutex_lock (&priv->lock);
  style = priv->style;
  g_mutex_unlock (&priv->lock);

  cairo_save (cairo);
  cairo_set_antialias (cairo, CAIRO_ANTIALIAS_FAST);
  hyscan_gtk_map_track_draw_bar_side (from, to, scale, data->port, cairo, &style);
  hyscan_gtk_map_track_draw_bar_side (from, to, scale, data->starboard, cairo, &style);
  hyscan_gtk_map_track_draw_bar_path (from, to, scale, data->nav, cairo, &style);
  hyscan_gtk_map_track_draw_bar_start (from, to, scale, data->nav, cairo, &style);
  cairo_restore (cairo);
}

static HyScanParam *
hyscan_gtk_map_track_draw_bar_get_style_param (HyScanGtkMapTrackDraw *track_draw)
{
  HyScanGtkMapTrackDrawBar *draw_bar = HYSCAN_GTK_MAP_TRACK_DRAW_BAR (track_draw);
  HyScanGtkMapTrackDrawBarPrivate *priv = draw_bar->priv;

  return g_object_ref (priv->param);
}

static void
hyscan_gtk_map_track_draw_bar_interface_init (HyScanGtkMapTrackDrawInterface *iface)
{
  iface->get_param = hyscan_gtk_map_track_draw_bar_get_style_param;
  iface->draw_region = hyscan_gtk_map_track_draw_bar_draw_region;
}

HyScanGtkMapTrackDraw *
hyscan_gtk_map_track_draw_bar_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW_BAR, NULL);
}
