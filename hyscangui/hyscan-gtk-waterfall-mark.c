/*
 * \file hyscan-gtk-waterfall-mark.c
 *
 * \brief Исходный файл слоя меток
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <hyscan-depthometer.h>
#include <hyscan-depth-nmea.h>
#include <hyscan-mark-manager.h>
#include <hyscan-projector.h>
#include <hyscan-tile-color.h>
#include "hyscan-gtk-waterfall-mark.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>
#include <hyscan-waterfall-mark.h>

enum
{
  PROP_WATERFALL = 1
};

enum
{
  SIGNAL_SELECTED,
  SIGNAL_LAST
};

/* Состояния объекта. */
enum
{
  LOCAL_EMPTY,
  LOCAL_CREATE,
  LOCAL_EDIT_CENTER,
  LOCAL_EDIT_BORDER,
  LOCAL_EDIT,
  LOCAL_CANCEL,
  LOCAL_REMOVE
};

typedef enum
{
  TASK_CREATE,
  TASK_MODIFY,
  TASK_REMOVE
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
  HyScanGtkWaterfallState    *wf_state;
  HyScanGtkWaterfall         *wfall;

  gboolean                    layer_visibility;

  HyScanGtkWaterfallMarkState new_state;
  HyScanGtkWaterfallMarkState state;
  gboolean                    state_changed;
  GMutex                      state_lock;     /* */

  GThread                    *processing;
  gint                        stop;
  gint                        cond_flag;
  GCond                       cond;

  GList                      *tasks;          /* Список меток, которые нужно отправить в БД. */
  GMutex                      task_lock;      /* Блокировка списка. */

  GList                      *drawable;       /* Все метки галса (с учетом используемых источников данных). */
  GMutex                      drawable_lock;  /* Блокировка списка. */

  GList                      *visible;        /* Список меток, которые реально есть на экране. */
  GList                      *cancellable;    /* Последняя метка (для отмены изменений). */

  gint                        mode; // edit or create or wat
  gint                        mouse_mode; // edit or create or wat

  HyScanCoordinates           press;
  HyScanCoordinates           release;
  HyScanCoordinates           pointer;

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
    GdkRGBA                   frame;
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
static void     hyscan_gtk_waterfall_mark_clear_state             (HyScanGtkWaterfallMarkState  *state);
static void     hyscan_gtk_waterfall_mark_clear_task              (gpointer                 data);
static gchar*   hyscan_gtk_waterfall_mark_get_track_id            (HyScanDB                *db,
                                                                   const gchar             *project,
                                                                   const gchar             *track);
static void     hyscan_gtk_waterfall_mark_sync_states             (HyScanGtkWaterfallMark  *self);
static gpointer hyscan_gtk_waterfall_mark_processing              (gpointer                 data);

static gint     hyscan_gtk_waterfall_mark_find_by_id              (gconstpointer            a,
                                                                   gconstpointer            b);
static gpointer hyscan_gtk_waterfall_mark_handle                  (HyScanGtkWaterfallState *state,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_key                     (GtkWidget               *widget,
                                                                   GdkEventKey             *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_button                  (GtkWidget               *widget,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_motion                  (GtkWidget               *widget,
                                                                   GdkEventMotion          *event,
                                                                   HyScanGtkWaterfallMark  *self);

static gboolean hyscan_gtk_waterfall_mark_mouse_interaction_processor (GtkWidget              *widget,
                                                                       HyScanGtkWaterfallMark *self);
static gboolean hyscan_gtk_waterfall_mark_interaction_resolver    (GtkWidget               *widget,
                                                                   GdkEventAny             *event,
                                                                   HyScanGtkWaterfallMark  *self);
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

static guint    hyscan_gtk_waterfall_mark_signals[SIGNAL_LAST] = {0};

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
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget",
                         HYSCAN_TYPE_GTK_WATERFALL_STATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_gtk_waterfall_mark_signals[SIGNAL_SELECTED] =
    g_signal_new ("selected", HYSCAN_TYPE_GTK_WATERFALL_MARK,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
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
    {
      self->priv->wfall = g_value_dup_object (value);
      self->priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_mark_object_constructed (GObject *object)
{
  GdkRGBA mark, shadow, blackout, frame;
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->constructed (object);

  g_mutex_init (&priv->task_lock);
  g_mutex_init (&priv->drawable_lock);
  g_mutex_init (&priv->state_lock);

  g_cond_init (&priv->cond);

  /* Сигналы Gtk. */
  g_signal_connect (priv->wf_state, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_mark_draw), self);
  g_signal_connect (priv->wf_state, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_mark_configure), self);
  g_signal_connect_after (priv->wf_state, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_mark_motion), self);
  g_signal_connect_after (priv->wf_state, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_mark_interaction_resolver), self);
  g_signal_connect_after (priv->wf_state, "button-release-event", G_CALLBACK (hyscan_gtk_waterfall_mark_interaction_resolver), self);
  g_signal_connect_after (priv->wf_state, "button-press-event",   G_CALLBACK (hyscan_gtk_waterfall_mark_button), self);

  g_signal_connect (priv->wf_state, "handle", G_CALLBACK (hyscan_gtk_waterfall_mark_handle), self);

  /* Сигналы модели водопада. */
  g_signal_connect (priv->wf_state, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_mark_sources_changed), self);
  g_signal_connect (priv->wf_state, "changed::tile-type",    G_CALLBACK (hyscan_gtk_waterfall_mark_tile_type_changed), self);
  g_signal_connect (priv->wf_state, "changed::track",        G_CALLBACK (hyscan_gtk_waterfall_mark_track_changed), self);
  g_signal_connect (priv->wf_state, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_mark_ship_speed_changed), self);
  g_signal_connect (priv->wf_state, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_mark_sound_velocity_changed), self);
  g_signal_connect (priv->wf_state, "changed::depth-source", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_source_changed), self);
  g_signal_connect (priv->wf_state, "changed::depth-params", G_CALLBACK (hyscan_gtk_waterfall_mark_depth_params_changed), self);
  g_signal_connect (priv->wf_state, "changed::cache",        G_CALLBACK (hyscan_gtk_waterfall_mark_cache_changed), self);

  hyscan_gtk_waterfall_mark_sources_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_tile_type_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_cache_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_track_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_ship_speed_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_sound_velocity_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_depth_source_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_mark_depth_params_changed (priv->wf_state, self);

  hyscan_gtk_waterfall_mark_set_mark_filter (self, HYSCAN_GTK_WATERFALL_MARKS_ALL);

  gdk_rgba_parse (&mark, "#f754e1");
  gdk_rgba_parse (&shadow, SHADOW_DEFAULT);
  gdk_rgba_parse (&blackout, SHADOW_DEFAULT);
  gdk_rgba_parse (&frame, FRAME_DEFAULT);

  gdk_rgba_parse (&priv->color.pend, SHADOW_DEFAULT);
  blackout.alpha = 0.5;

  hyscan_gtk_waterfall_mark_set_main_color (self, mark);
  hyscan_gtk_waterfall_mark_set_mark_width (self, 1);
  hyscan_gtk_waterfall_mark_set_frame_color (self, frame);
  hyscan_gtk_waterfall_mark_set_shadow_color (self, shadow);
  hyscan_gtk_waterfall_mark_set_shadow_width (self, 3);
  hyscan_gtk_waterfall_mark_set_blackout_color (self, blackout);

  priv->mode = LOCAL_EMPTY;

  priv->stop = FALSE;
  priv->processing = g_thread_new ("gtk-wf-mark", hyscan_gtk_waterfall_mark_processing, self);

  /* Включаем видимость слоя. */
  hyscan_gtk_waterfall_layer_set_visible (HYSCAN_GTK_WATERFALL_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_mark_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
  g_atomic_int_set (&priv->stop, TRUE);
  g_thread_join (priv->processing);

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wf_state, self);

  g_clear_object (&priv->wf_state);

  g_mutex_clear (&priv->task_lock);
  g_mutex_clear (&priv->drawable_lock);
  g_mutex_clear (&priv->state_lock);

  g_cond_clear (&priv->cond);

  g_list_free_full (priv->tasks, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->visible, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  hyscan_gtk_waterfall_mark_clear_task (&priv->current);

  g_object_unref (priv->font);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->finalize (object);
}

