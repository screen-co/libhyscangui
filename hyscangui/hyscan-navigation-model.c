/* hyscan-navigation-model.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-navigation-model
 * @Short_description: модель навигационных данных
 * @Title: HyScanNavigationModel
 *
 * Класс хранит информацию о местоположении и курсе движения некоторого объекта,
 * полученную из GPS-приёмника или другого навигационного датчика.
 *
 * Используемый датчик указывается при создании объекта модели через функцию
 * hyscan_navigation_model_new().
 *
 * При изменении состояния модель эмитирует сигнал "changed" с информацией о
 * текущем местоположении и времени его фиксации.
 *
 */

#include <hyscan-buffer.h>
#include <hyscan-geo.h>
#include <stdio.h>
#include <math.h>
#include <hyscan-nmea-data.h>
#include "hyscan-navigation-model.h"
#include "hyscan-gui-marshallers.h"

#define FIX_MIN_DELTA              0.1    /* Минимальное время между двумя фиксациями положения. */
#define FIX_MAX_DELTA              1.3    /* Время между двумя фиксами, которое считается обрывом. */
#define DELAY_TIME                 1.0    /* Время задержки вывода данных. */

#define MERIDIAN_LENGTH            20003930                                     /* Длина меридиана, метры. */
#define NAUTICAL_MILE              1852                                         /* Морская миля, метры. */

#define DEG2RAD(deg) (deg / 180.0 * G_PI)
#define KNOTS2ANGLE(knots, arc)  (180.0 / (arc) * (knots) * NAUTICAL_MILE / 3600)
#define KNOTS2LAT(knots)       KNOTS2ANGLE (knots, MERIDIAN_LENGTH)
#define KNOTS2LON(knots, lat)  KNOTS2ANGLE (knots, MERIDIAN_LENGTH * cos (DEG2RAD (lat)))

enum
{
  PROP_O,
  PROP_SENSOR,
  PROP_INTERVAL
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST,
};

/* Параметры экстраполяции. */
typedef struct
{
  gdouble a;
  gdouble b;
  gdouble c;
  gdouble d;
  gdouble c_speed;
} HyScanNavigationModelExParams;

typedef struct
{
  gdouble t0;
  HyScanNavigationModelExParams lat_params;
  HyScanNavigationModelExParams lon_params;
} HyScanNavigationModelParams;

typedef struct
{
  HyScanGeoGeodetic  coord;              /* Зафиксированные географические координаты. */
  gdouble            speed;              /* Скорость движения. */
  gdouble            speed_lat;          /* Скорость движения по широте. */
  gdouble            speed_lon;          /* Скорость движения по долготе. */
  gdouble            time;               /* Время фиксации (по датчику). */
} HyScanNavigationModelFix;

struct _HyScanNavigationModelPrivate
{
  HyScanSensor                *sensor;         /* Система датчиков HyScanSensor. */
  gchar                       *sensor_name;    /* Название датчика GPS-приёмника. */
  GMutex                       sensor_lock;    /* Блокировка доступа к полям sensor_. */

  guint                        interval;       /* Желаемая частота эмитирования сигналов "changed", милисекунды. */
  guint                        process_tag;    /* ID таймера отправки сигналов "changed". */

  GTimer                      *timer;          /* Внутренний таймер. */
  gdouble                      timer_offset;   /* Разница во времени между таймером и датчиком. */
  gboolean                     timer_set;      /* Признак того, что timer_offset установлен. */

  GList                       *fixes;          /* Список последних положений объекта, зафиксированных датчиком. */
  guint                        fixes_len;      /* Количество элементов в списке. */
  guint                        fixes_max_len;  /* Сколько последних фиксов необходимо хранить в fixes. */
  HyScanNavigationModelParams  params;         /* Параметры модели для экстраполяции на последнем участке. */
  gboolean                     params_set;     /* Признак того, что параметры установлены. */
  GMutex                       fixes_lock;     /* Блокировка доступа к перемееным этой группы. */
};

