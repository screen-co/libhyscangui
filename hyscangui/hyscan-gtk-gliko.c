/* hyscan-gtk-gliko.с
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
 *
 * This file is part of HyScanGui.
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
 * SECTION: hyscan-gtk-gliko
 * @Title HyScanGtkGliko
 * @Short_description: виджет индикатора кругового обзора
 */

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <hyscan-acoustic-data.h>
#include <hyscan-data-player.h>
#include <hyscan-nmea-data.h>

#include "hyscan-gtk-gliko.h"

#include "hyscan-gtk-gliko-area.h"
#include "hyscan-gtk-gliko-grid.h"

#include "hyscan-gtk-gliko-que.h"

enum
{
  SIGNAL_PARAMETERS_CHANGED,
  SIGNAL_LAST,
};

// очередь угловых меток
typedef struct _alpha_que_t
{
  gint64 time;   // время приема угловой метки
  gdouble value; // значение угла в градусах от 0 до 360
} alpha_que_t;

// очередь строк данных акустического изображения
typedef struct _data_que_t
{
  gint64 time;    // время приема данных
  guint32 index;  // индекс строки в базе данных
  guint32 length; // длина строки - количество отсчетов
} data_que_t;

// канал данных акустического изображения
typedef struct _channel_t
{
  HyScanAcousticData *acoustic_data;
  HyScanSourceType source;
  data_que_t *data_que_buffer;
  que_t data_que;
  int64_t alpha_que_count;
  int64_t data_que_count;
  guint32 process_index;
  gint64 process_time;
  gint64 alpha_time;
  gdouble alpha_value;
  gdouble data_rate;
  gfloat *buffer;
  guint32 iko_length;
  guint32 allocated;
  int process_init;
  int gliko_channel;
  int ready_alpha_init;
  int ready_data_init;
  int azimuth_displayed;
  int iko_length_initialized;
  guint32 azimuth;
  gchar *source_name;
  gfloat freq;
} channel_t;

struct _HyScanGtkGlikoPrivate
{
  HyScanGtkGlikoArea *iko;
  HyScanGtkGlikoGrid *grid;

  HyScanNMEAData *nmea_data;
  HyScanDataPlayer *player;

  guint num_azimuthes;
  float rotation;
  float full_rotation;
  float bottom;
  float sound_speed;
  float scale;
  float cx;
  float cy;
  float contrast;
  float brightness;
  float balance;
  float fade_coef;
  float freq;
  gdouble data_rate;

  float white;
  float black;
  float gamma;

  channel_t channel[2];
  guint32 iko_length;
  int iko_length_initialized;

  int player_open;

  guint32 nmea_index;
  int nmea_init;

  alpha_que_t *alpha_que_buffer;
  que_t alpha_que;

  int width;
  int height;

  gint nmea_angular_source;

  gint64 fade_time;

  int track_changed;
  gchar *project_name;
  gchar *track_name;

  int playback;
};

/* Define type */
G_DEFINE_TYPE (HyScanGtkGliko, hyscan_gtk_gliko, HYSCAN_TYPE_GTK_GLIKO_OVERLAY)

/* Internal API */
static void init_queues (HyScanGtkGlikoPrivate *p);
static void dispose (GObject *gobject);
static void finalize (GObject *gobject);
static void on_resize (GtkGLArea *area, gint width, gint height, gpointer user_data);

//static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width );
//static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height );

static guint gliko_signal[SIGNAL_LAST] = { 0 };