/* Функция захватывает ввод. */
static void
hyscan_gtk_waterfall_mark_grab_input (HyScanGtkWaterfallLayer *iface)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (iface);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!self->priv->layer_visibility)
    return;

  hyscan_gtk_waterfall_state_set_input_owner (self->priv->wf_state, self);
  hyscan_gtk_waterfall_state_set_changes_allowed (self->priv->wf_state, TRUE);
}

/* Функция захватывает ввод. */
static void
hyscan_gtk_waterfall_mark_set_visible (HyScanGtkWaterfallLayer *iface,
                                       gboolean                 visible)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (iface);

  self->priv->layer_visibility = visible;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функуция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_mark_get_mnemonic (HyScanGtkWaterfallLayer *iface)
{
  return "user-bookmarks-symbolic";
}

/* Функция создает новый HyScanDepth. */
static HyScanDepth*
hyscan_gtk_waterfall_mark_open_depth (HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkState *state = &priv->state;
  HyScanDepth *idepth = NULL;

  if (state->db == NULL || state->project == NULL || state->track == NULL)
    return NULL;

  if (HYSCAN_SOURCE_NMEA_DPT == state->depth_source)
    {
      HyScanDepthNMEA *dnmea;
      dnmea = hyscan_depth_nmea_new (state->db,
                                     state->project,
                                     state->track,
                                     state->depth_channel);
      idepth = HYSCAN_DEPTH (dnmea);
    }

  if (idepth == NULL)
    return NULL;

  hyscan_depth_set_cache (idepth, state->cache, state->prefix);
  return idepth;
}

