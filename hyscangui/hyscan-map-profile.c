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

/**
 * SECTION: hyscan-gtk-map-profile
 * @Short_description: Класс слоя с метками водопада
 * @Title: HyScanGtkMapProfile
 *
 * Профиль карты предназначен для упрощения процесса конфигурации виджета карты
 * #HyScanGtkMap и размещения на ней слоя подложки с изображением непосредственно карты.
 *
 * Профиль содержит в себе информацию об используемой подложке карты и соответствующем
 * ей стиле оформления других слоёв карты. Так, например, спутниковые тайлы как правило
 * имеют более тёмный цвет, чем векторные, и на их фоне инструменты карты должны
 * менять цвет на более светлый.
 *
 * Существует несколько способов создания профиля карты:
 * - конструктор по умолчанию hyscan_map_profile_new_default()
 * - конструктор hyscan_map_profile_new_full() с указанием всех параметров
 * - конструктор hyscan_map_profile_new(), а затем чтения ini-файла конфигурации
 *   с помощью функции hyscan_map_profile_read
 *
 * Переключение используемых профилей карты позволяет изменять подложку карты и
 * внешний вид слоёв.
 *
 * Чтобы применить профиль к карте необходимо воспользоваться функцией hyscan_map_profile_apply() -
 * её можно использовать как для первоначальной конфигурации виджета, так и для
 * изменения конфигурации карты во время выполнения. Применение профиля приводит
 * к размещению на карте слоя с тайлами #HyScanGtkMapTiles, настроенного на
 * загрузку тайлов по сети и их кэширование в некоторой папке на диске.
 *
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
  gchar                      **tiles_dir;   /* Имена подпапок внутри cache_dir для хранения загруженных тайлов. */
  gchar                      **url_format;  /* Формат URL тайла. */

  guint                        min_zoom;    /* Минимальный доступный уровень детализации. */
  guint                        max_zoom;    /* Максимальный доступный уровень детализации. */

  gchar                       *cache_dir;   /* Путь к директории, в которой хранится кэш тайлов. */
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
  g_free (priv->cache_dir);

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
      HyScanGtkMapTileSource *nw_source;
      HyScanGtkMapTileSource *source = NULL;

      gchar *cache_path = NULL;

      nw_source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_network_map_tile_source_new (priv->url_format[i],
                                                                                  projection,
                                                                                  priv->min_zoom,
                                                                                  priv->max_zoom));

      /* Если указана папка для кэширования, то используем источник HyscanGtkMapFsTileSource. */
      if (!g_str_equal (priv->tiles_dir[i], ""))
        {
          cache_path = g_build_path (G_DIR_SEPARATOR_S, priv->cache_dir, priv->tiles_dir[i], NULL);
          source = HYSCAN_GTK_MAP_TILE_SOURCE (hyscan_gtk_map_fs_tile_source_new (cache_path, nw_source));
        }
      else
        {
          source = g_object_ref (nw_source);
        }

      hyscan_merged_tile_source_append (merged_source, source);

      g_clear_object (&nw_source);
      g_clear_object (&source);
      g_free (cache_path);
    }

  cache = HYSCAN_CACHE (hyscan_cached_new (CACHE_SIZE));
  tiles = hyscan_gtk_map_tiles_new (cache, HYSCAN_GTK_MAP_TILE_SOURCE(merged_source));

  g_object_unref (cache);
  g_object_unref (merged_source);

  return HYSCAN_GTK_LAYER (tiles);
}

/**
 * hyscan_map_profile_new_full:
 * @url_format: формат URL тайла
 * @cache_dir: директория для кэширования тайлов
 * @cache_subdir: имя подпапки в @cache_dir
 * @projection: название проекции
 * @min_zoom: минимальный доступный уровень детализации
 * @max_zoom: максимальный доступный уровень детализации
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
                             const gchar   *cache_subdir,
                             const gchar   *projection,
                             guint          min_zoom,
                             guint          max_zoom)
{
  HyScanMapProfile *profile;
  gchar *url_formats[2];
  gchar *cache_dirs[2];

  url_formats[0] = (gchar *) url_format;
  url_formats[1] = NULL;

  cache_dirs[0] = cache_subdir == NULL ? "" : (gchar *) cache_subdir;
  cache_dirs[1] = NULL;

  profile = hyscan_map_profile_new (cache_dir);
  hyscan_gtk_map_profile_set_params (profile, title, url_formats, cache_dirs, projection, min_zoom, max_zoom);

  return profile;
}

/**
 * hyscan_map_profile_new_default:
 * @cache_dir: директория, в которой хранится кэш тайлов
 *
 * Создает профиль карты по умолчанию. Этот профиль может быть использован в случае,
 * если нет других пользовательских вариантов.
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanMapProfile *
hyscan_map_profile_new_default (const gchar *cache_dir)
{
  HyScanMapProfile *profile;
  gchar *url_formats[] = {"http://a.tile.openstreetmap.org/{z}/{x}/{y}.png", NULL};
  gchar *cache_dirs[]  = {"osm", NULL};

  profile = hyscan_map_profile_new (cache_dir);
  hyscan_gtk_map_profile_set_params (profile, "Default", url_formats, cache_dirs, PROJ_WEBMERC, 0, 19);

  return profile;
}

/**
 * hyscan_map_profile_new:
 * @cache_dir: директория, в которой хранится кэш тайлов
 *
 * Создаёт пустой профиль карты. Параметры профиля могут быть установлены через
 * hyscan_map_profile_read().
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanMapProfile *
hyscan_map_profile_new (const gchar *cache_dir)
{
  HyScanMapProfile *profile;

  profile =  g_object_new (HYSCAN_TYPE_MAP_PROFILE, NULL);

  if (cache_dir != NULL)
    profile->priv->cache_dir = g_strdup (cache_dir);
  else
    profile->priv->cache_dir = g_dir_make_tmp ("hyscan-map-XXXXXX", NULL);

  return profile;
}

/**
 * hyscan_map_profile_read:
 * @profile: указатель на профиль #HyScanMapProfile
 * @filename: путь к ini-файлу конфигурации
 *
 * Считавает конфигурацию профиля из файла @filename.
 *
 * Returns: %TRUE, если параметры профиля были успешно считанны
 */
gboolean
hyscan_map_profile_read (HyScanMapProfile *profile,
                         const gchar      *filename)
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
  
  g_key_file_load_from_file (priv->key_file, filename, G_KEY_FILE_NONE, &error);
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
 * @profile: указатель на #HyScanMapProfile
 * @map: указатель на карту #HyScanGtkMap
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
