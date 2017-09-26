#include <hyscan-depthometer.h>
#include <hyscan-depth-acoustic.h>
#include <hyscan-depth-nmea.h>
#include "hyscan-gtk-waterfall-mark.h"
#include <hyscan-mark-manager.h>
#include <hyscan-projector.h>
#include <hyscan-tile-color.h>
#include <math.h>

struct Coord
{
  gdouble x;
  gdouble y;
};

typedef struct
{
  HyScanWaterfallMark *mark;
  gdouble              x0;
  gdouble              y0;
  gdouble              x1;
  gdouble              y1;
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

  guint64                     mark_filter;
  gboolean                    mark_filter_changed;

} HyScanGtkWaterfallMarkState;

struct _HyScanGtkWaterfallMarkPrivate
{
  HyScanGtkWaterfall         *wfall;

  HyScanGtkWaterfallMarkState new_state;
  HyScanGtkWaterfallMarkState state;
  gboolean                    state_changed;
  GMutex                      state_lock;     /* */

  GThread                    *processing;
  gint                        stop;

  GSList                     *drawable;       /* Список меток, которые нужно показать. */
  GMutex                      drawable_lock;  /* */
  GSList                     *tasks;          /* Список меток, которые нужно отправить в БД. */
  GMutex                      task_lock;      /* */
  GSList                     *visible;        /* Список меток, которые реально есть на экране. */

  struct
  {
    gdouble x0;
    gdouble y0;
    gdouble x1;
    gdouble y1;
  } view;

  gboolean                    in_progress;
  struct Coord                mouse0;
  struct Coord                mouse1;
  struct Coord                mark0;
  struct Coord                mark1;

  gint                        width;
  gint                        height;

  GdkRGBA                     shadow_color;
  GdkRGBA                     mark_color;
};

static void    hyscan_gtk_waterfall_mark_interface_init           (HyScanGtkWaterfallLayerInterface *iface);
static void    hyscan_gtk_waterfall_mark_set_property             (GObject                 *object,
                                                                   guint                    prop_id,
                                                                   const GValue            *value,
                                                                   GParamSpec              *pspec);
static void    hyscan_gtk_waterfall_mark_object_constructed       (GObject                 *object);
static void    hyscan_gtk_waterfall_mark_object_finalize          (GObject                 *object);

static void    hyscan_gtk_waterfall_mark_free_task                (gpointer                 data);
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
                                                                   HyScanGtkWaterfallMarkTask *task,
                                                                   gboolean                 shadow);
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

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->constructed (object);

  g_mutex_init (&priv->task_lock);
  g_mutex_init (&priv->drawable_lock);
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
  g_signal_connect (priv->wfall, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_mark_ship_speed_changed), self);
  g_signal_connect (priv->wfall, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_mark_sound_velocity_changed), self);
  g_signal_connect (priv->wfall, "changed::depth-source", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_source_changed), self);
  g_signal_connect (priv->wfall, "changed::depth-params", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_params_changed), self);
  g_signal_connect (priv->wfall, "changed::cache",        G_CALLBACK (hyscan_gtk_waterfall_mark_cache_changed), self);
 /* Сигналы модели меток. */
  //g_signal_connect (priv->mark_model, "changed",                G_CALLBACK (hyscan_gtk_waterfall_mark_changed), self);

  hyscan_gtk_waterfall_mark_sources_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_tile_type_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_profile_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_cache_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_track_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_ship_speed_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_sound_velocity_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_depth_source_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_depth_params_changed (HYSCAN_GTK_WATERFALL_STATE (priv->wfall), self);
  hyscan_gtk_waterfall_mark_set_mark_filter (self, HYSCAN_GTK_WATERFALL_MARKS_ALL);
  hyscan_gtk_waterfall_mark_set_shadow_color (self, hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 0.5));
  hyscan_gtk_waterfall_mark_set_mark_color (self, hyscan_tile_color_converter_d2i (1.0, 1.0, 1.0, 1.0));

  // priv->filter = HYSCAN_GTK_WATERFALL_MARKS_ALL;

  priv->stop = FALSE;
  priv->processing = g_thread_new ("gtk-wfall-mark", hyscan_gtk_waterfall_mark_processing, self);
}

static void
hyscan_gtk_waterfall_mark_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_atomic_int_set (&priv->stop, TRUE);
  g_thread_join (priv->processing);
  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wfall, self);


  g_clear_object (&priv->wfall);

  g_mutex_clear (&priv->task_lock);
  g_mutex_clear (&priv->drawable_lock);
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

