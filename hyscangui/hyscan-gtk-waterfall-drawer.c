/*
 * \file hyscan-gtk-drawer.c
 *
 * \brief Исходный файл виджета водопад.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include "hyscan-gtk-waterfall-drawer.h"
#include <hyscan-gtk-waterfall-private.h>
#include <hyscan-gui-marshallers.h>
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
#include <hyscan-track-rect.h>
#include <math.h>

#define ZOOM_LEVELS 15
#define EPS -0.000001

enum
{
  SIGNAL_AUTOMOVE_STATE,
  SIGNAL_ZOOM,
  SIGNAL_LAST
};

enum
{
  SHOW        = 100,
  RECOLOR     = 200,
  SHOW_DUMMY  = 500
};

static const gint tile_sizes[ZOOM_LEVELS] =    {500e3,  200e3,  100e3,  80e3,  50e3,  40e3,  20e3,  10e3,  5e3,  2e3,  1e3,  500, 400, 200, 100};   /**< в миллиметрах*/
static const gdouble zooms_gost[ZOOM_LEVELS] = {5000.0, 2000.0, 1000.0, 800.0, 500.0, 400.0, 200.0, 100.0, 50.0, 20.0, 10.0, 5.0, 4.0, 2.0, 1.0};

struct _HyScanGtkWaterfallDrawerPrivate
{
  gfloat                 ppi;                /**< PPI. */
  cairo_surface_t       *surface;            /**< Поверхность тайла. */
  cairo_surface_t       *dummy;              /**< Поверхность заглушки. */

  HyScanTileQueue       *queue;
  HyScanTileColor       *color;

  guint64                view_id;

  gdouble               *zooms;              /**< Масштабы с учетом PPI. */
  gint                   zoom_index;

  gboolean               open;               /**< Флаг "галс открыт". */

  gboolean               view_finalised;     /**< "Все тайлы для этого вью найдены и показаны". */

  guint32                dummy_color;        /**< Цвет, которым будут рисоваться заглушки. */

  HyScanTrackRect       *lrect;
  HyScanTrackRect       *rrect;
  gboolean               writeable;
  gboolean               init;
  gboolean               once;
  gdouble                length;             /**< Длина галса. */
  gdouble                lwidth;             /**< Ширина по левому борту. */
  gdouble                rwidth;             /**< Ширина по правому борту. */

  guint                  widget_width;
  guint                  widget_height;

  HyScanGtkWaterfallType widget_type;
  HyScanSourceType       left_source;
  HyScanSourceType       right_source;

  gfloat                 ship_speed;
  gint64                 prev_time;
  gboolean               automove;           /**< Включение и выключение режима автоматической сдвижки. */
  guint                  auto_tag;           /**< Идентификатор функции сдвижки. */
  guint                  automove_time;      /**< Период обновления экрана. */

  gdouble                mouse_y;            /**< Y-координата мыши в момент нажатия кнопки. */
  gdouble                mouse_x;            /**< X-координата мыши в момент нажатия кнопки. */

  guint                  regen_time;         /**< Время предыдущей генерации. */
  guint                  regen_time_prev;    /**< Время предыдущей генерации. */
  guint                  regen_period;       /**< Интервал между перегенерациями. */
  gboolean               regen_sent;         /**< Интервал между перегенерациями. */
  gboolean               regen_allowed;         /**< Интервал между перегенерациями. */
};

/* Внутренние методы класса. */
static HyScanSourceType hyscan_gtk_waterfall_drawer_queue_source    (HyScanTile                    *tile,
                                                                     gpointer                       data);
static gboolean         hyscan_gtk_waterfall_drawer_queue_rotate    (HyScanTile                    *tile,
                                                                     gpointer                       data);

static void     hyscan_gtk_waterfall_drawer_object_constructed      (GObject                       *object);
static void     hyscan_gtk_waterfall_drawer_object_finalize         (GObject                       *object);

static void     hyscan_gtk_waterfall_drawer_visible_draw            (GtkWidget                     *widget,
                                                                     cairo_t                       *cairo);

static gboolean hyscan_gtk_waterfall_drawer_configure               (GtkWidget                     *widget,
                                                                     GdkEventConfigure             *event);

static gint32   hyscan_gtk_waterfall_drawer_aligner                 (gdouble                        in,
                                                                     gint                           size);
static void     hyscan_gtk_waterfall_drawer_prepare_csurface        (cairo_surface_t              **surface,
                                                                     gint                           required_width,
                                                                     gint                           required_height,
                                                                     HyScanTileSurface             *tile_surface);
