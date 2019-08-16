/* hyscan-gtk-waterfall.c
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-gtk-waterfall
 * @Title: HyScanGtkWaterfall
 * @Short_description: отображение гидролокационных данных в режиме водопад
 *
 * Данный виджет занимается всем, что связано с отрисовкой тайлов.
 * Он содержит объекты HyScanTileQueue и HyScanTileColor, которые занимаются
 * генерацией и раскрашиванием тайлов.
 *
 * Помимо этого он берет на себя ещё три задачи: организация масштабов,
 * ограничение области просмотра и автосдвижка.
 * Метод #hyscan_gtk_waterfall_get_scale позволяет получить
 * список масштабов и номер текущего масштаба.
 * При смене масштаба эмиттируется #HyScanGtkWaterfall::waterfall-zoom.
 *
 * Метод #hyscan_gtk_waterfall_set_upsample позволяет задать
 * величину передискретизации. Это единственная публичная настройка
 * генератора тайлов. Остальные параметры (тип отображения, источники, флаги)
 * берутся из родителя.
 *
 * Настройка раскрашивания тайлов выполняется следующими функциями:
 * - #hyscan_gtk_waterfall_set_colormap для установки цветовой
 *    схемы для конкретного источника данных;
 * - #hyscan_gtk_waterfall_set_colormap_for_all для установки
 *    цветовой схемы для всех источников сразу;
 * - #hyscan_gtk_waterfall_set_levels для установки уровней
 *    черной и белой точки и гамма-коррекции для конкретного источника
 *    данных;
 * - #hyscan_gtk_waterfall_set_levels_for_all для установки
 *    уровней для всех источников сразу.
 *
 * Автосдвижка:
 * - #hyscan_gtk_waterfall_automove включает и выключает автосдвижку;
 * - #hyscan_gtk_waterfall_set_automove_period задает период
 *    автосдвижки.
 *
 * Всякий раз при изменении состояния автосдвижки эмиттируется
 * #HyScanGtkWaterfall::automove-state
 *
 * Для снижения нагрузки на систему можно использовать метод
 * #hyscan_gtk_waterfall_set_regeneration_period. Все тайлы,
 * которые не были обнаружены в кэше, будут в любом случае отправлены
 * на генерацию. Те тайлы, которые в кэше есть, но помечены как требующие
 * перегенерации, будут отправлены не раньше, чем закончится указанное
 * время. При этом дистанция на изображении будет отличаться от реальной
 * на величину, соответствующую скорости судна умноженной на период
 * перегенерации.
 */

#include "hyscan-gtk-waterfall.h"
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

static const gint    tile_sizes[ZOOM_LEVELS] = {500e3,  200e3,  100e3,  80e3,  50e3,  40e3,  20e3,  10e3,  5e3,  2e3,  1e3,  500, 400, 200, 100}; /* В миллиметрах. */
static const gdouble zooms_gost[ZOOM_LEVELS] = {5000.0, 2000.0, 1000.0, 800.0, 500.0, 400.0, 200.0, 100.0, 50.0, 20.0, 10.0, 5.0, 4.0, 2.0, 1.0};

struct _HyScanGtkWaterfallPrivate
{
  gfloat                 ppi;                /* PPI. */
  cairo_surface_t       *surface;            /* Поверхность тайла. */

  HyScanTileQueue       *queue;
  HyScanTileColor       *color;

  guint64                view_id;
  gpointer               tq_hash;

  gdouble               *zooms;              /* Масштабы с учетом PPI. */
  gint                   zoom_index;

  gboolean               open;               /* Флаг "галс открыт". */
  gboolean               request_redraw;     /* Флаг "требуется перерисовка". */
  guint                  redraw_tag;

  gboolean               view_finalised;     /* "Все тайлы для этого вью найдены и показаны". */

  cairo_surface_t       *dummy;              /* Поверхность заглушки. */
  guint32                dummy_color;        /* Цвет подложки. */

  guint                  tile_upsample;      /* Величина передискретизации. */
  HyScanTileFlags        tile_flags;         /* Флаги генерации. */

  HyScanTrackRect       *lrect;
  HyScanTrackRect       *rrect;
  gboolean               writeable;
  gboolean               init;
  gboolean               once;
  gdouble                length;             /* Длина галса. */
  gdouble                lwidth;             /* Ширина по левому борту. */
  gdouble                rwidth;             /* Ширина по правому борту. */

  guint                  widget_width;
  guint                  widget_height;

  HyScanWaterfallDisplayType widget_type;
  HyScanSourceType       left_source;
  HyScanSourceType       right_source;

  gfloat                 ship_speed;
  gint64                 prev_time;
  gboolean               automove;           /* Включение и выключение режима автоматической сдвижки. */
  guint                  auto_tag;           /* Идентификатор функции сдвижки. */
  guint                  automove_time;      /* Период обновления экрана. */

  guint                  regen_time;         /* Время предыдущей генерации. */
  guint                  regen_time_prev;    /* Время предыдущей генерации. */
  guint                  regen_period;       /* Интервал между перегенерациями. */
  gboolean               regen_sent;         /* Интервал между перегенерациями. */
  gboolean               regen_allowed;      /* Интервал между перегенерациями. */

  GtkAdjustment         *hadjustment;
  GtkAdjustment         *vadjustment;
};

/* Внутренние методы класса. */
static void     hyscan_gtk_waterfall_object_constructed      (GObject                       *object);
static void     hyscan_gtk_waterfall_object_finalize         (GObject                       *object);

static gboolean hyscan_gtk_waterfall_redrawer                (gpointer                       data);

static gint32   hyscan_gtk_waterfall_aligner                 (gdouble                        in,
                                                              gint                           size);
