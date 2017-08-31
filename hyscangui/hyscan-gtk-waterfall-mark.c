#include <hyscan-depthometer.h>
#include <hyscan-depth-acoustic.h>
#include <hyscan-depth-nmea.h>
#include "hyscan-gtk-waterfall-mark.h"
#include <hyscan-mark-manager.h>
#include <hyscan-projector.h>
#include <math.h>

struct Coord
{
  gdouble x;
  gdouble y;
};

typedef struct
{
  HyScanWaterfallMark *mark;
  gdouble              x;
  gdouble              y;
  gdouble              r;
} HyScanGtkWaterfallMarkTask;

enum
{
  PROP_WATERFALL = 1
};

typedef struct
{
  HyScanWaterfallDisplayType  display_type;
  HyScanSourceType            lsource;
  HyScanSourceType            rsource;
  gboolean                    sources_changed;

  HyScanTileType              tile_type;
  gboolean                    tile_type_changed;

  gchar                      *profile;
  gboolean                    profile_changed;

  HyScanCache                *cache;
  gchar                      *prefix;
  gboolean                    cache_changed;

  HyScanDB                   *db;
  gchar                      *project;
  gchar                      *track;
  gboolean                    raw;
  gboolean                    track_changed;

  gfloat                      ship_speed;
  gboolean                    speed_changed;

  GArray                     *velocity;
  gboolean                    velocity_changed;

  HyScanSourceType            depth_source;
  guint                       depth_channel;
  gboolean                    depth_source_changed;

  gulong                      depth_time;
  guint                       depth_size;
  gboolean                    depth_params_changed;

} HyScanGtkWaterfallMarkState;

struct _HyScanGtkWaterfallMarkPrivate
{
  HyScanGtkWaterfall         *wfall;

  HyScanProjector            *lproj;
  HyScanProjector            *rproj;

  HyScanGtkWaterfallMarkState new_state;
  HyScanGtkWaterfallMarkState state;
  gboolean                    state_changed;
  GMutex                      state_lock;     /* */

  GThread                    *processing;
  gint                        stop;

  GSList                     *drawable; /* Список меток, которые нужно показать. */
  GSList                     *tasks;    /* Список меток, которые нужно отправить в БД. */
  GMutex                      task_lock;     /* */

  gboolean                    in_progress;
  struct Coord                mouse0;
  struct Coord                mouse1;
  struct Coord                mark0;
  struct Coord                mark1;
};

static void    hyscan_gtk_waterfall_mark_interface_init           (HyScanGtkWaterfallLayerInterface *iface);
static void    hyscan_gtk_waterfall_mark_set_property             (GObject                 *object,
                                                                   guint                    prop_id,
                                                                   const GValue            *value,
                                                                   GParamSpec              *pspec);
static void    hyscan_gtk_waterfall_mark_object_constructed       (GObject                 *object);
static void    hyscan_gtk_waterfall_mark_object_finalize          (GObject                 *object);

static void     hyscan_gtk_waterfall_mark_sync_states             (HyScanGtkWaterfallMark  *self);
static gpointer hyscan_gtk_waterfall_mark_processing              (gpointer                 data);

