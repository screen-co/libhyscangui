/*
 * \file hyscan-gtk-waterfall.c
 *
 * \brief Исходный файл виджета водопад.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include "hyscan-gtk-waterfall.h"
#include <hyscan-gui-marshallers.h>
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
//#include <hyscan-trackrect.h>
#include <math.h>

#define ZOOM_LEVELS 15
#define EPS 2

enum
{
  SHOW                  = 100,
  RECOLOR_AND_SHOW      = 200,
  SHOW_DUMMY            = 500
};
enum
{
  SIGNAL_AUTOMOVE_STATE,
  SIGNAL_ZOOM,
  SIGNAL_LAST
};

static const gint tile_sizes[ZOOM_LEVELS] =    {500e3,  200e3,  100e3,  80e3,  50e3,  40e3,  20e3,  10e3,  5e3,  2e3,  1e3,  500, 400, 200, 100};   /**< в миллиметрах*/
static const gdouble zooms_gost[ZOOM_LEVELS] = {5000.0, 2000.0, 1000.0, 800.0, 500.0, 400.0, 200.0, 100.0, 50.0, 20.0, 10.0, 5.0, 4.0, 2.0, 1.0};

struct _HyScanGtkWaterfallPrivate
{
  gfloat                ppi;                /**< PPI. */
  gint                  need_to_redraw;     /**< Флаг необходимости перерисовки. */
  cairo_surface_t      *surface;            /**< Поверхность тайла. */
  cairo_surface_t      *dummy;              /**< Поверхность заглушки. */

  HyScanTileQueue      *queue;
  HyScanTileColor      *color;
  // HyScanTrackRect      *rect;

  gboolean             cache_set;
  gboolean             open;
  guint64              view_id;

  gdouble              *zooms;              /**< Масштабы с учетом PPI. */
  gboolean              opened;             /**< Флаг "галс открыт". */

  gdouble               ship_speed;         /**< Скорость судна. */
  gint64                prev_time;          /**< Предыдущее время вызова функции сдвижки. */

  guint                 refresh_time;       /**< Период опроса новых тайлов. */
  guint                 refresh_tag;        /**< Идентификатор функции перерисовки. */

  HyScanTileType        trackrect_type;     /**< Тип тайла. */
  gboolean              trackrect_type_set; /**< Тип тайла задан пользователем. */

  gdouble               length;             /**< Длина галса. */
  gdouble               lwidth;             /**< Ширина по левому борту. */
  gdouble               rwidth;             /**< Ширина по правому борту. */

  gint                  view_finalised;     /**< "Все тайлы для этого вью найдены и показаны". */

  guint32               dummy_color;        /**< Цвет, которым будут рисоваться заглушки. */
};

/* Внутренние методы класса. */
static void     hyscan_gtk_waterfall_object_constructed      (GObject               *object);
static void     hyscan_gtk_waterfall_object_finalize         (GObject               *object);

// static gboolean hyscan_gtk_waterfall_scroll_before           (GtkWidget             *widget,
//                                                               GdkEventScroll        *event,
//                                                               gpointer               data);
// static gboolean hyscan_gtk_waterfall_scroll_after            (GtkWidget             *widget,
//                                                               GdkEventScroll        *event,
//                                                               gpointer               data);
// static gboolean hyscan_gtk_waterfall_key_press_before        (GtkWidget             *widget,
//                                                               GdkEventKey           *event,
//                                                               gpointer               data);
// static gboolean hyscan_gtk_waterfall_key_press_after         (GtkWidget             *widget,
//                                                               GdkEventKey           *event,
//                                                               gpointer               data);
// static gboolean hyscan_gtk_waterfall_button_press            (GtkWidget             *widget,
//                                                               GdkEventButton        *event,
//                                                               gpointer               data);
// static gboolean hyscan_gtk_waterfall_motion                  (GtkWidget             *widget,
//                                                               GdkEventMotion        *event,
//                                                               gpointer               data);

static void     hyscan_gtk_waterfall_tile_ready              (HyScanTileQueue     *queue,
                                                              gpointer               data);
static gboolean hyscan_gtk_waterfall_refresh                 (gpointer               data);
static void     hyscan_gtk_waterfall_visible_draw            (GtkWidget             *widget,
                                                              cairo_t               *cairo);
// static gboolean hyscan_gtk_waterfall_update_limits           (HyScanGtkWaterfall    *waterfall,
//                                                               gdouble               *left,
//                                                               gdouble               *right,
//                                                               gdouble               *bottom,
//                                                               gdouble               *top);
//static void     hyscan_gtk_waterfall_set_view_once           (HyScanGtkWaterfall    *waterfall);
// static gboolean hyscan_gtk_waterfall_automover               (gpointer               data);

