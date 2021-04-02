/*
Программа генерации тестовых данных для проверки индикатора кругового обзора

Для генерации данных проекта с тестовыми данными выполнить следующие команды:

$ cd ~/hyscan/bin
$ mkdir /tmp/ko
$ ./gen-ko-data file:///tmp/ko

*/

#include <hyscan-nmea-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-cached.h>
#include <string.h>
#include <math.h>
#include <glib/gprintf.h>

#define SENSOR_NAME    "rotation"
#define SENSOR_CHANNEL 1

//#define DATA_SIZE (8192+4096)
#define DATA_SIZE (22000)

gdouble rotation_value( const gint64 t, const gdouble speed, const gdouble range );
void nmea_rotation( char *buf, size_t *size, const gdouble value, const char *name );
void acoustic_get_info ( HyScanAcousticDataInfo *info, const guint n_channel);
void acoustic_samples( guint16 *d, const size_t n, const guint n_channel, const gint64 t, const gdouble rpm, int *f );

int main (int argc, char **argv)
{
  GError                 *error;
  gchar                  *db_uri = "file:///tmp/";
  gchar                  *name = "testko";
  HyScanDB               *db;

  /* Запись данных. */
  HyScanBuffer           *rbuffer;
  HyScanBuffer           *sbuffer;
  HyScanBuffer           *pbuffer;
  HyScanDataWriter       *writer;
  HyScanAntennaOffset     offset = {0};

  gint i;

  /* Параметры формирования данных */
  gdouble degrees_per_sec = 5.0;      /* скорость вращения, градус/сек */
  gdouble degrees_range = 90.0;       /* диапазон сканирования */
  gdouble rotate_sensor_sample = 0.2; /* период выдачи данных датчика обзора, с */
  gdouble scan_period = 0.100;        /* период зондирования, с */
  gdouble full_time = 180.0;          /* длительность формирования тестовых данных, с */
  gboolean verbose = FALSE;

  gint64 time0;
  gint64 rlim, slim, tlim;
  gint64 rcount, scount, tcount;
  guint16 *data_values;

  /* обрабатываем командную строку */
  {
    gchar **args;
    guint args_len;
    error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"rotation", 0, 0, G_OPTION_ARG_DOUBLE, &degrees_per_sec, "Rotation speed, degrees per second (default 5)", NULL},
      {"range", 0, 0, G_OPTION_ARG_DOUBLE, &degrees_range, "Scan from -range to +range, degrees (default 90)", NULL},
      {"rotate-sensor-sample", 0, 0, G_OPTION_ARG_DOUBLE, &rotate_sensor_sample, "Rotate sensor sample period, s (default 0.1)", NULL},
      {"scan-period", 0, 0, G_OPTION_ARG_DOUBLE, &scan_period, "Scan period, s (default 0.25)", NULL},
      {"full-time", 0, 0, G_OPTION_ARG_DOUBLE, &full_time, "Full time of simulation, s (default 180)", NULL},
      {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif
    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, TRUE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    args_len = g_strv_length (args);
    if (args_len == 2)
      {
        db_uri = g_strdup (args[1]);
      }
    else if (args_len > 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);

  if (db == NULL)
    g_error ("can't open db");

  /* Создаем объект записи данных. */
  writer = hyscan_data_writer_new ();

  /* Система хранения. */
  hyscan_data_writer_set_db (writer, db);

  /* Имя оператора. */
  hyscan_data_writer_set_operator_name (writer, "gen_ko_data");

  /* Информация о гидролокаторе. */
  hyscan_data_writer_set_sonar_info (writer, "This is sonar info");

  /* Время начала записи */
  time0 = 0; //g_get_real_time();

  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, NULL, time0))
  {
    hyscan_db_project_remove( db, name );
    if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, NULL, time0))
      g_error ("can't start write");
  }

  /* Смещение приёмных антенн. */
  hyscan_data_writer_sensor_set_offset (writer, SENSOR_NAME, &offset);
  hyscan_data_writer_sonar_set_offset (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &offset);
  hyscan_data_writer_sonar_set_offset (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT, &offset);

  /* буфер показаний датчика угла поворота */
  rbuffer = hyscan_buffer_new ();

  /* буфер выборок отсчетов */
  sbuffer = hyscan_buffer_new ();
  pbuffer = hyscan_buffer_new ();
  data_values = g_new (guint16, DATA_SIZE);

  /* параметры формирования данных */
  rlim = (gint64)( 1000000.0 * rotate_sensor_sample );
  slim = (gint64)( 1000000.0 * scan_period );
  tlim = (gint64)( 1000000.0 * full_time );

  rcount = rlim;
  scount = slim;

  /* формирование данных с шагом 1 мкс */
  for (i = 0, tcount = 0; tcount <= tlim; tcount++ )
    {
      // период формирования данных от датчика поворота
      if( rcount == rlim )
      {
        char tmp[256];
        size_t s = sizeof( tmp );

        nmea_rotation( tmp, &s, rotation_value( tcount, degrees_per_sec, degrees_range ), "HYRA" );

        i++;
        rcount = 0;

        hyscan_buffer_wrap (rbuffer, HYSCAN_DATA_BLOB, tmp, strlen (tmp));
        hyscan_data_writer_sensor_add_data (writer, SENSOR_NAME, HYSCAN_SOURCE_NMEA,
                                                  SENSOR_CHANNEL, time0 + tcount, rbuffer);

        if( verbose )
        {
          printf( "%d.%06d %s\n", (int)(tcount / 1000000), (int)(tcount % 1000000), tmp );
        }
      }
      rcount++;

      // период зондирования
      if( scount == slim )
      {
        HyScanAcousticDataInfo acoustic_info;
        int f;

        scount = 0;

        acoustic_get_info ( &acoustic_info, 1 );
        acoustic_samples( data_values, DATA_SIZE, 1, tcount, 10.0, &f );
        hyscan_buffer_wrap (sbuffer, HYSCAN_DATA_ADC14LE,
                            data_values, DATA_SIZE * sizeof (guint16));
        hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, FALSE,
                                                       time0 + tcount, &acoustic_info, sbuffer);

        acoustic_get_info ( &acoustic_info, 2 );
        acoustic_samples( data_values, DATA_SIZE, 2, tcount, 10.0, &f );
        hyscan_buffer_wrap (pbuffer, HYSCAN_DATA_ADC14LE,
                            data_values, DATA_SIZE * sizeof (guint16));
        hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, FALSE,
                                                       time0 + tcount, &acoustic_info, pbuffer);
        if( verbose )
        {
          printf( "%d.%06d acoustic data %s\n", (int)(tcount / 1000000), (int)(tcount % 1000000), f ? "full": "" );
        }
      }
      scount++;
    }

  /* останавливаем запись */
  hyscan_data_writer_stop (writer);  

  return 0;
}