/* Initialization */
static void
hyscan_gtk_gliko_class_init (HyScanGtkGlikoClass *klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  //GtkWidgetClass *w_class = GTK_WIDGET_CLASS( klass );

  /* Add private data */
  g_type_class_add_private (klass, sizeof (HyScanGtkGlikoPrivate));

  g_class->dispose = dispose;
  g_class->finalize = finalize;

  //w_class->get_preferred_width  = get_preferred_width;
  //w_class->get_preferred_height = get_preferred_height;

  gliko_signal[SIGNAL_PARAMETERS_CHANGED] =
      g_signal_new ("parameters-changed", HYSCAN_TYPE_GTK_GLIKO, G_SIGNAL_RUN_LAST, 0,
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
}

static void
hyscan_gtk_gliko_init (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->iko = NULL;
  p->grid = NULL;

  p->nmea_data = NULL;
  p->player = NULL;

  p->num_azimuthes = 1024;
  p->rotation = 0.0f;
  p->full_rotation = 0.0f;
  p->bottom = 0.0f;
  p->sound_speed = 1500.0f;
  p->scale = 1.0f;
  p->cx = 0.0f;
  p->cy = 0.0f;
  p->contrast = 0.0f;
  p->brightness = 0.0f;
  p->fade_coef = 0.99999f;

  p->white = 1.0f;
  p->black = 0.0f;
  p->gamma = 1.0f;

  p->iko_length = 1024;
  p->iko_length_initialized = 0;

  p->player_open = 0;

  p->nmea_index = 0;
  p->nmea_init = 0;

  p->nmea_angular_source = 1;

  p->alpha_que_buffer = NULL;

  p->channel[0].allocated = 0;
  p->channel[0].buffer = NULL;
  p->channel[0].data_que_buffer = NULL;

  p->channel[1].allocated = 0;
  p->channel[1].buffer = NULL;
  p->channel[1].data_que_buffer = NULL;

  p->channel[0].source_name = NULL;
  p->channel[1].source_name = NULL;

  p->channel[0].gliko_channel = 0;
  p->channel[1].gliko_channel = 1;

  p->channel[0].acoustic_data = NULL;
  p->channel[1].acoustic_data = NULL;

  p->data_rate = 250000.0;

  p->track_changed = 0;
  p->project_name = NULL;
  p->track_name = NULL;

  p->playback = 1;

  init_queues (p);
#if 0
  /* Определяем тип источника данных по его названию. */
  p->channel[0].source = hyscan_source_get_type_by_id (p->channel[0].source_name);
  if (p->channel[0].source == HYSCAN_SOURCE_INVALID)
    {
      g_error ("unknown source type %s", p->channel[0].source_name);
      return;
    }

  p->channel[1].source = hyscan_source_get_type_by_id (p->channel[1].source_name);
  if (p->channel[1].source == HYSCAN_SOURCE_INVALID)
    {
      g_error ("unknown source type %s", p->channel[1].source_name);
      return;
    }
#endif

  p->iko = hyscan_gtk_gliko_area_new ();
  p->grid = hyscan_gtk_gliko_grid_new ();
  hyscan_gtk_gliko_overlay_set_layer (HYSCAN_GTK_GLIKO_OVERLAY (instance), 0, HYSCAN_GTK_GLIKO_LAYER (p->iko));
  hyscan_gtk_gliko_overlay_enable_layer (HYSCAN_GTK_GLIKO_OVERLAY (instance), 0, 1);
  hyscan_gtk_gliko_overlay_set_layer (HYSCAN_GTK_GLIKO_OVERLAY (instance), 1, HYSCAN_GTK_GLIKO_LAYER (p->grid));
  hyscan_gtk_gliko_overlay_enable_layer (HYSCAN_GTK_GLIKO_OVERLAY (instance), 1, 1);

  g_object_set (p->iko, "gliko-color1-rgb", "#00FF80", NULL);
  g_object_set (p->iko, "gliko-color1-alpha", 1.0f, NULL);
  g_object_set (p->iko, "gliko-color2-rgb", "#FF8000", NULL);
  g_object_set (p->iko, "gliko-color2-alpha", 1.0f, NULL);
  g_object_set (p->iko, "gliko-background-rgb", "#404040", NULL);
  g_object_set (p->iko, "gliko-background-alpha", 0.25f, NULL);

  g_object_set (p->iko, "gliko-fade-coef", p->fade_coef, NULL);
  g_object_set (p->iko, "gliko-scale", p->scale, NULL);
  g_object_set (p->grid, "gliko-scale", p->scale, NULL);

  g_signal_connect (instance, "resize", G_CALLBACK (on_resize), NULL);
}

static void
init_queues (HyScanGtkGlikoPrivate *p)
{
  if (p->alpha_que_buffer == NULL)
    {
      // резервируем буфер для очереди угловых меток
      p->alpha_que_buffer = g_malloc (p->num_azimuthes * sizeof (alpha_que_t));
    }
  // инициализируем очередь угловых меток
  initque (&p->alpha_que, p->alpha_que_buffer, sizeof (alpha_que_t), p->num_azimuthes);

  if (p->channel[0].data_que_buffer == NULL)
    {
      p->channel[0].data_que_buffer = g_malloc (p->num_azimuthes * sizeof (data_que_t));
    }
  if (p->channel[1].data_que_buffer == NULL)
    {
      p->channel[1].data_que_buffer = g_malloc (p->num_azimuthes * sizeof (data_que_t));
    }
  // инициализируем очереди строк данных
  initque (&p->channel[0].data_que, p->channel[0].data_que_buffer, sizeof (data_que_t), p->num_azimuthes);
  initque (&p->channel[1].data_que, p->channel[1].data_que_buffer, sizeof (data_que_t), p->num_azimuthes);
}

static void
dispose (GObject *gobject)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (gobject, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  g_clear_object (&p->player);

  G_OBJECT_CLASS (hyscan_gtk_gliko_parent_class)
      ->dispose (gobject);
}

static void
finalize (GObject *gobject)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (gobject, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  g_clear_object (&p->iko);
  g_clear_object (&p->grid);
  g_clear_object (&p->nmea_data);
  g_clear_object (&p->channel[0].acoustic_data);
  g_clear_object (&p->channel[1].acoustic_data);

  if (p->project_name != NULL)
    {
      g_free (p->project_name);
    }
  if (p->track_name != NULL)
    {
      g_free (p->track_name);
    }
  G_OBJECT_CLASS (hyscan_gtk_gliko_parent_class)
      ->finalize (gobject);
}

GtkWidget *
hyscan_gtk_gliko_new (void)
{
  return GTK_WIDGET (g_object_new (hyscan_gtk_gliko_get_type (), NULL));
}

static void
on_resize (GtkGLArea *area, gint width, gint height, gpointer user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (area, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->width = width;
  p->height = height;
}

/**
 * hyscan_gtk_gliko_set_num_azimuthes:
 * @instance: указатель на объект индикатора кругового обзора
 * @num_azimuthes: количество угловых дискрет
 *
 * Задает разрешающую способность по углу для индикатора кругового обзора,
 * определяется количеством угловых дискрет на полный оборот 360 градусов.
 * Количество угловых дискрет должно являться степенью 2.
 * По умолчанию принимается количество угловых дискрет, равное 1024.
 * Разрешающая способность по углу должна быть задана только один раз,
 * перед подключением к проигрывателю
 * гидроакустических данных.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_num_azimuthes (HyScanGtkGliko *instance,
                                    const guint num_azimuthes)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  if (num_azimuthes != p->num_azimuthes)
    {
      g_free (p->alpha_que_buffer);
      p->alpha_que_buffer = NULL;

      g_free (p->channel[0].data_que_buffer);
      p->channel[0].data_que_buffer = NULL;

      g_free (p->channel[1].data_que_buffer);
      p->channel[1].data_que_buffer = NULL;

      p->num_azimuthes = num_azimuthes;
      init_queues (p);
    }
}

/**
 * hyscan_gtk_gliko_get_num_azimuthes:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Returns: заданное количество угловых дискрет индикатора кругового обзора.
 */
HYSCAN_API
guint
hyscan_gtk_gliko_get_num_azimuthes (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->num_azimuthes;
}

static void
channel_reset_playback (HyScanGtkGlikoPrivate *p,
                        const int channel_index)
{
  channel_t *c = p->channel + channel_index;

  // инициализируем индексы очередей
  c->alpha_que_count = p->alpha_que.count;
  c->data_que_count = c->data_que.count;

  // обнуляем признак инициализации обработки
  c->process_init = 0;

  // последний выданный на индикатор азимутальный дискрет
  c->azimuth_displayed = 0;
  c->azimuth = 0;

  c->ready_alpha_init = 0;
  c->ready_data_init = 0;
}

HYSCAN_API
guint
hyscan_gtk_gliko_set_playback (HyScanGtkGliko *instance, const int enable_playback)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  if (p->playback == 0 && enable_playback != 0)
    {
      if (enable_playback > 1)
        {
          channel_reset_playback (p, 0);
          channel_reset_playback (p, 1);
          hyscan_gtk_gliko_area_clear (HYSCAN_GTK_GLIKO_AREA (p->iko));
        }
      p->playback = 1;
    }
  else if (p->playback != 0 && enable_playback == 0)
    {
      p->playback = 0;
    }
  return TRUE;
}

// открытие канала данных
static void
channel_open (HyScanGtkGlikoPrivate *p,
              const int channel_index)
{
  channel_t *c = p->channel + channel_index;

  c->source = hyscan_source_get_type_by_id (c->source_name);
  if (c->source == HYSCAN_SOURCE_INVALID)
    {
      g_error ("unknown source type %s", c->source_name);
    }

  // размер считываемой строки пока не известен
  c->iko_length_initialized = 0;

  // размер зарезервированного буфера
  c->allocated = 0;

  // указатель на буфер
  c->buffer = NULL;

  /* Объект обработки акустических данных. */
  g_clear_object (&c->acoustic_data);
  c->acoustic_data = hyscan_acoustic_data_new (hyscan_data_player_get_db (p->player), NULL, p->project_name, p->track_name, c->source, 1, FALSE);
  if (c->acoustic_data == NULL)
    {
      g_warning ("can't open acoustic data source");
    }

  /* частота дискретизации */
  c->data_rate = hyscan_acoustic_data_get_info (c->acoustic_data).data_rate;

  // воспроизведение с чистого листа
  channel_reset_playback (p, channel_index);
}

// обработчик сигнала track-changed
static void
player_track_changed_callback (HyScanDataPlayer *player,
                               HyScanDB *db,
                               const gchar *project_name,
                               const gchar *track_name,
                               gpointer user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  if (p->project_name != NULL)
    {
      g_free (p->project_name);
    }
  if (p->track_name != NULL)
    {
      g_free (p->track_name);
    }
  p->project_name = g_strdup (project_name);
  p->track_name = g_strdup (track_name);
  p->track_changed = 1;

  //printf ("open %s %s\n", project_name, track_name);
  //fflush (stdout);
}

// обработка индексов принятых строк
static void
channel_process (HyScanGtkGlikoPrivate *p,
                 const int channel_index,
                 gint64 time)
{
  channel_t *c = p->channel + channel_index;
  guint32 ileft = 0, iright;
  gint32 idelta;
  gint64 tleft, tright;
  data_que_t data;

  if (c->acoustic_data == NULL)
    {
      return;
    }
  // проверяем наличие акустических данных
  if (hyscan_acoustic_data_find_data (c->acoustic_data, time, &ileft, &iright, &tleft, &tright) != HYSCAN_DB_FIND_OK)
    {
      return;
    }
  if (!c->process_init)
    {
      c->process_time = time;
      c->process_index = ileft;
      c->process_init = 1;
      return;
    }
  // приращение индекса для акустических данных
  idelta = ((ileft >= c->process_index) ? 1 : -1);

  // сохраняем в очереди данных индексы строк за время с момента последнего вызова
  for (; c->process_index != ileft; c->process_index += idelta)
    {
      if (hyscan_acoustic_data_get_size_time (c->acoustic_data, c->process_index, &data.length, &data.time))
        {
          data.index = c->process_index;
          enque (&c->data_que, &data, 1);
          if (c->iko_length_initialized == 0)
            {
              c->iko_length = data.length;
              c->iko_length_initialized = 1;
            }
        }
    }
}

static gdouble
range360 (const gdouble a)
{
  guint i;
  gdouble d;

  d = a * 65536.0 / 360.0;
  i = (int) d;
  d = (i & 0xFFFF);
  return d * 360.0 / 65536.0;
}

// обработчик сигнала process
void
player_process_callback (HyScanDataPlayer *player,
                         gint64 time,
                         gpointer user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  guint32 inleft, inright;
  gint64 tnleft, tnright;
  int indelta;
  alpha_que_t alpha;

  //printf ("process %"PRId64"\n", time );
  //fflush (stdout);

  if (p == NULL)
    return;

  if (!p->playback)
    {
      return;
    }

  if (p->track_changed)
    {
      p->track_changed = 0;

      init_queues (p);

      g_clear_object (&p->nmea_data);
      p->nmea_data = hyscan_nmea_data_new (hyscan_data_player_get_db (p->player), NULL, p->project_name, p->track_name, p->nmea_angular_source);
      if (p->nmea_data == NULL)
        {
          g_warning ("can't open nmea data\n");
          return;
        }
      channel_open (p, 0);
      channel_open (p, 1);
      p->nmea_init = 0;
      p->iko_length_initialized = 0;
    }

  // обработка индексов строк данных для двух каналов
  channel_process (p, 0, time);
  channel_process (p, 1, time);

  // запрашиваем индексы по текущему времени
  if (hyscan_nmea_data_find_data (p->nmea_data, time, &inleft, &inright, &tnleft, &tnright) != HYSCAN_DB_FIND_OK)
    {
      return;
    }
  // инициализации еще не было?
  if (!p->nmea_init)
    {
      // запоминаем индексы
      p->nmea_index = inleft;
      p->nmea_init = 1;
    }

  // приращение индекса для данных nmea
  indelta = ((inleft >= p->nmea_index) ? 1 : -1);

  // обрабатываем данные от датчика угла
  for (; p->nmea_index != inleft; p->nmea_index += indelta)
    {
      const gchar *nmea;
      const char header[6] = { '$', 'H', 'Y', 'R', 'A', ',' };

      /* Считываем строку nmea */
      nmea = hyscan_nmea_data_get (p->nmea_data, p->nmea_index, &alpha.time);

      if (nmea == NULL)
        {
          continue;
        }

      /* обрабатываем только датчик угла поворота */
      if (memcmp (nmea, header, sizeof (header)) != 0)
        {
          continue;
        }

      /* текущий угол поворота, градусы */
      alpha.value = range360 (g_ascii_strtod (nmea + sizeof (header), NULL));

      //printf( "process %s %"PRIu64" %.2lf\n", hyscan_data_player_get_track_name( player ), alpha.time, alpha.value );
      //fflush( stdout );

      // буферизуем считанное значение в очереди
      enque (&p->alpha_que, &alpha, 1);
    }
}

// обработка готовых к индикации строк
static void
channel_ready (HyScanGtkGlikoPrivate *p,
               const int channel_index,
               gint64 time)
{
  channel_t *c = p->channel + channel_index;
  int ra, rd;
  alpha_que_t alpha;
  data_que_t data;
  gdouble a, d, dm, dn, dp;
  const gfloat *amplitudes;
  guint32 i, j, length;
  gint64 t;

  // резервируем буфер для строки отсчетов
  if (c->allocated == 0)
    {
      if (c->buffer != NULL)
        {
          g_free (c->buffer);
          c->buffer = NULL;
        }
      for (c->allocated = (1 << 10); c->allocated < p->iko_length; c->allocated <<= 1)
        ;
      c->buffer = g_malloc0 (c->allocated * sizeof (gfloat));
    }

  // просматриваем очередь данных датчика угла
  do
    {
      // считываем один элемент из очереди
      ra = deque (&p->alpha_que, &alpha, 1, c->alpha_que_count);

      // если произошло переполнение буфера очереди
      if (ra < 0)
        {
          // начинаем чтение с текущей позиции
          c->alpha_que_count = p->alpha_que.count;
          return;
        }
      // если нового показания нет, выходим
      if (ra == 0)
        {
          return;
        }

      // если параметры датчика угла не были проинициализированы
      if (!c->ready_alpha_init)
        {
          // запоминаем время получения замера угла
          c->alpha_time = alpha.time;
          // запоминаем значение угла
          c->alpha_value = alpha.value;
          // следующий в очереди
          c->alpha_que_count++;
          // инициализация выполнена
          c->ready_alpha_init = 1;
          continue;
        }
      // если вдруг время замера больше текущего времени, прекращаем работу
      /*
      if (alpha.time >= time)
        {
          break;
        }
        */
      // отсуствие разницы во времени тоже не допустимо
      if (alpha.time <= c->alpha_time)
        {
          c->ready_alpha_init = 0;
          continue;
        }
      /* изменение угла, с учетом перехода через 0 */
      dn = alpha.value - c->alpha_value;
      dm = dn - 360.0;
      dp = dn + 360.0;
      d = dn;
      if (fabs (dm) < fabs (dn))
        {
          d = dm;
        }
      else if (fabs (dp) < fabs (dn))
        {
          d = dp;
        }

      // у нас есть два замера угла,
      // обрабатываем очередь данных,
      // предполагая, что угол изменялся линейно
      do
        {
          // считываем один элемент из очереди
          rd = deque (&c->data_que, &data, 1, c->data_que_count);

          // если произошло переполнение буфера очереди
          if (rd < 0)
            {
              // начинаем обработку с текущего места
              c->data_que_count = c->data_que.count;
              // требуется инициализация данных
              c->ready_data_init = 0;
              // прекращаем работу до следующей итерации
              return;
            }
          // если новых данных нет, выходим
          if (rd == 0)
            {
              return;
            }

          // если вышли за интервал замера угла
          if (data.time >= alpha.time)
            {
              // переходим к следующему углу
              break;
            }
          // значение угла
          a = c->alpha_value + d * (data.time - c->alpha_time) / (alpha.time - c->alpha_time);

          // следующая строка изображения
          c->data_que_count++;

          /* допустимый диапазон 0..360 */
          a = range360 (a);

          //printf( "ready %d %"PRIu64" %.2lf\n", channel_index, data.time, a );
          //fflush( stdout );

          /* номер углового дискрета */
          j = (guint32) (a * p->num_azimuthes / 360.0);
          j %= p->num_azimuthes;

          // если уже есть отрисованный азимут
          if (c->azimuth_displayed)
            {
              // если луч перепрыгнул через 1 азимутальный дискрет
              if (j == ((c->azimuth + 2) % p->num_azimuthes))
                {
                  // дублируем предыдущий азимут
                  hyscan_gtk_gliko_area_set_data (HYSCAN_GTK_GLIKO_AREA (p->iko), c->gliko_channel, (c->azimuth + 1) % p->num_azimuthes, c->buffer);
                }
              else if (j == ((c->azimuth - 2) % p->num_azimuthes))
                {
                  hyscan_gtk_gliko_area_set_data (HYSCAN_GTK_GLIKO_AREA (p->iko), c->gliko_channel, (c->azimuth - 1) % p->num_azimuthes, c->buffer);
                }
              // можно дорисовать только один пропущенный азимутальный дискрет
              c->azimuth_displayed = 0;
            }

          // считываем строку акустического изображения
          amplitudes = hyscan_acoustic_data_get_amplitude (c->acoustic_data, NULL, data.index, &length, &t);

          if (amplitudes == NULL)
            {
              g_warning ("can't read acoustic line %d", data.index);
              break;
            }

          if (length > p->iko_length)
            {
              length = p->iko_length;
            }

          /* запоминаем строку амплитуд в буфере */
          memcpy (c->buffer, amplitudes, length * sizeof (gfloat));

          /* если есть остаток, обнуляем его */
          if (length < p->iko_length)
            {
              for (i = length; i < p->iko_length; i++)
                {
                  c->buffer[i] = 0.0f;
                }
            }

          /* передаем строку в индикатор кругового обзора */
          hyscan_gtk_gliko_area_set_data (HYSCAN_GTK_GLIKO_AREA (p->iko), c->gliko_channel, j, c->buffer);
          c->azimuth = j;
          c->azimuth_displayed = 1;
        }
      while (rd > 0);

      // конец текущего временного интервала становится началом следущего
      c->alpha_value = alpha.value;
      c->alpha_time = alpha.time;

      // следующая угловая метка
      c->alpha_que_count++;
    }
  while (ra > 0);
}

// обработчик сигнала ready
void
player_ready_callback (HyScanDataPlayer *player,
                       gint64 time,
                       gpointer user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  if (p == NULL)
    return;

  if (!hyscan_data_player_is_played (player))
    {
      return;
    }

  //g_print ("Ready time: %"G_GINT64_FORMAT" %d\n", g_get_monotonic_time (), misc_read);

  // если индикатор кругового обзора не проинициализирован
  if (p->iko_length_initialized == 0 &&
      p->channel[0].iko_length_initialized != 0 &&
      p->channel[1].iko_length_initialized != 0)
    {
      p->iko_length_initialized = 1;
      // привязка к каналу с максимальной дальностью
      if (p->channel[0].iko_length >= p->channel[1].iko_length)
        {
          p->iko_length = p->channel[0].iko_length;
          p->data_rate = p->channel[0].data_rate;
          p->channel[0].freq = 1.0;
          p->channel[1].freq = p->channel[0].data_rate / p->channel[1].data_rate;
        }
      else
        {
          p->iko_length = p->channel[1].iko_length;
          p->data_rate = p->channel[1].data_rate;
          p->channel[1].freq = 1.0;
          p->channel[0].freq = p->channel[1].data_rate / p->channel[0].data_rate;
        }
      // инициализируем индикатор
      hyscan_gtk_gliko_area_init_dimension (HYSCAN_GTK_GLIKO_AREA (p->iko), p->num_azimuthes, p->iko_length);

      g_object_set (p->grid, "gliko-radius", p->iko_length * 0.5f * p->sound_speed / p->data_rate, NULL);

      g_object_set (p->iko, "gliko-scale", p->scale, NULL);
      g_object_set (p->grid, "gliko-scale", p->scale, NULL);

      g_object_set (p->iko, "gliko-freq1", p->channel[0].freq, NULL);
      g_object_set (p->iko, "gliko-freq2", p->channel[1].freq, NULL);

      // номер отсчета глубины для корректировки геометрических искажений
      g_object_set (p->iko, "gliko-bottom", (int) (2.0f * p->bottom * p->data_rate / p->sound_speed), NULL);

      g_signal_emit (G_OBJECT (user_data), gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);

      p->fade_time = time;
    }

  if (p->iko_length_initialized == 0)
    {
      return;
    }

  // обрабатываем каналы левого и правого борта
  channel_ready (p, 0, time);
  channel_ready (p, 1, time);
  if ((time - p->fade_time) > 1000000)
    {
      hyscan_gtk_gliko_area_fade (HYSCAN_GTK_GLIKO_AREA (p->iko));
    }
}

/**
 * hyscan_gtk_gliko_set_player:
 * @instance: указатель на объект индикатора кругового обзора
 * @player: указатель на объект проигрывателя гидроакустических данных
 *
 * Подключает индикатор кругового обзора к проигрывателю
 * гидроакустических данных.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_player (HyScanGtkGliko *instance,
                             HyScanDataPlayer *player)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  g_object_ref (G_OBJECT (player));
  p->player = player;
  g_signal_connect (p->player, "track-changed", G_CALLBACK (player_track_changed_callback), instance);
  g_signal_connect (p->player, "ready", G_CALLBACK (player_ready_callback), instance);
  g_signal_connect (p->player, "process", G_CALLBACK (player_process_callback), instance);
}

/**
 * hyscan_gtk_gliko_get_player:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Возвращает указатель на объект проигрывателя гидроакустических данных.
 * Returns: указатель на объект проигрывателя гидроакустических данных
 */
HYSCAN_API
HyScanDataPlayer *
hyscan_gtk_gliko_get_player (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->player;
}

/**
 * hyscan_gtk_gliko_set_scale:
 * @instance: указатель на объект индикатора кругового обзора
 * @scale: коэффициент масштаба изображения
 *
 * Задает масштаб изображения на индикаторе кругового обзора.
 * Коэффициент масштаба равен отношению диаметра круга к
 * высоте области элемента диалогового интерфейса.
 * Величина 1,0 (значение по умолчанию)
 * соответствует случаю, когда диаметр круга,
 * вписан в высоту области элемента диалогового интерфейса.
 * Величина больше 1 соответствует уменьшению изображения кругового обзора.
 * Величина меньше 1 соответствует увеличение изображения кругового обзора
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_scale (HyScanGtkGliko *instance,
                            const gdouble scale)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->scale = (float) scale;
  g_object_set (p->iko, "gliko-scale", p->scale, NULL);
  g_object_set (p->grid, "gliko-scale", p->scale, NULL);
  g_signal_emit (instance, gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);
}

/**
 * hyscan_gtk_gliko_get_scale:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Получение текущего коэффициента масштаба изображения.
 * Returns: коэффициент масштаба изображения
 */
HYSCAN_API
gdouble
hyscan_gtk_gliko_get_scale (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->scale;
}

/**
 * hyscan_gtk_gliko_set_center:
 * @instance: указатель на объект индикатора кругового обзора
 * @cx: смещение по оси X (направление оси слева направо)
 * @cy: смещение по оси Y (направление оси снизу вверх)
 *
 * Задает смещение центра круговой развертки относительно
 * центра области элемента диалогового интерфейса.
 * Смещение задается в единицах относительно диаметра круга,
 * в которых размер диаметра равен 1.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_center (HyScanGtkGliko *instance,
                             const gdouble cx,
                             const gdouble cy)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->cx = (float) cx;
  p->cy = (float) cy;
  g_object_set (G_OBJECT (p->iko), "gliko-cx", p->cx, "gliko-cy", p->cy, NULL);
  g_object_set (G_OBJECT (p->grid), "gliko-cx", p->cx, "gliko-cy", p->cy, NULL);
  g_signal_emit (instance, gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);
}

/**
 * hyscan_gtk_gliko_get_center:
 * @instance: указатель на объект индикатора кругового обзора
 * @cx: указатель переменную, в которой будет
 * сохранено смещение по оси X
 * @cy: указатель на переменную, в которой будет
 * сохранено смещение по оси Y
 *
 * Получение смещения центра круговой развертки относительно
 * центра области элемента диалогового интерфейса.
 */
HYSCAN_API
void
hyscan_gtk_gliko_get_center (HyScanGtkGliko *instance,
                             gdouble *cx,
                             gdouble *cy)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  *cx = p->cx;
  *cy = p->cy;
}