/* Функция создает новый HyScanDepthometer. */
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

/* Функция создает новый HyScanProjector. */
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

/* Функция очищает HyScanGtkWaterfallMarkState. */
static void
hyscan_gtk_waterfall_mark_clear_state (HyScanGtkWaterfallMarkState *state)
{
  g_clear_object (&state->cache);
  g_clear_pointer (&state->prefix, g_free);
  g_clear_object (&state->db);
  g_clear_pointer (&state->project, g_free);
  g_clear_pointer (&state->track, g_free);
  g_clear_pointer (&state->velocity, g_array_unref);
}

/* Функция очищает структуру HyScanGtkWaterfallMarkTask. Без освобождения! */
static void
hyscan_gtk_waterfall_mark_clear_task (gpointer data)
{
  HyScanGtkWaterfallMarkTask *task = data;

  hyscan_waterfall_mark_deep_free (task->mark);
  g_free (task->id);

  task->mark = NULL;
  task->id = NULL;
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

/* Функция определяет идентификатор галса. */
static gchar*
hyscan_gtk_waterfall_mark_get_track_id (HyScanDB    *db,
                                        const gchar *project,
                                        const gchar *track)
{
  gchar* track_id_str = NULL;
  gint32 project_id = 0;      /* Идентификатор проекта. */
  gint32 track_id = 0;      /* Идентификатор проекта. */
  gint32 track_param_id = 0;      /* Идентификатор проекта. */

  project_id = hyscan_db_project_open (db, project);
  if (project_id <= 0)
    {
      g_warning ("HyScanGtkWaterfallMark: can't open project %s", project);
      goto exit;
    }

  track_id = hyscan_db_track_open (db, project_id, track);
  if (track_id <= 0)
    {
      g_warning ("HyScanGtkWaterfallMark: can't open track %s (project %s)", track, project);
      goto exit;
    }

  track_param_id = hyscan_db_track_param_open (db, track_id);
  if (track_param_id <= 0)
    {
      g_warning ("HyScanGtkWaterfallMark: can't open track %s parameters (project %s)", track, project);
      goto exit;
    }

  track_id_str = hyscan_db_param_get_string (db, track_param_id, NULL, "/id");

exit:
  if (track_param_id > 0)
    hyscan_db_close (db, track_param_id);
  if (track_id > 0)
    hyscan_db_close (db, track_id);
  if (project_id> 0)
    hyscan_db_close (db, project_id);

  return track_id_str;
}
/* Функция синхронизирует состояния. */
static void
hyscan_gtk_waterfall_mark_sync_states (HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkState *new_st = &priv->new_state;
  HyScanGtkWaterfallMarkState *cur_st = &priv->state;

  if (new_st->sources_changed)
    {
      cur_st->display_type = new_st->display_type;
      cur_st->lsource = new_st->lsource;
      cur_st->rsource = new_st->rsource;

      new_st->sources_changed = FALSE;
      cur_st->sources_changed = TRUE;
    }
  if (new_st->track_changed)
    {
      /* Очищаем текущее. */
      g_clear_object (&cur_st->db);
      g_clear_pointer (&cur_st->project, g_free);
      g_clear_pointer (&cur_st->track, g_free);

      /* Копируем из нового. */
      cur_st->db = new_st->db;
      cur_st->project = new_st->project;
      cur_st->track = new_st->track;
      cur_st->raw = new_st->raw;

      /* Очищаем новое. */
      new_st->db = NULL;
      new_st->project = NULL;
      new_st->track = NULL;

      /* Выставляем флаги. */
      new_st->track_changed = FALSE;
      cur_st->track_changed = TRUE;
    }

  if (new_st->cache_changed)
    {
      g_clear_object (&cur_st->cache);
      g_clear_pointer (&cur_st->prefix, g_free);

      cur_st->cache = new_st->cache;
      cur_st->prefix = new_st->prefix;

      new_st->cache = NULL;
      new_st->prefix = NULL;

      new_st->cache_changed = FALSE;
      cur_st->cache_changed = TRUE;
    }

  if (new_st->depth_source_changed)
    {
      cur_st->depth_source = new_st->depth_source;
      cur_st->depth_channel = new_st->depth_channel;

      new_st->depth_source_changed = FALSE;
      cur_st->depth_source_changed = TRUE;
    }
  if (new_st->depth_params_changed)
    {
      cur_st->depth_time = new_st->depth_time;
      cur_st->depth_size = new_st->depth_size;

      new_st->depth_params_changed = FALSE;
      cur_st->depth_params_changed = TRUE;
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
  if (new_st->mark_filter_changed)
    {
      cur_st->mark_filter = new_st->mark_filter;

      cur_st->mark_filter_changed = TRUE;
      new_st->mark_filter_changed = FALSE;
    }
}

/* Поток получения и отправки меток. */
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

  gchar* track_id = NULL;

  GMutex cond_mutex;
  GList *list, *link;
  HyScanGtkWaterfallMarkTask *task;
  HyScanSourceType source;
  guint32 index0, count0;
  gboolean status;

  guint32 mc = 0, oldmc = 0;
  gchar **ids;
  guint nids, i;
  HyScanGtkWaterfallMarkState *state = &priv->state;

  g_mutex_init (&cond_mutex);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Проверяем счётчик изменений. */
      if (mdata != NULL)
        mc = hyscan_waterfall_mark_data_get_mod_count (mdata);

      /* Ждем сигнализации об изменениях. */
      if (mc == oldmc && g_atomic_int_get (&priv->cond_flag) == 0)
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
              g_clear_pointer (&track_id, g_free);
              state->track_changed = FALSE;
              mc = oldmc = 0;
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
                mdata = hyscan_waterfall_mark_data_new (state->db, state->project);

              if (mdata != NULL)
                {
                  oldmc = ~hyscan_waterfall_mark_data_get_mod_count (mdata);
                  track_id = hyscan_gtk_waterfall_mark_get_track_id (state->db, state->project, state->track);
                  if (track_id == NULL)
                    g_clear_object (&mdata);
                }
            }
          if (depth == NULL)
            {
              if (idepth == NULL)
                idepth = hyscan_gtk_waterfall_mark_open_depth (self);
              depth = hyscan_gtk_waterfall_mark_open_depthometer (self, idepth);
            }
        }

      /* Если в результате синхронизации нет объектов, с которыми можно иметь
       * дело, возвращаемся в начало. */
      if (mdata == NULL || rproj == NULL || lproj ==NULL)
        continue;

      /* Чтобы как можно меньше задерживать mainloop, копируем список задач. */
      g_mutex_lock (&priv->task_lock);
      list = priv->tasks;
      priv->tasks = NULL;
      g_mutex_unlock (&priv->task_lock);

      /* Пересчитываем текущие задачи и отправляем в БД. */
      for (link = list; link != NULL; link = link->next)
        {
          task = link->data;

          if (task->action == TASK_REMOVE)
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
              HyScanWaterfallMark mark = {0};

              mark.track             = g_strdup (track_id);
              mark.name              = g_strdup_printf ("Mark #%u", ++priv->count);
              mark.description       = g_strdup_printf ("description");
              mark.operator_name     = g_strdup_printf ("operator");
              mark.labels            = HYSCAN_GTK_WATERFALL_MARKS_ALL;
              mark.creation_time     = g_get_real_time ();
              mark.modification_time = g_get_real_time ();
              mark.source0           = source;
              mark.index0            = index0;
              mark.count0            = count0;
              mark.width             = task->dx * 1000;
              mark.height            = task->dy * 1000;

              hyscan_waterfall_mark_data_add (mdata, &mark);
              hyscan_waterfall_mark_free (&mark);
            }
          else if (task->action == TASK_MODIFY)
            {
              HyScanWaterfallMark mark = {0};

              mark.track             = g_strdup (task->mark->track);
              mark.name              = g_strdup (task->mark->name);
              mark.description       = g_strdup (task->mark->description);
              mark.operator_name     = g_strdup (task->mark->operator_name);
              mark.labels            = task->mark->labels;
              mark.creation_time     = task->mark->creation_time;
              mark.modification_time = g_get_real_time ();
              mark.source0           = source;
              mark.index0            = index0;
              mark.count0            = count0;
              mark.width             = task->dx * 1000;
              mark.height            = task->dy * 1000;

              hyscan_waterfall_mark_data_modify (mdata, task->id, &mark);
              hyscan_waterfall_mark_free (&mark);
            }
        }

      /* Очищаем список задач (напомню, что он предварительно скопирован). */
      g_list_free_full (list, hyscan_gtk_waterfall_mark_free_task);
      list = NULL;

      oldmc = hyscan_waterfall_mark_data_get_mod_count (mdata);

      /* Получаем список идентификаторов. */
      ids = hyscan_waterfall_mark_data_get_ids (mdata, &nids);
      priv->count = nids;

      /* Каждую метку переводим в наши координаты. */
      for (i = 0; i < nids; i++)
        {
          HyScanWaterfallMark *mark;

          mark = hyscan_waterfall_mark_data_get (mdata, ids[i]);

          if (mark == NULL)
            goto ignore;

          /* Фильтруем по галсу. */
          if (g_strcmp0 (mark->track, track_id) != 0)
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

      g_mutex_lock (&priv->drawable_lock);
      g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
      priv->drawable = list;
      list = NULL;
      g_mutex_unlock (&priv->drawable_lock);
      hyscan_gtk_waterfall_queue_draw (priv->wfall);
    }

  hyscan_gtk_waterfall_mark_clear_state (&priv->state);
  hyscan_gtk_waterfall_mark_clear_state (&priv->new_state);
  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&idepth);
  g_clear_object (&depth);
  g_clear_object (&mdata);

  g_free (track_id);

  return NULL;
}