static gboolean hyscan_gtk_waterfall_drawer_get_tile                (HyScanGtkWaterfallDrawer      *drawer,
                                                                     HyScanTile                    *tile,
                                                                     cairo_surface_t              **tile_surface);

static void     hyscan_gtk_waterfall_drawer_open                    (HyScanGtkWaterfall            *waterfall,
                                                                     HyScanDB                      *db,
                                                                     const gchar                   *project_name,
                                                                     const gchar                   *track_name,
                                                                     gboolean                       raw);
static void     hyscan_gtk_waterfall_drawer_close                   (HyScanGtkWaterfall            *waterfall);

static void     hyscan_gtk_waterfall_drawer_cifroarea_check_scale   (GtkCifroArea                  *carea,
                                                                     gdouble                       *scale_x,
                                                                     gdouble                       *scale_y);

static void     hyscan_gtk_waterfall_drawer_zoom_internal           (GtkCifroArea                  *carea,
                                                                     GtkCifroAreaZoomType           direction_x,
                                                                     GtkCifroAreaZoomType           direction_y,
                                                                     gdouble                        center_x,
                                                                     gdouble                        center_y);
static void     hyscan_gtk_waterfall_drawer_cifroarea_get_limits    (GtkCifroArea                  *carea,
                                                                     gdouble                       *min_x,
                                                                     gdouble                       *max_x,
                                                                     gdouble                       *min_y,
                                                                     gdouble                       *max_y);
static void     hyscan_gtk_waterfall_drawer_cifroarea_get_stick     (GtkCifroArea                  *carea,
                                                                     GtkCifroAreaStickType         *stick_x,
                                                                     GtkCifroAreaStickType         *stick_y);

static gboolean hyscan_gtk_waterfall_drawer_automover               (gpointer                       data);

static gboolean hyscan_gtk_waterfall_drawer_button_press           (GtkWidget                     *widget,
                                                                     GdkEventButton                *event,
                                                                     gpointer                       data);
static gboolean hyscan_gtk_waterfall_drawer_motion                 (GtkWidget                     *widget,
                                                                     GdkEventMotion                *event,
                                                                     gpointer                       data);

static guint    hyscan_gtk_waterfall_drawer_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfallDrawer, hyscan_gtk_waterfall_drawer, HYSCAN_TYPE_GTK_WATERFALL);

static void
hyscan_gtk_waterfall_drawer_class_init (HyScanGtkWaterfallDrawerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkWaterfallClass *waterfall = HYSCAN_GTK_WATERFALL_CLASS (klass);
  GtkCifroAreaClass *carea = GTK_CIFRO_AREA_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_drawer_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_drawer_object_finalize;

  waterfall->open = hyscan_gtk_waterfall_drawer_open;
  waterfall->close = hyscan_gtk_waterfall_drawer_close;

  carea->check_scale = hyscan_gtk_waterfall_drawer_cifroarea_check_scale;
  carea->get_limits = hyscan_gtk_waterfall_drawer_cifroarea_get_limits;
  carea->get_stick = hyscan_gtk_waterfall_drawer_cifroarea_get_stick;
  carea->zoom = hyscan_gtk_waterfall_drawer_zoom_internal;

  hyscan_gtk_waterfall_drawer_signals[SIGNAL_AUTOMOVE_STATE] =
    g_signal_new ("automove-state", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_BOOLEAN);

  hyscan_gtk_waterfall_drawer_signals[SIGNAL_ZOOM] =
    g_signal_new ("waterfall-zoom", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT_DOUBLE,
                  G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_DOUBLE);
}

static void
hyscan_gtk_waterfall_drawer_init (HyScanGtkWaterfallDrawer *drawer)
{
  drawer->priv = hyscan_gtk_waterfall_drawer_get_instance_private (drawer);
}

static HyScanSourceType
hyscan_gtk_waterfall_drawer_queue_source (HyScanTile *tile,
                                          gpointer    data)
{
  HyScanGtkWaterfallDrawerPrivate *priv = data;

  switch (priv->widget_type)
    {
    case HYSCAN_GTK_WATERFALL_SIDESCAN:
      if (tile->across_start <= 0 && tile->across_end <= 0)
        return priv->left_source;
      else
        return priv->right_source;

    case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
    default:
      return priv->right_source;
    }
}

static gboolean
hyscan_gtk_waterfall_drawer_queue_rotate (HyScanTile *tile,
                                          gpointer    data)
{
  HyScanGtkWaterfallDrawerPrivate *priv = data;

  switch (priv->widget_type)
    {
    case HYSCAN_GTK_WATERFALL_SIDESCAN:
      return FALSE;
    case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
    default:
      return TRUE;
    }
}

