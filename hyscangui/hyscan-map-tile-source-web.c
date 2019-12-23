/* hyscan-map-tile-source.c
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
 * SECTION: hyscan-map-tile-source-web
 * @Short_description: Сетевой источник тайлов
 * @Title: HyScanMapTileSourceWeb
 *
 * Класс реализует интерфейс #HyScanMapTileSource.
 *
 * Позволяет загружать тайлы из онлайн источником (тайловых сереров). Формат
 * URL тайлов необходимо указать при создании объекта с помошью функции
 * hyscan_map_tile_source_web_new().
 *
 * Поддерживается 2 формата URL-тайлов:
 *
 * 1. С плейсхолдерами {x}, {y}, {-y}, {z}. Такой формат используется большинством
 *    тайловых серверов. Для координаты по Y следует выбирать плейсхолдер:
 *    - {y}, если отсчет тайлов идёт сверху вниз (OpenStreetMap),
 *    - {-y}, если отсчет тайлов идёт снизу вверх (TMS-совместимые сетки).
 *
 *    Пример, https://a.tile.openstreetmap.org/{z}/{x}/{y}.png.
 *
 * 2. С плейсхолдером {quadkey}. Этот формат используется в Bing Maps (Microsoft).
 *    Пример, "http://ecn.t0.tiles.virtualearth.net/tiles/a{quadkey}.jpeg?g=6897"
 *
 * Существует возможность передавать в запросе к серверу дополнительные HTTP-заголовоки.
 * Указать заголовки запроса можно при помощи функции hyscan_map_tile_source_add_header().
 *
 */

#include "hyscan-map-tile-source-web.h"
#include <gio/gio.h>
#include <string.h>
#include <libsoup/soup.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define USER_AGENT     "Mozilla/5.0 (X11; Linux x86_64; rv:60.0) " \
                       "Gecko/20100101 Firefox/60.0"                      /* Заголовок "User-agent" для HTTP-запросов. */

/* Вспомогательный класс HyScanMapTileSourceWebTask - задача по загрузке тайла. */
#define HYSCAN_TYPE_MAP_TILE_SOURCE_WEB_TASK        (hyscan_map_tile_source_web_task_get_type ())
#define HYSCAN_MAP_TILE_SOURCE_WEB_TASK(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE_WEB_TASK, HyScanMapTileSourceWebTask))

/* Статусы задачи по загрузки тайла. */
enum {
  STATUS_IDLE,
  STATUS_PROCESSING,
  STATUS_OK,
  STATUS_FAILED,
  STATUS_CANCELLED,
};

typedef struct
{
  GObject parent_instance;

  guint                     status;            /* Статус задачи. */
  GCond                     cond;              /* Сигнализатор изменения статуса. */
  GMutex                    mutex;             /* Мутекс статуса. */
  HyScanMapTile            *tile;              /* Тайл, который надо заполнить. */
  gchar                    *url;               /* URL тайла. */
  GCancellable             *cancellable;       /* Объект для отмена задачи. */
  GCancellable             *soup_cancellable;  /* Объект для отмены, передаваемый в libsoup. */
  gulong                    handler_id;        /* Хэндлер обработчика сигнала от cancellable. */
  SoupSession              *soup_session;      /* HTTP-клиент, который загружает изображения тайлов. */
  SoupMessage              *soup_msg;          /* Запрос на сервер. */
} HyScanMapTileSourceWebTask;

typedef struct
{
  GObjectClass parent_class;
} HyScanMapTileSourceWebTaskClass;

enum
{
  PROP_O,
  PROP_PROJECTION,
  PROP_URL_FORMAT,
  PROP_MIN_ZOOM,
  PROP_MAX_ZOOM,
};

typedef enum
{
  URL_FORMAT_INVALID,                          /* Неправильный формат. */
  URL_FORMAT_XYZ,                              /* Формат с плейсхолдерами {x}, {y} / {-y}, {z}. */
  URL_FORMAT_QUAD,                             /* Формат с плейсхолдером {quadkey} (bing maps). */
} HyScanMapTileSourceWebType;

