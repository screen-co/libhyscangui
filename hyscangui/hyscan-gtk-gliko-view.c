/* hyscan-gtk-gliko-view.h
 *
 * Copyright 2019 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
 *
 * This file is part of HyScanGui.
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
 * SECTION: hyscan-gtk-gliko-view
 * @Title HyScanGtkGlikoView
 * @Short_description
 *
 */
#include <math.h>
#include <string.h>

#include <hyscan-gtk-gliko-drawing.h>
#include <hyscan-gtk-gliko.h>

#include "hyscan-gtk-gliko-view.h"
#include <hyscan-gui-marshallers.h>

enum
{
  POINTER_NONE = 0,
  POINTER_CENTER,
  POINTER_RULER_FROM,
  POINTER_RULER_TO,
  POINTER_RULER_LINE
};

enum
{
  SIGNAL_RULER_CHANGED,
  SIGNAL_LAST,
};

struct _HyScanGtkGlikoViewPrivate
{
  PangoLayout *pango_layout;
  HyScanGtkGlikoDrawing *drawing_layer;
  gint drawing_width;
  gint drawing_height;

  gdouble point_a_z;
  gdouble point_a_r;
  gdouble point_b_z;
  gdouble point_b_r;
  gdouble point_ab_z;
  gdouble point_ba_z;
  int pointer_move;

  gdouble sxc0;
  gdouble syc0;
  gdouble mx0;
  gdouble my0;
  gdouble mx1, my1, mx2, my2;
};

static void dispose (GObject *gobject);
static void finalize (GObject *gobject);
static void draw_view (HyScanGtkGlikoView *self);

static void hyscan_gtk_gliko_view_button (HyScanGtkGlikoView *self,
                                          GdkEventButton *event);
static void hyscan_gtk_gliko_view_motion (HyScanGtkGlikoView *self,
                                          GdkEventMotion *event);
static void hyscan_gtk_gliko_view_scroll (HyScanGtkGlikoView *self,
                                          GdkEventScroll *event);
static void parameters_changed_callback (HyScanGtkGliko *self);
static void resize_callback (GtkWidget *widget,
                             gint width,
                             gint height,
                             gpointer user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkGlikoView, hyscan_gtk_gliko_view, HYSCAN_TYPE_GTK_GLIKO);

static const int ruler_size = 6;
static guint gliko_view_signal[SIGNAL_LAST] = { 0 };

