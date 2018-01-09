#include "hyscan-gtk-waterfall-mayer.h"
#include "hyscan-gtk-waterfall-mayer-private.h"

enum
{
  PROP_WATERFALL = 1
};

struct _HyScanGtkWaterfallMayerPrivate
{
  HyScanGtkWaterfallState     *wf_state;
  HyScanGtkWaterfall          *wfall;

  _HyScanGtkWaterfallMayerState new_state;
  _HyScanGtkWaterfallMayerState state;
  gboolean                      state_changed;
  GMutex                        state_lock;


  HyScanGtkWaterfallMayerCheckFunc *check_func;
  HyScanGtkWaterfallMayerThreadFunc *thread_func;
  HyScanGtkWaterfallMayerStopFunc *stop_func;
  gpointer                         thread_data;

  GThread                     *thread;
  gint                         stop;

  GCond                        cond;
  GMutex                       cond_mutex;
  gint                         cond_flag;
};

static void    hyscan_gtk_waterfall_mayer_set_property             (GObject               *object,
                                                                    guint                  prop_id,
                                                                    const GValue          *value,
                                                                    GParamSpec            *pspec);
static void    hyscan_gtk_waterfall_mayer_object_constructed       (GObject               *object);
static void    hyscan_gtk_waterfall_mayer_object_finalize          (GObject               *object);

static gpointer hyscan_gtk_waterfall_mayer_thread                  (gpointer               data);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanGtkWaterfallMayer, hyscan_gtk_waterfall_mayer, G_TYPE_OBJECT);

static void
hyscan_gtk_waterfall_mayer_class_init (HyScanGtkWaterfallMayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_mayer_set_property;

  object_class->constructed = hyscan_gtk_waterfall_mayer_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_mayer_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget",
                         HYSCAN_TYPE_GTK_WATERFALL_STATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_mayer_init (HyScanGtkWaterfallMayer *self)
{
  self->priv = hyscan_gtk_waterfall_mayer_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_mayer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;

  if (prop_id == PROP_WATERFALL)
    {
      priv->wfall = g_value_dup_object (value);
      priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_mayer_object_constructed (GObject *object)
{
  HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;

  g_mutex_init (&priv->cond_mutex);
  g_cond_init (&priv->cond);
}

static void
hyscan_gtk_waterfall_mayer_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMayer *self = HYSCAN_GTK_WATERFALL_MAYER (object);
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;

  g_object_unref (priv->wfall);

  g_mutex_clear (&priv->cond_mutex);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mayer_parent_class)->finalize (object);
}

void
hyscan_gtk_waterfall_mayer_trigger (HyScanGtkWaterfallMayer *mayer)
{
  HyScanGtkWaterfallMayerPrivate *priv = mayer->priv;

  g_mutex_lock (&priv->cond_mutex);
  g_atomic_int_set (&priv->cond_flag, TRUE);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->cond_mutex);
}

void noop (void) {} // TODO: remove me

void
hyscan_gtk_waterfall_mayer_connect_signals (HyScanGtkWaterfallMayer *mayer,
                                            guint64                  mask)
{
  // HyScanGtkWaterfallMayerPrivate *priv = mayer->priv;

  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_SOURCES)
    noop();//g_signal_connect (priv->wf_state, "changed::sources", G_CALLBACK (hyscan_gtk_waterfall_mayer_sources_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_TILE_TYPE)
    noop();//g_signal_connect (priv->wf_state, "changed::tile-type", G_CALLBACK (hyscan_gtk_waterfall_mayer_tile_type_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_TRACK)
    noop();//g_signal_connect (priv->wf_state, "changed::track", G_CALLBACK (hyscan_gtk_waterfall_mayer_track_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_SPEED)
    noop();//g_signal_connect (priv->wf_state, "changed::speed", G_CALLBACK (hyscan_gtk_waterfall_mayer_ship_speed_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_VELOCITY)
    noop();//g_signal_connect (priv->wf_state, "changed::velocity", G_CALLBACK (hyscan_gtk_waterfall_mayer_sound_velocity_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_DEPTH_SOURCE)
    noop();//g_signal_connect (priv->wf_state, "changed::depth-source", G_CALLBACK (hyscan_gtk_waterfall_mayer_depth_source_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_DEPTH_PARAMS)
    noop();//g_signal_connect (priv->wf_state, "changed::depth-params", G_CALLBACK (hyscan_gtk_waterfall_mayer_depth_params_changed), mayer);
  if (mask & HYSCAN_GTK_WATERFALL_MAYER_SIGNAL_CACHE)
    noop();//g_signal_connect (priv->wf_state, "changed::cache", G_CALLBACK (hyscan_gtk_waterfall_mayer_cache_changed), mayer);
}

void
hyscan_gtk_waterfall_mayer_create_thread (HyScanGtkWaterfallMayer *mayer,
                                          HyScanGtkWaterfallMayerCheckFunc *check_func,
                                          HyScanGtkWaterfallMayerThreadFunc *thread_func,
                                          HyScanGtkWaterfallMayerStopFunc *stop_func,
                                          gpointer       thread_data,
                                          const gchar   *thread_name)
{
  HyScanGtkWaterfallMayerPrivate *priv = mayer->priv;

  priv->check_func = check_func;
  priv->thread_func = thread_func;
  priv->stop_func = stop_func;
  priv->thread_data = thread_data;

  if (thread_name == NULL)
    thread_name = "wf-layer";

  priv->thread = g_thread_new (thread_name, hyscan_gtk_waterfall_mayer_thread, mayer);
}

void
hyscan_gtk_waterfall_mayer_kill_thread (HyScanGtkWaterfallMayer *mayer)
{
  HyScanGtkWaterfallMayerPrivate *priv = mayer->priv;

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->thread, g_thread_join);
}

static gpointer
hyscan_gtk_waterfall_mayer_thread (gpointer data)
{
  HyScanGtkWaterfallMayer *self = data;
  HyScanGtkWaterfallMayerPrivate *priv = self->priv;
  HyScanGtkWaterfallMayerState *state = (HyScanGtkWaterfallMayerState*)&priv->state;

  HyScanGtkWaterfallMayerCheckFunc *check_func = priv->check_func;
  HyScanGtkWaterfallMayerThreadFunc *thread_func = priv->thread_func;
  HyScanGtkWaterfallMayerStopFunc *stop_func = priv->stop_func;
  gpointer thread_data = priv->thread_data;


  while (!g_atomic_int_get (&priv->stop))
    {
      gboolean triggered = FALSE;

      if (check_func != NULL)
        triggered = check_func (state, thread_data);

      /* Ждем сигнализации об изменениях. */
      if (!triggered && !g_atomic_int_get (&priv->cond_flag))
        {
          gboolean triggered;
          gint64 end = g_get_monotonic_time () + G_TIME_SPAN_MILLISECOND * 250;

          g_mutex_lock (&priv->cond_mutex);
          triggered = g_cond_wait_until (&priv->cond, &priv->cond_mutex, end);
          g_mutex_unlock (&priv->cond_mutex);

          /* Если наступил таймаут, повторяем заново. */
          if (!triggered)
            continue;
        }

      // Sync_func;
      if (thread_func != NULL)
        thread_func (state, thread_data, triggered);
    }

  if (stop_func != NULL)
    stop_func (state, thread_data);

  return NULL;
}
