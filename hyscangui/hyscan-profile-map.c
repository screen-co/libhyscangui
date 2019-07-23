/* hyscan-profile-map.c
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
 * SECTION: hyscan-profile-map
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
 * - конструктор по умолчанию hyscan_profile_map_new_default()
 * - конструктор hyscan_profile_map_new_full() с указанием всех параметров
 * - конструктор hyscan_profile_map_new(), а затем чтения ini-файла конфигурации
 *   с помощью функции hyscan_profile_read().
 *
 * Переключение используемых профилей карты позволяет изменять подложку карты и
 * внешний вид слоёв.
 *
 * Чтобы применить профиль к карте необходимо воспользоваться функцией hyscan_profile_map_apply() -
 * её можно использовать как для первоначальной конфигурации виджета, так и для
 * изменения конфигурации карты во время выполнения. Применение профиля приводит
 * к размещению на карте слоя подложки #HyScanGtkMapBase, настроенного на
 * загрузку тайлов по сети и их кэширование в некоторой папке на диске.
 *
 * Слою подложки будет присвоен идентификатор %HYSCAN_PROFILE_MAP_BASE_ID.
 *
 */

#include "hyscan-profile-map.h"
#include "hyscan-map-tile-source-web.h"
#include "hyscan-map-tile-source-file.h"
#include "hyscan-map-tile-source-blend.h"
#include "hyscan-gtk-map-base.h"
#include <hyscan-pseudo-mercator.h>
#include <hyscan-mercator.h>
#include <hyscan-cached.h>
#include <string.h>

#define CACHE_SIZE     256
#define PROJ_MERC      "merc"
#define PROJ_WEBMERC   "webmerc"
#define INI_PREFIX_URL "url"
#define INI_PREFIX_DIR "dir"
#define INI_GROUP      "global"

struct _HyScanProfileMapPrivate
{
  gchar                       *projection;  /* Название проекции. */
  gchar                      **tiles_dir;   /* Имена подпапок внутри cache_dir для хранения загруженных тайлов. */
  gchar                      **url_format;  /* Формат URL тайла. */
  gboolean                     offline;     /* Признак того, что необходимо работать оффлайн. */

  guint                        min_zoom;    /* Минимальный доступный уровень детализации. */
  guint                        max_zoom;    /* Максимальный доступный уровень детализации. */

  gchar                       *cache_dir;   /* Путь к директории, в которой хранится кэш тайлов. */
};

static void      hyscan_profile_map_object_finalize          (GObject                     *object);
static void      hyscan_profile_map_object_constructed       (GObject                     *object);
static gboolean  hyscan_profile_map_read                     (HyScanProfile               *profile,
                                                              GKeyFile                    *file);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileMap, hyscan_profile_map, HYSCAN_TYPE_PROFILE)

static void
hyscan_profile_map_class_init (HyScanProfileMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanProfileClass *profile_class = HYSCAN_PROFILE_CLASS (klass);

  object_class->constructed = hyscan_profile_map_object_constructed;
  object_class->finalize = hyscan_profile_map_object_finalize;
  profile_class->read = hyscan_profile_map_read;
}

static void
hyscan_profile_map_init (HyScanProfileMap *map_profile)
{
  map_profile->priv = hyscan_profile_map_get_instance_private (map_profile);
}

static void
hyscan_profile_map_object_constructed (GObject *object)
{
  G_OBJECT_CLASS (hyscan_profile_map_parent_class)->constructed (object);
}

static void
hyscan_profile_map_object_finalize (GObject *object)
{
  HyScanProfileMap *map_profile = HYSCAN_PROFILE_MAP (object);
  HyScanProfileMapPrivate *priv = map_profile->priv;

  g_strfreev (priv->url_format);
  g_strfreev (priv->tiles_dir);
  g_free (priv->projection);
  g_free (priv->cache_dir);

  G_OBJECT_CLASS (hyscan_profile_map_parent_class)->finalize (object);
}

/* Устанавливает параметры профиля.
 * url_format и cache_dir - нуль-терминированные массивы строк одинаковой длины;
 * если для какого-то источника папка кэширования не указана, то указывается
 * пустая строка "" != NULL. */
static void
hyscan_gtk_map_profile_set_params (HyScanProfileMap *profile,
                                   const gchar      *title,
                                   gchar           **url_format,
                                   gchar           **cache_dir,
                                   const gchar      *projection,
                                   guint             min_zoom,
                                   guint             max_zoom)
{
  HyScanProfileMapPrivate *priv = profile->priv;

  hyscan_profile_set_name (HYSCAN_PROFILE (profile), title);
  priv->url_format = g_strdupv (url_format);
  priv->tiles_dir = g_strdupv (cache_dir);
  priv->projection = g_strdup (projection);
  priv->min_zoom = min_zoom;
  priv->max_zoom = max_zoom;
}

/* Создаёт проекцию, соответствующую профилю. */
static HyScanGeoProjection *
hyscan_profile_map_create_projection (HyScanProfileMapPrivate *priv)
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

  g_warning ("HyScanProfileMap: unknown projection %s", priv->projection);

  return FALSE;
}