static void     hyscan_gtk_waterfall_prepare_csurface        (cairo_surface_t              **surface,
                                                              gint                           required_width,
                                                              gint                           required_height,
                                                              HyScanTileSurface             *tile_surface);
static void     hyscan_gtk_waterfall_prepare_tile            (HyScanGtkWaterfallPrivate     *priv,
                                                              HyScanTile                     *tile);
static gboolean hyscan_gtk_waterfall_get_tile                (HyScanGtkWaterfall            *self,
                                                              HyScanTile                    *tile,
                                                              cairo_surface_t              **tile_surface);
static void     hyscan_gtk_waterfall_hash_changed            (HyScanGtkWaterfall            *self,
                                                              gulong                         hash);
static void     hyscan_gtk_waterfall_image_generated         (HyScanGtkWaterfall            *self,
                                                              HyScanTile                    *tile,
                                                              gfloat                        *img,
                                                              gint                           size,
                                                              gulong                         hash);

static void     hyscan_gtk_waterfall_visible_draw            (GtkWidget                     *widget,
                                                              cairo_t                       *cairo);
static void     hyscan_gtk_waterfall_dummy_draw              (GtkWidget                     *widget,
                                                              cairo_t                       *cairo);

static void     hyscan_gtk_waterfall_draw_surface            (cairo_surface_t               *src,
                                                              cairo_t                       *dst,
                                                              gdouble                        x,
                                                              gdouble                        y);
static void     hyscan_gtk_waterfall_create_dummy            (HyScanGtkWaterfall            *self);
static gboolean hyscan_gtk_waterfall_configure               (GtkWidget                     *widget,
                                                              GdkEventConfigure             *event);

static void     hyscan_gtk_waterfall_cifroarea_zoom          (GtkCifroArea                  *carea,
                                                              GtkCifroAreaZoomType           direction_x,
                                                              GtkCifroAreaZoomType           direction_y,
                                                              gdouble                        center_x,
                                                              gdouble                        center_y);
static void     hyscan_gtk_waterfall_cifroarea_check_scale   (GtkCifroArea                  *carea,
                                                              gdouble                       *scale_x,
                                                              gdouble                       *scale_y);
static void     hyscan_gtk_waterfall_cifroarea_get_limits    (GtkCifroArea                  *carea,
                                                              gdouble                       *min_x,
                                                              gdouble                       *max_x,
                                                              gdouble                       *min_y,
                                                              gdouble                       *max_y);
static void     hyscan_gtk_waterfall_cifroarea_get_stick     (GtkCifroArea                  *carea,
                                                              GtkCifroAreaStickType         *stick_x,
                                                              GtkCifroAreaStickType         *stick_y);

static gboolean hyscan_gtk_waterfall_automover               (gpointer                       data);


static void     hyscan_gtk_waterfall_sources_changed         (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);
static void     hyscan_gtk_waterfall_tile_flags_changed       (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);
static void     hyscan_gtk_waterfall_profile_changed         (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);
static void     hyscan_gtk_waterfall_track_changed           (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);
static void     hyscan_gtk_waterfall_speed_changed           (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);
static void     hyscan_gtk_waterfall_velocity_changed        (HyScanGtkWaterfallState          *model,
                                                              HyScanGtkWaterfall            *self);

static guint    hyscan_gtk_waterfall_signals[SIGNAL_LAST] = {0};

static void
hyscan_gtk_waterfall_changed (HyScanGtkWaterfall *self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfall, hyscan_gtk_waterfall, HYSCAN_TYPE_GTK_WATERFALL_STATE);

static void
hyscan_gtk_waterfall_class_init (HyScanGtkWaterfallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCifroAreaClass *carea = GTK_CIFRO_AREA_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_object_finalize;

  carea->check_scale = hyscan_gtk_waterfall_cifroarea_check_scale;
  carea->get_limits = hyscan_gtk_waterfall_cifroarea_get_limits;
  carea->get_stick = hyscan_gtk_waterfall_cifroarea_get_stick;
  carea->zoom = hyscan_gtk_waterfall_cifroarea_zoom;

  /**
   * HyScanGtkWaterfall::automove-state:
   * @waterfall: объект, получивший сигнал
   * @state: состояние автосдвижки (%TRUE, если включена)
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Сигнал отправляется когда изменяются параметры автосдвижки
   */
  hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE] =
    g_signal_new ("automove-state", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1, G_TYPE_BOOLEAN);

  /**
   * HyScanGtkWaterfall::waterfall-zoom:
   * @waterfall: объект, получивший сигнал
   * @num: номер масштаба
   * @zoom: человекочитаемый масштаб (1:zoom)
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Сигнал отправляется когда изменяются параметры автосдвижки
   */
  hyscan_gtk_waterfall_signals[SIGNAL_ZOOM] =
    g_signal_new ("waterfall-zoom", HYSCAN_TYPE_GTK_WATERFALL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_gui_marshal_VOID__INT_DOUBLE,
                  G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_DOUBLE);
}