static void
hyscan_gtk_gliko_view_class_init (HyScanGtkGlikoViewClass *klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  //GtkWidgetClass *w_class = GTK_WIDGET_CLASS( klass );

  /* Add private data */
  g_type_class_add_private (klass, sizeof (HyScanGtkGlikoViewPrivate));

  g_class->dispose = dispose;
  g_class->finalize = finalize;

  gliko_view_signal[SIGNAL_RULER_CHANGED] =
      g_signal_new ("ruler-changed", HYSCAN_TYPE_GTK_GLIKO_VIEW,
                    G_SIGNAL_RUN_LAST, 0,
                    NULL, NULL,
                    hyscan_gui_marshal_VOID__DOUBLE_DOUBLE_DOUBLE_DOUBLE,
                    G_TYPE_NONE,
                    4, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void
hyscan_gtk_gliko_view_init (HyScanGtkGlikoView *self)
{
  HyScanGtkGlikoViewPrivate *p;
  gint event_mask = 0;
  int i;

  self->priv = hyscan_gtk_gliko_view_get_instance_private (self);
  p = self->priv;

  p->pango_layout = NULL;
  p->drawing_layer = NULL;
  p->drawing_width = -1;
  p->drawing_height = -1;

  p->point_a_z = 0.0;
  p->point_a_r = 0.0;
  p->point_b_z = 0.0;
  p->point_b_r = 10.0;
  p->point_ab_z = 0.0;
  p->point_ba_z = 180.0;

  p->pointer_move = POINTER_NONE;

  /* ищем доступный слой для отрисовки линейки */
  for (i = 0; i < HYSCAN_GTK_GLIKO_LAYER_MAX; i++)
    {
      HyScanGtkGlikoLayer *layer = NULL;

      if (hyscan_gtk_gliko_overlay_get_layer (HYSCAN_GTK_GLIKO_OVERLAY (self), i, &layer))
        {
          if (layer == NULL)
            {
              p->drawing_layer = hyscan_gtk_gliko_drawing_new ();

              hyscan_gtk_gliko_overlay_set_layer (HYSCAN_GTK_GLIKO_OVERLAY (self), i, HYSCAN_GTK_GLIKO_LAYER (p->drawing_layer));
              hyscan_gtk_gliko_overlay_enable_layer (HYSCAN_GTK_GLIKO_OVERLAY (self), i, 1);

              p->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "A");
              break;
            }
        }
    }

  event_mask |= GDK_BUTTON_PRESS_MASK;
  event_mask |= GDK_BUTTON_RELEASE_MASK;
  event_mask |= GDK_POINTER_MOTION_MASK;
  event_mask |= GDK_POINTER_MOTION_HINT_MASK;
  event_mask |= GDK_SCROLL_MASK;

  gtk_widget_add_events (GTK_WIDGET (self), event_mask);

  g_signal_connect (self, "button_press_event", G_CALLBACK (hyscan_gtk_gliko_view_button), NULL);
  g_signal_connect (self, "button_release_event", G_CALLBACK (hyscan_gtk_gliko_view_button), NULL);
  g_signal_connect (self, "motion_notify_event", G_CALLBACK (hyscan_gtk_gliko_view_motion), NULL);
  g_signal_connect (self, "scroll_event", G_CALLBACK (hyscan_gtk_gliko_view_scroll), NULL);
  g_signal_connect (self, "resize", G_CALLBACK (resize_callback), NULL);

  g_signal_connect (HYSCAN_GTK_GLIKO (self), "parameters-changed", G_CALLBACK (parameters_changed_callback), self);
}

static int
ruler_node_catch (HyScanGtkGlikoView *self, const gdouble a, const gdouble r, const gdouble x, const gdouble y)
{
  gdouble xn, yn;

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), a, r, &xn, &yn);
  if (x >= (xn - ruler_size) &&
      x < (xn + ruler_size) &&
      y >= (yn - ruler_size) &&
      y < (yn + ruler_size))
    {
      return 1;
    }
  return 0;
}

static int
ruler_line_catch (HyScanGtkGlikoView *self, const gdouble x, const gdouble y)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;
  gdouble x1, y1, x2, y2, d1, d2, d;

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_a_z, p->point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_b_z, p->point_b_r, &x2, &y2);

  d = sqrt ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  d1 = sqrt ((x - x1) * (x - x1) + (y - y1) * (y - y1));
  d2 = sqrt ((x - x2) * (x - x2) + (y - y2) * (y - y2));

  if (d1 > d || d2 > d || d < 1.0f)
    return 0;

  // расстояние от точки до отразка
  d = fabs ((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1) / d;
  if (d >= (0.95f * ruler_size))
    return 0;
  return 1;
}