static gboolean hyscan_gtk_waterfall_mark_mouse_button_press      (GtkWidget               *widget,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_mouse_button_release    (GtkWidget               *widget,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_mouse_motion            (GtkWidget               *widget,
                                                                   GdkEventMotion          *event,
                                                                   HyScanGtkWaterfallMark  *self);

static gdouble  hyscan_gtk_waterfall_mark_radius                  (struct Coord            *start,
                                                                   struct Coord            *end);

static void     hyscan_gtk_waterfall_mark_draw_task               (HyScanGtkWaterfallMark  *self,
                                                                   cairo_t                 *cairo,
                                                                   HyScanGtkWaterfallMarkTask *task);
static void     hyscan_gtk_waterfall_mark_draw                    (GtkWidget               *widget,
                                                                   cairo_t                 *cairo,
                                                                   HyScanGtkWaterfallMark  *self);

static void     hyscan_gtk_waterfall_mark_sources_changed         (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_tile_type_changed       (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_profile_changed         (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_cache_changed           (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_track_changed           (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_ship_speed_changed      (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_sound_velocity_changed  (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_depth_source_changed    (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_depth_params_changed    (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallMark, hyscan_gtk_waterfall_mark, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkWaterfallMark)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER, hyscan_gtk_waterfall_mark_interface_init));

static void
hyscan_gtk_waterfall_mark_class_init (HyScanGtkWaterfallMarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_mark_set_property;
  object_class->constructed = hyscan_gtk_waterfall_mark_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_mark_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget", HYSCAN_TYPE_GTK_WATERFALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_mark_init (HyScanGtkWaterfallMark *self)
{
  self->priv = hyscan_gtk_waterfall_mark_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_mark_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);

  if (prop_id == PROP_WATERFALL)
    self->priv->wfall = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_gtk_waterfall_mark_object_constructed (GObject *object)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_mutex_init (&priv->task_lock);
  g_mutex_init (&priv->state_lock);

  /* Сигналы Gtk. */
  g_signal_connect (priv->wfall, "visible-draw",             G_CALLBACK (hyscan_gtk_waterfall_mark_draw), self);
  //g_signal_connect_after (priv->wfall, "configure-event",    G_CALLBACK (hyscan_gtk_waterfall_mark_configure), self);
  g_signal_connect (priv->wfall, "button-press-event",       G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_button_press), self);
  g_signal_connect (priv->wfall, "button-release-event",     G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_button_release), self);
  g_signal_connect (priv->wfall, "motion-notify-event",      G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_motion), self);
  // g_signal_connect (priv->wfall, "leave-notify-event", G_CALLBACK (leave_notify), self);
  //g_signal_connect (priv->wfall, "scroll-event",             G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_wheel), self);
  //g_signal_connect (priv->wfall, "key-press-event",          G_CALLBACK (hyscan_gtk_waterfall_mark_keyboard), self);

  /* Сигналы модели водопада. */
  g_signal_connect (priv->wfall, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_mark_sources_changed), self);
  g_signal_connect (priv->wfall, "changed::tile-type",    G_CALLBACK (hyscan_gtk_waterfall_mark_tile_type_changed), self);
  g_signal_connect (priv->wfall, "changed::profile",      G_CALLBACK (hyscan_gtk_waterfall_mark_profile_changed), self);
  g_signal_connect (priv->wfall, "changed::track",        G_CALLBACK (hyscan_gtk_waterfall_mark_track_changed), self);
  g_signal_connect (priv->wfall, "changed::ship_speed",   G_CALLBACK (hyscan_gtk_waterfall_mark_ship_speed_changed), self);
  g_signal_connect (priv->wfall, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_mark_sound_velocity_changed), self);
  g_signal_connect (priv->wfall, "changed::depth-source", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_source_changed), self);
  g_signal_connect (priv->wfall, "changed::depth-params", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_params_changed), self);
  g_signal_connect (priv->wfall, "changed::cache",        G_CALLBACK (hyscan_gtk_waterfall_mark_cache_changed), self);
 /* Сигналы модели меток. */
  //g_signal_connect (priv->mark_model, "changed",                G_CALLBACK (hyscan_gtk_waterfall_mark_changed), self);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->constructed (object);
}

static void
hyscan_gtk_waterfall_mark_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wfall, self);

  g_clear_object (&priv->wfall);

  g_mutex_clear (&priv->task_lock);
  g_mutex_clear (&priv->state_lock);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_mark_grab_input (HyScanGtkWaterfallLayer *iface)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (iface);

  hyscan_gtk_waterfall_grab_input (self->priv->wfall, self);
}

static const gchar*
hyscan_gtk_waterfall_mark_get_mnemonic (HyScanGtkWaterfallLayer *iface)
{
  return "waterfall-mark";
}

