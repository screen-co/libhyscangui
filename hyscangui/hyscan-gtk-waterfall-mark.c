#include <hyscan-depthometer.h>
#include <hyscan-depth-acoustic.h>
#include <hyscan-depth-nmea.h>
#include <hyscan-mark-manager.h>
#include <hyscan-projector.h>
#include <hyscan-tile-color.h>
#include "hyscan-gtk-waterfall-mark.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>

enum
{
  PROP_WATERFALL = 1
};

/* Состояния объекта. */
enum
{
  EMPTY,
  CREATE,
  EDIT_CENTER,
  EDIT_BORDER,
  EDIT,
  DELETE
};

typedef enum
{
  TASK_CREATE,
  TASK_MODIFY,
  TASK_DELETE
} HyScanGtkWaterfallMarkTaskActions;

/* Структура с новыми или создаваемыми метками. */
typedef struct
{
  gchar               *id;        /*< Идентификатор. */
  HyScanWaterfallMark *mark;      /*< Метка. */
  HyScanCoordinates    center;    /*< Координата центра. */
  gdouble              dx;        /*< Ширина. */
  gdouble              dy;        /*< Высота. */
  HyScanCoordinates    px_center; /*< Координата центра. */
  gdouble              px_dx;     /*< Ширина. */
  gdouble              px_dy;     /*< Высота. */
  HyScanGtkWaterfallMarkTaskActions action;    /*< Что требуется сделать. */
} HyScanGtkWaterfallMarkTask;

/* Стейты. */
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
  HyScanGtkWaterfallState    *wfall;

  HyScanGtkWaterfallMarkState new_state;
  HyScanGtkWaterfallMarkState state;
  gboolean                    state_changed;
  GMutex                      state_lock;     /* */

  GThread                    *processing;
  gint                        stop;

  GList                      *tasks;          /* Список меток, которые нужно отправить в БД. */
  GMutex                      task_lock;      /* Блокировка списка. */

  GList                      *drawable;       /* Все метки галса (с учетом используемых источников данных). */
  GMutex                      drawable_lock;  /* Блокировка списка. */

  GList                      *visible;        /* Список меток, которые реально есть на экране. */

  gint                        mode; // edit or create or wat
  gint                        mouse_mode; // edit or create or wat

  HyScanCoordinates           press;
  HyScanCoordinates           release;

  HyScanGtkWaterfallMarkTask  current;

  struct
  {
    HyScanCoordinates         _0;
    HyScanCoordinates         _1;
  } view;

  struct
  {
    GdkRGBA                   mark;
    GdkRGBA                   pend;
    gdouble                   mark_width;
    GdkRGBA                   shadow;
    gdouble                   shadow_width;
    GdkRGBA                   blackout;
  } color;

  guint             count;

  PangoLayout      *font;              /* Раскладка шрифта. */
  gint              text_height;
};

static void     hyscan_gtk_waterfall_mark_interface_init          (HyScanGtkWaterfallLayerInterface *iface);
static void     hyscan_gtk_waterfall_mark_set_property            (GObject                 *object,
                                                                   guint                    prop_id,
                                                                   const GValue            *value,
                                                                   GParamSpec              *pspec);
static void     hyscan_gtk_waterfall_mark_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_mark_object_finalize         (GObject                 *object);

static void     hyscan_gtk_waterfall_mark_free_task               (gpointer                 data);
static void     hyscan_gtk_waterfall_mark_sync_states             (HyScanGtkWaterfallMark  *self);
static gpointer hyscan_gtk_waterfall_mark_processing              (gpointer                 data);

static gint     hyscan_gtk_waterfall_mark_find_by_id              (gconstpointer            a,
                                                                   gconstpointer            b);