static void
hyscan_gtk_waterfall_init (HyScanGtkWaterfall *self)
{
  self->priv = hyscan_gtk_waterfall_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_object_constructed (GObject *object)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv;
  HyScanCache *cache;
  HyScanFactoryAmplitude *af;
  HyScanFactoryDepth *df;
  guint n_threads = g_get_num_processors ();

  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->constructed (object);
  priv = self->priv;

  cache = hyscan_gtk_waterfall_state_get_cache (HYSCAN_GTK_WATERFALL_STATE (self));
  af = hyscan_gtk_waterfall_state_get_amp_factory (HYSCAN_GTK_WATERFALL_STATE (self));
  df = hyscan_gtk_waterfall_state_get_dpt_factory (HYSCAN_GTK_WATERFALL_STATE (self));

  priv->queue = hyscan_tile_queue_new (n_threads, cache, af, df);
  priv->color = hyscan_tile_color_new (cache);
  priv->lrect = hyscan_track_rect_new (cache, af, df);
  priv->rrect = hyscan_track_rect_new (cache, af, df);

  priv->hadjustment = gtk_adjustment_new (0, 0, 0, 0, 1, 1);
  priv->vadjustment = gtk_adjustment_new (0, 0, 0, 0, 1, 1);

  g_signal_connect_swapped (priv->hadjustment, "value-changed", G_CALLBACK (hyscan_gtk_waterfall_changed), self);
  g_signal_connect_swapped (priv->vadjustment, "value-changed", G_CALLBACK (hyscan_gtk_waterfall_changed), self);

  /* Параметры GtkCifroArea по умолчанию. */
  gtk_cifro_area_set_scale_on_resize (GTK_CIFRO_AREA (self), FALSE);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (self), -1, 1, -1, 1);

  /* Сигналы HyScanTileQueue. */
  g_signal_connect_swapped (priv->queue, "tile-queue-image", G_CALLBACK (hyscan_gtk_waterfall_image_generated), self);
  g_signal_connect_swapped (priv->queue, "tile-queue-hash", G_CALLBACK (hyscan_gtk_waterfall_hash_changed), self);

  /* Сигналы Gtk и GtkCifroArea. */
  g_signal_connect (self, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_visible_draw), NULL);
  g_signal_connect_after (self, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_configure), NULL);

  /* Сигналы HyScanGtkWaterfallState. */
  g_signal_connect (self, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_sources_changed), self);
  g_signal_connect (self, "changed::tile-flags",   G_CALLBACK (hyscan_gtk_waterfall_tile_flags_changed), self);
  g_signal_connect (self, "changed::profile",      G_CALLBACK (hyscan_gtk_waterfall_profile_changed), self);
  g_signal_connect (self, "changed::track",        G_CALLBACK (hyscan_gtk_waterfall_track_changed), self);
  g_signal_connect (self, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_speed_changed), self);
  g_signal_connect (self, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_velocity_changed), self);
  // g_signal_connect (self, "changed::amp-factory",  G_CALLBACK (hyscan_gtk_waterfall_depth_amp_changed), self);
  // g_signal_connect (self, "changed::dpt-factory",  G_CALLBACK (hyscan_gtk_waterfall_cache_dpt_changed), self);

  hyscan_gtk_waterfall_sources_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);
  hyscan_gtk_waterfall_tile_flags_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);
  hyscan_gtk_waterfall_profile_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);
  hyscan_gtk_waterfall_track_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);
  hyscan_gtk_waterfall_speed_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);
  hyscan_gtk_waterfall_velocity_changed (HYSCAN_GTK_WATERFALL_STATE (self), self);

  /* Инициализируем масштабы. */
  priv->zooms = g_malloc0 (ZOOM_LEVELS * sizeof (gdouble));
  priv->view_finalised = 0;
  priv->automove_time = 40;
  priv->regen_period = 1 * G_TIME_SPAN_SECOND;

  priv->tile_upsample = 2;

  priv->redraw_tag = g_timeout_add (10, hyscan_gtk_waterfall_redrawer, self);

  g_object_unref (cache);
  g_object_unref (af);
  g_object_unref (df);

}

static void
hyscan_gtk_waterfall_object_finalize (GObject *object)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv = self->priv;

  cairo_surface_destroy (priv->surface);
  cairo_surface_destroy (priv->dummy);

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

  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->finalize (object);
}