static gboolean hyscan_gtk_waterfall_configure               (GtkWidget             *widget,
                                                              GdkEventConfigure     *event);
static gint32   hyscan_gtk_waterfall_aligner                 (gdouble                in,
                                                              gint                   size);

static gboolean hyscan_gtk_waterfall_view_start              (HyScanGtkWaterfall      *waterfall);
static gboolean hyscan_gtk_waterfall_get_tile                (HyScanGtkWaterfall      *waterfall,
                                                              HyScanTile             *tile,
                                                              cairo_surface_t       **tile_surface);
static gboolean hyscan_gtk_waterfall_view_end                (HyScanGtkWaterfall     *waterfall);
// static gboolean hyscan_gtk_waterfall_track_params            (HyScanGtkWaterfall     *waterfall,
//                                                               gdouble                *lwidth,
//                                                               gdouble                *rwidth,
//                                                               gdouble                *length);
static void    hyscan_gtk_waterfall_prepare_csurface         (cairo_surface_t      **surface,
                                                              gint                   required_width,
                                                              gint                   required_height,
                                                              HyScanTileSurface     *tile_surface);




static guint    hyscan_gtk_waterfall_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfall, hyscan_gtk_waterfall, GTK_TYPE_CIFRO_AREA);

static void
hyscan_gtk_waterfall_class_init (HyScanGtkWaterfallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_object_finalize;

  hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE] =
    g_signal_new ("automove-state", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_BOOLEAN);

  hyscan_gtk_waterfall_signals[SIGNAL_ZOOM] =
    g_signal_new ("waterfall-zoom", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT_DOUBLE,
                  G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_INT);
}

static void
hyscan_gtk_waterfall_init (HyScanGtkWaterfall *gtk_waterfall)
{
  gtk_waterfall->priv = hyscan_gtk_waterfall_get_instance_private (gtk_waterfall);
}

static void
hyscan_gtk_waterfall_object_constructed (GObject *object)
{
  HyScanGtkWaterfall *gtk_waterfall = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv;
  guint n_threads = g_get_num_processors ();


  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->constructed (object);
  priv = gtk_waterfall->priv;

  priv->queue = hyscan_tile_queue_new (n_threads);
  priv->color = hyscan_tile_color_new ();
  // priv->rect  = hyscan_trackrect_new ();

  /* Параметры GtkCifroArea по умолчанию. */
  gtk_cifro_area_set_scale_on_resize (GTK_CIFRO_AREA (gtk_waterfall), FALSE);
  gtk_cifro_area_set_zoom_on_center (GTK_CIFRO_AREA (gtk_waterfall), FALSE);
  gtk_cifro_area_set_rotation (GTK_CIFRO_AREA (gtk_waterfall), FALSE);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (gtk_waterfall), -G_MAXDOUBLE, G_MAXDOUBLE, -G_MAXDOUBLE, G_MAXDOUBLE);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (gtk_waterfall), -50, 50, 0, 1000 );

  g_signal_connect (priv->queue, "tile-queue-ready", G_CALLBACK (hyscan_gtk_waterfall_tile_ready), gtk_waterfall);

  g_signal_connect (gtk_waterfall, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_visible_draw), NULL);

  g_signal_connect (gtk_waterfall, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_configure), NULL);
  // g_signal_connect (gtk_waterfall, "button-press-event", G_CALLBACK (hyscan_gtk_waterfall_button_press), gtk_waterfall);
  // g_signal_connect (gtk_waterfall, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_motion), gtk_waterfall);
  //
  // g_signal_connect_after (gtk_waterfall, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_scroll_after), NULL);
  // g_signal_connect (gtk_waterfall, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_scroll_before), NULL);
  // g_signal_connect_after (gtk_waterfall, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_key_press_after), NULL);
  // g_signal_connect (gtk_waterfall, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_key_press_before), NULL);

  priv->length = 0;
  priv->lwidth = 0;
  priv->rwidth = 0;

  // priv->auto_tag = 0;
  priv->refresh_tag = 0;
  priv->refresh_time = 40;
  // priv->automove_time = 40;

  priv->view_finalised = 0;

  priv->trackrect_type_set = FALSE;

  priv->cache_set = FALSE;
  priv->opened = FALSE;

  priv->zooms = NULL;
}

static void
hyscan_gtk_waterfall_object_finalize (GObject *object)
{
  HyScanGtkWaterfall *gtk_waterfall = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv = gtk_waterfall->priv;

  // if (priv->auto_tag > 0)
  //   g_source_remove (priv->auto_tag);
  if (priv->refresh_tag > 0)
    g_source_remove (priv->refresh_tag);
  cairo_surface_destroy (priv->surface);

  g_free (priv->zooms);

  g_clear_object (&priv->queue);
  g_clear_object (&priv->color);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->finalize (object);
}

