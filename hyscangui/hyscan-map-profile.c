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
#define PROJ_MERC      "merc"
#define PROJ_WEBMERC   "webmerc"

struct _HyScanMapProfilePrivate
{
  gchar                       *title;       /* Название профиля. */
  gchar                       *projection;  /* Название проекции. */
  gchar                       *tiles_dir;   /* Папка для хранения загруженных тайлов. */
  gchar                       *url_format;  /* Формат URL тайла. */

  guint                        min_zoom;    /* Минимальный доступный уровень детализации. */
  guint                        max_zoom;    /* Максимальный доступный уровень детализации. */
};

static void    hyscan_map_profile_object_finalize          (GObject                     *object);

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

  g_free (priv->title);
  g_free (priv->url_format);
  g_free (priv->tiles_dir);
  g_free (priv->projection);

  G_OBJECT_CLASS (hyscan_map_profile_parent_class)->finalize (object);
}

/* Устанавливает параметры профиля. */
static void
hyscan_gtk_map_profile_set_params (HyScanMapProfile *profile,
                                   const gchar   *title,
                                   const gchar   *url_format,
                                   const gchar   *cache_dir,
                                   const gchar   *projection,
                                   guint          min_zoom,
                                   guint          max_zoom)
{
  HyScanMapProfilePrivate *priv = profile->priv;

  priv->title = g_strdup (title);
  priv->url_format = g_strdup (url_format);
  priv->tiles_dir = g_strdup (cache_dir);
  priv->projection = g_strdup (projection);
  priv->min_zoom = min_zoom;
  priv->max_zoom = max_zoom;
}

/* Создаёт проекцию, соответствующую профилю. */
static HyScanGeoProjection *
hyscan_map_profile_create_projection (HyScanMapProfilePrivate *priv)
{
  if (g_str_equal (priv->projection, PROJ_WEBMERC))
    {
      return hyscan_pseudo_mercator_new ();
    }

  if (g_str_equal (priv->projection, PROJ_MERC))
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
  HyScanGtkLayer *tiles;

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
 * hyscan_map_profile_new_full:
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
hyscan_map_profile_new_full (const gchar   *title,
                             const gchar   *url_format,
                             const gchar   *cache_dir,
                             const gchar   *projection,
                             guint          min_zoom,
                             guint          max_zoom)
{
  HyScanMapProfile *profile;

  profile = hyscan_map_profile_new ();
  hyscan_gtk_map_profile_set_params (profile, title, url_format, cache_dir, projection, min_zoom, max_zoom);

  return profile;
}

/**
 * hyscan_map_profile_new_default:
 *
 * Создает профиль карты по умолчанию. Этот профиль может быть использован в случае,
 * если нет других пользовательских вариантов.
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanMapProfile *
hyscan_map_profile_new_default (void)
{
  HyScanMapProfile *profile;

  profile = hyscan_map_profile_new ();
  hyscan_gtk_map_profile_set_params (profile, "Default",
                                     "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png", NULL,
                                     PROJ_WEBMERC, 0, 19);

  return profile;
}

/**
 * hyscan_map_profile_new:
 * @url_format:
 * @cache_dir:
 * @projection:
 * @min_zoom:
 * @max_zoom:
 *
 * Создаёт пустой профиль карты. Параметры профиля могут быть установлены через
 * hyscan_map_profile_read().
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanMapProfile *
hyscan_map_profile_new (void)
{
  return g_object_new (HYSCAN_TYPE_MAP_PROFILE, NULL);
}

/**
 * hyscan_map_profile_read:
 * @param serializable
 * @param name
 * @return
 *
 */
gboolean
hyscan_map_profile_read (HyScanMapProfile    *serializable,
                         const gchar         *name)
{
  HyScanMapProfile *profile;
  gboolean result = FALSE;

  guint max_zoom, min_zoom;
  gchar *title = NULL, *url_format = NULL, *cache_dir = NULL, *projection = NULL;

  GKeyFile *key_file;
  GError *error = NULL;

  // TODO: эту функцию можно использовать в HyScanSerializable

  profile = HYSCAN_MAP_PROFILE (serializable);
  key_file = g_key_file_new ();

  g_key_file_load_from_file (key_file, name, G_KEY_FILE_NONE, &error);
  if (error != NULL)
    goto exit;

  title = g_key_file_get_string (key_file, "global", "title", &error);
  if (error != NULL)
    goto exit;

  url_format = g_key_file_get_string (key_file, "global", "url", &error);
  if (error != NULL)
    goto exit;

  projection = g_key_file_get_string (key_file, "global", "proj", &error);
  if (error != NULL)
    goto exit;

  cache_dir = g_key_file_get_string (key_file, "global", "dir", NULL);

  min_zoom = g_key_file_get_uint64 (key_file, "global", "min_zoom", &error);
  if (error != NULL)
    goto exit;

  max_zoom = g_key_file_get_uint64 (key_file, "global", "max_zoom", &error);
  if (error != NULL)
    goto exit;

  hyscan_gtk_map_profile_set_params (profile, title, url_format, cache_dir, projection, min_zoom, max_zoom);
  result = TRUE;

exit:
  if (error != NULL)
    {
      g_warning ("HyScanMapProfile: %s", error->message);
      g_clear_error (&error);
    }

  g_key_file_free (key_file);
  g_free (title);
  g_free (url_format);
  g_free (cache_dir);
  g_free (projection);

  return result;
}

/**
 * hyscan_map_profile_get_title:
 * @profile: указатель на #HyScanMapProfile.
 *
 * Returns: название профиля или %NULL. Для удаления g_free().
 */
gchar *
hyscan_map_profile_get_title (HyScanMapProfile *profile)
{
  g_return_val_if_fail (profile, NULL);

  return g_strdup (profile->priv->title);
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
  hyscan_gtk_layer_container_add (container, hyscan_map_profile_create_tiles (priv), TILES_LAYER_ID);

  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_object_unref (projection);

  return TRUE;
}