static gpointer hyscan_gtk_waterfall_mark_handle                  (HyScanGtkWaterfallState *state,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_mouse_button_press      (GtkWidget               *widget,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_mouse_button_release    (GtkWidget               *widget,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_mouse_motion            (GtkWidget               *widget,
                                                                   GdkEventMotion          *event,
                                                                   HyScanGtkWaterfallMark  *self);

static gdouble  hyscan_gtk_waterfall_mark_radius                  (HyScanCoordinates       *start,
                                                                   HyScanCoordinates       *end);

static gboolean hyscan_gtk_waterfall_mark_intersection            (HyScanGtkWaterfallMark     *self,
                                                                   HyScanGtkWaterfallMarkTask *task);
static gboolean hyscan_gtk_waterfall_mark_draw_task               (HyScanGtkWaterfallMark  *self,
                                                                   cairo_t                 *cairo,
                                                                   HyScanGtkWaterfallMarkTask *task,
                                                                   gboolean                 blackout,
                                                                   gboolean                 pending);
static void     hyscan_gtk_waterfall_mark_draw                    (GtkWidget               *widget,
                                                                   cairo_t                 *cairo,
                                                                   HyScanGtkWaterfallMark  *self);

static gboolean hyscan_gtk_waterfall_mark_configure               (GtkWidget               *widget,
                                                                  GdkEventConfigure       *event,
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
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget", HYSCAN_TYPE_GTK_WATERFALL_STATE,
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
  GdkRGBA mark, shadow, blackout;
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->constructed (object);

  g_mutex_init (&priv->task_lock);
  g_mutex_init (&priv->drawable_lock);
  g_mutex_init (&priv->state_lock);

  /* Сигналы Gtk. */
  g_signal_connect (priv->wfall, "visible-draw",             G_CALLBACK (hyscan_gtk_waterfall_mark_draw), self);
  g_signal_connect (priv->wfall, "configure-event",          G_CALLBACK (hyscan_gtk_waterfall_mark_configure), self);
  g_signal_connect (priv->wfall, "button-press-event",       G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_button_press), self);
  g_signal_connect (priv->wfall, "button-release-event",     G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_button_release), self);
  g_signal_connect (priv->wfall, "motion-notify-event",      G_CALLBACK (hyscan_gtk_waterfall_mark_mouse_motion), self);

  g_signal_connect (priv->wfall, "handle", G_CALLBACK (hyscan_gtk_waterfall_mark_handle), self);
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

  hyscan_gtk_waterfall_mark_sources_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_tile_type_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_profile_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_cache_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_track_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_ship_speed_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_sound_velocity_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_depth_source_changed (priv->wfall, self);
  hyscan_gtk_waterfall_mark_depth_params_changed (priv->wfall, self);

  hyscan_gtk_waterfall_mark_set_mark_filter (self, HYSCAN_GTK_WATERFALL_MARKS_ALL);

  gdk_rgba_parse (&mark, "magenta");
  gdk_rgba_parse (&shadow, "black");
  gdk_rgba_parse (&blackout, "rgba(0.0,0.0,0.0,0.5)");

  gdk_rgba_parse (&priv->color.pend, "rgba(100.0,100.0,100.0,0.75)");

  hyscan_gtk_waterfall_mark_set_mark_color (self, mark);
  hyscan_gtk_waterfall_mark_set_mark_width (self, 3);
  hyscan_gtk_waterfall_mark_set_shadow_color (self, shadow);
  hyscan_gtk_waterfall_mark_set_shadow_width (self, 5);
  hyscan_gtk_waterfall_mark_set_blackout_color (self, blackout);

  priv->mode = EMPTY;

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

  hyscan_gtk_waterfall_state_set_input_owner (self->priv->wfall, self);
  hyscan_gtk_waterfall_state_set_changes_allowed (self->priv->wfall, TRUE);
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
  //hyscan_depthometer_set_validity_time (depth, priv->state.depth_time);

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
  g_free (task->id);
  g_free (task);
}

/* Функция очищает структуру HyScanGtkWaterfallMarkTask. Без освобождения! */
static void
hyscan_gtk_waterfall_mark_clear_task (gpointer data)
{
  HyScanGtkWaterfallMarkTask *task = data;

  g_clear_pointer (&task->mark, hyscan_waterfall_mark_deep_free);
  g_clear_pointer (&task->id, g_free);
}

/* Функция очищает структуру HyScanGtkWaterfallMarkTask. Без освобождения! */
static void
hyscan_gtk_waterfall_mark_copy_task (HyScanGtkWaterfallMarkTask *src,
                                     HyScanGtkWaterfallMarkTask *dst)
{
  *dst = *src;
  dst->mark = hyscan_waterfall_mark_copy (src->mark);
  dst->id = g_strdup (src->id);
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
  HyScanProjector         *_proj = NULL;
  HyScanDepth             *idepth = NULL;
  HyScanDepthometer       *depth = NULL;
  HyScanWaterfallMarkData *mdata = NULL;

  GList *list, *link;
  HyScanGtkWaterfallMarkTask *task;
  HyScanSourceType source;
  guint32 index0, count0;//, width, height;
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
                  //hyscan_depthometer_set_validity_time (depth, state->depth_time);
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

      /* Чтобы как можно меньше задерживать mainloop, копируем список задач. */
      g_mutex_lock (&priv->task_lock);
      list = priv->tasks;
      priv->tasks = NULL;
      g_mutex_unlock (&priv->task_lock);

      /* Пересчитываем текущие задачи и отправляем в БД. */
      for (link = list; link != NULL; link = link->next)
        {
          refresh = TRUE;

          task = link->data;

          if (task->action == TASK_DELETE)
            {
              hyscan_waterfall_mark_data_remove (mdata, task->id);
              continue;
            }

          if (task->center.x >= 0)
            {
              source = state->rsource;
              _proj = rproj;
            }
          else
            {
              source = state->lsource;
              _proj = lproj;
            }

          status  = hyscan_projector_coord_to_index (_proj, task->center.y, &index0);
          status &= hyscan_projector_coord_to_count (_proj, ABS (task->center.x), &count0, 0.0);

          if (!status)
            continue;

          if (task->action == TASK_CREATE)
            {
              gchar *name = g_strdup_printf ("Mark #%u", ++priv->count);
              hyscan_waterfall_mark_data_add_full (mdata, name, "description", "operator",
                                                   HYSCAN_GTK_WATERFALL_MARKS_ALL,
                                                   g_get_real_time (), g_get_real_time (),
                                                   source, index0, count0, task->dx * 1000, task->dy * 1000);
              g_free (name);
            }
          else if (task->action == TASK_MODIFY)
            {
              hyscan_waterfall_mark_data_modify_full (mdata,
                                                      task->id,
                                                      task->mark->name,
                                                      task->mark->description,
                                                      task->mark->operator_name,
                                                      task->mark->labels,
                                                      task->mark->creation_time,
                                                      g_get_real_time (),
                                                      source, index0, count0,
                                                      task->dx * 1000, task->dy * 1000);
            }
        }

      /* Очищаем весь список заданий. */
      g_list_free_full (list, hyscan_gtk_waterfall_mark_free_task);
      list = NULL;

      /* Проверяем счётчик изменений и при необходимости
       * забираем из БД обновленный список меток. */
      mc = hyscan_waterfall_mark_data_get_mod_count (mdata);

      /* Кстати, это можно переписать в две строки вместо девяти.
       * Корректно, но абсолютно нечитаемо.
       * if (!((mc == oldmc ? 0 : (oldmc = mc, 1)) + (!refresh ? 0 : (refresh = TRUE, 1))))
       *   continue;
       */
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

      priv->count = nids;

      if (ids == NULL)
        goto next;

      /* Каждую метку переводим в наши координаты. */
      for (i = 0; i < nids; i++)
        {
          HyScanWaterfallMark *mark;

          mark = hyscan_waterfall_mark_data_get (mdata, ids[i]);

          if (mark == NULL)
            goto ignore;

          /* Фильтруем по лейблу. */
          if (mark->labels && !(mark->labels & state->mark_filter))
            goto ignore;

          /* Фильтруем по источнику. */
          if (mark->source0 == state->lsource)
            _proj = lproj;
          else if (mark->source0 == state->rsource)
            _proj = rproj;
          else
            goto ignore;



          task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
          task->mark = mark;
          task->id = ids[i];
          hyscan_projector_index_to_coord (_proj, mark->index0, &task->center.y);
          hyscan_projector_count_to_coord (_proj, mark->count0, &task->center.x, 0.0);
          task->dx = mark->width / 1000.0;
          task->dy = mark->height / 1000.0;

          if (mark->source0 == state->lsource)
            task->center.x *= -1;

          list = g_list_prepend (list, task);
          continue;

ignore:
          hyscan_waterfall_mark_deep_free (mark);
          g_free (ids[i]);
        }

      g_free (ids);

next:
      g_mutex_lock (&priv->drawable_lock);
      g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
      priv->drawable = list;
      list = NULL;
      g_mutex_unlock (&priv->drawable_lock);
      hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (priv->wfall));
    }

  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&idepth);
  g_clear_object (&depth);
  g_clear_object (&mdata);

  return NULL;
}