/* Создаёт тайловый слой, соответствующий профилю. */
static HyScanGtkLayer *
hyscan_profile_map_create_base (HyScanProfileMapPrivate *priv,
                                HyScanGeoProjection     *projection)
{
  HyScanGtkLayer *base;

  HyScanCache *cache;
  HyScanMapTileSourceBlend *merged_source;

  gint i;

  merged_source = hyscan_map_tile_source_blend_new ();
  for (i = 0; priv->url_format[i] != NULL; ++i)
    {
      HyScanMapTileSource *nw_source;
      HyScanMapTileSource *source = NULL;

      gchar *cache_path = NULL;

      nw_source = HYSCAN_MAP_TILE_SOURCE (hyscan_map_tile_source_web_new (priv->url_format[i],
                                                                          projection,
                                                                          priv->min_zoom,
                                                                          priv->max_zoom));

      /* Если указана папка для кэширования, то используем источник HyscanMapTileSourceFile. */
      if (!g_str_equal (priv->tiles_dir[i], ""))
        {
          HyScanMapTileSourceFile *file_source;

          cache_path = g_build_path (G_DIR_SEPARATOR_S, priv->cache_dir, priv->tiles_dir[i], NULL);
          file_source = hyscan_map_tile_source_file_new (cache_path, nw_source);
          hyscan_map_tile_source_file_fb_enable (file_source, !priv->offline);

          source = HYSCAN_MAP_TILE_SOURCE (file_source);
        }
      else
        {
          source = g_object_ref (nw_source);
        }

      hyscan_map_tile_source_blend_append (merged_source, source);

      g_clear_object (&nw_source);
      g_clear_object (&source);
      g_free (cache_path);
    }

  cache = HYSCAN_CACHE (hyscan_cached_new (CACHE_SIZE));
  base = hyscan_gtk_map_base_new (cache, HYSCAN_MAP_TILE_SOURCE (merged_source));

  g_object_unref (cache);
  g_object_unref (merged_source);

  return base;
}

/**
 * hyscan_profile_map_new_full:
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
HyScanProfileMap *
hyscan_profile_map_new_full (const gchar   *title,
                             const gchar   *url_format,
                             const gchar   *cache_dir,
                             const gchar   *cache_subdir,
                             const gchar   *projection,
                             guint          min_zoom,
                             guint          max_zoom)
{
  HyScanProfileMap *profile;
  gchar *url_formats[2];
  gchar *cache_dirs[2];

  url_formats[0] = (gchar *) url_format;
  url_formats[1] = NULL;

  cache_dirs[0] = cache_subdir == NULL ? "" : (gchar *) cache_subdir;
  cache_dirs[1] = NULL;

  profile = hyscan_profile_map_new (cache_dir, NULL);
  hyscan_gtk_map_profile_set_params (profile, title, url_formats, cache_dirs, projection, min_zoom, max_zoom);

  return profile;
}

/**
 * hyscan_profile_map_new_default:
 * @cache_dir: директория, в которой хранится кэш тайлов
 *
 * Создает профиль карты по умолчанию. Этот профиль может быть использован в случае,
 * если нет других пользовательских вариантов.
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanProfileMap *
hyscan_profile_map_new_default (const gchar *cache_dir)
{
  HyScanProfileMap *profile;
  gchar *url_formats[] = {"http://a.tile.openstreetmap.org/{z}/{x}/{y}.png", NULL};
  gchar *cache_dirs[]  = {"osm", NULL};

  profile = hyscan_profile_map_new (cache_dir, NULL);
  hyscan_gtk_map_profile_set_params (profile, "OpenStreetMap", url_formats, cache_dirs, PROJ_WEBMERC, 0, 19);

  return profile;
}

/**
 * hyscan_profile_map_new:
 * @cache_dir: директория, в которой хранится кэш тайлов
 * @file_name: (nullable): путь к файлу конфигурации или %NULL
 *
 * Создаёт профиль карты из файла @file_name. Параметры профиля могут быть установлены через
 * hyscan_profile_read().
 *
 * Returns: указатель на новый профиль. Для удаления g_object_unref().
 */
HyScanProfileMap *
hyscan_profile_map_new (const gchar *cache_dir,
                        const gchar *file_name)
{
  HyScanProfileMap *profile;

  profile =  g_object_new (HYSCAN_TYPE_MAP_PROFILE, "file", file_name, NULL);

  if (cache_dir != NULL)
    profile->priv->cache_dir = g_strdup (cache_dir);
  else
    profile->priv->cache_dir = g_dir_make_tmp ("hyscan-map-XXXXXX", NULL);

  return profile;
}

/**
 * hyscan_profile_map_set_offline:
 * @profile: указатель на #HyScanProfileMap
 * @offline: признак того, что необходимо работать оффлайн
 *
 * Установка режима работы оффлайн для профиля.
 */
