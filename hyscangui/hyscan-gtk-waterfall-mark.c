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
 * @Title HyScanGtkWaterfallMark
 * @Short_description Слой меток водопада
 *
 * Слой HyScanGtkWaterfallMark предназначен для отображения, создания,
 * редактирования и удаления меток.
 */

#include "hyscan-gtk-waterfall-mark.h"
#include "hyscan-gtk-waterfall-tools.h"
#include "hyscan-gtk-waterfall.h"
#include <hyscan-projector.h>
#include <hyscan-mark.h>
#include <math.h>

enum
{
  PROP_MARK_MODEL = 1,
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
  LOCAL_CANCEL,
  LOCAL_REMOVE
};

typedef enum
{
  TASK_NONE,
  TASK_CREATE,
  TASK_MODIFY,
  TASK_REMOVE
} HyScanGtkWaterfallMarkTaskActions;

/* Структура с новыми или создаваемыми метками. */
typedef struct
{
  gchar               *id;        /*< Идентификатор. */
  HyScanMarkWaterfall *mark;      /*< Метка. */
  HyScanCoordinates    center;    /*< Координата центра. */
  HyScanCoordinates    d;         /*< Размеры метки. */
  HyScanGtkWaterfallMarkTaskActions action;    /*< Что требуется сделать. */
} HyScanGtkWaterfallMarkTask;

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
  gboolean                    raw;
  gboolean                    amp_changed;

  gfloat                      ship_speed;
  gboolean                    speed_changed;

  GArray                     *velocity;
  gboolean                    velocity_changed;

  gboolean                    dpt_changed;
} HyScanGtkWaterfallMarkState;

struct _HyScanGtkWaterfallMarkPrivate
{
  HyScanGtkWaterfall         *wfall;
  HyScanObjectModel          *markmodel;

  gboolean                    layer_visibility;

  HyScanGtkWaterfallMarkState new_state;
  HyScanGtkWaterfallMarkState state;
  gboolean                    state_changed;
  GMutex                      state_lock;     /* */

  GHashTable                 *new_marks;
  GMutex                      mmodel_lock;

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

  gint                        mode;           /* Режим слоя */

  HyScanGtkWaterfallMarkTask  current;

  guint64                     mark_filter;

