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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

#define USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:60.0) " \
                   "Gecko/20100101 Firefox/60.0"                      /* Заголовок "User-agent" для HTTP-запросов. */

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
static void           hyscan_network_map_tile_source_set_property        (GObject                         *object,
                                                                          guint                            prop_id,
                                                                          const GValue                    *value,
                                                                          GParamSpec                      *pspec);
static void           hyscan_network_map_tile_source_object_constructed  (GObject                         *object);
static void           hyscan_network_map_tile_source_object_finalize     (GObject                         *object);
static gboolean       hyscan_network_map_tile_source_fill_tile           (HyScanGtkMapTileSource          *source,
                                                                          HyScanGtkMapTile                *tile);

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

  /* Инициализируем HTTP-клиент SoupSession.
   * Устанавливаем заголовок "user-agent", иначе некоторые серверы блокируют запросы. */
  priv->session = soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
                                                 SOUP_SESSION_USER_AGENT, USER_AGENT,
                                                 NULL);
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

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_network_map_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                          HyScanGtkMapTile       *tile)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (source);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  gchar *url = NULL;
  GError *error = NULL;

  SoupMessage *soup_msg;
  GInputStream *input_stream;
  GdkPixbuf *pixbuf = NULL;

  gboolean status_ok = FALSE;

  url = g_strdup_printf (priv->url_format,
                         hyscan_gtk_map_tile_get_zoom (tile),
                         hyscan_gtk_map_tile_get_x (tile),
                         hyscan_gtk_map_tile_get_y (tile));

  /* Отправляем HTTP-запрос на получение тайла. */
  soup_msg = soup_message_new ("GET", url);
  input_stream = soup_session_send (priv->session, soup_msg, NULL, &error);
  if (error != NULL)
    {
      g_warning ("HyScanNetworkMapTileSource: failed to get \"%s\" (%s)", url, error->message);
      g_clear_error (&error);

      goto exit;
    }

  /* Из тела ответа формируем изображение pixbuf. */
  pixbuf = gdk_pixbuf_new_from_stream (input_stream, NULL, &error);
  if (error != NULL)
    {
      g_warning ("HyScanNetworkMapTileSource: failed to load image %s", error->message);
      g_clear_error (&error);

      goto exit;
    }

  /* Устанавливаем загруженную поверхность в тайл. */
  status_ok = hyscan_gtk_map_tile_set_pixbuf (tile, pixbuf);

exit:
  g_clear_object (&pixbuf);
  g_clear_object (&soup_msg);
  g_clear_object (&input_stream);
  g_free (url);

  return status_ok;
}

/* Реализация интерфейса HyScanGtkMapTileSource. */
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