static void
hyscan_gtk_gliko_view_button (HyScanGtkGlikoView *self,
                              GdkEventButton *event)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;

  if (event->button == 1)
    {
      if (event->type == GDK_BUTTON_PRESS)
        {
          if (ruler_node_catch (self, p->point_a_z, p->point_a_r, event->x, event->y))
            {
              p->pointer_move = POINTER_RULER_FROM;
            }
          else if (ruler_node_catch (self, p->point_b_z, p->point_b_r, event->x, event->y))
            {
              p->pointer_move = POINTER_RULER_TO;
            }
          else if (ruler_line_catch (self, event->x, event->y))
            {
              p->pointer_move = POINTER_RULER_LINE;
              hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_a_z, p->point_a_r, &p->mx1, &p->my1);
              hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_b_z, p->point_b_r, &p->mx2, &p->my2);
              p->mx0 = event->x;
              p->my0 = event->y;
            }
          else
            {
              p->pointer_move = POINTER_CENTER;
              hyscan_gtk_gliko_get_center (HYSCAN_GTK_GLIKO (self), &p->sxc0, &p->syc0);
              p->mx0 = event->x;
              p->my0 = event->y;
            }
        }
      else if (event->type == GDK_BUTTON_RELEASE)
        {
          p->pointer_move = 0;
        }
    }
  else if (event->button == 2)
    {
      hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (self), 1.0);
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (self), 0.0, 0.0);
    }
}

static void
hyscan_gtk_gliko_view_motion (HyScanGtkGlikoView *self,
                              GdkEventMotion *event)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;

  GtkAllocation allocation;
  int baseline;
  gdouble s, x, y;

  if (p->pointer_move == POINTER_CENTER)
    {
      gtk_widget_get_allocated_size (GTK_WIDGET (self), &allocation, &baseline);

      s = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (self));
      x = p->sxc0 - s * (p->mx0 - (gdouble) event->x) / allocation.height;
      y = p->syc0 - s * ((gdouble) event->y - p->my0) / allocation.height;
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (self), x, y);
      draw_view (self);
    }
  else if (p->pointer_move == POINTER_RULER_FROM)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (self), event->x, event->y, &p->point_a_z, &p->point_a_r);
      draw_view (self);
      g_signal_emit (self, gliko_view_signal[SIGNAL_RULER_CHANGED], 0, p->point_a_z, p->point_a_r, p->point_b_z, p->point_b_r);
    }
  else if (p->pointer_move == POINTER_RULER_TO)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (self), event->x, event->y, &p->point_b_z, &p->point_b_r);
      draw_view (self);
      g_signal_emit (self, gliko_view_signal[SIGNAL_RULER_CHANGED], 0, p->point_a_z, p->point_a_r, p->point_b_z, p->point_b_r);
    }
  else if (p->pointer_move == POINTER_RULER_LINE)
    {
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (self), p->mx1 + (event->x - p->mx0), p->my1 + (event->y - p->my0), &p->point_a_z, &p->point_a_r);
      hyscan_gtk_gliko_pixel2polar (HYSCAN_GTK_GLIKO (self), p->mx2 + (event->x - p->mx0), p->my2 + (event->y - p->my0), &p->point_b_z, &p->point_b_r);
      draw_view (self);
      g_signal_emit (self, gliko_view_signal[SIGNAL_RULER_CHANGED], 0, p->point_a_z, p->point_a_r, p->point_b_z, p->point_b_r);
    }
}

static void
hyscan_gtk_gliko_view_scroll (HyScanGtkGlikoView *self,
                              GdkEventScroll *event)
{
  gdouble f;

  if (event->direction == GDK_SCROLL_UP)
    {
      f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (self));
      hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (self), 0.875 * f);
      draw_view (self);
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (self));
      hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (self), 1.125 * f);
      draw_view (self);
    }
}

static gdouble
range360 (const gdouble a)
{
  guint i;
  gdouble d;

  d = a * 65536.0 / 360.0;
  i = (int) d;
  d = (i & 0xFFFF);
  return d * 360.0 / 65536.0;
}

