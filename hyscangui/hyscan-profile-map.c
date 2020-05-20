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
 * - конструктор по умолчанию hyscan_profile_map_new_default(),
 * - конструктор hyscan_profile_map_new_full() с указанием всех параметров,
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
 * Пример конфигурации c заголовками сервера:
 * |[
 * [global]
 * title=My Example Server
 * url=https://example.com/tiles/{z}/{x}/{-y}.png
 * header0=Referer: https://example.com/
 * header1=Cookie: foo=bar
 * proj=webmerc
 * min_zoom=1
 * max_zoom=17
 * dir=example
 * ]|
 *
 * Пример конфигурации с наложением тайлов:
 * |[
 * [global]
 * title=Blend
 * url1=https://foo.com/tiles/{z}/{x}/{-y}.png
 * dir1=foo
 * url2=https://bar.com/tiles/{z}/{x}/{y}.png
 * dir2=bar
 * proj=webmerc
 * min_zoom=1
 * max_zoom=17
 * ]|
 *
 */

#include "hyscan-profile-map.h"
#include "hyscan-map-tile-source-web.h"
#include "hyscan-map-tile-source-file.h"
#include "hyscan-map-tile-source-blend.h"
#include "hyscan-gtk-map-base.h"
#include "hyscan-gtk-layer-param.h"
#include <hyscan-proj.h>
#include <string.h>

#define HYSCAN_PROFILE_MAP_VERSION 0X1343AA4
#define PROJ_MERC                  "merc"
#define PROJ_WEBMERC               "webmerc"
#define INI_PREFIX_URL             "url"
#define INI_PREFIX_DIR             "dir"
#define INI_PREFIX_HEADER          "header"
#define INI_GROUP                  "global"

typedef struct
{
  gchar       *url_format;  /* Формат URL тайла. */
  gchar       *cache_dir;   /* Имя подпапки внутри cache_dir для хранения загруженных тайлов. */
  gchar      **headers;     /* Заголовки запроса. */
} HyScanProfileMapSource;

struct _HyScanProfileMapPrivate
{
  GArray                *sources;           /* Источники тайлов #HyScanProfileMapSource. */
  gchar                 *projection;        /* Название проекции. */
  guint                  min_zoom;          /* Минимальный доступный уровень детализации. */
  guint                  max_zoom;          /* Максимальный доступный уровень детализации. */
  gchar                 *cache_dir;         /* Путь к директории, в которой хранится кэш тайлов. */

  gboolean               offline;           /* Признак того, что необходимо работать оффлайн. */

  HyScanGeoProjection   *geo_projection;    /* Объект географической проекции. */
  HyScanMapTileSource   *tile_source;       /* Объект источника тайлов. */
  HyScanParam           *layers_param;      /* Параметры слоёв карты. */
};

static void                 hyscan_profile_map_object_finalize     (GObject                       *object);
static void                 hyscan_profile_map_object_constructed  (GObject                       *object);
static gboolean             hyscan_profile_map_read                (HyScanProfile                 *profile,
                                                                    GKeyFile                      *file);
static gboolean             hyscan_profile_map_write               (HyScanProfile                 *profile,
                                                                    GKeyFile                      *file);
static HyScanGeoProjection *hyscan_profile_map_projection_create   (HyScanProfileMap              *profile);
static HyScanMapTileSource *hyscan_profile_map_source_create       (HyScanProfileMap              *profile);
static void                 hyscan_profile_map_source_params_read  (GKeyFile                      *file,
                                                                    const gchar                   *suffix,
                                                                    HyScanProfileMapSource        *params);
static void                 hyscan_profile_map_source_param_clear  (HyScanProfileMapSource        *params);
static void                 hyscan_profile_map_source_param_add    (HyScanProfileMap              *profile,
                                                                    const HyScanProfileMapSource  *source);
static gboolean             hyscan_profile_map_split_header        (gchar                         *header,
                                                                    gchar                        **name,
                                                                    gchar                        **value);
static HyScanMapTileSource *hyscan_profile_map_source_create_web   (HyScanProfileMap              *profile,
                                                                    HyScanProfileMapSource        *params);
