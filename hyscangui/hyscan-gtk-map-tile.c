#include "hyscan-gtk-map-tile.h"
#include <cairo.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_X,
  PROP_Y,
  PROP_ZOOM,
  PROP_SIZE,
};

struct _HyScanGtkMapTilePrivate
{
  guint                        x;              /* Положение по x в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        y;              /* Положение по y в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        zoom;           /* Масштаб. */
  guint                        size;           /* Размер тайла size x size - тайлы квадратные. */

  GdkPixbuf                   *pixbuf;         /* Изображение тайла. */
};

static void    hyscan_gtk_map_tile_set_property             (GObject               *object,
                                                             guint                  prop_id,
                                                             const GValue          *value,
                                                             GParamSpec            *pspec);
static void    hyscan_gtk_map_tile_object_constructed       (GObject               *object);
static void    hyscan_gtk_map_tile_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapTile, hyscan_gtk_map_tile, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tile_class_init (HyScanGtkMapTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tile_set_property;

  object_class->constructed = hyscan_gtk_map_tile_object_constructed;
  object_class->finalize = hyscan_gtk_map_tile_object_finalize;

  g_object_class_install_property (object_class, PROP_X,
    g_param_spec_uint ("x", "X", "X coordinate", 0, G_MAXUINT32, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_Y,
    g_param_spec_uint ("y", "Y", "Y coordinate", 0, G_MAXUINT32, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_ZOOM,
    g_param_spec_uint ("z", "Z", "Zoom", 0, 19, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SIZE,
    g_param_spec_uint ("size", "Size", "Tile size, px", 0, G_MAXUINT, 256,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tile_init (HyScanGtkMapTile *gtk_map_tile)
{
  gtk_map_tile->priv = hyscan_gtk_map_tile_get_instance_private (gtk_map_tile);
}

static void
hyscan_gtk_map_tile_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  switch (prop_id)
    {
    case PROP_X:
      priv->x = g_value_get_uint (value);
      break;

    case PROP_Y:
      priv->y = g_value_get_uint (value);
      break;

    case PROP_ZOOM:
      priv->zoom = g_value_get_uint (value);
      break;

    case PROP_SIZE:
      priv->size = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tile_object_constructed (GObject *object)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tile_parent_class)->constructed (object);
}

static void
hyscan_gtk_map_tile_object_finalize (GObject *object)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  g_clear_object (&priv->pixbuf);

  G_OBJECT_CLASS (hyscan_gtk_map_tile_parent_class)->finalize (object);
}

HyScanGtkMapTile *
hyscan_gtk_map_tile_new (guint x,
                         guint y,
                         guint z,
                         guint size)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILE,
                       "x", x,
                       "y", y,
                       "z", z,
                       "size", size, NULL);
}

guint
hyscan_gtk_map_tile_get_x (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->x;
}

guint
hyscan_gtk_map_tile_get_y (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->y;
}

guint
hyscan_gtk_map_tile_get_zoom (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->zoom;
}

guint
hyscan_gtk_map_tile_get_size (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->size;
}

gboolean 
hyscan_gtk_map_tile_set_pixbuf (HyScanGtkMapTile *tile,
                                GdkPixbuf        *pixbuf)
{
  HyScanGtkMapTilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), FALSE);
  g_return_val_if_fail (pixbuf != NULL, FALSE);

  priv = tile->priv;

  if (gdk_pixbuf_get_width (pixbuf) != (gint) priv->size ||
      gdk_pixbuf_get_height (pixbuf) != (gint) priv->size)
    {
      return FALSE;
    }

  g_clear_object (&priv->pixbuf);
  priv->pixbuf = g_object_ref (pixbuf);

  return TRUE;
}

GdkPixbuf *
hyscan_gtk_map_tile_get_pixbuf (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), NULL);

  return tile->priv->pixbuf;
}

/**
 * hyscan_gtk_map_tile_compare:
 * @a
 * @b
 *
 * Сравнивает тайлы a и b.
 *
 * Returns: Возвращает 0, если тайлы одинаковые.
 */
gint
hyscan_gtk_map_tile_compare (HyScanGtkMapTile *a,
                             HyScanGtkMapTile *b)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (a) && HYSCAN_IS_GTK_MAP_TILE (b), 0);

  if (a->priv->x == b->priv->x && a->priv->y == b->priv->y && a->priv->zoom == b->priv->zoom)
    return 0;
  else
    return 1;
}
