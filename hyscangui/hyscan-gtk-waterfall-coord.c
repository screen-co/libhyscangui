/* hyscan-gtk-waterfall-mark.c
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
 * SECTION: hyscan-gtk-waterfall-mark
 * @Title HyScanGtkWaterfallCoord
 * @Short_description Слой меток водопада
 *
 * Слой HyScanGtkWaterfallCoord предназначен для отображения, создания,
 * редактирования и удаления меток.
 */

#include "hyscan-gtk-waterfall-tools.h"
#include "hyscan-gtk-waterfall.h"
#include "hyscan-gtk-waterfall-coord.h"
#include <hyscan-mloc.h>
#include <hyscan-projector.h>
#include <math.h>

enum
{
  SIGNAL_SELECTED,
  SIGNAL_LAST
};

/* Стейты. */
typedef struct
{
  HyScanWaterfallDisplayType  display_type;
  HyScanSourceType            lsource;
  HyScanSourceType            rsource;
  gboolean                    sources_changed;

  HyScanTileFlags             tile_flags;
  gboolean                    tile_flags_changed;

  HyScanDB                   *db;
  gchar                      *project;
  gchar                      *track;
  gboolean                    amp_changed;

  gfloat                      ship_speed;
  gboolean                    speed_changed;

  GArray                     *velocity;
  gboolean                    velocity_changed;

  gboolean                    dpt_changed;
} HyScanGtkWaterfallCoordState;

struct _HyScanGtkWaterfallCoordPrivate
{
  HyScanGtkWaterfall         *wfall;

  gboolean                    layer_visibility;

  HyScanGtkWaterfallCoordState new_state;
  HyScanGtkWaterfallCoordState state;
  gboolean                    state_changed;
  GMutex                      state_lock;

  GThread                    *processing;
  gint                        stop;
  gint                        cond_flag;
  GCond                       cond;

  HyScanCoordinates           click;
  gboolean                    click_set;
  HyScanGeoGeodetic           click_out;
  gboolean                    click_out_set;

  struct
  {
    GdkRGBA                   main;
    GdkRGBA                   shadow;
    gdouble                   shadow_width;
    gdouble                   main_width;
  } color;

  PangoLayout      *font;              /* Раскладка шрифта. */
  gint              text_height;
};

static void     hyscan_gtk_waterfall_coord_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_waterfall_coord_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_coord_object_finalize         (GObject                 *object);

static void     hyscan_gtk_waterfall_coord_added                   (HyScanGtkLayer          *layer,
                                                                   HyScanGtkLayerContainer *container);
static void     hyscan_gtk_waterfall_coord_removed                 (HyScanGtkLayer          *iface);
static gboolean hyscan_gtk_waterfall_coord_grab_input              (HyScanGtkLayer          *iface);
static void     hyscan_gtk_waterfall_coord_set_visible             (HyScanGtkLayer          *iface,
                                                                   gboolean                 visible);
static const gchar * hyscan_gtk_waterfall_coord_get_icon_name      (HyScanGtkLayer          *iface);

static void     hyscan_gtk_waterfall_coord_clear_state             (HyScanGtkWaterfallCoordState  *state);
static void     hyscan_gtk_waterfall_coord_sync_states             (HyScanGtkWaterfallCoord  *self);
static gpointer hyscan_gtk_waterfall_coord_processing              (gpointer                 data);

static gboolean hyscan_gtk_waterfall_coord_key                     (GtkWidget               *widget,
                                                                    GdkEventKey             *event,
                                                                    HyScanGtkWaterfallCoord *self);
static gboolean hyscan_gtk_waterfall_coord_handle_create           (HyScanGtkLayer          *layer,
                                                                   GdkEventButton          *event);

static void     hyscan_gtk_waterfall_coord_draw                    (GtkWidget               *widget,
                                                                   cairo_t                 *cairo,
                                                                   HyScanGtkWaterfallCoord  *self);

static gboolean hyscan_gtk_waterfall_coord_configure               (GtkWidget               *widget,
                                                                  GdkEventConfigure       *event,
                                                                  HyScanGtkWaterfallCoord  *self);

static void     hyscan_gtk_waterfall_coord_sources_changed         (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallCoord  *self);
static void     hyscan_gtk_waterfall_coord_tile_flags_changed       (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallCoord  *self);
static void     hyscan_gtk_waterfall_coord_track_changed           (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallCoord  *self);
static void     hyscan_gtk_waterfall_coord_ship_speed_changed      (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallCoord  *self);
static void     hyscan_gtk_waterfall_coord_sound_velocity_changed  (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallCoord  *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallCoord, hyscan_gtk_waterfall_coord, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallCoord)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_coord_interface_init));

