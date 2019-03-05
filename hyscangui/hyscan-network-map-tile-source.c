/**
 * SECTION: hyscan-network-map-tile-source
 * @Short_description: Сетевой источник тайлов
 * @Title: HyScanNetworkMapTileSource
 *
 * Класс реализует интерфейс #HyScanGtkMapTileSource.
 *
 * Позволяет загружать тайлы из онлайн источником (тайловых сереров). Формат
 * URL тайлов необходимо указать при создании объекта с помошью функции
 * hyscan_network_map_tile_source_new().
 *
 * Поддерживается 2 формата URL-тайлов:
 *
 * 1. С плейсхолдерами {x}, {y}, {z}. Такой формат используется большинством тайловых серверов.
 *    Пример, https://a.tile.openstreetmap.org/{z}/{x}/{y}.png.
 *
 * 2. С плейсхолдером {quadkey}. Этот формат используется в Bing Maps (Microsoft).
 *    Пример, "http://ecn.t0.tiles.virtualearth.net/tiles/a{quadkey}.jpeg?g=6897"
 *
 */

#include "hyscan-network-map-tile-source.h"
#include <gio/gio.h>
#include <string.h>
#include <libsoup/soup.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

#define MAX_URL_LENGTH 2083                                               /* Максимальная длина URL. */
#define USER_AGENT     "Mozilla/5.0 (X11; Linux x86_64; rv:60.0) " \
                       "Gecko/20100101 Firefox/60.0"                      /* Заголовок "User-agent" для HTTP-запросов. */

enum
{
  PROP_O,
  PROP_URL_FORMAT,
  PROP_MIN_ZOOM,
  PROP_MAX_ZOOM,
};

typedef enum
{
  URL_FORMAT_INVALID,                          /* Неправильный формат. */
  URL_FORMAT_XYZ,                              /* Формат с плейсхолдерами {x}, {y}, {z}. */
  URL_FORMAT_QUAD,                             /* Формат с плейсхолдером {quadkey} (bing maps). */
} HyScanNetworkMapTileSourceType;

struct _HyScanNetworkMapTileSourcePrivate
{
  gchar                          *url_format;    /* Формат URL к изображению тайла. */
  HyScanNetworkMapTileSourceType  url_type;      /* Тип формата URL. */

  guint                           min_zoom;      /* Минимальный доступный зум (уровень детализации). */
  guint                           max_zoom;      /* Максимальный доступный зум (уровень детализации). */
  guint                           tile_size;     /* Размер тайла, пикселы. */

  SoupSession                    *session;       /* HTTP-клиент, который загружает изображения тайлов. */
};

static void           hyscan_network_map_tile_source_interface_init      (HyScanGtkMapTileSourceInterface   *iface);
static void           hyscan_network_map_tile_source_set_property        (GObject                           *object,
                                                                          guint                              prop_id,
                                                                          const GValue                      *value,
                                                                          GParamSpec                        *pspec);
static void           hyscan_network_map_tile_source_object_constructed  (GObject                           *object);
static void           hyscan_network_map_tile_source_object_finalize     (GObject                           *object);
static gboolean       hyscan_network_map_tile_source_fill_tile           (HyScanGtkMapTileSource            *source,
                                                                          HyScanGtkMapTile                  *tile,
                                                                          GCancellable                      *cancellable);
static void           hyscan_network_map_tile_source_get_zoom_limits     (HyScanGtkMapTileSource            *source,
                                                                          guint                             *min_zoom,
                                                                          guint                             *max_zoom);
static guint          hyscan_network_map_tile_source_get_tile_size       (HyScanGtkMapTileSource            *source);
static void           hyscan_network_map_tile_source_set_format          (HyScanNetworkMapTileSourcePrivate *priv,
                                                                          const gchar                       *url_tpl);
static gchar *        hyscan_network_map_tile_source_get_quad            (HyScanGtkMapTile                  *tile);
static gboolean       hyscan_network_map_tile_source_make_url            (HyScanNetworkMapTileSourcePrivate *priv,
                                                                          HyScanGtkMapTile                  *tile,
                                                                          gchar                             *url,
                                                                          gsize                              max_length);