static void    hyscan_navigation_model_set_property             (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);
static void    hyscan_navigation_model_object_constructed       (GObject               *object);
static void    hyscan_navigation_model_object_finalize          (GObject               *object);
static void    hyscan_navigation_model_update_params            (HyScanNavigationModel *model);
static gdouble hyscan_navigation_model_extrapolate_value        (HyScanNavigationModelExParams *ex_params,
                                                                 gdouble                        dt,
                                                                 gdouble                       *v);

static guint hyscan_navigation_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNavigationModel, hyscan_navigation_model, G_TYPE_OBJECT)

static void
hyscan_navigation_model_class_init (HyScanNavigationModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_navigation_model_set_property;

  object_class->constructed = hyscan_navigation_model_object_constructed;
  object_class->finalize = hyscan_navigation_model_object_finalize;

  g_object_class_install_property (object_class, PROP_SENSOR,
    g_param_spec_object ("sensor", "Navigation Sensor", "HyScanSensor", HYSCAN_TYPE_SENSOR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_INTERVAL,
    g_param_spec_uint ("interval", "Interval", "Interval in milliseconds", 0, G_MAXUINT, 40,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanNavigationModel::changed:
   * @model: указатель на #HyScanNavigationModel
   * @time: время фиксации нового положения, секунды
   * @coord: координаты местоположения #HyScanGeoGeodetic
   *
   * Сигнал сообщает об изменении текущего местоположения.
   */
  hyscan_navigation_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_NAVIGATION_MODEL,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__DOUBLE_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_DOUBLE, G_TYPE_POINTER);
}

static void
hyscan_navigation_model_init (HyScanNavigationModel *navigation_model)
{
  navigation_model->priv = hyscan_navigation_model_get_instance_private (navigation_model);
}

static void
hyscan_navigation_model_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanNavigationModel *navigation_model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = navigation_model->priv;

  switch (prop_id)
    {
    case PROP_SENSOR:
      priv->sensor = g_value_dup_object (value);
      break;

    case PROP_INTERVAL:
      priv->interval = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Парсит время из NMEA-строки. Получает время в секундах.
 * На основе libHyScanGui/HyScanGui/hyscan-nmea-parser.c */
static gboolean
hyscan_nmea_parser_parse_time (const gchar *sentence,
                               gdouble     *_val)
{
  gchar *end;

  gdouble hour, min;
  gdouble sec, val;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  /* Пусть на входе 094012.000 */
  hour = floor (val / 1e4);        /* 9 */
  val -= hour * 1e4;               /* 4012.000 */
  min = floor (val / 1e2);         /* 40 */
  sec = val - min * 1e2;           /* 12.000 */

  *_val = 3600.0 * hour + 60.0 * min + sec;
  return TRUE;
}

/* Парсит широту и долготу.
 * На основе libHyScanGui/HyScanGui/hyscan-nmea-parser.c */
static gboolean
hyscan_navigation_model_parse_lat_lon (const gchar *sentence,
                                       gdouble     *_val)
{
  gchar *end, side;
  gdouble val, deg, min;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  /* Переводим в градусы десятичные. */
                             /* Пусть на входе 5530.671 */
  deg = floor (val / 100.0); /* 55 */
  min = val - deg * 100;     /* 30.671 */
  min /= 60.0;               /* 30.671 -> 0.5111 */
  val = deg + min;           /* 55.511 */

  side = *(end + 1);

  /* Южное и западное полушарие со знаком минус. */
  if (side == 'S' || side == 'W')
    val *= -1;

  *_val = val;
  return TRUE;
}

/* Парсит дробное значение. */
static gboolean
hyscan_navigation_model_parse_value (const gchar *sentence,
                                     gdouble     *_val)
{
  gchar *end;
  gdouble val;

  val = g_ascii_strtod (sentence, &end);

  if (val == 0 && end == sentence)
    return FALSE;

  *_val = val;
  return TRUE;
}

/* Копирует структуру HyScanNavigationModelFix. */
static HyScanNavigationModelFix *
hyscan_navigation_model_fix_copy (HyScanNavigationModelFix *fix)
{
  HyScanNavigationModelFix *copy;

  copy = g_slice_new (HyScanNavigationModelFix);
  copy->coord.lon = fix->coord.lon;
  copy->coord.lat = fix->coord.lat;
  copy->coord.h = fix->coord.h;
  copy->speed = fix->speed;
  copy->speed_lon = fix->speed_lon;
  copy->speed_lat = fix->speed_lat;
  copy->time = fix->time;

  return copy;
}

/* Освобождает память, занятую структурой HyScanNavigationModelFix. */
static void
hyscan_navigation_model_fix_free (HyScanNavigationModelFix *fix)
{
  g_slice_free (HyScanNavigationModelFix, fix);
}

/* Парсит GGA-строку. */
static gboolean
hyscan_navigation_model_read_gga (HyScanNavigationModel      *model,
                                  gchar                     **words,
                                  HyScanNavigationModelFix   *fix)
{

  if (g_strv_length (words) != 15)
    return FALSE;

  /* Время. */
  if (!hyscan_nmea_parser_parse_time (words[1], &fix->time))
    return FALSE;

  /* Широта. */
  if (!hyscan_navigation_model_parse_lat_lon (words[2], &fix->coord.lat))
    return FALSE;

  /* Долгота. */
  if (!hyscan_navigation_model_parse_lat_lon (words[4], &fix->coord.lon))
    return FALSE;

  return TRUE;
}

/* Парсит RMC-строку. */
static gboolean
hyscan_navigation_model_read_rmc (HyScanNavigationModel      *model,
                                  gchar                     **words,
                                  HyScanNavigationModelFix   *fix)
{
  if (g_strv_length (words) != 13)
    return FALSE;

  if (!hyscan_nmea_parser_parse_time (words[1], &fix->time))
    return FALSE;

  if (!hyscan_navigation_model_parse_lat_lon (words[3], &fix->coord.lat))
    return FALSE;

  if (!hyscan_navigation_model_parse_lat_lon (words[5], &fix->coord.lon))
    return FALSE;

  if (!hyscan_navigation_model_parse_value (words[7], &fix->speed))
    return FALSE;

  if (!hyscan_navigation_model_parse_value (words[8], &fix->coord.h))
    return FALSE;

  return TRUE;
}

/* Парсит NMEA-строку. */
static void
hyscan_navigation_model_read_sentence (HyScanNavigationModel *model,
                                       const gchar           *sentence)
{
  HyScanNavigationModelPrivate *priv = model->priv;

  gboolean parsed;
  gchar **words;
  HyScanNavigationModelFix fix;

  GList *last_fix_l;
  HyScanNavigationModelFix *last_fix;
  HyScanNmeaDataType nmea_type;

  g_return_if_fail (sentence != NULL);

  words = g_strsplit (sentence, ",", -1);

  // if (g_str_equal (words[0], "$GPGGA"))
  //   hyscan_navigation_model_read_gga (model, words);
  // else
  nmea_type = hyscan_nmea_data_check_sentence (sentence);
  if (nmea_type == HYSCAN_NMEA_DATA_RMC)
    parsed = hyscan_navigation_model_read_rmc (model, words, &fix);
  else if (nmea_type == HYSCAN_NMEA_DATA_INVALID)
    {
      g_message ("Invalid sentence: %s", sentence);
      parsed = FALSE;
    }
  else
    parsed = FALSE;


  if (!parsed)
    goto exit;

  // GRand *rand;
  // rand = g_rand_new ();
  // if (g_rand_int_range (rand, 0, 10) < 5)
  //   {
  //     fix.coord.lon = 0;
  //     fix.coord.lat = 0;
  //   }
  // g_rand_free (rand);

  /* Расчитываем скорости по широте и долготе. */
  if (fix.speed > 0)
    {
      gdouble bearing = DEG2RAD (fix.coord.h);

      fix.speed_lat = KNOTS2LAT (fix.speed * cos (bearing));
      fix.speed_lon = KNOTS2LON (fix.speed * sin (bearing), fix.coord.lat);
    }
  else
    {
      fix.speed_lat = 0;
      fix.speed_lon = 0;
    }

  g_mutex_lock (&priv->fixes_lock);

  /* Находим последний фикс. */
  last_fix_l = g_list_last (priv->fixes);
  if (last_fix_l != NULL)
    last_fix = last_fix_l->data;
  else
    last_fix = NULL;

  /* Обрыв: удаляем из списка старые данные. */
  if (last_fix != NULL && fix.time - last_fix->time > FIX_MAX_DELTA)
    {
      g_list_free_full (priv->fixes, (GDestroyNotify) hyscan_navigation_model_fix_free);
      priv->fixes_len = 0;
      priv->fixes = NULL;
      priv->params_set = FALSE;
      last_fix = NULL;
    }

  /* Фиксируем данные только если они для нового момента времени. */
  if (last_fix == NULL || fix.time - last_fix->time > FIX_MIN_DELTA)
    {
      priv->fixes = g_list_append (priv->fixes, hyscan_navigation_model_fix_copy (&fix));
      priv->fixes_len++;

      /* При поступлении первого фикса запоминаем timer_offset. */
      if (!priv->timer_set) {
        priv->timer_offset = fix.time - g_timer_elapsed (priv->timer, NULL) - DELAY_TIME;
        priv->timer_set = TRUE;
      }
    }

  /* Удаляем из списка старые данные. */
  if (priv->fixes_len > priv->fixes_max_len)
    {
      GList *first_fix_l = priv->fixes;

      priv->fixes = g_list_remove_link (priv->fixes, first_fix_l);
      priv->fixes_len--;

      hyscan_navigation_model_fix_free (first_fix_l->data);
      g_list_free (first_fix_l);
    }

  hyscan_navigation_model_update_params (model);
  g_mutex_unlock (&priv->fixes_lock);

exit:
  g_strfreev (words);
}

/* Обработчик сигнала "sensor-data".
 * Парсит полученное от датчика @sensor сообщение. Может выполняться не в MainLoop. */
static void
hyscan_navigation_model_sensor_data (HyScanSensor          *sensor,
                                     const gchar           *name,
                                     HyScanSourceType       source,
                                     gint64                 time,
                                     HyScanBuffer          *data,
                                     HyScanNavigationModel *model)
{
  HyScanNavigationModelPrivate *priv = model->priv;

  const gchar *msg;
  guint32 msg_size;
  gchar **sentences;
  gint i;

  gboolean is_target_sensor;

  g_mutex_lock (&priv->sensor_lock);
  is_target_sensor = (g_strcmp0 (name, priv->sensor_name) == 0);
  g_mutex_unlock (&priv->sensor_lock);

  if (!is_target_sensor)
    return;

  msg = hyscan_buffer_get_data (data, &msg_size);

  /* Имитируем задержку.  */
  // GRand *rand;
  // gint delay;
  // rand = g_rand_new ();
  // delay = g_rand_int_range (rand, 0, G_USEC_PER_SEC);
  // g_message ("Delay %.2f", (gdouble) delay / G_USEC_PER_SEC);
  // g_usleep (delay);

  sentences = hyscan_nmea_data_split_sentence (msg, msg_size);
  for (i = 0; sentences[i] != NULL; i++)
    hyscan_navigation_model_read_sentence (model, sentences[i]);

  g_strfreev (sentences);
}

/* Рассчитывает значения параметров экстраполяции. */
static void
hyscan_navigation_model_update_expn_params (HyScanNavigationModelExParams *params0,
                                            gdouble                        value0,
                                            gdouble                        d_value0,
                                            gdouble                        value_next,
                                            gdouble                        d_value_next,
                                            gdouble                        dt)
{
  params0->a = value0;
  params0->b = d_value0;
  params0->d = dt * dt * (d_value0 + d_value_next) - 2 * dt * (value_next - value0);
  params0->c = 1 / (dt * dt) * (value_next - value0 - d_value0 * dt) - params0->d * dt;

  params0->c_speed = (d_value_next - params0->b) / (2 * dt);
}

/* Экстраполирует значение ex_params на время dt.
 * В переменную v передается скорость изменения значения. */
static gdouble
hyscan_navigation_model_extrapolate_value (HyScanNavigationModelExParams *ex_params,
                                           gdouble                        dt,
                                           gdouble                       *v)
{
  gdouble s;

  s = ex_params->a + ex_params->b * dt + ex_params->c * pow (dt, 2) + ex_params->d * pow (dt, 3);

  if (v != NULL)
    *v = ex_params->b + 2 * ex_params->c * dt + 3 * ex_params->d * pow (dt, 2);
    // *v = ex_params->b + 2 * ex_params->c_speed * dt;

  return s;
}

/* Экстраполирует реальные данные на указанный момент времени. */
static gboolean
hyscan_navigation_model_extrapolate (HyScanNavigationModel *model,
                                     gdouble                time,
                                     HyScanGeoGeodetic     *geo,
                                     gdouble               *time_delta)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  HyScanNavigationModelExParams lat_params;
  HyScanNavigationModelExParams lon_params;

  gdouble v_lat, v_lon;
  gdouble dt;
  gdouble time_end;

  /* Получаем актуальные параметры модели. */
  {
    GList *last_fix_l;
    HyScanNavigationModelFix *last_fix;

    g_mutex_lock (&priv->fixes_lock);

    last_fix_l = g_list_last (priv->fixes);
    if (!priv->params_set || last_fix_l == NULL)
      {
        g_mutex_unlock (&priv->fixes_lock);
        *time_delta = 0;
        return FALSE;
      }

    lat_params = priv->params.lat_params;
    lon_params = priv->params.lon_params;
    dt = time - priv->params.t0;

    last_fix = last_fix_l->data;
    time_end = last_fix->time;

    g_mutex_unlock (&priv->fixes_lock);
  }

  *time_delta = dt;

  if (time > time_end)
    return FALSE;

  /* При относительно малых расстояних (V * dt << R_{Земли}), чтобы облегчить вычисления,
   * можем использовать (lon, lat) в качестве декартовых координат (x, y). */
  geo->lat = hyscan_navigation_model_extrapolate_value (&lat_params, dt, &v_lat);
  geo->lon = hyscan_navigation_model_extrapolate_value (&lon_params, dt, &v_lon);
  geo->h = atan2 (v_lon, v_lat / cos (DEG2RAD (geo->lat)));


  return TRUE;
}

/* Эмитирует сигналы "changed" через равные промежутки времени. */
static gboolean
hyscan_navigation_model_process (HyScanNavigationModel *model)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  HyScanGeoGeodetic geo;

  gdouble time;
  gdouble time_delta;

  gboolean extrapolated;

  time = g_timer_elapsed (priv->timer, NULL) + priv->timer_offset;
  extrapolated = hyscan_navigation_model_extrapolate (model, time, &geo, &time_delta);
  if (extrapolated)
    {
      g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0,
                     time, &geo);
    }
  else if (time_delta > FIX_MAX_DELTA)
    {
      g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0,
                     time, NULL);
    }


  return TRUE;
}