  HyScanCoordinates           handle_to_highlight;
  gboolean                    highlight;

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

static void     hyscan_gtk_waterfall_mark_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_waterfall_mark_set_property            (GObject                 *object,
                                                                   guint                    prop_id,
                                                                   const GValue            *value,
                                                                   GParamSpec              *pspec);
static void     hyscan_gtk_waterfall_mark_object_constructed      (GObject                 *object);
static void     hyscan_gtk_waterfall_mark_object_finalize         (GObject                 *object);

static void     hyscan_gtk_waterfall_mark_added                   (HyScanGtkLayer          *layer,
                                                                   HyScanGtkLayerContainer *container);
static void     hyscan_gtk_waterfall_mark_removed                 (HyScanGtkLayer          *iface);
static gboolean hyscan_gtk_waterfall_mark_grab_input              (HyScanGtkLayer          *iface);
static void     hyscan_gtk_waterfall_mark_set_visible             (HyScanGtkLayer          *iface,
                                                                   gboolean                 visible);
static const gchar * hyscan_gtk_waterfall_mark_get_icon_name      (HyScanGtkLayer          *iface);

static void     hyscan_gtk_waterfall_mark_free_task               (gpointer                 data);
static void     hyscan_gtk_waterfall_mark_clear_task              (gpointer                 data);
static void     hyscan_gtk_waterfall_mark_copy_task               (HyScanGtkWaterfallMarkTask *src,
                                                                   HyScanGtkWaterfallMarkTask *dst);
static HyScanGtkWaterfallMarkTask *
                hyscan_gtk_waterfall_mark_dup_task                (HyScanGtkWaterfallMarkTask *src);
static void     hyscan_gtk_waterfall_mark_add_task                (HyScanGtkWaterfallMark     *self,
                                                                   HyScanGtkWaterfallMarkTask *task);

static void     hyscan_gtk_waterfall_mark_clear_state             (HyScanGtkWaterfallMarkState  *state);
static void     hyscan_gtk_waterfall_mark_model_changed           (HyScanObjectModel            *model,
                                                                   HyScanGtkWaterfallMark  *self);
static gchar *  hyscan_gtk_waterfall_mark_get_track_id            (HyScanDB                *db,
                                                                   const gchar             *project,
                                                                   const gchar             *track);
static void     hyscan_gtk_waterfall_mark_sync_states             (HyScanGtkWaterfallMark  *self);
static gpointer hyscan_gtk_waterfall_mark_processing              (gpointer                 data);

static gint     hyscan_gtk_waterfall_mark_find_by_id              (gconstpointer            a,
                                                                   gconstpointer            b);

static GList *  hyscan_gtk_waterfall_mark_find_closest            (HyScanGtkWaterfallMark  *self,
                                                                   HyScanCoordinates       *pointer,
                                                                   HyScanCoordinates       *point);
static gboolean hyscan_gtk_waterfall_mark_handle_create           (HyScanGtkLayer          *layer,
                                                                   GdkEventButton          *event);
static gboolean hyscan_gtk_waterfall_mark_handle_release          (HyScanGtkLayer          *layer,
                                                                   GdkEventButton          *event,
                                                                   gconstpointer            howner);
static gboolean hyscan_gtk_waterfall_mark_handle_find             (HyScanGtkLayer          *layer,
                                                                   gdouble                  x,
                                                                   gdouble                  y,
                                                                   HyScanGtkLayerHandle    *handle);
static void     hyscan_gtk_waterfall_mark_handle_show             (HyScanGtkLayer          *layer,
                                                                   HyScanGtkLayerHandle    *handle);
static void     hyscan_gtk_waterfall_mark_handle_drag             (HyScanGtkLayer          *layer,
                                                                   GdkEventButton          *event,
                                                                   HyScanGtkLayerHandle    *handle);

static gboolean hyscan_gtk_waterfall_mark_key                     (GtkWidget               *widget,
                                                                   GdkEventKey             *event,
                                                                   HyScanGtkWaterfallMark  *self);
static gboolean hyscan_gtk_waterfall_mark_motion                  (GtkWidget               *widget,
                                                                   GdkEventMotion          *event,
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
static void     hyscan_gtk_waterfall_mark_tile_flags_changed       (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_track_changed           (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_ship_speed_changed      (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);
static void     hyscan_gtk_waterfall_mark_sound_velocity_changed  (HyScanGtkWaterfallState *model,
                                                                   HyScanGtkWaterfallMark  *self);

static guint    hyscan_gtk_waterfall_mark_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallMark, hyscan_gtk_waterfall_mark, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallMark)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_mark_interface_init));

static void
hyscan_gtk_waterfall_mark_class_init (HyScanGtkWaterfallMarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_mark_set_property;
  object_class->constructed = hyscan_gtk_waterfall_mark_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_mark_object_finalize;

  g_object_class_install_property (object_class, PROP_MARK_MODEL,
    g_param_spec_object ("markmodel", "ObjectModel", "HyScanObjectModel",
                         HYSCAN_TYPE_OBJECT_MODEL,
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

  if (prop_id == PROP_MARK_MODEL)
    {
      self->priv->markmodel = g_value_dup_object (value);
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
  g_mutex_init (&priv->mmodel_lock);

  g_cond_init (&priv->cond);

  /* Сигналы Gtk. */
  g_signal_connect (priv->markmodel, "changed",  G_CALLBACK (hyscan_gtk_waterfall_mark_model_changed), self);
  hyscan_gtk_waterfall_mark_set_mark_filter (self, HYSCAN_GTK_WATERFALL_MARKS_ALL);

  gdk_rgba_parse (&mark, "#ea3546");
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
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_mark_object_finalize (GObject *object)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (object);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  g_atomic_int_set (&priv->stop, TRUE);
  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
  g_thread_join (priv->processing);

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  g_signal_handlers_disconnect_by_data (priv->markmodel, self);
  g_clear_object (&priv->markmodel);

  g_clear_pointer (&priv->new_marks, g_hash_table_unref);

  g_mutex_clear (&priv->task_lock);
  g_mutex_clear (&priv->drawable_lock);
  g_mutex_clear (&priv->state_lock);
  g_mutex_clear (&priv->mmodel_lock);

  g_cond_clear (&priv->cond);

  g_list_free_full (priv->tasks, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->visible, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  hyscan_gtk_waterfall_mark_clear_task (&priv->current);

  g_object_unref (priv->font);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_mark_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_mark_added (HyScanGtkLayer          *layer,
                                 HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfallState *wfall;
  HyScanGtkWaterfallMark *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_MARK (layer);
  wfall = HYSCAN_GTK_WATERFALL_STATE (container);
  self->priv->wfall = g_object_ref (wfall);

  /* Сигналы Gtk. */
  g_signal_connect (wfall, "visible-draw",               G_CALLBACK (hyscan_gtk_waterfall_mark_draw), self);
  g_signal_connect (wfall, "configure-event",            G_CALLBACK (hyscan_gtk_waterfall_mark_configure), self);
  g_signal_connect_after (wfall, "motion-notify-event",  G_CALLBACK (hyscan_gtk_waterfall_mark_motion), self);
  g_signal_connect_after (wfall, "key-press-event",      G_CALLBACK (hyscan_gtk_waterfall_mark_key), self);

  /* Сигналы водопада. */
  g_signal_connect (wfall, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_mark_sources_changed), self);
  g_signal_connect (wfall, "changed::tile-flags",   G_CALLBACK (hyscan_gtk_waterfall_mark_tile_flags_changed), self);
  g_signal_connect (wfall, "changed::track",        G_CALLBACK (hyscan_gtk_waterfall_mark_track_changed), self);
  g_signal_connect (wfall, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_mark_ship_speed_changed), self);
  g_signal_connect (wfall, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_mark_sound_velocity_changed), self);
  // g_signal_connect (wfall, "changed::amp-changed",  G_CALLBACK (hyscan_gtk_waterfall_mark_depth_amp_changed), self);
  // g_signal_connect (wfall, "changed::dpt-changed",  G_CALLBACK (hyscan_gtk_waterfall_mark_depth_dpt_changed), self);

  hyscan_gtk_waterfall_mark_sources_changed (wfall, self);
  hyscan_gtk_waterfall_mark_tile_flags_changed (wfall, self);
  hyscan_gtk_waterfall_mark_track_changed (wfall, self);
  hyscan_gtk_waterfall_mark_ship_speed_changed (wfall, self);
  hyscan_gtk_waterfall_mark_sound_velocity_changed (wfall, self);
}

static void
hyscan_gtk_waterfall_mark_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

/* Функция захватывает ввод. */
static gboolean
hyscan_gtk_waterfall_mark_grab_input (HyScanGtkLayer *iface)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (iface);

  /* Мы не можем захватить ввод, если слой отключен. */
  if (!self->priv->layer_visibility)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), self);
  hyscan_gtk_layer_container_set_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), TRUE);
  return TRUE;
}

