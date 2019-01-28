#include "hyscan-gtk-map-tiles-osm.h"
#include <hyscan-gtk-map-tile-source.h>
#include <gio/gio.h>
#include <string.h>

#define HYSCAN_GTK_MAP_TILES_OSM_BUFFER_SIZE 4096

enum
{
  PROP_O,
};

enum
{
  SIGNAL_TILE_FOUND,   /* Сигнал о том, что запрошенный тайл найден. */
  SIGNAL_LAST
};

struct _HyScanGtkMapTilesOsmPrivate
{
  cairo_surface_t *tile_surface;
};

static void           hyscan_gtk_map_tiles_osm_interface_init           (HyScanGtkMapTileSourceInterface *iface);
static void           hyscan_gtk_map_tiles_osm_set_property             (GObject               *object,
                                                                         guint                  prop_id,
                                                                         const GValue          *value,
                                                                         GParamSpec            *pspec);
static void           hyscan_gtk_map_tiles_osm_object_constructed       (GObject               *object);
static void           hyscan_gtk_map_tiles_osm_object_finalize          (GObject               *object);
static gboolean       hyscan_gtk_map_tiles_osm_process_task             (HyScanGtkMapTile      *tile,
                                                                         HyScanGtkMapTilesOsm  *osm);
static cairo_status_t hyscan_gtk_map_tiles_osm_download                 (GDataInputStream *input_stream,
                                                                         unsigned char    *data,
                                                                         unsigned int      length);

static guint   hyscan_gtk_map_tiles_osm_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTilesOsm, hyscan_gtk_map_tiles_osm, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTilesOsm)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                                                hyscan_gtk_map_tiles_osm_interface_init))

static void
hyscan_gtk_map_tiles_osm_class_init (HyScanGtkMapTilesOsmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiles_osm_set_property;

  object_class->constructed = hyscan_gtk_map_tiles_osm_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiles_osm_object_finalize;

  /**
   * ::tile-found
   *
   * todo: сделать сигналы "tile-found" в родительском классе.
   */
  hyscan_gtk_map_tiles_osm_signals[SIGNAL_TILE_FOUND] = g_signal_new ("tile-found", HYSCAN_TYPE_GTK_MAP_TILES_OSM,
                                                                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                                                      g_cclosure_marshal_VOID__POINTER,
                                                                      G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
hyscan_gtk_map_tiles_osm_init (HyScanGtkMapTilesOsm *gtk_map_tiles_osm)
{
  gtk_map_tiles_osm->priv = hyscan_gtk_map_tiles_osm_get_instance_private (gtk_map_tiles_osm);
}

static void
hyscan_gtk_map_tiles_osm_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkMapTilesOsm *gtk_map_tiles_osm = HYSCAN_GTK_MAP_TILES_OSM (object);
  HyScanGtkMapTilesOsmPrivate *priv = gtk_map_tiles_osm->priv;

  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tiles_osm_object_constructed (GObject *object)
{
  HyScanGtkMapTilesOsm *osm = HYSCAN_GTK_MAP_TILES_OSM (object);
  HyScanGtkMapTilesOsmPrivate *priv = osm->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_osm_parent_class)->constructed (object);

  /* Инициализируем тайл-заглушку. */
  priv->tile_surface = cairo_image_surface_create_from_png ("/home/alex/Downloads/1.png");
}

static void
hyscan_gtk_map_tiles_osm_object_finalize (GObject *object)
{
  HyScanGtkMapTilesOsm *gtk_map_tiles_osm = HYSCAN_GTK_MAP_TILES_OSM (object);
  HyScanGtkMapTilesOsmPrivate *priv = gtk_map_tiles_osm->priv;

  /* Освобождаем память. */
  cairo_surface_destroy (priv->tile_surface);

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_osm_parent_class)->finalize (object);
}