/* Функция запрашивает перерисовку виджета, если необходимо. */
static gboolean
hyscan_gtk_waterfall_redrawer (gpointer data)
{
  HyScanGtkWaterfall *self = data;

  if (g_atomic_int_compare_and_exchange (&self->priv->request_redraw, TRUE, FALSE))
    gtk_widget_queue_draw (GTK_WIDGET (self));

  return G_SOURCE_CONTINUE;
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

/* Функция проверяет параметры cairo_surface_t и пересоздает в случае необходимости. */
static void
hyscan_gtk_waterfall_prepare_csurface (cairo_surface_t  **surface,
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

/* Функция подготавливает структуру HyScanTile. */
static void
hyscan_gtk_waterfall_prepare_tile (HyScanGtkWaterfallPrivate *priv,
                                   HyScanTile                *tile)
{
  /* Тип КД и поворот для этого тайла. */
  if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    {
      if (tile->across_start <= 0 && tile->across_end <= 0)
        tile->source = priv->left_source;
      else
        tile->source = priv->right_source;

      tile->rotate = FALSE;
    }
  else /* HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER */
    {
      tile->source = priv->right_source;

      tile->rotate = TRUE;
    }

  tile->flags = priv->tile_flags;
  tile->upsample = priv->tile_upsample;
}

/* Функция получения тайла. */
static gboolean
hyscan_gtk_waterfall_get_tile (HyScanGtkWaterfall *self,
                               HyScanTile         *tile,
                               cairo_surface_t   **surface)
{
  HyScanGtkWaterfallPrivate *priv = self->priv;;

  HyScanTile requested_tile, queue_tile, color_tile;
  gboolean   queue_found, color_found;
  gboolean   regenerate;
  gint       show_strategy = SHOW_DUMMY;
  gfloat    *buffer        = NULL;
  guint32    buffer_size   = 0;
  HyScanTileSurface tile_surface;
  HyScanCancellable *cancellable = NULL;

  requested_tile = *tile;
  queue_found = color_found = regenerate = FALSE;

  /* Дописываем в структуру с тайлом всю необходимую информацию (из очереди). */
  hyscan_gtk_waterfall_prepare_tile (priv, &requested_tile);

  /* Собираем информацию об имеющихся тайлах и их тождественности. */
  queue_found = hyscan_tile_queue_check (priv->queue, &requested_tile, &queue_tile, &regenerate);
  color_found = hyscan_tile_color_check (priv->color, &requested_tile, &color_tile);

  /* На основании этого определяем, как поступить. */
  if (color_found && (color_tile.finalized || !queue_found))
    {
      show_strategy = SHOW;
      hyscan_gtk_waterfall_prepare_csurface (surface, color_tile.w, color_tile.h, &tile_surface);
    }
  else if (queue_found)
    {
      show_strategy = RECOLOR;
      hyscan_gtk_waterfall_prepare_csurface (surface, queue_tile.w, queue_tile.h, &tile_surface);
    }
  else
    {
      show_strategy = SHOW_DUMMY;
      regenerate = TRUE;
    }

  /* Ненайденные тайлы сразу же отправляем на генерацию. */
  if (!queue_found)
    {
      cancellable = hyscan_cancellable_new ();
      hyscan_tile_queue_add (priv->queue, &requested_tile, cancellable);
      g_object_unref (cancellable);
    }
  /* ПЕРЕгенерация разрешена не всегда. */
  else if (regenerate && priv->regen_allowed)
    {
      cancellable = hyscan_cancellable_new ();
      hyscan_tile_queue_add (priv->queue, &requested_tile, cancellable);
      g_object_unref (cancellable);
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
      return TRUE;

    case SHOW_DUMMY:
    default:
      return FALSE;
    }

  return TRUE;
}

/* Функция отлавливает сигнал "tile-queue-hash". */
static void
hyscan_gtk_waterfall_hash_changed (HyScanGtkWaterfall *self,
                                   gulong              hash)
{
  /* gsize -- это typedef unsigned long, поэтому я считаю безопасным
   * использовать GSIZE_TO_POINTER.
   */
  g_atomic_pointer_set (&self->priv->tq_hash, GSIZE_TO_POINTER (hash));
}

/* Функция асинхронно разукрашивает тайл. */
static void
hyscan_gtk_waterfall_image_generated (HyScanGtkWaterfall *self,
                                      HyScanTile         *tile,
                                      gfloat             *img,
                                      gint                size,
                                      gulong              hash)
{
  HyScanGtkWaterfallPrivate *priv = self->priv;
  HyScanTileSurface surface;
  gpointer tq_hash;

  tq_hash = g_atomic_pointer_get (&priv->tq_hash);

  if (hash != GPOINTER_TO_SIZE (tq_hash))
    return;

  surface.width = tile->w;
  surface.height = tile->h;
  surface.stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, surface.width);
  surface.data = g_malloc0 (surface.height * surface.stride);

  hyscan_tile_color_add (priv->color, tile, img, size, &surface);

  g_free (surface.data);
  hyscan_gtk_waterfall_queue_draw (self);
}

static void
hyscan_gtk_waterfall_changed (HyScanGtkWaterfall *self)
{
  HyScanGtkWaterfallPrivate *priv = self->priv;
  gdouble from_x, to_x, from_y, to_y;

  g_object_get (priv->hadjustment, "value", &from_x, "page-size", &to_x, NULL);
  g_object_get (priv->vadjustment, "value", &from_y, "page-size", &to_y, NULL);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (self), from_x, from_x + to_x, from_y, from_y + to_y);
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_visible_draw (GtkWidget *widget,
                                   cairo_t   *cairo)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = self->priv;

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
      hyscan_gtk_waterfall_dummy_draw (widget, cairo);
      return;
    }

  gtk_cifro_area_get_view	(GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);

  if (isnan (from_x)|| isnan (to_x)|| isnan (from_y)|| isnan (to_y))
    {
      g_warning ("Carea");
      return;
    }

  from_x *= 1000.0;
  to_x   *= 1000.0;
  from_y *= 1000.0;
  to_y   *= 1000.0;

  /* Определяем размер тайла. */
  scale = zooms_gost[priv->zoom_index];
  tile_size = tile_sizes[priv->zoom_index];

  /* Определяем параметры искомых тайлов. */
  start_tile_x0 = hyscan_gtk_waterfall_aligner (from_x, tile_size);
  start_tile_y0 = hyscan_gtk_waterfall_aligner (from_y, tile_size);
  end_tile_x0   = hyscan_gtk_waterfall_aligner (to_x,   tile_size);
  end_tile_y0   = hyscan_gtk_waterfall_aligner (to_y,   tile_size);

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
            case HYSCAN_WATERFALL_DISPLAY_SIDESCAN:
              tile.across_start = start_tile_x0 + j * tile_size;
              tile.along_start  = start_tile_y0 + i * tile_size;
              tile.across_end   = start_tile_x0 + (j + 1) * tile_size;
              tile.along_end    = start_tile_y0 + (i + 1) * tile_size;
              break;

            case HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER:
            default:
              tile.along_start  = start_tile_x0 + j * tile_size;
              tile.across_start = start_tile_y0 + i * tile_size;
              tile.along_end    = start_tile_x0 + (j + 1) * tile_size;
              tile.across_end   = start_tile_y0 + (i + 1) * tile_size;
            }

          /* Ищем тайл. */
          if (priv->open && hyscan_gtk_waterfall_get_tile (self, &tile, &(priv->surface)))
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
            hyscan_gtk_waterfall_draw_surface (source_surface, cairo, x_coord, y_coord);
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

  {
    gdouble from_x, to_x, from_y, to_y, min_x, max_x, min_y, max_y;
    gtk_cifro_area_get_view (GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
    gtk_cifro_area_get_limits (GTK_CIFRO_AREA (widget), &min_x, &max_x, &min_y, &max_y);

    g_signal_handlers_block_by_func (priv->hadjustment, hyscan_gtk_waterfall_changed, self);
    g_signal_handlers_block_by_func (priv->vadjustment, hyscan_gtk_waterfall_changed, self);

    gtk_adjustment_set_lower (priv->hadjustment, min_x);
    gtk_adjustment_set_upper (priv->hadjustment, max_x);
    gtk_adjustment_set_value (priv->hadjustment, from_x);
    gtk_adjustment_set_page_size (priv->hadjustment, ABS (to_x - from_x));

    gtk_adjustment_set_lower (priv->vadjustment, min_y);
    gtk_adjustment_set_upper (priv->vadjustment, max_y);
    gtk_adjustment_set_value (priv->vadjustment, from_y);
    gtk_adjustment_set_page_size (priv->vadjustment, ABS (to_y - from_y));

    g_signal_handlers_unblock_by_func (priv->hadjustment, hyscan_gtk_waterfall_changed, self);
    g_signal_handlers_unblock_by_func (priv->vadjustment, hyscan_gtk_waterfall_changed, self);
  }
}

