/* hyscan-gtk-map-profile.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include "hyscan-map-profile.h"
#include <hyscan-merged-tile-source.h>
#include <hyscan-pseudo-mercator.h>
#include <hyscan-mercator.h>
#include <hyscan-gtk-map-tiles.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-map-fs-tile-source.h>
#include <hyscan-network-map-tile-source.h>
#include <string.h>

#define CACHE_SIZE     256
#define TILES_LAYER_ID "tiles-layer"
#define PROJ_MERC      "merc"
#define PROJ_WEBMERC   "webmerc"
#define INI_PREFIX_URL "url"
#define INI_PREFIX_DIR "dir"
#define INI_GROUP      "global"

struct _HyScanMapProfilePrivate
{
  gchar                       *title;       /* Название профиля. */
  gchar                       *projection;  /* Название проекции. */
  gchar                      **tiles_dir;   /* Папка для хранения загруженных тайлов. */
  gchar                      **url_format;  /* Формат URL тайла. */

  guint                        min_zoom;    /* Минимальный доступный уровень детализации. */
  guint                        max_zoom;    /* Максимальный доступный уровень детализации. */

  GKeyFile                    *key_file;
};

static void    hyscan_map_profile_object_finalize          (GObject                     *object);
static void    hyscan_map_profile_object_constructed       (GObject                     *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapProfile, hyscan_map_profile, G_TYPE_OBJECT)

static void
hyscan_map_profile_class_init (HyScanMapProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_map_profile_object_constructed;
  object_class->finalize = hyscan_map_profile_object_finalize;
}

static void
hyscan_map_profile_init (HyScanMapProfile *map_profile)
{
  map_profile->priv = hyscan_map_profile_get_instance_private (map_profile);
}

static void
hyscan_map_profile_object_constructed (GObject *object)
{
  HyScanMapProfile *map_profile = HYSCAN_MAP_PROFILE (object);
  HyScanMapProfilePrivate *priv = map_profile->priv;

  G_OBJECT_CLASS (hyscan_map_profile_parent_class)->constructed (object);

  priv->key_file = g_key_file_new ();
}

static void
hyscan_map_profile_object_finalize (GObject *object)
{
  HyScanMapProfile *map_profile = HYSCAN_MAP_PROFILE (object);
  HyScanMapProfilePrivate *priv = map_profile->priv;

  g_key_file_free (priv->key_file);
  g_free (priv->title);
  g_strfreev (priv->url_format);
  g_strfreev (priv->tiles_dir);
  g_free (priv->projection);

  G_OBJECT_CLASS (hyscan_map_profile_parent_class)->finalize (object);
}

/* Устанавливает параметры профиля.
 * url_format и cache_dir - нуль-терминированные массивы строк одинаковой длины;
 * если для какого-то источника папка кэширования не указана, то указывается
 * пустая строка "" != NULL. */
static void
hyscan_gtk_map_profile_set_params (HyScanMapProfile *profile,
                                   const gchar      *title,
                                   gchar           **url_format,
                                   gchar           **cache_dir,
                                   const gchar      *projection,
                                   guint             min_zoom,
                                   guint             max_zoom)
{
  HyScanMapProfilePrivate *priv = profile->priv;

  priv->title = g_strdup (title);
  priv->url_format = g_strdupv (url_format);
  priv->tiles_dir = g_strdupv (cache_dir);
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
      return hyscan_mercator_new (p);
    }

  g_warning ("HyScanMapProfile: unknown projection %s", priv->projection);

  return FALSE;
}

