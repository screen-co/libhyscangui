#include "hyscan-gtk-map-tiles-osm.h"
#include <hyscan-gtk-map-tile-source.h>
#include <gio/gio.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_HOST,
  PROP_URI_FORMAT
};

struct _HyScanGtkMapTilesOsmPrivate
{
  gchar *host;
  gchar *uri_format;
};

static void           hyscan_gtk_map_tiles_osm_interface_init           (HyScanGtkMapTileSourceInterface *iface);
static void           hyscan_gtk_map_tiles_osm_set_property             (GObject               *object,
                                                                         guint                  prop_id,
                                                                         const GValue          *value,
                                                                         GParamSpec            *pspec);
static void           hyscan_gtk_map_tiles_osm_object_constructed       (GObject               *object);
static void           hyscan_gtk_map_tiles_osm_object_finalize          (GObject               *object);
static cairo_status_t hyscan_gtk_map_tiles_osm_download                 (GDataInputStream      *input_stream,
                                                                         unsigned char         *data,
                                                                         unsigned int           length);

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

  g_object_class_install_property (object_class, PROP_HOST,
    g_param_spec_string ("host", "Host", "Server host name, e.g. \"example.com\"", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_URI_FORMAT,
    g_param_spec_string ("uri-format", "Uri Format", "Format of tile uri for integer values z, x, y", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
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
    case PROP_HOST:
      priv->host = g_value_dup_string (value);
      break;

    case PROP_URI_FORMAT:
      priv->uri_format = g_value_dup_string (value);
      break;

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
}

static void
hyscan_gtk_map_tiles_osm_object_finalize (GObject *object)
{
  HyScanGtkMapTilesOsm *gtk_map_tiles_osm = HYSCAN_GTK_MAP_TILES_OSM (object);
  HyScanGtkMapTilesOsmPrivate *priv = gtk_map_tiles_osm->priv;

  g_free (priv->uri_format);
  g_free (priv->host);

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_osm_parent_class)->finalize (object);
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
hyscan_gtk_map_tiles_osm_fill_tile (HyScanGtkMapTileSource *source,
                                    HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTilesOsm *osm = HYSCAN_GTK_MAP_TILES_OSM (source);
  HyScanGtkMapTilesOsmPrivate *priv = osm->priv;

  gchar *uri = NULL;
  gchar *request = NULL;
  GError *error = NULL;

  GSocketClient *client;
  GSocketConnection *connection;
  GInputStream *input_stream;
  GOutputStream *output_stream;

  GDataInputStream *data_stream = NULL;

  gboolean status_ok = FALSE;

  /* Чтобы часто не отправлять запросы на сервер, добавляем задержку между запросами. */
  //  g_usleep (G_USEC_PER_SEC / 4);

  /* Подключаемся к серверу с тайлами. */
  client = g_socket_client_new ();
  connection = g_socket_client_connect_to_host (client, priv->host, 80, NULL, &error);

  if (error != NULL)
    {
      g_warning ("HyScanGtkMapTilesOsm: %s", error->message);
      g_clear_error (&error);
      goto exit;
    }

  input_stream = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  /* Отправляем HTTP-запрос на получение тайла. */
  uri = g_strdup_printf (priv->uri_format,
                         hyscan_gtk_map_tile_get_zoom (tile),
                         hyscan_gtk_map_tile_get_x (tile),
                         hyscan_gtk_map_tile_get_y (tile));

  request = g_strdup_printf ("GET %s HTTP/1.1\r\n"
                             "Host: %s\r\n"
                             "\r\n",
                             uri, priv->host);

  g_message ("Downloading http://%s%s", priv->host, uri);
  g_output_stream_write (output_stream, request, strlen (request), NULL, &error);

  /* Читаем ответ сервера. */
  data_stream = g_data_input_stream_new (G_INPUT_STREAM (input_stream));
  {
    gchar *header_line;

    /* Если статус ответа 200, то всё хорошо, можно выполнять загрузку. */
    header_line = g_data_input_stream_read_line (data_stream, NULL, NULL, NULL);
    status_ok = g_strcmp0 (header_line, "HTTP/1.1 200 OK\r") == 0;
    g_free (header_line);

    /* Проматываем остальные заголовки в ответе, они нас не интересуют. */
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

  /* Получаем изображение тайла. */
  if (status_ok)
    {
      cairo_surface_t *png_surface;

      /* Из тела ответа формируем поверхность cairo. */
      png_surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t) hyscan_gtk_map_tiles_osm_download,
                                                                 data_stream);

      /* Устанавливаем загруженную поверхность в тайл. */
      status_ok = hyscan_gtk_map_tile_set_content (tile, png_surface);
      cairo_surface_destroy (png_surface);
    }

exit:
  g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
  g_clear_object (&connection);
  g_clear_object (&data_stream);
  g_clear_object (&client);
  g_free (request);
  g_free (uri);

  return status_ok;
}

/* Реализация интрефейса HyScanGtkMapTileSource. */
static void
hyscan_gtk_map_tiles_osm_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_gtk_map_tiles_osm_fill_tile;
}

/**
 * hyscan_gtk_map_tiles_osm_new:
 *
 * Создаёт новый источник тайлов из сервера тайлов OpenStreetMap.
 *
 * Returns: новый объект #HyScanGtkMapTilesOsm.
 */
HyScanGtkMapTilesOsm *
hyscan_gtk_map_tiles_osm_new (const gchar *host,
                              const gchar *uri_format)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILES_OSM,
                       "host", host,
                       "uri-format", uri_format, NULL);
}