/* Функция отрисовывает заглушку на весь виджет. */
static void
hyscan_gtk_waterfall_dummy_draw (GtkWidget *widget,
                                 cairo_t   *cairo)
{
  gint32 tile_size, start_x, start_y, end_x, end_y;
  gint num_of_tiles_x, num_of_tiles_y, i, j;
  gdouble from_x, from_y, to_x, to_y,
          x_coord, y_coord, x_coord0,
          y_coord0;
  gfloat step;
  gint tile_pixels;

  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = self->priv;

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
  start_x = hyscan_gtk_waterfall_aligner (from_x, tile_size);
  start_y = hyscan_gtk_waterfall_aligner (from_y, tile_size);
  end_x   = hyscan_gtk_waterfall_aligner (to_x,   tile_size);
  end_y   = hyscan_gtk_waterfall_aligner (to_y,   tile_size);
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

        hyscan_gtk_waterfall_draw_surface (priv->dummy, cairo, x_coord, y_coord);
      }
}

/* Вспомогательная функция отрисовки. */
static void
hyscan_gtk_waterfall_draw_surface (cairo_surface_t *src,
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

/* Функция создает заглушку. */
static void
hyscan_gtk_waterfall_create_dummy (HyScanGtkWaterfall *self)
{
  gfloat step;
  gint size, stride, i, j;
  guchar *data;

  HyScanGtkWaterfallPrivate *priv = self->priv;

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
hyscan_gtk_waterfall_configure (GtkWidget         *widget,
                                GdkEventConfigure *event)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = self->priv;
  GdkScreen *gdkscreen;
  GdkRectangle mon_geom;

  gint i = 0;
  gint monitor_num, monitor_h, monitor_w;
  gfloat old_ppi, ppi, diagonal_mm, diagonal_pix;

  /* Получаем монитор, на котором расположено окно. */
  gdkscreen = gdk_window_get_screen (event->window);
  monitor_num = gdk_screen_get_monitor_at_window (gdkscreen, event->window);

  /* Диагональ в пикселях. */
  gdk_screen_get_monitor_geometry (gdkscreen, monitor_num, &mon_geom);
  diagonal_pix = sqrt (mon_geom.width * mon_geom.width + mon_geom.height * mon_geom.height);

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
  hyscan_gtk_waterfall_create_dummy (self);

exit:

  gtk_cifro_area_get_size (GTK_CIFRO_AREA (widget), &priv->widget_width, &priv->widget_height);

  gtk_widget_queue_draw (widget);

  return FALSE;
}

/* Функция зуммирует изображение. */
static void
hyscan_gtk_waterfall_cifroarea_zoom (GtkCifroArea *carea,
                                     GtkCifroAreaZoomType direction_x,
                                     GtkCifroAreaZoomType direction_y,
                                     gdouble center_x,
                                     gdouble center_y)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (carea);
  HyScanGtkWaterfallPrivate *priv = self->priv;

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

  g_signal_emit (self, hyscan_gtk_waterfall_signals[SIGNAL_ZOOM], 0, priv->zoom_index, zooms_gost[priv->zoom_index]);

  if (priv->automove)
    gtk_cifro_area_move (carea, 0, G_MAXINT);
}

/* Функция проверки масштаба. */
static void
hyscan_gtk_waterfall_cifroarea_check_scale (GtkCifroArea *carea,
                                            gdouble      *scale_x,
                                            gdouble      *scale_y)
{
  gint i = 0, optimal = 0;
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (carea);
  HyScanGtkWaterfallPrivate *priv = self->priv;
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
  g_signal_emit (self, hyscan_gtk_waterfall_signals[SIGNAL_ZOOM], 0, priv->zoom_index, zooms_gost[priv->zoom_index]);
}

/* Функция проверки границ. */
static void
hyscan_gtk_waterfall_cifroarea_get_limits (GtkCifroArea *carea,
                                           gdouble      *min_x,
                                           gdouble      *max_x,
                                           gdouble      *min_y,
                                           gdouble      *max_y)
{
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (carea);
  HyScanGtkWaterfallPrivate *priv = self->priv;

  if (G_LIKELY (priv->init))
    {
      if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
        {
          *min_x = -priv->lwidth;
          *max_x = priv->rwidth;
          *min_y = 0;
          *max_y = priv->length;
        }
      if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)
        {
          *min_x = 0;
          *max_x = priv->length;
          *min_y = -priv->rwidth;
          *max_y = 0;
        }
    }
  else
    {
      *min_x = *min_y = -1;
      *max_x = *max_y = +1;
    }


  gtk_adjustment_set_lower (priv->hadjustment, *min_x);
  gtk_adjustment_set_upper (priv->hadjustment, *max_x);
  gtk_adjustment_set_lower (priv->vadjustment, *min_y);
  gtk_adjustment_set_upper (priv->vadjustment, *max_y);
}