/* Функция ищет метку по идентификатору. */
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

/* Функция ищет ближайшую к указателю метку. */
static GList*
hyscan_gtk_waterfall_mark_find_closest (HyScanGtkWaterfallMark *self,
                                        HyScanCoordinates      *pointer,
                                        HyScanCoordinates      *point)
{
  HyScanCoordinates returnable;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  gdouble thres = HANDLE_RADIUS;
  GList *link, *mlink = NULL;
  gdouble rmin = G_MAXDOUBLE;

  for (link = priv->visible; link != NULL; link = link->next)
    {
      gdouble r;
      HyScanGtkWaterfallMarkTask *task = link->data;
      HyScanCoordinates corner;

      r = hyscan_gtk_waterfall_tools_distance (&task->px_center, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = task->px_center;
          mlink = link;
        }

      corner.x = task->px_center.x - task->px_dx;
      corner.y = task->px_center.y - task->px_dy;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->px_center.x + task->px_dx;
      corner.y = task->px_center.y - task->px_dy;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->px_center.x - task->px_dx;
      corner.y = task->px_center.y + task->px_dy;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->px_center.x + task->px_dx;
      corner.y = task->px_center.y + task->px_dy;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

    }

  if (point != NULL)
    *point = returnable;

  if (rmin > thres)
    return NULL;

  return mlink;
}

