/*
 * \file hyscan-gtk-waterfall-state.c
 *
 * \brief Исходный файл класса хранения параметров водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-state.h"
#include <hyscan-gui-marshallers.h>
#include <string.h>

enum
{
  SIGNAL_CHANGED,
  SIGNAL_HANDLE,
  SIGNAL_LAST
};

enum
{
  DETAIL_SOURCES,
  DETAIL_TILE_TYPE,
  DETAIL_PROFILE,
  DETAIL_TRACK,
  DETAIL_SPEED,
  DETAIL_VELOCITY,
  DETAIL_DEPTH_SOURCE,
  DETAIL_DEPTH_PARAMS,
  DETAIL_CACHE,
  DETAIL_LAST
};

struct _HyScanGtkWaterfallStatePrivate
{
  HyScanWaterfallDisplayType  disp_type;
  HyScanSourceType            lsource;
  HyScanSourceType            rsource;
  HyScanTileFlags             tile_flags;

  HyScanDB                   *db;
  gchar                      *project;
  gchar                      *track;
  gboolean                    raw;

  gfloat                      speed;

  GArray                     *velocity;

  HyScanSourceType            depth_source;
  guint                       depth_channel;
  gulong                      depth_usecs;
  guint                       depth_size;

  HyScanCache                *cache;
  HyScanCache                *cache2;
  gchar                      *prefix;

  gconstpointer               input_owner;
  gconstpointer               handle_processor;
  gboolean                    changes_allowed;
};

static void     hyscan_gtk_waterfall_state_object_constructed    (GObject                 *object);
static void     hyscan_gtk_waterfall_state_object_finalize       (GObject                 *object);
static gboolean hyscan_gtk_waterfall_state_handle_accumulator    (GSignalInvocationHint   *ihint,
                                                                  GValue                  *return_accu,
                                                                  const GValue            *handler_return,
                                                                  gpointer                 data);
static gboolean hyscan_gtk_waterfall_state_mouse_button_release  (GtkWidget               *widget,
                                                                  GdkEventButton          *event,
                                                                  HyScanGtkWaterfallState *self);


static guint       hyscan_gtk_waterfall_state_signals[SIGNAL_LAST] = {0};
static GQuark      hyscan_gtk_waterfall_state_details[DETAIL_LAST] = {0};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanGtkWaterfallState, hyscan_gtk_waterfall_state, GTK_TYPE_CIFRO_AREA);

static void
hyscan_gtk_waterfall_state_class_init (HyScanGtkWaterfallStateClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->constructed = hyscan_gtk_waterfall_state_object_constructed;
  obj_class->finalize = hyscan_gtk_waterfall_state_object_finalize;

  hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_GTK_WATERFALL_STATE,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  hyscan_gtk_waterfall_state_signals[SIGNAL_HANDLE] =
    g_signal_new ("handle", HYSCAN_TYPE_GTK_WATERFALL_STATE,
                  G_SIGNAL_RUN_LAST,
                  0, hyscan_gtk_waterfall_state_handle_accumulator, NULL,
                  g_cclosure_user_marshal_POINTER__POINTER,
                  G_TYPE_POINTER,
                  1, G_TYPE_POINTER);

  hyscan_gtk_waterfall_state_details[DETAIL_SOURCES]      = g_quark_from_static_string ("sources");
  hyscan_gtk_waterfall_state_details[DETAIL_TILE_TYPE]    = g_quark_from_static_string ("tile-type");
  hyscan_gtk_waterfall_state_details[DETAIL_PROFILE]      = g_quark_from_static_string ("profile");
  hyscan_gtk_waterfall_state_details[DETAIL_TRACK]        = g_quark_from_static_string ("track");
  hyscan_gtk_waterfall_state_details[DETAIL_SPEED]        = g_quark_from_static_string ("speed");
  hyscan_gtk_waterfall_state_details[DETAIL_VELOCITY]     = g_quark_from_static_string ("velocity");
  hyscan_gtk_waterfall_state_details[DETAIL_DEPTH_SOURCE] = g_quark_from_static_string ("depth-source");
  hyscan_gtk_waterfall_state_details[DETAIL_DEPTH_PARAMS] = g_quark_from_static_string ("depth-params");
  hyscan_gtk_waterfall_state_details[DETAIL_CACHE]        = g_quark_from_static_string ("cache");
}

static void
hyscan_gtk_waterfall_state_init (HyScanGtkWaterfallState *self)
{
  self->priv = hyscan_gtk_waterfall_state_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_state_object_constructed (GObject *object)
{
  HyScanGtkWaterfallState *self = HYSCAN_GTK_WATERFALL_STATE (object);
  HyScanGtkWaterfallStatePrivate *priv = self->priv;
  G_OBJECT_CLASS (hyscan_gtk_waterfall_state_parent_class)->constructed (object);

  g_signal_connect (self, "button-release-event",
                    G_CALLBACK (hyscan_gtk_waterfall_state_mouse_button_release), self);
  /* Задаем умолчания. */
  priv->disp_type     = HYSCAN_WATERFALL_DISPLAY_SIDESCAN;
  priv->lsource       = HYSCAN_SOURCE_SIDE_SCAN_PORT;
  priv->rsource       = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
  priv->tile_flags     = 0;
  priv->speed         = 1.0;
  priv->depth_source  = HYSCAN_SOURCE_SIDE_SCAN_PORT;
  priv->depth_channel = 1;
  priv->depth_usecs   = 1000000;
  priv->depth_size    = 4;
}


