#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-nmea-data.h>
#include <hyscan-data-player.h>

#include "hyscan-gtk-gliko.h"

#include <string.h>
#include <GL/gl.h>

#include "hyscan-gtk-gliko-overlay.h"
#include "hyscan-gtk-gliko-area.h"
#include "hyscan-gtk-gliko-grid.h"

#include "hyscan-gtk-gliko-que.h"

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
  data_que_t data_que_buffer[128];
  que_t data_que;
  int64_t alpha_que_count;
  int64_t data_que_count;
  guint32 process_index;
  gint64 process_time;
  gint64 alpha_time;
  gdouble alpha_value;
  unsigned char *buffer;
  guint32 max_length;
  guint32 allocated;
  int process_init;
  int ready_init;
  int gliko_channel;
  int ready_alpha_init;
  int ready_data_init;
  int azimuth_displayed;
  guint32 azimuth;
  gchar *source_name;
} channel_t;

typedef struct _HyScanGtkGlikoPrivate HyScanGtkGlikoPrivate;

struct _HyScanGtkGliko
{
  HyScanGtkGlikoOverlay parent;
  void *pv;
};

struct _HyScanGtkGlikoClass
{
  HyScanGtkGlikoOverlayClass parent_class;
};

struct _HyScanGtkGlikoPrivate
{
  HyScanGtkGlikoArea *iko;
  HyScanGtkGlikoGrid *grid;

  HyScanDB *db;
  HyScanNMEAData *nmea_data;
  HyScanDataPlayer *player;

  gint32 num_azimuthes;
  float scale;
  float cx;
  float cy;
  float contrast;
  float brightness;

  gdouble white_point;
  gdouble black_point;
  gdouble gamma_value;

  channel_t channel[2];
  guint32 iko_length;

  int player_open;

  guint32 nmea_index;
  int nmea_init;

  alpha_que_t alpha_que_buffer[128];
  que_t alpha_que;

};

/* Define type */
G_DEFINE_TYPE( HyScanGtkGliko, hyscan_gtk_gliko, HYSCAN_TYPE_GTK_GLIKO_OVERLAY )

/* Internal API */
static void dispose( GObject *gobject );
static void finalize( GObject *gobject );

//static void get_preferred_width( GtkWidget *widget, gint *minimal_width, gint *natural_width );
//static void get_preferred_height( GtkWidget *widget, gint *minimal_height, gint *natural_height );

/* Initialization */
static void hyscan_gtk_gliko_class_init( HyScanGtkGlikoClass *klass )
{
  GObjectClass *g_class = G_OBJECT_CLASS( klass );
  //GtkWidgetClass *w_class = GTK_WIDGET_CLASS( klass );

  /* Add private data */
  g_type_class_add_private( klass, sizeof( HyScanGtkGlikoPrivate ) );

  g_class->dispose = dispose;
  g_class->finalize = finalize;

  //w_class->get_preferred_width  = get_preferred_width;
  //w_class->get_preferred_height = get_preferred_height;
}