/* Вызывать за мьютексом fix_lock! */
static void
hyscan_navigation_model_update_params (HyScanNavigationModel *model)
{
  GList *fix_l;
  HyScanNavigationModelFix *fix_next, *fix0;
  HyScanNavigationModelPrivate *priv = model->priv;

  gdouble dt;

  if (priv->fixes_len < 2)
    return;

  fix_l = g_list_last (priv->fixes);
  fix_next = fix_l->data;
  fix0 = fix_l->prev->data;

  /* Считаем производные через конечные разности. Брать большое количество предыдущих точек
   * и считать больше производных не имеет смысла, т.к. старые данные менее актуальные. */
  priv->params.t0 = fix0->time;

  dt = fix_next->time - fix0->time;
  hyscan_navigation_model_update_expn_params (&priv->params.lat_params,
                                              fix0->coord.lat,
                                              fix0->speed_lat,
                                              fix_next->coord.lat,
                                              fix_next->speed_lat,
                                              dt);
  hyscan_navigation_model_update_expn_params (&priv->params.lon_params,
                                              fix0->coord.lon,
                                              fix0->speed_lon,
                                              fix_next->coord.lon,
                                              fix_next->speed_lon,
                                              dt);
  priv->params_set = TRUE;
}

static void
hyscan_navigation_model_object_constructed (GObject *object)
{
  HyScanNavigationModel *model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = model->priv;

  G_OBJECT_CLASS (hyscan_navigation_model_parent_class)->constructed (object);

  g_mutex_init (&priv->sensor_lock);
  g_mutex_init (&priv->fixes_lock);
  priv->timer = g_timer_new ();
  priv->fixes_max_len = 10;

  priv->process_tag = g_timeout_add (priv->interval, (GSourceFunc) hyscan_navigation_model_process, model);
}