static void
hyscan_gtk_waterfall_drawer_object_constructed (GObject *object)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (object);
  HyScanGtkWaterfallDrawerPrivate *priv;
  guint n_threads = g_get_num_processors ();

  G_OBJECT_CLASS (hyscan_gtk_waterfall_drawer_parent_class)->constructed (object);
  priv = drawer->priv;

  n_threads = 1;
  priv->queue = hyscan_tile_queue_new (n_threads);
  priv->color = hyscan_tile_color_new ();
  priv->lrect = hyscan_track_rect_new ();
  priv->rrect = hyscan_track_rect_new ();

  /* Параметры GtkCifroArea по умолчанию. */
  gtk_cifro_area_set_scale_on_resize (GTK_CIFRO_AREA (drawer), FALSE);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (drawer), -50, 50, 0, 100 );

  g_signal_connect_swapped (priv->queue, "tile-queue-ready", G_CALLBACK (gtk_widget_queue_draw), GTK_WIDGET(drawer));

  // TODO: может, перенести в _опен и делать дисконнект в _клоуз?
  g_signal_connect (drawer, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_drawer_visible_draw), NULL);
  g_signal_connect_after (drawer, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_configure), NULL);

  g_signal_connect (drawer, "button-press-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_button_press), drawer);
  g_signal_connect (drawer, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_motion), drawer);

  priv->view_finalised = 0;
  priv->automove_time = 40;
  priv->regen_period = 1 * G_TIME_SPAN_SECOND;
}

static void
hyscan_gtk_waterfall_drawer_object_finalize (GObject *object)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (object);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  cairo_surface_destroy (priv->surface);

  g_free (priv->zooms);

  g_clear_object (&priv->queue);
  g_clear_object (&priv->color);
  g_clear_object (&priv->lrect);
  g_clear_object (&priv->rrect);

  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = 0;
    }

  G_OBJECT_CLASS (hyscan_gtk_waterfall_drawer_parent_class)->finalize (object);
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_drawer_visible_draw (GtkWidget *widget,
                                          cairo_t   *cairo)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  HyScanTile tile;
  gint32 tile_size      = 0,
         start_tile_x0  = 0,
         start_tile_y0  = 0,
         end_tile_x0    = 0,
         end_tile_y0    = 0;
  gint num_of_tiles_x = 0,
       num_of_tiles_y = 0,
       i, j;
  gdouble from_x, from_y, to_x, to_y,
          x_coord, y_coord, x_coord0, y_coord0,
          scale;
  gboolean view_finalised = TRUE;

  if (!priv->open)
    return;

  gtk_cifro_area_get_view	(GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (widget), &scale, NULL);

  from_x *= 1000.0;
  to_x   *= 1000.0;
  from_y *= 1000.0;
  to_y   *= 1000.0;
  scale  *= 1000.0 / (25.4 / priv->ppi);

  /* Определяем размер тайла. */
  tile_size = tile_sizes[priv->zoom_index];

  /* Определяем параметры искомых тайлов. */
  start_tile_x0 = hyscan_gtk_waterfall_drawer_aligner (from_x, tile_size);
  start_tile_y0 = hyscan_gtk_waterfall_drawer_aligner (from_y, tile_size);
  end_tile_x0   = hyscan_gtk_waterfall_drawer_aligner (to_x,   tile_size);
  end_tile_y0   = hyscan_gtk_waterfall_drawer_aligner (to_y,   tile_size);

  num_of_tiles_x = (end_tile_x0 - start_tile_x0) / tile_size + 1;
  num_of_tiles_y = (end_tile_y0 - start_tile_y0) / tile_size + 1;

  /* Начался новый view. */
  priv->view_id++;

  /* Определяем возможность ПЕРЕгенерации тайлов. */
  priv->regen_time = g_get_monotonic_time ();
  priv->regen_allowed =  priv->regen_time - priv->regen_time_prev > priv->regen_period;

  gtk_cifro_area_visible_value_to_point	(GTK_CIFRO_AREA (widget), &x_coord0, &y_coord0,
                                        (gdouble)(start_tile_x0 / 1000.0),
                                        (gdouble)((start_tile_y0 + tile_size) / 1000.0));
  x_coord0 = round(x_coord0);
  y_coord0 = round(y_coord0);

  /* Ищем тайлы .*/
  tile.scale = scale;
  tile.ppi = priv->ppi;

  for (j = 0; j < num_of_tiles_x; j++)
    {
      for (i = num_of_tiles_y - 1; i >= 0; i--)
        {
          /* Виджет знает только о том, какие размеры тайла, масштаб и ppi.
           * У него нет информации ни о цветовой схеме, ни о фильтре. */
          switch (priv->widget_type)
            {
            case HYSCAN_GTK_WATERFALL_SIDESCAN:
              tile.across_start = start_tile_x0 + j * tile_size;
              tile.along_start  = start_tile_y0 + i * tile_size;
              tile.across_end   = start_tile_x0 + (j + 1) * tile_size;
              tile.along_end    = start_tile_y0 + (i + 1) * tile_size;
              break;

            case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
            default:
              tile.along_start  = start_tile_x0 + j * tile_size;
              tile.across_start = start_tile_y0 + i * tile_size;
              tile.along_end    = start_tile_x0 + (j + 1) * tile_size;
              tile.across_end   = start_tile_y0 + (i + 1) * tile_size;
            }

          /* Делаем запрос тайл-менеджеру. */
          if (hyscan_gtk_waterfall_drawer_get_tile (drawer, &tile, &(priv->surface)))
            {
              /* Отрисовываем тайл */
              x_coord = x_coord0 + j * tile.w;
              y_coord = y_coord0 - i * tile.h;

              cairo_surface_mark_dirty (priv->surface);
              cairo_save (cairo);
              cairo_translate (cairo, x_coord, y_coord);
              cairo_set_source_surface (cairo, priv->surface, 0, 0);
              cairo_paint (cairo);
              cairo_restore (cairo);
            }
          else
            {
              view_finalised = FALSE;
              /* Заглушечка-тудушечка */
            }
        }
    }

  priv->view_finalised = view_finalised;

  if (priv->regen_sent)
    {
      priv->regen_time_prev = priv->regen_time;
      priv->regen_sent = FALSE;
    }

  /* Необходимо сообщить тайл-менеджеру, что view закончился. */
  hyscan_tile_queue_add_finished (priv->queue, priv->view_id);
}