static gint
hyscan_gtk_waterfall_mark_find_by_id (gconstpointer _a,
                                      gconstpointer _b)
{
  const HyScanGtkWaterfallMarkTask *a = _a;
  const HyScanGtkWaterfallMarkTask *b = _b;

  if (a->id == NULL || b->id == NULL)
    g_message ("Null id also occures!");

  return g_strcmp0 (a->id, b->id);
}

static gpointer
hyscan_gtk_waterfall_mark_handle (HyScanGtkWaterfallState *state,
                                  GdkEventButton          *event,
                                  HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkTask *task;
  HyScanCoordinates mouse, corner;
  gdouble thres;
  gint mode;
  GList *link;

  mouse.x = event->x;
  mouse.y = event->y;

  if (hyscan_gtk_waterfall_tools_radius (&priv->press, &mouse) > 2)
    return NULL;

  thres = priv->color.shadow_width;

  /* Предполагается, что редактировать можно только метки в БД.
   * Ещё не отправленные редактировать нельзя. */
  for (link = priv->visible; link != NULL; link = link->next)
    {
      gdouble rc, rtl, rtr, rbl, rbr;
      task = link->data;

      rc = hyscan_gtk_waterfall_tools_radius (&task->px_center, &mouse);

      corner.x = task->px_center.x - task->px_dx;
      corner.y = task->px_center.y - task->px_dy;
      rtl = hyscan_gtk_waterfall_tools_radius (&corner, &mouse);

      corner.x = task->px_center.x + task->px_dx;
      corner.y = task->px_center.y - task->px_dy;
      rtr = hyscan_gtk_waterfall_tools_radius (&corner, &mouse);

      corner.x = task->px_center.x - task->px_dx;
      corner.y = task->px_center.y + task->px_dy;
      rbl = hyscan_gtk_waterfall_tools_radius (&corner, &mouse);

      corner.x = task->px_center.x + task->px_dx;
      corner.y = task->px_center.y + task->px_dy;
      rbr = hyscan_gtk_waterfall_tools_radius (&corner, &mouse);

      if (rc < thres)
        mode = EDIT_CENTER;
      else if (rtl < thres || rtr < thres || rbl < thres || rbr < thres)
        mode = EDIT_BORDER;
      else
        continue;

      /* Очищаем current, чтобы ничего не текло. */
      hyscan_gtk_waterfall_mark_clear_task (&priv->current);

      /* Переписываем редактируемую метку из общего списка в current. */
      hyscan_gtk_waterfall_mark_copy_task (task, &priv->current);
      priv->current.action = TASK_MODIFY;
      priv->mode = mode;
      priv->mouse_mode = mode;

      /* Удаляем метку из общего списка. */
      link = g_list_find_custom (priv->drawable, task, hyscan_gtk_waterfall_mark_find_by_id);
      priv->drawable = g_list_remove_link (priv->drawable, link);
      g_list_free_full (link, hyscan_gtk_waterfall_mark_free_task);

      return self;
    }

  return NULL;
}

