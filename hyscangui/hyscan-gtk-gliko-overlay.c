/* hyscan-gtk-gliko-overlay.с
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
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

#if defined(_MSC_VER)
#include <glad/glad.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

#include "hyscan-gtk-gliko-overlay.h"

#define N 16

typedef struct _HyScanGtkGlikoOverlayPrivate HyScanGtkGlikoOverlayPrivate;

struct _HyScanGtkGlikoOverlayPrivate
{
  HyScanGtkGlikoLayer *layer[N];
  unsigned int enable;
  int width, height;
  int has_alpha;
  int redraw_parent;
};

/* Define type */
G_DEFINE_TYPE (HyScanGtkGlikoOverlay, hyscan_gtk_gliko_overlay, GTK_TYPE_GL_AREA)

/* Internal API */
static void dispose (GObject *gobject);
static void finalize (GObject *gobject);

static void get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width);
static void get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height);

static void
on_resize (GtkGLArea *area, gint width, gint height, gpointer user_data)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);
  int i;

  p->width = width;
  p->height = height;

  for (i = 0; i < N; i++)
    {
      if (p->layer[i] != NULL)
        {
          hyscan_gtk_gliko_layer_resize (p->layer[i], width, height);
        }
    }
}

static gboolean
on_render (GtkGLArea *area, GdkGLContext *context)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);
  int i;

  if (p->redraw_parent)
    {
      p->redraw_parent = 0;
      gtk_widget_queue_draw (gtk_widget_get_parent (GTK_WIDGET (area)));
    }

  glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor (0.2f, 0.3f, 0.3f, 1.0f);
  glClear (GL_COLOR_BUFFER_BIT);
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //glViewport( 0, 0, p->width, p->height );

  for (i = 0; i < N; i++)
    {
      if ((p->layer[i] != NULL) && ((p->enable >> i) & 1))
        {
          hyscan_gtk_gliko_layer_render (p->layer[i], context);
        }
    }

  // если отрисовка выполняется с альфа-каналом
  if (p->has_alpha)
    {
      // прописываем альфа канал в 1, отрисовываем без наложения
      glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
      glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
      glClear (GL_COLOR_BUFFER_BIT);
    }
  return TRUE;
}

static void
on_update (GdkFrameClock *clock,
           gpointer user_data)
{
  GtkGLArea *area = user_data;
  gtk_gl_area_queue_render (area);
}

static void
on_realize (GtkGLArea *area)
{
  HyScanGtkGlikoOverlayPrivate *p;
  int i;

  p = G_TYPE_INSTANCE_GET_PRIVATE (area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);

  // Make current:
  gtk_gl_area_make_current (area);

#if defined(_MSC_VER)
  if (!gladLoadGL ())
    {
      g_error ("gladLoadGLLoader failed");
    }
  gtk_gl_area_set_has_alpha (area, TRUE);
#endif

  // Print version info:
  const GLubyte *renderer = glGetString (GL_RENDERER);
  const GLubyte *version = glGetString (GL_VERSION);
  printf ("Renderer: %s\n", renderer);
  printf ("OpenGL version supported %s\n", version);
  fflush (stdout);

  // check for alpha channel of destinantion
  p->has_alpha = (gtk_gl_area_get_has_alpha (area) ? 1 : 0);

  // Disable depth buffer:
  gtk_gl_area_set_has_depth_buffer (area, FALSE);

  for (i = 0; i < N; i++)
    {
      if (p->layer[i] != NULL)
        {
          hyscan_gtk_gliko_layer_realize (p->layer[i]);
        }
    }

  // Get frame clock:
  GdkGLContext *glcontext = gtk_gl_area_get_context (area);
  GdkWindow *glwindow = gdk_gl_context_get_window (glcontext);
  GdkFrameClock *frame_clock = gdk_window_get_frame_clock (glwindow);

  // Connect update signal:
  //g_signal_connect_swapped (frame_clock, "update", G_CALLBACK (gtk_gl_area_queue_render), area);
  g_signal_connect (frame_clock, "update", G_CALLBACK (on_update), area);

  // Start updating:
  gdk_frame_clock_begin_updating (frame_clock);
}

/* Initialization */
static void
hyscan_gtk_gliko_overlay_class_init (HyScanGtkGlikoOverlayClass *klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *w_class = GTK_WIDGET_CLASS (klass);

  /* Add private data */
  g_type_class_add_private (klass, sizeof (HyScanGtkGlikoOverlayPrivate));

  g_class->dispose = dispose;
  g_class->finalize = finalize;

  w_class->get_preferred_width = get_preferred_width;
  w_class->get_preferred_height = get_preferred_height;
}

static void
hyscan_gtk_gliko_overlay_init (HyScanGtkGlikoOverlay *overlay)
{
  HyScanGtkGlikoOverlayPrivate *p;
  int i;

  p = G_TYPE_INSTANCE_GET_PRIVATE (overlay, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);

  /* Create cache for faster access */
  overlay->priv = p;

  for (i = 0; i < N; i++)
    {
      p->layer[i] = NULL;
    }
  p->enable = 0;
  p->has_alpha = 0;
  p->redraw_parent = 1;

  g_signal_connect (overlay, "realize", G_CALLBACK (on_realize), NULL);
  g_signal_connect (overlay, "render", G_CALLBACK (on_render), NULL);
  g_signal_connect (overlay, "resize", G_CALLBACK (on_resize), NULL);
}

static void
dispose (GObject *gobject)
{
  G_OBJECT_CLASS (hyscan_gtk_gliko_overlay_parent_class)
      ->dispose (gobject);
}

static void
finalize (GObject *instance)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);
  int i;

  for (i = 0; i < N; i++)
    {
      if (p->layer[i] != NULL)
        {
          g_object_unref (G_OBJECT (p->layer[i]));
        }
    }
  G_OBJECT_CLASS (hyscan_gtk_gliko_overlay_parent_class)
      ->finalize (instance);
}

static void
get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width)
{
  //HyScanGtkGlikoOverlayPrivate *p = GLIKO_OVERLAY(widget)->priv;

  *minimal_width = 512;
  *natural_width = 512;
}

static void
get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height)
{
  //HyScanGtkGlikoOverlayPrivate *p = GLIKO_OVERLAY(widget)->priv;

  *minimal_height = 512;
  *natural_height = 512;
}

GtkWidget *
hyscan_gtk_gliko_overlay_new (void)
{
  return GTK_WIDGET (g_object_new (hyscan_gtk_gliko_overlay_get_type (), NULL));
}

void
hyscan_gtk_gliko_overlay_set_layer (HyScanGtkGlikoOverlay *instance, const int index, HyScanGtkGlikoLayer *layer)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);

  if (index >= 0 && index < N)
    {
      p->layer[index] = layer;
    }
}

void
hyscan_gtk_gliko_overlay_enable_layer (HyScanGtkGlikoOverlay *instance, const int index, const int enable)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate);

  if (index >= 0 && index < N)
    {
      if (enable)
        {
          p->enable |= (1 << index);
        }
      else
        {
          p->enable &= ~(1 << index);
        }
    }
}
