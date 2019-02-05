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

  cairo_format_t               format;         /* Формат тайла CAIRO_FORMAT_RGB24 или CAIRO_FORMAT_ARGB32. */
  guint                        size;           /* Размер тайла size x size - тайлы квадратные. */

  cairo_surface_t             *surface;        /* Поверхность cairo с изображением тайла. */
  guint32                      data_size;
  gboolean                     filled;         /* Заполнена ли поверхность surface реальным изображением. */
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
    g_param_spec_uint ("z", "Z", "Zoom", 1, 19, 1,
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

  priv->format = CAIRO_FORMAT_RGB24;
  /* todo: может быть использовать cairo_image_surface_create_for_data. */
  priv->surface = cairo_image_surface_create (priv->format, priv->size, priv->size);
  priv->data_size = (guint32) (cairo_image_surface_get_height (priv->surface) * cairo_image_surface_get_stride (priv->surface));
}

static void
hyscan_gtk_map_tile_object_finalize (GObject *object)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  g_clear_pointer (&priv->surface, cairo_surface_destroy);

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

cairo_surface_t *
hyscan_gtk_map_tile_get_surface (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), NULL);

  return tile->priv->surface;
}

guchar *
hyscan_gtk_map_tile_get_data (HyScanGtkMapTile *tile,
                              guint32          *size)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), NULL);

  *size = tile->priv->data_size;
  return cairo_image_surface_get_data (tile->priv->surface);
}

gboolean
hyscan_gtk_map_tile_is_filled (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), FALSE);

  return tile->priv->filled;
}

void
hyscan_gtk_map_tile_set_filled (HyScanGtkMapTile *tile,
                                gboolean          filled)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile));

  tile->priv->filled = filled;
}

gboolean
hyscan_gtk_map_tile_set_content (HyScanGtkMapTile *tile,
                                 cairo_surface_t  *content)
{
  HyScanGtkMapTilePrivate *priv;
  gssize dsize;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), FALSE);

  priv = tile->priv;

  /* Проверяем формат загруженного изображения. */
  if (cairo_image_surface_get_format (content) != priv->format)
    {
      g_warning ("Wrong image format");
      return FALSE;
    }

  /* Копируем поверхность в целевой тайл. */
  dsize = cairo_image_surface_get_height (content) * cairo_image_surface_get_stride (content);

  return hyscan_gtk_map_tile_set_data (tile, cairo_image_surface_get_data (content), (guint32) dsize);
}

gboolean
hyscan_gtk_map_tile_set_data (HyScanGtkMapTile *tile,
                              guchar           *data,
                              guint32           size)
{
  HyScanGtkMapTilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), FALSE);

  priv = tile->priv;

  /* Проверяем размер буфера. */
  if (size != priv->data_size)
    return FALSE;

  memcpy (cairo_image_surface_get_data (priv->surface), data, size);
  priv->filled = TRUE;
  cairo_surface_mark_dirty (priv->surface);

  return TRUE;
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