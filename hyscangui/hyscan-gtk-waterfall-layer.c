/*
 * \file hyscan-gtk-waterfall-layer.c
 *
 * \brief Исходный файл интерфейса слоев водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-layer.h"

G_DEFINE_INTERFACE (HyScanGtkWaterfallLayer, hyscan_gtk_waterfall_layer, G_TYPE_OBJECT);

static void
hyscan_gtk_waterfall_layer_default_init (HyScanGtkWaterfallLayerInterface *iface)
{
}

void
hyscan_gtk_waterfall_layer_grab_input (HyScanGtkWaterfallLayer *layer)
{
  HyScanGtkWaterfallLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_LAYER (layer));

  iface = HYSCAN_GTK_WATERFALL_LAYER_GET_IFACE (layer);

  if (iface->grab_input != NULL)
    (* iface->grab_input) (layer);

}

const gchar*
hyscan_gtk_waterfall_layer_get_mnemonic (HyScanGtkWaterfallLayer *layer)
{
  HyScanGtkWaterfallLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_LAYER (layer), NULL);

  iface = HYSCAN_GTK_WATERFALL_LAYER_GET_IFACE (layer);
  if (iface->get_mnemonic != NULL)
    return (* iface->get_mnemonic) (layer);

  return NULL;
}