/* Обработка задачи по загрузке тайла с сервера. */
static gboolean
hyscan_gtk_map_tiles_osm_process_task (HyScanGtkMapTile     *tile,
                                       HyScanGtkMapTilesOsm *osm)
{
  gchar *uri;
  gchar *request;
  GError *error = NULL;

  GSocketClient *client;
  GSocketConnection *connection;
  GInputStream *input_stream;
  GOutputStream *output_stream;

  GDataInputStream *data_stream;

  /* Хост и шаблон uri тайлов. */
//  const gchar *host = "a.tile.openstreetmap.org";
//  const gchar *uri_tpl = "/%d/%d/%d.png";

  /* thunderforest
   * https://manage.thunderforest.com/dashboard */
  const gchar *host = "tile.thunderforest.com";
  const gchar *uri_tpl = "/cycle/%d/%d/%d.png?apikey=03fb8295553d4a2eaacc64d7dd88e3b9";

  /* Wikimedia - todo: TLS redirect. */
//  const gchar *host = "maps.wikimedia.org";
//  const gchar *uri_tpl = "/osm-intl/%d/%d/%d.png";

  gboolean status_ok;

  /* Чтобы часто не отправлять запросы на сервер, добавляем задержку между запросами. */
  g_usleep (G_USEC_PER_SEC / 4);

  /* Формируем URL тайла. */
  uri = g_strdup_printf (uri_tpl, tile->zoom, tile->x, tile->y);

  g_message ("Downloading http://%s%s", host, uri);

  /* Подключаемся к серверу с тайлами. */
  client = g_socket_client_new ();
  connection = g_socket_client_connect_to_host (client, host, 80, NULL, &error);

  if (error)
    {
      g_warning ("HyScanGtkMapTilesOsm: %s", error->message);
      g_clear_error (&error);
    }

  input_stream = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  /* Отправляем HTTP-запрос на получение тайла. */
  request = g_strdup_printf ("GET %s HTTP/1.1\r\n"
                             "Host: %s\r\n\r\n",
                             uri, host);

  g_output_stream_write (output_stream, request, strlen (request), NULL, &error);

  /* Читаем ответ сервера. */
  data_stream = g_data_input_stream_new (G_INPUT_STREAM (input_stream));
  {
    gchar *header_line;

    header_line = g_data_input_stream_read_line (data_stream, NULL, NULL, NULL);
    g_message ("Status: %s", header_line);
    status_ok = g_strcmp0 (header_line, "HTTP/1.1 200 OK\r") == 0;
    g_free (header_line);

    /* Проматываем заголовки в ответе. */
    while ((header_line = g_data_input_stream_read_line (data_stream, NULL, NULL, NULL)))
      {
        gboolean headers_end;

        /* Заголовки отделяются от тела ответа пустой строкой "\r\n". Как
         * только нашли такую строку - прекращаем листать заголовки. */
        headers_end = g_str_equal (header_line, "\r");
        g_free (header_line);

        if (headers_end)
          break;
      }
  }

  /* Загружаем файл тайла в поверхность cairo. */
  if (status_ok)
    {
      cairo_surface_t *png_surface;
      gssize dsize;

      png_surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t) hyscan_gtk_map_tiles_osm_download,
                                                                 data_stream);

      dsize = cairo_image_surface_get_height (png_surface) * cairo_image_surface_get_stride (png_surface);

      if (cairo_image_surface_get_format (tile->surface) != cairo_image_surface_get_format (png_surface))
        {
          g_warning ("Wrong image format");
        }

      memcpy (cairo_image_surface_get_data (tile->surface),
              cairo_image_surface_get_data (png_surface),
              dsize);

      g_message ("Loaded surface %dx%d",
                 cairo_image_surface_get_width (tile->surface),
                 cairo_image_surface_get_height (tile->surface));

      cairo_surface_destroy (png_surface);
    }

  g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
  g_object_unref (connection);

  g_object_unref (data_stream);
  g_free (request);
  g_free (uri);
  g_object_unref (client);

  return status_ok;
}

/* Загружает png-поверхность cairo из указанного потока @input_stream. */
static cairo_status_t
hyscan_gtk_map_tiles_osm_download (GDataInputStream *input_stream,
                                   unsigned char    *data,
                                   unsigned int      length)
{
  gsize bytes_read;
  static gssize total_bytes_read = 0;
  GError *error = NULL;

  g_input_stream_read_all (G_INPUT_STREAM (input_stream), data, length, &bytes_read, NULL, &error);
  total_bytes_read += bytes_read;

  g_clear_error (&error);

  return bytes_read > 0 ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
}

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_gtk_map_tiles_osm_find (HyScanGtkMapTileSource *tsource,
                               HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTilesOsm *osm = HYSCAN_GTK_MAP_TILES_OSM (tsource);

  return hyscan_gtk_map_tiles_osm_process_task (tile, osm);
}

/* Реализация интрефейса HyScanGtkMapTileSource. */
static void
hyscan_gtk_map_tiles_osm_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->find_tile = hyscan_gtk_map_tiles_osm_find;
}

/**
 * hyscan_gtk_map_tiles_osm_new:
 *
 * Создаёт новый источник тайлов из OpenStreetMap
 *
 * Returns: новый объект #HyScanGtkMapTilesOsm.
 */
HyScanGtkMapTilesOsm *
hyscan_gtk_map_tiles_osm_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILES_OSM, NULL);
}