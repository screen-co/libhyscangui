#include "hyscan-tile-manager.h"
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
#include <hyscan-trackrect.h>
#include <math.h>

enum
{
  SHOW                  = 100,
  RECOLOR_AND_SHOW      = 200,
  SHOW_DUMMY            = 500
};

struct _HyScanTileManagerPrivate
{
  HyScanTileQueue     *queue;
  HyScanTileColor     *color;
  HyScanTrackRect     *rect;

  gboolean             cache_set;
  gboolean             open;
  guint64              view_id;
};

enum
{
  SIGNAL_READY,
  SIGNAL_LAST
};

static void    hyscan_tile_manager_object_constructed       (GObject           *object);
static void    hyscan_tile_manager_object_finalize          (GObject           *object);

static void    hyscan_tile_manager_redraw_sender            (HyScanTileQueue   *queue,
                                                             gpointer           user_data);
static void    hyscan_tile_manager_prepare_csurface         (cairo_surface_t  **surface,
                                                             gint               required_width,
                                                             gint               required_height,
                                                             HyScanTileSurface *tile_surface);

static guint   hyscan_tile_manager_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTileManager, hyscan_tile_manager, G_TYPE_OBJECT);

static void
hyscan_tile_manager_class_init (HyScanTileManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_tile_manager_object_finalize;
  object_class->constructed = hyscan_tile_manager_object_constructed;

  /* Создаем сигнал, сообщающий, что нужно перерисовать. */
  hyscan_tile_manager_signals[SIGNAL_READY] =
    g_signal_new ("please-redraw", HYSCAN_TYPE_TILE_MANAGER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
hyscan_tile_manager_init (HyScanTileManager *tilemgr)
{
  tilemgr->priv = hyscan_tile_manager_get_instance_private (tilemgr);
}

static void
hyscan_tile_manager_object_constructed (GObject *object)
{
  HyScanTileManager *tilemgr = HYSCAN_TILE_MANAGER (object);
  HyScanTileManagerPrivate *priv = tilemgr->priv;
  guint n_threads = g_get_num_processors ();

  /* Создаем объекты обработки тайлов. */
  priv->queue = hyscan_tile_queue_new (n_threads);
  priv->color = hyscan_tile_color_new ();
  priv->rect  = hyscan_trackrect_new ();

  g_signal_connect (priv->queue, "tile-queue-ready", G_CALLBACK (hyscan_tile_manager_redraw_sender), tilemgr);
}

static void
hyscan_tile_manager_object_finalize (GObject *object)
{
  HyScanTileManager *tilemgr = HYSCAN_TILE_MANAGER (object);
  HyScanTileManagerPrivate *priv = tilemgr->priv;

  g_clear_object (&(priv->color));
  g_clear_object (&(priv->queue));

  G_OBJECT_CLASS (hyscan_tile_manager_parent_class)->finalize (object);
}

/* Функция отсылает сигнал "необходимо перерисовать". */
static void
hyscan_tile_manager_redraw_sender (HyScanTileQueue *queue,
                                   gpointer         user_data)
{
  g_signal_emit (HYSCAN_TILE_MANAGER (user_data), hyscan_tile_manager_signals[SIGNAL_READY], 0, NULL);
}

/* Функция проверяет параметры cairo_surface_t и пересоздает в случае необходимости. */
static void
hyscan_tile_manager_prepare_csurface (cairo_surface_t **surface,
                                     gint               required_width,
                                     gint               required_height,
                                     HyScanTileSurface *tile_surface)
{
  gint w, h;

  g_return_if_fail (tile_surface != NULL);

  if (surface == NULL)
    return;

  if (*surface != NULL)
    {
      w = cairo_image_surface_get_width (*surface);
      h = cairo_image_surface_get_height (*surface);
      if (w != required_width || h != required_height)
        {
          cairo_surface_destroy (*surface);
          *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, required_width, required_height);
        }
    }
  else
    {
      *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, required_width, required_height);
    }

    tile_surface->width = cairo_image_surface_get_width (*surface);
    tile_surface->height = cairo_image_surface_get_height (*surface);
    tile_surface->stride = cairo_image_surface_get_stride (*surface);
    tile_surface->data = cairo_image_surface_get_data (*surface);
}

/* Функция создает новый объект. */
HyScanTileManager*
hyscan_tile_manager_new (void)
{
  return g_object_new (HYSCAN_TYPE_TILE_MANAGER, NULL);
}