static HyScanDepth*
hyscan_gtk_waterfall_mark_open_depth (HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkState *state = &priv->state;
  HyScanDepth *idepth = NULL;

  if (state->db == NULL || state->project == NULL || state->track == NULL)
    return NULL;

  if (hyscan_source_is_acoustic (state->depth_source))
    {
      HyScanDepthAcoustic *dacoustic;
      dacoustic = hyscan_depth_acoustic_new (state->db,
                                             state->project,
                                             state->track,
                                             state->depth_source,
                                             state->raw);

      if (dacoustic != NULL)
        hyscan_depth_acoustic_set_sound_velocity (dacoustic, state->velocity);

      idepth = HYSCAN_DEPTH (dacoustic);
    }

  else if (HYSCAN_SOURCE_NMEA_DPT == state->depth_source)
    {
      HyScanDepthNMEA *dnmea;
      dnmea = hyscan_depth_nmea_new (state->db,
                                     state->project,
                                     state->track,
                                     state->depth_channel);
      idepth = HYSCAN_DEPTH (dnmea);
    }

  hyscan_depth_set_cache (idepth, state->cache, state->prefix);
  return idepth;
}

static HyScanDepthometer*
hyscan_gtk_waterfall_mark_open_depthometer (HyScanGtkWaterfallMark *self,
                                             HyScanDepth            *idepth)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanDepthometer *depth = NULL;

  if (idepth == NULL)
    return NULL;

  depth = hyscan_depthometer_new (idepth);
  if (depth == NULL)
    return NULL;

  hyscan_depthometer_set_cache (depth, priv->state.cache, priv->state.prefix);
  hyscan_depthometer_set_filter_size (depth, priv->state.depth_size);
  hyscan_depthometer_set_time_precision (depth, priv->state.depth_time);

  return depth;
}

static HyScanProjector*
hyscan_gtk_waterfall_mark_open_projector (HyScanGtkWaterfallMark *self,
                                          HyScanSourceType        source)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkState *state = &priv->state;
  HyScanProjector *projector;

  if (state->db == NULL || state->project == NULL || state->track == NULL)
    return NULL;

  projector = hyscan_projector_new (state->db, state->project, state->track, source, state->raw);

  if (projector == NULL)
    return NULL;

  hyscan_projector_set_cache (projector, state->cache, state->prefix);
  hyscan_projector_set_ship_speed (projector, state->ship_speed);
  hyscan_projector_set_sound_velocity (projector, state->velocity);

  return projector;
}

/* Функция освобождает память, занятую структурой HyScanGtkWaterfallMarkTask. */
static void
hyscan_gtk_waterfall_mark_free_task (gpointer data)
{
  HyScanGtkWaterfallMarkTask *task = data;

  hyscan_waterfall_mark_deep_free (task->mark);
  g_free (task);
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
  if (new->mark_filter_changed)
    {
      cur->mark_filter = new->mark_filter;

      cur->mark_filter_changed = TRUE;
      new->mark_filter_changed = FALSE;
    }
}