static void
hyscan_navigation_model_object_finalize (GObject *object)
{
  HyScanNavigationModel *model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = model->priv;

  g_signal_handlers_disconnect_by_data (priv->sensor, model);

  g_source_remove (priv->process_tag);

  g_mutex_lock (&priv->fixes_lock);
  g_list_free_full (priv->fixes, (GDestroyNotify) hyscan_navigation_model_fix_free);
  g_mutex_unlock (&priv->fixes_lock);
  g_mutex_clear (&priv->fixes_lock);

  g_mutex_lock (&priv->sensor_lock);
  g_clear_object (&priv->sensor);
  g_free (priv->sensor_name);
  g_mutex_unlock (&priv->sensor_lock);
  g_mutex_clear (&priv->sensor_lock);

  g_timer_destroy (priv->timer);

  G_OBJECT_CLASS (hyscan_navigation_model_parent_class)->finalize (object);
}

/**
 * hyscan_navigation_model_new:
 *
 * Создает модель навигационных данных, которая обрабатывает NMEA-строки
 * из GPS-датчика. Установить целевой датчик можно функциями
 * hyscan_navigation_model_set_sensor() и hyscan_navigation_model_set_sensor_name().
 *
 * Returns: указатель на #HyScanNavigationModel. Для удаления g_object_unref()
 */
