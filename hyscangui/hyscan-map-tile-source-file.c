/* hyscan-map-tile-source-file.c
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
 * SECTION: hyscan-map-tile-source-file
 * @Short_description: Файловый источник тайлов
 * @Title: HyScanMapTileSourceFile
 * @See_also: #HyScanMapTileSource
 *
 * Класс реализует интерфейс загрузки тайлов, загружая их из директории на
 * диске пользователя. Для создания объекта используется функция
 * hyscan_map_tile_source_file_new().
 *
 * Если класс #HyScanMapTileSourceFile не находит файл с изображением тайла в
 * каталоге тайлов на диске, то он загружает тайл из источника @fb_source, а
 * затем сохранят загруженный тайл в каталоге. Таким образом класс может быть
 * использован в качестве кэша тайлов для других источников.
 *
 */

#include "hyscan-map-tile-source-file.h"

enum
{
  PROP_O,
  PROP_SOURCE_DIR,
  PROP_FALLBACK_SOURCE,
};

struct _HyScanMapTileSourceFilePrivate
{
  gchar                       *source_dir;        /* Каталог для хранения тайлов. */
  HyScanMapTileSource         *fallback_source;   /* Источник тайлов на случай, если файл с тайлом отсутствует. */
  gboolean                     fallback_enabled;  /* Признак того, что fallback-источник можно использовать. */
};

static void       hyscan_map_tile_source_file_interface_init           (HyScanMapTileSourceInterface    *iface);
static void       hyscan_map_tile_source_file_set_property             (GObject                         *object,
                                                                        guint                            prop_id,
                                                                        const GValue                    *value,
                                                                        GParamSpec                      *pspec);
static void       hyscan_map_tile_source_file_object_finalize          (GObject                         *object);
static gboolean   hyscan_map_tile_source_file_fill_from_file           (HyScanMapTile                   *tile,
                                                                        const gchar                     *tile_path);
static gboolean   hyscan_map_tile_source_file_fill_tile                (HyScanMapTileSource             *source,
                                                                        HyScanMapTile                   *tile,
                                                                        GCancellable                    *cancellable);
static gboolean   hyscan_map_tile_source_file_save                     (HyScanMapTile                *tile,
                                                                        const gchar                     *tile_path);

G_DEFINE_TYPE_WITH_CODE (HyScanMapTileSourceFile, hyscan_map_tile_source_file, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMapTileSourceFile)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_MAP_TILE_SOURCE, hyscan_map_tile_source_file_interface_init))