/* Функция определяет, к какой границе прилипает изображение. */
static void
hyscan_gtk_waterfall_cifroarea_get_stick (GtkCifroArea          *carea,
                                          GtkCifroAreaStickType *stick_x,
                                          GtkCifroAreaStickType *stick_y)
{
  HyScanGtkWaterfallPrivate *priv;
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (carea);
  priv = self->priv;

  if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    {
      if (priv->init && priv->writeable)
        *stick_y = GTK_CIFRO_AREA_STICK_TOP;
      else
        *stick_y = GTK_CIFRO_AREA_STICK_CENTER;

      *stick_x = GTK_CIFRO_AREA_STICK_CENTER;
    }

  else /*if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)*/
    {
      if (priv->init && priv->writeable)
        *stick_x = GTK_CIFRO_AREA_STICK_RIGHT;
      else
        *stick_x = GTK_CIFRO_AREA_STICK_CENTER;

      *stick_y = GTK_CIFRO_AREA_STICK_TOP;
    }
}

/* Функция автосдвижки. */
static gboolean
hyscan_gtk_waterfall_automover (gpointer data)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (data);
  HyScanGtkWaterfall *self = HYSCAN_GTK_WATERFALL (data);
  HyScanGtkWaterfallPrivate *priv = self->priv;

  gdouble lwidth, rwidth;
  gdouble l_length = 0.0, r_length = 0.0, length = 0.0;
  gboolean l_writeable = FALSE, r_writeable = FALSE;
  gboolean writeable;
  gboolean l_init = FALSE, r_init = FALSE;

  if (!priv->open)
    return G_SOURCE_CONTINUE;

  /* Параметры галса. */
  if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    {
      l_init = hyscan_track_rect_get (priv->lrect, &l_writeable, &lwidth, &l_length);
      r_init = hyscan_track_rect_get (priv->rrect, &r_writeable, &rwidth, &r_length);
    }
  else /*if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)*/
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

  length = writeable ? MIN (l_length, r_length) : MAX (l_length, r_length);
  priv->lwidth = lwidth + 0.1;
  priv->rwidth = rwidth + 0.1;
  priv->writeable = writeable;

  if (G_UNLIKELY (!priv->once))
    {
      priv->length = length;
      gtk_cifro_area_set_view (carea, -lwidth, rwidth, 0, length);
      g_signal_emit (self, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, writeable);
      priv->once = TRUE;
    }

  /* Если данные уже есть, но галс в режиме чтения. */
  if (!writeable)
    {
      priv->length = length;
      priv->automove = FALSE;
      priv->auto_tag = 0;
      g_signal_emit (self, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, writeable);
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
        case HYSCAN_WATERFALL_DISPLAY_SIDESCAN:
          if (y1 + distance < length - priv->ship_speed - regen_gap)
            {
              y0 += distance;
              y1 += distance;
              priv->length = y0 < 0 ? y1 : length;
            }
          break;
        case HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER:
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

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return G_SOURCE_CONTINUE;
}

/* Функция обрабатывает смену типа отображения и источников. */
static void
hyscan_gtk_waterfall_sources_changed (HyScanGtkWaterfallState *model,
                                      HyScanGtkWaterfall   *self)
{
  HyScanGtkWaterfallPrivate *priv = self->priv;

  priv->widget_type = hyscan_gtk_waterfall_state_get_sources (model,
                                                              &priv->left_source,
                                                              &priv->right_source);

  hyscan_track_rect_set_source (priv->lrect, priv->left_source);
  hyscan_track_rect_set_source (priv->rrect, priv->right_source);
}

/* Функция обрабатывает смену типа тайлов. */
static void
hyscan_gtk_waterfall_tile_flags_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfall   *self)
{
  self->priv->tile_flags = hyscan_gtk_waterfall_state_get_tile_flags (model);
}

/* Функция обрабатывает смену профиля. */
static void
hyscan_gtk_waterfall_profile_changed (HyScanGtkWaterfallState *model,
                                      HyScanGtkWaterfall   *self)
{
}

/* Функция обрабатывает смену БД, проекта, галса. */
static void
hyscan_gtk_waterfall_track_changed (HyScanGtkWaterfallState *model,
                                    HyScanGtkWaterfall      *self)
{
  HyScanFactoryAmplitude *af;
  HyScanFactoryDepth *df;
  HyScanGtkWaterfallPrivate *priv = self->priv;
  HyScanDB *db;
  gchar *db_uri;
  gchar *project;
  gchar *track;

  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = 0;
    }

  priv->open = FALSE;

  hyscan_gtk_waterfall_state_get_track (model, &db, &project, &track);

  if (project == NULL || track == NULL)
    return;

  db_uri = hyscan_db_get_uri (db);

  af = hyscan_gtk_waterfall_state_get_amp_factory (model);
  df = hyscan_gtk_waterfall_state_get_dpt_factory (model);

  hyscan_factory_amplitude_set_track (af, db, project, track);
  hyscan_factory_depth_set_track (df, db, project, track);

  hyscan_tile_color_open (priv->color, db_uri, project, track);

  hyscan_tile_queue_amp_changed (priv->queue);
  hyscan_tile_queue_dpt_changed (priv->queue);
  hyscan_track_rect_amp_changed (priv->lrect);
  hyscan_track_rect_dpt_changed (priv->lrect);
  hyscan_track_rect_amp_changed (priv->rrect);
  hyscan_track_rect_dpt_changed (priv->rrect);

  priv->open = TRUE;

  priv->length = priv->lwidth = priv->rwidth = 0.0;

  priv->once = FALSE;
  priv->automove = TRUE;
  priv->prev_time = g_get_monotonic_time ();

  priv->auto_tag = g_timeout_add (priv->automove_time,
                                  hyscan_gtk_waterfall_automover,
                                  self);
  g_free (db_uri);
  g_free (project);
  g_free (track);
  g_object_unref (db);
  g_object_unref (af);
  g_object_unref (df);

  gtk_widget_queue_draw (GTK_WIDGET (self));

}