static void
hyscan_gtk_waterfall_coord_class_init (HyScanGtkWaterfallCoordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_coord_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_coord_object_finalize;

}

static void
hyscan_gtk_waterfall_coord_init (HyScanGtkWaterfallCoord *self)
{
  self->priv = hyscan_gtk_waterfall_coord_get_instance_private (self);
}


static void
hyscan_gtk_waterfall_coord_object_constructed (GObject *object)
{
  GdkRGBA text, shadow;
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (object);
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_coord_parent_class)->constructed (object);

  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->cond);

  /* Сигналы Gtk. */
  gdk_rgba_parse (&text, "#0496ff");
  gdk_rgba_parse (&shadow, SHADOW_DEFAULT);

  hyscan_gtk_waterfall_coord_set_main_color (self, text);
  hyscan_gtk_waterfall_coord_set_shadow_color (self, shadow);
  hyscan_gtk_waterfall_coord_set_shadow_width (self, 3);
  hyscan_gtk_waterfall_coord_set_main_width (self, 1);

  priv->stop = FALSE;
  priv->processing = g_thread_new ("gtk-wf-mark", hyscan_gtk_waterfall_coord_processing, self);

  /* Включаем видимость слоя. */
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_coord_object_finalize (GObject *object)
{
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (object);
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  g_atomic_int_set (&priv->stop, TRUE);
  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
  g_thread_join (priv->processing);

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  g_mutex_clear (&priv->state_lock);

  g_cond_clear (&priv->cond);
  g_clear_object (&priv->font);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_coord_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_coord_added (HyScanGtkLayer          *layer,
                                 HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfallState *wfall;
  HyScanGtkWaterfallCoord *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_COORD (layer);
  wfall = HYSCAN_GTK_WATERFALL_STATE (container);
  self->priv->wfall = g_object_ref (wfall);

  /* Сигналы Gtk. */
  g_signal_connect (wfall, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_coord_draw), self);
  g_signal_connect (wfall, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_coord_configure), self);
  // g_signal_connect_after (wfall, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_coord_motion), self);
  g_signal_connect_after (wfall, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_coord_key), self);

  /* Сигналы водопада. */
  g_signal_connect (wfall, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_coord_sources_changed), self);
  g_signal_connect (wfall, "changed::tile-flags",   G_CALLBACK (hyscan_gtk_waterfall_coord_tile_flags_changed), self);
  g_signal_connect (wfall, "changed::track",        G_CALLBACK (hyscan_gtk_waterfall_coord_track_changed), self);
  g_signal_connect (wfall, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_coord_ship_speed_changed), self);
  g_signal_connect (wfall, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_coord_sound_velocity_changed), self);
  // g_signal_connect (wfall, "changed::amp-changed",  G_CALLBACK (hyscan_gtk_waterfall_coord_depth_amp_changed), self);
  // g_signal_connect (wfall, "changed::dpt-changed",  G_CALLBACK (hyscan_gtk_waterfall_coord_depth_dpt_changed), self);

  hyscan_gtk_waterfall_coord_sources_changed (wfall, self);
  hyscan_gtk_waterfall_coord_tile_flags_changed (wfall, self);
  hyscan_gtk_waterfall_coord_track_changed (wfall, self);
  hyscan_gtk_waterfall_coord_ship_speed_changed (wfall, self);
  hyscan_gtk_waterfall_coord_sound_velocity_changed (wfall, self);
}

static void
hyscan_gtk_waterfall_coord_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

/* Функция захватывает ввод. */
static gboolean
hyscan_gtk_waterfall_coord_grab_input (HyScanGtkLayer *iface)
{
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (iface);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!self->priv->layer_visibility)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), self);
  hyscan_gtk_layer_container_set_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), TRUE);
  return TRUE;
}

