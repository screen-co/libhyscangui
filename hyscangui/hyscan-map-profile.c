#include "hyscan-map-profile.h"
#include <hyscan-pseudo-mercator.h>
#include <hyscan-mercator.h>
#include <hyscan-gtk-map-tiles.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-map-fs-tile-source.h>
#include <hyscan-network-map-tile-source.h>

#define MERCATOR_MAX_LAT 85.08405905010976
#define CACHE_SIZE 256
#define TILES_LAYER_ID "tiles-layer"

struct _HyScanMapProfilePrivate
{
  gchar                       *projection;  /* Название проекции. */
  gchar                       *tiles_dir;   /* Папка для хранения загруженных тайлов. */
  gchar                       *url_format;  /* Формат URL тайла. */

  guint                        min_zoom;    /* Минимальный доступный уровень детализации. */
  guint                        max_zoom;    /* Максимальный доступный уровень детализации. */
};

static void    hyscan_map_profile_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapProfile, hyscan_map_profile, G_TYPE_OBJECT)

static void
hyscan_map_profile_class_init (HyScanMapProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_map_profile_object_finalize;
}

static void
hyscan_map_profile_init (HyScanMapProfile *map_profile)
{
  map_profile->priv = hyscan_map_profile_get_instance_private (map_profile);
}

static void
hyscan_map_profile_object_finalize (GObject *object)
{
  HyScanMapProfile *map_profile = HYSCAN_MAP_PROFILE (object);
  HyScanMapProfilePrivate *priv = map_profile->priv;

  g_free (priv->url_format);
  g_free (priv->tiles_dir);
  g_free (priv->projection);

  G_OBJECT_CLASS (hyscan_map_profile_parent_class)->finalize (object);
}

/* Создаёт проекцию, соответствующую профилю. */
static HyScanGeoProjection *
hyscan_map_profile_create_projection (HyScanMapProfilePrivate *priv)
{
  if (g_str_equal (priv->projection, "webmerc"))
    {
      return hyscan_pseudo_mercator_new ();
    }
  else if (g_str_equal (priv->projection, "merc"))
    {
      HyScanGeoEllipsoidParam p;

      hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
      return hyscan_mercator_new (p, -MERCATOR_MAX_LAT, MERCATOR_MAX_LAT);
    }

  g_warning ("HyScanMapProfile: unknown projection %s", priv->projection);
  return FALSE;
}

/* Создаёт тайловый слой, соответствующий профилю. */
static HyScanGtkLayer *
hyscan_map_profile_create_tiles (HyScanMapProfilePrivate *priv)
{
  HyScanGtkMapTiles *tiles;

  HyScanCache *cache;

  HyScanGtkMapTileSource *fs_source;
  HyScanGtkMapTileSource *nw_source;

  gchar *tmp_dir = NULL;
  gchar *cache_path;

  /* Если папка для кэширования тайлов не указана, создаём свою. */
  if (priv->tiles_dir != NULL)
    cache_path = priv->tiles_dir;
  else
    cache_path = (tmp_dir = g_dir_make_tmp ("hyscan-map-XXXXXX", NULL));

  nw_source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_network_map_tile_source_new (priv->url_format,
                                                                              priv->min_zoom,
                                                                              priv->max_zoom));
  fs_source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_gtk_map_fs_tile_source_new (cache_path, nw_source));
  cache = HYSCAN_CACHE (hyscan_cached_new (CACHE_SIZE));
  tiles = hyscan_gtk_map_tiles_new (cache, fs_source);

  g_free (tmp_dir);
  g_object_unref (nw_source);
  g_object_unref (fs_source);
  g_object_unref (cache);

  return HYSCAN_GTK_LAYER (tiles);
}

/**
 * hyscan_map_profile_new:
 * @url_format
 * @cache_dir
 * @projection
 * @min_zoom
 * @max_zoom
 *
 * Создаёт новый профиль карты, который содержит в себе информацию об источнике
 * тайлов и соответствующей ему картографической проекции.
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanMapProfile *
hyscan_map_profile_new (const gchar   *url_format,
                        const gchar   *cache_dir,
                        const gchar   *projection,
                        guint          min_zoom,
                        guint          max_zoom)
{
  HyScanMapProfile *profile;
  HyScanMapProfilePrivate *priv;

  profile = g_object_new (HYSCAN_TYPE_MAP_PROFILE, NULL);

  priv = profile->priv;
  priv->url_format = g_strdup (url_format);
  priv->tiles_dir = g_strdup (cache_dir);
  priv->projection = g_strdup (projection);
  priv->min_zoom = min_zoom;
  priv->max_zoom = max_zoom;

  return profile;
}

/**
 * hyscan_map_profile_apply:
 * @profile
 * @map
 *
 * Применяет профиль @profile к карте @map.
 */
gboolean
hyscan_map_profile_apply (HyScanMapProfile *profile,
                          HyScanGtkMap     *map)
{
  HyScanMapProfilePrivate *priv;
  HyScanGtkLayerContainer *container;
  HyScanGeoProjection *projection;

  HyScanGtkLayer *tiles;
  HyScanGtkLayer *tiles_old;

  g_return_val_if_fail (HYSCAN_IS_MAP_PROFILE (profile), FALSE);

  priv = profile->priv;
  container = HYSCAN_GTK_LAYER_CONTAINER (map);

  projection = hyscan_map_profile_create_projection (priv);
  if (projection == NULL)
    return FALSE;

  /* Убираем старый слой тайлов. */
  tiles_old = hyscan_gtk_layer_container_lookup (container, TILES_LAYER_ID);
  if (tiles_old != NULL)
    hyscan_gtk_layer_container_remove (container, tiles_old);

  /* Заменяем проекцию. */
  hyscan_gtk_map_set_projection (map, projection);

  /* Устанавливаем новый слой тайлов в самый низ. */
  tiles = hyscan_map_profile_create_tiles (priv);
  hyscan_gtk_layer_container_add (container, tiles, TILES_LAYER_ID);

  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_object_unref (projection);
  g_object_unref (tiles);

  return TRUE;
}