/* Функция обрабатывает смену скорости судна. */
static void
hyscan_gtk_waterfall_speed_changed (HyScanGtkWaterfallState *model,
                                    HyScanGtkWaterfall   *self)
{
  gfloat speed = hyscan_gtk_waterfall_state_get_ship_speed (model);

  hyscan_tile_queue_set_ship_speed (self->priv->queue, speed);
  hyscan_track_rect_set_ship_speed (self->priv->lrect, speed);
  hyscan_track_rect_set_ship_speed (self->priv->rrect, speed);

  self->priv->ship_speed = speed;
}

/* Функция обрабатывает смену профиля скорости звука. */
static void
hyscan_gtk_waterfall_velocity_changed (HyScanGtkWaterfallState *model,
                                       HyScanGtkWaterfall   *self)
{
  GArray *velocity;

  velocity = hyscan_gtk_waterfall_state_get_sound_velocity (model);

  hyscan_tile_queue_set_sound_velocity (self->priv->queue, velocity);
  hyscan_track_rect_set_sound_velocity (self->priv->lrect, velocity);
  hyscan_track_rect_set_sound_velocity (self->priv->rrect, velocity);

  if (velocity != NULL)
    g_array_unref (velocity);
}

/**
 * hyscan_gtk_waterfall_new:
 * @cache: объект #HyScanCache
 * Функция создает новый виджет #HyScanGtkWaterfall
 *
 * Returns: (transfer full): #HyScanGtkWaterfall
 */
GtkWidget*
hyscan_gtk_waterfall_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL,
                       "cache", cache, NULL);

}

/**
 * hyscan_gtk_waterfall_get_hadjustment:
 * @self: объект #HyScanGtkWaterfall
 *
 * Функция возвращает #GtkAdjustment, соответствующий горизонтальной оси.
 *
 * Returns: (transfer none): #GtkAdjustment
 */
GtkAdjustment *
hyscan_gtk_waterfall_get_hadjustment (HyScanGtkWaterfall *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), NULL);

  return self->priv->hadjustment;
}

/**
 * hyscan_gtk_waterfall_get_vadjustment:
 * @self: объект #HyScanGtkWaterfall
 *
 * Функция возвращает #GtkAdjustment, соответствующий вертикальной оси.
 *
 * Returns: (transfer none): #GtkAdjustment
 */
GtkAdjustment *
hyscan_gtk_waterfall_get_vadjustment (HyScanGtkWaterfall *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), NULL);

  return self->priv->vadjustment;
}

/**
 * hyscan_gtk_waterfall_queue_draw:
 * @wfall: объект #HyScanGtkWaterfall
 *
 * Функция потокобезопасно инициирует перерисовку виджета.
 */
void
hyscan_gtk_waterfall_queue_draw (HyScanGtkWaterfall *self)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (self));

  g_atomic_int_set (&self->priv->request_redraw, TRUE);
}

/**
 * hyscan_gtk_waterfall_set_upsample:
 * @wfall: объект #HyScanGtkWaterfall
 * @upsample: величина передискретизации
 *
 * Функция устанавливает величину передискретизации.
 */
void
hyscan_gtk_waterfall_set_upsample (HyScanGtkWaterfall *self,
                                   gint                upsample)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (self));

  self->priv->tile_upsample = upsample;
}

/**
 * hyscan_gtk_waterfall_set_colormap:
 * @wfall: объект #HyScanGtkWaterfall
 * @source: тип источника, для которого будет применена цветовая схема
 * @colormap: (in) (array length=length): цветовая схема
 * @length: количество элементов в цветовой схеме
 * @background: цвет фона
 *
 * Функция обновляет цветовую схему объекта #HyScanTileColor.
 *
 * Returns: TRUE, если параметры успешно скопированы.
 */
gboolean
hyscan_gtk_waterfall_set_colormap (HyScanGtkWaterfall *self,
                                   HyScanSourceType    source,
                                   guint32            *colormap,
                                   guint               length,
                                   guint32             background)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return hyscan_tile_color_set_colormap (self->priv->color, source, colormap, length, background);
}

/**
 * hyscan_gtk_waterfall_set_colormap_for_all:
 * @wfall: объект #HyScanGtkWaterfall
 * @colormap: (in) (array length=length): цветовая схема
 * @length: количество элементов в цветовой схеме
 * @background: цвет фона
 *
 * Функция обновляет цветовую схему объекта #HyScanTileColor.
 *
 * Returns: TRUE, если параметры успешно скопированы.
 */
gboolean
hyscan_gtk_waterfall_set_colormap_for_all (HyScanGtkWaterfall *self,
                                           guint32            *colormap,
                                           guint               length,
                                           guint32             background)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return hyscan_tile_color_set_colormap_for_all (self->priv->color, colormap, length, background);
}