/* Функция синхронизирует состояния. */
static void
hyscan_gtk_waterfall_mark_sync_states (HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkState *new = &priv->new_state;
  HyScanGtkWaterfallMarkState *cur = &priv->state;

  if (new->sources_changed)
    {
      cur->display_type = new->display_type;
      cur->lsource = new->lsource;
      cur->rsource = new->rsource;

      new->sources_changed = FALSE;
      cur->sources_changed = TRUE;
    }
  if (new->track_changed)
    {
      /* Очищаем текущее. */
      g_clear_object (&cur->db);
      g_clear_pointer (&cur->project, g_free);
      g_clear_pointer (&cur->track, g_free);

      /* Копируем из нового. */
      cur->db = new->db;
      cur->project = new->project;
      cur->track = new->track;
      cur->raw = new->raw;

      /* Очищаем новое. */
      new->db = NULL;
      new->project = NULL;
      new->track = NULL;

      /* Выставляем флаги. */
      new->track_changed = FALSE;
      cur->track_changed = TRUE;
    }
  if (new->profile_changed)
    {
      /* Очищаем текущее. */
      g_clear_pointer (&cur->profile, g_free);

      /* Копируем из нового. */
      cur->profile = new->profile;
      new->profile = NULL;

      /* Выставляем флаги. */
      new->profile_changed = FALSE;
      cur->profile_changed = TRUE;
    }

  if (new->cache_changed)
    {
      g_clear_object (&cur->cache);
      g_clear_pointer (&cur->prefix, g_free);

      cur->cache = new->cache;
      cur->prefix = new->prefix;

      new->cache = NULL;
      new->prefix = NULL;

      new->cache_changed = FALSE;
      cur->cache_changed = TRUE;
    }

  if (new->depth_source_changed)
    {
      cur->depth_source = new->depth_source;
      cur->depth_channel = new->depth_channel;

      new->depth_source_changed = FALSE;
      cur->depth_source_changed = TRUE;
    }
  if (new->depth_params_changed)
    {
      cur->depth_time = new->depth_time;
      cur->depth_size = new->depth_size;

      new->depth_params_changed = FALSE;
      cur->depth_params_changed = TRUE;
    }


  if (new->speed_changed)
    {
      cur->ship_speed = new->ship_speed;

      cur->speed_changed = TRUE;
      new->speed_changed = FALSE;
    }
  if (new->velocity_changed)
    {
      if (cur->velocity != NULL)
        g_clear_pointer (&cur->velocity, g_array_unref);

      cur->velocity = new->velocity;

      new->velocity = NULL;

      cur->velocity_changed = TRUE;
      new->velocity_changed = FALSE;
    }
}

static gpointer
hyscan_gtk_waterfall_mark_processing (gpointer data)
{
  HyScanGtkWaterfallMark *self = data;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  HyScanProjector         *lproj = NULL;
  HyScanProjector         *rproj = NULL;
  HyScanDepth             *idepth = NULL;
  HyScanDepthometer       *depth = NULL;
  HyScanWaterfallMarkData *mdata = NULL;

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Проверяем, поменялось ли что-то. */
      if (g_atomic_int_get (&priv->state_changed))
        {
          HyScanGtkWaterfallMarkState *state = &priv->state;

          /* Синхронизация. */
          g_mutex_lock (&priv->state_lock);
          hyscan_gtk_waterfall_mark_sync_states (self);
          g_mutex_unlock (&priv->state_lock);

          /* Применяем обновления. */
          if (state->sources_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
            }
          if (state->track_changed)
            {

            }
          if (state->profile_changed)
            {

            }
          if (state->cache_changed)
            {

            }
          if (state->depth_source_changed)
            {

            }
          if (state->depth_params_changed)
            {

            }
          if (state->speed_changed)
            {

            }
          if (state->velocity_changed)
            {

            }

        }
    }

  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&idepth);
  g_clear_object (&depth);

  return NULL;
}