static gboolean
hyscan_gtk_waterfall_mark_mouse_button_press (GtkWidget              *widget,
                                              GdkEventButton         *event,
                                              HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  /* Просто запомним координату, где нажата кнопка мыши. */
  priv->press.x = event->x;
  priv->press.y = event->y;

  return FALSE;
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_mark_mouse_button_processor (GtkWidget              *widget,
                                                  GdkEventButton         *event,
                                                  HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  /* Мы оказываемся в этой функции только когда функция
   * hyscan_gtk_waterfall_mark_mouse_button_release решила,
   * что мы имеем право обработать это воздействие.
   *
   * priv->mode фактически показывает, каким будет состояние ПЕРЕД
   * следующим воздействием.
   *
   * EMPTY -> CREATE -> EMPTY
   * EDIT_* -> EDIT -> EMPTY
   *
   */

  if (priv->mode == EMPTY)
    {
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wfall, self);
      priv->mouse_mode = priv->mode = CREATE;

      /* Очищаем current. */
      hyscan_gtk_waterfall_mark_clear_task (&priv->current);

      /* Координаты центра. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), event->x, event->y,
                                             &priv->current.center.x, &priv->current.center.y);
      priv->current.dx = priv->current.dy = 0;
      priv->current.action = TASK_CREATE;
    }

  /* Если начало редактирования */
  else if (priv->mode == EDIT_CENTER || priv->mode == EDIT_BORDER)
    {
      priv->mode = EDIT;
    }
  else if (priv->mode == EDIT || priv->mode == CREATE)
    {
      HyScanGtkWaterfallMarkTask *new;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wfall, NULL);

      new = g_new0 (HyScanGtkWaterfallMarkTask, 1);
      *new = priv->current;
      new->id = g_strdup (priv->current.id);
      new->mark = hyscan_waterfall_mark_copy (priv->current.mark);

      g_mutex_lock (&priv->task_lock);
      priv->tasks = g_list_prepend (priv->tasks, new);
      g_mutex_unlock (&priv->task_lock);

      priv->mouse_mode = priv->mode = EMPTY;
    }

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_mark_mouse_button_release (GtkWidget              *widget,
                                                GdkEventButton         *event,
                                                HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  gconstpointer howner, iowner;

  /* Игнорируем нажатия всех кнопок, кроме левой. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  /* Слишком большое перемещение говорит о том, что пользователь двигает область.  */
  priv->release.x = event->x;
  priv->release.y = event->y;
  if (hyscan_gtk_waterfall_mark_radius (&priv->press, &priv->release) > 2)
    return FALSE;

  /* Проверяем режим (просмотр/редактирование). */
  if (!hyscan_gtk_waterfall_state_get_changes_allowed (priv->wfall))
    return FALSE;

  howner = hyscan_gtk_waterfall_state_get_handle_grabbed (priv->wfall);
  iowner = hyscan_gtk_waterfall_state_get_input_owner (priv->wfall);

  if (howner == self || (howner == NULL && iowner == self))
    {
      hyscan_gtk_waterfall_mark_mouse_button_processor (widget, event, self);
      return TRUE;
    }

  return FALSE;
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_mark_mouse_motion (GtkWidget              *widget,
                                        GdkEventMotion         *event,
                                        HyScanGtkWaterfallMark *self)
{
  HyScanCoordinates coord;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget),
                                         event->x, event->y,
                                         &coord.x, &coord.y);
  switch (priv->mouse_mode)
    {
    case CREATE:
    case EDIT_BORDER:
      priv->current.dx = ABS (coord.x - priv->current.center.x);
      priv->current.dy = ABS (coord.y - priv->current.center.y);
      break;
    case EDIT_CENTER:
      priv->current.center.x = coord.x;
      priv->current.center.y = coord.y;
      break;
    default:
      break;
    }

  return FALSE;
}

