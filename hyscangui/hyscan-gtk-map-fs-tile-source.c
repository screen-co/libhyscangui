#include "hyscan-gtk-map-fs-tile-source.h"

enum
{
  PROP_O,
  PROP_SOURCE_DIR,
  PROP_FALLBACK_SOURCE,
};

struct _HyScanGtkMapFsTileSourcePrivate
{
  gchar                       *source_dir;        /* Каталог для хранения тайлов. */
  HyScanGtkMapTileSource      *fallback_source;   /* Источник тайлов на случай, если файл с тайлом отсутствует. */
};

static void       hyscan_gtk_map_fs_tile_source_interface_init           (HyScanGtkMapTileSourceInterface *iface);
static void       hyscan_gtk_map_fs_tile_source_set_property             (GObject                         *object,
                                                                          guint                            prop_id,
                                                                          const GValue                    *value,
                                                                          GParamSpec                      *pspec);
static void       hyscan_gtk_map_fs_tile_source_object_constructed       (GObject                         *object);
static void       hyscan_gtk_map_fs_tile_source_object_finalize          (GObject                         *object);
static gboolean   hyscan_gtk_map_fs_tile_source_fill_from_file           (HyScanGtkMapTile                *tile,
                                                                          const gchar                     *tile_path);
static gboolean   hyscan_gtk_map_fs_tile_source_fill_tile                (HyScanGtkMapTileSource          *source,
                                                                          HyScanGtkMapTile                *tile);
static gboolean   hyscan_gtk_map_fs_tile_source_save                     (HyScanGtkMapTile                *tile,
                                                                          const gchar                     *tile_path);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapFsTileSource, hyscan_gtk_map_fs_tile_source, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapFsTileSource)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, hyscan_gtk_map_fs_tile_source_interface_init))