static gboolean
hyscan_gtk_waterfall_state_handle_accumulator (GSignalInvocationHint *ihint,
                                               GValue                *return_accu,
                                               const GValue          *handler_return,
                                               gpointer               data)
{
  gpointer instance;

  instance = g_value_get_pointer (handler_return);
  g_value_set_pointer (return_accu, instance);

  /* Для остановки эмиссии надо вернуть FALSE.
   * Эмиссия останавливается, если нашелся хэндл. */
  return (instance == NULL);
}

static gboolean
hyscan_gtk_waterfall_state_mouse_button_release (GtkWidget               *widget,
                                                 GdkEventButton          *event,
                                                 HyScanGtkWaterfallState *self)
{
  gconstpointer instance = NULL;

 /* Нам нужно выяснить, кто имеет право отреагировать на это воздействие.
   * Возможны следующие ситуации:
   * - handle_owner != NULL - значит, этот слой уже обрабатывает взаимодействия,
   *   нельзя ему мешать
   * - handle_owner == NULL - значит, никто не обрабатывает взаимодействие.
   */
  if (!hyscan_gtk_waterfall_state_get_changes_allowed (self))
    return FALSE;

  instance = hyscan_gtk_waterfall_state_get_handle_grabbed (self);

  if (instance != NULL)
    return FALSE;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_HANDLE], 0, event, &instance);
  hyscan_gtk_waterfall_state_set_handle_grabbed (self, instance);

  return FALSE;
}

static void
hyscan_gtk_waterfall_state_object_finalize (GObject *object)
{
  HyScanGtkWaterfallState *self = HYSCAN_GTK_WATERFALL_STATE (object);
  HyScanGtkWaterfallStatePrivate *priv = self->priv;

  g_clear_object (&priv->db);

  g_free (priv->project);
  g_free (priv->track);
  if (priv->velocity != NULL)
    g_array_unref (priv->velocity);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->cache2);
  g_free (priv->prefix);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_state_parent_class)->finalize (object);
}

/* Функция создает новый объект. */
HyScanGtkWaterfallState*
hyscan_gtk_waterfall_state_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_STATE, NULL);
}

/* Функция позволяет слою захватить ввод. */
void
hyscan_gtk_waterfall_state_set_input_owner (HyScanGtkWaterfallState *self,
                                            gconstpointer            instance)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));

  /* Ничего не делаем, если владелец не изменился. */
  if (self->priv->input_owner == instance)
    return;

  self->priv->input_owner = instance;
}

/* Функция возвращает владельца ввода. */
gconstpointer
hyscan_gtk_waterfall_state_get_input_owner (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);

  return self->priv->input_owner;
}

/* Функция позволяет слою захватить ввод. */
void
hyscan_gtk_waterfall_state_set_handle_grabbed (HyScanGtkWaterfallState *self,
                                               gconstpointer            instance)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));

  self->priv->handle_processor = instance;
}