static gchar *        hyscan_network_map_tile_source_replace             (const gchar                       *url_tpl,
                                                                          gchar                            **find,
                                                                          gchar                            **replace);

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
    g_param_spec_string ("url-format", "URL Format", "Tile URL format with placeholders", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MIN_ZOOM,
    g_param_spec_uint ("min-zoom", "Min zoom", "Minimal zoom", 0, G_MAXUINT, 0,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MAX_ZOOM,
    g_param_spec_uint ("max-zoom", "Max zoom", "Maximum zoom", 0, G_MAXUINT, 19,
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
      hyscan_network_map_tile_source_set_format (priv, g_value_get_string (value));
      break;

    case PROP_MIN_ZOOM:
      priv->min_zoom = g_value_get_uint (value);
      break;

    case PROP_MAX_ZOOM:
      priv->max_zoom = g_value_get_uint (value);
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

  priv->tile_size = 256;

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

/* Заменяет в строке url_tpl одни плейсхолдеры на другие.
 * Возвращает полученную строку, если все плейсхолдеры были найдены; иначе NULL. */
static gchar *
hyscan_network_map_tile_source_replace (const gchar  *url_tpl,
                                        gchar       **find,
                                        gchar       **replace)
{
  gchar *format;

  guint i;

  format = g_strdup (url_tpl);
  for (i = 0; find[i] != NULL && replace[i] != NULL; ++i)
    {
      gchar **tokens;

      tokens = g_strsplit (format, find[i], 2);
      g_clear_pointer (&format, g_free);

      if (tokens[1] != NULL)
        format = g_strconcat (tokens[0], replace[i], tokens[1], NULL);

      g_strfreev (tokens);

      if (format == NULL)
        break;
    }

  return format;
}

/* Устанавливает printf-формат URL на основе шаблона url_tpl. */
static void
hyscan_network_map_tile_source_set_format (HyScanNetworkMapTileSourcePrivate *priv,
                                           const gchar                       *url_tpl)
{
  g_free (priv->url_format);

  priv->url_format = hyscan_network_map_tile_source_replace (url_tpl,
                                                             (gchar *[]){"{x}",  "{y}",  "{z}",  NULL},
                                                             (gchar *[]){"%1$d", "%2$d", "%3$d", NULL});
  if (priv->url_format != NULL)
    {
      priv->url_type = URL_FORMAT_XYZ;
      return;
    }

  priv->url_format = hyscan_network_map_tile_source_replace (url_tpl,
                                                             (gchar *[]){"{quadkey}", NULL},
                                                             (gchar *[]){"%s", NULL});
  if (priv->url_format != NULL)
    {
      priv->url_type = URL_FORMAT_QUAD;
      return;
    }

  priv->url_type = URL_FORMAT_INVALID;
  g_warning ("HyScanNetworkMapTileSource: wrong URL template");
}

/* Гененрирует quadkey для bing maps. Для удаления g_free.
 * https://docs.microsoft.com/en-us/bingmaps/articles/bing-maps-tile-system#sample-code. */
static gchar *
hyscan_network_map_tile_source_get_quad (HyScanGtkMapTile *tile)
{
  guint x;
  guint y;
  guint zoom;

  guint i;

  gchar *quad_key;

  zoom = hyscan_gtk_map_tile_get_zoom (tile);
  x = hyscan_gtk_map_tile_get_x (tile);
  y = hyscan_gtk_map_tile_get_y (tile);

  quad_key = g_new (gchar, zoom + 1);
  for (i = zoom; i > 0; i--)
    {
      gchar digit = '0';
      guint mask = 1u << (i - 1);

      if ((x & mask) != 0)
        {
          digit++;
        }
      if ((y & mask) != 0)
        {
          digit++;
          digit++;
        }
      quad_key[zoom  - i] = digit;
    }
  quad_key[zoom] = '\0';

  return quad_key;
}

/* Формирует URL тайла и помещает его в буфер url длины max_length. */
static gboolean
hyscan_network_map_tile_source_make_url (HyScanNetworkMapTileSourcePrivate *priv,
                                         HyScanGtkMapTile                  *tile,
                                         gchar                             *url,
                                         gsize                              max_length)
{
  gchar *quad_key;

  switch (priv->url_type)
    {
    case URL_FORMAT_XYZ:
      g_snprintf (url, max_length,
                  priv->url_format,
                  hyscan_gtk_map_tile_get_x (tile),
                  hyscan_gtk_map_tile_get_y (tile),
                  hyscan_gtk_map_tile_get_zoom (tile));
      return TRUE;

    case URL_FORMAT_QUAD:
      quad_key = hyscan_network_map_tile_source_get_quad (tile);
      g_snprintf (url, max_length,
                  priv->url_format,
                  quad_key);
      g_free (quad_key);
      return TRUE;

    default:
      (max_length > 0) ? url[0] = '\0' : 0;
      return FALSE;
    }
}

/* Обработчик сигнала "cancelled" у @cancellable. */
static void
hyscan_network_map_tile_source_propagate_cancel (GCancellable *cancellable,
                                                 GCancellable *soup_cancellable)
{
  g_cancellable_cancel (soup_cancellable);
}

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_network_map_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                          HyScanGtkMapTile       *tile,
                                          GCancellable           *cancellable)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (source);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  gchar url[MAX_URL_LENGTH];
  GError *error = NULL;

  SoupMessage *soup_msg;
  GInputStream *input_stream;
  GdkPixbuf *pixbuf = NULL;

  gboolean status_ok = FALSE;

  if (g_cancellable_is_cancelled (cancellable))
    return FALSE;

  /* Отправляем HTTP-запрос на получение тайла. */
  if (!hyscan_network_map_tile_source_make_url (priv, tile, url, sizeof (url)))
    goto exit;

  /* Делаем HTTP-запрос с отдельным GCancellable,
   * потому что soup_session_send() может сделать g_cancellable_reset(). */
  {
    GCancellable *soup_cancellable;
    gulong id;

    soup_cancellable = g_cancellable_new ();

    id = g_cancellable_connect (cancellable, G_CALLBACK (hyscan_network_map_tile_source_propagate_cancel),
                                soup_cancellable, NULL);

    soup_msg = soup_message_new ("GET", url);
    input_stream = soup_session_send (priv->session, soup_msg, soup_cancellable, &error);

    g_cancellable_disconnect (cancellable, id);
    g_object_unref (soup_cancellable);

    if (error != NULL)
      goto error;
  }

  /* Из тела ответа формируем изображение pixbuf. */
  pixbuf = gdk_pixbuf_new_from_stream (input_stream, cancellable, &error);
  if (error != NULL)
    goto error;

  /* Устанавливаем загруженную поверхность в тайл. */
  status_ok = hyscan_gtk_map_tile_set_pixbuf (tile, pixbuf);

error:
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("HyScanNetworkMapTileSource: failed to read \"%s\" (%s)", url, error->message);

      g_clear_error (&error);
    }

exit:
  g_clear_object (&pixbuf);
  g_clear_object (&soup_msg);
  g_clear_object (&input_stream);

  return status_ok;
}