static HyScanMapTileSource *hyscan_profile_map_source_wrap         (HyScanProfileMap              *profile,
                                                                    HyScanProfileMapSource        *params,
                                                                    HyScanMapTileSource           *fb_source);
static void                 hyscan_profile_map_configure           (HyScanProfileMap              *profile);
static void                 hyscan_profile_map_set_params          (HyScanProfileMap              *profile,
                                                                    const gchar                   *projection,
                                                                    guint                          min_zoom,
                                                                    guint                          max_zoom);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileMap, hyscan_profile_map, HYSCAN_TYPE_PROFILE)

static void
hyscan_profile_map_class_init (HyScanProfileMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanProfileClass *profile_class = HYSCAN_PROFILE_CLASS (klass);

  object_class->constructed = hyscan_profile_map_object_constructed;
  object_class->finalize = hyscan_profile_map_object_finalize;
  profile_class->version = HYSCAN_PROFILE_MAP_VERSION;
  profile_class->read = hyscan_profile_map_read;
  profile_class->write = hyscan_profile_map_write;
}

static void
hyscan_profile_map_init (HyScanProfileMap *map_profile)
{
  map_profile->priv = hyscan_profile_map_get_instance_private (map_profile);
}

static void
hyscan_profile_map_object_constructed (GObject *object)
{
  HyScanProfileMap *map_profile = HYSCAN_PROFILE_MAP (object);
  HyScanProfileMapPrivate *priv = map_profile->priv;

  G_OBJECT_CLASS (hyscan_profile_map_parent_class)->constructed (object);

  priv->sources = g_array_new (FALSE, FALSE, sizeof (HyScanProfileMapSource));
  g_array_set_clear_func (priv->sources, (GDestroyNotify) hyscan_profile_map_source_param_clear);
}

static void
hyscan_profile_map_object_finalize (GObject *object)
{
  HyScanProfileMap *map_profile = HYSCAN_PROFILE_MAP (object);
  HyScanProfileMapPrivate *priv = map_profile->priv;

  g_array_free (priv->sources, TRUE);
  g_free (priv->projection);
  g_free (priv->cache_dir);
  g_clear_object (&priv->geo_projection);
  g_clear_object (&priv->tile_source);
  g_clear_object (&priv->layers_param);

  G_OBJECT_CLASS (hyscan_profile_map_parent_class)->finalize (object);
}

/* Очищает поля структуры HyScanProfileMapSource. */
static void
hyscan_profile_map_source_param_clear (HyScanProfileMapSource *params)
{
  g_free (params->cache_dir);
  g_free (params->url_format);
  g_strfreev (params->headers);
}

/* Завершает конфигурацию: создаёт объекты геопроекции и источника тайлов согласно указанным параметрам. */
static void
hyscan_profile_map_configure (HyScanProfileMap *profile)
{
  HyScanProfileMapPrivate *priv = profile->priv;

  g_clear_object (&priv->geo_projection);
  g_clear_object (&priv->tile_source);
  priv->geo_projection = hyscan_profile_map_projection_create (profile);
  priv->tile_source = hyscan_profile_map_source_create (profile);
}

/* Добавляет параметры ещё одного источника тайлов. */
static void
hyscan_profile_map_source_param_add (HyScanProfileMap             *profile,
                                     const HyScanProfileMapSource *source)
{
  HyScanProfileMapPrivate *priv = profile->priv;
  HyScanProfileMapSource new_source;

  new_source.url_format = g_strdup (source->url_format);
  new_source.cache_dir = g_strdup (source->cache_dir);
  new_source.headers = source->headers != NULL ? g_strdupv (source->headers) : NULL;
  g_array_append_vals (priv->sources, &new_source, 1);
}

/* Устанавливает параметры профиля. */
static void
hyscan_profile_map_set_params (HyScanProfileMap  *profile,
                               const gchar       *projection,
                               guint              min_zoom,
                               guint              max_zoom)
{
  HyScanProfileMapPrivate *priv = profile->priv;

  g_free (priv->projection);
  priv->projection = g_strdup (projection);
  priv->min_zoom = min_zoom;
  priv->max_zoom = max_zoom;
}