/* Функция определяет, есть ли хэндл под указателем. */
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

  /* Мы не можем обрабатывать действия, если слой отключен. */
  if (!priv->layer_visibility)
    return NULL;

  if (hyscan_gtk_waterfall_tools_distance(&priv->press, &mouse) > 2)
    return NULL;

  thres = priv->color.shadow_width;

  /* Предполагается, что редактировать можно только метки в БД.
   * Ещё не отправленные редактировать нельзя. */
  link = hyscan_gtk_waterfall_mark_find_closest (self, &mouse, NULL);

  if (link == NULL)
    return NULL;

  gdouble rc, rtl, rtr, rbl, rbr;
  task = link->data;

  rc = hyscan_gtk_waterfall_tools_distance(&task->px_center, &mouse);

  corner.x = task->px_center.x - task->px_dx;
  corner.y = task->px_center.y - task->px_dy;
  rtl = hyscan_gtk_waterfall_tools_distance(&corner, &mouse);

  corner.x = task->px_center.x + task->px_dx;
  corner.y = task->px_center.y - task->px_dy;
  rtr = hyscan_gtk_waterfall_tools_distance(&corner, &mouse);

  corner.x = task->px_center.x - task->px_dx;
  corner.y = task->px_center.y + task->px_dy;
  rbl = hyscan_gtk_waterfall_tools_distance(&corner, &mouse);

  corner.x = task->px_center.x + task->px_dx;
  corner.y = task->px_center.y + task->px_dy;
  rbr = hyscan_gtk_waterfall_tools_distance(&corner, &mouse);


  if (rc < MIN (MIN(rtl, rtr), MIN(rbl, rbr)))
    mode = LOCAL_EDIT_CENTER;
  else
    mode = LOCAL_EDIT_BORDER;

  /* Очищаем current, чтобы ничего не текло. */
  hyscan_gtk_waterfall_mark_clear_task (&priv->current);

  /* Переписываем редактируемую метку из общего списка в current. */
  hyscan_gtk_waterfall_mark_copy_task (task, &priv->current);
  priv->current.action = TASK_MODIFY;
  priv->mode = mode;
  priv->mouse_mode = mode;

  return self;
}