static gpointer
hyscan_gtk_waterfall_mark_processing (gpointer data)
{
  HyScanGtkWaterfallMark *self = data;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  HyScanProjector         *lproj = NULL;
  HyScanProjector         *rproj = NULL;
  HyScanProjector         *_proj0 = NULL;
  HyScanProjector         *_proj1 = NULL;
  HyScanDepth             *idepth = NULL;
  HyScanDepthometer       *depth = NULL;
  HyScanWaterfallMarkData *mdata = NULL;

  GSList *list, *link;
  HyScanGtkWaterfallMarkTask *task;
  HyScanSourceType source0, source1;
  guint32 index0, count0, index1, count1;
  gboolean status;

  guint32 mc = 0, oldmc = 0;
  gchar **ids;
  guint nids, i;
  gboolean refresh = FALSE;
  HyScanGtkWaterfallMarkState *state = &priv->state;

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Проверяем, поменялось ли что-то. */
      if (g_atomic_int_get (&priv->state_changed))
        {
          /* Синхронизация. */
          g_mutex_lock (&priv->state_lock);
          hyscan_gtk_waterfall_mark_sync_states (self);
          g_mutex_unlock (&priv->state_lock);

          /* Применяем обновления. */
          if (state->sources_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              state->sources_changed = FALSE;
            }
          if (state->track_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              g_clear_object (&idepth);
              g_clear_object (&depth);
              g_clear_object (&mdata);
              state->track_changed = FALSE;
            }
          if (state->profile_changed)
            {
              g_clear_object (&mdata);
              state->profile_changed = FALSE;
            }
          if (state->depth_source_changed)
            {
              g_clear_object (&idepth);
              g_clear_object (&depth);
              state->depth_source_changed = FALSE;
            }
          if (state->depth_params_changed)
            {
              if (depth != NULL)
                {
                  hyscan_depthometer_set_filter_size (depth, state->depth_size);
                  hyscan_depthometer_set_time_precision (depth, state->depth_time);
                }
              state->depth_params_changed = FALSE;
            }
          if (state->speed_changed)
            {
              if (lproj != NULL)
                hyscan_projector_set_ship_speed (lproj, state->ship_speed);
              if (rproj != NULL)
                hyscan_projector_set_ship_speed (rproj, state->ship_speed);
              state->speed_changed = FALSE;
            }
          if (state->velocity_changed)
            {
              if (lproj != NULL)
                hyscan_projector_set_sound_velocity (lproj, state->velocity);
              if (rproj != NULL)
                hyscan_projector_set_sound_velocity (rproj, state->velocity);
              if (HYSCAN_IS_DEPTH_ACOUSTIC (idepth))
                hyscan_depth_acoustic_set_sound_velocity (HYSCAN_DEPTH_ACOUSTIC (idepth), state->velocity);
              state->velocity_changed = FALSE;
            }
          if (state->cache_changed)
            {
              if (idepth != NULL)
                hyscan_depth_set_cache (idepth, state->cache, state->prefix);
              if (depth != NULL)
                hyscan_depthometer_set_cache (depth, state->cache, state->prefix);
              if (lproj != NULL)
                hyscan_projector_set_cache (lproj, state->cache, state->prefix);
              if (rproj != NULL)
                hyscan_projector_set_cache (rproj, state->cache, state->prefix);
              state->cache_changed = FALSE;
            }

          g_atomic_int_set (&priv->state_changed, FALSE);

          if (lproj == NULL)
            {
              lproj = hyscan_gtk_waterfall_mark_open_projector (self, state->lsource);
              // TEST PERFORMANCE PLS hyscan_projector_set_precalc_points (lproj, 10000);
            }
          if (rproj == NULL)
            {
              rproj = hyscan_gtk_waterfall_mark_open_projector (self, state->rsource);
              // TEST PERFORMANCE PLS hyscan_projector_set_precalc_points (rproj, 10000);
            }
          if (mdata == NULL)
            {
              if (state->db != NULL || state->project != NULL || state->track != NULL)
                mdata = hyscan_waterfall_mark_data_new (state->db, state->project, state->track, state->profile);
              if (mdata != NULL)
                oldmc = ~hyscan_waterfall_mark_data_get_mod_count (mdata);
            }
          if (depth == NULL)
            {
              if (idepth == NULL)
                idepth = hyscan_gtk_waterfall_mark_open_depth (self);
              depth = hyscan_gtk_waterfall_mark_open_depthometer (self, idepth);
            }

          refresh = TRUE;
        }

      if (mdata == NULL || rproj == NULL || lproj ==NULL)
        continue;

      /* Нормальная работа. */
      /* Пересчитываем текущие задачи и отправляем в БД.
       * Чтобы как можно меньше задерживать mainloop, копируем
       * список задач. */
      g_mutex_lock (&priv->task_lock);
      list = priv->tasks;
      priv->tasks = NULL;
      g_mutex_unlock (&priv->task_lock);

      for (link = list; link != NULL; link = link->next)
        {
          refresh = TRUE;

          task = link->data;
          if (task->x0 >= 0)
            {
              source0 = state->rsource;
              _proj0 = rproj;
            }
          else
            {
              source0 = state->lsource;
              _proj0 = lproj;
            }
          if (task->x1 >= 0)
            {
              source1 = state->rsource;
              _proj1 = rproj;
            }
          else
            {
              source1 = state->lsource;
              _proj1 = lproj;
            }

          status  = hyscan_projector_coord_to_index (_proj0, task->y0, &index0);
          status &= hyscan_projector_coord_to_count (_proj0, ABS (task->x0), &count0, 0.0);
          status &= hyscan_projector_coord_to_index (_proj1, task->y1, &index1);
          status &= hyscan_projector_coord_to_count (_proj1, ABS (task->x1), &count1, 0.0);

          if (!status)
            continue;

          hyscan_waterfall_mark_data_add_full (mdata, "name", "description", "operator",
                                               HYSCAN_GTK_WATERFALL_MARKS_ALL,
                                               g_get_real_time (), g_get_real_time (),
                                               source0, index0, count0,
                                               source1, index1, count1);
        }

      /* Очищаем весь список заданий. */
      g_slist_free_full (list, hyscan_gtk_waterfall_mark_free_task);
      list = NULL;

      /* Проверяем счётчик изменений и при необходимости
       * забираем из БД обновленный список меток. */
      mc = hyscan_waterfall_mark_data_get_mod_count (mdata);
      if (mc == oldmc && !refresh)
        {
          continue;
        }
      else
        {
          oldmc = mc;
          refresh = FALSE;
        }

      /* Получаем список идентификаторов. */
      ids = hyscan_waterfall_mark_data_get_ids (mdata, &nids);
      if (ids == NULL)
        continue;

      /* Каждую метку переводим в наши координаты. */
      for (i = 0; i < nids; i++)
        {
          HyScanWaterfallMark *mark;

          mark = hyscan_waterfall_mark_data_get (mdata, ids[i]);

          if (mark == NULL)
            continue;

          /* Фильтруем по лейблу. */
          if (!(mark->labels & state->mark_filter))
            {
              hyscan_waterfall_mark_deep_free (mark);
              continue;
            }
          /* Фильтруем по источнику. */
          if (mark->source0 == state->lsource)
            {
              _proj0 = lproj;
            }
          else if (mark->source0 == state->rsource)
            {
              _proj0 = rproj;
            }
          else
            {
              hyscan_waterfall_mark_deep_free (mark);
              continue;
            }
          if (mark->source1 == state->lsource)
            {
              _proj1 = lproj;
            }
          else if (mark->source1 == state->rsource)
            {
              _proj1 = rproj;
            }
          else
            {
              hyscan_waterfall_mark_deep_free (mark);
              continue;
            }

          task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
          task->mark = mark;
          hyscan_projector_index_to_coord (_proj0, mark->index0, &task->y0);
          hyscan_projector_count_to_coord (_proj0, mark->count0, &task->x0, 0.0);
          hyscan_projector_index_to_coord (_proj1, mark->index1, &task->y1);
          hyscan_projector_count_to_coord (_proj1, mark->count1, &task->x1, 0.0);

          if (mark->source0 == state->lsource)
            task->x0 *= -1;
          if (mark->source1 == state->lsource)
            task->x1 *= -1;

          list = g_slist_prepend (list, task);
        }

      g_strfreev (ids);

      g_mutex_lock (&priv->drawable_lock);
      g_slist_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
      priv->drawable = list;
      list = NULL;
      g_mutex_unlock (&priv->drawable_lock);
      hyscan_gtk_waterfall_queue_draw (priv->wfall);
    }

  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&idepth);
  g_clear_object (&depth);
  g_clear_object (&mdata);

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
  struct Coord mark0, mark1;
  HyScanGtkWaterfallMarkTask *to_draw, *to_task;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

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
  if (hyscan_gtk_waterfall_mark_radius (&priv->mouse0, &priv->mouse1) < 9)
    goto reset;

  /* Создаем метку. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), priv->mouse0.x, priv->mouse0.y, &mark0.x, &mark0.y);
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), priv->mouse1.x, priv->mouse1.y, &mark1.x, &mark1.y);

  to_draw = g_new0 (HyScanGtkWaterfallMarkTask, 1);
  to_task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
  to_task->x0 = to_draw->x0 = mark0.x;
  to_task->y0 = to_draw->y0 = mark0.y;
  to_task->x1 = to_draw->x1 = mark1.x;
  to_task->y1 = to_draw->y1 = mark1.y;

  g_mutex_lock (&priv->drawable_lock);
  priv->drawable = g_slist_prepend (priv->drawable, to_draw);
  g_mutex_unlock (&priv->drawable_lock);

  g_mutex_lock (&priv->task_lock);
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
                                     HyScanGtkWaterfallMarkTask *task,
                                     gboolean                    shadow)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (self->priv->wfall);

  struct Coord center;
  struct Coord corner;
  gdouble dx, dy;
  guint w, h;

  dx = task->x0 - task->x1;
  dy = task->y0 - task->y1;

  // if () // TODO: detect intersection

  gtk_cifro_area_visible_value_to_point (carea, &center.x, &center.y, task->x0, task->y0);
  gtk_cifro_area_visible_value_to_point (carea, &corner.x, &corner.y, task->x1, task->y1);
  g_message ("CORNER: xy %f %f", corner.x, corner.y);

  dx = center.x - corner.x;
  dy = center.y - corner.y;

  cairo_set_source_rgba (cairo,
                         self->priv->mark_color.red,
                         self->priv->mark_color.green,
                         self->priv->mark_color.blue,
                         self->priv->mark_color.alpha);

  cairo_rectangle (cairo, center.x - dx, center.y - dy, 2 * dx, 2 * dy);

  if (shadow)
    {
      gtk_cifro_area_get_size (carea, &w, &h);
      cairo_set_source_rgba (cairo,
                             self->priv->shadow_color.red,
                             self->priv->shadow_color.green,
                             self->priv->shadow_color.blue,
                             self->priv->shadow_color.alpha);
      cairo_rectangle (cairo, 0, 0, w, h);
      cairo_set_fill_rule (cairo, CAIRO_FILL_RULE_EVEN_ODD);
      cairo_fill (cairo);

      cairo_set_source_rgba (cairo,
                             self->priv->mark_color.red,
                             self->priv->mark_color.green,
                             self->priv->mark_color.blue,
                             self->priv->mark_color.alpha);

      cairo_rectangle (cairo, center.x - dx, center.y - dy, 2 * dx, 2 * dy);
    }

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

  gtk_cifro_area_get_view (carea, &priv->view.x0, &priv->view.x1,
                           &priv->view.y0, &priv->view.y1);

  cairo_save (cairo);

  /* Отрисовываем drawable метки. */
  g_mutex_lock (&priv->drawable_lock);

  for (task = priv->drawable; task != NULL; task = task->next)
    hyscan_gtk_waterfall_mark_draw_task (self, cairo, task->data, FALSE);

  g_mutex_unlock (&priv->drawable_lock);

  /* Отрисовываем создаваемую метку. */
  if (priv->in_progress)
    {
      struct Coord start;
      struct Coord end;
      HyScanGtkWaterfallMarkTask to_draw;

      gtk_cifro_area_visible_point_to_value (carea, priv->mouse0.x, priv->mouse0.y, &start.x, &start.y);
      gtk_cifro_area_visible_point_to_value (carea, priv->mouse1.x, priv->mouse1.y, &end.x, &end.y);

      to_draw.x0 = start.x;
      to_draw.y0 = start.y;
      to_draw.x1 = end.x;
      to_draw.y1 = end.y;

      hyscan_gtk_waterfall_mark_draw_task (self, cairo, &to_draw, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
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

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);
}