static void
hyscan_gtk_map_fs_tile_source_class_init (HyScanGtkMapFsTileSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_fs_tile_source_set_property;

  object_class->constructed = hyscan_gtk_map_fs_tile_source_object_constructed;
  object_class->finalize = hyscan_gtk_map_fs_tile_source_object_finalize;

  g_object_class_install_property (object_class, PROP_FALLBACK_SOURCE,
    g_param_spec_object ("fallback-source", "Fallback tile source", "HyScanGtkMapTileSource",
                         HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SOURCE_DIR,
    g_param_spec_string ("dir", "Source dir", "Directory containing tiles", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_fs_tile_source_init (HyScanGtkMapFsTileSource *gtk_map_fs_tile_source)
{
  gtk_map_fs_tile_source->priv = hyscan_gtk_map_fs_tile_source_get_instance_private (gtk_map_fs_tile_source);
}

static void
hyscan_gtk_map_fs_tile_source_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  HyScanGtkMapFsTileSource *gtk_map_fs_tile_source = HYSCAN_GTK_MAP_FS_TILE_SOURCE (object);
  HyScanGtkMapFsTileSourcePrivate *priv = gtk_map_fs_tile_source->priv;

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
hyscan_gtk_map_fs_tile_source_object_constructed (GObject *object)
{
  HyScanGtkMapFsTileSource *gtk_map_fs_tile_source = HYSCAN_GTK_MAP_FS_TILE_SOURCE (object);

  G_OBJECT_CLASS (hyscan_gtk_map_fs_tile_source_parent_class)->constructed (object);
}

static void
hyscan_gtk_map_fs_tile_source_object_finalize (GObject *object)
{
  HyScanGtkMapFsTileSource *gtk_map_fs_tile_source = HYSCAN_GTK_MAP_FS_TILE_SOURCE (object);
  HyScanGtkMapFsTileSourcePrivate *priv = gtk_map_fs_tile_source->priv;

  g_clear_object (&priv->fallback_source);
  g_free (priv->source_dir);

  G_OBJECT_CLASS (hyscan_gtk_map_fs_tile_source_parent_class)->finalize (object);
}

/* Загружает поверхность тайла из файла tile_path. */
static gboolean
hyscan_gtk_map_fs_tile_source_fill_from_file (HyScanGtkMapTile *tile,
                                              const gchar      *tile_path)
{
  GdkPixbuf *pixbuf;
  gboolean success;

  if (!g_file_test (tile_path, G_FILE_TEST_EXISTS))
    return FALSE;

  /* Если файл существует, то загружаем поверхность тайла из него. */
  pixbuf = gdk_pixbuf_new_from_file (tile_path, NULL);
  if (pixbuf == NULL)
    return FALSE;

  success = hyscan_gtk_map_tile_set_pixbuf (tile, pixbuf);
  g_object_unref (pixbuf);

  return success;
}

/* Сохраняет файл с тайлом на диск. */
static gboolean
hyscan_gtk_map_fs_tile_source_save (HyScanGtkMapTile *tile,
                                    const gchar      *tile_path)
{
  GError *write_error = NULL;
  GdkPixbuf *pixbuf;

  gchar *tile_dir;

  /* Создаём подкаталог для записи тайла. */
  tile_dir = g_path_get_dirname (tile_path);
  g_mkdir_with_parents (tile_dir, 0755);
  g_free (tile_dir);

  /* Записываем PNG-файл с тайлом. */
  pixbuf = hyscan_gtk_map_tile_get_pixbuf (tile);
  gdk_pixbuf_save (pixbuf, tile_path, "png", &write_error, NULL);

  if (write_error != NULL)
    {
      g_warning ("HyScanGtkMapFsTileSource: failed to save tile, %s", write_error->message);
      g_clear_error (&write_error);

      return FALSE;
    }

  return TRUE;
}

/* Ищет указанный тайл и загружает его изображение. */
static gboolean
hyscan_gtk_map_fs_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                         HyScanGtkMapTile       *tile)
{
  HyScanGtkMapFsTileSource *fsource = HYSCAN_GTK_MAP_FS_TILE_SOURCE (source);
  HyScanGtkMapFsTileSourcePrivate *priv = fsource->priv;

  gboolean success;
  gchar *tile_path;

  /* Путь к файлу с тайлом. */
  tile_path = g_strdup_printf ("%s/%d/%d/%d.png",
                               priv->source_dir,
                               hyscan_gtk_map_tile_get_zoom (tile),
                               hyscan_gtk_map_tile_get_x (tile),
                               hyscan_gtk_map_tile_get_y (tile));

  /* Пробуем загрузить из файла. */
  success = hyscan_gtk_map_fs_tile_source_fill_from_file (tile, tile_path);

  /* Если файл отсутствует, то загружаем поверхность из запасного источника. */
  if (!success && priv->fallback_source != NULL)
    {
      success = hyscan_gtk_map_tile_source_fill (priv->fallback_source, tile);
      if (success)
        hyscan_gtk_map_fs_tile_source_save (tile, tile_path);
    }

  g_free (tile_path);

  return success;
}

static void
hyscan_gtk_map_fs_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_gtk_map_fs_tile_source_fill_tile;
}

/**
 * hyscan_gtk_map_fs_tile_source_new:
 * @dir: каталог для хранения тайлов
 * @fb_source: запасной источник тайлов #HyScanGtkMapTileSource
 *
 * Создаёт новый файловый источник тайлов. Источник будет искать тайлы в каталоге
 * @dir; в случае, если файла с тайлом не окажется, тайл будет загружен из источника
 * @fb_source и сохранён в каталоге тайлов.
 *
 * Returns: новый объект #HyScanGtkMapFsTileSource. Для удаления g_object_unref()
 */
HyScanGtkMapFsTileSource *
hyscan_gtk_map_fs_tile_source_new (const gchar            *dir,
                                   HyScanGtkMapTileSource *fb_source)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE,
                       "dir", dir,
                       "fallback-source", fb_source,
                       NULL);
}