/* Создаёт проекцию, соответствующую профилю. */
static HyScanGeoProjection *
hyscan_profile_map_projection_create (HyScanProfileMap *profile)
{
  HyScanProfileMapPrivate *priv = profile->priv;

  if (g_str_equal (priv->projection, PROJ_WEBMERC))
    return hyscan_proj_new (HYSCAN_PROJ_WEBMERC);

  if (g_str_equal (priv->projection, PROJ_MERC))
    return hyscan_proj_new (HYSCAN_PROJ_MERC);

  g_warning ("HyScanProfileMap: unknown projection %s", priv->projection);

  return hyscan_proj_new (priv->projection);
}

/* Разделяет HTTP-заголовок на ключ и значение. */
static gboolean
hyscan_profile_map_split_header (gchar  *header,
                                 gchar **name,
                                 gchar **value)
{
  gchar **name_value;
  gboolean ok;

  name_value = g_strsplit (header, ":", 2);
  ok = (g_strv_length (name_value) > 1);
  if (ok)
    {
      *name = g_strdup (name_value[0]);
      *value = g_strdup (name_value[1]);
    }

  g_strfreev (name_value);

  return ok;
}

/* Создаёт серверный источник тайлов по указанным параметрам. */
static HyScanMapTileSource *
hyscan_profile_map_source_create_web (HyScanProfileMap       *profile,
                                      HyScanProfileMapSource *params)
{
  HyScanProfileMapPrivate *priv = profile->priv;
  HyScanMapTileSourceWeb *source;
  gchar **headers;
  gint i;

  source = hyscan_map_tile_source_web_new (params->url_format, priv->geo_projection, priv->min_zoom, priv->max_zoom);

  if ((headers = params->headers) != NULL)
    {
      for (i = 0; headers[i] != NULL; i++)
        {
          gchar *name, *value;

          if (hyscan_profile_map_split_header (headers[i], &name, &value))
            {
              hyscan_map_tile_source_add_header (source, name, value);
              g_free (name);
              g_free (value);
            }
        }
    }

  return HYSCAN_MAP_TILE_SOURCE (source);
}

/* Делает кэширующую обертку для указанного источника тайлов. */
static HyScanMapTileSource *
hyscan_profile_map_source_wrap (HyScanProfileMap       *profile,
                                HyScanProfileMapSource *params,
                                HyScanMapTileSource    *fb_source)
{
  HyScanProfileMapPrivate *priv = profile->priv;
  HyScanMapTileSourceFile *file_source;
  gchar *cache_path;

  /* Если не указана папка для кэширования, то используем исходный источник. */
  if (g_str_equal (params->cache_dir, ""))
    return g_object_ref (fb_source);

  cache_path = g_build_path (G_DIR_SEPARATOR_S, priv->cache_dir, params->cache_dir, NULL);
  file_source = hyscan_map_tile_source_file_new (cache_path, fb_source);
  hyscan_map_tile_source_file_fb_enable (file_source, !priv->offline);
  g_free (cache_path);

  return HYSCAN_MAP_TILE_SOURCE (file_source);
}

/* Создаёт источник тайлов для профиля. */
static HyScanMapTileSource *
hyscan_profile_map_source_create (HyScanProfileMap *profile)
{
  HyScanProfileMapPrivate *priv = profile->priv;
  HyScanMapTileSourceBlend *merged_source;

  guint i;

  merged_source = hyscan_map_tile_source_blend_new ();
  for (i = 0; i < priv->sources->len; ++i)
    {
      HyScanMapTileSource *nw_source;
      HyScanMapTileSource *source = NULL;
      HyScanProfileMapSource params;

      params = g_array_index (priv->sources, HyScanProfileMapSource, i);
      nw_source = hyscan_profile_map_source_create_web (profile, &params);
      source = hyscan_profile_map_source_wrap (profile, &params, nw_source);
      hyscan_map_tile_source_blend_append (merged_source, source);

      g_clear_object (&nw_source);
      g_clear_object (&source);
    }

  return HYSCAN_MAP_TILE_SOURCE (merged_source);
}