/* Функция захватывает ввод. */
static void
hyscan_gtk_waterfall_mark_set_visible (HyScanGtkLayer *iface,
                                       gboolean        visible)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (iface);

  self->priv->layer_visibility = visible;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция возвращает название иконки. */
static const gchar *
hyscan_gtk_waterfall_mark_get_icon_name (HyScanGtkLayer *iface)
{
  return "user-bookmarks-symbolic";
}

/* Функция создает новый HyScanProjector. */
static HyScanProjector*
hyscan_gtk_waterfall_mark_open_projector (HyScanGtkWaterfallMark *self,
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

/* Функция освобождает память, занятую структурой HyScanGtkWaterfallMarkTask. */
static void
hyscan_gtk_waterfall_mark_free_task (gpointer data)
{
  HyScanGtkWaterfallMarkTask *task = data;

  hyscan_mark_waterfall_free (task->mark);
  g_free (task->id);
  g_free (task);
}

/* Функция очищает HyScanGtkWaterfallMarkState. */
static void
hyscan_gtk_waterfall_mark_clear_state (HyScanGtkWaterfallMarkState *state)
{
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

  g_clear_pointer (&task->mark, hyscan_mark_waterfall_free);
  g_clear_pointer (&task->id, g_free);
}

/* Функция копирует структуру HyScanGtkWaterfallMarkTask. */
static void
hyscan_gtk_waterfall_mark_copy_task (HyScanGtkWaterfallMarkTask *src,
                                     HyScanGtkWaterfallMarkTask *dst)
{
  *dst = *src;

  if (dst->mark != NULL)
    dst->mark = hyscan_mark_waterfall_copy (src->mark);
  if (dst->id != NULL)
    dst->id = g_strdup (src->id);
}

/* Функция копирует структуру HyScanGtkWaterfallMarkTask. */
static HyScanGtkWaterfallMarkTask *
hyscan_gtk_waterfall_mark_dup_task (HyScanGtkWaterfallMarkTask *src)
{
  HyScanGtkWaterfallMarkTask * dst = g_new0 (HyScanGtkWaterfallMarkTask, 1);

  hyscan_gtk_waterfall_mark_copy_task (src, dst);
  return dst;
}

/* Функция обработки сигнала changed от модели меток. */
static void
hyscan_gtk_waterfall_mark_model_changed (HyScanObjectModel      *model,
                                         HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  GHashTable *marks;

  marks = hyscan_object_model_get (model);
  if (marks == NULL)
    return;

  /* Переписываем метки в private. */
  g_mutex_lock (&priv->mmodel_lock);

  g_clear_pointer (&priv->new_marks, g_hash_table_unref);
  priv->new_marks = g_hash_table_ref (marks);
  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->mmodel_lock);

  g_hash_table_unref (marks);
}

/* Функция определяет идентификатор галса. */
static gchar*
hyscan_gtk_waterfall_mark_get_track_id (HyScanDB    *db,
                                        const gchar *project,
                                        const gchar *track)
{
  gchar* track_id_str = NULL;
  gint32 project_id = 0;      /* Идентификатор проекта. */
  gint32 track_id = 0;        /* Идентификатор проекта. */
  gint32 track_param_id = 0;  /* Идентификатор проекта. */
  HyScanParamList *list = NULL;

  if (db == NULL || project == NULL || track == NULL)
    return NULL;

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

  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, "/id");
  if (!hyscan_db_param_get (db, track_param_id, NULL, list))
    {
      g_warning ("HyScanGtkWaterfallMark: can't get track %s parameters (project %s)", track, project);
      goto exit;
    }

  track_id_str = hyscan_param_list_dup_string (list, "/id");

