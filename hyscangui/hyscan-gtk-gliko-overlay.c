#define GL_GLEXT_PROTOTYPES

#include <gtk/gtk.h>
#include <GL/gl.h>
#include "hyscan-gtk-gliko-overlay.h"

#define N 16

typedef struct _HyScanGtkGlikoOverlayPrivate HyScanGtkGlikoOverlayPrivate;

struct _HyScanGtkGlikoOverlayPrivate
{
  HyScanGtkGlikoLayer *layer[N];
  unsigned int enable;
  int width, height;
};


/* Define type */
G_DEFINE_TYPE( HyScanGtkGlikoOverlay, hyscan_gtk_gliko_overlay, GTK_TYPE_GL_AREA )

/* Internal API */
static void dispose( GObject *gobject );
static void finalize( GObject *gobject );

static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width );
static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height );

static void on_resize (GtkGLArea *area, gint width, gint height)
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );
  int i;

  p->width = width;
  p->height = height;

  for( i = 0; i < N; i++ )
  {
    if( p->layer[i] != NULL )
    {
      hyscan_gtk_gliko_layer_resize( p->layer[i], width, height );
    }
  }
}

static gboolean on_render( GtkGLArea *area, GdkGLContext *context )
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );
  int i;

  gtk_gl_area_make_current( area );

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear( GL_COLOR_BUFFER_BIT );
  glEnable( GL_BLEND );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

  //glViewport( 0, 0, p->width, p->height );

  for( i = 0; i < N; i++ )
  {
    if( (p->layer[i] != NULL) && ((p->enable >> i) & 1) )
    {
      hyscan_gtk_gliko_layer_render( p->layer[i], context );
    }
  }
  return TRUE;
}

static void on_realize( GtkGLArea *area )
{
  HyScanGtkGlikoOverlayPrivate *p;
  int i;

  p = G_TYPE_INSTANCE_GET_PRIVATE( area, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );

  // Make current:
  gtk_gl_area_make_current( area );

  // Print version info:
  const GLubyte* renderer = glGetString(GL_RENDERER);
  const GLubyte* version = glGetString(GL_VERSION);
  printf("Renderer: %s\n", renderer);
  printf("OpenGL version supported %s\n", version);

  // Disable depth buffer:
  gtk_gl_area_set_has_depth_buffer(area, FALSE);

  for( i = 0; i < N; i++ )
  {
    if( p->layer[i] != NULL )
    {
      hyscan_gtk_gliko_layer_realize( p->layer[i] );
    }
  }

  // Get frame clock:
  GdkGLContext *glcontext = gtk_gl_area_get_context(area);
  GdkWindow *glwindow = gdk_gl_context_get_window(glcontext);
  GdkFrameClock *frame_clock = gdk_window_get_frame_clock(glwindow);

  // Connect update signal:
  g_signal_connect_swapped( frame_clock, "update", G_CALLBACK(gtk_gl_area_queue_render), area );

  // Start updating:
  gdk_frame_clock_begin_updating(frame_clock);
}

/* Initialization */
static void hyscan_gtk_gliko_overlay_class_init( HyScanGtkGlikoOverlayClass *klass )
{
  GObjectClass *g_class = G_OBJECT_CLASS( klass );
  GtkWidgetClass *w_class = GTK_WIDGET_CLASS( klass );

  /* Add private data */
  g_type_class_add_private( klass, sizeof( HyScanGtkGlikoOverlayPrivate ) );

  g_class->dispose = dispose;
  g_class->finalize = finalize;

  w_class->get_preferred_width  = get_preferred_width;
  w_class->get_preferred_height = get_preferred_height;
}

static void hyscan_gtk_gliko_overlay_init( HyScanGtkGlikoOverlay *overlay )
{
  HyScanGtkGlikoOverlayPrivate *p;
  int i;

  p = G_TYPE_INSTANCE_GET_PRIVATE( overlay, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );

  /* Create cache for faster access */
  overlay->priv = p;

  for( i = 0; i < N; i++ )
  {
    p->layer[i] = NULL;
  }
  p->enable = 0;

  g_signal_connect( overlay, "realize", G_CALLBACK(on_realize), NULL);
  g_signal_connect( overlay, "render", G_CALLBACK(on_render), NULL);
  g_signal_connect( overlay, "resize", G_CALLBACK(on_resize), NULL);
}

static void dispose( GObject *gobject )
{
  G_OBJECT_CLASS( hyscan_gtk_gliko_overlay_parent_class )->dispose( gobject );
}

static void finalize( GObject *gobject )
{
  G_OBJECT_CLASS( hyscan_gtk_gliko_overlay_parent_class )->finalize( gobject );
}

static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width )
{
  //HyScanGtkGlikoOverlayPrivate *p = GLIKO_OVERLAY(widget)->priv;

  *minimal_width = 512;
  *natural_width = 512;
}

static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height )
{
  //HyScanGtkGlikoOverlayPrivate *p = GLIKO_OVERLAY(widget)->priv;

  *minimal_height = 512;
  *natural_height = 512;
}

GtkWidget *hyscan_gtk_gliko_overlay_new( void )
{
  return GTK_WIDGET( g_object_new( hyscan_gtk_gliko_overlay_get_type(), NULL ) );
}

void hyscan_gtk_gliko_overlay_set_layer( HyScanGtkGlikoOverlay *instance, const int index, HyScanGtkGlikoLayer *layer )
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );

  if( index >= 0 && index < N )
  {
    p->layer[ index ] = layer;
  }
}

void hyscan_gtk_gliko_overlay_enable_layer( HyScanGtkGlikoOverlay *instance, const int index, const int enable )
{
  HyScanGtkGlikoOverlayPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO_OVERLAY, HyScanGtkGlikoOverlayPrivate );

  if( index >= 0 && index < N )
  {
    if( enable )
    {
      p->enable |= (1 << index);
    }
    else
    {
      p->enable &= ~(1 << index);
    }
  }
}
