/**
 * \file hyscan-gtk-waterfall-player.h
 *
 * \brief Исходный файл плеера для водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-player.h"

enum
{
  PROP_WATERFALL = 1
};

enum
{
  SIGNAL_STOP,
  SIGNAL_LAST
};

struct _HyScanGtkWaterfallPlayerPrivate
{
  HyScanGtkWaterfall          *wfall;        /**< Водопад. */
  HyScanGtkWaterfallState     *wf_state;     /**< Параметры водопада. */
  GtkCifroArea                *carea;        /**< Цифроариа. */
  HyScanWaterfallDisplayType   display_type; /**< Тип отображения. */

  guint                        player_tag;   /**< Идентификатор функции проигрывания. */
  gint64                       last_time;    /**< Предыдущее время вызова ф-ии прогрывания. */

  guint                        fps;          /**< Количество кадров в секунду. */
  gdouble                      speed;        /**< Скорость сдвижки. */

};

static void     hyscan_gtk_waterfall_player_interface_init           (HyScanGtkWaterfallLayerInterface *iface);
static void     hyscan_gtk_waterfall_player_set_property             (GObject                  *object,
                                                                      guint                     prop_id,
                                                                      const GValue             *value,
                                                                      GParamSpec               *pspec);
static void     hyscan_gtk_waterfall_player_object_constructed       (GObject                  *object);
static void     hyscan_gtk_waterfall_player_object_finalize          (GObject                  *object);
static void     hyscan_gtk_waterfall_player_sources_changed          (HyScanGtkWaterfallState  *state,
                                                                      HyScanGtkWaterfallPlayer *self);
static void     hyscan_gtk_waterfall_player_starter                  (HyScanGtkWaterfallPlayer *player);
static gboolean hyscan_gtk_waterfall_player_player                   (gpointer                  data);

static guint    hyscan_gtk_waterfall_player_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallPlayer, hyscan_gtk_waterfall_player, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkWaterfallPlayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER, hyscan_gtk_waterfall_player_interface_init));

static void
hyscan_gtk_waterfall_player_class_init (HyScanGtkWaterfallPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_player_set_property;

  object_class->constructed = hyscan_gtk_waterfall_player_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_player_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget",
                         HYSCAN_TYPE_GTK_WATERFALL_STATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_gtk_waterfall_player_signals[SIGNAL_STOP] =
    g_signal_new ("player-stop", HYSCAN_TYPE_GTK_WATERFALL_PLAYER,
                   G_SIGNAL_RUN_LAST,
                   0, NULL, NULL,
                   g_cclosure_marshal_VOID__VOID,
                   G_TYPE_NONE, 0);
}

static void
hyscan_gtk_waterfall_player_init (HyScanGtkWaterfallPlayer *self)
{
  self->priv = hyscan_gtk_waterfall_player_get_instance_private (self);
}