exit:
  if (track_param_id > 0)
    hyscan_db_close (db, track_param_id);
  if (track_id > 0)
    hyscan_db_close (db, track_id);
  if (project_id> 0)
    hyscan_db_close (db, project_id);

  g_object_unref (list);
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
      cur_st->raw = new_st->raw;

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
hyscan_gtk_waterfall_mark_processing (gpointer data)
{
  HyScanGtkWaterfallMark *self = data;
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  HyScanProjector *lproj = NULL;
  HyScanProjector *rproj = NULL;
  HyScanProjector *_proj = NULL;
  HyScanDepthometer *depth = NULL;

  gchar *track_id = NULL;
  GHashTable *marks;
  GMutex cond_mutex;
  GList *list, *link, *tasks, *next;
  HyScanGtkWaterfallMarkTask *task;
  HyScanSourceType source;
  guint32 index0, count0;
  gboolean status;
  HyScanMarkWaterfall *mark;
  HyScanGtkWaterfallMarkState *state = &priv->state;

  g_mutex_init (&cond_mutex);

  list = NULL;

  /* Тут необходимо забрать метки из модели. */
  hyscan_gtk_waterfall_mark_model_changed (priv->markmodel, self);

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
          hyscan_gtk_waterfall_mark_sync_states (self);
          g_mutex_unlock (&priv->state_lock);

          /* Применяем обновления. */
          if (state->sources_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
              state->sources_changed = FALSE;
            }
          if (state->amp_changed)
            {
              g_clear_object (&lproj);
              g_clear_object (&rproj);
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
              lproj = hyscan_gtk_waterfall_mark_open_projector (self, state->track, state->lsource);
              if (lproj != NULL)
                {
                  hyscan_projector_set_ship_speed (lproj, state->ship_speed);
                  hyscan_projector_set_sound_velocity (lproj, state->velocity);
                }
            }
          if (rproj == NULL)
            {
              rproj = hyscan_gtk_waterfall_mark_open_projector (self, state->track, state->rsource);
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

          if (track_id == NULL)
            {
              track_id = hyscan_gtk_waterfall_mark_get_track_id (state->db,
                                                                 state->project,
                                                                 state->track);
            }
        }

      /* Если в результате синхронизации нет объектов, с которыми можно иметь
       * дело, возвращаемся в начало. */
      if (rproj == NULL || lproj ==NULL)
        continue;

      /* 0. Начинаем работу с метками в БД и свежими. Сначала переписываем
       * всё к себе. */
      g_mutex_lock (&priv->task_lock);
      tasks = priv->tasks;
      priv->tasks = NULL;
      g_mutex_unlock (&priv->task_lock);
      g_mutex_lock (&priv->mmodel_lock);
      marks = hyscan_object_model_copy (priv->new_marks);
      g_mutex_unlock (&priv->mmodel_lock);

      /* 1. Отправляем сделанные пользователем изменения. */
      /* Пересчитываем текущие задачи и отправляем в БД. */
      for (link = tasks; link != NULL; link = link->next)
        {
          task = link->data;
          HyScanCoordinates mc;
          gdouble mw, mh;

          if (task->action == TASK_REMOVE)
            {
              hyscan_object_model_remove_object (priv->markmodel, task->id);
              continue;
            }

          if (state->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
            {
              mc.y = task->center.y;
              mc.x = ABS (task->center.x);
              mw = task->d.x;
              mh = task->d.y;
            }
          else /*if (state->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)*/
            {
              mc.y = task->center.x;
              mc.x = ABS (task->center.y);
              mw = task->d.y;
              mh = task->d.x;
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

          status  = hyscan_projector_coord_to_index (_proj, mc.y, &index0);
          status &= hyscan_projector_coord_to_count (_proj, mc.x, &count0, 0.0);
          if (!status)
            continue;

          if (task->action == TASK_CREATE)
            {
              gint64 mtime = g_get_real_time ();

              gchar *label = g_strdup_printf ("Mark #%u", ++priv->count);
              mark =  hyscan_mark_waterfall_new ();

              hyscan_mark_set_text   ((HyScanMark*)mark, label, "", "");
              hyscan_mark_set_labels ((HyScanMark*)mark, HYSCAN_GTK_WATERFALL_MARKS_ALL);
              hyscan_mark_set_ctime  ((HyScanMark*)mark, mtime);
              hyscan_mark_set_mtime  ((HyScanMark*)mark, mtime);
              hyscan_mark_set_size   ((HyScanMark*)mark, mw, mh);
              hyscan_mark_waterfall_set_center_by_type (mark, source, index0, count0);
              hyscan_mark_waterfall_set_track  (mark, track_id);

              hyscan_object_model_add_object (priv->markmodel, (const HyScanObject *) mark);
              hyscan_mark_waterfall_free (mark);
              g_free (label);
            }
          else if (task->action == TASK_MODIFY)
            {
              mark = hyscan_mark_waterfall_copy (task->mark);

              hyscan_mark_set_mtime ((HyScanMark*)mark, g_get_real_time ());
              hyscan_mark_set_size ((HyScanMark*)mark, mw, mh);
              hyscan_mark_waterfall_set_center_by_type (mark, source, index0, count0);

              hyscan_object_model_modify_object (priv->markmodel, task->id, (const HyScanObject *) mark);
              hyscan_mark_waterfall_free (mark);
            }
        }

      /* 2. Обрабатываем обновления модели.
       * Каждую метку переводим в наши координаты. */
      if (marks != NULL)
        {
          priv->count = 0;
          GHashTableIter iter;
          gchar * id;

          g_hash_table_iter_init (&iter, marks);
          while (g_hash_table_iter_next (&iter, (gpointer)&id, (gpointer)&mark))
            {
              HyScanCoordinates mc;
              gdouble mw, mh;

              priv->count++;
              /* Фильтруем по галсу. */
              if (g_strcmp0 (mark->track, track_id) != 0)
                continue;

              /* Фильтруем по источнику. */
              source = hyscan_source_get_type_by_id (mark->source);
              if (source == state->lsource)
                _proj = lproj;
              else if (source == state->rsource)
                _proj = rproj;
              else
                continue;

              task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
              task->mark = mark;
              task->id = id;
              hyscan_projector_index_to_coord (_proj, mark->index, &mc.y);
              hyscan_projector_count_to_coord (_proj, mark->count, &mc.x, 0.0);
              mw = mark->width;
              mh = mark->height;

              if (state->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
                {
                  task->center.y = mc.y;
                  task->center.x = mc.x;
                  task->d.x = mw;
                  task->d.y = mh;

                  if (source == state->lsource)
                    task->center.x *= -1;

                }
              else if (state->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)
                {
                  task->center.y = - mc.x;
                  task->center.x = mc.y;
                  task->d.x = mh;
                  task->d.y = mw;
                }

              list = g_list_prepend (list, task);

              /* Убираем из ХТ без удаления ключа и стуктуры, т.к.
               * выше используются их указатели. */
              g_hash_table_iter_steal (&iter);
            }
        }

      /* 3. Теперь сливаем список заданий и список меток в БД. */
      for (link = tasks; link != NULL; link = next)
        {
          GList *found;
          next = link->next;
          task = link->data;

          /* Create - просто вставляем созданную метку.
           * Modify - находим такую же и заменяем.
           * Remove - находим такую же и удаляем. */
          if (task->action == TASK_CREATE)
            {
              tasks = g_list_remove_link (tasks, link);
              list = g_list_concat (list, link);
            }
          else if (task->action == TASK_MODIFY)
            {
              found = g_list_find_custom (list, link->data,
                                          hyscan_gtk_waterfall_mark_find_by_id);
              if (found != NULL)
                {
                  list = g_list_remove_link (list, found);
                  g_list_free_full (found, hyscan_gtk_waterfall_mark_free_task);
                  tasks = g_list_remove_link (tasks, link);
                  list = g_list_concat (list, link);
                }
            }
          else
            if (task->action == TASK_REMOVE)
            {
              found = g_list_find_custom (list, link->data,
                                          hyscan_gtk_waterfall_mark_find_by_id);
              if (found != NULL)
                {
                  list = g_list_remove_link (list, found);
                  g_list_free_full (found, hyscan_gtk_waterfall_mark_free_task);
                }
            }
        }

      /* Очищаем скопированное на шаге 0.*/
      g_list_free_full (tasks, hyscan_gtk_waterfall_mark_free_task);
      tasks = NULL;
      g_clear_pointer (&marks, g_hash_table_unref);

      /* Переписываем drawable. */
      g_mutex_lock (&priv->drawable_lock);

      g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
      priv->drawable = list;
      list = NULL;
      g_mutex_unlock (&priv->drawable_lock);

      if (priv->wfall != NULL)
        hyscan_gtk_waterfall_queue_draw (priv->wfall);
    }

  hyscan_gtk_waterfall_mark_clear_state (&priv->state);
  hyscan_gtk_waterfall_mark_clear_state (&priv->new_state);
  g_clear_object (&lproj);
  g_clear_object (&rproj);
  g_clear_object (&depth);
  g_clear_pointer (&track_id, g_free);

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
  HyScanCoordinates returnable = {0, 0};
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  GList *link, *mlink = NULL;
  gdouble rmin = G_MAXDOUBLE;

  for (link = priv->visible; link != NULL; link = link->next)
    {
      gdouble r;
      HyScanGtkWaterfallMarkTask *task = link->data;
      HyScanCoordinates corner;

      r = hyscan_gtk_waterfall_tools_distance (&task->center, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = task->center;
          mlink = link;
        }

      corner.x = task->center.x - task->d.x;
      corner.y = task->center.y - task->d.y;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->center.x + task->d.x;
      corner.y = task->center.y - task->d.y;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->center.x - task->d.x;
      corner.y = task->center.y + task->d.y;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }

      corner.x = task->center.x + task->d.x;
      corner.y = task->center.y + task->d.y;
      r = hyscan_gtk_waterfall_tools_distance (&corner, pointer);
      if (r < rmin)
        {
          rmin = r;
          returnable = corner;
          mlink = link;
        }
    }

  if (mlink == NULL)
    return NULL;

  /* Проверяем, что хендл в радиусе действия. */
  {
    HyScanCoordinates px_pointer, px_handle;
    gdouble thres = HANDLE_RADIUS;

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                           &px_handle.x, &px_handle.y,
                                           returnable.x, returnable.y);
    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->wfall),
                                           &px_pointer.x, &px_pointer.y,
                                           pointer->x, pointer->y);
    if (hyscan_gtk_waterfall_tools_distance (&px_handle, &px_pointer) > thres)
      return NULL;
  }

  *point = returnable;
  return mlink;
}


