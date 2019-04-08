/* hyscan-gtk-map-tile-source.c
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

/* Вспомогательный класс HyScanNetworkMapTileSourceTask - задача по загрузке тайла. */
#define HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE_TASK        (hyscan_network_map_tile_source_task_get_type ())
#define HYSCAN_NETWORK_MAP_TILE_SOURCE_TASK(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE_TASK, HyScanNetworkMapTileSourceTask))

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
  HyScanGtkMapTile         *tile;              /* Тайл, который надо заполнить. */
  gchar                    *url;               /* URL тайла. */
  GCancellable             *cancellable;       /* Объект для отмена задачи. */
  GCancellable             *soup_cancellable;  /* Объект для отмены, передаваемый в libsoup. */
  gulong                    handler_id;        /* Хэндлер обработчика сигнала от cancellable. */
  SoupSession              *soup_session;      /* HTTP-клиент, который загружает изображения тайлов. */
  SoupMessage              *soup_msg;          /* Запрос на сервер. */
} HyScanNetworkMapTileSourceTask;

typedef struct
{
  GObjectClass parent_class;
} HyScanNetworkMapTileSourceTaskClass;

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

  GThreadPool                    *thread_pool;   /* Пул потоков, обрабатывающих задачи по загрузке тайлов. */
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
static HyScanNetworkMapTileSourceTask *
                      hyscan_network_map_tile_source_task_new            (HyScanNetworkMapTileSourcePrivate *priv,
                                                                          HyScanGtkMapTile                  *tile,
                                                                          GCancellable                      *cancellable);
static gchar *        hyscan_network_map_tile_source_replace             (const gchar                       *url_tpl,
                                                                          gchar                            **find,
                                                                          gchar                            **replace);

G_DEFINE_TYPE_WITH_CODE (HyScanNetworkMapTileSource, hyscan_network_map_tile_source, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNetworkMapTileSource)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                                                hyscan_network_map_tile_source_interface_init))
G_DEFINE_TYPE (HyScanNetworkMapTileSourceTask, hyscan_network_map_tile_source_task, G_TYPE_OBJECT)

static void
hyscan_network_map_tile_source_task_init (HyScanNetworkMapTileSourceTask *task)
{
  task->handler_id = 0;
  task->status = STATUS_IDLE;
  g_mutex_init (&task->mutex);
  g_cond_init (&task->cond);
}