/* Функция установки кэша. */
void
hyscan_tile_manager_set_cache (HyScanTileManager *tilemgr,
                               HyScanCache       *cache,
                               HyScanCache       *cache2,
                               const gchar       *cache_prefix)
{
  HyScanTileManagerPrivate *priv;
  g_return_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr));
  priv = tilemgr->priv;

  /* Запрещаем менять систему кэширования на лету. */
  if (priv->open)
    return;

  /* Запрещаем NULL вместо кэша. */
  if (cache == NULL || cache2 == NULL)
    return;

  hyscan_tile_color_set_cache (priv->color, cache2, cache_prefix);
  hyscan_tile_queue_set_cache (priv->queue, cache, cache_prefix);
  hyscan_trackrect_set_cache (priv->rect, cache, cache_prefix);

  /* Поднимаем флаг. */
  priv->cache_set = TRUE;
}

/* Функция устанавливает параметры определения линии дна. */
void
hyscan_tile_manager_setup_depth (HyScanTileManager *tilemgr,
                                 HyScanDepthSource  source,
                                 gint               size,
                                 gulong             microseconds)
{
  HyScanTileManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr));
  priv = tilemgr->priv;

  /* Запрещаем менять на лету. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_depth (priv->queue, source, size, microseconds);
  hyscan_trackrect_set_depth (priv->rect, source, size, microseconds);
}

/* Функция устанавливает скорость звука и судна. */
void
hyscan_tile_manager_set_speeds (HyScanTileManager      *tilemgr,
                                gfloat                  ship_speed,
                                GArray                 *sound_speed)
{
  HyScanTileManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr));
  priv = tilemgr->priv;

  /* Запрещаем менять на лету. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_speed (priv->queue, ship_speed, sound_speed);
  hyscan_trackrect_set_speeds (priv->rect, ship_speed, sound_speed);
}

/* Функция обновляет параметры HyScanTileQueue. */
gboolean
hyscan_tile_manager_update_tilequeue (HyScanTileManager *tilemgr,
                                      HyScanTileType     type,
                                      guint              upsample)
{
  HyScanTileManagerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);
  priv = tilemgr->priv;

  /* Настраивать HyScanTileQueue можно только когда галс не открыт. */
  if (priv->open)
    return FALSE;

  hyscan_tile_queue_set_params (priv->queue, type, upsample);
  hyscan_trackrect_set_type (priv->rect, type);
  hyscan_tile_manager_redraw_sender (NULL, tilemgr);

  return TRUE;
}

/* Функция обновляет параметры HyScanTileColor. */
gboolean
hyscan_tile_manager_set_colormap (HyScanTileManager  *tilemgr,
                                  guint32            *colormap,
                                  gint                levels,
                                  guint32             background)
{
  gboolean status = FALSE;
  HyScanTileManagerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);
  priv = tilemgr->priv;

  status = hyscan_tile_color_set_colormap (priv->color, colormap, levels, background);
  hyscan_tile_manager_redraw_sender (NULL, tilemgr);

  return status;
}

gboolean
hyscan_tile_manager_set_levels (HyScanTileManager  *tilemgr,
                                gdouble             black,
                                gdouble             gamma,
                                gdouble             white)
{
  gboolean status = FALSE;
  HyScanTileManagerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);
  priv = tilemgr->priv;

  status = hyscan_tile_color_set_levels (priv->color, black, gamma, white);
  hyscan_tile_manager_redraw_sender (NULL, tilemgr);

  return status;
}

/* Функция открытия БД, проекта, галса и т.д. */
void
hyscan_tile_manager_open (HyScanTileManager      *tilemgr,
                          HyScanDB               *db,
                          const gchar            *project,
                          const gchar            *track,
                          HyScanTileSource        source,
                          gboolean                raw)
{
  HyScanTileManagerPrivate *priv;
  gchar *db_uri;

  g_return_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr));
  priv = tilemgr->priv;

  /* Запрещаем вызывать эту функцию, если не установлен кэш. */
  if (!priv->cache_set)
    return;

  /* Вызываем соответствующие методы _open. */
  db_uri = hyscan_db_get_uri (db);

  hyscan_tile_color_open (priv->color, db_uri, project, track);
  hyscan_tile_queue_open (priv->queue, db, project, track, source, raw);
  hyscan_trackrect_open (priv->rect, db, project, track, raw);

  priv->open = TRUE;
  hyscan_tile_manager_redraw_sender (NULL, tilemgr);

  g_free (db_uri);
}

void
hyscan_tile_manager_close (HyScanTileManager      *tilemgr)
{
  HyScanTileManagerPrivate *priv;
  g_return_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr));
  priv = tilemgr->priv;

  hyscan_tile_color_close (priv->color);
  hyscan_tile_queue_close (priv->queue);
  hyscan_trackrect_close (priv->rect);

  priv->open = FALSE;
}