gboolean
hyscan_gtk_waterfall_mark_handle_create (HyScanGtkLayer *layer,
                                         GdkEventButton *event)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  /* Обязательные проверки: слой не владеет вводом, редактирование запрещено, слой скрыт. */
  if (self != hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall)))
    return FALSE;

  if (priv->mode != LOCAL_EMPTY)
    {
      g_warning ("HyScanGtkWaterfallMark: wrong flow");
      return FALSE;
    }

  priv->mode = LOCAL_CREATE;

  /* Очищаем current. */
  hyscan_gtk_waterfall_mark_clear_task (&priv->current);

  /* Координаты центра. */
  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (priv->wfall), event->x, event->y,
                                         &priv->current.center.x, &priv->current.center.y);
  priv->current.d.x = priv->current.d.y = 0;
  priv->current.action = TASK_CREATE;

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), self);
  return TRUE;
}

gboolean
hyscan_gtk_waterfall_mark_handle_release (HyScanGtkLayer *layer,
                                          GdkEventButton *event,
                                          gconstpointer   howner)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkTask *task = NULL;

  if (self != howner)
    return FALSE;

  /* Отпускание хэндла -- это либо создание новой метки (LOCAL_CREATE),
   * либо завершение редактирования (LOCAL_EDIT_CENTER, LOCAL_EDIT_BORDER) */
  if (priv->mode != LOCAL_CREATE &&
      priv->mode != LOCAL_EDIT_CENTER &&
      priv->mode != LOCAL_EDIT_BORDER)
    {
      return FALSE;
    }

  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  priv->cancellable = NULL;

  task = hyscan_gtk_waterfall_mark_dup_task (&priv->current);
  task->action = priv->mode == LOCAL_CREATE ? TASK_CREATE : TASK_MODIFY;
  hyscan_gtk_waterfall_mark_add_task (self, task);

  priv->mode = LOCAL_EMPTY;

  return TRUE;
}