/* Функция возвращает владельца ввода. */
gconstpointer
hyscan_gtk_waterfall_state_get_handle_grabbed (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);

  return self->priv->handle_processor;
}

/* Функция возвращает владельца ввода. */
void
hyscan_gtk_waterfall_state_set_changes_allowed (HyScanGtkWaterfallState *self,
                                                gboolean                 allowed)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));

  /* Ничего не делаем, если владелец не изменился. */
  self->priv->changes_allowed = allowed;
}

/* Функция возвращает владельца ввода. */
gboolean
hyscan_gtk_waterfall_state_get_changes_allowed (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), TRUE);

  return self->priv->changes_allowed;
}

/* Функция устанавливает режим отображения эхолот. */
void
hyscan_gtk_waterfall_state_echosounder (HyScanGtkWaterfallState *self,
                                        HyScanSourceType         source)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->disp_type = HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER;
  priv->lsource = priv->rsource = source;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SOURCES], NULL);
}

/* Функция устанавливает режим отображения водопад. */
void
hyscan_gtk_waterfall_state_sidescan (HyScanGtkWaterfallState *self,
                                     HyScanSourceType         lsource,
                                     HyScanSourceType         rsource)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->disp_type = HYSCAN_WATERFALL_DISPLAY_SIDESCAN;
  priv->lsource = lsource;
  priv->rsource = rsource;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SOURCES], NULL);
}

/* Функция устанавливает тип тайла (наклонная или горизонтальная дальность). */
void
hyscan_gtk_waterfall_state_set_tile_flags (HyScanGtkWaterfallState *self,
                                           HyScanTileFlags          flags)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->tile_flags = flags;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_TILE_TYPE], NULL);
}

/* Функция устанавливает систему кэширования. */
void
hyscan_gtk_waterfall_state_set_cache (HyScanGtkWaterfallState *self,
                                      HyScanCache             *cache,
                                      HyScanCache             *cache2,
                                      const gchar             *prefix)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (cache == NULL)
    return;

  if (cache2 == NULL)
    cache2 = cache;

  if (prefix == NULL)
    prefix = "none";

  g_clear_object (&priv->cache);
  g_clear_object (&priv->cache2);
  g_clear_pointer (&priv->prefix, g_free);

  priv->cache = g_object_ref (cache);
  priv->cache2 = g_object_ref (cache2);
  priv->prefix = g_strdup (prefix);
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_CACHE], NULL);
}

/* Функция задает БД, проект и галс */
void
hyscan_gtk_waterfall_state_set_track (HyScanGtkWaterfallState *self,
                                      HyScanDB                *db,
                                      const gchar             *project,
                                      const gchar             *track,
                                      gboolean                 raw)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (db == NULL || project == NULL || track == NULL)
    return;

  g_clear_object (&priv->db);
  g_clear_pointer (&priv->project, g_free);
  g_clear_pointer (&priv->track, g_free);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project);
  priv->track = g_strdup (track);
  priv->raw = raw;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_TRACK], NULL);
}

/* Функция устанавливает скорость судна. */
void
hyscan_gtk_waterfall_state_set_ship_speed (HyScanGtkWaterfallState *self,
                                           gfloat                   speed)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->speed = speed;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SPEED], NULL);
}

/* Функция устанавливает скорость звука. */
void
hyscan_gtk_waterfall_state_set_sound_velocity (HyScanGtkWaterfallState *self,
                                               GArray                  *velocity)
{
  GArray *copy = NULL;
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (velocity != NULL)
    {
      copy = g_array_new (FALSE, FALSE, sizeof (HyScanSoundVelocity));
      g_array_set_size (copy, velocity->len);
      memcpy (copy->data, velocity->data, sizeof (HyScanSoundVelocity) * velocity->len);
    }

  if (priv->velocity != NULL)
    g_array_unref (priv->velocity);
  priv->velocity = copy;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_VELOCITY], NULL);
}

/* Функция задает источник данных глубины. */
void
hyscan_gtk_waterfall_state_set_depth_source (HyScanGtkWaterfallState *self,
                                             HyScanSourceType         source,
                                             guint                    channel)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->depth_source = source;
  priv->depth_channel = channel;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_DEPTH_SOURCE], NULL);
}