/* Функция захватывает ввод. */
static void
hyscan_gtk_waterfall_coord_set_visible (HyScanGtkLayer *iface,
                                       gboolean        visible)
{
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (iface);

  self->priv->layer_visibility = visible;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция возвращает название иконки. */
static const gchar *
hyscan_gtk_waterfall_coord_get_icon_name (HyScanGtkLayer *iface)
{
  return "mark-location-symbolic";
}

/* Функция создает новый HyScanProjector. */
static HyScanProjector*
hyscan_gtk_waterfall_coord_open_projector (HyScanGtkWaterfallCoord *self,
                                          const gchar            *track,
                                          HyScanSourceType        source)
{
  HyScanProjector *projector;
  HyScanFactoryAmplitude *af;
  HyScanAmplitude *dc;

  af = hyscan_gtk_waterfall_state_get_amp_factory (HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall));
  dc = hyscan_factory_amplitude_produce (af, track, source);
  projector = hyscan_projector_new (dc);

  g_clear_object (&af);
  g_clear_object (&dc);
  return projector;
}

/* Функция очищает HyScanGtkWaterfallCoordState. */
static void
hyscan_gtk_waterfall_coord_clear_state (HyScanGtkWaterfallCoordState *state)
{
  g_clear_object (&state->db);
  g_clear_pointer (&state->project, g_free);
  g_clear_pointer (&state->track, g_free);
  g_clear_pointer (&state->velocity, g_array_unref);
}

/* Функция синхронизирует состояния. */
static void
hyscan_gtk_waterfall_coord_sync_states (HyScanGtkWaterfallCoord *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  HyScanGtkWaterfallCoordState *new_st = &priv->new_state;
  HyScanGtkWaterfallCoordState *cur_st = &priv->state;

  if (new_st->sources_changed)
    {
      cur_st->display_type = new_st->display_type;
      cur_st->lsource = new_st->lsource;
      cur_st->rsource = new_st->rsource;

      new_st->sources_changed = FALSE;
      cur_st->sources_changed = TRUE;
    }
  if (new_st->amp_changed)
    {
      /* Очищаем текущее. */
      g_clear_object (&cur_st->db);
      g_clear_pointer (&cur_st->project, g_free);
      g_clear_pointer (&cur_st->track, g_free);

      /* Копируем из нового. */
      cur_st->db = new_st->db;
      cur_st->project = new_st->project;
      cur_st->track = new_st->track;

      /* Очищаем новое. */
      new_st->db = NULL;
      new_st->project = NULL;
      new_st->track = NULL;

      /* Выставляем флаги. */
      new_st->amp_changed = FALSE;
      cur_st->amp_changed = TRUE;
    }
  if (new_st->dpt_changed)
    {
      new_st->dpt_changed = FALSE;
      cur_st->dpt_changed = TRUE;
    }

  if (new_st->speed_changed)
    {
      cur_st->ship_speed = new_st->ship_speed;

      cur_st->speed_changed = TRUE;
      new_st->speed_changed = FALSE;
    }
  if (new_st->velocity_changed)
    {
      if (cur_st->velocity != NULL)
        g_clear_pointer (&cur_st->velocity, g_array_unref);

      cur_st->velocity = new_st->velocity;

      new_st->velocity = NULL;

      cur_st->velocity_changed = TRUE;
      new_st->velocity_changed = FALSE;
    }
}