// /* Обработчик сигнала прокрутки мыши. Выполняется до обработчика cifroarea. */
// static gboolean
// hyscan_gtk_waterfall_scroll_before (GtkWidget      *widget,
//                                     GdkEventScroll *event,
//                                     gpointer        data)
// {
//   if ((event->state & GDK_SHIFT_MASK) ||
//       (event->state & GDK_MOD1_MASK) ||
//       (event->state & GDK_CONTROL_MASK))
//     {
//       gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), -999999, 999999, -999999, 999999);
//     }
//   return FALSE;
// }
//
// /* Обработчик сигнала прокрутки мыши. Выполняется после обработчика cifroarea.*/
// static gboolean
// hyscan_gtk_waterfall_scroll_after (GtkWidget      *widget,
//                                    GdkEventScroll *event,
//                                    gpointer        data)
// {
//   gdouble left, right, bottom, top;
//   hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (widget), &left, &right, &bottom, &top);
//   gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), left, right, bottom, top);
//
//   if (HYSCAN_GTK_WATERFALL (widget)->priv->automove)
//     gtk_cifro_area_move (GTK_CIFRO_AREA (widget), 0, G_MAXDOUBLE);
//   return FALSE;
// }
//
// /* Обработчик сигнала нажатия кнопки. Выполняется до обработчика cifroarea. */
// static gboolean
// hyscan_gtk_waterfall_key_press_before (GtkWidget   *widget,
//                                        GdkEventKey *event,
//                                        gpointer     data)
// {
//   if ((event->keyval == GDK_KEY_KP_Add) ||
//       (event->keyval == GDK_KEY_KP_Subtract) ||
//       (event->keyval == GDK_KEY_plus) ||
//       (event->keyval == GDK_KEY_minus))
//     {
//       gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), -999999, 999999, -999999, 999999);
//     }
//   return FALSE;
// }
//
// /* Обработчик сигнала нажатия кнопки. Выполняется после обработчика cifroarea.*/
// static gboolean
// hyscan_gtk_waterfall_key_press_after (GtkWidget   *widget,
//                                       GdkEventKey *event,
//                                       gpointer     data)
// {
//   gdouble left, right, bottom, top;
//
//   if ((event->keyval == GDK_KEY_KP_Add) ||
//       (event->keyval == GDK_KEY_KP_Subtract) ||
//       (event->keyval == GDK_KEY_plus) ||
//       (event->keyval == GDK_KEY_minus))
//     {
//       hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (widget), &left, &right, &bottom, &top);
//       gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), left, right, bottom, top);
//
//
//       if (HYSCAN_GTK_WATERFALL (widget)->priv->automove)
//         gtk_cifro_area_move (GTK_CIFRO_AREA (widget), 0, G_MAXDOUBLE);
//     }
//   return FALSE;
// }
//
// /* Обработчик нажатия кнопок мышки. */
// static gboolean
// hyscan_gtk_waterfall_button_press (GtkWidget      *widget,
//                                    GdkEventButton *event,
//                                    gpointer        data)
// {
//   HyScanGtkWaterfall *waterfall = data;
//   HyScanGtkWaterfallPrivate *priv = waterfall->priv;
//
//   if (event->type == GDK_BUTTON_PRESS && (event->button == 1))
//     priv->mouse_y = event->y;
//
//   return FALSE;
// }
//
// static gboolean
// hyscan_gtk_waterfall_motion (GtkWidget             *widget,
//                              GdkEventMotion        *event,
//                              gpointer               data)
// {
//   HyScanGtkWaterfall *waterfall = data;
//   HyScanGtkWaterfallPrivate *priv = waterfall->priv;
//
//   if (event->state & GDK_BUTTON1_MASK)
//     {
//       if (event->y < priv->mouse_y && priv->automove)
//         {
//           priv->automove = FALSE;
//           g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
//         }
//     }
//   return FALSE;
// }

/* Обработчик сигнала готовности тайла*/
static void
hyscan_gtk_waterfall_tile_ready (HyScanTileQueue   *queue,
                                 gpointer           data)
{
  HyScanGtkWaterfall *waterfall = data;

  /* Когда тайл готов, устанавливается флаг need_to_redraw. */
  g_atomic_int_inc (&waterfall->priv->need_to_redraw);
}