static void hyscan_gtk_gliko_init( HyScanGtkGliko *instance )
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->iko = NULL;
  p->grid = NULL;

  p->db = NULL;
  p->nmea_data = NULL;
  p->player = NULL;

  p->num_azimuthes = 1024;
  p->scale = 1.0f;
  p->cx = 0.0f;
  p->cy = 0.0f;
  p->contrast = 0.0f;
  p->brightness = 0.0f;

  p->white_point = 1.0;
  p->black_point = 0.0;
  p->gamma_value = 1.0;

  p->iko_length = 0;

  p->player_open = 0;

  p->nmea_index = 0;
  p->nmea_init = 0;

  p->channel[0].source_name = "ss-starboard";
  p->channel[1].source_name = "ss-port";

  p->channel[0].gliko_channel = 0;
  p->channel[1].gliko_channel = 1;

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


  p->iko = hyscan_gtk_gliko_area_new();
  p->grid = hyscan_gtk_gliko_grid_new();
  hyscan_gtk_gliko_overlay_set_layer( HYSCAN_GTK_GLIKO_OVERLAY( instance ), 0, HYSCAN_GTK_GLIKO_LAYER( p->iko ) );
  hyscan_gtk_gliko_overlay_enable_layer( HYSCAN_GTK_GLIKO_OVERLAY( instance ), 0, 1 );
  hyscan_gtk_gliko_overlay_set_layer( HYSCAN_GTK_GLIKO_OVERLAY( instance ), 1, HYSCAN_GTK_GLIKO_LAYER( p->grid ) );
  hyscan_gtk_gliko_overlay_enable_layer( HYSCAN_GTK_GLIKO_OVERLAY( instance ), 1, 1 );

  g_object_set( p->iko, "gliko-color1-rgb", "#00FF80", NULL );
  g_object_set( p->iko, "gliko-color1-alpha", 1.0f, NULL );
  g_object_set( p->iko, "gliko-color2-rgb", "#FF8000", NULL );
  g_object_set( p->iko, "gliko-color2-alpha", 1.0f, NULL );
  g_object_set( p->iko, "gliko-background-rgb", "#404040", NULL );
  g_object_set( p->iko, "gliko-background-alpha", 0.25f, NULL );

  g_object_set( p->iko, "gliko-fade-coef", 0.98f, NULL );
  g_object_set( p->iko, "gliko-scale", p->scale, NULL );
  g_object_set( p->grid, "gliko-scale", p->scale, NULL );
}

static void dispose( GObject *gobject )
{
  G_OBJECT_CLASS( hyscan_gtk_gliko_parent_class )->dispose( gobject );
}

static void finalize( GObject *gobject )
{
  G_OBJECT_CLASS( hyscan_gtk_gliko_parent_class )->finalize( gobject );
}

GtkWidget *hyscan_gtk_gliko_new( void )
{
  return GTK_WIDGET( g_object_new( hyscan_gtk_gliko_get_type(), NULL ) );
}

HYSCAN_API
void            hyscan_gtk_gliko_set_num_azimuthes (HyScanGtkGliko *instance,
                                                const guint num_azimuthes)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  p->num_azimuthes = num_azimuthes;
}

HYSCAN_API
guint           hyscan_gtk_gliko_get_num_azimuthes (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->num_azimuthes;
}

// открытие канала данных
static void
channel_open( HyScanGtkGlikoPrivate *p,
              const int          channel_index,
              HyScanDataPlayer   *player,
              HyScanDB           *db,
              const gchar        *project_name,
              const gchar        *track_name)
{
  channel_t *c = p->channel + channel_index;

  c->source = hyscan_source_get_type_by_id (c->source_name);
  if (c->source == HYSCAN_SOURCE_INVALID)
    {
      g_error ("unknown source type %s", c->source_name);
    }

  // инициализируем очередь строк данных
  initque( &c->data_que, c->data_que_buffer, sizeof( data_que_t ), sizeof( c->data_que_buffer ) / sizeof( c->data_que_buffer[0] ) );

  // инициализируем индексы очередей
  c->alpha_que_count = p->alpha_que.count;
  c->data_que_count = c->data_que.count;

  // обнуляем признак инициализации обработки
  c->process_init = 0;
  c->ready_init = 0;

  // размер считываемой строки
  c->max_length = 0;

  // размер зарезервированного буфера
  c->allocated = 0;

  // указатель на буфер
  c->buffer = NULL;

  // последний выданный на индикатор азимутальный дискрет
  c->azimuth_displayed = 0;
  c->azimuth = 0;

  /* Объект обработки акустических данных. */
  c->acoustic_data = hyscan_acoustic_data_new (db, NULL, project_name, track_name, c->source, 1, FALSE);
  if (c->acoustic_data == NULL)
    {
      g_warning ("can't open acoustic data source");
    }
}

// обработчик сигнала open
static void
player_open_callback (HyScanDataPlayer *player,
                      HyScanDB         *db,
                      const gchar      *project_name,
                      const gchar      *track_name,
                      gpointer          user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  if( ++p->player_open != 2 ) return;

  initque( &p->alpha_que, p->alpha_que_buffer, sizeof( alpha_que_t ), sizeof( p->alpha_que_buffer ) / sizeof( p->alpha_que_buffer[0] ) );

  p->nmea_data = hyscan_nmea_data_new( db, NULL, project_name, track_name, 1 );
  if (p->nmea_data == NULL)
    {
      g_warning ("can't open nmea data\n");
      return;
    }

  channel_open( p, 0, player, db, project_name, track_name );
  channel_open( p, 1, player, db, project_name, track_name );
}