/* Поток получения и отправки меток. */
static gpointer
hyscan_gtk_waterfall_coord_processing (gpointer data)
{
  HyScanGtkWaterfallCoord *self = data;
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  HyScanProjector *lproj = NULL;
  HyScanProjector *rproj = NULL;
  HyScanProjector *_proj = NULL;
  HyScanDepthometer *depth = NULL;
  HyScanGeoGeodetic position;
  HyScanmLoc *mloc = NULL;

  gchar *track_id = NULL;
  GMutex cond_mutex;
  guint32 index0, n;
  HyScanGtkWaterfallCoordState *state = &priv->state;
  HyScanAmplitude *amp;
  gint64 time;
  HyScanAntennaOffset apos;

  g_mutex_init (&cond_mutex);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Ждем сигнализации об изменениях. */
      if (g_atomic_int_get (&priv->cond_flag) == 0)
        {
          gboolean triggered;
          gint64 end = g_get_monotonic_time () + G_TIME_SPAN_MILLISECOND * 250;

          g_mutex_lock (&cond_mutex);
          triggered = g_cond_wait_until (&priv->cond, &cond_mutex, end);
          g_mutex_unlock (&cond_mutex);

          /* Если наступил таймаут, повторяем заново. */
          if (!triggered)
            continue;
        }

      g_atomic_int_set (&priv->cond_flag, 0);

      /* Проверяем, требуется ли синхронизация. */
      if (g_atomic_int_get (&priv->state_changed))
        {
          /* Синхронизация. */
          g_mutex_lock (&priv->state_lock);
          hyscan_gtk_waterfall_coord_sync_states (self);
          g_mutex_unlock (&priv->state_lock);

          /* Применяем обновления. */
          if (state->sources_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              g_clear_object (&mloc);
              state->sources_changed = FALSE;
            }
          if (state->amp_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              g_clear_object (&mloc);
              g_clear_object (&depth);
              g_clear_pointer (&track_id, g_free);
              state->amp_changed = FALSE;
            }

          if (state->dpt_changed)
            {
              g_clear_object (&depth);
              state->dpt_changed = FALSE;
            }
          if (state->speed_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              state->speed_changed = FALSE;
            }
          if (state->velocity_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              state->velocity_changed = FALSE;
            }

          g_atomic_int_set (&priv->state_changed, FALSE);

          if (lproj == NULL)
            {
              lproj = hyscan_gtk_waterfall_coord_open_projector (self, state->track, state->lsource);
              if (lproj != NULL)
                {
                  hyscan_projector_set_ship_speed (lproj, state->ship_speed);
                  hyscan_projector_set_sound_velocity (lproj, state->velocity);
                }
            }
          if (rproj == NULL)
            {
              rproj = hyscan_gtk_waterfall_coord_open_projector (self, state->track, state->rsource);
              if (rproj != NULL)
                {
                  hyscan_projector_set_ship_speed (rproj, state->ship_speed);
                  hyscan_projector_set_sound_velocity (rproj, state->velocity);
                }
            }
          if (depth == NULL)
            {
              HyScanFactoryDepth *df;
              df = hyscan_gtk_waterfall_state_get_dpt_factory (HYSCAN_GTK_WATERFALL_STATE (priv->wfall));
              depth = hyscan_factory_depth_produce (df, state->track);
              g_object_unref (df);
            }

          if (mloc == NULL)
            {
              mloc = hyscan_mloc_new (state->db, NULL, state->project, state->track);
            }
        }

      /* Если в результате синхронизации нет объектов, с которыми можно иметь
       * дело, возвращаемся в начало. */
      if (rproj == NULL || lproj == NULL || mloc == NULL)
        continue;

      /* Обрабатываем. */
      if (!g_atomic_int_get (&priv->click_set))
        continue;

      if (priv->click.x >= 0)
        _proj = rproj;
      else
        _proj = lproj;

      hyscan_projector_coord_to_index (_proj, priv->click.y, &index0);

      amp = hyscan_projector_get_amplitude (_proj);
      apos = hyscan_amplitude_get_offset (amp);
      hyscan_amplitude_get_amplitude (amp, NULL, index0, &n, &time, NULL);
      g_object_unref (amp);
      if (!hyscan_mloc_get (mloc, NULL, time, &apos, 0, priv->click.x, 0, &position))
        continue;

      priv->click_out = position;
      g_atomic_int_set (&priv->click_out_set, TRUE);

      if (priv->wfall != NULL)
        hyscan_gtk_waterfall_queue_draw (priv->wfall);
    }

  hyscan_gtk_waterfall_coord_clear_state (&priv->state);
  hyscan_gtk_waterfall_coord_clear_state (&priv->new_state);
  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&depth);
  g_clear_pointer (&track_id, g_free);

  return NULL;
}

/* Функция обрабатывает нажатия клавиш. */
static gboolean
hyscan_gtk_waterfall_coord_key (GtkWidget               *widget,
                                GdkEventKey             *event,
                                HyScanGtkWaterfallCoord *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  if (self != hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)))
    return GDK_EVENT_PROPAGATE;

  /* Удаление. */
  if (event->keyval == GDK_KEY_Delete)
    {
      g_atomic_int_set (&priv->click_set, FALSE);
      hyscan_gtk_waterfall_queue_draw (priv->wfall);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

gboolean
hyscan_gtk_waterfall_coord_handle_create (HyScanGtkLayer *layer,
                                         GdkEventButton *event)
{
  HyScanGtkWaterfallCoord *self = HYSCAN_GTK_WATERFALL_COORD (layer);
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  /* Обязательные проверки: слой не владеет вводом, редактирование запрещено, слой скрыт. */
  if (self != hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)))
    return FALSE;

  /* Координаты центра. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (priv->wfall),
                                         event->x, event->y,
                                         &priv->click.x,
                                         &priv->click.y);
  g_atomic_int_set (&priv->click_set, TRUE);
  g_atomic_int_set (&priv->click_out_set, FALSE);
  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);

  hyscan_gtk_waterfall_queue_draw (priv->wfall);
  return TRUE;
}

/* Функция отрисовки хэндла. */
void
hyscan_gtk_waterfall_coord_draw_handle (cairo_t          *cairo,
                                        GdkRGBA           inner,
                                        GdkRGBA           outer,
                                        HyScanCoordinates center,
                                        gdouble           radius)
{
  cairo_pattern_t *pttrn;
  cairo_matrix_t matrix;

  pttrn = hyscan_gtk_waterfall_tools_make_handle_pattern (radius, inner, outer);

  cairo_matrix_init_translate (&matrix, -center.x, -center.y);
  cairo_pattern_set_matrix (pttrn, &matrix);

  cairo_set_source (cairo, pttrn);
  cairo_arc (cairo, center.x, center.y, radius, 0, G_PI * 2);
  cairo_fill (cairo);

  cairo_pattern_destroy (pttrn);
}

