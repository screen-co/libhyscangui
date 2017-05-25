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
#include "hyscan-gtk-waterfall-private.h"
#include <hyscan-gui-marshallers.h>
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
#include <hyscan-track-rect.h>
#include <math.h>

#define ZOOM_LEVELS 15
#define EPS -0.000001
#define EPS2 0.000001

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

  HyScanTileQueue       *queue;
  HyScanTileColor       *color;

  guint64                view_id;

  gdouble               *zooms;              /**< Масштабы с учетом PPI. */
  gint                   zoom_index;

  gboolean               open;               /**< Флаг "галс открыт". */
  gboolean               request_redraw;     /**< Флаг "требуется перерисовка". */
  guint                  redraw_tag;

  gboolean               view_finalised;     /**< "Все тайлы для этого вью найдены и показаны". */

  cairo_surface_t       *dummy;              /**< Поверхность заглушки. */
  guint32                dummy_color;        /**< Цвет подложки. */

  guint                  tile_upsample;      /**< Величина передискретизации. */
  HyScanTileType         tile_type;          /**< Тип отображения. */

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
  gboolean               regen_allowed;      /**< Интервал между перегенерациями. */
};

/* Внутренние методы класса. */
static void     hyscan_gtk_waterfall_drawer_object_constructed      (GObject                       *object);
static void     hyscan_gtk_waterfall_drawer_object_finalize         (GObject                       *object);

static gboolean hyscan_gtk_waterfall_drawer_redrawer                (gpointer                       data);
static void     hyscan_gtk_waterfall_drawer_request_redraw          (HyScanGtkWaterfallDrawer      *drawer);

static gint32   hyscan_gtk_waterfall_drawer_aligner                 (gdouble                        in,
                                                                     gint                           size);
static void     hyscan_gtk_waterfall_drawer_prepare_csurface        (cairo_surface_t              **surface,
                                                                     gint                           required_width,
                                                                     gint                           required_height,
                                                                     HyScanTileSurface             *tile_surface);
static void     hyscan_gtk_waterfall_drawer_prepare_tile            (HyScanGtkWaterfallDrawerPrivate *priv,
                                                                     HyScanTile                      *tile);
static gboolean hyscan_gtk_waterfall_drawer_get_tile                (HyScanGtkWaterfallDrawer      *drawer,
                                                                     HyScanTile                    *tile,
                                                                     cairo_surface_t              **tile_surface);
static void     hyscan_gtk_waterfall_drawer_image_generated         (HyScanGtkWaterfallDrawer      *drawer,
                                                                     HyScanTile                    *tile,
                                                                     gfloat                        *img,
                                                                     gint                           size);

static void     hyscan_gtk_waterfall_drawer_visible_draw            (GtkWidget                     *widget,
                                                                     cairo_t                       *cairo);
static void     hyscan_gtk_waterfall_drawer_dummy_draw              (GtkWidget                     *widget,
                                                                     cairo_t                       *cairo);

static void     hyscan_gtk_waterfall_drawer_draw_surface            (cairo_surface_t               *src,
                                                                     cairo_t                       *dst,
                                                                     gdouble                        x,
                                                                     gdouble                        y);
static void     hyscan_gtk_waterfall_drawer_create_dummy            (HyScanGtkWaterfallDrawer      *drawer);
static gboolean hyscan_gtk_waterfall_drawer_configure               (GtkWidget                     *widget,
                                                                     GdkEventConfigure             *event);

static void     hyscan_gtk_waterfall_drawer_open                    (HyScanGtkWaterfall            *waterfall,
                                                                     HyScanDB                      *db,
                                                                     const gchar                   *project,
                                                                     const gchar                   *track,
                                                                     gboolean                       raw);
static void     hyscan_gtk_waterfall_drawer_close                   (HyScanGtkWaterfall            *waterfall);

static void     hyscan_gtk_waterfall_drawer_zoom_internal           (GtkCifroArea                  *carea,
                                                                     GtkCifroAreaZoomType           direction_x,
                                                                     GtkCifroAreaZoomType           direction_y,
                                                                     gdouble                        center_x,
                                                                     gdouble                        center_y);
static void     hyscan_gtk_waterfall_drawer_cifroarea_check_scale   (GtkCifroArea                  *carea,
                                                                     gdouble                       *scale_x,
                                                                     gdouble                       *scale_y);