/**
 * hyscan_gtk_waterfall_set_levels:
 * @wfall: объект #HyScanGtkWaterfall
 * @source: тип источника, для которого будет применена цветовая схема
 * @black: уровень черной точки
 * @gamma: гамма
 * @white: уровень белой точки
 *
 * Функция обновляет параметры объекта #HyScanTileColor.
 *
 * Returns: TRUE, если параметры успешно скопированы.
 */
gboolean
hyscan_gtk_waterfall_set_levels (HyScanGtkWaterfall *self,
                                 HyScanSourceType    source,
                                 gdouble             black,
                                 gdouble             gamma,
                                 gdouble             white)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return hyscan_tile_color_set_levels (self->priv->color, source, black, gamma, white);
}

/**
 * hyscan_gtk_waterfall_set_levels_for_all:
 * @wfall: объект #HyScanGtkWaterfall
 * @black: уровень черной точки
 * @gamma: гамма
 * @white: уровень белой точки
 *
 * Функция обновляет параметры объекта #HyScanTileColor.
 *
 * Returns: TRUE, если параметры успешно скопированы.
 */
gboolean
hyscan_gtk_waterfall_set_levels_for_all (HyScanGtkWaterfall *self,
                                         gdouble             black,
                                         gdouble             gamma,
                                         gdouble             white)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return hyscan_tile_color_set_levels_for_all (self->priv->color, black, gamma, white);
}

/**
 * hyscan_gtk_waterfall_get_scale:
 * @wfall: объект #HyScanGtkWaterfall
 * @zooms: (out) (optional): указатель на массив масштабов
 * @num: (out) (optional): число масштабов
 *
 * Функция возвращает масштабы и индекс текущего масштаба.
 *
 * Returns: номер масштаба; -1 в случае ошибки.
 */
gint
hyscan_gtk_waterfall_get_scale (HyScanGtkWaterfall *self,
                                const gdouble     **zooms,
                                gint                *num)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), -1);

  if (zooms != NULL)
    *zooms = zooms_gost;
  if (num != NULL)
    *num = ZOOM_LEVELS;

  return self->priv->zoom_index;
}

/**
 * hyscan_gtk_waterfall_automove:
 * @wfall: объект #HyScanGtkWaterfall
 * @automove: TRUE для включения и FALSE для отключения автосдвижки
 *
 * Функция включает и выключает автосдвижку изображения.
 */
gboolean
hyscan_gtk_waterfall_automove (HyScanGtkWaterfall *self,
                               gboolean            automove)
{
  HyScanGtkWaterfallPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (self), FALSE);
  priv = self->priv;

  if (priv->automove == automove)
    return automove;

  if (priv->auto_tag == 0)
    return FALSE;

  priv->automove = automove;

  if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    gtk_cifro_area_move (GTK_CIFRO_AREA (self), 0, G_MAXINT);
  else /*if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER) */
    gtk_cifro_area_move (GTK_CIFRO_AREA (self), G_MAXINT, 0);

  g_signal_emit (self, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, automove);
  return automove;
}

/**
 * hyscan_gtk_waterfall_set_automove_period:
 * @wfall: объект #HyScanGtkWaterfall
 * @usecs: время в микросекундах между сдвижками
 *
 * Функция устанавливает период автосдвижки.
 */
void
hyscan_gtk_waterfall_set_automove_period (HyScanGtkWaterfall *self,
                                          gint64              usecs)
{
  HyScanGtkWaterfallPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (self));
  priv = self->priv;

  priv->automove_time = usecs / G_TIME_SPAN_MILLISECOND;

  if (!priv->open)
    return;

  if (priv->auto_tag != 0)
    {
      g_source_remove (priv->auto_tag);
      priv->auto_tag = 0;
    }

  priv->auto_tag = g_timeout_add (priv->automove_time,
                                  hyscan_gtk_waterfall_automover,
                                  self);
}

/**
 * hyscan_gtk_waterfall_set_regeneration_period:
 * @wfall: объект #HyScanGtkWaterfall
 * @usecs: время в микросекундах между перегенерацией
 *
 * Функция устанавливает период перегенерации тайлов.
 */
void
hyscan_gtk_waterfall_set_regeneration_period (HyScanGtkWaterfall *self,
                                              gint64              usecs)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (self));

  self->priv->regen_period = usecs;
}

/**
 * hyscan_gtk_waterfall_set_substrate:
 * @wfall: объект #HyScanGtkWaterfall
 * @substrate: цвет подложки
 *
 * Функция устанавливает цвет подложки.
 */
void
hyscan_gtk_waterfall_set_substrate (HyScanGtkWaterfall *self,
                                    guint32             substrate)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (self));

  self->priv->dummy_color = substrate;
  hyscan_gtk_waterfall_create_dummy (self);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * hyscan_gtk_waterfall_make_grid:
 * @wfall: виджет #HyScanGtkWaterfall
 *
 * Функция упаковывает виджет в #GtkGrid и добавляет вертикальную прокрутку.
 */
GtkWidget *
hyscan_gtk_waterfall_make_grid (HyScanGtkWaterfall *wfall)
{
  GtkWidget * container;
  GtkWidget * hscroll, * vscroll;
  GtkAdjustment * hadj, * vadj;

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (wfall), NULL);

  container = gtk_grid_new ();
  hadj = hyscan_gtk_waterfall_get_hadjustment (wfall);
  vadj = hyscan_gtk_waterfall_get_vadjustment (wfall);

  hscroll = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadj);
  vscroll = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadj);
  gtk_range_set_inverted (GTK_RANGE (vscroll), TRUE);

  gtk_widget_set_hexpand (GTK_WIDGET (wfall), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (wfall), TRUE);

  gtk_grid_attach (GTK_GRID (container), GTK_WIDGET (wfall),   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (container), hscroll, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (container), vscroll, 1, 0, 1, 1);

  return container;
}