static void
ruler_caption (HyScanGtkGlikoView *self, cairo_t *cr, const double x1, const double y1, const double a, const char *caption, const int length)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;
  double z, w, h, x, y;
  PangoRectangle inc_rect, logical_rect;
  const double margin = 4;
  gdouble b = hyscan_gtk_gliko_get_full_rotation (HYSCAN_GTK_GLIKO (self));

  pango_layout_set_text (p->pango_layout, caption, length);
  pango_layout_get_pixel_extents (p->pango_layout, &inc_rect, &logical_rect);
  w = logical_rect.width;
  h = logical_rect.height;

  z = range360 (a - 45.0 + b);

  switch ((int) (z / 90.0))
    {
    case 0: // угол 45-135, надпись слева
      x = x1 - ruler_size - w - margin;
      y = y1 - 0.5 * h;
      break;
    case 1: // угол 135-225, надпись сверху
      x = x1 - 0.5 * w;
      y = y1 - ruler_size - h - margin;
      break;
    case 2: // угол 225-315, надпись справа
      x = x1 + ruler_size + margin;
      y = y1 - 0.5 * h;
      break;
    default: // угол 315-45, надпись снизу
      x = x1 - 0.5 * w;
      y = y1 + ruler_size + margin;
      break;
    }
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);
  cairo_rectangle (cr, x - 2, y, w + 4, h);
  cairo_fill (cr);
  cairo_stroke (cr);
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  pango_cairo_show_layout (cr, p->pango_layout);
  cairo_stroke (cr);
}

static void
draw_step_distance (HyScanGtkGlikoView *self, cairo_t *cr)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;
  gdouble step;
  guint width, height;
  gdouble x1, y1, x2, y2, d;
  double w, h, x, y;
  const double margin = 4;
  PangoRectangle inc_rect, logical_rect;
  char t[32];

  step = hyscan_gtk_gliko_get_step_distance (HYSCAN_GTK_GLIKO (self));

  width = gtk_widget_get_allocated_width (GTK_WIDGET (self));
  height = gtk_widget_get_allocated_height (GTK_WIDGET (self));

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), 90.0, 0.0, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), 90.0, step, &x2, &y2);

  d = sqrt ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));

  if (step < 1.0)
    {
      sprintf (t, "%.1f", step);
    }
  else
    {
      sprintf (t, "%.0f", step);
    }

  pango_layout_set_text (p->pango_layout, t, strlen (t));
  pango_layout_get_pixel_extents (p->pango_layout, &inc_rect, &logical_rect);
  w = logical_rect.width;
  h = logical_rect.height;

  x2 = width - margin;
  x1 = x2 - d;
  y1 = height - margin - margin;

  x = x1 + 0.5 * (d - w);
  y = y1 - margin - h;

  // рисуем затенение
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);

  cairo_set_line_width (cr, 4.0);
  cairo_move_to (cr, x1, y1 - margin);
  cairo_line_to (cr, x1, y1);
  cairo_line_to (cr, x2, y1);
  cairo_line_to (cr, x2, y1 - margin);
  cairo_stroke (cr);

  // рисуем отрезок
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);

  cairo_set_line_width (cr, 2.0);
  cairo_move_to (cr, x1, y1 - margin);
  cairo_line_to (cr, x1, y1);
  cairo_line_to (cr, x2, y1);
  cairo_line_to (cr, x2, y1 - margin);
  cairo_stroke (cr);

  // рисуем текст
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);
  cairo_rectangle (cr, x - 2, y, w + 4, h);
  cairo_fill (cr);
  cairo_stroke (cr);
  cairo_move_to (cr, x, y);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  pango_cairo_show_layout (cr, p->pango_layout);
  cairo_stroke (cr);
}

