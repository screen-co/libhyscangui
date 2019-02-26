#ifndef __HYSCAN_GTK_MAP_PIN_LAYER_H__
#define __HYSCAN_GTK_MAP_PIN_LAYER_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_PIN_LAYER             (hyscan_gtk_map_pin_layer_get_type ())
#define HYSCAN_GTK_MAP_PIN_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayer))
#define HYSCAN_IS_GTK_MAP_PIN_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER))
#define HYSCAN_GTK_MAP_PIN_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayerClass))
#define HYSCAN_IS_GTK_MAP_PIN_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_PIN_LAYER))
#define HYSCAN_GTK_MAP_PIN_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayerClass))

typedef struct _HyScanGtkMapPinLayer HyScanGtkMapPinLayer;
typedef struct _HyScanGtkMapPinLayerPrivate HyScanGtkMapPinLayerPrivate;
typedef struct _HyScanGtkMapPinLayerClass HyScanGtkMapPinLayerClass;

struct _HyScanGtkMapPinLayer
{
  GObject parent_instance;

  HyScanGtkMapPinLayerPrivate *priv;
};

struct _HyScanGtkMapPinLayerClass
{
  GObjectClass parent_class;

  void       (*draw)                     (HyScanGtkMapPinLayer *layer,
                                          cairo_t              *cairo);
};

HYSCAN_API
GType                  hyscan_gtk_map_pin_layer_get_type       (void);

HYSCAN_API
HyScanGtkMapPinLayer * hyscan_gtk_map_pin_layer_new            (void);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_clear          (HyScanGtkMapPinLayer *pin_layer);

HYSCAN_API
GList*                 hyscan_gtk_map_pin_layer_get_points     (HyScanGtkMapPinLayer *pin_layer);

HYSCAN_API
HyScanGtkMapPoint *    hyscan_gtk_map_pin_layer_insert_before  (HyScanGtkMapPinLayer *pin_layer,
                                                                HyScanGtkMapPoint    *point,
                                                                GList                *sibling);

HYSCAN_API
HyScanGtkMap*          hyscan_gtk_map_pin_layer_get_map        (HyScanGtkMapPinLayer *pin_layer);

HYSCAN_API
GdkRGBA*               hyscan_gtk_map_pin_layer_get_color      (HyScanGtkMapPinLayer *pin_layer);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_start_drag     (HyScanGtkMapPinLayer *layer,
                                                                HyScanGtkMapPoint    *handle_point);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_PIN_LAYER_H__ */