gdouble rotation_value( const gint64 t, const gdouble speed, const gdouble range )
{
  double a, b, c, d;

  // период качания привода, с
  b = 4.0 * range / speed;

  // фаза движения
  c = fmod( 0.000001 * t + 0.25 * b, b );

  // полупериод качания привода
  d  = 0.5 * b;

  // в первом полупериоде вращение по часовой стрелке
  if( c < d )
  {
    a = -range + 2.0 * range * c / d;
  }
  else
  {
    // а во втором полупериоде вращение против часовой стрелки
    a = range - 2.0 * range * (c - d) / d;
  }
  return a;
}

void nmea_rotation( char *buf, size_t *size, const gdouble value, const char *name )
{
  size_t j, k;
  int s;

  bzero( buf, *size );
  snprintf( buf, *size - 1, "$%s,%.3lf", name, value );
  if( buf[ *size ] != 0 ) return;
  k = strlen( buf + 1 );
  if( (k + 4) > *size ) return;
  for( j = 0, s = 0; j < k; j++ )
  {
    s ^= buf[1+j];
  }
  snprintf( buf + k + 1, *size - k - 2, "*%02X", s & 0xFF );
  *size = strlen( buf );
}

void acoustic_get_info ( HyScanAcousticDataInfo *info, const guint n_channel)
{
  info->data_type = HYSCAN_DATA_ADC14LE;// + ((n_channel-1) % 2);
  info->data_rate = 1000.0 * n_channel;

  info->signal_frequency = 1.0 * n_channel;
  info->signal_bandwidth = 2.0 * n_channel;

  info->antenna_voffset = 3.0 * n_channel;
  info->antenna_hoffset = 4.0 * n_channel;
  info->antenna_vaperture = 5.0 * n_channel;
  info->antenna_haperture = 6.0 * n_channel;
  info->antenna_frequency = 7.0 * n_channel;
  info->antenna_bandwidth = 8.0 * n_channel;

  info->adc_vref = 9.0 * n_channel;
  info->adc_offset = 10 * n_channel;
}

void acoustic_samples( guint16 *d, const size_t n, const guint n_channel, const gint64 t, const gdouble rpm, int *f )
{
  static gint64 r = 0;     // прохождение 30 градусов
  static size_t k = 0;
  size_t i, j;
  const int z = (1 << 13);
  const int a = z - 1;
  int c;

  // нулевая амплитуда
  c = z;
  *f = 0;

  // каждые 30 градусов засвечиваем всю строку
  if( t >= r )
  {
    r += (gint64)(60000000.0 / (12.0 * rpm));
    c += (a >> 2);
    *f = 1;
  }

  for( i = 0; i < n; i++ )
  {
    d[i] = c;
  }
  i = (n >> 1);
  j = (n >> 2);
  d[ n - 1 ] = z + a;
  d[i] = z + a;
  d[i + 5 + (k&1) ] = z + a;
  d[i-k] = z + a;
  if( n_channel == 1 )
  {
    d[i-j-1] = z + a;
    d[i-j+1] = z + a;
  }
  if( n_channel == 2 )
  {
    d[i+j-1] = z + a;
    d[i+j+1] = z + a;
  }
  k++;
}