/* Функция, запрашивающая перерисовку виджета. Вызывается из mainloop. */
static gboolean
hyscan_gtk_waterfall_refresh (gpointer data)
{
  HyScanGtkWaterfall *waterfall = data;

  if (g_atomic_int_get (&(waterfall->priv->need_to_redraw)) > 0)
    {
      g_atomic_int_set (&waterfall->priv->need_to_redraw, 0);
      gtk_widget_queue_draw (GTK_WIDGET(waterfall));
    }

  return G_SOURCE_CONTINUE;
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_visible_draw (GtkWidget *widget,
                                   cairo_t   *cairo)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = waterfall->priv;

  HyScanTile tile;
  gint32 tile_size      = 0,
         start_tile_x0  = 0,
         start_tile_y0  = 0,
         end_tile_x0    = 0,
         end_tile_y0    = 0;
  gint num_of_tiles_x = 0,
       num_of_tiles_y = 0,
       i, j,
       zoom_num;
  gdouble from_x, from_y, to_x, to_y,
          x_coord, y_coord, x_coord0, y_coord0,
          scale;
  gboolean view_finalised = TRUE;

  if (!priv->opened)
    return;

  gtk_cifro_area_get_view	(GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
  zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (widget), &scale, NULL);

  from_x *= 1000.0;
  to_x   *= 1000.0;
  from_y *= 1000.0;
  to_y   *= 1000.0;
  scale  *= 1000.0 / (25.4 / priv->ppi);

  /* Определяем размер тайла. */
  tile_size = tile_sizes[zoom_num];

  /* Определяем параметры искомых тайлов. */
  start_tile_x0 = hyscan_gtk_waterfall_aligner (from_x, tile_size);
  start_tile_y0 = hyscan_gtk_waterfall_aligner (from_y, tile_size);
  end_tile_x0   = hyscan_gtk_waterfall_aligner (to_x,   tile_size);
  end_tile_y0   = hyscan_gtk_waterfall_aligner (to_y,   tile_size);

  num_of_tiles_x = (end_tile_x0 - start_tile_x0) / tile_size + 1;
  num_of_tiles_y = (end_tile_y0 - start_tile_y0) / tile_size + 1;

  /* Необходимо сообщить тайл-менеджеру, что начался новый view. */
  hyscan_gtk_waterfall_view_start (waterfall);

  gtk_cifro_area_visible_value_to_point	(GTK_CIFRO_AREA (widget), &x_coord0, &y_coord0,
                                        (gdouble)(start_tile_x0 / 1000.0),
                                        (gdouble)((start_tile_y0 + tile_size) / 1000.0));
  x_coord0 = round(x_coord0);
  y_coord0 = round(y_coord0);

  /* Ищем тайлы .*/
  for (i = num_of_tiles_y - 1; i >= 0; i--)
    {
      for (j = 0; j < num_of_tiles_x; j++)
        {
          /* Виджет знает только о том, какие размеры тайла, масштаб и ppi.
           * У него нет информации ни о цветовой схеме, ни о фильтре. */
          tile.across_start = start_tile_x0 + j * tile_size;
          tile.along_start   = start_tile_y0 + i * tile_size;
          tile.across_end   = start_tile_x0 + (j + 1) * tile_size;
          tile.along_end     = start_tile_y0 + (i + 1) * tile_size;
          tile.scale            = scale;
          tile.ppi              = priv->ppi;

          /* Делаем запрос тайл-менеджеру. */
          if (hyscan_gtk_waterfall_get_tile (waterfall, &tile, &(priv->surface)))
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
      i = num_of_tiles_y;
      for (j = 0; j < num_of_tiles_x; j++)
        {
          tile.across_start = start_tile_x0 + j * tile_size;
          tile.along_start   = start_tile_y0 + i * tile_size;
          tile.across_end   = start_tile_x0 + (j + 1) * tile_size;
          tile.along_end     = start_tile_y0 + (i + 1) * tile_size;
          hyscan_gtk_waterfall_get_tile (waterfall, &tile, NULL);
        }

  priv->view_finalised = (view_finalised) ? 1 : 0;

  /* Необходимо сообщить тайл-менеджеру, что view закончился. */
  hyscan_gtk_waterfall_view_end (waterfall);
}

// static gboolean
// hyscan_gtk_waterfall_update_limits (HyScanGtkWaterfall *waterfall,
//                                     gdouble            *left,
//                                     gdouble            *right,
//                                     gdouble            *bottom,
//                                     gdouble            *top)
// {
//   HyScanGtkWaterfallPrivate *priv = waterfall->priv;
//
//   gdouble lwidth = 0.0;
//   gdouble rwidth = 0.0;
//   gdouble length = 0.0;
//   gboolean writeable;
//   gint visible_w, visible_h,
//        lborder, rborder, tborder, bborder,
//        zoom_num;
//   gdouble req_width = 0.0,
//           req_length = 0.0,
//           min_length = 0.0,
//           max_length = 0.0;
//   gdouble bottom_limit = 0.0,
//           bottom_view = 0.0;
//
//   /* К этому моменту времени я уверен, что у меня есть хоть какие-то
//    * геометрические параметры галса. */
//   if (priv->opened)
//     {
//       writeable = hyscan_gtk_waterfall_track_params (waterfall, &lwidth, &rwidth, &length);
//     }
//   else
//     {
//       writeable = TRUE;
//       length = lwidth = rwidth = 0.0;
//     }
//
//   priv->length = length;
//   priv->lwidth = lwidth;
//   priv->rwidth = rwidth;
//
//   /* Получаем параметры цифроарии. */
//   gtk_cifro_area_get_size	(GTK_CIFRO_AREA (waterfall), &visible_w, &visible_h);
//   gtk_cifro_area_get_border (GTK_CIFRO_AREA (waterfall), &lborder, &rborder, &tborder, &bborder);
//   zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);
//
//   visible_w -= (lborder + rborder);
//   visible_h -= (tborder + bborder);
//
//   /* Для текущего масштаба рассчиываем необходимую длину и ширину по каждому борту. */
//   req_length = visible_h * priv->zooms[zoom_num];
//   req_width = visible_w * priv->zooms[zoom_num];
//
//   /* Горизонтальные размеры области рассчитываются независимо от вертикальных и режима работы. */
//   req_width = MAX (req_width, (lwidth + rwidth));
//
//   if (lwidth <= 0.0 && rwidth <= 0.0)
//     {
//       lwidth = req_width / 2.0;
//       rwidth = req_width / 2.0;
//     }
//   else
//     {
//       gdouble lratio = lwidth / (rwidth + lwidth);
//       lwidth = req_width * lratio;
//       rwidth = req_width - lwidth;
//     }
//
//   /* Вертикальные размеры области. */
//   if (!writeable) /*< Режим чтения. */
//     {
//       priv->automove = FALSE;
//       g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, waterfall);
//
//       req_length = MAX (length, req_length);
//       if (length >= req_length)
//         {
//           min_length = 0;
//           max_length = length;
//         }
//       else
//         {
//           min_length = (req_length - length) / 2.0;
//           max_length = req_length - min_length;
//         }
//     }
//   else if (length < req_length) /*< Режим дозаписи, длина галса меньше виджета на текущем масштабе. */
//     {
//       max_length = length;
//       min_length = (req_length - max_length);
//     }
//   else /*< Режим дозаписи, длина галса больше виджета на самом мелком масштабе. */
//     {
//       min_length = 0;
//       max_length = length;
//     }
//
//   if (!priv->automove && writeable)
//     {
//       gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (waterfall), NULL, NULL, &bottom_limit, NULL);
//       gtk_cifro_area_get_view (GTK_CIFRO_AREA (waterfall), NULL, NULL, &bottom_view, NULL);
//       bottom_limit = MAX (bottom_view, bottom_limit);
//       bottom_limit = MIN (-min_length, bottom_limit);
//     }
//   else
//     {
//       bottom_limit = -min_length;
//     }
//
//   if (left != NULL)
//     *left = -lwidth;
//   if (right != NULL)
//     *right = rwidth;
//   if (bottom != NULL)
//     *bottom = bottom_limit;
//   if (top != NULL)
//     *top = max_length;
//
//   return writeable;
// }



// static gboolean
// hyscan_gtk_waterfall_automover (gpointer data)
// {
//   HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (data);
//   HyScanGtkWaterfallPrivate *priv;
//   gdouble distance;
//   gint64 time;
//   gint zoom_num;
//   priv = waterfall->priv;
//   gdouble left, right;
//   gdouble old_bottom, old_top;
//   gdouble required_bottom, required_top;
//   gdouble bottom, top;
//   gboolean writeable;
//
//   if (!priv->opened)
//     return G_SOURCE_CONTINUE;
//
//   /* Актуальные данные по пределам видимой области. */
//   gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (waterfall), NULL, NULL, &old_bottom, NULL);
//   writeable = hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (waterfall), &left, &right, &required_bottom, &required_top);
//
//   /* Наращиваем пределы видимой области. */
//   gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, old_bottom, required_top);
//
//   /* Теперь при необходимости сдвигаемся и сжимаем видимую область. */
//   if (priv->view_finalised && priv->automove)
//     {
//       time = g_get_monotonic_time ();
//       distance = (time - priv->prev_time) / 1e6;
//       distance *= priv->ship_speed;
//
//       /* Сдвигаем видимую область. */
//       gtk_cifro_area_get_view (GTK_CIFRO_AREA (waterfall), &left, &right, &bottom, &old_top);
//       zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);
//       if (old_top + distance < required_top - 1.0 * priv->ship_speed)
//         {
//           distance = required_top - 0.5 * priv->ship_speed - old_top;
//           distance /= priv->zooms[zoom_num];
//           gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, distance);
//         }
//       else if (old_top + distance < required_top - 0.5 * priv->ship_speed)
//         {
//           distance /= priv->zooms[zoom_num];
//           gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, distance);
//         }
//       priv->prev_time = time;
//     }
//
//   /* Когда данных недостаточно, нужно сжимать видимую область. */
//   gtk_cifro_area_get_view (GTK_CIFRO_AREA (waterfall), NULL, NULL, &bottom, &top);
//   gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (waterfall), NULL, NULL, &old_bottom, &old_top);
//   if (required_bottom < 0.0)
//     {
//       gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, old_top);
//     }
//
//   if (writeable)
//     return G_SOURCE_CONTINUE;
//
//   hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (waterfall), &left, &right, &bottom, &top);
//   gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);
//
//   priv->automove = FALSE;
//   g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
//   priv->auto_tag = 0;
//   return G_SOURCE_REMOVE;
// }

/* Функция обработки сигнала изменения параметров дисплея. */
static gboolean
hyscan_gtk_waterfall_configure (GtkWidget            *widget,
                                GdkEventConfigure    *event)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);

  HyScanGtkWaterfallPrivate *priv = waterfall->priv;
  GdkScreen *screen;
  GdkRectangle rect;
  gint monitor_num = 0,
       monitor_h = 0,
       monitor_w = 0,
       square_pixels = 0,
       i = 0;
  gfloat ppi, diagonal;
  //gdouble left, right, bottom, top;

  screen = gdk_window_get_screen (event->window);
  monitor_num = gdk_screen_get_monitor_at_window (screen, event->window);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &rect);
  monitor_h = gdk_screen_get_monitor_height_mm (screen, monitor_num);
  monitor_w = gdk_screen_get_monitor_width_mm (screen, monitor_num);

  square_pixels = sqrt (rect.width * rect.width + rect.height * rect.height);
  diagonal = sqrt (monitor_w * monitor_w + monitor_h * monitor_h) / 25.4;
  ppi = square_pixels / diagonal;
  if (isnan (ppi) || isinf(ppi) || ppi <= 0.0 || monitor_h <= 0 || monitor_w <= 0)
    ppi = 96.0;
  priv->ppi = ppi;

  /* Обновляем масштабы. */
  if (priv->zooms == NULL)
    priv->zooms = g_malloc0 (ZOOM_LEVELS * sizeof (gdouble));

  for (i = 0; i < ZOOM_LEVELS; i++)
    priv->zooms[i] = zooms_gost[i] / (1000.0 / (25.4 / priv->ppi));
  gtk_cifro_area_set_fixed_zoom_scales (GTK_CIFRO_AREA (waterfall), priv->zooms, priv->zooms, ZOOM_LEVELS);

  /* Пересоздаем заглушку. */
  // TODO: priv->dummy = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  // TODO: hyscan_gtk_waterfall_update_limits (waterfall, &left, &right, &bottom, &top);
  //gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);

  g_atomic_int_set (&(priv->need_to_redraw), 1);

  return FALSE;
}