HyScanGtkWaterfallMark*
hyscan_gtk_waterfall_mark_new (HyScanGtkWaterfall   *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MARK,
                       "waterfall", waterfall,
                       NULL);
}

void
hyscan_gtk_waterfall_mark_enter_create_mode (HyScanGtkWaterfallMark *self)
{
}

void
hyscan_gtk_waterfall_mark_enter_edit_mode (HyScanGtkWaterfallMark *self)
{
}

// void
// hyscan_gtk_waterfall_mark_set_model (HyScanGtkWaterfallMark *mark,
//                                      HyScanMarkModel        *model)
// {
//   g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (mark));
//   g_object_set (mark, "mmodel", model, NULL);
// }

void
hyscan_gtk_waterfall_mark_set_mark_filter (HyScanGtkWaterfallMark *self,
                                      guint64                 filter)
{
  HyScanGtkWaterfallMarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.mark_filter = filter;
  priv->new_state.mark_filter_changed = TRUE;
  g_atomic_int_set (&priv->state_changed, TRUE);

  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_gtk_waterfall_mark_set_draw_type (HyScanGtkWaterfallMark     *self,
                                         HyScanGtkWaterfallMarksDraw type)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));
}

void
hyscan_gtk_waterfall_mark_set_shadow_color (HyScanGtkWaterfallMark *self,
                                            guint32                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  hyscan_tile_color_converter_i2d (color,
                                   &self->priv->shadow_color.red,
                                   &self->priv->shadow_color.green,
                                   &self->priv->shadow_color.blue,
                                   &self->priv->shadow_color.alpha);

}

void
hyscan_gtk_waterfall_mark_set_mark_color (HyScanGtkWaterfallMark *self,
                                          guint32                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  hyscan_tile_color_converter_i2d (color,
                                   &self->priv->mark_color.red,
                                   &self->priv->mark_color.green,
                                   &self->priv->mark_color.blue,
                                   &self->priv->mark_color.alpha);



}

static void
hyscan_gtk_waterfall_mark_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_mark_grab_input;
  iface->get_mnemonic = hyscan_gtk_waterfall_mark_get_mnemonic;
}