gboolean
hyscan_gtk_waterfall_mark_handle_find (HyScanGtkLayer       *layer,
                                       gdouble               x,
                                       gdouble               y,
                                       HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);
  HyScanCoordinates mouse = {.x = x, .y = y};
  HyScanCoordinates point;
  GList *link;

  link = hyscan_gtk_waterfall_mark_find_closest (self, &mouse, &point);

  if (link == NULL)
    return FALSE;

  handle->user_data = link;
  handle->val_x = point.x;
  handle->val_y = point.y;

  return TRUE;
}

void
hyscan_gtk_waterfall_mark_handle_show (HyScanGtkLayer       *layer,
                                       HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  priv->highlight = handle != NULL;
  if (handle != NULL)
    {
      priv->handle_to_highlight.x = handle->val_x;
      priv->handle_to_highlight.y = handle->val_y;
    }
}

static void
hyscan_gtk_waterfall_mark_handle_drag (HyScanGtkLayer       *layer,
                                       GdkEventButton       *event,
                                       HyScanGtkLayerHandle *handle)
{
  HyScanGtkWaterfallMark *self = HYSCAN_GTK_WATERFALL_MARK (layer);
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkTask *task;
  gdouble rc, rb;
  HyScanCoordinates mouse, corner;
  gint mode;
  GList *link;

  link = handle->user_data;

  if (link == NULL)
    return;

  task = link->data;

  mouse.x = handle->val_x;
  mouse.y = handle->val_y;
  rc = hyscan_gtk_waterfall_tools_distance (&task->center, &mouse);

  mouse.x = task->center.x + ABS (task->center.x - mouse.x);
  mouse.y = task->center.y + ABS (task->center.y - mouse.y);
  corner.x = task->center.x + ABS (task->d.x);
  corner.y = task->center.y + ABS (task->d.y);
  rb = hyscan_gtk_waterfall_tools_distance (&corner, &mouse);

  if (rc < rb)
    mode = LOCAL_EDIT_CENTER;
  else
    mode = LOCAL_EDIT_BORDER;
  g_message("CENTER I %f %f", task->center.x, task->center.y);
  g_message("DX I %f %f", task->d.x, task->d.y);
  // g_message("MOUSE O  %f %f", mouse.x, mouse.y);
  g_message("CORNER O  %f %f", corner.x, corner.y);
  // g_message("CENTER O  %f %f", task->center.x, task->center.y);
  g_message ("Mode (C2 B3): %i| c%f b%f", mode, rc, rb);

  priv->current.action = TASK_MODIFY;
  priv->mode = mode;

  /* Очищаем current, чтобы ничего не текло. */
  hyscan_gtk_waterfall_mark_clear_task (&priv->current);

  /* Переписываем редактируемую метку из общего списка в current. */
  hyscan_gtk_waterfall_mark_copy_task (task, &priv->current);

  /* Удаляем метку из общего списка. */
  g_mutex_lock (&priv->drawable_lock);
  link = g_list_find_custom (priv->drawable, &priv->current, hyscan_gtk_waterfall_mark_find_by_id);
  priv->drawable = g_list_remove_link (priv->drawable, link);
  g_mutex_unlock (&priv->drawable_lock);

  priv->cancellable = link;

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->wfall), self);
}