/* Функция обработки сигнала изменения параметров дисплея. */
static gboolean
hyscan_gtk_waterfall_drawer_configure (GtkWidget         *widget,
                                       GdkEventConfigure *event)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;
  GdkScreen *gdkscreen;
  GdkRectangle monitor;

  gint i = 0;
  gint monitor_num, monitor_h, monitor_w;
  gfloat ppi, diagonal_mm, diagonal_pix;

  /* Получаем монитор, на котором расположено окно. */
  gdkscreen = gdk_window_get_screen (event->window);
  monitor_num = gdk_screen_get_monitor_at_window (gdkscreen, event->window);

  /* Диагональ в пикселях. */
  gdk_screen_get_monitor_geometry (gdkscreen, monitor_num, &monitor);
  diagonal_pix = sqrt (monitor.width * monitor.width + monitor.height * monitor.height);

  /* Диагональ в миллиметрах. */
  monitor_h = gdk_screen_get_monitor_height_mm (gdkscreen, monitor_num);
  monitor_w = gdk_screen_get_monitor_width_mm (gdkscreen, monitor_num);
  diagonal_mm = sqrt (monitor_w * monitor_w + monitor_h * monitor_h) / 25.4;

  /* Вычисляем PPI. */
  ppi = diagonal_pix / diagonal_mm;
  if (isnan (ppi) || isinf(ppi) || ppi <= 0.0 || monitor_h <= 0 || monitor_w <= 0)
    ppi = 96.0;
  priv->ppi = ppi;

  /* Обновляем масштабы. */
  if (priv->zooms == NULL)
    priv->zooms = g_malloc0 (ZOOM_LEVELS * sizeof (gdouble));

  for (i = 0; i < ZOOM_LEVELS; i++)
    priv->zooms[i] = zooms_gost[i] / (1000.0 / (25.4 / priv->ppi));

  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (widget), priv->zooms[priv->zoom_index], priv->zooms[priv->zoom_index], 0, 0);

  gtk_cifro_area_get_size (GTK_CIFRO_AREA (widget), &priv->widget_width, &priv->widget_height);

  /* TODO: Пересоздаем заглушку. */
  // cairo_surface_destroy (priv->dummy);
  // priv->dummy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  gtk_widget_queue_draw (widget);
  return FALSE;
}

/* Округление координат тайла. */
static gint32
hyscan_gtk_waterfall_drawer_aligner (gdouble in,
                                     gint    size)
{
  gint32 out = 0;

  if (in > 0)
    {
      out = floor(in) / size;
      out *= size;
    }

  if (in < 0)
    {
      out = ceil(in) / size;
      out = (out - 1) * size;
    }

  return out;
}

