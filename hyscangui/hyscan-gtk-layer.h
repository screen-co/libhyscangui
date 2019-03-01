#ifndef __HYSCAN_GTK_LAYER_H__
#define __HYSCAN_GTK_LAYER_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>
#include <hyscan-gtk-layer-container.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER            (hyscan_gtk_layer_get_type ())
#define HYSCAN_GTK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayer))
#define HYSCAN_IS_GTK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER))
#define HYSCAN_GTK_LAYER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayerInterface))

typedef struct _HyScanGtkLayer HyScanGtkLayer;
typedef struct _HyScanGtkLayerInterface HyScanGtkLayerInterface;
typedef struct _HyScanGtkLayerContainer HyScanGtkLayerContainer;

/**
 * HyScanGtkLayerInterface:
 * @g_iface: Базовый интерфейс.
 * @added: Регистрирует слой в контейнере @container.
 * @grab_input: Захватывает пользовательский ввод в своём контейнере.
 * @set_visible: Устанавливает, видно ли пользователю слой.
 * @get_visible: Возвращает, видно ли пользователю слой.
 * @get_icon_name: Возвращает имя иконки для слоя (см. #GtkIconTheme для подробностей).
 */
struct _HyScanGtkLayerInterface
{
  GTypeInterface       g_iface;

  void               (*added)            (HyScanGtkLayer          *gtk_layer,
                                          HyScanGtkLayerContainer *container);
  gboolean           (*grab_input)       (HyScanGtkLayer          *layer);
  void               (*set_visible)      (HyScanGtkLayer          *layer,
                                          gboolean                 visible);
  gboolean           (*get_visible)      (HyScanGtkLayer          *layer);
  const gchar *      (*get_icon_name)    (HyScanGtkLayer          *layer);
};

HYSCAN_API
GType         hyscan_gtk_layer_get_type               (void);

HYSCAN_API
void          hyscan_gtk_layer_added                  (HyScanGtkLayer          *layer,
                                                       HyScanGtkLayerContainer *container);

HYSCAN_API
gboolean      hyscan_gtk_layer_grab_input             (HyScanGtkLayer          *layer);

HYSCAN_API
void          hyscan_gtk_layer_set_visible            (HyScanGtkLayer          *layer,
                                                       gboolean                 visible);

HYSCAN_API
gboolean      hyscan_gtk_layer_get_visible            (HyScanGtkLayer          *layer);

HYSCAN_API
const gchar * hyscan_gtk_layer_get_icon_name          (HyScanGtkLayer          *layer);

G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_H__ */