struct _HyScanMapTileSourceWebPrivate
{
  GList                          *headers;       /* Список заголовков запроса. */
  gchar                          *url_format;    /* Формат URL к изображению тайла. */
  HyScanMapTileSourceWebType      url_type;      /* Тип формата URL. */
  gboolean                        inverse_y;     /* Признак того, что координаты по y должны быть инвертированны. */
  guint                           hash;          /* Хэш источника тайлов. */

  guint                           min_zoom;      /* Минимальный доступный зум (уровень детализации). */
  guint                           max_zoom;      /* Максимальный доступный зум (уровень детализации). */
  guint                           tile_size;     /* Размер тайла, пикселы. */
  HyScanMapTileGrid              *grid;          /* Параметры тайловой сетки. */
  HyScanGeoProjection            *projection;    /* Картографическая проекция источника тайлов. */

  GThreadPool                    *thread_pool;   /* Пул потоков, обрабатывающих задачи по загрузке тайлов. */
};

static void           hyscan_map_tile_source_web_interface_init      (HyScanMapTileSourceInterface   *iface);
static void           hyscan_map_tile_source_web_set_property        (GObject                        *object,
                                                                      guint                           prop_id,
                                                                      const GValue                   *value,
                                                                      GParamSpec                     *pspec);
static void           hyscan_map_tile_source_web_object_constructed  (GObject                        *object);
static void           hyscan_map_tile_source_web_object_finalize     (GObject                        *object);
static gboolean       hyscan_map_tile_source_web_fill_tile           (HyScanMapTileSource            *source,
                                                                      HyScanMapTile                  *tile,
                                                                      GCancellable                   *cancellable);
static void           hyscan_map_tile_source_web_set_format          (HyScanMapTileSourceWebPrivate  *priv,
                                                                      const gchar                    *url_tpl);
static gchar *        hyscan_map_tile_source_web_get_quad            (HyScanMapTile                  *tile);
static HyScanMapTileSourceWebTask *
                      hyscan_map_tile_source_web_task_new            (HyScanMapTileSourceWebPrivate  *priv,
                                                                      HyScanMapTile                  *tile,
                                                                      GCancellable                   *cancellable);
static gchar *        hyscan_map_tile_source_web_replace             (const gchar                    *url_tpl,
                                                                      gchar                         **find_replace);

G_DEFINE_TYPE_WITH_CODE (HyScanMapTileSourceWeb, hyscan_map_tile_source_web, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMapTileSourceWeb)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_MAP_TILE_SOURCE,
                                                hyscan_map_tile_source_web_interface_init))
G_DEFINE_TYPE (HyScanMapTileSourceWebTask, hyscan_map_tile_source_web_task, G_TYPE_OBJECT)

static void
hyscan_map_tile_source_web_task_init (HyScanMapTileSourceWebTask *task)
{
  task->handler_id = 0;
  task->status = STATUS_IDLE;
  g_mutex_init (&task->mutex);
  g_cond_init (&task->cond);
}

static void
hyscan_map_tile_source_web_task_finalize (GObject *object)
{
  HyScanMapTileSourceWebTask *task = HYSCAN_MAP_TILE_SOURCE_WEB_TASK (object);
  g_free (task->url);

  g_cancellable_disconnect (task->cancellable, task->handler_id);

  g_mutex_lock (&task->mutex);
  g_clear_object (&task->soup_msg);
  g_clear_object (&task->soup_session);
  g_clear_object (&task->cancellable);
  g_clear_object (&task->soup_cancellable);
  g_clear_object (&task->tile);
  g_mutex_unlock (&task->mutex);

  g_mutex_clear (&task->mutex);
  g_cond_clear (&task->cond);
}

static void
hyscan_map_tile_source_web_task_class_init (HyScanMapTileSourceWebTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = hyscan_map_tile_source_web_task_finalize;
}