static void
hyscan_gtk_waterfall_player_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  HyScanGtkWaterfallPlayer *self = HYSCAN_GTK_WATERFALL_PLAYER (object);
  HyScanGtkWaterfallPlayerPrivate *priv = self->priv;

  if (prop_id == PROP_WATERFALL)
    {
      priv->wfall = g_value_dup_object (value);
      priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (priv->wfall);
      priv->carea = GTK_CIFRO_AREA (priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_player_object_constructed (GObject *object)
{
  HyScanGtkWaterfallPlayer *self = HYSCAN_GTK_WATERFALL_PLAYER (object);
  HyScanGtkWaterfallPlayerPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_player_parent_class)->constructed (object);

  /* Сигналы модели. */
  g_signal_connect (priv->wf_state, "changed::sources",
                    G_CALLBACK (hyscan_gtk_waterfall_player_sources_changed), self);

  hyscan_gtk_waterfall_player_set_speed (self, 0.0);
  hyscan_gtk_waterfall_player_set_fps (self, 25);
  hyscan_gtk_waterfall_player_sources_changed (priv->wf_state, self);

  /* Включаем видимость слоя. */
  hyscan_gtk_waterfall_layer_set_visible (HYSCAN_GTK_WATERFALL_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_player_object_finalize (GObject *object)
{
  HyScanGtkWaterfallPlayer *self = HYSCAN_GTK_WATERFALL_PLAYER (object);
  HyScanGtkWaterfallPlayerPrivate *priv = self->priv;

  if (priv->player_tag != 0)
    g_source_remove (priv->player_tag);

  g_clear_object (&priv->wfall);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_player_parent_class)->finalize (object);
}

/* Функция обработки сигнала смены типа отображения. */
static void
hyscan_gtk_waterfall_player_sources_changed (HyScanGtkWaterfallState   *state,
                                             HyScanGtkWaterfallPlayer *self)
{
  self->priv->display_type = hyscan_gtk_waterfall_state_get_sources (state, NULL, NULL);
}

/* Функция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_player_get_mnemonic (HyScanGtkWaterfallLayer *iface)
{
  return "applications-multimedia-symbolic";
}

/* Функция запускает проигрывание. */
static void
hyscan_gtk_waterfall_player_starter (HyScanGtkWaterfallPlayer *self)
{
  HyScanGtkWaterfallPlayerPrivate *priv = self->priv;
  guint interval;

  /* Сначала прекращаем проигрывание. */
  if (priv->player_tag != 0)
    {
      g_source_remove (priv->player_tag);
      priv->player_tag = 0;
    }

  if (priv->speed == 0.0)
    return;

  priv->last_time = 0;

  /* Иначе создаем функцию.*/
  interval = 1000 / priv->fps;
  priv->player_tag = g_timeout_add (interval, hyscan_gtk_waterfall_player_player, self);
}

/* Функция, сдвигающая изображение. */
static gboolean
hyscan_gtk_waterfall_player_player (gpointer data)
{
  gdouble from_x, to_x;
  gdouble from_y, to_y;
  gdouble min_x, max_x;
  gdouble min_y, max_y;
  gdouble shift;
  gint64 time;

  HyScanGtkWaterfallPlayer *self = data;
  HyScanGtkWaterfallPlayerPrivate *priv = self->priv;

  gtk_cifro_area_get_view (priv->carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_limits (priv->carea, &min_x, &max_x, &min_y, &max_y);

  /* Узнаем текущее время. */
  time = g_get_real_time ();

  /* Если только запустились, дождемся следующего вызова функции. */
  if (priv->last_time == 0)
    goto next;

  /* Узнаем величину сдвижки. */
  shift = priv->speed * (time - priv->last_time) / G_TIME_SPAN_SECOND;

  if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    {
      /* Водопад - движение вверх-вниз.
         Сначала проверяем, возможна ли сдвижка в принципе. Если нет - останавливаем плеер */
      if (from_y + shift < min_y || to_y + shift > max_y)
        {
          from_y += shift;
          to_y += shift;
          goto stop;
        }

      from_y += shift;
      to_y += shift;
    }
  else /* if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER) */
    {
      /* Эхолот - движение влево-вправо.
         Сначала проверяем, возможна ли сдвижка в принципе. Если нет - останавливаем плеер */
      if (from_x + shift < min_x || to_x + shift > max_x)
        {
          from_x += shift;
          to_x += shift;
          goto stop;
        }

      from_x += shift;
      to_x += shift;
    }

  gtk_cifro_area_set_view (priv->carea, from_x, to_x, from_y, to_y);

  /* Запоминаем время вызова и продолжаем работу. */
next:
  priv->last_time = time;
  return G_SOURCE_CONTINUE;

 /* Если пора остановиться: выполняем сдвижку (чтобы гарантированно прийти в
 *  начало или конец галса), эмиттируем сигнал, удаляем функцию из главного потока. */
stop:
  gtk_cifro_area_set_view (priv->carea, from_x, to_x, from_y, to_y);
  g_signal_emit (self, hyscan_gtk_waterfall_player_signals[SIGNAL_STOP], 0);

  priv->player_tag = 0;
  return G_SOURCE_REMOVE;
}


/* Функция создает новый объект. */
HyScanGtkWaterfallPlayer*
hyscan_gtk_waterfall_player_new (HyScanGtkWaterfall *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_PLAYER,
                       "waterfall", waterfall,
                       NULL);
}

/* Функция задает количество кадров в секунду. */
void
hyscan_gtk_waterfall_player_set_fps (HyScanGtkWaterfallPlayer *self,
                                     guint                     fps)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_PLAYER (self));

  if (fps == 0)
    fps = 1;

  self->priv->fps = fps;

  hyscan_gtk_waterfall_player_starter (self);
}

/* Функция задает скорость сдвижки. */
void
hyscan_gtk_waterfall_player_set_speed (HyScanGtkWaterfallPlayer *self,
                                       gdouble             speed)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_PLAYER (self));

  self->priv->speed = speed;

  hyscan_gtk_waterfall_player_starter (self);
}

static void
hyscan_gtk_waterfall_player_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = NULL;
  iface->set_visible = NULL;
  iface->get_mnemonic = hyscan_gtk_waterfall_player_get_mnemonic;
}