/**
 * hyscan_profile_map_new_full:
 * @url_format: формат URL тайла
 * @cache_dir: директория для кэширования тайлов
 * @cache_subdir: имя подпапки в @cache_dir
 * @headers: NULL-терминированный массив заголовков запроса
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
hyscan_profile_map_new_full (const gchar  *title,
                             const gchar  *url_format,
                             const gchar  *cache_dir,
                             const gchar  *cache_subdir,
                             gchar       **headers,
                             const gchar  *projection,
                             guint         min_zoom,
                             guint         max_zoom)
{
  HyScanProfileMap *profile;
  HyScanProfileMapSource source;

  source.url_format = (gchar *) url_format;
  source.cache_dir = cache_subdir == NULL ? "" : (gchar *) cache_subdir;
  source.headers = g_strdupv (headers);

  profile = hyscan_profile_map_new (cache_dir, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), title);
  hyscan_profile_map_set_params (profile, projection, min_zoom, max_zoom);
  hyscan_profile_map_source_param_add (profile, &source);
  hyscan_profile_map_configure (profile);

  return profile;
}

static void
hyscan_profile_map_source_params_read (GKeyFile               *file,
                                       const gchar            *suffix,
                                       HyScanProfileMapSource *params)
{
  gchar *key;
  gchar **keys;
  GArray *headers;
  gint i;

  /* Получаем формат ссылки источника тайлов. */
  key = g_strdup_printf (INI_PREFIX_URL"%s", suffix);
  params->url_format = g_key_file_get_string (file, INI_GROUP, key, NULL);
  g_free (key);

  /* Находим соответствующую папку для кэширования. */
  key = g_strdup_printf (INI_PREFIX_DIR"%s", suffix);
  params->cache_dir = g_key_file_get_string (file, INI_GROUP, key, NULL);
  if (params->cache_dir == NULL)
    params->cache_dir = g_strdup ("");
  g_free (key);

  /* Считываем заголовки запроса. */
  key = g_strdup_printf (INI_PREFIX_HEADER"%s", suffix);
  keys = g_key_file_get_keys (file, INI_GROUP, NULL, NULL);
  headers = g_array_new (TRUE, FALSE, sizeof (gchar *));
  for (i = 0; keys[i] != NULL; ++i)
    {
      gchar *header;
      if (!g_str_has_prefix (keys[i], key))
        continue;

      header = g_key_file_get_string (file, INI_GROUP, keys[i], NULL);
      g_array_append_val (headers, header);
    }
  params->headers = (gchar**) g_array_free (headers, FALSE);
  g_strfreev (keys);
  g_free (key);
}

static gboolean
hyscan_profile_map_read (HyScanProfile *profile,
                         GKeyFile      *file)
{
  HyScanProfileMapPrivate *priv = HYSCAN_PROFILE_MAP (profile)->priv;
  gboolean result = FALSE;

  guint max_zoom, min_zoom;
  gchar *projection = NULL;

  GError *error = NULL;
  gchar **keys;
  gint i;

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
  g_array_remove_range (priv->sources, 0, priv->sources->len);
  keys = g_key_file_get_keys (file, INI_GROUP, NULL, NULL);
  for (i = 0; keys[i] != NULL; ++i)
    {
      gchar *suffix;
      HyScanProfileMapSource param;

      if (g_strrstr (keys[i], INI_PREFIX_URL) != keys[i])
        continue;

      suffix = keys[i] + strlen (INI_PREFIX_URL);
      hyscan_profile_map_source_params_read (file, suffix, &param);
      hyscan_profile_map_source_param_add (HYSCAN_PROFILE_MAP (profile), &param);
      hyscan_profile_map_source_param_clear (&param);
    }
  g_strfreev (keys);

  hyscan_profile_map_set_params (HYSCAN_PROFILE_MAP (profile), projection, min_zoom, max_zoom);
  hyscan_profile_map_configure (HYSCAN_PROFILE_MAP (profile));

  result = TRUE;

exit:
  if (error != NULL)
    {
      g_warning ("HyScanProfileMap: %s", error->message);
      g_clear_error (&error);
    }

  g_free (projection);

  return result;
}