static void
hyscan_network_map_tile_source_task_finalize (GObject *object)
{
  HyScanNetworkMapTileSourceTask *task = HYSCAN_NETWORK_MAP_TILE_SOURCE_TASK (object);
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
hyscan_network_map_tile_source_task_class_init (HyScanNetworkMapTileSourceTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = hyscan_network_map_tile_source_task_finalize;
}

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

/* Выполняет загрузку изображения и установку его в тайл. */
static void
hyscan_network_map_tile_source_task_do (gpointer data,
                                        gpointer user_data)
{
  HyScanNetworkMapTileSourceTask *task = data;

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
  if (hyscan_gtk_map_tile_set_pixbuf (task->tile, pixbuf))
    task->status = STATUS_OK;
  else
    task->status = STATUS_FAILED;

error:
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("HyScanNetworkMapTileSource: failed to read \"%s\" (%s)", task->url, error->message);

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
hyscan_network_map_tile_source_object_constructed (GObject *object)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (object);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  G_OBJECT_CLASS (hyscan_network_map_tile_source_parent_class)->constructed (object);

  priv->tile_size = 256;

  priv->thread_pool = g_thread_pool_new (hyscan_network_map_tile_source_task_do, nw_source, -1, FALSE, NULL);
}

static void
hyscan_network_map_tile_source_object_finalize (GObject *object)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (object);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  g_free (priv->url_format);
  g_thread_pool_free (priv->thread_pool, FALSE, TRUE);

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
  g_warning ("HyScanNetworkMapTileSource: wrong URL format");
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

/* Обработчик сигнала "cancelled" у @cancellable. */
static void
hyscan_network_map_tile_source_propagate_cancel (GCancellable                   *cancellable,
                                                 HyScanNetworkMapTileSourceTask *task)
{
  /* Сообщаем libsoup, что запрос отменён. */
  g_cancellable_cancel (task->soup_cancellable);

  /* Завершаем работу hyscan_network_map_tile_source_fill_tile(),
   * не дожидаясь libsoup. */
  g_mutex_lock (&task->mutex);
  task->status = STATUS_CANCELLED;
  g_cond_signal (&task->cond);
  g_mutex_unlock (&task->mutex);

}

/* Формирует URL тайла и помещает его в буфер url длины max_length. */
static HyScanNetworkMapTileSourceTask *
hyscan_network_map_tile_source_task_new (HyScanNetworkMapTileSourcePrivate *priv,
                                         HyScanGtkMapTile                  *tile,
                                         GCancellable                      *cancellable)
{
  gchar *quad_key;
  HyScanNetworkMapTileSourceTask *task;

  gchar *url;

  switch (priv->url_type)
    {
    case URL_FORMAT_XYZ:
      url = g_strdup_printf (priv->url_format,
                             hyscan_gtk_map_tile_get_x (tile),
                             hyscan_gtk_map_tile_get_y (tile),
                             hyscan_gtk_map_tile_get_zoom (tile));
      break;

    case URL_FORMAT_QUAD:
      quad_key = hyscan_network_map_tile_source_get_quad (tile);
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

  task = g_object_new (HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE_TASK, NULL);
  task->tile = g_object_ref (tile);
  task->url = url;
  task->soup_msg = soup_message_new ("GET", task->url);
  task->soup_session = soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
                                                 SOUP_SESSION_USER_AGENT, USER_AGENT,
                                                 NULL);

  /* Делаем HTTP-запрос с отдельным GCancellable,
   * потому что soup_session_send() может сделать g_cancellable_reset(). */
  if (cancellable != NULL)
    {
      task->cancellable = g_object_ref (cancellable);
      task->soup_cancellable = g_cancellable_new ();
      task->handler_id = g_cancellable_connect (task->cancellable,
                                                G_CALLBACK (hyscan_network_map_tile_source_propagate_cancel),
                                                task, NULL);
    }

  return task;
}

/* Ищет указанный тайл и загружает его. */
static gboolean
hyscan_network_map_tile_source_fill_tile (HyScanGtkMapTileSource *source,
                                          HyScanGtkMapTile       *tile,
                                          GCancellable           *cancellable)
{
  HyScanNetworkMapTileSource *nw_source = HYSCAN_NETWORK_MAP_TILE_SOURCE (source);
  HyScanNetworkMapTileSourcePrivate *priv = nw_source->priv;

  HyScanNetworkMapTileSourceTask *task;

  gboolean status_ok;

  if (g_cancellable_is_cancelled (cancellable))
    return FALSE;

  /* Создаем задачу по загрузке тайла. */
  task = hyscan_network_map_tile_source_task_new (priv, tile, cancellable);
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

/* Реализация функции get_zoom_limits интерфейса HyScanGtkMapTileSource.*/
static void
hyscan_network_map_tile_source_get_zoom_limits (HyScanGtkMapTileSource *source,
                                                guint                  *min_zoom,
                                                guint                  *max_zoom)
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
 * @url_format: формат URL тайла с плейсхолдерами
 * @min_zoom: минимальный доступный уровень детализации
 * @max_zoom: максимальный доступный уровень детализации
 *
 * Создаёт новый источник тайлов из сервера тайлов. Подробнее о плейсхолдерах в
 * описании класса #HyScanNetworkMapTileSource.
 *
 * Returns: новый объект #HyScanNetworkMapTileSource. Для удаления g_object_unref().
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