/* Функция проверяет параметры cairo_surface_t и пересоздает в случае необходимости. */
static void
hyscan_gtk_waterfall_prepare_csurface (cairo_surface_t **surface,
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


/* Округление координат тайла. */
static gint32
hyscan_gtk_waterfall_aligner (gdouble in,
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

/* Функция создает новый виджет водопада. */
GtkWidget*
hyscan_gtk_waterfall_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL, NULL);
}

void
hyscan_gtk_waterfall_set_cache (HyScanGtkWaterfall *waterfall,
                                HyScanCache        *cache,
                                HyScanCache        *cache2,
                                const gchar        *cache_prefix)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  /* Запрещаем менять систему кэширования на лету. */
  if (priv->open)
    return;

  /* Запрещаем NULL вместо кэша. */
  if (cache == NULL || cache2 == NULL)
    return;

  hyscan_tile_color_set_cache (priv->color, cache2, cache_prefix);
  hyscan_tile_queue_set_cache (priv->queue, cache, cache_prefix);
  //hyscan_trackrect_set_cache (priv->rect, cache, cache_prefix);

  /* Поднимаем флаг. */
  priv->cache_set = TRUE;
}

/* Функция установки скорости. */
void
hyscan_gtk_waterfall_set_speeds (HyScanGtkWaterfall *waterfall,
                                 gfloat              ship_speed,
                                 GArray             *sound_speed)
{
  HyScanGtkWaterfallPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  /* Запрещаем менять на лету. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_speed (priv->queue, ship_speed, sound_speed);
  //hyscan_trackrect_set_speeds (priv->rect, ship_speed, sound_speed);
}

void
hyscan_gtk_waterfall_setup_depth (HyScanGtkWaterfall *waterfall,
                                  HyScanDepthSource       source,
                                  gint                    size,
                                  gulong                  microseconds)
{
  HyScanGtkWaterfallPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  /* Запрещаем менять на лету. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_depth (priv->queue, source, size, microseconds);
  //hyscan_trackrect_set_depth (priv->rect, source, size, microseconds);
}

/* Функция обновляет параметры HyScanTileQueue и HyScanTrackRect. */
void
hyscan_gtk_waterfall_update_tile_param (HyScanGtkWaterfall *waterfall,
                                        HyScanTileType      type,
                                        gint                upsample)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);
  priv = waterfall->priv;

  /* Настраивать HyScanTileQueue можно только когда галс не открыт. */
  if (priv->open)
    return;

  hyscan_tile_queue_set_params (priv->queue, type, upsample);
  //hyscan_trackrect_set_type (priv->rect, type);
  // TODO: redraw hyscan_gtk_waterfall_redraw_sender (NULL, waterfall);
}

