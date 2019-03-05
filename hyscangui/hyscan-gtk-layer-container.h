#ifndef __HYSCAN_GTK_LAYER_CONTAINER_H__
#define __HYSCAN_GTK_LAYER_CONTAINER_H__

#include <hyscan-api.h>
#include <hyscan-gtk-layer.h>
#include <gtk-cifro-area.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER_CONTAINER             (hyscan_gtk_layer_container_get_type ())
#define HYSCAN_GTK_LAYER_CONTAINER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainer))
#define HYSCAN_IS_GTK_LAYER_CONTAINER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER))
#define HYSCAN_GTK_LAYER_CONTAINER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainerClass))
#define HYSCAN_IS_GTK_LAYER_CONTAINER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_LAYER_CONTAINER))
#define HYSCAN_GTK_LAYER_CONTAINER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainerClass))

typedef struct _HyScanGtkLayerContainer HyScanGtkLayerContainer;
typedef struct _HyScanGtkLayerContainerPrivate HyScanGtkLayerContainerPrivate;
typedef struct _HyScanGtkLayerContainerClass HyScanGtkLayerContainerClass;
typedef struct _HyScanGtkLayer HyScanGtkLayer;

struct _HyScanGtkLayerContainer
{
  GtkCifroArea parent_instance;

  HyScanGtkLayerContainerPrivate *priv;
};

struct _HyScanGtkLayerContainerClass
{
  GtkCifroAreaClass parent_class;
};

GType                     hyscan_gtk_layer_container_get_type            (void);

HYSCAN_API
void                      hyscan_gtk_layer_container_add                 (HyScanGtkLayerContainer *container,
                                                                          HyScanGtkLayer          *layer,
                                                                          const gchar             *key);

HYSCAN_API
void                      hyscan_gtk_layer_container_remove              (HyScanGtkLayerContainer *container,
                                                                          HyScanGtkLayer          *layer);

HYSCAN_API
void                     hyscan_gtk_layer_container_remove_all           (HyScanGtkLayerContainer *container);

HYSCAN_API
HyScanGtkLayer *          hyscan_gtk_layer_container_lookup              (HyScanGtkLayerContainer *container,
                                                                          const gchar             *key);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_input_owner     (HyScanGtkLayerContainer *container,
                                                                          gconstpointer            instance);

HYSCAN_API
gconstpointer             hyscan_gtk_layer_container_get_input_owner     (HyScanGtkLayerContainer *container);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_handle_grabbed  (HyScanGtkLayerContainer *container,
                                                                          gconstpointer            instance);

HYSCAN_API
gconstpointer             hyscan_gtk_layer_container_get_handle_grabbed  (HyScanGtkLayerContainer *container);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_changes_allowed (HyScanGtkLayerContainer *container,
                                                                          gboolean                 changes_allowed);
HYSCAN_API
gboolean                  hyscan_gtk_layer_container_get_changes_allowed (HyScanGtkLayerContainer *container);


G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_CONTAINER_H__ */