/* Функция, начинающая view. */
gboolean
hyscan_tile_manager_view_start (HyScanTileManager *tilemgr)
{
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);
  tilemgr->priv->view_id++;
  return TRUE;
}

/* Функция получения тайла. */
gboolean
hyscan_tile_manager_get_tile (HyScanTileManager *tilemgr,
                              HyScanTile        *tile,
                              cairo_surface_t  **surface)
{
  HyScanTileManagerPrivate *priv;

  HyScanTile requested_tile = *tile,
             queue_tile,
             color_tile;
  gboolean   queue_found   = FALSE,
             color_found   = FALSE,
             regenerate    = FALSE,
             mod_count_ok  = FALSE,
             queue_equal   = FALSE,
             color_equal   = FALSE;
  gint       show_strategy = 0;
  gfloat    *buffer        = NULL;
  guint32    buffer_size   = 0;
  HyScanTileSurface tile_surface;

  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);
  priv = tilemgr->priv;

  if (tile == NULL)
    return FALSE;

  /* Запрещаем работу, если кэш не установлен и не открыты БД/проект/галс. */
  g_return_val_if_fail (priv->open, FALSE);

  /* Дописываем в структуру с тайлом всю необходимую информацию (из очереди). */
  hyscan_tile_queue_prepare_tile (priv->queue, &requested_tile);

  /* Собираем информацию об имеющихся тайлах и их тождественности. */
  queue_found = hyscan_tile_queue_check (priv->queue, &requested_tile, &queue_tile, &queue_equal, &regenerate);
  color_found = hyscan_tile_color_check (priv->color, &requested_tile, &color_tile, &mod_count_ok, &color_equal);

  /* На основании этого определяем, как поступить. */
  if (color_found && color_equal && mod_count_ok &&
      queue_tile.finalized == color_tile.finalized)  /* Кэш2: правильный и неустаревший тайл. */
    {
      show_strategy = SHOW;
      hyscan_tile_manager_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found && queue_equal)             /* Кэш2: неправильный тайл; кэш1: правильный. */
    {
      show_strategy = RECOLOR_AND_SHOW;
      hyscan_tile_manager_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else if (color_found)                            /* Кэш2: неправильный тайл; кэш1: неправильный. */
    {
      show_strategy = SHOW;
      regenerate = TRUE;
      hyscan_tile_manager_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found)                            /* Кэш2: ничего нет; кэш1: неправильный. */
    {
      show_strategy = RECOLOR_AND_SHOW;
      regenerate = TRUE;
      hyscan_tile_manager_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else                                             /* Кэш2: ничего; кэш1: ничего. Заглушка. */
    {
      show_strategy = SHOW_DUMMY;
      regenerate = TRUE;
    }

  /* Теперь можно запросить тайл откуда следует и отправить его куда следует. */
  if (regenerate)
    hyscan_tile_queue_add (priv->queue, &requested_tile);

  if (surface == NULL)
    return FALSE;

  switch (show_strategy)
    {
    case SHOW:
      /* Просто отображаем тайл. */
      *tile = color_tile;
      return hyscan_tile_color_get (priv->color, &requested_tile, &color_tile, &tile_surface);

    case RECOLOR_AND_SHOW:
      /* Перекрашиваем и отображаем. */
      queue_found = hyscan_tile_queue_get (priv->queue, &requested_tile, &queue_tile, &buffer, &buffer_size);
      *tile = queue_tile;
      hyscan_tile_color_add (priv->color, &queue_tile, buffer, buffer_size, &tile_surface);
      g_free (buffer);
      break;

    case SHOW_DUMMY:
    default:
      return FALSE;
    }

  return TRUE;
}

/* Функция, завершающая view. */
gboolean
hyscan_tile_manager_view_end (HyScanTileManager *tilemgr)
{
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), FALSE);

  /* Запрещаем работу, если кэш не установлен и не открыты БД/проект/галс. */
  if (!tilemgr->priv->open)
    return FALSE;

  hyscan_tile_queue_add_finished (tilemgr->priv->queue, tilemgr->priv->view_id);

  return TRUE;
}

gboolean
hyscan_tile_manager_track_params (HyScanTileManager *tilemgr,
                                   gdouble           *lwidth,
                                   gdouble           *rwidth,
                                   gdouble           *length)
{
  g_return_val_if_fail (HYSCAN_IS_TILE_MANAGER (tilemgr), TRUE);

  return hyscan_trackrect_get (tilemgr->priv->rect, lwidth, rwidth, length);
}