/* Вспомогательная функция отрисовки. */
static void
hyscan_gtk_waterfall_coord_draw_helper (cairo_t           *cairo,
                                        HyScanCoordinates *point,
                                        GdkRGBA           *color,
                                        gdouble            width)
{
  hyscan_cairo_set_source_gdk_rgba (cairo, color);

  cairo_arc (cairo, point->x, point->y, width, 0, 2 * G_PI);
  cairo_fill (cairo);
}

/* Функция отрисовки одной метки.*/
static gboolean
hyscan_gtk_waterfall_coord_draw_task (HyScanGtkWaterfallCoord *self,
                                      cairo_t                 *cairo,
                                      HyScanGeoGeodetic        human,
                                      HyScanCoordinates        machine,
                                      gboolean                 ready)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  PangoLayout *font = priv->font;

  /* Рисуем тень под элементами. */
  hyscan_gtk_waterfall_coord_draw_helper (cairo, &machine, &priv->color.shadow, priv->color.shadow_width);

  if (!ready)
    return TRUE;

  hyscan_gtk_waterfall_coord_draw_helper (cairo, &machine, &priv->color.main, priv->color.main_width);

  {
    gint w, h;
    gchar * text = g_strdup_printf ("%f°%s, %f°%s",
                                    human.lat,
                                    human.lat > 0 ? "N" : "S",
                                    human.lon,
                                    human.lon > 0 ? "E" : "W");

    pango_layout_set_text (font, text, -1);
    pango_layout_get_size (font, &w, &h);

    w /= PANGO_SCALE;
    h /= PANGO_SCALE;

    hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.shadow);
    cairo_rectangle (cairo, machine.x - w / 2.0, machine.y + priv->color.shadow_width, w, h);
    cairo_fill (cairo);
    hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.main);
    cairo_rectangle (cairo, machine.x - w / 2.0, machine.y + priv->color.shadow_width, w, h);
    cairo_stroke (cairo);

    cairo_move_to (cairo, machine.x - w / 2.0, machine.y + priv->color.shadow_width);

    hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.main);
    pango_cairo_show_layout (cairo, font);
  }

  cairo_stroke (cairo);
  return TRUE;
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_coord_draw (GtkWidget              *widget,
                                cairo_t                *cairo,
                                HyScanGtkWaterfallCoord *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  /* Проверяем видимость слоя. */
  if (!priv->layer_visibility)
    return;

  cairo_save (cairo);

  /* Отрисовываем current метку. */
  if (g_atomic_int_get (&priv->click_set))
    {
      HyScanCoordinates machine;
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                             &machine.x,
                                             &machine.y,
                                             priv->click.x,
                                             priv->click.y);
      hyscan_gtk_waterfall_coord_draw_task (self, cairo,
                                            priv->click_out,
                                            machine,
                                            g_atomic_int_get (&priv->click_out_set));
    }
  cairo_restore (cairo);
}

/* Функция обрабатывает смену параметров виджета. */
static gboolean
hyscan_gtk_waterfall_coord_configure (GtkWidget              *widget,
                                     GdkEventConfigure      *event,
                                     HyScanGtkWaterfallCoord *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  gint text_height;

  /* Текущий шрифт приложения. */
  g_clear_pointer (&priv->font, g_object_unref);
  priv->font = gtk_widget_create_pango_layout (widget, NULL);

  pango_layout_set_text (priv->font,
                         "0123456789"
                         "abcdefghijklmnopqrstuvwxyz"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         ".,?!_-+=/\\", -1);

  pango_layout_get_size (priv->font, NULL, &text_height);

  priv->text_height = text_height / PANGO_SCALE;

  return FALSE;
}