void
hyscan_profile_map_set_offline (HyScanProfileMap   *profile,
                                gboolean            offline)
{
  g_return_if_fail (HYSCAN_IS_MAP_PROFILE (profile));

  profile->priv->offline = offline;
}

static gboolean
hyscan_profile_map_read (HyScanProfile *profile,
                         GKeyFile      *file)
{
  gboolean result = FALSE;

  guint max_zoom, min_zoom;
  gchar *title = NULL, *projection = NULL;

  GError *error = NULL;
  GArray *url_formats_arr, *cache_dirs_arr;
  gchar **url_formats = NULL, **cache_dirs = NULL;

  gchar **keys;
  gint i;

  title = g_key_file_get_string (file, INI_GROUP, "title", &error);
  if (error != NULL)
    goto exit;

  projection = g_key_file_get_string (file, INI_GROUP, "proj", &error);
  if (error != NULL)
    goto exit;

  min_zoom = g_key_file_get_uint64 (file, INI_GROUP, "min_zoom", &error);
  if (error != NULL)
    goto exit;

  max_zoom = g_key_file_get_uint64 (file, INI_GROUP, "max_zoom", &error);
  if (error != NULL)
    goto exit;

  /* Ищем пары ключей url{suffix} и dir{suffix} - формат ссылки на тайл и папку кэширования.
   * Например: url_osm - dir_osm, url1 - dir1.
   * */
  url_formats_arr = g_array_new (TRUE, FALSE, sizeof (gchar *));
  cache_dirs_arr  = g_array_new (TRUE, FALSE, sizeof (gchar *));
  keys = g_key_file_get_keys (file, INI_GROUP, NULL, NULL);
  for (i = 0; keys[i] != NULL; ++i)
    {
      gchar *url_format, *cache_dir;
      gchar *suffix;
      gchar *dir_key;

      if (g_strrstr (keys[i], INI_PREFIX_URL) != keys[i])
        continue;

      suffix = keys[i] + strlen (INI_PREFIX_URL);

      /* Получаем формат ссылки источника тайлов. */
      url_format = g_key_file_get_string (file, INI_GROUP, keys[i], NULL);
      if (url_format == NULL)
        continue;

      /* Находим соответствующую папку для кэширования. */
      dir_key = g_strdup_printf (INI_PREFIX_DIR"%s", suffix);
      cache_dir = g_key_file_get_string (file, INI_GROUP, dir_key, NULL);
      if (cache_dir == NULL)
        cache_dir = g_strdup ("");

      g_free (dir_key);

      g_array_append_val (url_formats_arr, url_format);
      g_array_append_val (cache_dirs_arr, cache_dir);
    }
  url_formats = (gchar**) g_array_free (url_formats_arr, FALSE);
  cache_dirs  = (gchar**) g_array_free (cache_dirs_arr, FALSE);
  g_strfreev (keys);

  hyscan_gtk_map_profile_set_params (HYSCAN_PROFILE_MAP (profile), title, url_formats, cache_dirs,
                                     projection, min_zoom, max_zoom);

  g_strfreev (url_formats);
  g_strfreev (cache_dirs);

  result = TRUE;

exit:
  if (error != NULL)
    {
      g_warning ("HyScanProfileMap: %s", error->message);
      g_clear_error (&error);
    }

  g_free (title);
  g_free (projection);

  return result;
}

/**
 * hyscan_profile_map_apply:
 * @profile: указатель на #HyScanProfileMap
 * @map: указатель на карту #HyScanGtkMap
 *
 * Применяет профиль @profile к карте @map.
 */
gboolean
hyscan_profile_map_apply (HyScanProfileMap *profile,
                          HyScanGtkMap     *map)
{
  HyScanProfileMapPrivate *priv;
  HyScanGtkLayerContainer *container;
  HyScanGeoProjection *projection;
  const gchar *file_name;

  g_return_val_if_fail (HYSCAN_IS_MAP_PROFILE (profile), FALSE);

  priv = profile->priv;
  container = HYSCAN_GTK_LAYER_CONTAINER (map);

  projection = hyscan_profile_map_create_projection (priv);
  if (projection == NULL)
    return FALSE;

  /* Заменяем проекцию. */
  hyscan_gtk_map_set_projection (map, projection);

  /* Устанавливаем новый слой тайлов в самый низ. */
  hyscan_gtk_layer_container_add (container,
                                  hyscan_profile_map_create_base (priv, projection),
                                  HYSCAN_PROFILE_MAP_BASE_ID);

  /* Конфигурируем остальные слои. */
  file_name = hyscan_profile_get_file (HYSCAN_PROFILE (profile));
  if (file_name != NULL)
    {
      GKeyFile *key_file;

      key_file = g_key_file_new ();
      g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, NULL);
      hyscan_gtk_layer_container_load_key_file (container, key_file);
      g_key_file_unref (key_file);
    }

  gtk_widget_queue_draw (GTK_WIDGET (map));

  g_object_unref (projection);

  return TRUE;
}