/* Функция задает окно валидности данных глубины. */
void
hyscan_gtk_waterfall_state_set_depth_time (HyScanGtkWaterfallState *self,
                                           gulong                   usecs)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->depth_usecs = usecs;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_DEPTH_PARAMS], NULL);
}

/* Функция задает размер фильтра для данных глубины. */
void
hyscan_gtk_waterfall_state_set_depth_filter_size (HyScanGtkWaterfallState *self,
                                                  guint                    size)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->depth_size = size;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_DEPTH_PARAMS], NULL);
}

/* Функция возвращает тип отображения и источники. */
void
hyscan_gtk_waterfall_state_get_sources (HyScanGtkWaterfallState       *self,
                                        HyScanWaterfallDisplayType    *type,
                                        HyScanSourceType              *lsource,
                                        HyScanSourceType              *rsource)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (type != NULL)
    *type = priv->disp_type;
  if (lsource != NULL)
    *lsource = priv->lsource;
  if (rsource != NULL)
    *rsource = priv->rsource;
}

/* Функция возвращает тип тайла. */
void
hyscan_gtk_waterfall_state_get_tile_flags (HyScanGtkWaterfallState *self,
                                           HyScanTileFlags         *flags)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (flags != NULL)
    *flags = priv->tile_flags;
}

/* Функция возвращает систему кэширования. */
void
hyscan_gtk_waterfall_state_get_cache (HyScanGtkWaterfallState *self,
                                      HyScanCache            **cache,
                                      HyScanCache            **cache2,
                                      gchar                  **prefix)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (cache != NULL)
    *cache = (priv->cache != NULL) ? g_object_ref (priv->cache) : NULL;
  if (cache2 != NULL)
    *cache2 = (priv->cache2 != NULL) ? g_object_ref (priv->cache2) : NULL;
  if (prefix != NULL)
    *prefix = (priv->prefix != NULL) ? g_strdup (priv->prefix) : NULL;
}

/* Функция возвращает БД, проект и галс. */
void
hyscan_gtk_waterfall_state_get_track (HyScanGtkWaterfallState *self,
                                      HyScanDB               **db,
                                      gchar                  **project,
                                      gchar                  **track,
                                      gboolean                *raw)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (db != NULL)
    *db = (priv->db != NULL) ? g_object_ref (priv->db) : NULL;
  if (project != NULL)
    *project = (priv->project != NULL) ? g_strdup (priv->project) : NULL;
  if (track != NULL)
    *track = (priv->track != NULL) ? g_strdup (priv->track) : NULL;
  if (raw != NULL)
    *raw = priv->raw;
}

/* Функция возвращает скорость судна. */
void
hyscan_gtk_waterfall_state_get_ship_speed (HyScanGtkWaterfallState *self,
                                           gfloat                  *speed)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (speed != NULL)
    *speed = priv->speed;
}

/* Функция возвращает профиль скорости звука. */
void
hyscan_gtk_waterfall_state_get_sound_velocity (HyScanGtkWaterfallState *self,
                                               GArray                 **velocity)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (velocity != NULL)
    *velocity = (priv->velocity != NULL) ? g_array_ref (priv->velocity) : NULL;
}

/* Функция возвращает источник данных глубины */
void
hyscan_gtk_waterfall_state_get_depth_source (HyScanGtkWaterfallState *self,
                                             HyScanSourceType        *source,
                                             guint                   *channel)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (source != NULL)
    *source = priv->depth_source;
  if (channel != NULL)
    *channel = priv->depth_channel;
}

/* Функция возвращает окно валидности данных глубины. */
void
hyscan_gtk_waterfall_state_get_depth_time (HyScanGtkWaterfallState *self,
                                           gulong                  *usecs)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (usecs != NULL)
    *usecs = priv->depth_usecs;
}

/* Функция возвращает размер фильтра глубины. */
void
hyscan_gtk_waterfall_state_get_depth_filter_size (HyScanGtkWaterfallState *self,
                                                  guint                   *size)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (size != NULL)
    *size = priv->depth_size;
}