/* Функция проверяет параметры cairo_surface_t и пересоздает в случае необходимости. */
static void
hyscan_gtk_waterfall_drawer_prepare_csurface (cairo_surface_t  **surface,
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

/* Функция получения тайла. */
static gboolean
hyscan_gtk_waterfall_drawer_get_tile (HyScanGtkWaterfallDrawer *drawer,
                                      HyScanTile               *tile,
                                      cairo_surface_t         **surface)
{
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;;

  HyScanTile requested_tile, queue_tile, color_tile;
  gboolean   queue_found, color_found;
  gboolean   regenerate, mod_count_ok;
  gboolean   queue_equal, color_equal;
  gint       show_strategy = SHOW_DUMMY;
  gfloat    *buffer        = NULL; // TODO: NO REALLOC (это надо фиксить на уровне очереди)
  guint32    buffer_size   = 0;
  HyScanTileSurface tile_surface;

  requested_tile = *tile;
  queue_found = color_found = regenerate = mod_count_ok = queue_equal = color_equal = FALSE;

  if (tile == NULL)
    return FALSE;

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
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found && queue_equal)             /* Кэш2: неправильный тайл; кэш1: правильный. */
    {
      show_strategy = RECOLOR;
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else if (color_found)                            /* Кэш2: неправильный тайл; кэш1: неправильный. */
    {
      show_strategy = SHOW;
      regenerate = TRUE;
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found)                            /* Кэш2: ничего нет; кэш1: неправильный. */
    {
      show_strategy = RECOLOR;
      regenerate = TRUE;
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else                                             /* Кэш2: ничего; кэш1: ничего. Заглушка. */
    {
      show_strategy = SHOW_DUMMY;
      regenerate = TRUE;
    }

  /* Теперь можно запросить тайл откуда следует и отправить его куда следует. */

  if (!queue_found)
    {
      hyscan_tile_queue_add (priv->queue, &requested_tile);
    }
  else if (regenerate && priv->regen_allowed)
    {
      hyscan_tile_queue_add (priv->queue, &requested_tile);
      priv->regen_sent = TRUE;
    }

  if (surface == NULL)
    return FALSE;

  switch (show_strategy)
    {
    case SHOW:
      /* Просто отображаем тайл. */
      *tile = color_tile;
      return hyscan_tile_color_get (priv->color, &requested_tile, &color_tile, &tile_surface);

    case RECOLOR:
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

/* Функция создает новый виджет водопада. */
GtkWidget*
hyscan_gtk_waterfall_drawer_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_DRAWER, NULL);
}

/* Функция обновляет параметры HyScanTileQueue. */
void
hyscan_gtk_waterfall_drawer_set_upsample (HyScanGtkWaterfallDrawer *drawer,
                                          gint                      upsample)
{
  HyScanGtkWaterfallDrawerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);
  priv = drawer->priv;

  /* Настраивать HyScanTileQueue можно только когда галс не открыт. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_upsample (priv->queue, upsample);
}

gboolean
hyscan_gtk_waterfall_drawer_set_colormap (HyScanGtkWaterfallDrawer *drawer,
                                          guint32                  *colormap,
                                          gint                      length,
                                          guint32                   background)
{
  gboolean status = FALSE;
  HyScanGtkWaterfallDrawerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);
  priv = drawer->priv;

  status = hyscan_tile_color_set_colormap (priv->color, colormap, length, background);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return status;
}

/* Функция устанавливает уровни. */
gboolean
hyscan_gtk_waterfall_drawer_set_levels (HyScanGtkWaterfallDrawer *drawer,
                                        gdouble                   black,
                                        gdouble                   gamma,
                                        gdouble                   white)
{
  gboolean status = FALSE;
  HyScanGtkWaterfallDrawerPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);
  priv = drawer->priv;

  status = hyscan_tile_color_set_levels (priv->color, black, gamma, white);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return status;
}

/* Функция открывает БД, проект, галс... */
static void
hyscan_gtk_waterfall_drawer_open (HyScanGtkWaterfall *waterfall,
                                  HyScanDB           *db,
                                  const gchar        *project,
                                  const gchar        *track,
                                  gboolean            raw)
{
  HyScanCache *cache;
  HyScanCache *cache2;
  const gchar *cache_prefix;
  HyScanTileType tile_type;
  gfloat ship_speed;
  GArray *sound_velocity;
  HyScanSourceType depth_source;
  guint depth_channel;
  guint depth_size;
  gulong depth_time;
  gchar *db_uri;

  HyScanGtkWaterfallDrawerPrivate *priv;
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (waterfall);
  priv = drawer->priv;

  if (priv->open)
    return;

  db_uri = hyscan_db_get_uri (db);

  cache = hyscan_gtk_waterfall_get_cache (waterfall);
  cache2 = hyscan_gtk_waterfall_get_cache2 (waterfall);
  cache_prefix = hyscan_gtk_waterfall_get_cache_prefix (waterfall);

  //HyScanTileSource tile_source = hyscan_gtk_waterfall_get_tile_source (waterfall);
  tile_type = HYSCAN_TILE_SLANT;

  ship_speed = hyscan_gtk_waterfall_get_ship_speed (waterfall);
  sound_velocity = hyscan_gtk_waterfall_get_sound_velocity (waterfall);

  depth_source = hyscan_gtk_waterfall_get_depth_source (waterfall, &depth_channel);
  depth_size = hyscan_gtk_waterfall_get_depth_filter_size (waterfall);
  depth_time = hyscan_gtk_waterfall_get_depth_time (waterfall);

  priv->widget_type = hyscan_gtk_waterfall_get_sources (waterfall, &priv->left_source, &priv->right_source);

  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    tile_type = hyscan_gtk_waterfall_get_tile_type (waterfall);

  //if (parent_open != NULL)
  //  parent_open (waterfall, db, project, track, raw);

  if (cache == NULL || cache2 == NULL)
    return;

  hyscan_tile_color_set_cache (priv->color, cache2, cache_prefix);
  hyscan_tile_queue_set_cache (priv->queue, cache, cache_prefix);
  hyscan_track_rect_set_cache (priv->lrect, cache, cache_prefix);
  hyscan_track_rect_set_cache (priv->rrect, cache, cache_prefix);

  hyscan_tile_queue_set_ship_speed (priv->queue, ship_speed);
  hyscan_tile_queue_set_sound_velocity (priv->queue, sound_velocity);

  hyscan_track_rect_set_ship_speed (priv->lrect, ship_speed);
  hyscan_track_rect_set_ship_speed (priv->rrect, ship_speed);
  hyscan_track_rect_set_sound_velocity (priv->lrect, sound_velocity);
  hyscan_track_rect_set_sound_velocity (priv->rrect, sound_velocity);

  hyscan_tile_queue_set_depth_source (priv->queue, depth_source, depth_channel);
  hyscan_tile_queue_set_depth_filter_size (priv->queue, depth_size);
  hyscan_tile_queue_set_depth_time (priv->queue, depth_time);

  hyscan_track_rect_set_depth_source (priv->lrect, depth_source, depth_channel);
  hyscan_track_rect_set_depth_source (priv->rrect, depth_source, depth_channel);
  hyscan_track_rect_set_depth_filter_size (priv->lrect, depth_size);
  hyscan_track_rect_set_depth_filter_size (priv->rrect, depth_size);
  hyscan_track_rect_set_depth_time (priv->lrect, depth_time);
  hyscan_track_rect_set_depth_time (priv->rrect, depth_time);

  hyscan_tile_queue_set_type (priv->queue, tile_type);

  hyscan_tile_queue_set_source_func (priv->queue, hyscan_gtk_waterfall_drawer_queue_source);
  hyscan_tile_queue_set_rotate_func (priv->queue, hyscan_gtk_waterfall_drawer_queue_rotate);
  hyscan_tile_queue_set_data (priv->queue, priv);

  hyscan_tile_color_open (priv->color, db_uri, project, track);
  hyscan_tile_queue_open (priv->queue, db, project, track, raw);
  hyscan_track_rect_open (priv->lrect, db, project, track, priv->left_source, raw);
  hyscan_track_rect_open (priv->rrect, db, project, track, priv->right_source, raw);

  priv->open = TRUE;

  priv->length = priv->lwidth = priv->rwidth = 0.0;

  priv->ship_speed = ship_speed;
  priv->once = FALSE;
  priv->automove = TRUE;
  priv->prev_time = g_get_monotonic_time ();
  priv->auto_tag = g_timeout_add (priv->automove_time,
                                  hyscan_gtk_waterfall_drawer_automover,
                                  drawer);

  g_free (db_uri);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));
}