static gboolean
hyscan_profile_map_write (HyScanProfile *profile,
                          GKeyFile      *key_file)
{
  HyScanProfileMapPrivate *priv = HYSCAN_PROFILE_MAP (profile)->priv;

  /* Записываем параметры профиля. */
  {
    guint i, j;

    g_key_file_set_string (key_file, INI_GROUP, "proj", priv->projection);
    g_key_file_set_uint64 (key_file, INI_GROUP, "min_zoom", priv->min_zoom);
    g_key_file_set_uint64 (key_file, INI_GROUP, "max_zoom", priv->max_zoom);

    for (i = 0; i < priv->sources->len; i++)
      {
        HyScanProfileMapSource *source;
        gchar *url_key, *dir_key, *suffix;

        source = &g_array_index (priv->sources, HyScanProfileMapSource, i);

        suffix = i == 0 ? g_strdup ("") : g_strdup_printf ("%d", i);
        url_key = g_strdup_printf (INI_PREFIX_URL"%s", suffix);
        dir_key = g_strdup_printf (INI_PREFIX_DIR"%s", suffix);

        g_key_file_set_string (key_file, INI_GROUP, url_key, source->url_format);
        g_key_file_set_string (key_file, INI_GROUP, dir_key, source->cache_dir);

        for (j = 0; source->headers != NULL && source->headers[j] != NULL; j++)
          {
            gchar *hdr_key;

            hdr_key = g_strdup_printf (INI_PREFIX_HEADER"%s%d", suffix, j);
            g_key_file_set_string (key_file, INI_GROUP, hdr_key, source->headers[j]);
            g_free (hdr_key);
          }

        g_free (suffix);
        g_free (url_key);
        g_free (dir_key);
      }
  }

  /* Записываем параметры карты. */
  if (priv->layers_param == NULL)
    return TRUE;

  {
    HyScanDataSchema *schema;
    HyScanParamList *list;
    const gchar * const * keys;
    gint i;

    list = hyscan_param_list_new ();
    schema = hyscan_param_schema (priv->layers_param);

    keys = hyscan_data_schema_list_keys (schema);
    for (i = 0; keys != NULL && keys[i] != NULL; i++)
      hyscan_param_list_add (list, keys[i]);

    hyscan_param_get (priv->layers_param, list);
    hyscan_gtk_layer_param_list_to_file (key_file, list, schema);

    g_object_unref (list);
    g_object_unref (schema);
  }

  return TRUE;
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
  HyScanProfileMapSource source;

  profile = hyscan_profile_map_new (cache_dir, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), "OpenStreetMap");
  hyscan_profile_map_set_params (profile, PROJ_WEBMERC, 0, 19);
  source.cache_dir = "osm";
  source.url_format = "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png";
  source.headers = NULL;
  hyscan_profile_map_source_param_add (profile, &source);
  hyscan_profile_map_configure (profile);

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

  profile = g_object_new (HYSCAN_TYPE_PROFILE_MAP, "file", file_name, NULL);

  if (cache_dir != NULL)
    profile->priv->cache_dir = g_strdup (cache_dir);
  else
    profile->priv->cache_dir = g_dir_make_tmp ("hyscan-map-XXXXXX", NULL);

  return profile;
}

/**
 * hyscan_profile_map_copy:
 * @profile: указатель на #HyScanProfileMap
 * @file_name: имя файла для хранения профиля
 *
 * Returns: (transfer full): копия профиля, для удаления g_object_unref().
 */
HyScanProfileMap *
hyscan_profile_map_copy (HyScanProfileMap *profile,
                         const gchar      *file_name)
{
  HyScanProfileMap *copy;
  HyScanProfileMapPrivate *priv, *copy_priv;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_MAP (profile), NULL);

  copy = g_object_new (HYSCAN_TYPE_PROFILE_MAP, "file", file_name, NULL);

  priv = profile->priv;
  copy_priv = copy->priv;

  copy_priv->cache_dir = g_strdup (priv->cache_dir);
  copy_priv->offline = priv->offline;
  hyscan_profile_set_name (HYSCAN_PROFILE (copy), hyscan_profile_get_name (HYSCAN_PROFILE (profile)));
  hyscan_profile_map_set_params (copy, priv->projection, priv->min_zoom, priv->max_zoom);
  for (i = 0; i < priv->sources->len; i++)
    {
      HyScanProfileMapSource *source;

      source = &g_array_index (priv->sources, HyScanProfileMapSource, i);
      hyscan_profile_map_source_param_add (copy, source);
    }
  hyscan_profile_map_configure (profile);

  return copy;
}