/**
 * hyscan_gtk_gliko_set_contrast:
 * @instance: указатель на объект индикатора кругового обзора
 * @contrast: значение контрастности изображения
 *
 * Регулировка контрастности изображения.
 * Значение контрастности изображения задается в диапазоне
 * [-1;+1].
 * Яркость отметки гидроакустического изображения
 * вычисляется по следущей формуле:
 * I = B + C * A,
 * где A - нормированная амплитуда сигнала (0..255),
 *     С - коэффициент контрастности,
 *     B - постоянная составляющая яркости;
 *     I - результирующая яркость отметки
 * Коэффициент контрастности рассчитывается по формуле
 * C = 1 / (1 - @contrast), при @contrast > 0
 * C = 1 + @contrast, при @contrast <= 0
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_contrast (HyScanGtkGliko *instance,
                               const gdouble contrast)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->contrast = (float) contrast;
  g_object_set (p->iko, "gliko-contrast", p->contrast, NULL);
}

/**
 * hyscan_gtk_gliko_get_contrast:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Получение текущей контрастности гидроакустического изображения.
 * Returns: значение контрастности гидроакустического изображения.
 */
HYSCAN_API
gdouble
hyscan_gtk_gliko_get_contrast (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->contrast;
}

/**
 * hyscan_gtk_gliko_set_brightness:
 * @instance: указатель на объект индикатора кругового обзора
 * @contrast: значение яркости изображения
 *
 * Регулировка яркости изображения.
 * Расчет яркости отметки гидроакустического изображения приведен
 * в описании hyscan_gtk_gliko_set_contrast.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_brightness (HyScanGtkGliko *instance,
                                 const gdouble brightness)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->brightness = (float) brightness;
  g_object_set (p->iko, "gliko-bright", p->brightness, NULL);
}

/**
 * hyscan_gtk_gliko_get_brightness:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Получение текущей яркости гидроакустического изображения.
 * Returns: значение яркости гидроакустического изображения.
 */