// обработка индексов принятых строк
static void
channel_process( HyScanGtkGlikoPrivate *p,
                 const int          channel_index,
                 gint64 time )
{
  channel_t *c = p->channel + channel_index;
  guint32 ileft = 0, iright;
  gint32 idelta;
  gint64 tleft, tright;
  data_que_t data;

  if( c->acoustic_data == NULL )
  {
    return;
  }
  // проверяем наличие акустических данных
  if( hyscan_acoustic_data_find_data( c->acoustic_data, time, &ileft, &iright, &tleft, &tright ) != HYSCAN_DB_FIND_OK )
  {
    return;
  }
  if( !c->process_init )
  {
    c->process_time = time;
    c->process_index = ileft;
    c->process_init = 1;
    return;
  }
  // приращение индекса для акустических данных
  idelta = ((ileft >= c->process_index) ? 1: -1);

  // сохраняем в очереди данных индексы строк за время с момента последнего вызова
  for( ; c->process_index != ileft; c->process_index += idelta )
  {
    if( hyscan_acoustic_data_get_size_time( c->acoustic_data, c->process_index, &data.length, &data.time ) )
    {
      data.index = c->process_index;
      enque( &c->data_que, &data, 1 );
      if( data.length > c->max_length && p->iko_length == 0 )
      {
        c->max_length = data.length;
      }
    }
  }
}

// обработчик сигнала process
void
player_process_callback (HyScanDataPlayer *player,
                  gint64            time,
                  gpointer          user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  guint32 inleft, inright;
  gint64 tnleft, tnright;
  int indelta;
  alpha_que_t alpha;

  // обработка индексов строк данных для двух каналов
  channel_process( p, 0, time );
  channel_process( p, 1, time );

  // запрашиваем индексы по текущему времени
  if( hyscan_nmea_data_find_data( p->nmea_data, time, &inleft, &inright, &tnleft, &tnright ) != HYSCAN_DB_FIND_OK )
  {
    return;
  }
  // инициализации еще не было?
  if( !p->nmea_init )
  {
    // запоминаем индексы
    p->nmea_index = inleft;
    p->nmea_init = 1;
  }

  // приращение индекса для данных nmea
  indelta = ((inleft >= p->nmea_index) ? 1: -1);

  // обрабатываем данные от датчика угла
  for( ; p->nmea_index != inleft; p->nmea_index += indelta )
  {
    const gchar *nmea;
    const char header[4] = { '$', 'R', 'A', ',' };

    /* Считываем строку nmea */
    nmea = hyscan_nmea_data_get( p->nmea_data, p->nmea_index, &alpha.time );

    /* обрабатываем только датчик угла поворота */
    if( memcmp( nmea, header, sizeof( header ) ) != 0 )
    {
      continue;
    }

    /* текущий угол поворота, градусы 0..360 */
    alpha.value = g_ascii_strtod( nmea + sizeof( header ), NULL );

    // буферизуем считанное значение в очереди
    enque( &p->alpha_que, &alpha, 1 );
  }
}