/**
 * hyscan_profile_map_set_offline:
 * @profile: указатель на #HyScanProfileMap
 * @offline: признак того, что необходимо работать оффлайн
 *
 * Установка режима работы оффлайн для профиля.
 */
void
hyscan_profile_map_set_offline (HyScanProfileMap *profile,
                                gboolean          offline)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_MAP (profile));

  profile->priv->offline = offline;
  hyscan_profile_map_configure (profile);
}

/**
 * hyscan_profile_map_set_cache_dir:
 * @profile: указатель на #HyScanProfileMap
 * @cache_dir: путь к директории для кэширования тайлов
 *
 * Установка пути к кэшу тайлов карты
 */
void
hyscan_profile_map_set_cache_dir (HyScanProfileMap   *profile,
                                  const gchar        *cache_dir)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_MAP (profile));

  g_free (profile->priv->cache_dir);
  profile->priv->cache_dir = g_strdup (cache_dir);
  hyscan_profile_map_configure (profile);
}

/**
 * hyscan_profile_map_apply:
 * @profile: указатель на #HyScanProfileMap
 * @map: указатель на карту #HyScanGtkMap
 * @base_layer_id: идентификатор слоя подложки
 *
 * Применяет профиль @profile к карте @map.
 */
gboolean
hyscan_profile_map_apply (HyScanProfileMap *profile,
                          HyScanGtkMap     *map,
                          const gchar      *base_layer_id)
{
  HyScanProfileMapPrivate *priv;
  HyScanGtkLayerContainer *container;
  HyScanGtkLayer *base;
  GKeyFile *key_file;
  const gchar *file_name;
  HyScanParam *param;
  HyScanDataSchema *schema;
  HyScanParamList *list;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_MAP (profile), FALSE);

  priv = profile->priv;
  container = HYSCAN_GTK_LAYER_CONTAINER (map);

  g_return_val_if_fail (priv->projection != NULL && priv->tile_source != NULL, FALSE);

  /* Заменяем проекцию. */
  hyscan_gtk_map_set_projection (map, priv->geo_projection);

  /* Обновляем источник тайлов в подложке. */
  base = base_layer_id != NULL ? hyscan_gtk_layer_container_lookup (container, base_layer_id) : NULL;
  if (HYSCAN_IS_GTK_MAP_BASE (base))
    hyscan_gtk_map_base_set_source (HYSCAN_GTK_MAP_BASE (base), priv->tile_source);

  /* Конфигурируем остальные слои. */
  param = hyscan_gtk_layer_container_get_param (container);
  schema = hyscan_param_schema (param);
  list = hyscan_param_list_new ();
  key_file = g_key_file_new ();
  file_name = hyscan_profile_get_file (HYSCAN_PROFILE (profile));
  if (file_name != NULL)
    g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, NULL);

  hyscan_gtk_layer_param_file_to_list (key_file, list, schema);
  hyscan_param_set (param, list);

  g_key_file_unref (key_file);
  g_object_unref (param);
  g_object_unref (schema);
  g_object_unref (list);

  gtk_widget_queue_draw (GTK_WIDGET (map));

  return TRUE;
}

/**
 * hyscan_profile_map_set_param:
 * @profile: указатель на #HyScanProfileMap
 * @layers_param: параметры слоёв карты #HyScanParam
 *
 * Устанавливает параметры слоёв карты.
 */
void
hyscan_profile_map_set_param (HyScanProfileMap *profile,
                              HyScanParam      *layers_param)
{
  HyScanProfileMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PROFILE_MAP (profile));
  priv = profile->priv;

  g_clear_object (&priv->layers_param);
  priv->layers_param = layers_param != NULL ? g_object_ref (layers_param) : NULL;
}

/**
 * hyscan_profile_map_get_source:
 * @profile: указатель на профиль карты HyScanProfileMap
 *
 * Returns: возвращает указатель на объект источника тайлов #HyScanMapTileSource. Для удаления g_object_unref()
 */
HyScanMapTileSource *
hyscan_profile_map_get_source (HyScanProfileMap *profile)
{
  HyScanProfileMapPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_MAP (profile), NULL);
  priv = profile->priv;

  return g_object_ref (priv->tile_source);
}
