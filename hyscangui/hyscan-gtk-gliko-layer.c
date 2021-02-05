#include "hyscan-gtk-gliko-layer.h"

G_DEFINE_INTERFACE( HyScanGtkGlikoLayer, hyscan_gtk_gliko_layer, G_TYPE_OBJECT );

static void hyscan_gtk_gliko_layer_default_init( HyScanGtkGlikoLayerInterface *iface )
{
}

void hyscan_gtk_gliko_layer_realize( HyScanGtkGlikoLayer *instance )
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail( HYSCAN_IS_GTK_GLIKO_LAYER( instance ) );
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE( instance );
  g_return_if_fail( iface->realize != NULL );
  iface->realize( instance );
}

void hyscan_gtk_gliko_layer_render( HyScanGtkGlikoLayer *instance, GdkGLContext *context )
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail( HYSCAN_IS_GTK_GLIKO_LAYER( instance ) );
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE( instance );
  g_return_if_fail( iface->render != NULL );
  iface->render( instance, context );
}

void hyscan_gtk_gliko_layer_resize( HyScanGtkGlikoLayer *instance, const int width, const int height )
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail( HYSCAN_IS_GTK_GLIKO_LAYER( instance ) );
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE( instance );
  g_return_if_fail( iface->resize != NULL );
  iface->resize( instance, width, height );
}