/* Функция обрабатывает нажатия клавиш. */
static gboolean
hyscan_gtk_waterfall_mark_key (GtkWidget              *widget,
                               GdkEventKey            *event,
                               HyScanGtkWaterfallMark *self)
{
  if (self->priv->mode == LOCAL_EMPTY)
    return FALSE;

  if (event->keyval == GDK_KEY_Escape)
    self->priv->mode = LOCAL_CANCEL;
  else if (event->keyval == GDK_KEY_Delete)
    self->priv->mode = LOCAL_REMOVE;
  else
    return FALSE;

  return hyscan_gtk_waterfall_mark_mouse_interaction_processor (widget, self);
}

/* Функция обрабатывает нажатия кнопок мыши. */
static gboolean
hyscan_gtk_waterfall_mark_button (GtkWidget              *widget,
                                  GdkEventButton         *event,
                                  HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  /* Игнорируем нажатия всех кнопок, кроме левой. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  /* Если это НАЖАТИЕ, то просто запомним координату. */
  if (event->type == GDK_BUTTON_PRESS)
    {
      priv->press.x = event->x;
      priv->press.y = event->y;
      return FALSE;
    }

  /* Игнорируем события двойного и тройного нажатия. */
  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  priv->release.x = event->x;
  priv->release.y = event->y;

  /* Слишком большое перемещение говорит о том, что пользователь двигает область.  */
  if (hyscan_gtk_waterfall_tools_distance (&priv->press, &priv->release) > 2)
    return FALSE;

  return hyscan_gtk_waterfall_mark_mouse_interaction_processor (widget, self);
}

/* В эту функцию вынесена обработка нажатия. */
static gboolean
hyscan_gtk_waterfall_mark_mouse_interaction_processor (GtkWidget              *widget,
                                                       HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkTask *task = NULL;
  /* Мы оказываемся в этой функции только когда функция
   * hyscan_gtk_waterfall_mark_mouse_button_release решила,
   * что мы имеем право обработать это воздействие.
   *
   * priv->mode фактически показывает, каким будет состояние ПЕРЕД
   * следующим воздействием.
   *
   * LOCAL_EMPTY -> LOCAL_CREATE -> LOCAL_EMPTY
   * LOCAL_EDIT_* -> LOCAL_EDIT -> LOCAL_EMPTY
   *
   */

  if (priv->mode == LOCAL_EMPTY)
    {
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, self);
      priv->mouse_mode = priv->mode = LOCAL_CREATE;

      /* Очищаем current. */
      hyscan_gtk_waterfall_mark_clear_task (&priv->current);

      /* Координаты центра. */
      gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (widget), priv->release.x, priv->release.y,
                                             &priv->current.center.x, &priv->current.center.y);
      priv->current.dx = priv->current.dy = 0;
      priv->current.action = TASK_CREATE;
    }

  /* Если начало редактирования */
  else if (priv->mode == LOCAL_EDIT_CENTER || priv->mode == LOCAL_EDIT_BORDER)
    {
      GList *link;
      HyScanGtkWaterfallMarkTask *emit;

      /* Удаляем метку из общего списка. */
      g_mutex_lock (&priv->drawable_lock);
      link = g_list_find_custom (priv->drawable, &priv->current, hyscan_gtk_waterfall_mark_find_by_id);
      priv->drawable = g_list_remove_link (priv->drawable, link);
      g_mutex_unlock (&priv->drawable_lock);

      priv->cancellable = link;

      priv->mode = LOCAL_EDIT;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, self);

      emit = link->data;

      g_signal_emit (self, hyscan_gtk_waterfall_mark_signals[SIGNAL_SELECTED],
                     0, emit->id);

    }
  else if (priv->mode == LOCAL_EDIT || priv->mode == LOCAL_CREATE)
    {

      task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
      hyscan_gtk_waterfall_mark_copy_task (&priv->current, task);

      priv->mouse_mode = priv->mode = LOCAL_EMPTY;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, NULL);

      g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
      priv->cancellable = NULL;
    }

  else if (priv->mode == LOCAL_CANCEL || priv->mode == LOCAL_REMOVE)
    {
      if (priv->cancellable == NULL)
        goto reset;

      if (priv->mode == LOCAL_CANCEL)
        {
          g_mutex_lock (&priv->drawable_lock);
          priv->drawable = g_list_append (priv->drawable, priv->cancellable->data);
          g_mutex_unlock (&priv->drawable_lock);
          priv->cancellable = g_list_delete_link (priv->cancellable, priv->cancellable);
        }

      if (priv->mode == LOCAL_REMOVE)
        {
          g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
          priv->cancellable = NULL;

          task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
          task->id = g_strdup (priv->current.id);
          task->action = TASK_REMOVE;
        }

reset:
      priv->mouse_mode = priv->mode = LOCAL_EMPTY;
      hyscan_gtk_waterfall_state_set_handle_grabbed (priv->wf_state, NULL);
    }

  if (task != NULL)
    {
      g_mutex_lock (&priv->task_lock);
      priv->tasks = g_list_prepend (priv->tasks, task);
      g_mutex_unlock (&priv->task_lock);

      g_atomic_int_inc (&priv->cond_flag);
      g_cond_signal (&priv->cond);
    }

  hyscan_gtk_waterfall_queue_draw (priv->wfall);
  return FALSE;
}

