#include "hyscan-gtk-map-tile-factory.h"

G_DEFINE_INTERFACE (HyScanGtkMapTileFactory, hyscan_gtk_map_file_factory, G_TYPE_OBJECT)

static void
hyscan_gtk_map_file_factory_default_init (HyScanGtkMapTileFactoryInterface *iface)
{
}

/**
 * hyscan_gtk_map_file_factory_create:
 * @factory:
 * @tile:
 *
 * Создание тайла @tile на фабрике @factory.
 */
gboolean hyscan_gtk_map_file_factory_create (HyScanGtkMapTileFactory *factory,
                                             HyScanGtkMapTile        *tile)
{
  HyScanGtkMapTileFactoryInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_FILE_FACTORY (factory), FALSE);

  iface = HYSCAN_GTK_MAP_FILE_FACTORY_GET_IFACE (factory);
  if (iface->create_tile != NULL)
    return (* iface->create_tile) (factory, tile);

  return FALSE;
}