/* Реализация функции get_zoom_limits интерфейса HyScanGtkMapTileSource.*/
static void
hyscan_network_map_tile_source_get_zoom_limits (HyScanGtkMapTileSource *source,
                                               guint                   *min_zoom,
                                               guint                   *max_zoom)
{
  HyScanNetworkMapTileSourcePrivate *priv;

  priv = HYSCAN_NETWORK_MAP_TILE_SOURCE (source)->priv;
  (max_zoom != NULL) ? *max_zoom = priv->max_zoom : 0;
  (min_zoom != NULL) ? *min_zoom = priv->min_zoom : 0;
}

/* Реализация функции get_tile_size интерфейса HyScanGtkMapTileSource.*/
static guint
hyscan_network_map_tile_source_get_tile_size (HyScanGtkMapTileSource *source)
{
  HyScanNetworkMapTileSourcePrivate *priv;

  priv = HYSCAN_NETWORK_MAP_TILE_SOURCE (source)->priv;

  return priv->tile_size;
}

/* Реализация интерфейса HyScanGtkMapTileSource. */
static void
hyscan_network_map_tile_source_interface_init (HyScanGtkMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_network_map_tile_source_fill_tile;
  iface->get_zoom_limits = hyscan_network_map_tile_source_get_zoom_limits;
  iface->get_tile_size = hyscan_network_map_tile_source_get_tile_size;
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
hyscan_network_map_tile_source_new (const gchar *url_format,
                                    guint        min_zoom,
                                    guint        max_zoom)
{
  return g_object_new (HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE,
                       "url-format", url_format,
                       "min-zoom", min_zoom,
                       "max-zoom", max_zoom,
                       NULL);
}