/* Функция закрывает БД, проект, галс... */
static void
hyscan_gtk_waterfall_drawer_close (HyScanGtkWaterfall *waterfall)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (waterfall);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  //if (parent_close != NULL)
  //  parent_close (waterfall);

  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = 0;
    }

  hyscan_tile_color_close (priv->color);
  hyscan_tile_queue_close (priv->queue);
  hyscan_track_rect_close (priv->lrect);
  hyscan_track_rect_close (priv->rrect);

  priv->open = FALSE;

}

/* Функция возвращает текущий масштаб. */
gint
hyscan_gtk_waterfall_drawer_get_scale (HyScanGtkWaterfallDrawer *drawer,
                                       const gdouble           **zooms,
                                       gint                      *num)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), -1);

  if (zooms != NULL)
    *zooms = zooms_gost;
  if (num != NULL)
    *num = ZOOM_LEVELS;

  return drawer->priv->zoom_index;
}

static void
hyscan_gtk_waterfall_drawer_cifroarea_check_scale (GtkCifroArea *carea,
                                                   gdouble      *scale_x,
                                                   gdouble      *scale_y)
{
  gint i = 0, optimal = 0;
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (carea);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;
  gdouble *zooms = priv->zooms;
  gdouble requested = MAX (*scale_x, *scale_y);

  while (i < ZOOM_LEVELS && zooms[i] - requested > EPS)
    optimal = i++;

  priv->zoom_index = optimal;
  *scale_x = *scale_y = zooms[optimal];
}