HyScanNavigationModel *
hyscan_navigation_model_new ()
{
  return g_object_new (HYSCAN_TYPE_NAVIGATION_MODEL, NULL);
}

/**
 * hyscan_navigation_model_set_sensor:
 * @model: указатель на #HyScanNavigationModel
 * @sensor: указатель на #HyScanSensor
 *
 * Устанавливает используемую систему датчиков #HyScanSensor. Чтобы установить
 * имя датчика используйте hyscan_navigation_model_set_sensor_name().
 */
void
hyscan_navigation_model_set_sensor (HyScanNavigationModel *model,
                                    HyScanSensor          *sensor)
{
  HyScanNavigationModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_NAVIGATION_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->sensor_lock);
  if (priv->sensor != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->sensor, hyscan_navigation_model_sensor_data, model);
      g_object_unref (priv->sensor);
    }

  priv->sensor = g_object_ref (sensor);
  g_signal_connect (priv->sensor, "sensor-data", G_CALLBACK (hyscan_navigation_model_sensor_data), model);
  g_mutex_unlock (&priv->sensor_lock);
}

/**
 * hyscan_navigation_model_set_sensor_name:
 * @model: указатель на #HyScanNavigationModel
 * @name: имя датчика
 *
 * Устанавливает имя используемого датчика текущей системы датчиков. Чтобы
 * установить другую систему используйте hyscan_navigation_model_set_sensor().
 */
void
hyscan_navigation_model_set_sensor_name (HyScanNavigationModel *model,
                                         const gchar           *name)
{
  HyScanNavigationModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_NAVIGATION_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->sensor_lock);
  g_free (priv->sensor_name);
  priv->sensor_name = g_strdup (name);
  g_mutex_unlock (&priv->sensor_lock);
}