/* Функция обрабатывает смену типа отображения и источников. */
static void
hyscan_gtk_waterfall_coord_sources_changed (HyScanGtkWaterfallState *model,
                                           HyScanGtkWaterfallCoord  *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.display_type = hyscan_gtk_waterfall_state_get_sources (model,
                                                                         &priv->new_state.lsource,
                                                                         &priv->new_state.rsource);
  priv->new_state.sources_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену типа тайлов. */
static void
hyscan_gtk_waterfall_coord_tile_flags_changed (HyScanGtkWaterfallState *model,
                                              HyScanGtkWaterfallCoord  *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  priv->new_state.tile_flags = hyscan_gtk_waterfall_state_get_tile_flags (model);
  priv->new_state.tile_flags_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену БД, проекта, галса. */
static void
hyscan_gtk_waterfall_coord_track_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfallCoord  *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  g_clear_object (&priv->new_state.db);
  g_clear_pointer (&priv->new_state.project, g_free);
  g_clear_pointer (&priv->new_state.track, g_free);
  hyscan_gtk_waterfall_state_get_track (model,
                                       &priv->new_state.db,
                                       &priv->new_state.project,
                                       &priv->new_state.track);
  priv->new_state.amp_changed = TRUE;

  g_atomic_int_set (&priv->click_set, FALSE);
  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену скорости судна. */
static void
hyscan_gtk_waterfall_coord_ship_speed_changed (HyScanGtkWaterfallState *model,
                                              HyScanGtkWaterfallCoord  *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.ship_speed = hyscan_gtk_waterfall_state_get_ship_speed (model);
  priv->new_state.speed_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену профиля скорости звука. */
static void
hyscan_gtk_waterfall_coord_sound_velocity_changed (HyScanGtkWaterfallState *model,
                                                  HyScanGtkWaterfallCoord  *self)
{
  HyScanGtkWaterfallCoordPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  priv->new_state.velocity = hyscan_gtk_waterfall_state_get_sound_velocity (model);
  priv->new_state.velocity_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/**
 * hyscan_gtk_waterfall_coord_new:
 * @markmodel: указатель на #HyScanObjectModel
 *
 * Функция создает новый объект #HyScanGtkWaterfallCoord.
 *
 * Returns: (transfer full): новый объект HyScanGtkWaterfallCoord.
 */
HyScanGtkWaterfallCoord*
hyscan_gtk_waterfall_coord_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_COORD, NULL);
}

/**
 * hyscan_gtk_waterfall_coord_set_main_color:
 * @mark: указатель на объект #HyScanGtkWaterfallCoord
 * @color: основной цвет
 *
 * Функция задает основной цвет.
 * Влияет на цвет меток и подписей.
 */
void
hyscan_gtk_waterfall_coord_set_main_color (HyScanGtkWaterfallCoord *self,
                                          GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_COORD (self));

  self->priv->color.main = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

void
hyscan_gtk_waterfall_coord_set_main_width (HyScanGtkWaterfallCoord *self,
                                           gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_COORD (self));

  self->priv->color.main_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_coord_set_shadow_color:
 * @mark: указатель на объект #HyScanGtkWaterfallCoord
 * @color: цвет подложки
 *
 * Функция задает цвет подложки.
 * Подложка рисуется под текстом и линиями метки.
 */
void
hyscan_gtk_waterfall_coord_set_shadow_color (HyScanGtkWaterfallCoord *self,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_COORD (self));

  self->priv->color.shadow = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_coord_set_shadow_width:
 * @mark: указатель на объект #HyScanGtkWaterfallCoord
 * @width: ширина линий
 *
 * Функция задает ширину линий подложки.
 */
void
hyscan_gtk_waterfall_coord_set_shadow_width (HyScanGtkWaterfallCoord *self,
                                            gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_COORD (self));

  self->priv->color.shadow_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}


static void
hyscan_gtk_waterfall_coord_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_coord_added;
  iface->removed = hyscan_gtk_waterfall_coord_removed;
  iface->grab_input = hyscan_gtk_waterfall_coord_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_coord_set_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_coord_get_icon_name;
  iface->handle_create = hyscan_gtk_waterfall_coord_handle_create;
  // iface->handle_release = hyscan_gtk_waterfall_coord_handle_release;
  // iface->handle_find = hyscan_gtk_waterfall_coord_handle_find;
  // iface->handle_show = hyscan_gtk_waterfall_coord_handle_show;
  // iface->handle_click = hyscan_gtk_waterfall_coord_handle_click;
}