// обработка готовых к индикации строк
static void
channel_ready( HyScanGtkGlikoPrivate *p,
               const int          channel_index,
               gint64 time )
{
  channel_t *c = p->channel + channel_index;
  int ra, rd;
  alpha_que_t alpha;
  data_que_t data;
  gdouble a, d, dm, dn, dp;
  const gfloat *amplitudes;
  guint32 j, k, length;
  gint64 t;

  // длина строки должна быть определена
  if( c->max_length == 0 )
  {
    return;
  }

  // резервируем буфер для строки отсчетов
  if( c->allocated == 0 )
  {
    for( c->allocated = (1 << 10); c->allocated < c->max_length; c->allocated <<= 1 );
    c->buffer = g_malloc0( c->allocated * sizeof( unsigned char ) );
  }

  // просматриваем очередь данных датчика угла
  do
  {
    // считываем один элемент из очереди
    ra = deque( &p->alpha_que, &alpha, 1, c->alpha_que_count );

    // если произошло переполнение буфера очереди
    if( ra < 0 )
    {
      // начинаем чтение с текущей позиции
      c->alpha_que_count = p->alpha_que.count;
      return;
    }
    // если нового показания нет, выходим
    if( ra == 0 )
    {
      return;
    }

    // если параметры датчика угла не были проинициализированы
    if( !c->ready_alpha_init )
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
    if( alpha.time >= time )
    {
      break;
    }
    // отсуствие разницы во времени тоже не допустимо
    if( alpha.time == c->alpha_time )
    {
      // следующий элемент очереди
      c->alpha_que_count++;
      continue;
    }
    /* изменение угла, с учетом перехода через 0 */
    dn = alpha.value - c->alpha_value;
    dm = dn - 360.0;
    dp = dn + 360.0;
    d = dn;
    if( fabs( dm ) < fabs( dn ) )
    {
      d = dm;
    }
    else if( fabs( dp ) < fabs( dn ) )
    {
      d = dp;
    }

    // у нас есть два замера угла,
    // обрабатываем очередь данных,
    // предполагая, что угол изменялся линейно
    do
    {
      // считываем один элемент из очереди
      rd = deque( &c->data_que, &data, 1, c->data_que_count );

      // если произошло переполнение буфера очереди
      if( rd < 0 )
      {
        // начинаем обработку с текущего места
        c->data_que_count = c->data_que.count;
        // требуется инициализация данных
        c->ready_data_init = 0;
        // прекращаем работу до следующей итерации
        return;
      }
      // если новых данных нет, выходим
      if( rd == 0 )
      {
        return;
      }
      // если вышли за интервал замера угла
      if( data.time >= alpha.time )
      {
        // переходим к следующему углу
        break;
      }

      // следующая строка изображения
      c->data_que_count++;

      // значение угла
      a = c->alpha_value + d * (data.time - c->alpha_time) / (alpha.time - c->alpha_time);

      /* допустимый диапазон 0..360 */
      while( a < 0.0 )
      {
        a += 360.0;
      }
      while( a >= 360.0 )
      {
        a -= 360.0;
      }

      /* номер углового дискрета */
      j = (guint32)(a * p->num_azimuthes / 360.0);
      j %= p->num_azimuthes;

      // если уже есть отрисованный азимут
      if( c->azimuth_displayed )
      {
        // если луч перепрыгнул через 1 азимутальный дискрет
        if( j == ((c->azimuth + 2) % p->num_azimuthes) )
        {
          // дублируем предыдущий азимут
          hyscan_gtk_gliko_area_set_data( HYSCAN_GTK_GLIKO_AREA( p->iko ), c->gliko_channel, (c->azimuth + 1) % p->num_azimuthes, c->buffer );
        }
        else if( j == ((c->azimuth - 2) % p->num_azimuthes) )
        {
          hyscan_gtk_gliko_area_set_data( HYSCAN_GTK_GLIKO_AREA( p->iko ), c->gliko_channel, (c->azimuth - 1) % p->num_azimuthes, c->buffer );
        }
      }

      // считываем строку акустического изображения
      amplitudes = hyscan_acoustic_data_get_amplitude (c->acoustic_data, NULL, data.index, &length, &t );

      if (amplitudes == NULL)
      {
        g_warning ("can't read acoustic line %d", data.index);
        break;
      }

      if( length > c->max_length )
      {
        length = c->max_length;
      }

      /* формируем строку амплитуд */
      for (k = 0; k < length; k++)
      {
        gdouble amplitude;

        amplitude = (amplitudes[k] - p->black_point) / (p->white_point - p->black_point);
        amplitude = powf (amplitude, p->gamma_value);
        amplitude = CLAMP (amplitude, 0.0, 1.0);
        c->buffer[k] = 255 * amplitude;
      }

      /* передаем строку в индикатор кругового обзора */
      if( p->iko_length != 0 )
      {
        hyscan_gtk_gliko_area_set_data( HYSCAN_GTK_GLIKO_AREA( p->iko ), c->gliko_channel, j, c->buffer );
        c->azimuth = j;
        c->azimuth_displayed = 1;
      }

    } while( rd > 0 );

    // конец текущего временного интервала становится началом следущего
    c->alpha_value = alpha.value;
    c->alpha_time = alpha.time;

    // следующая угловая метка
    c->alpha_que_count++;

  } while( ra > 0 );
}

