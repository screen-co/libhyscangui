/**
 * Слой - это сущность, рисующая какой-то предопределенный тип объектов. Например,
 * сетка, метки, линейка.
 *
 * ### Общие замечания ###
 *
 * Если слой что-то отображает, то рекомендуется иметь хотя бы следующий набор
 * методов для настройки цвета:
 *
 * - new_layer_set_shadow_color - цвет подложки для рисования под элементами и текстом
 * - new_layer_set_main_color - основной цвет для элементов и текста
 * - new_layer_set_frame_color - цвет окантовки вокруг подложки под текстом
 *
 * Однако это всего лишь совет, а не требование.
 *
 * Подробнее про добавление слоёв описано в классе #HyScanGtkLayerContainer.
 *
 */

#include "hyscan-gtk-layer.h"

G_DEFINE_INTERFACE (HyScanGtkLayer, hyscan_gtk_layer, G_TYPE_OBJECT)

static void
hyscan_gtk_layer_default_init (HyScanGtkLayerInterface *iface)
{
}

void
hyscan_gtk_layer_added (HyScanGtkLayer *layer,
                        HyScanGtkLayerContainer *event)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->added != NULL)
    (*iface->added) (layer, event);
}

void
hyscan_gtk_layer_set_visible (HyScanGtkLayer *layer,
                              gboolean visible)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->set_visible != NULL)
    (*iface->set_visible) (layer, visible);
}

gboolean
hyscan_gtk_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), TRUE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->get_visible != NULL)
    return (*iface->get_visible) (layer);

  return TRUE;
}

const gchar *
hyscan_gtk_layer_get_icon (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), NULL);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->get_icon != NULL)
    return (*iface->get_icon) (layer);

  return "image-missing";
}