/* Функция определяет возможность обработки нажатия. */
static gboolean
hyscan_gtk_waterfall_mark_interaction_resolver (GtkWidget               *widget,
                                                GdkEventAny             *event,
                                                HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallState *state = self->priv->wf_state;
  gconstpointer howner, iowner;

  /* Проверяем режим (просмотр/редактирование). */
  if (!hyscan_gtk_waterfall_state_get_changes_allowed (state))
    return FALSE;

  howner = hyscan_gtk_waterfall_state_get_handle_grabbed (state);
  iowner = hyscan_gtk_waterfall_state_get_input_owner (state);

  /* Можно обработать это взаимодействие пользователя в следующих случаях:
   * - howner - это мы (в данный момент идет обработка)
   * - под указателем есть хэндл, за который можно схватиться
   * - хэндла нет, но мы - iowner.
   */

  if ((howner != self) && (howner != NULL || iowner != self))
    return FALSE;

  /* Обработка кнопок мыши. */
  if (event->type == GDK_BUTTON_RELEASE)
    hyscan_gtk_waterfall_mark_button (widget, (GdkEventButton*)event, self);
  else if (event->type == GDK_KEY_PRESS)
    hyscan_gtk_waterfall_mark_key (widget, (GdkEventKey*)event, self);

  return TRUE;
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_mark_motion (GtkWidget              *widget,
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
    case LOCAL_CREATE:
    case LOCAL_EDIT_BORDER:
      priv->current.dx = ABS (coord.x - priv->current.center.x);
      priv->current.dy = ABS (coord.y - priv->current.center.y);
      break;
    case LOCAL_EDIT_CENTER:
      priv->current.center.x = coord.x;
      priv->current.center.y = coord.y;
      break;
    default:
      break;
    }

  priv->pointer.x = event->x;
  priv->pointer.y = event->y;
  return FALSE;
}

/* Функция определяет, видна ли задача на экране. */
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

/* Функция отрисовки хэндла. */
void
hyscan_gtk_waterfall_mark_draw_handle  (cairo_t          *cairo,
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
hyscan_gtk_waterfall_mark_draw_helper (cairo_t                    *cairo,
                                       HyScanCoordinates          *center,
                                       gdouble                     dx,
                                       gdouble                     dy,
                                       GdkRGBA                    *color,
                                       gdouble                     width)
{
  gdouble x = center->x;
  gdouble y = center->y;

  hyscan_cairo_set_source_gdk_rgba (cairo, color);

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
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->wf_state);
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

  dx = ABS (center.x - corner.x);
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
      gint w, h;

      pango_layout_set_text (font, task->mark->name, -1);
      pango_layout_get_size (font, &w, &h);

      w /= PANGO_SCALE;
      h /= PANGO_SCALE;

      hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.shadow);
      cairo_rectangle (cairo, center.x - w / 2.0, center.y + dy + priv->color.shadow_width, w, h);
      cairo_fill (cairo);
      hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.frame);
      cairo_rectangle (cairo, center.x - w / 2.0, center.y + dy + priv->color.shadow_width, w, h);
      cairo_stroke (cairo);

      cairo_move_to (cairo, center.x - w / 2.0, center.y + dy + priv->color.shadow_width /*+ priv->text_height*/);

      hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.mark);
      pango_cairo_show_layout (cairo, font);
    }

  /* Рисуем затемнение. */
  if (blackout)
    {
      hyscan_cairo_set_source_gdk_rgba (cairo, &priv->color.blackout);
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

  /* Проверяем видимость слоя. */
  if (!priv->layer_visibility)
    return;

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
      HyScanGtkWaterfallMarkTask *new_task;

      task = link->data;

      /* Отрисовываем метку. */
      visible = hyscan_gtk_waterfall_mark_draw_task (self, cairo, task, FALSE, FALSE);

      /* Если она не видима, движемся дальше.*/
      if (!visible)
        continue;

      /* Иначе добавляем в список видимых.*/
      new_task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
      hyscan_gtk_waterfall_mark_copy_task (task, new_task);
      priv->visible = g_list_prepend (priv->visible, new_task);
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
  if (priv->mode == LOCAL_EMPTY)
    {
      HyScanCoordinates handle;
      link = hyscan_gtk_waterfall_mark_find_closest (self, &priv->pointer, &handle);
      if (link != NULL)
        {
          hyscan_gtk_waterfall_mark_draw_handle (cairo, priv->color.mark, priv->color.shadow,
                                                 handle, HANDLE_RADIUS);
        }
    }
  else /*if (priv->mode == LOCAL_EMPTY)*/
    {
      HyScanGtkWaterfallMarkTask new_task = {.mark = NULL};

      new_task = priv->current;
      hyscan_gtk_waterfall_mark_draw_task (self, cairo, &new_task, TRUE, FALSE);

    }

  cairo_restore (cairo);
}