static void     hyscan_gtk_waterfall_drawer_cifroarea_get_limits    (GtkCifroArea                  *carea,
                                                                     gdouble                       *min_x,
                                                                     gdouble                       *max_x,
                                                                     gdouble                       *min_y,
                                                                     gdouble                       *max_y);
static void     hyscan_gtk_waterfall_drawer_cifroarea_get_stick     (GtkCifroArea                  *carea,
                                                                     GtkCifroAreaStickType         *stick_x,
                                                                     GtkCifroAreaStickType         *stick_y);

static gboolean hyscan_gtk_waterfall_drawer_automover               (gpointer                       data);

static gboolean hyscan_gtk_waterfall_drawer_button_press            (GtkWidget                     *widget,
                                                                     GdkEventButton                *event,
                                                                     gpointer                       data);
static gboolean hyscan_gtk_waterfall_drawer_motion                  (GtkWidget                     *widget,
                                                                     GdkEventMotion                *event,
                                                                     gpointer                       data);
static gboolean hyscan_gtk_waterfall_drawer_keyboard                (GtkWidget                     *widget,
                                                                     GdkEventKey                   *event);
static gboolean hyscan_gtk_waterfall_drawer_scroll                  (GtkWidget                     *widget,
                                                                     GdkEventScroll                *event);


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

static void
hyscan_gtk_waterfall_drawer_object_constructed (GObject *object)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (object);
  HyScanGtkWaterfallDrawerPrivate *priv;
  guint n_threads = g_get_num_processors ();

  G_OBJECT_CLASS (hyscan_gtk_waterfall_drawer_parent_class)->constructed (object);
  priv = drawer->priv;

  priv->queue = hyscan_tile_queue_new (n_threads);
  priv->color = hyscan_tile_color_new ();
  priv->lrect = hyscan_track_rect_new ();
  priv->rrect = hyscan_track_rect_new ();

  /* Параметры GtkCifroArea по умолчанию. */
  gtk_cifro_area_set_scale_on_resize (GTK_CIFRO_AREA (drawer), FALSE);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (drawer), -50, 50, 0, 100 );

  g_signal_connect_swapped (priv->queue, "tile-queue-ready", G_CALLBACK (hyscan_gtk_waterfall_drawer_request_redraw), drawer);
  g_signal_connect_swapped (priv->queue, "tile-queue-image", G_CALLBACK (hyscan_gtk_waterfall_drawer_image_generated), drawer);

  g_signal_connect (drawer, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_drawer_visible_draw), NULL);
  g_signal_connect_after (drawer, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_configure), NULL);

  g_signal_connect (drawer, "button-press-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_button_press), drawer);
  g_signal_connect (drawer, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_motion), drawer);
  g_signal_connect (drawer, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_keyboard), drawer);
  g_signal_connect (drawer, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_drawer_scroll), drawer);

  /* Инициализируем масштабы. */

  priv->zooms = g_malloc0 (ZOOM_LEVELS * sizeof (gdouble));
  priv->view_finalised = 0;
  priv->automove_time = 40;
  priv->regen_period = 1 * G_TIME_SPAN_SECOND;

  priv->tile_upsample = 2;

  priv->redraw_tag = g_timeout_add (1, (hyscan_gtk_waterfall_drawer_redrawer), GTK_WIDGET (drawer));
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
  if (priv->redraw_tag != 0)
    {
      g_source_remove (priv->redraw_tag);
      priv->redraw_tag = 0;
    }

  G_OBJECT_CLASS (hyscan_gtk_waterfall_drawer_parent_class)->finalize (object);
}

/* Функция запрашивает перерисовку виджета, если необходимо. */
static gboolean
hyscan_gtk_waterfall_drawer_redrawer (gpointer data)
{
  GtkWidget *widget = data;
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);

  if (g_atomic_int_get (&drawer->priv->request_redraw))
    gtk_widget_queue_draw (widget);

  g_atomic_int_set (&drawer->priv->request_redraw, FALSE);

  return G_SOURCE_CONTINUE;
}

static void
hyscan_gtk_waterfall_drawer_request_redraw (HyScanGtkWaterfallDrawer *drawer)
{
  g_atomic_int_set (&drawer->priv->request_redraw, TRUE);
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

  if (G_LIKELY (*surface != NULL))
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

  if (tile_surface != NULL)
    {
      tile_surface->width = cairo_image_surface_get_width (*surface);
      tile_surface->height = cairo_image_surface_get_height (*surface);
      tile_surface->stride = cairo_image_surface_get_stride (*surface);
      tile_surface->data = cairo_image_surface_get_data (*surface);
    }
}