gboolean
hyscan_gtk_waterfall_set_colormap (HyScanGtkWaterfall *waterfall,
                                   guint32            *colormap,
                                   gint                length,
                                   guint32             background)
{
  gboolean status = FALSE;
  HyScanGtkWaterfallPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);
  priv = waterfall->priv;

  status = hyscan_tile_color_set_colormap (priv->color, colormap, length, background);
  // TODO: redraw hyscan_gtk_waterfall_redraw_sender (NULL, waterfall);

  return status;
}

/* Функция устанавливает уровни. */
gboolean
hyscan_gtk_waterfall_set_levels (HyScanGtkWaterfall *waterfall,
                                 gdouble             black,
                                 gdouble             gamma,
                                 gdouble             white)
{
  gboolean status = FALSE;
  HyScanGtkWaterfallPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);
  priv = waterfall->priv;

  status = hyscan_tile_color_set_levels (priv->color, black, gamma, white);
  // TODO: redraw hyscan_gtk_waterfall_redraw_sender (NULL, waterfall);

  return status;
}

/* Функция открывает БД, проект, галс... */
void
hyscan_gtk_waterfall_open (HyScanGtkWaterfall *waterfall,
                           HyScanDB           *db,
                           const gchar        *project,
                           const gchar        *track,
                           HyScanTileSource    source,
                           gboolean            raw)
{
  HyScanGtkWaterfallPrivate *priv;
  gchar *db_uri;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  /* Запрещаем вызывать эту функцию, если не установлен кэш. */
  if (!priv->cache_set)
    return;

  //if (priv->auto_tag != 0)
    //g_source_remove (priv->auto_tag);
  if (priv->refresh_tag != 0)
    g_source_remove (priv->refresh_tag);

  db_uri = hyscan_db_get_uri (db);

  hyscan_tile_color_open (priv->color, db_uri, project, track);
  hyscan_tile_queue_open (priv->queue, db, project, track, source, raw);
  //hyscan_trackrect_open (priv->rect, db, project, track, raw);

  priv->open = TRUE;

  g_free (db_uri);

  priv->length = 0;
  priv->lwidth = 0;
  priv->rwidth = 0;
  priv->opened = TRUE;
  //priv->automove = FALSE;
  //TODO:
  //TODO: hyscan_gtk_waterfall_set_view_once (waterfall);
  priv->prev_time = g_get_monotonic_time ();

  //priv->auto_tag = g_timeout_add (priv->automove_time, hyscan_gtk_waterfall_automover, waterfall);
  priv->refresh_tag = g_timeout_add (priv->refresh_time, hyscan_gtk_waterfall_refresh, waterfall);
}