static void
hyscan_map_tile_source_web_class_init (HyScanMapTileSourceWebClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_tile_source_web_set_property;

  object_class->constructed = hyscan_map_tile_source_web_object_constructed;
  object_class->finalize = hyscan_map_tile_source_web_object_finalize;

  g_object_class_install_property (object_class, PROP_URL_FORMAT,
    g_param_spec_string ("url-format", "URL Format", "Tile URL format with placeholders", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECTION,
    g_param_spec_object ("projection", "HyScanGeoProjection", "GeoProjection of tile source",
                         HYSCAN_TYPE_GEO_PROJECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MIN_ZOOM,
    g_param_spec_uint ("min-zoom", "Min zoom", "Minimal zoom", 0, G_MAXUINT, 0,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MAX_ZOOM,
    g_param_spec_uint ("max-zoom", "Max zoom", "Maximum zoom", 0, G_MAXUINT, 19,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_map_tile_source_web_init (HyScanMapTileSourceWeb *nw_source)
{
  nw_source->priv = hyscan_map_tile_source_web_get_instance_private (nw_source);
}

static void
hyscan_map_tile_source_web_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanMapTileSourceWeb *nw_source = HYSCAN_MAP_TILE_SOURCE_WEB (object);
  HyScanMapTileSourceWebPrivate *priv = nw_source->priv;

  switch (prop_id)
    {
    case PROP_PROJECTION:
      priv->projection = g_value_dup_object (value);
      break;

    case PROP_URL_FORMAT:
      hyscan_map_tile_source_web_set_format (priv, g_value_get_string (value));
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

/* Выполняет загрузку изображения и установку его в тайл. */
static void
hyscan_map_tile_source_web_task_do (gpointer data,
                                    gpointer user_data)
{
  HyScanMapTileSourceWebTask *task = data;

  GInputStream *input_stream = NULL;
  GdkPixbuf *pixbuf = NULL;

  GError *error = NULL;

  input_stream = soup_session_send (task->soup_session, task->soup_msg, task->soup_cancellable, &error);

  g_mutex_lock (&task->mutex);
  if (task->status != STATUS_PROCESSING)
    goto exit;

  /* Получаем ответ сервера. */
  if (error != NULL)
    {
      task->status = STATUS_FAILED;
      goto error;
    }

  /* Из тела ответа формируем изображение pixbuf. */
  pixbuf = gdk_pixbuf_new_from_stream (input_stream, task->cancellable, &error);
  if (error != NULL)
    {
      task->status = STATUS_FAILED;
      goto error;
    }

  /* Устанавливаем загруженную поверхность в тайл. */
  if (hyscan_map_tile_set_pixbuf (task->tile, pixbuf))
    task->status = STATUS_OK;
  else
    task->status = STATUS_FAILED;

error:
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("HyScanMapTileSourceWeb: failed to read \"%s\" (%s)", task->url, error->message);

      g_clear_error (&error);
    }

exit:
  g_clear_object (&input_stream);
  g_clear_object (&pixbuf);
  g_cond_signal (&task->cond);
  g_mutex_unlock (&task->mutex);
  g_object_unref (task);
}

static void
hyscan_map_tile_source_web_object_constructed (GObject *object)
{
  HyScanMapTileSourceWeb *nw_source = HYSCAN_MAP_TILE_SOURCE_WEB (object);
  HyScanMapTileSourceWebPrivate *priv = nw_source->priv;
  gchar *hash_str;

  G_OBJECT_CLASS (hyscan_map_tile_source_web_parent_class)->constructed (object);

  priv->tile_size = 256;

  /* Формируем тайловую сетку. */
  {
    gdouble min_x, max_x, min_y, max_y;
    guint *nums;
    gint nums_len;
    guint min_zoom = priv->min_zoom, max_zoom = priv->max_zoom, zoom;

    nums_len = max_zoom - min_zoom + 1;
    nums = g_newa (guint , nums_len);
    for (zoom = min_zoom; zoom < max_zoom + 1; zoom++)
      nums[zoom - min_zoom] = 1u << zoom;

    hyscan_geo_projection_get_limits (priv->projection, &min_x, &max_x, &min_y, &max_y);
    priv->grid = hyscan_map_tile_grid_new (min_x, max_x, min_y, max_y, priv->min_zoom, priv->tile_size);
    hyscan_map_tile_grid_set_xnums (priv->grid, nums, nums_len);
  }

  hash_str = g_strdup_printf ("%u:%u:%u:%s",
                              priv->min_zoom, priv->max_zoom,
                              hyscan_geo_projection_hash (priv->projection),
                              priv->url_format);
  priv->hash = g_str_hash (hash_str);
  g_free (hash_str);

  priv->thread_pool = g_thread_pool_new (hyscan_map_tile_source_web_task_do, nw_source, -1, FALSE, NULL);
}

static void
hyscan_map_tile_source_web_object_finalize (GObject *object)
{
  HyScanMapTileSourceWeb *nw_source = HYSCAN_MAP_TILE_SOURCE_WEB (object);
  HyScanMapTileSourceWebPrivate *priv = nw_source->priv;

  g_free (priv->url_format);
  g_list_free_full (priv->headers, g_free);
  g_thread_pool_free (priv->thread_pool, FALSE, TRUE);
  g_clear_object (&priv->grid);
  g_clear_object (&priv->projection);

  G_OBJECT_CLASS (hyscan_map_tile_source_web_parent_class)->finalize (object);
}

/* Заменяет в строке url_tpl одни плейсхолдеры на другие.
 * Возвращает полученную строку, если все плейсхолдеры были найдены; иначе NULL. */
static gchar *
hyscan_map_tile_source_web_replace (const gchar  *url_tpl,
                                    gchar       **find_replace)
{
  gchar *result;
  guint i;

  result = g_strdup (url_tpl);
  for (i = 0; find_replace[i] != NULL; ++i)
    {
      gchar **tokens;
      gchar *find, *replace;

      find    = find_replace[i];
      replace = find_replace[++i];

      /* Разбиваем строку на две части строкой find. */
      tokens = g_strsplit (result, find, 2);
      g_clear_pointer (&result, g_free);

      /* Склеиваем строку обратно, заменяя find на replace. */
      if (tokens[1] != NULL)
        result = g_strconcat (tokens[0], replace, tokens[1], NULL);

      g_strfreev (tokens);

      if (result == NULL)
        break;
    }

  return result;
}

/* Устанавливает printf-формат URL на основе шаблона url_tpl. */
static void
hyscan_map_tile_source_web_set_format (HyScanMapTileSourceWebPrivate *priv,
                                       const gchar                   *url_tpl)
{
  gchar *replace_inv_y[] = {"{-y}", "{y}", NULL};
  gchar *replace_xyz[] = {"{x}", "%1$d", "{y}", "%2$d", "{z}", "%3$d", NULL};
  gchar *replace_quad[] = {"{quadkey}", "%s", NULL};
  gchar *inverse_y = NULL;
  g_free (priv->url_format);

  /* Проверяем, не инвентированна ли ось Y у источника тайлов. */
  inverse_y = hyscan_map_tile_source_web_replace (url_tpl, replace_inv_y);
  priv->inverse_y = (inverse_y != NULL);
  if (priv->inverse_y)
    url_tpl = inverse_y;

  /* Проверяем, подходит ли формат xyz. */
  priv->url_format = hyscan_map_tile_source_web_replace (url_tpl, replace_xyz);
  g_free (inverse_y);
  if (priv->url_format != NULL)
    {
      priv->url_type = URL_FORMAT_XYZ;
      return;
    }

  /* Проверяем, подходит ли формат quad. */
  priv->url_format = hyscan_map_tile_source_web_replace (url_tpl, replace_quad);
  if (priv->url_format != NULL)
    {
      priv->url_type = URL_FORMAT_QUAD;
      return;
    }

  priv->url_type = URL_FORMAT_INVALID;
  g_warning ("HyScanMapTileSourceWeb: wrong URL format");
}

/* Гененрирует quadkey для bing maps. Для удаления g_free.
 * https://docs.microsoft.com/en-us/bingmaps/articles/bing-maps-tile-system#sample-code. */
static gchar *
hyscan_map_tile_source_web_get_quad (HyScanMapTile *tile)
{
  guint x;
  guint y;
  guint zoom;

  guint i;

  gchar *quad_key;

  zoom = hyscan_map_tile_get_zoom (tile);
  x = hyscan_map_tile_get_x (tile);
  y = hyscan_map_tile_get_y (tile);

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

/* Обработчик сигнала "cancelled" у @cancellable. */
static void
hyscan_map_tile_source_web_propagate_cancel (GCancellable               *cancellable,
                                             HyScanMapTileSourceWebTask *task)
{
  /* Сообщаем libsoup, что запрос отменён. */
  g_cancellable_cancel (task->soup_cancellable);

  /* Завершаем работу hyscan_map_tile_source_web_fill_tile(),
   * не дожидаясь libsoup. */
  g_mutex_lock (&task->mutex);
  task->status = STATUS_CANCELLED;
  g_cond_signal (&task->cond);
  g_mutex_unlock (&task->mutex);

}

/* Формирует URL тайла и помещает его в буфер url длины max_length. */
static HyScanMapTileSourceWebTask *
hyscan_map_tile_source_web_task_new (HyScanMapTileSourceWebPrivate *priv,
                                     HyScanMapTile                 *tile,
                                     GCancellable                  *cancellable)
{
  gchar *quad_key;
  guint x, y, z;
  HyScanMapTileSourceWebTask *task;
  GList *link;

  gchar *url;

  switch (priv->url_type)
    {
    case URL_FORMAT_XYZ:
      x = hyscan_map_tile_get_x (tile);
      y = priv->inverse_y ? hyscan_map_tile_inv_y (tile) : hyscan_map_tile_get_y (tile);
      z = hyscan_map_tile_get_zoom (tile);
      url = g_strdup_printf (priv->url_format, x, y, z);
      break;

    case URL_FORMAT_QUAD:
      quad_key = hyscan_map_tile_source_web_get_quad (tile);
      url = g_strdup_printf (priv->url_format, quad_key);
      g_free (quad_key);
      break;

    case URL_FORMAT_INVALID:
    default:
      url = NULL;
      break;
    }

  if (url == NULL)
    return NULL;

  task = g_object_new (HYSCAN_TYPE_MAP_TILE_SOURCE_WEB_TASK, NULL);
  task->tile = g_object_ref (tile);
  task->url = url;
  task->soup_msg = soup_message_new ("GET", task->url);
  link = priv->headers;
  while (link != NULL)
    {
      gchar *hdr_name, *hdr_value;
      hdr_name = link->data;
      link = link->next;
      hdr_value = link->data;
      link = link->next;
      soup_message_headers_append (task->soup_msg->request_headers, hdr_name, hdr_value);
    }

  task->soup_session = soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
                                                      SOUP_SESSION_USER_AGENT, USER_AGENT,
                                                      /* На windows не базы данных сертификатов, поэтому отключаем их проверку. */
                                                      SOUP_SESSION_SSL_STRICT, FALSE,
                                                      NULL);

  /* Делаем HTTP-запрос с отдельным GCancellable,
   * потому что soup_session_send() может сделать g_cancellable_reset(). */
  if (cancellable != NULL)
    {
      task->cancellable = g_object_ref (cancellable);
      task->soup_cancellable = g_cancellable_new ();
      task->handler_id = g_cancellable_connect (task->cancellable,
                                                G_CALLBACK (hyscan_map_tile_source_web_propagate_cancel),
                                                task, NULL);
    }

  return task;
}

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_map_tile_source_web_fill_tile (HyScanMapTileSource *source,
                                      HyScanMapTile       *tile,
                                      GCancellable        *cancellable)
{
  HyScanMapTileSourceWeb *nw_source = HYSCAN_MAP_TILE_SOURCE_WEB (source);
  HyScanMapTileSourceWebPrivate *priv = nw_source->priv;

  HyScanMapTileSourceWebTask *task;

  gboolean status_ok;

  if (g_cancellable_is_cancelled (cancellable))
    return FALSE;

  /* Создаем задачу по загрузке тайла. */
  task = hyscan_map_tile_source_web_task_new (priv, tile, cancellable);
  if (task == NULL)
    return FALSE;

  /* Отправляем HTTP-запрос. */
  g_mutex_lock (&task->mutex);
  task->status = STATUS_PROCESSING;

  g_thread_pool_push (priv->thread_pool, g_object_ref (task), NULL);
  while (task->status == STATUS_PROCESSING)
    g_cond_wait (&task->cond, &task->mutex);

  status_ok = (task->status == STATUS_OK);

  g_mutex_unlock (&task->mutex);

  g_clear_object (&task);

  return status_ok;
}

/* Реализация функции get_grid интерфейса HyScanMapTileSource.*/
static HyScanMapTileGrid *
hyscan_map_tile_source_web_get_grid (HyScanMapTileSource *source)
{
  HyScanMapTileSourceWebPrivate *priv;

  priv = HYSCAN_MAP_TILE_SOURCE_WEB (source)->priv;

  return g_object_ref (priv->grid);
}

/* Реализация функции get_projection интерфейса HyScanMapTileSource.*/
static HyScanGeoProjection *
hyscan_map_tile_source_web_get_projection (HyScanMapTileSource *source)
{
  HyScanMapTileSourceWebPrivate *priv;

  priv = HYSCAN_MAP_TILE_SOURCE_WEB (source)->priv;

  return g_object_ref (priv->projection);
}

/* Реализация функции get_hash интерфейса HyScanMapTileSource.*/
static guint
hyscan_map_tile_source_web_hash (HyScanMapTileSource *source)
{
  HyScanMapTileSourceWebPrivate *priv;

  priv = HYSCAN_MAP_TILE_SOURCE_WEB (source)->priv;

  return priv->hash;
}

/* Реализация интерфейса HyScanMapTileSource. */
static void
hyscan_map_tile_source_web_interface_init (HyScanMapTileSourceInterface *iface)
{
  iface->fill_tile = hyscan_map_tile_source_web_fill_tile;
  iface->get_grid = hyscan_map_tile_source_web_get_grid;
  iface->get_projection = hyscan_map_tile_source_web_get_projection;
  iface->hash = hyscan_map_tile_source_web_hash;
}

/**
 * hyscan_map_tile_source_web_new:
 * @url_format: формат URL тайла с плейсхолдерами
 * @min_zoom: минимальный доступный уровень детализации
 * @max_zoom: максимальный доступный уровень детализации
 *
 * Создаёт новый источник тайлов из сервера тайлов. Подробнее про плейсхолдеры в
 * формате @url_format описано в описании класса #HyScanMapTileSourceWeb.
 *
 * Returns: новый объект #HyScanMapTileSourceWeb. Для удаления g_object_unref().
 */
HyScanMapTileSourceWeb *
hyscan_map_tile_source_web_new (const gchar         *url_format,
                                HyScanGeoProjection *projection,
                                guint                min_zoom,
                                guint                max_zoom)
{
  return g_object_new (HYSCAN_TYPE_MAP_TILE_SOURCE_WEB,
                       "url-format", url_format,
                       "projection", projection,
                       "min-zoom", min_zoom,
                       "max-zoom", max_zoom,
                       NULL);
}

/**
 * hyscan_map_tile_source_add_header:
 * @source: указатель на HyScanMapTileSourceWeb
 * @name: имя заголовка
 * @value: значение
 *
 * Добавляет заголовок в запрос к серверу тайлов. Например, чтобы добавить
 * заголовок "Referer: https://example.com", необходимо передать в качестве аргументов
 * @name - "Referer", @value - "https://example.com".
 */
void
hyscan_map_tile_source_add_header (HyScanMapTileSourceWeb *source,
                                   const gchar            *name,
                                   const gchar            *value)
{
  HyScanMapTileSourceWebPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_SOURCE_WEB (source));
  priv = source->priv;

  priv->headers = g_list_append (priv->headers, g_strdup (name));
  priv->headers = g_list_append (priv->headers, g_strdup (value));
}