static void
hyscan_gtk_waterfall_drawer_prepare_tile (HyScanGtkWaterfallDrawerPrivate *priv,
                                          HyScanTile                      *tile)
{
  /* Тип КД и поворот для этого тайла. */
  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    {
      if (tile->across_start <= 0 && tile->across_end <= 0)
        tile->source = priv->left_source;
      else
        tile->source = priv->right_source;

      tile->rotate = FALSE;
    }
  else /* HYSCAN_GTK_WATERFALL_ECHOSOUNDER */
    {
      tile->source = priv->right_source;

      tile->rotate = TRUE;
    }

  tile->type = priv->tile_type;
  tile->upsample = priv->tile_upsample;
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
  gboolean   regenerate;
  gint       show_strategy = SHOW_DUMMY;
  gfloat    *buffer        = NULL;
  guint32    buffer_size   = 0;
  HyScanTileSurface tile_surface;

  requested_tile = *tile;
  queue_found = color_found = regenerate = FALSE;

  /* Дописываем в структуру с тайлом всю необходимую информацию (из очереди). */
  hyscan_gtk_waterfall_drawer_prepare_tile (priv, &requested_tile);

  /* Собираем информацию об имеющихся тайлах и их тождественности. */
  queue_found = hyscan_tile_queue_check (priv->queue, &requested_tile, &queue_tile, &regenerate);
  color_found = hyscan_tile_color_check (priv->color, &requested_tile, &color_tile);

  /* На основании этого определяем, как поступить. */
  if (color_found)
    {
      show_strategy = SHOW;
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found)
    {
      show_strategy = RECOLOR;
      hyscan_gtk_waterfall_drawer_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else
    {
      show_strategy = SHOW_DUMMY;
      regenerate = TRUE;
    }

  /* Ненайденные тайлы сразу же отправляем на генерацию. */
  if (!queue_found)
    {
      hyscan_tile_queue_add (priv->queue, &requested_tile);
    }
  /* ПЕРЕгенерация разрешена не всегда. */
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
      return TRUE;

    case SHOW_DUMMY:
    default:
      return FALSE;
    }

  return TRUE;
}

/* Функция асинхронно разукрашивает тайл. */
static void
hyscan_gtk_waterfall_drawer_image_generated (HyScanGtkWaterfallDrawer *drawer,
                                             HyScanTile               *tile,
                                             gfloat                   *img,
                                             gint                      size)
{
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;
  HyScanTileSurface surface;
  surface.width = tile->w;
  surface.height = tile->h;
  surface.stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, surface.width);
  surface.data = g_malloc0 (surface.height * surface.stride);

  hyscan_tile_color_add (priv->color, tile, img, size, &surface);

  g_free (surface.data);
  hyscan_gtk_waterfall_drawer_request_redraw (drawer);
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_drawer_visible_draw (GtkWidget *widget,
                                          cairo_t   *cairo)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  cairo_surface_t *source_surface;
  HyScanTile tile;
  gint32 tile_size      = 0;
  gint32 start_tile_x0  = 0;
  gint32 start_tile_y0  = 0;
  gint32 end_tile_x0    = 0;
  gint32 end_tile_y0    = 0;

  gint num_of_tiles_x = 0;
  gint num_of_tiles_y = 0;
  gint i, j;

  gdouble from_x, from_y, to_x, to_y,
          x_coord, y_coord, x_coord0, y_coord0,
          scale;
  gboolean view_finalised = TRUE;

  /* Если галс не открыт, отрисовываем заглушку на весь виджет. */
  if (!priv->open)
    {
      hyscan_gtk_waterfall_drawer_dummy_draw (widget, cairo);
      return;
    }

  gtk_cifro_area_get_view	(GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);

  from_x *= 1000.0;
  to_x   *= 1000.0;
  from_y *= 1000.0;
  to_y   *= 1000.0;

  /* Определяем размер тайла. */
  scale = zooms_gost[priv->zoom_index];
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

          /* Ищем тайл. */
          if (priv->open && hyscan_gtk_waterfall_drawer_get_tile (drawer, &tile, &(priv->surface)))
            {
              x_coord = x_coord0 + j * tile.w;
              y_coord = y_coord0 - i * tile.h;

              source_surface = priv->surface;
            }
          /* Если не нашли, отрисовываем заглушку. */
          else
            {

              gfloat step = hyscan_tile_common_mm_per_pixel (zooms_gost[0], priv->ppi);
              gint tile_pixels = hyscan_tile_common_tile_size (0, tile_sizes[0], step);

              x_coord = x_coord0 + j * tile_pixels;
              y_coord = y_coord0 - i * tile_pixels;

              view_finalised = FALSE;

              source_surface = priv->dummy;
            }

          /* Отрисовка. */
          if (source_surface != NULL)
            hyscan_gtk_waterfall_drawer_draw_surface (source_surface, cairo, x_coord, y_coord);
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