/* Функция обрабатывает смену параметров виджета. */
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
hyscan_gtk_waterfall_mark_sources_changed (HyScanGtkWaterfallState *model,
                                           HyScanGtkWaterfallMark  *self)
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

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену типа тайлов. */
static void
hyscan_gtk_waterfall_mark_tile_type_changed (HyScanGtkWaterfallState *model,
                                             HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_tile_type (model, &priv->new_state.tile_type);
  priv->new_state.tile_type_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену системы кэширования. */
static void
hyscan_gtk_waterfall_mark_cache_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfallMark  *self)
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

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену БД, проекта, галса. */
static void
hyscan_gtk_waterfall_mark_track_changed (HyScanGtkWaterfallState *model,
                                         HyScanGtkWaterfallMark  *self)
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

  g_mutex_lock (&priv->drawable_lock);
  g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->visible, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  priv->drawable = NULL;
  priv->visible = NULL;
  priv->cancellable = NULL;
  g_mutex_unlock (&priv->drawable_lock);
  priv->mode = priv->mouse_mode = LOCAL_EMPTY;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену скорости судна. */
static void
hyscan_gtk_waterfall_mark_ship_speed_changed (HyScanGtkWaterfallState *model,
                                              HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_ship_speed (model, &priv->new_state.ship_speed);
  priv->new_state.speed_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену профиля скорости звука. */
static void
hyscan_gtk_waterfall_mark_sound_velocity_changed (HyScanGtkWaterfallState *model,
                                                  HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_sound_velocity (model, &priv->new_state.velocity);
  priv->new_state.velocity_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену источников данных глубины. */
static void
hyscan_gtk_waterfall_mark_depth_source_changed (HyScanGtkWaterfallState *model,
                                                HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_depth_source (model,
                                               &priv->new_state.depth_source,
                                               &priv->new_state.depth_channel);
  priv->new_state.depth_source_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция обрабатывает смену параметров определения глубины. */
static void
hyscan_gtk_waterfall_mark_depth_params_changed (HyScanGtkWaterfallState *model,
                                                HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  g_mutex_lock (&priv->state_lock);

  hyscan_gtk_waterfall_state_get_depth_time (model, &priv->new_state.depth_time);
  hyscan_gtk_waterfall_state_get_depth_filter_size (model, &priv->new_state.depth_size);
  priv->new_state.depth_params_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/* Функция создает новый объект HyScanGtkWaterfallMark. */
HyScanGtkWaterfallMark*
hyscan_gtk_waterfall_mark_new (HyScanGtkWaterfall *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MARK,
                       "waterfall", waterfall,
                       NULL);
}

/* Функция задает фильтр меток. */
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

/* Функция задает тип отображения и пока что не работает. */
void
hyscan_gtk_waterfall_mark_set_draw_type (HyScanGtkWaterfallMark     *self,
                                         HyScanGtkWaterfallMarksDraw type)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает основной цвет. */
void
hyscan_gtk_waterfall_mark_set_main_color (HyScanGtkWaterfallMark *self,
                                          GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает толщину основных линий. */
void
hyscan_gtk_waterfall_mark_set_mark_width (HyScanGtkWaterfallMark *self,
                                          gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark_width = width;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает цвет рамки вокруг текста. */
void
hyscan_gtk_waterfall_mark_set_frame_color (HyScanGtkWaterfallMark *self,
                                           GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.frame = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает цвет подложки. */
void
hyscan_gtk_waterfall_mark_set_shadow_color (HyScanGtkWaterfallMark *self,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}
/* Функция задает ширину линий подложки. */
void
hyscan_gtk_waterfall_mark_set_shadow_width (HyScanGtkWaterfallMark *self,
                                            gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow_width = width;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает цвет затемнения. */
void
hyscan_gtk_waterfall_mark_set_blackout_color (HyScanGtkWaterfallMark     *self,
                                              GdkRGBA                     color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.blackout = color;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_mark_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_mark_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_mark_set_visible;
  iface->get_mnemonic = hyscan_gtk_waterfall_mark_get_mnemonic;
}