HYSCAN_API
gdouble
hyscan_gtk_gliko_get_brightness (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->brightness;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_black_point (HyScanGtkGliko *instance,
                                  const gdouble black)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->black = (float) black;
  g_object_set (p->iko, "gliko-black", p->black, NULL);
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_black_point (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->black;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_white_point (HyScanGtkGliko *instance,
                                  const gdouble white)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->white = (float) white;
  g_object_set (p->iko, "gliko-white", p->white, NULL);
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_white_point (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->white;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_gamma_value (HyScanGtkGliko *instance,
                                  const gdouble gamma)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->gamma = (float) gamma;
  g_object_set (p->iko, "gliko-gamma", p->gamma, NULL);
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_gamma_value (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->gamma;
}

HYSCAN_API
HyScanSourceType
hyscan_gtk_gliko_get_source (HyScanGtkGliko *instance,
                             const gint c)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->channel[c].source;
}

/**
 * hyscan_gtk_gliko_set_fade_koeff:
 * @instance: указатель на объект индикатора кругового обзора
 * @koef: коэффициент гашения
 *
 * Задает коэффициент гашения kF для обеспечения послесвечениия
 * гидроакустической информации.
 * Яркость отметок гидроакустической информации изменяется
 * при каждой итерации гашения по формуле
 * A[i] = A[i-1] * kF
 * Период итераций гашения задается вызовом hyscan_gtk_gliko_set_fade_period.
 * По умолчанию используется значение kF равное 0,98.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_fade_koeff (HyScanGtkGliko *instance,
                                 const gdouble koef)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->fade_coef = (float) koef;
  g_object_set (p->iko, "gliko-fade-coef", p->fade_coef, NULL);
}

/**
 * hyscan_gtk_gliko_get_fade_koeff:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Возвращает значение коэффициента гашения.
 * Returns: коэффициент гашения.
 */
HYSCAN_API
gdouble
hyscan_gtk_gliko_get_fade_koeff (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->fade_coef;
}

/**
 * hyscan_gtk_gliko_set_rotation:
 * @instance: указатель на объект индикатора кругового обзора
 * @alpha: угол поворота, градусы
 *
 * Задает поворот гидроакустического изображения от нулевого угла (направление вверх)
 * относительно центра круга.
 * Угол поворота задается в градусах,
 * увеличение угла соответствует направлению поворота
 * по часовой стрелке.
 */
HYSCAN_API
void
hyscan_gtk_gliko_set_rotation (HyScanGtkGliko *instance,
                               const gdouble alpha)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->rotation = (float) alpha;
  g_object_set (p->iko, "gliko-rotate", p->full_rotation + p->rotation, NULL);
  g_signal_emit (instance, gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);
}

/**
 * hyscan_gtk_gliko_get_rotation:
 * @instance: указатель на объект индикатора кругового обзора
 *
 * Получение текущего угла поворота гидроакустического изображения.
 * Returns: угол поворота, градусы.
 */
HYSCAN_API
gdouble
hyscan_gtk_gliko_get_rotation (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->rotation;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_full_rotation (HyScanGtkGliko *instance,
                                    const gdouble alpha)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->full_rotation = (float) alpha;
  g_object_set (p->iko, "gliko-rotate", p->full_rotation + p->rotation, NULL);
  g_object_set (p->grid, "gliko-rotate", p->full_rotation, NULL);
  g_signal_emit (instance, gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_full_rotation (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->full_rotation;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_angular_source (HyScanGtkGliko *instance, const gint channel)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->nmea_angular_source = channel;
}

HYSCAN_API
gint
hyscan_gtk_gliko_get_angular_source (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  return p->nmea_angular_source;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_source_name (HyScanGtkGliko *instance, const gint channel_index, const gchar *source_name)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  channel_t *c;
  HyScanSourceType source;

  if ((channel_index & 1) != channel_index)
    {
      g_error ("bad channel index %d", channel_index);
      return;
    }
  source = hyscan_source_get_type_by_id (source_name);
  if (source == HYSCAN_SOURCE_INVALID)
    {
      g_error ("unknown source type %s", source_name);
      return;
    }
  c = p->channel + channel_index;
  if (c->source_name != NULL)
    {
      g_free (c->source_name);
      c->source_name = NULL;
    }
  c->source_name = g_strdup (source_name);
  c->source = source;
}

static char *
str_rgb (char *t, const guint32 c)
{
  sprintf (t, "#%02X%02X%02X", (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  return t;
}

/**
 *
 * Функция устанавливает цветовую схему.
 * Можно передать массив любого размера (больше нуля).
 *
 * \param color указатель на \link HyScanTileColor \endlink;
 * \param source тип источника, к которому будут применены параметры;
 * \param colormap значения цветов точек;
 * \param length число элементов в цветовой схеме;
 * \param background цвет фона.
 *
 * \return TRUE, если цвета успешно установлены.
 */
HYSCAN_API
gboolean
hyscan_gtk_gliko_set_colormap (HyScanGtkGliko *instance,
                               const gint channel_index,
                               guint32 *colormap,
                               guint length,
                               guint32 background)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  char t[64];
  float a = 1.0f / 255.0f;
  guint32 color = colormap[length - 1];
  int i;

  if (channel_index == 0)
    {
      g_object_set (p->iko, "gliko-color1-rgb", str_rgb (t, color), NULL);
      g_object_set (p->iko, "gliko-color1-alpha", a * (color >> 24), NULL);
    }
  else if (channel_index == 1)
    {
      g_object_set (p->iko, "gliko-color2-rgb", str_rgb (t, color), NULL);
      g_object_set (p->iko, "gliko-color2-alpha", a * (color >> 24), NULL);
    }
  else
    {
      return FALSE;
    }
  g_object_set (p->iko, "gliko-background-rgb", str_rgb (t, background), NULL);
  g_object_set (p->iko, "gliko-background-alpha", a * (background >> 24), NULL);
  hyscan_gtk_gliko_overlay_set_background (HYSCAN_GTK_GLIKO_OVERLAY (instance), background);

  i = (((background >> 16) & 0xFF) + ((background >> 8) & 0xFF) + (background & 0xFF)) / 3;
  if (i >= 0x40)
    {
      g_object_set (p->grid, "gliko-color-rgb", "#000000", NULL);
    }
  else
    {
      g_object_set (p->grid, "gliko-color-rgb", "#FFFFFF", NULL);
    }
  return TRUE;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_bottom (HyScanGtkGliko *instance,
                             const gdouble bottom)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->bottom = (float) bottom;
  if (p->iko_length != 0)
    {
      g_object_set (p->iko, "gliko-bottom", (int) (2.0f * p->bottom * p->data_rate / p->sound_speed), NULL);
      g_signal_emit (instance, gliko_signal[SIGNAL_PARAMETERS_CHANGED], 0);
    }
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_bottom (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->bottom;
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_step_distance (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  float f = 0.0;

  g_object_get (G_OBJECT (p->grid), "gliko-step-distance", &f, NULL);
  return f;
}

HYSCAN_API
void
hyscan_gtk_gliko_pixel2polar (HyScanGtkGliko *instance,
                              const gdouble x,
                              const gdouble y,
                              gdouble *a,
                              gdouble *r)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  gdouble w, h;
  gdouble px, py;
  gdouble pcx, pcy;
  gdouble real_scale;
  gdouble d;
  const gdouble radians_to_degrees = 180.0 / G_PI;

  if (p->height == 0)
    return;

  // ширина и высота
  w = p->width;
  h = p->height;

  // реальный масштаб изображения на экране
  real_scale = p->scale * p->iko_length * 2.0 / h;

  // пиксельные координаты точки относительно центра изображения
  px = x;
  px -= (0.5 * w);

  py = 0.5 * h - y;

  // пиксельные координаты центра развертки
  pcx = p->cx * 2.0 * p->iko_length / real_scale;
  pcy = p->cy * 2.0 * p->iko_length / real_scale;

  // пиксельные координаты точки относительно центра развертки
  px -= pcx;
  py -= pcy;

  // угол в градусах
  d = atan2 (px, py) * radians_to_degrees - p->full_rotation;

  // в диапазоне от 0 до 360
  if (d > 360.0)
    {
      d -= 360.0;
    }
  if (d < 0.0)
    {
      d += 360.0;
    }
  *a = d;

  // дальность в пикселях
  d = sqrt (px * px + py * py);

  // дальность в дискретах
  d *= real_scale;

  // дальность в метрах при наличии частоты дискретизации
  if (p->data_rate > 1.0)
    {
      d = d * 0.5 * p->sound_speed / p->data_rate;
    }
  else
    {
      d = 0.0;
    }
  *r = d;
}

HYSCAN_API
void
hyscan_gtk_gliko_polar2pixel (HyScanGtkGliko *instance,
                              const gdouble a,
                              const gdouble r,
                              gdouble *x,
                              gdouble *y)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  gdouble w, h;
  gdouble px, py;
  gdouble pcx, pcy;
  gdouble real_scale;
  gdouble d;
  const gdouble degrees_to_radians = G_PI / 180.0;

  if (p->height == 0)
    return;

  // ширина и высота
  w = p->width;
  h = p->height;

  // реальный масштаб изображения на экране
  real_scale = p->scale * p->iko_length * 2.0 / h;

  // пиксельные координаты центра развертки
  pcx = p->cx * 2.0 * p->iko_length / real_scale;
  pcy = p->cy * 2.0 * p->iko_length / real_scale;

  // дальность в дискретах
  d = r * 2.0 * p->data_rate / p->sound_speed;

  // дальность в пикселях
  d = d / real_scale;

  // пискельные координаты точки
  px = d * sinf ((a + p->full_rotation) * degrees_to_radians);
  py = d * cosf ((a + p->full_rotation) * degrees_to_radians);

  // координаты пикселя на экране
  px += pcx;
  py += pcy;

  px += (0.5 * w);
  py = 0.5 * h - py;

  *x = px;
  *y = py;
}

HYSCAN_API
void
hyscan_gtk_gliko_set_balance (HyScanGtkGliko *instance,
                              const gdouble balance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);

  p->balance = (float) balance;
  g_object_set (p->iko, "gliko-balance", p->balance, NULL);
}

HYSCAN_API
gdouble
hyscan_gtk_gliko_get_balance (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE (instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate);
  return p->balance;
}