/* Функция закрывает БД, проект, галс... */
void
hyscan_gtk_waterfall_close (HyScanGtkWaterfall *waterfall)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  hyscan_tile_color_close (priv->color);
  hyscan_tile_queue_close (priv->queue);
  //hyscan_trackrect_close (priv->rect);

  priv->open = FALSE;}

void
hyscan_gtk_waterfall_set_refresh_time (HyScanGtkWaterfall *waterfall,
                                       guint               interval)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  if (priv->refresh_time != interval)
    {
      priv->refresh_time = interval;
      if (priv->refresh_tag != 0)
        g_source_remove (priv->refresh_tag);

      priv->refresh_tag = g_timeout_add (priv->refresh_time, hyscan_gtk_waterfall_refresh, waterfall);
    }
}

// /* Функция устанавливает частоту кадров. */
// void
// hyscan_gtk_waterfall_set_fps (HyScanGtkWaterfall *waterfall,
//                               guint               fps)
// {
//   HyScanGtkWaterfallPrivate *priv;
//   guint interval;
//   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
//   priv = waterfall->priv;
//
//   interval = 1000 / fps;
//
//   if (priv->automove_time != interval)
//     {
//       priv->automove_time = interval;
//       if (priv->auto_tag != 0)
//         g_source_remove (priv->auto_tag);
//       priv->auto_tag = g_timeout_add (priv->automove_time, hyscan_gtk_waterfall_automover, waterfall);
//     }
// }