/* Функция вычисляет расстояние между точками. */
static gdouble
hyscan_gtk_waterfall_mark_radius (HyScanCoordinates *start,
                                  HyScanCoordinates *end)
{
  return sqrt (pow (ABS(end->x - start->x), 2) + pow (ABS(end->y - start->y), 2));
}

static gboolean
hyscan_gtk_waterfall_mark_intersection (HyScanGtkWaterfallMark     *self,
                                        HyScanGtkWaterfallMarkTask *task)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  gdouble mx0, my0, mw, mh, dx, dy;
  gdouble vx0, vy0, vw, vh;

  dx = ABS (task->center.x - task->dx);
  dy = ABS (task->center.y - task->dy);

  mx0 = task->center.x - dx;
  my0 = task->center.y - dy;
  mw  = 2 * dx;
  mh  = 2 * dy;

  vx0 = priv->view._0.x;
  vy0 = priv->view._0.y;
  vw  = priv->view._1.x - priv->view._0.x;
  vh  = priv->view._1.y - priv->view._0.y;

  if (mx0 + mw < vx0 || my0 + mh < vy0 || vx0 + vw < mx0 || vy0 + vh < my0)
    return FALSE;

  return TRUE;
}

static void
hyscan_gtk_waterfall_mark_draw_helper (cairo_t                    *cairo,
                                       HyScanCoordinates          *center,
                                       gdouble                     dx,
                                       gdouble                     dy,
                                       GdkRGBA                    *color,
                                       gdouble                     width)
{
  gdouble x = center->x;
  gdouble y = center->y;

  cairo_set_source_rgba (cairo, color->red, color->green, color->blue, color->alpha);

  /* Центр. */
  cairo_arc (cairo, x, y, width, 0, 2 * G_PI);
  cairo_fill (cairo);

  /* Углы. */
  cairo_arc (cairo, x - dx, y - dy, width, 0, 2 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo, x - dx, y + dy, width, 0, 2 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo, x + dx, y - dy, width, 0, 2 * G_PI);
  cairo_fill (cairo);
  cairo_arc (cairo, x + dx, y + dy, width, 0, 2 * G_PI);
  cairo_fill (cairo);
  cairo_stroke (cairo);

  /* Граница. */
  cairo_set_line_width (cairo, width);
  cairo_rectangle (cairo, x - dx, y - dy, 2 * dx, 2 * dy);
  cairo_stroke (cairo);
}
/* Функция отрисовки одной метки.*/
static gboolean
hyscan_gtk_waterfall_mark_draw_task (HyScanGtkWaterfallMark     *self,
                                     cairo_t                    *cairo,
                                     HyScanGtkWaterfallMarkTask *task,
                                     gboolean                    blackout,
                                     gboolean                    pending)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->wfall);
  PangoLayout *font = priv->font;

  HyScanCoordinates center;
  HyScanCoordinates corner;
  gdouble dx, dy;
  guint w, h;

  gtk_cifro_area_get_size (carea, &w, &h);

  /* Если метка не попадет на экран, игнорируем её. */
  if (!hyscan_gtk_waterfall_mark_intersection (self, task))
    return FALSE;

  /* Иначе показываем на экране и добавляем в список видимых меток. */
  gtk_cifro_area_visible_value_to_point (carea, &center.x, &center.y, task->center.x, task->center.y);
  gtk_cifro_area_visible_value_to_point (carea, &corner.x, &corner.y, task->center.x - task->dx, task->center.y - task->dy);

  dx = ABS (center.x - corner.x); // TODO ABS
  dy = ABS (center.y - corner.y);

  task->px_center = center;
  task->px_dx = dx;
  task->px_dy = dy;

  /* Рисуем тень под элементами. */
  hyscan_gtk_waterfall_mark_draw_helper (cairo, &center, dx, dy, &priv->color.shadow, priv->color.shadow_width);
  /* Рисуем сами элементы. */
  hyscan_gtk_waterfall_mark_draw_helper (cairo, &center, dx, dy,
                                         pending ? &priv->color.pend : &priv->color.mark,
                                         priv->color.mark_width);

  /* Подпись. */
  if (task->mark != NULL && task->mark->name != NULL)
    {
      gint text_width;

      cairo_set_source_rgba (cairo, priv->color.mark.red, priv->color.mark.green, priv->color.mark.blue, priv->color.mark.alpha);
      pango_layout_set_text (font, task->mark->name, -1);


      pango_layout_get_size (font, &text_width, NULL);
      text_width /= PANGO_SCALE;
      cairo_move_to (cairo, center.x - text_width / 2.0, center.y + ABS (dy) /*+ priv->text_height*/);
      pango_cairo_show_layout (cairo, font);
    }

  /* Рисуем затемнение. */
  if (blackout)
    {
      cairo_set_source_rgba (cairo, priv->color.blackout.red, priv->color.blackout.green,
                             priv->color.blackout.blue, priv->color.blackout.alpha);
      cairo_rectangle (cairo, 0, 0, w, h);
      cairo_rectangle (cairo, center.x - dx, center.y - dy, 2 * dx, 2 * dy);
      cairo_set_fill_rule (cairo, CAIRO_FILL_RULE_EVEN_ODD);
      cairo_fill (cairo);
    }

  cairo_stroke (cairo);

  return TRUE;
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_mark_draw (GtkWidget              *widget,
                                cairo_t                *cairo,
                                HyScanGtkWaterfallMark *self)
{
  GList *link;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  /* Актуализируем пределы видимой области. */
  gtk_cifro_area_get_view (carea, &priv->view._0.x, &priv->view._1.x, &priv->view._0.y, &priv->view._1.y);

  cairo_save (cairo);

  /* Очищаем visible. */
  g_list_free_full (priv->visible, hyscan_gtk_waterfall_mark_free_task);
  priv->visible = NULL;

  /* Отрисовываем drawable. */
  g_mutex_lock (&priv->drawable_lock);
  for (link = priv->drawable; link != NULL; link = link->next)
    {
      gboolean visible;
      HyScanGtkWaterfallMarkTask *task;
      HyScanGtkWaterfallMarkTask *new;

      task = link->data;

      /* Отрисовываем метку. */
      visible = hyscan_gtk_waterfall_mark_draw_task (self, cairo, task, FALSE, FALSE);

      /* Если она не видима, движемся дальше.*/
      if (!visible)
        continue;

      /* Иначе добавляем в список видимых.*/
      new = g_new0 (HyScanGtkWaterfallMarkTask, 1);
      hyscan_gtk_waterfall_mark_copy_task (task, new);
      priv->visible = g_list_prepend (priv->visible, new);
    }
  g_mutex_unlock (&priv->drawable_lock);

  /* Отрисовываем те метки, которые ещё не в БД. */
  g_mutex_lock (&priv->task_lock);
  for (link = priv->tasks; link != NULL; link = link->next)
    {
      HyScanGtkWaterfallMarkTask *task = link->data;
      hyscan_gtk_waterfall_mark_draw_task (self, cairo, task, FALSE, TRUE);
    }
  g_mutex_unlock (&priv->task_lock);

  /* Отрисовываем current метку. */
  if (priv->mode != EMPTY)
    {
      HyScanGtkWaterfallMarkTask new = {.mark = NULL};

      new = priv->current;
      hyscan_gtk_waterfall_mark_draw_task (self, cairo, &new, TRUE, FALSE);
    }

  cairo_restore (cairo);
}

