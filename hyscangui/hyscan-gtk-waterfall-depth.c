#include "hyscan-gtk-waterfall-depth.h"
#include <hyscan-depth-nmea.h>
#include <hyscan-depthometer.h>
#include <hyscan-tile-color.h>
#include <math.h>

enum
{
  PROP_WATERFALL = 1
};

struct _HyScanGtkWaterfallDepthPrivate
{
  HyScanGtkWaterfallState *wf_state;
  HyScanGtkWaterfall      *wfall;

  HyScanDB              *db;
  gchar                 *project;
  gchar                 *track;
  gboolean               raw;

  HyScanCache           *cache;
  gchar                 *prefix;

  HyScanWaterfallDisplayType widget_type;

  HyScanTileType         tile_type;

  gfloat                 ship_speed;
  GArray                *sound_velocity;
  HyScanSourceType       depth_source;
  guint                  depth_channel;
  guint                  depth_size;
  gulong                 depth_time;


  HyScanDepthometer     *depthometer;
  HyScanDepth           *idepth;

  gint64                 ltime;
  gint                   ltime_set;

  GThread               *processing;
  gint                   cond_flag;
  GCond                  cond;

  gboolean               open;
  gint                   stop;

  guint32                color;
  gdouble                width;
  gboolean               show;

  gint                   widget_height;
  gint                   widget_width;

  GArray                *tasks;
  GMutex                 task_lock;
};

static void               hyscan_gtk_waterfall_depth_interface_init       (HyScanGtkWaterfallLayerInterface *iface);
static void               hyscan_gtk_waterfall_depth_set_property         (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           const GValue            *value,
                                                                           GParamSpec              *pspec);

static void               hyscan_gtk_waterfall_depth_object_constructed   (GObject                 *object);
static void               hyscan_gtk_waterfall_depth_object_finalize      (GObject                 *object);

static void               hyscan_gtk_waterfall_depth_visible_draw         (GtkWidget               *widget,
                                                                           cairo_t                 *cairo);
static gboolean           hyscan_gtk_waterfall_depth_configure            (GtkWidget               *widget,
                                                                           GdkEventConfigure       *event);
static HyScanDepthometer *hyscan_gtk_waterfall_depth_open_depth           (HyScanGtkWaterfallDepthPrivate *priv,
                                                                           HyScanDepth                    *depth);
static HyScanDepth       *hyscan_gtk_waterfall_depth_open_idepth          (HyScanGtkWaterfallDepthPrivate *priv);
static gpointer           hyscan_gtk_waterfall_depth_processing           (gpointer                 data);

static void               hyscan_gtk_waterfall_depth_open                 (HyScanGtkWaterfall            *waterfall,
                                                                           HyScanDB                      *db,
                                                                           const gchar                   *project_name,
                                                                           const gchar                   *track_name,
                                                                           gboolean                       raw);
static void               hyscan_gtk_waterfall_depth_close                (HyScanGtkWaterfall            *waterfall);
static void               hyscan_gtk_waterfall_depth_open_internal        (HyScanGtkWaterfall            *waterfall,
                                                                           HyScanDB                      *db,
                                                                           const gchar                   *project_name,
                                                                           const gchar                   *track_name,
                                                                           gboolean                       raw);
static void               hyscan_gtk_waterfall_depth_close_internal       (HyScanGtkWaterfall            *waterfall);