/* Функция отрисовывает заглушку на весь виджет. */
static void
hyscan_gtk_waterfall_drawer_dummy_draw (GtkWidget *widget,
                                        cairo_t   *cairo)
{
  gint32 tile_size, start_x, start_y, end_x, end_y;
  gint num_of_tiles_x, num_of_tiles_y, i, j;
  gdouble from_x, from_y, to_x, to_y,
          x_coord, y_coord, x_coord0,
          y_coord0;
  gfloat step;
  gint tile_pixels;

  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
  from_x *= 1000.0;
  to_x   *= 1000.0;
  from_y *= 1000.0;
  to_y   *= 1000.0;

  /* Определяем размер тайла. */
  tile_size = tile_sizes[priv->zoom_index];
  step = hyscan_tile_common_mm_per_pixel (zooms_gost[0], priv->ppi);
  tile_pixels = hyscan_tile_common_tile_size (0, tile_sizes[0], step);

  /* Определяем параметры искомых тайлов. */
  start_x = hyscan_gtk_waterfall_drawer_aligner (from_x, tile_size);
  start_y = hyscan_gtk_waterfall_drawer_aligner (from_y, tile_size);
  end_x   = hyscan_gtk_waterfall_drawer_aligner (to_x,   tile_size);
  end_y   = hyscan_gtk_waterfall_drawer_aligner (to_y,   tile_size);
  num_of_tiles_x = (end_x - start_x) / tile_size + 1;
  num_of_tiles_y = (end_y - start_y) / tile_size + 1;

  gtk_cifro_area_visible_value_to_point	(GTK_CIFRO_AREA (widget), &x_coord0, &y_coord0,
                                        (gdouble)(start_x / 1000.0),
                                        (gdouble)((start_y + tile_size) / 1000.0));
  x_coord0 = round(x_coord0);
  y_coord0 = round(y_coord0);

  for (j = 0; j < num_of_tiles_x; j++)
    for (i = 0; i < num_of_tiles_y; i++)
      {
        x_coord = x_coord0 + j * tile_pixels;
        y_coord = y_coord0 - i * tile_pixels;

        hyscan_gtk_waterfall_drawer_draw_surface (priv->dummy, cairo, x_coord, y_coord);
      }
}

static void
hyscan_gtk_waterfall_drawer_draw_surface (cairo_surface_t *src,
                                          cairo_t         *dst,
                                          gdouble          x,
                                          gdouble          y)
{
  cairo_surface_mark_dirty (src);
  cairo_save (dst);
  cairo_translate (dst, x, y);
  cairo_set_source_surface (dst, src, 0, 0);
  cairo_paint (dst);
  cairo_restore (dst);
}

