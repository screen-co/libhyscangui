/* hyscan-gtk-gliko-control.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-gtk-gliko-control
 * @Title HyScanGtkGlikoControl
 * @Short_description
 *
 */

#include "hyscan-gtk-gliko-control.h"

enum
{
  PROP_0,
};

struct _HyScanGtkGlikoControlPrivate
{
  gboolean move;
  gdouble  sxc0;
  gdouble  syc0;
  gdouble  mx0;
  gdouble  my0;
};

static void    hyscan_gtk_gliko_control_set_property             (GObject               *object,
                                                                  guint                  prop_id,
                                                                  const GValue          *value,
                                                                  GParamSpec            *pspec);
static void    hyscan_gtk_gliko_control_object_constructed       (GObject               *object);
static void    hyscan_gtk_gliko_control_object_finalize          (GObject               *object);

static void    hyscan_gtk_gliko_control_button                   (HyScanGtkGlikoControl *self,
                                                                  GdkEventButton        *event);
static void    hyscan_gtk_gliko_control_motion                   (HyScanGtkGlikoControl *self,
                                                                  GdkEventMotion        *event);
static void    hyscan_gtk_gliko_control_scroll                   (HyScanGtkGlikoControl *self,
                                                                  GdkEventScroll        *event);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkGlikoControl, hyscan_gtk_gliko_control, G_TYPE_OBJECT);

static void
hyscan_gtk_gliko_control_class_init (HyScanGtkGlikoControlClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_gtk_gliko_control_set_property;
  oclass->constructed = hyscan_gtk_gliko_control_object_constructed;
  oclass->finalize = hyscan_gtk_gliko_control_object_finalize;
}

static void
hyscan_gtk_gliko_control_init (HyScanGtkGlikoControl *self)
{
  gint event_mask = 0;
  self->priv = hyscan_gtk_gliko_control_get_instance_private (self);

  event_mask |= GDK_BUTTON_PRESS_MASK;
  event_mask |= GDK_BUTTON_RELEASE_MASK;
  event_mask |= GDK_POINTER_MOTION_MASK;
  event_mask |= GDK_POINTER_MOTION_HINT_MASK;
  event_mask |= GDK_SCROLL_MASK;

  gtk_widget_add_events (GTK_WIDGET (self), event_mask);

  g_signal_connect (self, "button_press_event", G_CALLBACK (hyscan_gtk_gliko_control_button), NULL);
  g_signal_connect (self, "button_release_event", G_CALLBACK (hyscan_gtk_gliko_control_button), NULL);
  g_signal_connect (self, "motion_notify_event", G_CALLBACK (hyscan_gtk_gliko_control_motion), NULL);
  g_signal_connect (self, "scroll_event", G_CALLBACK (hyscan_gtk_gliko_control_scroll), NULL);
}


static void
hyscan_gtk_gliko_control_object_constructed (GObject *object)
{
  HyScanGtkGlikoControl *self = HYSCAN_GTK_GLIKO_CONTROL (object);
  HyScanGtkGlikoControlPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_gliko_control_parent_class)->constructed (object);
}

static void
hyscan_gtk_gliko_control_object_finalize (GObject *object)
{
  HyScanGtkGlikoControl *self = HYSCAN_GTK_GLIKO_CONTROL (object);
  HyScanGtkGlikoControlPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_gliko_control_parent_class)->finalize (object);
}

static void
hyscan_gtk_gliko_control_button (HyScanGtkGlikoControl *self,
                                 GdkEventButton        *event)
{
  HyScanGtkGlikoControlPrivate *priv = self->priv;

  if (event->button != 1)
    return;

  if (event->type == GDK_BUTTON_PRESS)
    {
      priv->move = TRUE;
      hyscan_gtk_gliko_get_center (HYSCAN_GTK_GLIKO (self), &priv->sxc0, &priv->syc0);
      priv->mx0 = (gdouble) event->x;
      priv->my0 = (gdouble) event->y;
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      priv->move = FALSE;
    }
}

static void
hyscan_gtk_gliko_control_motion (HyScanGtkGlikoControl *self,
                                 GdkEventMotion        *event)
{
  HyScanGtkGlikoControlPrivate *priv = self->priv;

  GtkAllocation allocation;
  int baseline;
  gdouble s, x, y;

  gtk_widget_get_allocated_size (GTK_WIDGET (self), &allocation, &baseline);

  if (priv->move)
    {
      s = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (self));
      x = priv->sxc0 - s * (priv->mx0 - (gdouble) event->x) / allocation.height;
      y = priv->syc0 - s * ((gdouble) event->y - priv->my0) / allocation.height;
      hyscan_gtk_gliko_set_center (HYSCAN_GTK_GLIKO (self), x, y);
    }
}

static void
hyscan_gtk_gliko_control_scroll (HyScanGtkGlikoControl *self,
                                 GdkEventScroll        *event)
{
  gdouble f;

  f = hyscan_gtk_gliko_get_scale (HYSCAN_GTK_GLIKO (self));

  if (event->direction == GDK_SCROLL_UP)
    hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (self), 0.875 * f);
  if (event->direction == GDK_SCROLL_DOWN)
    hyscan_gtk_gliko_set_scale (HYSCAN_GTK_GLIKO (self), 1.125 * f);
}

HyScanGtkGlikoControl *
hyscan_gtk_gliko_control_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_GLIKO_CONTROL, NULL);
}