/* Функция обрабатывает нажатия клавиш. */
static gboolean
hyscan_gtk_waterfall_mark_key (GtkWidget              *widget,
                               GdkEventKey            *event,
                               HyScanGtkWaterfallMark *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
  HyScanGtkWaterfallMarkTask *task = NULL;

  if (self->priv->mode == LOCAL_EMPTY)
    return GDK_EVENT_PROPAGATE;

  /* Отмена (если есть что отменять). */
  if (event->keyval == GDK_KEY_Escape)
    {
      if (priv->cancellable != NULL)
        {
          g_mutex_lock (&priv->drawable_lock);
          priv->drawable = g_list_concat (priv->drawable, priv->cancellable);
          priv->cancellable = NULL;
          g_mutex_unlock (&priv->drawable_lock);
        }
    }
  /* Удаление. */
  else if (event->keyval == GDK_KEY_Delete)
    {
      if (priv->current.id != NULL)
        {
          task = g_new0 (HyScanGtkWaterfallMarkTask, 1);
          task->id = g_strdup (priv->current.id);
          task->action = TASK_REMOVE;
          hyscan_gtk_waterfall_mark_add_task (self, task);
        }
    }
  else
    {
      return GDK_EVENT_PROPAGATE;
    }

  /* Очищаю cancellable. */
  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  priv->cancellable = NULL;

  /* Сбрасываю режим. */
  priv->mode = LOCAL_EMPTY;

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (widget), NULL);
  return GDK_EVENT_STOP;
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
  switch (priv->mode)
    {
    case LOCAL_CREATE:
    case LOCAL_EDIT_BORDER:
      priv->current.d.x = ABS (coord.x - priv->current.center.x);
      priv->current.d.y = ABS (coord.y - priv->current.center.y);
      break;
    case LOCAL_EDIT_CENTER:
      priv->current.center.x = coord.x;
      priv->current.center.y = coord.y;
      break;
    default:
      break;
    }

  return FALSE;
}

static void
hyscan_gtk_waterfall_mark_add_task (HyScanGtkWaterfallMark     *self,
                                    HyScanGtkWaterfallMarkTask *task)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  if (task == NULL)
    return;

  /* Запихиваем в задания и отрисовываемые. */
  g_mutex_lock (&priv->task_lock);
  priv->tasks = g_list_prepend (priv->tasks, task);
  g_mutex_unlock (&priv->task_lock);

  g_mutex_lock (&priv->drawable_lock);
  priv->drawable = g_list_prepend (priv->drawable, hyscan_gtk_waterfall_mark_dup_task (task));
  g_mutex_unlock (&priv->drawable_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);

  if (priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);
}

/* Функция определяет, видна ли задача на экране. */
static gboolean
hyscan_gtk_waterfall_mark_intersection (HyScanGtkWaterfallMark     *self,
                                        HyScanGtkWaterfallMarkTask *task)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;

  gdouble mx0, my0, mw, mh, dx, dy;
  gdouble vx0, vy0, vw, vh;

  dx = ABS (task->center.x - task->d.x);
  dy = ABS (task->center.y - task->d.y);

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
hyscan_gtk_waterfall_mark_draw_helper (cairo_t           *cairo,
                                       HyScanCoordinates *center,
                                       gdouble            dx,
                                       gdouble            dy,
                                       GdkRGBA           *color,
                                       gdouble            width)
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
  gtk_cifro_area_visible_value_to_point (carea, &corner.x, &corner.y, task->center.x - task->d.x, task->center.y - task->d.y);

  dx = ABS (center.x - corner.x);
  dy = ABS (center.y - corner.y);

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
      gboolean pending;
      HyScanGtkWaterfallMarkTask *task;
      HyScanGtkWaterfallMarkTask *new_task;

      task = link->data;

      /* Фильтруем по лейблу. */
      if (task->mark != NULL && task->mark->labels != 0 && !(task->mark->labels & priv->mark_filter))
        continue;

      /* Отрисовываем метку. */
      pending = task->action != TASK_NONE;
      if (!hyscan_gtk_waterfall_mark_draw_task (self, cairo, task, FALSE, pending))
        continue;

      /* Иначе добавляем в список видимых.*/
      if (!pending)
        {
          new_task = hyscan_gtk_waterfall_mark_dup_task (task);
          priv->visible = g_list_prepend (priv->visible, new_task);
        }
    }
  g_mutex_unlock (&priv->drawable_lock);

  /* Отрисовываем current метку. */
  if (priv->highlight)
    {
      HyScanCoordinates coords;
      gtk_cifro_area_visible_value_to_point (carea,
                                             &coords.x, &coords.y,
                                             priv->handle_to_highlight.x,
                                             priv->handle_to_highlight.y);
      hyscan_gtk_waterfall_mark_draw_handle (cairo,
                                             priv->color.mark,
                                             priv->color.shadow,
                                             coords,
                                             HANDLE_RADIUS);
    }
  if (priv->mode != LOCAL_EMPTY)
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
hyscan_gtk_waterfall_mark_tile_flags_changed (HyScanGtkWaterfallState *model,
                                              HyScanGtkWaterfallMark  *self)
{
  HyScanGtkWaterfallMarkPrivate *priv = self->priv;
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
                                       &priv->new_state.track);
  priv->new_state.amp_changed = TRUE;

  g_mutex_lock (&priv->drawable_lock);
  g_list_free_full (priv->drawable, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->visible, hyscan_gtk_waterfall_mark_free_task);
  g_list_free_full (priv->cancellable, hyscan_gtk_waterfall_mark_free_task);
  priv->drawable = NULL;
  priv->visible = NULL;
  priv->cancellable = NULL;
  g_mutex_unlock (&priv->drawable_lock);
  priv->mode = LOCAL_EMPTY;

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

  priv->new_state.ship_speed = hyscan_gtk_waterfall_state_get_ship_speed (model);
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

  priv->new_state.velocity = hyscan_gtk_waterfall_state_get_sound_velocity (model);
  priv->new_state.velocity_changed = TRUE;

  g_atomic_int_set (&priv->state_changed, TRUE);
  g_mutex_unlock (&priv->state_lock);

  g_atomic_int_inc (&priv->cond_flag);
  g_cond_signal (&priv->cond);
}