static void
hyscan_gtk_waterfall_drawer_create_dummy (HyScanGtkWaterfallDrawer *drawer)
{
  gfloat step;
  gint size, stride, i, j;
  guchar *data;

  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;

  /* Уничтожаем старую заглушку. */
  cairo_surface_destroy (priv->dummy);

  /* Определяем размеры новой. */
  step = hyscan_tile_common_mm_per_pixel (zooms_gost[0], priv->ppi);
  size = hyscan_tile_common_tile_size (0, tile_sizes[0], step);

  /* Пересоздаем заглушку. */
  priv->dummy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);

  /* Заполняем её цветом. */
  data = cairo_image_surface_get_data (priv->dummy);
  stride = cairo_image_surface_get_stride (priv->dummy);

  for (i = 0; i < size; i++)
    for (j = 0; j < size; j++)
      *((guint32*)(data + i * stride + j * sizeof (guint32))) = priv->dummy_color;
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
  gfloat old_ppi, ppi, diagonal_mm, diagonal_pix;

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

  /* Запомним старый PPI, чтобы определить, надо ли его менять. */
  old_ppi = priv->ppi;

  /* Вычисляем PPI. */
  ppi = diagonal_pix / diagonal_mm;
  if (isnan (ppi) || isinf(ppi) || ppi <= 0.0 || monitor_h <= 0 || monitor_w <= 0)
    ppi = 96.0;
  priv->ppi = ppi;

  /* Если PPI не изменился, то ничего менять не надо. */
  if (ppi == old_ppi)
    goto exit;

  /* Иначе обновляем масштабы. */
  for (i = 0; i < ZOOM_LEVELS; i++)
    priv->zooms[i] = zooms_gost[i] / (1000.0 / (25.4 / priv->ppi));

  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (widget), priv->zooms[priv->zoom_index], priv->zooms[priv->zoom_index], 0, 0);

  /* И пересоздаем заглушку. */
  hyscan_gtk_waterfall_drawer_create_dummy (drawer);

exit:

  gtk_cifro_area_get_size (GTK_CIFRO_AREA (widget), &priv->widget_width, &priv->widget_height);

  gtk_widget_queue_draw (widget);

  return FALSE;
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

  ship_speed = hyscan_gtk_waterfall_get_ship_speed (waterfall);
  sound_velocity = hyscan_gtk_waterfall_get_sound_velocity (waterfall);

  depth_source = hyscan_gtk_waterfall_get_depth_source (waterfall, &depth_channel);
  depth_size = hyscan_gtk_waterfall_get_depth_filter_size (waterfall);
  depth_time = hyscan_gtk_waterfall_get_depth_time (waterfall);

  priv->widget_type = hyscan_gtk_waterfall_get_sources (waterfall, &priv->left_source, &priv->right_source);

  tile_type = HYSCAN_TILE_SLANT;
  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    tile_type = hyscan_gtk_waterfall_get_tile_type (waterfall);

  priv->tile_type = tile_type;

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
hyscan_gtk_waterfall_drawer_cifroarea_check_scale (GtkCifroArea *carea,
                                                   gdouble      *scale_x,
                                                   gdouble      *scale_y)
{
  gint i = 0, optimal = 0;
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (carea);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;
  gdouble *zooms = priv->zooms;
  gdouble requested, current;

  /* Сначала проверим, а не равны ли заправшиваемый и текущий масштаб. */
  current = zooms[priv->zoom_index];
  if (fabs (*scale_x - current) < EPS2 && fabs (*scale_y - current) < EPS2)
    return;

  /* Ну раз так, то надо подобрать новый масштаб. */
  requested = MAX (*scale_x, *scale_y);
  while (i < ZOOM_LEVELS && zooms[i] - requested > EPS)
    optimal = i++;

  priv->zoom_index = optimal;
  *scale_x = *scale_y = zooms[optimal];

  /* Сигнал на смену зума. */
  g_signal_emit (drawer, hyscan_gtk_waterfall_drawer_signals[SIGNAL_ZOOM], 0, priv->zoom_index, zooms_gost[priv->zoom_index]);
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
  gdouble l_length = 0.0, r_length = 0.0, length = 0.0;
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

  priv->init = l_init || r_init;
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
              priv->length = y0 < 0 ? y1 : length;
            }
          break;
        case HYSCAN_GTK_WATERFALL_ECHOSOUNDER:
          if (x1 + distance < length - priv->ship_speed - regen_gap)
            {
              x0 += distance;
              x1 += distance;
              priv->length = x0 < 0 ? x1 : length;
            }
          break;
        }

      gtk_cifro_area_set_view (carea, x0, x1, y0, y1);

      priv->prev_time = time;
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

