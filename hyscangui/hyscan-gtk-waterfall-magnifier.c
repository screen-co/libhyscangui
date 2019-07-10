/*
 * \file hyscan-gtk-waterfall-magnifier.c
 *
 * \brief Исходный файл лупы для виджета водопад
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-magnifier.h"
#include "hyscan-gtk-waterfall-tools.h"
#include "hyscan-gtk-waterfall.h"
#include <math.h>


struct _HyScanGtkWaterfallMagnifierPrivate
{
  HyScanGtkWaterfall          *wfall;             /**< Водопад. */
  GtkCifroArea                *carea;             /**< Цифроариа. */

  gboolean                     layer_visibility;  /**< Видимость слоя. */

  guint                        zoom;              /**< Во сколько раз увеличивать. */
  gdouble                      height;            /**< Высота области. */
  gdouble                      width;             /**< Ширина области. */
  HyScanCoordinates            window;            /**< Начальная координата окошка. */
  gboolean                     width_auto;        /**< Автоматическая ширина области. */

  GdkRGBA                      frame_color;       /**< Цвет рамки. */
  gdouble                      frame_width;       /**< Ширина рамки. */

  HyScanCoordinates            mouse;             /**< Координаты указателя мыши. */

  cairo_surface_t             *surface;           /**< Внутренний сёрфейс, который и увеличивается. */
};

static void     hyscan_gtk_waterfall_magnifier_interface_init           (HyScanGtkLayerInterface      *iface);
static void     hyscan_gtk_waterfall_magnifier_object_constructed       (GObject                      *object);
static void     hyscan_gtk_waterfall_magnifier_object_finalize          (GObject                      *object);

static void     hyscan_gtk_waterfall_magnifier_added                    (HyScanGtkLayer               *layer,
                                                                         HyScanGtkLayerContainer      *container);
static void     hyscan_gtk_waterfall_magnifier_removed                  (HyScanGtkLayer               *layer);
static void     hyscan_gtk_waterfall_magnifier_set_visible              (HyScanGtkLayer               *layer,
                                                                         gboolean                      visible);
static const gchar* hyscan_gtk_waterfall_magnifier_get_icon_name        (HyScanGtkLayer               *layer);

static void     hyscan_gtk_waterfall_magnifier_draw                     (GtkWidget                    *widget,
                                                                         cairo_t                      *cairo,
                                                                         HyScanGtkWaterfallMagnifier  *self);
static gboolean hyscan_gtk_waterfall_magnifier_motion                   (GtkWidget                    *widget,
                                                                         GdkEventMotion               *event,
                                                                         HyScanGtkWaterfallMagnifier  *self);
static gboolean hyscan_gtk_waterfall_magnifier_leave                    (GtkWidget                    *widget,
                                                                         GdkEventCrossing             *event,
                                                                         HyScanGtkWaterfallMagnifier  *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallMagnifier, hyscan_gtk_waterfall_magnifier, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallMagnifier)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_magnifier_interface_init));

static void
hyscan_gtk_waterfall_magnifier_class_init (HyScanGtkWaterfallMagnifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_magnifier_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_magnifier_object_finalize;
}

static void
hyscan_gtk_waterfall_magnifier_init (HyScanGtkWaterfallMagnifier *self)
{
  self->priv = hyscan_gtk_waterfall_magnifier_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_magnifier_object_constructed (GObject *object)
{
  GdkRGBA color_frame;
  HyScanGtkWaterfallMagnifier *self = HYSCAN_GTK_WATERFALL_MAGNIFIER (object);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_magnifier_parent_class)->constructed (object);

  hyscan_gtk_waterfall_magnifier_set_size (self, 100, 100);
  hyscan_gtk_waterfall_magnifier_set_position (self, 16, 16);

  gdk_rgba_parse (&color_frame, FRAME_DEFAULT);
  hyscan_gtk_waterfall_magnifier_set_frame_color (self, color_frame);
  hyscan_gtk_waterfall_magnifier_set_frame_width (self, 1);

  hyscan_gtk_waterfall_magnifier_set_zoom (self, 2);

  /* Включаем видимость слоя. */
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_magnifier_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMagnifier *self = HYSCAN_GTK_WATERFALL_MAGNIFIER (object);
  HyScanGtkWaterfallMagnifierPrivate *priv = self->priv;

  g_clear_object (&priv->wfall);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_magnifier_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_magnifier_added (HyScanGtkLayer          *layer,
                                      HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfall *wfall;
  HyScanGtkWaterfallMagnifier *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_MAGNIFIER (layer);

  wfall = HYSCAN_GTK_WATERFALL (container);
  self->priv->wfall = g_object_ref (wfall);
  self->priv->carea = GTK_CIFRO_AREA (wfall);

  /* Сигналы. */
  g_signal_connect (wfall, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_magnifier_draw), self);
  g_signal_connect (wfall, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_magnifier_motion), self);
  g_signal_connect (wfall, "leave-notify-event", G_CALLBACK (hyscan_gtk_waterfall_magnifier_leave), self);
}

static void
hyscan_gtk_waterfall_magnifier_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallMagnifier *self = HYSCAN_GTK_WATERFALL_MAGNIFIER (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
  self->priv->carea = NULL;
}