static gboolean
hyscan_gtk_waterfall_mark_configure (GtkWidget              *widget,
                                     GdkEventConfigure      *event,
                                     HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  gint text_height;

  /* Текущий шрифт приложения. */
  g_clear_pointer (&priv->font, g_object_unref);
  priv->font = gtk_widget_create_pango_layout (widget, NULL);

  pango_layout_set_text (priv->font, "0123456789qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM.,?!", -1);
  pango_layout_get_size (priv->font, NULL, &text_height);

  priv->text_height = text_height / PANGO_SCALE;

  return FALSE;
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
hyscan_gtk_waterfall_mark_new (HyScanGtkWaterfallState *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MARK,
                       "waterfall", waterfall,
                       NULL);
}

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
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

void
hyscan_gtk_waterfall_mark_set_mark_color (HyScanGtkWaterfallMark *self,
                                          GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark = color;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

void
hyscan_gtk_waterfall_mark_set_mark_width (HyScanGtkWaterfallMark *self,
                                          gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark_width = width;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

void
hyscan_gtk_waterfall_mark_set_shadow_color (HyScanGtkWaterfallMark *self,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow = color;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}
void
hyscan_gtk_waterfall_mark_set_shadow_width (HyScanGtkWaterfallMark *self,
                                            gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow_width = width;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

void
hyscan_gtk_waterfall_mark_set_blackout_color (HyScanGtkWaterfallMark     *self,
                                              GdkRGBA                     color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.blackout = color;
  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (self->priv->wfall));
}

static void
hyscan_gtk_waterfall_mark_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_mark_grab_input;
  iface->get_mnemonic = hyscan_gtk_waterfall_mark_get_mnemonic;
}