static void
draw_ruler (HyScanGtkGlikoView *self, cairo_t *cr)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;
  gint b = ruler_size * 2 / 3;
  gdouble x1, y1, x2, y2;
  const gdouble radians_to_degrees = 180.0 / G_PI;
  gdouble a;

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_a_z, p->point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_b_z, p->point_b_r, &x2, &y2);

  a = hyscan_gtk_gliko_get_full_rotation (HYSCAN_GTK_GLIKO (self));
  p->point_ab_z = range360 (atan2 (x2 - x1, y1 - y2) * radians_to_degrees - a);
  p->point_ba_z = range360 (atan2 (x1 - x2, y2 - y1) * radians_to_degrees - a);

  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_a_z, p->point_a_r, &x1, &y1);
  hyscan_gtk_gliko_polar2pixel (HYSCAN_GTK_GLIKO (self), p->point_b_z, p->point_b_r, &x2, &y2);

  // рисуем затенение
  cairo_set_source_rgba (cr, 0, 0, 0, 0.25);

  cairo_set_line_width (cr, 4.0);
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_stroke (cr);

  cairo_rectangle (cr, x1 - ruler_size, y1 - ruler_size, ruler_size + ruler_size, ruler_size + ruler_size);
  cairo_fill (cr);
  cairo_stroke (cr);

  cairo_rectangle (cr, x2 - ruler_size, y2 - ruler_size, ruler_size + ruler_size, ruler_size + ruler_size);
  cairo_fill (cr);
  cairo_stroke (cr);

  // рисуем отрезок
  cairo_set_source_rgba (cr, 1, 1, 1, 0.5);

  cairo_set_line_width (cr, 2.0);
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_stroke (cr);

  cairo_rectangle (cr, x1 - b, y1 - b, b + b, b + b);
  cairo_fill (cr);
  cairo_stroke (cr);

  cairo_rectangle (cr, x2 - b, y2 - b, b + b, b + b);
  cairo_fill (cr);
  cairo_stroke (cr);

  ruler_caption (self, cr, x1, y1, p->point_ab_z, "A", 1);
  ruler_caption (self, cr, x2, y2, p->point_ba_z, "B", 1);

  draw_step_distance (self, cr);
}

static void
draw_view (HyScanGtkGlikoView *self)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;

  if (p->drawing_layer != NULL)
    {
      cairo_t *cr = hyscan_gtk_gliko_drawing_begin (p->drawing_layer);
      if (cr != NULL)
        {
          draw_ruler (self, cr);
          hyscan_gtk_gliko_drawing_end (p->drawing_layer);
        }
    }
}

static void
parameters_changed_callback (HyScanGtkGliko *gliko)
{
  draw_view (HYSCAN_GTK_GLIKO_VIEW (gliko));
}

static void
resize_callback (GtkWidget *widget, gint width, gint height, gpointer user_data)
{
  HyScanGtkGlikoView *self = HYSCAN_GTK_GLIKO_VIEW (widget);
  HyScanGtkGlikoViewPrivate *p = self->priv;

  if (width == p->drawing_width && height == p->drawing_height)
    return;

  p->drawing_width = width;
  p->drawing_height = height;

  draw_view (self);
  g_signal_emit (self, gliko_view_signal[SIGNAL_RULER_CHANGED], 0, p->point_a_z, p->point_a_r, p->point_b_z, p->point_b_r);
}

HYSCAN_API
void
hyscan_gtk_gliko_view_get_ruler (HyScanGtkGlikoView *self, double *az, double *ar, double *bz, double *br)
{
  HyScanGtkGlikoViewPrivate *p = self->priv;

  if (az != NULL)
    *az = p->point_a_z;
  if (ar != NULL)
    *ar = p->point_a_r;
  if (bz != NULL)
    *az = p->point_b_z;
  if (br != NULL)
    *az = p->point_b_r;
}

static void
dispose (GObject *gobject)
{
  G_OBJECT_CLASS (hyscan_gtk_gliko_view_parent_class)
      ->dispose (gobject);
}

static void
finalize (GObject *gobject)
{
  HyScanGtkGlikoViewPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (gobject, HYSCAN_TYPE_GTK_GLIKO_VIEW, HyScanGtkGlikoViewPrivate);

  g_clear_object (&p->drawing_layer);

  G_OBJECT_CLASS (hyscan_gtk_gliko_view_parent_class)
      ->finalize (gobject);
}

HyScanGtkGlikoView *
hyscan_gtk_gliko_view_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_GLIKO_VIEW, NULL);
}