static void               hyscan_gtk_waterfall_depth_sources_changed        (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_tile_type_changed      (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_cache_changed          (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_track_changed          (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_ship_speed_changed     (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_sound_velocity_changed (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_depth_source_changed   (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);
static void               hyscan_gtk_waterfall_depth_depth_params_changed   (HyScanGtkWaterfallState *state,
                                                                             HyScanGtkWaterfallDepth *self);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallDepth, hyscan_gtk_waterfall_depth, HYSCAN_TYPE_GTK_WATERFALL_DEPTH,
                         G_ADD_PRIVATE (HyScanGtkWaterfallDepth)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER,
                                                hyscan_gtk_waterfall_depth_interface_init));

static void
hyscan_gtk_waterfall_depth_class_init (HyScanGtkWaterfallDepthClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_depth_set_property;
  object_class->constructed = hyscan_gtk_waterfall_depth_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_depth_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "Waterfall widget",
                         HYSCAN_TYPE_GTK_WATERFALL_STATE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_depth_init (HyScanGtkWaterfallDepth *self)
{
  self->priv = hyscan_gtk_waterfall_depth_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_depth_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkWaterfallDepth *self = HYSCAN_GTK_WATERFALL_DEPTH (object);

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
hyscan_gtk_waterfall_depth_object_constructed (GObject *object)
{
  HyScanGtkWaterfallDepth *self = HYSCAN_GTK_WATERFALL_DEPTH (object);
  HyScanGtkWaterfallDepthPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_depth_parent_class)->constructed (object);

  priv->show = TRUE;
  priv->color = hyscan_tile_color_converter_d2i (1.0, 0.0, 0.0, 1.0);
  priv->ltime = -1;

  priv->width = 10;

  priv->tasks = g_array_new (FALSE, TRUE, sizeof (gint64));
  g_mutex_init (&priv->task_lock);

  /* Waterfall.*/
  g_signal_connect (self, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_depth_visible_draw), NULL);
  g_signal_connect_after (self, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_depth_configure), NULL);

  g_signal_connect (priv->wf_state, "changed::sources",      G_CALLBACK (hyscan_gtk_waterfall_depth_sources_changed), self);
  g_signal_connect (priv->wf_state, "changed::tile-type",    G_CALLBACK (hyscan_gtk_waterfall_depth_tile_type_changed), self);
  g_signal_connect (priv->wf_state, "changed::speed",        G_CALLBACK (hyscan_gtk_waterfall_depth_ship_speed_changed), self);
  g_signal_connect (priv->wf_state, "changed::velocity",     G_CALLBACK (hyscan_gtk_waterfall_depth_sound_velocity_changed), self);
  g_signal_connect (priv->wf_state, "changed::depth-source", G_CALLBACK (hyscan_gtk_waterfall_depth_depth_source_changed), self);
  g_signal_connect (priv->wf_state, "changed::depth-params", G_CALLBACK (hyscan_gtk_waterfall_depth_depth_params_changed), self);
  g_signal_connect (priv->wf_state, "changed::cache",        G_CALLBACK (hyscan_gtk_waterfall_depth_cache_changed), self);

  hyscan_gtk_waterfall_depth_sources_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_tile_type_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_cache_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_track_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_ship_speed_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_sound_velocity_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_depth_source_changed (priv->wf_state, self);
  hyscan_gtk_waterfall_depth_depth_params_changed (priv->wf_state, self);
}

static void
hyscan_gtk_waterfall_depth_object_finalize (GObject *object)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallDepth *self = HYSCAN_GTK_WATERFALL_DEPTH (object);
  HyScanGtkWaterfallDepthPrivate *priv = self->priv;

  hyscan_gtk_waterfall_depth_close (waterfall);

  g_array_free (priv->tasks, TRUE);
  g_mutex_clear (&priv->task_lock);

  g_clear_object (&priv->cache);
  g_free (priv->prefix);

  g_clear_object (&priv->depthometer);
  g_clear_object (&priv->idepth);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_depth_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_depth_visible_draw (GtkWidget *widget,
                                         cairo_t   *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallDepth *self = HYSCAN_GTK_WATERFALL_DEPTH (widget);
  HyScanGtkWaterfallDepthPrivate *priv = self->priv;

  cairo_surface_t *surface = cairo_get_target (cairo);
  guchar *data = cairo_image_surface_get_data (surface);
  gint stride = cairo_image_surface_get_stride (surface);
  gint i, j;
  gint size;

  gint across_px, along_px;

  gdouble line_width, line_from, line_to, ss_m, echo_m;
  gint from_px, to_px;

  gdouble depth, depth_px;

  gint64 time, ltime;
  gfloat speed;

  // g_message ("0");
  if (!priv->open || priv->depthometer == NULL || !priv->show || priv->width == 0.0)
    return;
    // g_message ("1");
  if (g_atomic_int_get (&priv->ltime_set) == 0)
    return;
    // g_message ("2");

  ltime = priv->ltime;
  speed = priv->ship_speed;

  /* Вычисляем ширину линии в пикселях. */
  line_width = priv->width / 1000.0;
  gtk_cifro_area_value_to_point (carea, &line_from, NULL, 0, 0);
  gtk_cifro_area_value_to_point (carea, &line_to, NULL, line_width, 0);
  line_width = (line_to - line_from) / 2.0;

  if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    size = priv->widget_height;
  else
    size = priv->widget_width;

  for (along_px = 0; along_px < size; along_px++)
    {
      /* Переводим координату из экранной в метры. */
      gtk_cifro_area_point_to_value (carea, along_px, along_px, &echo_m, &ss_m);

      /* Теперь вычисляем время для этой координаты.*/
      if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
        time = (ss_m / speed) * 1000000 + ltime;
      else
        time = (echo_m / speed) * 1000000 + ltime;

      depth = hyscan_depthometer_check (priv->depthometer, time);
      // g_message ("%f depth %p", depth, priv->cache);

      /* Если вернулось -1, отправляем задание. */
      if (depth == -1.0)
        {
          g_atomic_int_set (&priv->cond_flag, 1);
          g_mutex_lock (&priv->task_lock);
          g_array_append_val (priv->tasks, time);
          g_mutex_unlock (&priv->task_lock);
          continue;
        }

      if (priv->widget_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
        {
          /* Рисуем точку на правом и левом бортах. */
          for (j = 0; j < 2; j++)
            {
              gtk_cifro_area_value_to_point (carea, &depth_px, NULL, depth, 0);

              from_px = MAX (round (depth_px - line_width), 0);
              to_px = MIN (round (depth_px + line_width), priv->widget_width - 1);

              for (i = from_px; i <= to_px; i++)
                {
                  across_px = along_px * stride + i * sizeof (guint32);
                  *((guint32*)(data + across_px)) = priv->color;
                }

              depth *= -1.0;
            }
        }
      else
        {
          /* Рисуем точку на эхолоте. */
          gtk_cifro_area_value_to_point (carea, NULL, &depth_px, 0, -depth);

          from_px = MAX (round (depth_px - line_width), 0);
          to_px = MIN (round (depth_px + line_width), priv->widget_height - 1);

          for (i = from_px; i <= to_px; i++)
            {
              across_px = i * stride + along_px * sizeof (guint32);
              *((guint32*)(data + across_px)) = priv->color;
            }
        }
    }

  g_cond_signal (&priv->cond);
}

static gboolean
hyscan_gtk_waterfall_depth_configure (GtkWidget         *widget,
                                      GdkEventConfigure *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallDepth *self = HYSCAN_GTK_WATERFALL_DEPTH (widget);
  guint width, height;

  /* Выясняем высоту виджета в пикселях. */
  gtk_cifro_area_get_size (carea, &width, &height);
  self->priv->widget_width = width;
  self->priv->widget_height = height;

  return FALSE;
}

/* Функция создает объект определения глубины. */
static HyScanDepth*
hyscan_gtk_waterfall_depth_open_idepth (HyScanGtkWaterfallDepthPrivate *priv)
{
  HyScanDepth *idepth = NULL;

  if (priv->depth_source == HYSCAN_SOURCE_NMEA_DPT)
    {
      HyScanDepthNMEA *dnmea;
      dnmea = hyscan_depth_nmea_new (priv->db, priv->project, priv->track, priv->depth_channel);
      idepth = HYSCAN_DEPTH (dnmea);
    }

  /* Устанавливаем кэш, создаем объект измерения глубины. */
  if (idepth == NULL)
    return NULL;

  hyscan_depth_set_cache (idepth, priv->cache, priv->prefix);

  return idepth;
}

/* Функция создает объект определения глубины. */
static HyScanDepthometer*
hyscan_gtk_waterfall_depth_open_depth (HyScanGtkWaterfallDepthPrivate *priv,
                                       HyScanDepth                    *idepth)
{
  HyScanDepthometer *meter = NULL;

  if (idepth == NULL)
    return NULL;

  /* Создаем объект измерения глубины, устанавливаем кэш. */
  meter = hyscan_depthometer_new (idepth);

  hyscan_depthometer_set_cache (meter, priv->cache, priv->prefix);
  hyscan_depthometer_set_filter_size (meter, priv->depth_size);
  hyscan_depthometer_set_validity_time (meter, priv->depth_time);

  return meter;
}

static gpointer
hyscan_gtk_waterfall_depth_processing (gpointer data)
{
  HyScanGtkWaterfallDepthPrivate *priv = data;
  HyScanDepthometer *depthometer = NULL;
  HyScanDepth *idepth = NULL;

  GMutex mutex;
  gint64 end_time, time;

  g_mutex_init (&mutex);

  while (g_atomic_int_get (&priv->stop) == 0)
    {
      /* Создаем объект измерения глубины. */
      if (idepth == NULL)
        idepth = hyscan_gtk_waterfall_depth_open_idepth (priv);
      if (depthometer == NULL)
        depthometer = hyscan_gtk_waterfall_depth_open_depth (priv, idepth);

      if (priv->idepth == NULL)
        priv->idepth = hyscan_gtk_waterfall_depth_open_idepth (priv);
      if (priv->depthometer == NULL)
        priv->depthometer = hyscan_gtk_waterfall_depth_open_depth (priv, priv->idepth);

      if (g_atomic_int_get (&priv->ltime_set) == 0)
        {
          guint32 lindex;
          if (hyscan_depth_get_range (idepth, &lindex, NULL))
            {
              hyscan_depth_get (idepth, lindex, &priv->ltime);
              g_atomic_int_set (&priv->ltime_set, 1);
            }
        }

      end_time = g_get_monotonic_time () + 250 * G_TIME_SPAN_MILLISECOND;
      g_mutex_lock (&mutex);
      if (g_atomic_int_get (&priv->cond_flag) == 0)
        if (!g_cond_wait_until (&priv->cond, &mutex, end_time))
          goto exit;

      /* Запрашиваем глубину в нужный момент времени. */
      if (depthometer == NULL)
        goto exit;

      while (priv->tasks->len > 0)
         {
           time = g_array_index (priv->tasks, gint64, 0);
           hyscan_depthometer_get (depthometer, time);
           g_mutex_lock (&priv->task_lock);
           g_array_remove_index_fast (priv->tasks, 0);
           g_mutex_unlock (&priv->task_lock);
           hyscan_gtk_waterfall_queue_draw (priv->wfall);
         }


     exit:
      g_mutex_unlock (&mutex);
    }

  g_clear_object (&idepth);
  g_clear_object (&depthometer);
  g_clear_object (&priv->idepth);
  g_clear_object (&priv->depthometer);
  return NULL;
}

static void
hyscan_gtk_waterfall_depth_open (HyScanGtkWaterfall *waterfall,
                                 HyScanDB           *db,
                                 const gchar        *project,
                                 const gchar        *track,
                                 gboolean            raw)
{
  hyscan_gtk_waterfall_depth_open_internal (waterfall, db, project, track, raw);
}

static void
hyscan_gtk_waterfall_depth_close (HyScanGtkWaterfall *waterfall)
{
  hyscan_gtk_waterfall_depth_close_internal (waterfall);
}

static void hyscan_gtk_waterfall_depth_sources_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_tile_type_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_cache_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_track_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_ship_speed_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_sound_velocity_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_depth_source_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}
static void hyscan_gtk_waterfall_depth_depth_params_changed (HyScanGtkWaterfallState *state, HyScanGtkWaterfallDepth *self) {}

GtkWidget*
hyscan_gtk_waterfall_depth_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_DEPTH, NULL);
}

void
hyscan_gtk_waterfall_depth_show (HyScanGtkWaterfallDepth *self,
                                 gboolean                 show)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DEPTH (self));

  self->priv->show = show;
}

void
hyscan_gtk_waterfall_depth_set_color (HyScanGtkWaterfallDepth *self,
                                      guint32                  color)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DEPTH (self));

  self->priv->color = color;
}

void
hyscan_gtk_waterfall_depth_set_width (HyScanGtkWaterfallDepth *self,
                                      gdouble                  width)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_DEPTH (self));

  if (width < 0.0)
    return;

  self->priv->width = width / 2;
}

static void
hyscan_gtk_waterfall_depth_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  g_warning ("implement");
}
