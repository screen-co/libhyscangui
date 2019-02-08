/**
 * SECTION: hyscan-network-map-tile-source
 * @Short_description: Онлайн источник тайлов
 * @Title: HyScanNetworkMapTileSource
 *
 * Класс реализует интерфейс #HyScanGtkMapTileSource.
 *
 * Позволяет загружать тайлы из онлайн источником (тайловых сереров). Формат
 * URL тайлов необходимо указать при создании объекта с помошью функции
 * hyscan_network_map_tile_source_new().
 *
 * Например,
 *
 * |[<!-- language="C" -->
 *   // Формат для URL https://a.tile.openstreetmap.org/{z}/{x}/{y}.png
 *   const gchar *format = "https://a.tile.openstreetmap.org/%d/%d/%d.png";
 *   
 *   hyscan_network_map_tile_source_new (format);
 * ]|
 *
 */

#include "hyscan-network-map-tile-source.h"
#include <gio/gio.h>
#include <string.h>
#include <libsoup/soup.h>

enum
{
  PROP_O,
  PROP_URL_FORMAT
};

struct _HyScanNetworkMapTileSourcePrivate
{
  gchar       *url_format;    /* Формат URL изображения тайла, printf (url_format, z, x, y). */

  SoupSession *session;       /* HTTP-клиент, который загружает изображения тайлов. */
};

static void           hyscan_network_map_tile_source_interface_init      (HyScanGtkMapTileSourceInterface *iface);
static void           hyscan_network_map_tile_source_set_property        (GObject               *object,
                                                                          guint                  prop_id,
                                                                          const GValue          *value,
                                                                          GParamSpec            *pspec);
static void           hyscan_network_map_tile_source_object_constructed  (GObject               *object);
static void           hyscan_network_map_tile_source_object_finalize     (GObject               *object);
static cairo_status_t hyscan_network_map_tile_source_download            (GInputStream          *input_stream,
                                                                          unsigned char         *data,
                                                                          unsigned int           length);

G_DEFINE_TYPE_WITH_CODE (HyScanNetworkMapTileSource, hyscan_network_map_tile_source, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNetworkMapTileSource)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                                                hyscan_network_map_tile_source_interface_init))

static void
hyscan_network_map_tile_source_class_init (HyScanNetworkMapTileSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_network_map_tile_source_set_property;

  object_class->constructed = hyscan_network_map_tile_source_object_constructed;
  object_class->finalize = hyscan_network_map_tile_source_object_finalize;

  g_object_class_install_property (object_class, PROP_URL_FORMAT,
    g_param_spec_string ("url-format", "URL Format", "Tile URL format suitable for printf (format, z, x, y)", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_network_map_tile_source_init (HyScanNetworkMapTileSource *nw_source)
{
  nw_source->priv = hyscan_network_map_tile_source_get_instance_private (nw_source);
}

static void
hyscan_network_map_tile_source_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (object);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  switch (prop_id)
    {
    case PROP_URL_FORMAT:
      priv->url_format = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_network_map_tile_source_object_constructed (GObject *object)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (object);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;
  SoupLoggerLogLevel debug_level = SOUP_LOGGER_LOG_NONE;
  // SoupLoggerLogLevel debug_level = SOUP_LOGGER_LOG_MINIMAL;

  G_OBJECT_CLASS (hyscan_network_map_tile_source_parent_class)->constructed (object);

  /* Инициализируем HTTP-клиент SoupSession. */
  priv->session = soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER, NULL);
  if (debug_level != SOUP_LOGGER_LOG_NONE) {
    SoupLogger *logger;

    logger = soup_logger_new (debug_level, -1);
    soup_session_add_feature (priv->session, SOUP_SESSION_FEATURE (logger));
    g_object_unref (logger);
  }
}

static void
hyscan_network_map_tile_source_object_finalize (GObject *object)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (object);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  g_free (priv->url_format);
  g_object_unref (priv->session);

  G_OBJECT_CLASS (hyscan_network_map_tile_source_parent_class)->finalize (object);
}

/* Загружает png-поверхность cairo из указанного потока @input_stream. */
static cairo_status_t
hyscan_network_map_tile_source_download (GInputStream  *input_stream,
                                         unsigned char *data,
                                         unsigned int   length)
{
  gsize bytes_read;
  static gssize total_bytes_read = 0;
  GError *error = NULL;

  g_input_stream_read_all (input_stream, data, length, &bytes_read, NULL, &error);
  total_bytes_read += bytes_read;

  g_clear_error (&error);

  return bytes_read > 0 ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
}

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_network_map_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                          HyScanGtkMapTile       *tile)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (source);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  gchar *url = NULL;
  GError *error = NULL;

  SoupMessage *msg;
  GInputStream *input_stream;

  gboolean status_ok = FALSE;

  url = g_strdup_printf (priv->url_format,
                         hyscan_gtk_map_tile_get_zoom (tile),
                         hyscan_gtk_map_tile_get_x (tile),
                         hyscan_gtk_map_tile_get_y (tile));

  /* Отправляем HTTP-запрос на получение тайла. */
  msg = soup_message_new ("GET", url);
  input_stream = soup_session_send (priv->session, msg, NULL, &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  /* Чтобы часто не отправлять запросы на сервер, добавляем задержку между запросами. */
  //  g_usleep (G_USEC_PER_SEC / 4);

  /* Получаем изображение тайла из тела ответа. */
  if (input_stream != NULL)
    {
      cairo_surface_t *png_surface;

      /* Из тела ответа формируем поверхность cairo. */
      png_surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t) hyscan_network_map_tile_source_download,
                                                                 input_stream);

      /* Устанавливаем загруженную поверхность в тайл. */
      status_ok = hyscan_gtk_map_tile_set_content (tile, png_surface);
      cairo_surface_destroy (png_surface);
    }

  g_clear_object (&msg);
  g_clear_object (&input_stream);
  g_free (url);

  return status_ok;
}

/* Реализация интрефейса HyScanGtkMapTileSource. */
static void
hyscan_network_map_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_network_map_tile_source_fill_tile;
}

/**
 * hyscan_network_map_tile_source_new:
 * @url_format: формат URL тайла с парамтерами (zoom, x, y)
 *
 * Создаёт новый источник тайлов из сервера тайлов.
 *
 * Returns: новый объект #HyScanNetworkMapTileSource. Для удаления g_object_unref()
 */
HyScanNetworkMapTileSource *
hyscan_network_map_tile_source_new (const gchar *url_format)
{
  return g_object_new (HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE,
                       "url-format", url_format, NULL);
}