static gboolean
hyscan_gtk_waterfall_mark_mouse_button_press (GtkWidget              *widget,
                                              GdkEventButton         *event,
                                              HyScanGtkWaterfallMark *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  guint b_top, b_bottom, b_left, b_right;
  guint width, height;
  gdouble x = event->x;
  gdouble y = event->y;

  /* Проверяем, есть ли у нас право обработки ввода. */
  if (!hyscan_gtk_waterfall_has_input (HYSCAN_GTK_WATERFALL (widget), self))
    return FALSE;

  if (event->button != 1)
    return FALSE;

  gtk_cifro_area_get_size (carea, &width, &height);
  gtk_cifro_area_get_border (carea, &b_top, &b_bottom, &b_left, &b_right);

  if (x > b_left && x < width - b_right && y > b_top && y < height - b_bottom)
    {
      priv->mouse1.x = priv->mouse0.x = x;
      priv->mouse1.y = priv->mouse0.y = y;

      priv->in_progress = TRUE;
      hyscan_gtk_waterfall_queue_draw (priv->wfall);
    }

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_mark_mouse_button_release (GtkWidget              *widget,
                                                GdkEventButton         *event,
                                                HyScanGtkWaterfallMark *self)
{
  struct Coord mark0, mark1, mouse;
  HyScanGtkWaterfallMarkTask *to_draw, *to_task;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  mouse.x = event->x;
  mouse.y = event->y;
  gdouble radius;

  /* Проверяем, есть ли у нас право обработки ввода.
   * Может оказаться так, что ввод отобрали прямо
   * во время создания метки. Поэтому принудительно
   * прекратим создание метки. */
  if (!hyscan_gtk_waterfall_has_input (HYSCAN_GTK_WATERFALL (widget), self))
    goto reset;

  if (event->button != 1)
    return FALSE;

  /* Проверяем, создается ли сейчас метка. */
  if (!priv->in_progress)
    return FALSE;


  /* Проверяем, на сколько сдвинут указатель.
   * Если слишком мало, считаем, что это случайность. */
  if (hyscan_gtk_waterfall_mark_radius (&priv->mouse0, &mouse) < 5)
    goto reset;

  /* Создаем метку. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), priv->mouse0.x, priv->mouse0.y, &mark0.x, &mark0.y);
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), mouse.x, mouse.y, &mark1.x, &mark1.y);

  radius = hyscan_gtk_waterfall_mark_radius (&mark0, &mark1);

  to_draw = g_new0 (HyScanGtkWaterfallMarkTask, 1);
  to_task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
  to_task->x = to_draw->x = mark0.x;
  to_task->y = to_draw->y = mark0.y;
  to_task->r = to_draw->r = radius;

  g_mutex_lock (&priv->task_lock);
  priv->drawable = g_slist_prepend (priv->drawable, to_draw);
  priv->tasks = g_slist_prepend (priv->tasks, to_task);
  g_mutex_unlock (&priv->task_lock);

reset:
  priv->in_progress = FALSE;
  return FALSE;
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_mark_mouse_motion (GtkWidget              *widget,
                                        GdkEventMotion         *event,
                                        HyScanGtkWaterfallMark *self)
{
  if (!self->priv->in_progress)
    return FALSE;

  self->priv->mouse1.x = event->x;
  self->priv->mouse1.y = event->y;

  return FALSE;
}

/* Функция вычисляет расстояние между точками. */
static gdouble
hyscan_gtk_waterfall_mark_radius (struct Coord *start,
                                  struct Coord *end)
{
  return sqrt (pow (ABS(end->x - start->x), 2) + pow (ABS(end->y - start->y), 2));
}

/* Функция отрисовки одной метки.*/
static void
hyscan_gtk_waterfall_mark_draw_task (HyScanGtkWaterfallMark     *self,
                                     cairo_t                    *cairo,
                                     HyScanGtkWaterfallMarkTask *task)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (self->priv->wfall);
  gdouble r0, r1;

  struct Coord center;

  gtk_cifro_area_visible_value_to_point (carea, &center.x, &center.y, task->x, task->y);

  gtk_cifro_area_visible_value_to_point (carea, NULL, &r1, 0, task->r);
  gtk_cifro_area_visible_value_to_point (carea, NULL, &r0, 0, 0);

  r1 = ABS (r1 - r0);
  // r1 = 10;

  cairo_set_source_rgba (cairo, 1.0, 0.0, 0.0, 1.0);
  cairo_move_to (cairo, center.x + r1, center.y);
  cairo_arc (cairo, center.x, center.y, r1, 0, 2 * G_PI);
  cairo_stroke (cairo);
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_mark_draw (GtkWidget              *widget,
                                cairo_t                *cairo,
                                HyScanGtkWaterfallMark *self)
{
  GSList *task;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  cairo_save (cairo);

  g_mutex_lock (&priv->task_lock);

  /* Отрисовываем drawable метки. */
  for (task = priv->drawable; task != NULL; task = task->next)
    hyscan_gtk_waterfall_mark_draw_task (self, cairo, task->data);
  g_mutex_unlock (&priv->task_lock);

  /* Отрисовываем создаваемую метку. */
  if (priv->in_progress)
    {
      struct Coord start;
      struct Coord end;
      HyScanGtkWaterfallMarkTask to_draw;

      gtk_cifro_area_visible_point_to_value (carea, priv->mouse0.x, priv->mouse0.y, &start.x, &start.y);
      gtk_cifro_area_visible_point_to_value (carea, priv->mouse1.x, priv->mouse1.y, &end.x, &end.y);

      to_draw.x = start.x;
      to_draw.y = start.y;
      to_draw.r = hyscan_gtk_waterfall_mark_radius (&start, &end);

      hyscan_gtk_waterfall_mark_draw_task (self, cairo, &to_draw);
    }

  cairo_restore (cairo);
}

static void
hyscan_gtk_waterfall_mark_sources_changed (HyScanGtkWaterfallState *model,
                                           HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_sources (model,
                                      &priv->new_state.display_type,
                                      &priv->new_state.lsource,
                                      &priv->new_state.rsource);
  priv->new_state.sources_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_tile_type_changed (HyScanGtkWaterfallState *model,
                                             HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_tile_type (model, &priv->new_state.tile_type);
  priv->new_state.tile_type_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_profile_changed (HyScanGtkWaterfallState *model,
                                           HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  g_clear_pointer (&priv->new_state.profile, g_free);
  hyscan_gtk_waterfall_state_get_profile (model, &priv->new_state.profile);

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_cache_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  g_clear_object (&priv->new_state.cache);
  g_clear_pointer (&priv->new_state.prefix, g_free);
  hyscan_gtk_waterfall_state_get_cache (model,
                                    &priv->new_state.cache,
                                    NULL,
                                    &priv->new_state.prefix);
  priv->new_state.cache_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_track_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  g_clear_object (&priv->new_state.db);
  g_clear_pointer (&priv->new_state.project, g_free);
  g_clear_pointer (&priv->new_state.track, g_free);
  hyscan_gtk_waterfall_state_get_track (model,
                                    &priv->new_state.db,
                                    &priv->new_state.project,
                                    &priv->new_state.track,
                                    &priv->new_state.raw);
  priv->new_state.track_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_ship_speed_changed (HyScanGtkWaterfallState *model,
                                              HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_ship_speed (model, &priv->new_state.ship_speed);
  priv->new_state.speed_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}
static void
hyscan_gtk_waterfall_mark_sound_velocity_changed (HyScanGtkWaterfallState *model,
                                                  HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_sound_velocity (model, &priv->new_state.velocity);
  priv->new_state.velocity_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

static void
hyscan_gtk_waterfall_mark_depth_source_changed (HyScanGtkWaterfallState *model,
                                                HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_depth_source (model,
                                           &priv->new_state.depth_source,
                                           &priv->new_state.depth_channel);
  priv->new_state.depth_source_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}
static void
hyscan_gtk_waterfall_mark_depth_params_changed (HyScanGtkWaterfallState *model,
                                                HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_depth_time (model, &priv->new_state.depth_time);
  hyscan_gtk_waterfall_state_get_depth_filter_size (model, &priv->new_state.depth_size);
  priv->new_state.depth_params_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

HyScanGtkWaterfallMark*
hyscan_gtk_waterfall_mark_new (HyScanGtkWaterfall   *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MARK,
                       "waterfall", waterfall,
                       NULL);
}

// void
// hyscan_gtk_waterfall_mark_set_model (HyScanGtkWaterfallMark *mark,
//                                      HyScanMarkModel        *model)
// {
//   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (mark));
//   g_object_set (mark, "mmodel", model, NULL);
// }

static void
hyscan_gtk_waterfall_mark_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_mark_grab_input;
  iface->get_mnemonic = hyscan_gtk_waterfall_mark_get_mnemonic;
}