static void
hyscan_gtk_waterfall_drawer_zoom_internal (GtkCifroArea          *carea,
                                           GtkCifroAreaZoomType   direction_x,
                                           GtkCifroAreaZoomType   direction_y,
                                           gdouble                center_x,
                                           gdouble                center_y)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (carea);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  if (priv->zoom_index == 0 && direction_x == GTK_CIFRO_AREA_ZOOM_OUT)
    return;
  if (priv->zoom_index == ZOOM_LEVELS - 1 && direction_x == GTK_CIFRO_AREA_ZOOM_IN)
    return;

  if (direction_x == GTK_CIFRO_AREA_ZOOM_IN)
    priv->zoom_index++;
  if (direction_x == GTK_CIFRO_AREA_ZOOM_OUT)
    priv->zoom_index--;

  gtk_cifro_area_set_scale (carea,
                            priv->zooms[priv->zoom_index],
                            priv->zooms[priv->zoom_index],
                            center_x, center_y);

  g_signal_emit (drawer, hyscan_gtk_waterfall_drawer_signals[SIGNAL_ZOOM], 0, priv->zoom_index, zooms_gost[priv->zoom_index]);

  if (priv->automove)
    gtk_cifro_area_move (carea, 0, G_MAXINT);
}

static void
hyscan_gtk_waterfall_drawer_cifroarea_get_limits (GtkCifroArea *carea,
                                                  gdouble      *min_x,
                                                  gdouble      *max_x,
                                                  gdouble      *min_y,
                                                  gdouble      *max_y)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (carea);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  if (G_LIKELY (priv->init))
    {
      if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
        {
          *min_x = -priv->lwidth;
          *max_x = priv->rwidth;
          *min_y = 0;
          *max_y = priv->length;
        }
      if (priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER)
        {
          *min_x = 0;
          *max_x = priv->length;
          *min_y = -priv->rwidth;
          *max_y = 0;
        }
    }
  else
    {
      *min_x = *max_x = *min_y = *max_y = 0;
    }
}

static void
hyscan_gtk_waterfall_drawer_cifroarea_get_stick (GtkCifroArea          *carea,
                                                 GtkCifroAreaStickType *stick_x,
                                                 GtkCifroAreaStickType *stick_y)
{
  HyScanGtkWaterfallDrawerPrivate *priv;
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (carea);
  priv = drawer->priv;

  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    {
      if (priv->init && priv->writeable)
        *stick_y = GTK_CIFRO_AREA_STICK_TOP;
      else
        *stick_y = GTK_CIFRO_AREA_STICK_CENTER;

      *stick_x = GTK_CIFRO_AREA_STICK_CENTER;
    }

  else /*if (priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER)*/
    {
      if (priv->init && priv->writeable)
        *stick_x = GTK_CIFRO_AREA_STICK_RIGHT;
      else
        *stick_x = GTK_CIFRO_AREA_STICK_CENTER;

      *stick_y = GTK_CIFRO_AREA_STICK_TOP;
    }
}