// /* Функция сдвижки изображения. */
// void
// hyscan_gtk_waterfall_automove (HyScanGtkWaterfall *waterfall,
//                                gboolean            on)
// {
//   HyScanGtkWaterfallPrivate *priv;
//   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
//   priv = waterfall->priv;
//
//   priv->automove = on;
//
//   if (on)
//     {
//       gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, G_MAXDOUBLE);
//       /* Если функции нет, создаем её. */
//       if (priv->auto_tag == 0)
//         priv->auto_tag = g_timeout_add (priv->automove_time, hyscan_gtk_waterfall_automover, waterfall);
//     }
// }

/* Функция возвращает текущий масштаб. */
gint
hyscan_gtk_waterfall_get_scale (HyScanGtkWaterfall *waterfall,
                                const gdouble     **zooms,
                                gint               *num)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1);

  if (zooms != NULL)
    *zooms = zooms_gost;
  if (num != NULL)
    *num = ZOOM_LEVELS;

  return gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);
}

// /* Функция зуммирует изображение. */
// gint
// hyscan_gtk_waterfall_zoom (HyScanGtkWaterfall *waterfall,
//                            gboolean            zoom_in)
// {
//   gint zoom_num;
//   gdouble min_x, max_x, min_y, max_y;
//   gdouble left, right, bottom, top;
//
//   g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1);
//
//   if (!waterfall->priv->opened)
//     return -1;
//
//   gtk_cifro_area_get_view	(GTK_CIFRO_AREA (waterfall), &min_x, &max_x, &min_y, &max_y);
//   gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), -999999, 999999, -999999, 999999);
//
//   gtk_cifro_area_fixed_zoom	(GTK_CIFRO_AREA (waterfall), (max_x + min_x) / 2.0, (max_y + min_y) / 2.0, zoom_in);
//
//   hyscan_gtk_waterfall_update_limits (waterfall, &left, &right, &bottom, &top);
//   gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);
//
//   if (waterfall->priv->automove)
//     gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, G_MAXDOUBLE);
//
//   zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);
//
//   if (zoom_num >= 0 && zoom_num < ZOOM_LEVELS)
//     g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_ZOOM], 0, zoom_num, zooms_gost[zoom_num]);
//
//   return zoom_num;
// }

/* Функция, начинающая view. */
gboolean
hyscan_gtk_waterfall_view_start (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);
  waterfall->priv->view_id++;
  return TRUE;
}

/* Функция получения тайла. */
gboolean
hyscan_gtk_waterfall_get_tile (HyScanGtkWaterfall *waterfall,
                              HyScanTile        *tile,
                              cairo_surface_t  **surface)
{
  HyScanGtkWaterfallPrivate *priv;

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

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);
  priv = waterfall->priv;

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
      hyscan_gtk_waterfall_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found && queue_equal)             /* Кэш2: неправильный тайл; кэш1: правильный. */
    {
      show_strategy = RECOLOR_AND_SHOW;
      hyscan_gtk_waterfall_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else if (color_found)                            /* Кэш2: неправильный тайл; кэш1: неправильный. */
    {
      show_strategy = SHOW;
      regenerate = TRUE;
      hyscan_gtk_waterfall_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found)                            /* Кэш2: ничего нет; кэш1: неправильный. */
    {
      show_strategy = RECOLOR_AND_SHOW;
      regenerate = TRUE;
      hyscan_gtk_waterfall_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
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
hyscan_gtk_waterfall_view_end (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), FALSE);

  /* Запрещаем работу, если кэш не установлен и не открыты БД/проект/галс. */
  if (!waterfall->priv->open)
    return FALSE;

  hyscan_tile_queue_add_finished (waterfall->priv->queue, waterfall->priv->view_id);

  return TRUE;
}

// gboolean
// hyscan_gtk_waterfall_track_params (HyScanGtkWaterfall *waterfall,
//                                    gdouble           *lwidth,
//                                    gdouble           *rwidth,
//                                    gdouble           *length)
// {
//   g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), TRUE);
//
//   return hyscan_trackrect_get (waterfall->priv->rect, lwidth, rwidth, length);
// }