static gboolean
hyscan_gtk_waterfall_drawer_keyboard (GtkWidget   *widget,
                                      GdkEventKey *event)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;


  if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN &&
      (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Page_Down || event->keyval == GDK_KEY_Home))
    {
      priv->automove = FALSE;
    }

  if (priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER &&
      (event->keyval == GDK_KEY_Left || event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_Home))
    {
      priv->automove = FALSE;
    }

  g_signal_emit (drawer, hyscan_gtk_waterfall_drawer_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_drawer_scroll (GtkWidget      *widget,
                                    GdkEventScroll *event)
{
  HyScanGtkWaterfallDrawer *drawer = HYSCAN_GTK_WATERFALL_DRAWER (widget);
  HyScanGtkWaterfallDrawerPrivate *priv = drawer->priv;
  gboolean scroll_only;
  gboolean is_scroll;
  gboolean scroll_to_start = FALSE;

  scroll_only = hyscan_gtk_waterfall_get_wheel_behaviour (HYSCAN_GTK_WATERFALL (drawer));
  is_scroll = scroll_only == !(event->state & GDK_CONTROL_MASK);

  if (!is_scroll)
    return FALSE;

  if ((priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN) && (event->direction == GDK_SCROLL_DOWN))
    scroll_to_start = TRUE;

  if ((priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER) && (event->direction == GDK_SCROLL_UP))
    scroll_to_start = TRUE;

  if (scroll_to_start)
    {
      priv->automove = FALSE;
      g_signal_emit (drawer, hyscan_gtk_waterfall_drawer_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
    }

  return FALSE;
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
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->tile_upsample = upsample;
}

/* Функция устанавливает цветовую схему. */
gboolean
hyscan_gtk_waterfall_drawer_set_colormap (HyScanGtkWaterfallDrawer *drawer,
                                          HyScanSourceType          source,
                                          guint32                  *colormap,
                                          guint                     length,
                                          guint32                   background)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return hyscan_tile_color_set_colormap (drawer->priv->color, source, colormap, length, background);
}

/* Функция устанавливает резервную цветовую схему. */
gboolean
hyscan_gtk_waterfall_drawer_set_colormap_for_all (HyScanGtkWaterfallDrawer *drawer,
                                                  guint32                  *colormap,
                                                  guint                     length,
                                                  guint32                   background)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return hyscan_tile_color_set_colormap_for_all (drawer->priv->color, colormap, length, background);
}

/* Функция устанавливает уровни. */
gboolean
hyscan_gtk_waterfall_drawer_set_levels (HyScanGtkWaterfallDrawer *drawer,
                                        HyScanSourceType          source,
                                        gdouble                   black,
                                        gdouble                   gamma,
                                        gdouble                   white)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return hyscan_tile_color_set_levels (drawer->priv->color, source, black, gamma, white);
}

/* Функция устанавливает резезрвные уровни. */
gboolean
hyscan_gtk_waterfall_drawer_set_levels_for_all (HyScanGtkWaterfallDrawer *drawer,
                                                gdouble                   black,
                                                gdouble                   gamma,
                                                gdouble                   white)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));

  return hyscan_tile_color_set_levels_for_all (drawer->priv->color, black, gamma, white);
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

void
hyscan_gtk_waterfall_drawer_automove (HyScanGtkWaterfallDrawer *drawer,
                                      gboolean                  automove)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->automove = automove;

  if (drawer->priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
    gtk_cifro_area_move (GTK_CIFRO_AREA (drawer), 0, G_MAXINT);
  else /*if (drawer->priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER) */
  gtk_cifro_area_move (GTK_CIFRO_AREA (drawer), G_MAXINT, 0);

}

void
hyscan_gtk_waterfall_drawer_set_automove_period (HyScanGtkWaterfallDrawer *drawer,
                                                 gint64                    usecs)
{
  HyScanGtkWaterfallDrawerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));
  priv = drawer->priv;

  priv->automove_time = usecs / G_TIME_SPAN_MILLISECOND;

  if (!priv->open)
    return;

  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = 0;
    }

  priv->auto_tag = g_timeout_add (priv->automove_time,
                                  hyscan_gtk_waterfall_drawer_automover,
                                  drawer);
}

void
hyscan_gtk_waterfall_drawer_set_regeneration_period (HyScanGtkWaterfallDrawer *drawer,
                                                     gint64                    usecs)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->regen_period = usecs;
}

void
hyscan_gtk_waterfall_drawer_set_substrate (HyScanGtkWaterfallDrawer *drawer,
                                           guint32                   substrate)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DRAWER (drawer));

  drawer->priv->dummy_color = substrate;
  hyscan_gtk_waterfall_drawer_create_dummy (drawer);

  gtk_widget_queue_draw (GTK_WIDGET (drawer));
}