// обработчик сигнала ready
void
player_ready_callback (HyScanDataPlayer *player,
                gint64            time,
                gpointer          user_data)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( user_data, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  //g_print ("Ready time: %"G_GINT64_FORMAT" %d\n", g_get_monotonic_time (), misc_read);

  // если ни одна строка не была принята, выходим
  if( p->channel[0].max_length == 0 && p->channel[1].max_length == 0 ) return;

  // если индикатор кругового обзора не проинициализирован
  if( p->iko_length == 0 )
  {
    // берем максимальную дальность из двух каналов
    p->iko_length = p->channel[0].max_length;
    if( p->channel[1].max_length > p->iko_length )
    {
      p->iko_length = p->channel[1].max_length;
    }
    // инициализируем индикатор
    hyscan_gtk_gliko_area_init_dimension( HYSCAN_GTK_GLIKO_AREA( p->iko ), p->num_azimuthes, p->iko_length );
    g_object_set( p->iko, "gliko-scale", p->scale, NULL );
    g_object_set( p->grid, "gliko-scale", p->scale, NULL );
  }
  // обрабатываем каналы левого и правого борта
  channel_ready( p, 0, time );
  channel_ready( p, 1, time );
}

HYSCAN_API
void            hyscan_gtk_gliko_set_player (HyScanGtkGliko *instance,
                                         HyScanDataPlayer *player)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->player = player;
  g_signal_connect (p->player, "open", G_CALLBACK (player_open_callback), instance);
  g_signal_connect (p->player, "ready", G_CALLBACK (player_ready_callback), instance);
  g_signal_connect (p->player, "process", G_CALLBACK (player_process_callback), instance);
}

HYSCAN_API
HyScanDataPlayer *hyscan_gtk_gliko_get_player (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->player;
}

HYSCAN_API
void           hyscan_gtk_gliko_set_scale (HyScanGtkGliko *instance,
                                       const gdouble scale)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->scale = (float)scale;
  g_object_set( p->iko, "gliko-scale", p->scale, NULL );
  g_object_set( p->grid, "gliko-scale", p->scale, NULL );
}

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_scale (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->scale;
}

HYSCAN_API
void           hyscan_gtk_gliko_set_center (HyScanGtkGliko *instance,
                                        const gdouble cx,
                                        const gdouble cy)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->cx = (float)cx;
  p->cy = (float)cy;
  g_object_set( G_OBJECT( p->iko  ), "gliko-cx", p->cx, "gliko-cy", p->cy, NULL );
  g_object_set( G_OBJECT( p->grid ), "gliko-cx", p->cx, "gliko-cy", p->cy, NULL );
}

HYSCAN_API
void           hyscan_gtk_gliko_get_center (HyScanGtkGliko *instance,
                                        gdouble *cx,
                                        gdouble *cy)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  *cx = p->cx;
  *cy = p->cy;
}

HYSCAN_API
void           hyscan_gtk_gliko_set_contrast (HyScanGtkGliko *instance,
                                          const gdouble contrast)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->contrast = (float)contrast;
  g_object_set( p->iko, "gliko-contrast", p->contrast, NULL );
}

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_contrast (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->contrast;
}

HYSCAN_API
void           hyscan_gtk_gliko_set_brightness (HyScanGtkGliko *instance,
                                          const gdouble brightness)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );

  p->brightness = (float)brightness;
  g_object_set( p->iko, "gliko-bright", p->brightness, NULL );
}

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_brightness (HyScanGtkGliko *instance)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->brightness;
}

HYSCAN_API
HyScanSourceType hyscan_gtk_gliko_get_source (HyScanGtkGliko *instance,
                                          const gint c)
{
  HyScanGtkGlikoPrivate *p = G_TYPE_INSTANCE_GET_PRIVATE( instance, HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoPrivate );
  return p->channel[c].source;
}