static void
hyscan_map_tile_source_file_class_init (HyScanMapTileSourceFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_tile_source_file_set_property;

  object_class->finalize = hyscan_map_tile_source_file_object_finalize;

  g_object_class_install_property (object_class, PROP_FALLBACK_SOURCE,
    g_param_spec_object ("fallback-source", "Fallback tile source", "HyScanMapTileSource",
                         HYSCAN_TYPE_MAP_TILE_SOURCE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SOURCE_DIR,
    g_param_spec_string ("dir", "Source dir", "Directory containing tiles", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_map_tile_source_file_init (HyScanMapTileSourceFile *map_tile_source_file)
{
  map_tile_source_file->priv = hyscan_map_tile_source_file_get_instance_private (map_tile_source_file);
}

static void
hyscan_map_tile_source_file_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  HyScanMapTileSourceFile *map_tile_source_file = HYSCAN_MAP_TILE_SOURCE_FILE (object);
  HyScanMapTileSourceFilePrivate *priv = map_tile_source_file->priv;

  switch (prop_id)
    {
    case PROP_SOURCE_DIR:
      priv->source_dir = g_value_dup_string (value);
      break;

    case PROP_FALLBACK_SOURCE:
      priv->fallback_source = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_map_tile_source_file_object_finalize (GObject *object)
{
  HyScanMapTileSourceFile *map_tile_source_file = HYSCAN_MAP_TILE_SOURCE_FILE (object);
  HyScanMapTileSourceFilePrivate *priv = map_tile_source_file->priv;

  g_clear_object (&priv->fallback_source);
  g_free (priv->source_dir);

  G_OBJECT_CLASS (hyscan_map_tile_source_file_parent_class)->finalize (object);
}

/* Загружает поверхность тайла из файла tile_path. */
static gboolean
hyscan_map_tile_source_file_fill_from_file (HyScanMapTile *tile,
                                            const gchar   *tile_path)
{
  GdkPixbuf *pixbuf;
  gboolean success;

  if (!g_file_test (tile_path, G_FILE_TEST_EXISTS))
    return FALSE;

  /* Если файл существует, то загружаем поверхность тайла из него. */
  pixbuf = gdk_pixbuf_new_from_file (tile_path, NULL);
  if (pixbuf == NULL)
    return FALSE;

  success = hyscan_map_tile_set_pixbuf (tile, pixbuf);
  g_object_unref (pixbuf);

  return success;
}

/* Сохраняет файл с тайлом на диск. */
static gboolean
hyscan_map_tile_source_file_save (HyScanMapTile *tile,
                                  const gchar   *tile_path)
{
  gchar *tile_dir;
  gchar *tile_path_locale;
  cairo_surface_t *surface;
  cairo_status_t status;

  /* Создаём подкаталог для записи тайла. */
  tile_dir = g_path_get_dirname (tile_path);

#ifdef G_OS_WIN32
  tile_path_locale = g_win32_locale_filename_from_utf8 (tile_path); 
#else
  tile_path_locale = g_strdup (tile_path);
#endif

  if (g_mkdir_with_parents (tile_dir, 0755) == -1)
    g_warning ("HyScanMapTileSourceFile: failed to create dir %s", tile_dir);

  g_free (tile_dir);

  /* Записываем PNG-файл с тайлом. */
  surface = hyscan_map_tile_get_surface (tile);
  status = cairo_surface_write_to_png (surface, tile_path_locale);
  cairo_surface_destroy (surface);
  g_free (tile_path_locale);

  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_warning ("HyScanMapTileSourceFile: failed to save tile, %s", cairo_status_to_string (status));
      return FALSE;
    }

  return TRUE;
}

/* Ищет указанный тайл и загружает его изображение.
   Реализация #HyScanMapTileSourceInterface.fill_tile. */
static gboolean
hyscan_map_tile_source_file_fill_tile (HyScanMapTileSource *source,
                                       HyScanMapTile       *tile,
                                       GCancellable        *cancellable)
{
  HyScanMapTileSourceFile *fsource = HYSCAN_MAP_TILE_SOURCE_FILE (source);
  HyScanMapTileSourceFilePrivate *priv = fsource->priv;

  gboolean success;
  gchar *tile_path;

  if (g_cancellable_is_cancelled (cancellable))
    return FALSE;

  /* Путь к файлу с тайлом. */
  tile_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d" G_DIR_SEPARATOR_S "%d.png",
                               priv->source_dir,
                               hyscan_map_tile_get_zoom (tile),
                               hyscan_map_tile_get_x (tile),
                               hyscan_map_tile_get_y (tile));

  /* Пробуем загрузить из файла. */
  success = hyscan_map_tile_source_file_fill_from_file (tile, tile_path);

  if (!g_atomic_int_get (&priv->fallback_enabled))
    return success;

  /* Если файл отсутствует, то загружаем поверхность из запасного источника. */
  if (!success && priv->fallback_source != NULL)
    {
      success = hyscan_map_tile_source_fill (priv->fallback_source, tile, cancellable);
      if (success)
        hyscan_map_tile_source_file_save (tile, tile_path);
    }

  g_free (tile_path);

  return success;
}

/* Реализация #HyScanMapTileSourceInterface.get_grid. */
static HyScanMapTileGrid *
hyscan_map_tile_source_file_get_grid (HyScanMapTileSource *source)
{
  HyScanMapTileSourceFilePrivate *priv = HYSCAN_MAP_TILE_SOURCE_FILE (source)->priv;

  g_return_val_if_fail (priv->fallback_source != NULL, NULL);

  return hyscan_map_tile_source_get_grid (priv->fallback_source);
}

/* Реализация #HyScanMapTileSourceInterface.get_projection. */
static HyScanGeoProjection *
hyscan_map_tile_source_file_get_projection (HyScanMapTileSource *source)
{
  HyScanMapTileSourceFilePrivate *priv = HYSCAN_MAP_TILE_SOURCE_FILE (source)->priv;

  g_return_val_if_fail (priv->fallback_source != NULL, NULL);

  return hyscan_map_tile_source_get_projection (priv->fallback_source);
}

static void
hyscan_map_tile_source_file_interface_init (HyScanMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_map_tile_source_file_fill_tile;
  iface->get_grid = hyscan_map_tile_source_file_get_grid;
  iface->get_projection = hyscan_map_tile_source_file_get_projection;
}

/**
 * hyscan_map_tile_source_file_new:
 * @dir: каталог для хранения тайлов
 * @fb_source: запасной источник тайлов #HyScanMapTileSource
 *
 * Создаёт новый файловый источник тайлов. Источник будет искать тайлы в каталоге
 * @dir; в случае, если файла с тайлом не окажется, тайл будет загружен из источника
 * @fb_source и сохранён в каталоге тайлов.
 *
 * Returns: новый объект #HyScanMapTileSourceFile. Для удаления g_object_unref()
 */
HyScanMapTileSourceFile *
hyscan_map_tile_source_file_new (const gchar         *dir,
                                 HyScanMapTileSource *fb_source)
{
  HyScanMapTileSourceFile *fs_source;

  fs_source = g_object_new (HYSCAN_TYPE_MAP_TILE_SOURCE_FILE,
                            "dir", dir,
                            "fallback-source", fb_source,
                            NULL);

  hyscan_map_tile_source_file_fb_enable (fs_source, TRUE);

  return fs_source;
}

/**
 * hyscan_map_tile_source_file_fb_enable:
 * @fs_source: указатель на #HyScanMapTileSourceFile
 * @enable: признак того, можно ли использовать fallback-источник
 *
 * Определяет возможность использования запасного источника "fallback-source",
 * указанного при создании объекта.
 *
 * Если @enable = %FALSE, то при отстуствии файла с запрошенным тайлом
 * #HyScanMapTileSourceFile не будет обращаться к запасному источнику и вернёт
 * %FALSE.
 *
 * Функция позволяет ограничить доступ к запасному истоничку, например, если его
 * использование связано с какими-либо затратами.
 */
void
hyscan_map_tile_source_file_fb_enable (HyScanMapTileSourceFile *fs_source,
                                       gboolean                 enable)
{
  g_return_if_fail (HYSCAN_IS_MAP_TILE_SOURCE_FILE (fs_source));

  g_atomic_int_set (&fs_source->priv->fallback_enabled, enable);
}