/**
 * hyscan_gtk_waterfall_mark_new:
 * @markmodel: указатель на #HyScanObjectModel
 *
 * Функция создает новый объект #HyScanGtkWaterfallMark.
 *
 * Returns: (transfer full): новый объект HyScanGtkWaterfallMark.
 */
HyScanGtkWaterfallMark*
hyscan_gtk_waterfall_mark_new (HyScanObjectModel *markmodel)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_MARK,
                       "markmodel", markmodel,
                       NULL);
}

/**
 * hyscan_gtk_waterfall_mark_set_mark_filter:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @filter: битовая маска меток
 *
 * Функция задает фильтр меток.
 */
void
hyscan_gtk_waterfall_mark_set_mark_filter (HyScanGtkWaterfallMark *self,
                                           guint64                 filter)
{
  HyScanGtkWaterfallMarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));
  priv = self->priv;

  priv->mark_filter = filter;
  if (priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_draw_type:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @type
 *
 * Функция задает тип отображения и пока что не работает.
 */
void
hyscan_gtk_waterfall_mark_set_draw_type (HyScanGtkWaterfallMark     *self,
                                         HyScanGtkWaterfallMarksDraw type)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_main_color:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @color: основной цвет
 *
 * Функция задает основной цвет.
 * Влияет на цвет меток и подписей.
 */
void
hyscan_gtk_waterfall_mark_set_main_color (HyScanGtkWaterfallMark *self,
                                          GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_mark_width:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @width: толщина в пикселях
 *
 * Функция задает толщину основных линий.
 */
void
hyscan_gtk_waterfall_mark_set_mark_width (HyScanGtkWaterfallMark *self,
                                          gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.mark_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_frame_color:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @color: цвет рамки
 *
 * Функция задает цвет рамки вокруг текста.
 */
void
hyscan_gtk_waterfall_mark_set_frame_color (HyScanGtkWaterfallMark *self,
                                           GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.frame = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_shadow_color:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @color: цвет подложки
 *
 * Функция задает цвет подложки.
 * Подложка рисуется под текстом и линиями метки.
 */
void
hyscan_gtk_waterfall_mark_set_shadow_color (HyScanGtkWaterfallMark *self,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_shadow_width:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @width: ширина линий
 *
 * Функция задает ширину линий подложки.
 */
void
hyscan_gtk_waterfall_mark_set_shadow_width (HyScanGtkWaterfallMark *self,
                                            gdouble                 width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.shadow_width = width;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/**
 * hyscan_gtk_waterfall_mark_set_blackout_color:
 * @mark: указатель на объект #HyScanGtkWaterfallMark
 * @color: цвет затемнения
 *
 * Функция задает цвет затемнения. Затемнение рисуется в момент создания метки.
 */
void
hyscan_gtk_waterfall_mark_set_blackout_color (HyScanGtkWaterfallMark     *self,
                                              GdkRGBA                     color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_MARK (self));

  self->priv->color.blackout = color;
  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_mark_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_mark_added;
  iface->removed = hyscan_gtk_waterfall_mark_removed;
  iface->grab_input = hyscan_gtk_waterfall_mark_grab_input;
  iface->set_visible = hyscan_gtk_waterfall_mark_set_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_mark_get_icon_name;
  iface->handle_create = hyscan_gtk_waterfall_mark_handle_create;
  iface->handle_release = hyscan_gtk_waterfall_mark_handle_release;
  iface->handle_find = hyscan_gtk_waterfall_mark_handle_find;
  iface->handle_show = hyscan_gtk_waterfall_mark_handle_show;
  iface->handle_drag = hyscan_gtk_waterfall_mark_handle_drag;
}