static gboolean
hyscan_gtk_waterfall_drawer_automover (gpointer data)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (data);
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (data);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  gdouble lwidth, rwidth;
  gdouble l_length, r_length, length;
  gboolean l_writeable = FALSE, r_writeable = FALSE;
  gboolean writeable;
  gboolean l_init = FALSE, r_init = FALSE;

  if (!priv->open)
    return G_SOURCE_CONTINUE;

  /* Параметры галса. */
  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    {
      l_init = hyscan_track_rect_get (priv->lrect, &l_writeable, &lwidth, &l_length);
      r_init = hyscan_track_rect_get (priv->rrect, &r_writeable, &rwidth, &r_length);
    }
  else /*if (priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER)*/
    {
      r_init = hyscan_track_rect_get (priv->rrect, &r_writeable, &rwidth, &r_length);
      l_init = TRUE;
      l_writeable = r_writeable;
      l_length = r_length;
      lwidth = 0.0;
    }

  priv->init = l_init && r_init;
  writeable = l_writeable || r_writeable;

  /* Если данных ещё нет, выходим. */
  if (!priv->init)
    return G_SOURCE_CONTINUE;

  length = MIN (l_length, r_length);
  priv->lwidth = lwidth;
  priv->rwidth = rwidth;
  priv->writeable = writeable;

  if (G_UNLIKELY (!priv->once))
    {
      priv->length = length;
      gtk_cifro_area_set_view (carea, -lwidth, rwidth, 0, length);
      priv->once = TRUE;
    }

  /* Если данные уже есть, но галс в режиме чтения. */
  if (!writeable)
    {
      priv->length = length;
      priv->automove = FALSE;
      priv->auto_tag = 0;
      gtk_widget_queue_draw (GTK_WIDGET(data));
      return G_SOURCE_REMOVE;
    }

  if (/*priv->view_finalised &&*/ priv->automove)
    {
      gdouble regen_gap;
      gdouble x0, y0, x1, y1;
      gint64 time = g_get_monotonic_time ();
      gdouble distance = (time - priv->prev_time) / 1e6;
      distance *= priv->ship_speed;

      regen_gap = priv->regen_period / 1000000.0 * priv->ship_speed;

      gtk_cifro_area_get_view (carea, &x0, &x1, &y0, &y1);

      switch (priv->widget_type)
        {
        case HYSCAN_GTK_WATERFALL_SIDESCAN:
          if (y1 + distance < length - priv->ship_speed - regen_gap)
            {
              y0 += distance;
              y1 += distance;
              priv->length = y1;
            }
          break;
        case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
          if (x1 + distance < length - priv->ship_speed - regen_gap)
            {
              x0 += distance;
              x1 += distance;
              priv->length = x1;
            }
          break;
        }

      gtk_cifro_area_set_view (carea, x0, x1, y0, y1);

      priv->prev_time = time;

      gtk_widget_queue_draw (GTK_WIDGET(data));
    }
  else
    {
      priv->length = length;
    }

  gtk_widget_queue_draw (GTK_WIDGET (drawer));
  return G_SOURCE_CONTINUE;
}

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_waterfall_drawer_button_press (GtkWidget      *widget,
                                          GdkEventButton *event,
                                          gpointer        data)
{
  HyScanGtkWaterfallDrawer *drawer = data;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      drawer->priv->mouse_y = event->y;
      drawer->priv->mouse_x = event->x;
    }
  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_drawer_motion (GtkWidget      *widget,
                                    GdkEventMotion *event,
                                    gpointer        data)
{
  HyScanGtkWaterfallDrawer *drawer = data;
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  gdouble stored, received;
  guint widget_size;

  if (!(event->state & GDK_BUTTON1_MASK))
    return FALSE;

  switch (priv->widget_type)
    {
    case HYSCAN_GTK_WATERFALL_SIDESCAN:
      stored = priv->mouse_y;
      received = event->y;
      widget_size = priv->widget_height;
      break;
    case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
    default:
      received = priv->mouse_x;
      stored = event->x;
      widget_size = priv->widget_width;
    }

   if (received < stored - widget_size / 10.0 && priv->automove)
      {
        priv->automove = FALSE;
        g_signal_emit (drawer, hyscan_gtk_waterfall_drawer_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
      }

  return FALSE;
}

void
hyscan_gtk_waterfall_drawer_zoom (HyScanGtkWaterfallDrawer *drawer,
                                  gboolean                  zoom_in)
{
  GtkCifroArea *carea;
  GtkCifroAreaZoomType direction;
  gdouble from_x, to_x, from_y, to_y;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));
  carea = GTK_CIFRO_AREA (drawer);

  direction = zoom_in ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  gtk_cifro_area_zoom (carea, direction, direction, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
}


void
hyscan_gtk_waterfall_drawer_automove (HyScanGtkWaterfallDrawer *drawer,
                                      gboolean                  automove)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->automove = automove;
}

void
hyscan_gtk_waterfall_drawer_set_automove_period (HyScanGtkWaterfallDrawer *drawer,
                                                 gint64                    usecs)
{
  HyScanGtkWaterfallDrawerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));
  priv = drawer->priv;

  priv->automove_time = usecs;
  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = g_timeout_add (priv->automove_time,
                                      hyscan_gtk_waterfall_drawer_automover,
                                      drawer);
    }

}

void
hyscan_gtk_waterfall_drawer_set_regeneration_period (HyScanGtkWaterfallDrawer *drawer,
                                                     gint64                    usecs)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->regen_period = usecs;
}