static void
hyscan_gtk_waterfall_magnifier_set_visible (HyScanGtkLayer *layer,
                                            gboolean        visible)
{
  HyScanGtkWaterfallMagnifier *self = HYSCAN_GTK_WATERFALL_MAGNIFIER (layer);

  self->priv->layer_visibility = visible;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static const gchar*
hyscan_gtk_waterfall_magnifier_get_icon_name (HyScanGtkLayer *layer)
{
  return "edit-find-symbolic";
}

/* Функция отрисовки. */
static void
hyscan_gtk_waterfall_magnifier_draw (GtkWidget                    *widget,
                                     cairo_t                      *cairo,
                                     HyScanGtkWaterfallMagnifier  *self)
{
  gint i, j;
  gint imin, imax, jmin, jmax, ishift, jshift;
  gdouble w_2, h_2;
  guchar *src, *dst;
  gint w, h, src_s, dst_s;
  cairo_surface_t *cairo_target;

  HyScanGtkWaterfallMagnifierPrivate *priv = self->priv;

  if (!priv->layer_visibility)
    return;

  if (priv->mouse.x == -1 || priv->mouse.y == -1)
    return;

  if (priv->zoom == 0)
    return;

  /* Наивное решение: просто берем и поточечно копируем всё, что попало в
   * нужную область. */
  cairo_target = cairo_get_target (cairo);
  src = cairo_image_surface_get_data (cairo_target);
  w = cairo_image_surface_get_width (cairo_target);
  h = cairo_image_surface_get_height (cairo_target);
  src_s = cairo_image_surface_get_stride (cairo_target);

  dst = cairo_image_surface_get_data (priv->surface);
  dst_s = cairo_image_surface_get_stride (priv->surface);

  w_2 = priv->width / 2.0;
  h_2 = priv->height / 2.0;

  imin = CLAMP (priv->mouse.y - h_2, 0, h - 1);
  imax = CLAMP (priv->mouse.y + h_2, 0, h - 1);
  jmin = CLAMP (priv->mouse.x - w_2, 0, w - 1);
  jmax = CLAMP (priv->mouse.x + w_2, 0, w - 1);

  ishift = imin - (priv->mouse.y - h_2);
  jshift = jmin - (priv->mouse.x - w_2);

  for (i = 0; i < priv->height; i++)
    for (j = 0; j < priv->width; j++)
      *((guint32*)(dst + i * dst_s + j * sizeof (guint32))) = 0x7F000000;

  for (i = imin; i < imax; i++)
    {
      for (j = jmin; j < jmax; j++)
        {
          gint dest_i = i - imin + ishift;
          gint dest_j = j - jmin + jshift;

          *((guint32*)(dst + dest_i * dst_s + dest_j * sizeof (guint32))) =
            *((guint32*)(src + i * src_s + j * sizeof (guint32)));
        }
    }

  /* Рисуем отзуммированную картинку. */
  cairo_surface_mark_dirty (priv->surface);
  cairo_save (cairo);
  cairo_translate (cairo, priv->window.x, priv->window.y);
  cairo_scale (cairo, priv->zoom, priv->zoom);
  cairo_set_source_surface (cairo, priv->surface, 0, 0);
  cairo_paint (cairo);
  cairo_restore (cairo);

  /* Рисуем рамочку вокруг. */
  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->frame_color);
  cairo_set_line_width (cairo, priv->frame_width);
  cairo_rectangle (cairo,
                   round (priv->window.x) + 0.5,
                   round (priv->window.y) + 0.5,
                   round (priv->width * priv->zoom),
                   round (priv->height * priv->zoom));
  cairo_stroke (cairo);
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_magnifier_motion (GtkWidget                   *widget,
                                       GdkEventMotion              *event,
                                       HyScanGtkWaterfallMagnifier *self)
{
  self->priv->mouse.x = event->x;
  self->priv->mouse.y = event->y;

  return FALSE;
}

/* Обработчик сигнала покидания курсором области виджета. */
static gboolean
hyscan_gtk_waterfall_magnifier_leave (GtkWidget                   *widget,
                                      GdkEventCrossing            *event,
                                      HyScanGtkWaterfallMagnifier *self)
{
  self->priv->mouse.x = -1;
  self->priv->mouse.y = -1;

  return FALSE;
}

/* Функция создает новый объект \link HyScanGtkWaterfallMagnifier \endlink */
HyScanGtkWaterfallMagnifier*
hyscan_gtk_waterfall_magnifier_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MAGNIFIER, NULL);
}


/* Функция задает степень увеличения. */
void
hyscan_gtk_waterfall_magnifier_set_zoom (HyScanGtkWaterfallMagnifier *self,
                                         guint                        zoom)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAGNIFIER (self));

  self->priv->zoom = zoom;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}


/* Функция задает размеры области под указатем, которая зуммируется. */
void
hyscan_gtk_waterfall_magnifier_set_size (HyScanGtkWaterfallMagnifier *self,
                                         gdouble                      width,
                                         gdouble                      height)
{
  HyScanGtkWaterfallMagnifierPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAGNIFIER (self));
  priv = self->priv;

  priv->height = height;
  priv->width = width;

  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  priv->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);
}

/* Функция задает начальную координату (левый верхний угол) окошка с увеличенным изображением. */
void
hyscan_gtk_waterfall_magnifier_set_position (HyScanGtkWaterfallMagnifier *self,
                                             gdouble                      x,
                                             gdouble                      y)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAGNIFIER (self));

  self->priv->window.x = x;
  self->priv->window.y = y;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}


/* Функция задает цвет рамки. */
void
hyscan_gtk_waterfall_magnifier_set_frame_color (HyScanGtkWaterfallMagnifier *self,
                                                GdkRGBA                      color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAGNIFIER (self));

  self->priv->frame_color = color;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}


/* Функция задает ширину рамки. */
void
hyscan_gtk_waterfall_magnifier_set_frame_width (HyScanGtkWaterfallMagnifier *self,
                                                gdouble                      width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MAGNIFIER (self));

  self->priv->frame_width = width;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_magnifier_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_magnifier_added;
  iface->removed = hyscan_gtk_waterfall_magnifier_removed;
  iface->set_visible = hyscan_gtk_waterfall_magnifier_set_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_magnifier_get_icon_name;
}