/* Создаёт тайловый слой, соответствующий профилю. */
static HyScanGtkLayer *
hyscan_map_profile_create_tiles (HyScanMapProfilePrivate *priv,
                                 HyScanGeoProjection     *projection)
{
  HyScanGtkLayer *tiles;

  HyScanCache *cache;
  HyScanMergedTileSource *merged_source;

  gint i;

  merged_source = hyscan_merged_tile_source_new ();
  for (i = 0; priv->url_format[i] != NULL; ++i)
    {
      HyScanGtkMapTileSource *fs_source;
      HyScanGtkMapTileSource *nw_source;

      gchar *cache_path;
      gchar *tmp_dir = NULL;

      nw_source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_network_map_tile_source_new (priv->url_format[i],
                                                                                  projection,
                                                                                  priv->min_zoom,
                                                                                  priv->max_zoom));

      /* Если папка для кэширования тайлов не указана, создаём свою. */
      if (!g_str_equal (priv->tiles_dir[i], ""))
        cache_path = priv->tiles_dir[i];
      else
        cache_path = (tmp_dir = g_dir_make_tmp ("hyscan-map-XXXXXX", NULL));

      fs_source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_gtk_map_fs_tile_source_new (cache_path, nw_source));

      hyscan_merged_tile_source_append (merged_source, fs_source);

      g_object_unref (nw_source);
      g_object_unref (fs_source);
      g_free (tmp_dir);
    }

  cache = HYSCAN_CACHE (hyscan_cached_new (CACHE_SIZE));
  tiles = hyscan_gtk_map_tiles_new (cache, HYSCAN_GTK_MAP_TILE_SOURCE(merged_source));

  g_object_unref (cache);
  g_object_unref (merged_source);

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
  gchar *url_formats[2];
  gchar *cache_dirs[2];

  url_formats[0] = (gchar *) url_format;
  url_formats[1] = NULL;

  cache_dirs[0] = cache_dir == NULL ? "" : (gchar *) cache_dir;
  cache_dirs[1] = NULL;

  profile = hyscan_map_profile_new ();
  hyscan_gtk_map_profile_set_params (profile, title, url_formats, cache_dirs, projection, min_zoom, max_zoom);

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
  gchar *url_formats[] = {"http://a.tile.openstreetmap.org/{z}/{x}/{y}.png", NULL};
  gchar *cache_dirs[]  = {"", NULL};

  profile = hyscan_map_profile_new ();
  hyscan_gtk_map_profile_set_params (profile, "Default", url_formats, cache_dirs, PROJ_WEBMERC, 0, 19);

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
hyscan_map_profile_read (HyScanMapProfile    *profile,
                         const gchar         *name)
{
  HyScanMapProfilePrivate *priv;
  gboolean result = FALSE;

  guint max_zoom, min_zoom;
  gchar *title = NULL, *projection = NULL;

  GError *error = NULL;
  GArray *url_formats_arr, *cache_dirs_arr;
  gchar **url_formats = NULL, **cache_dirs = NULL;

  gchar **keys;
  gint i;

  g_return_val_if_fail (HYSCAN_IS_MAP_PROFILE (profile), FALSE);
  priv = profile->priv;
  
  g_key_file_load_from_file (priv->key_file, name, G_KEY_FILE_NONE, &error);
  if (error != NULL)
    goto exit;

  title = g_key_file_get_string (priv->key_file, INI_GROUP, "title", &error);
  if (error != NULL)
    goto exit;

  projection = g_key_file_get_string (priv->key_file, INI_GROUP, "proj", &error);
  if (error != NULL)
    goto exit;

  min_zoom = g_key_file_get_uint64 (priv->key_file, INI_GROUP, "min_zoom", &error);
  if (error != NULL)
    goto exit;

  max_zoom = g_key_file_get_uint64 (priv->key_file, INI_GROUP, "max_zoom", &error);
  if (error != NULL)
    goto exit;

  /* Ищем пары ключей url{suffix} и dir{suffix} - формат ссылки на тайл и папку кэширования.
   * Например: url_osm - dir_osm, url1 - dir1.
   * */
  url_formats_arr = g_array_new (TRUE, FALSE, sizeof (gchar *));
  cache_dirs_arr  = g_array_new (TRUE, FALSE, sizeof (gchar *));
  keys = g_key_file_get_keys (priv->key_file, INI_GROUP, NULL, NULL);
  for (i = 0; keys[i] != NULL; ++i)
    {
      gchar *url_format, *cache_dir;
      gchar *suffix;
      gchar *dir_key;

      if (g_strrstr (keys[i], INI_PREFIX_URL) != keys[i])
        continue;
      
      suffix = keys[i] + strlen (INI_PREFIX_URL);

      /* Получаем формат ссылки источника тайлов. */
      url_format = g_key_file_get_string (priv->key_file, INI_GROUP, keys[i], NULL);
      if (url_format == NULL)
        continue;

      /* Находим соответствующую папку для кэширования. */
      dir_key = g_strdup_printf (INI_PREFIX_DIR"%s", suffix);
      cache_dir = g_key_file_get_string (priv->key_file, INI_GROUP, dir_key, NULL);
      if (cache_dir == NULL)
        cache_dir = strdup ("");
      
      g_free (dir_key);

      g_array_append_val (url_formats_arr, url_format);
      g_array_append_val (cache_dirs_arr, cache_dir);
    }
  url_formats = (gchar**) g_array_free (url_formats_arr, FALSE);
  cache_dirs  = (gchar**) g_array_free (cache_dirs_arr, FALSE);
  g_strfreev (keys);

  hyscan_gtk_map_profile_set_params (profile, title, url_formats, cache_dirs,
                                     projection, min_zoom, max_zoom);

  g_strfreev (url_formats);
  g_strfreev (cache_dirs);

  result = TRUE;

exit:
  if (error != NULL)
    {
      g_warning ("HyScanMapProfile: %s", error->message);
      g_clear_error (&error);
    }

  g_free (title);
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
  hyscan_gtk_layer_container_add (container, hyscan_map_profile_create_tiles (priv, projection), TILES_LAYER_ID);

  /* Конфигурируем остальные слои. */
  hyscan_gtk_layer_container_load_key_file (container, priv->key_file);

  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_object_unref (projection);

  return TRUE;
}
