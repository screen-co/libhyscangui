/* hyscan-navigation-model.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
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

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
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

#define FIX_MIN_DELTA       0.1    /* Минимальное время между двумя фиксациями положения. */
#define SMOOTHING_TIME      1.5    /* Время сглаживаения ошибок экстраполяции. */
#define MAX_EXTRAPOLATE     3.0    /* Время отсутствия сигнала, после которого движение останавливается. */

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
  gdouble s0;         /* Начальное значение. */
  gdouble v0;         /* Скорость - первая производная. */
  gdouble a0;         /* Ускорение - вторая производная. */

  /* Коэффициенты функции коррекции ошибки. */
  gdouble c0;         /* Коэффициент C0. */
  gdouble c1;         /* Коэффициент C1. */
  gdouble c2;         /* Коэффициент C2. */
  gdouble c3;         /* Коэффициент C3. */
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
  gdouble            time;               /* Время фиксации (по датчику). */
} HyScanNavigationModelFix;

struct _HyScanNavigationModelPrivate
{
  HyScanSensor           *sensor;         /* Система датчиков HyScanSensor. */
  gchar                  *sensor_name;    /* Название датчика GPS-приёмника. */
  GMutex                  sensor_lock;    /* Блокировка доступа к полям sensor_. */

  guint                   interval;       /* Желаемая частота эмитирования сигналов "changed", милисекунды. */

  GTimer                 *timer;          /* Внутренний таймер. */
  gdouble                 timer_offset;   /* Разница во времени между таймером и датчиком. */

  GList                  *fixes;          /* Список последних положений объекта, зафиксированных датчиком. */
  guint                   fixes_len;      /* Количество элементов в списке. */
  guint                   fixes_max_len;  /* Сколько последних фиксов необходимо хранить в fixes. */
  GList                  *param_list;     /* Параметры модели для экстраполяции на каждом участке между фиксами. */
  GMutex                  fixes_lock;     /* Блокировка доступа к перемееным этой группы. */
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
 * На основе libhyscancore/hyscancore/hyscan-nmea-parser.c */
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
 * На основе libhyscancore/hyscancore/hyscan-nmea-parser.c */
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

  gboolean parsed = FALSE;
  gchar **words;
  HyScanNavigationModelFix fix;

  g_return_if_fail (sentence != NULL);

  words = g_strsplit (sentence, ",", -1);

  // if (g_str_equal (words[0], "$GPGGA"))
  //   hyscan_navigation_model_read_gga (model, words);
  // else
  if (g_str_equal (words[0], "$GNRMC"))
    parsed = hyscan_navigation_model_read_rmc (model, words, &fix);

  if (parsed)
    {
      GList *last_fix_l;
      HyScanNavigationModelFix *last_fix = NULL;

      g_mutex_lock (&priv->fixes_lock);

      /* Находим последний фикс. */
      last_fix_l = g_list_last (priv->fixes);
      if (last_fix_l != NULL)
        last_fix = last_fix_l->data;

      /* Фиксируем данные только если они для нового момента времени. */
      if (last_fix == NULL || fix.time > last_fix->time + FIX_MIN_DELTA)
        {
          priv->fixes = g_list_append (priv->fixes, hyscan_navigation_model_fix_copy (&fix));
          priv->fixes_len++;

          /* При поступлении первого фикса запоминаем timer_offset. */
          if (last_fix == NULL)
            priv->timer_offset = fix.time - g_timer_elapsed (priv->timer, NULL);
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
    }

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

  sentences = hyscan_nmea_data_split_sentence (msg, msg_size);
  for (i = 0; sentences[i] != NULL; i++)
    hyscan_navigation_model_read_sentence (model, sentences[i]);

  g_strfreev (sentences);
}

/* Рассчитывает значения параметров экстраполяции. */
static void
hyscan_navigation_model_update_expn_params (HyScanNavigationModelExParams *params0,
                                            HyScanNavigationModelExParams *params1,
                                            gdouble                        value,
                                            gdouble                        dt)
{
  gdouble e_s, e_v;

  params0->s0 = value;

  if (params1 == NULL)
    return;

  params0->v0 = (params0->s0 - params1->s0) / dt;
  params0->a0 = (params0->v0 - params1->v0) / dt;

  e_s = hyscan_navigation_model_extrapolate_value (params1, MIN (MAX_EXTRAPOLATE, dt), &e_v);
  e_s = e_s - params0->s0;
  e_v = e_v - params0->v0;

  params0->c0 = e_s;
  params0->c1 = e_v;
  params0->c2 = - (2 * e_v * SMOOTHING_TIME + 3 * e_s) / pow (SMOOTHING_TIME, 2);
  params0->c3 = (e_v * SMOOTHING_TIME + 2 * e_s) / pow (SMOOTHING_TIME, 3);
}

/* В момент вызова функции поток должен владеть мутексом fix_lock. */
static void
hyscan_navigation_model_update_params (HyScanNavigationModel *model)
{
  GList *fix_l;
  HyScanNavigationModelFix *fix0;
  HyScanNavigationModelPrivate *priv = model->priv;
  HyScanNavigationModelParams *params0;

  GList *list_param_l;
  HyScanNavigationModelParams *params1 = NULL;

  if (priv->fixes_len < 3)
    return;

  list_param_l = g_list_last (priv->param_list);
  if (list_param_l != NULL)
    params1 = list_param_l->data;

  fix_l = g_list_last (priv->fixes);
  fix0 = fix_l->data;

  /* Считаем производные через конечные разности. Брать большое количество предыдущих точек
   * и считать больше производныех не имеет смысла, т.к. старые данные менее актуальные. */
  params0 = g_new0 (HyScanNavigationModelParams, 1);
  params0->t0 = fix0->time;
  gdouble dt;

  dt = (params1 == NULL ? 0 : params0->t0 - params1->t0);
  hyscan_navigation_model_update_expn_params (&params0->lat_params,
                                              params1 == NULL ? NULL : &params1->lat_params,
                                              fix0->coord.lat,
                                              dt);
  hyscan_navigation_model_update_expn_params (&params0->lon_params,
                                              params1 == NULL ? NULL : &params1->lon_params,
                                              fix0->coord.lon,
                                              dt);

  priv->param_list = g_list_append (priv->param_list, params0);
}

/* Экстраполирует значение ex_params на время dt.
 * В переменную v передается скорость изменения значения. */
static gdouble
hyscan_navigation_model_extrapolate_value (HyScanNavigationModelExParams *ex_params,
                                           gdouble                        dt,
                                           gdouble                       *v)
{
  gdouble e_s, e_v;
  gdouble s;

  /* s   - экстраполирует значение по его первой и второй производной на текущем шаге;
   * e_s - сглаживает значение при переходе с последнего на текущий шаг. */
  s = ex_params->s0 + ex_params->v0 * dt + 0.5 * ex_params->a0 * dt * dt;

  if (dt > SMOOTHING_TIME)
    {
      e_s = 0;
      e_v = 0;
    }
  else
    {
      e_s = ex_params->c3 * dt * dt * dt + ex_params->c2 * dt * dt + ex_params->c1 * dt + ex_params->c0;
      e_v = 3 * ex_params->c3 * dt * dt + 2 * ex_params->c2 * dt + ex_params->c1;
    }

  /* Производная d(s(t) + e(t)) / dt. */
  *v = ex_params->v0 + ex_params->a0 * dt + e_v;

  return s + e_s;
}

/* Экстраполирует реальные данные на указанный момент времени. */
static gboolean
hyscan_navigation_model_extrapolate (HyScanNavigationModel *model,
                                     gdouble                time,
                                     HyScanGeoGeodetic     *geo)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  GList *last_params_l;
  HyScanNavigationModelExParams lat_params;
  HyScanNavigationModelExParams lon_params;
  gdouble dt;

  /* Получаем актуальные параметры модели. */
  {
    HyScanNavigationModelParams *params;

    g_mutex_lock (&priv->fixes_lock);

    last_params_l = g_list_last (priv->param_list);
    if (last_params_l == NULL)
      {
        g_mutex_unlock (&priv->fixes_lock);
        return FALSE;
      }

    params = last_params_l->data;
    lat_params = params->lat_params;
    lon_params = params->lon_params;
    dt = time - params->t0;
    g_mutex_unlock (&priv->fixes_lock);
  }

  if (dt > MAX_EXTRAPOLATE)
    return FALSE;

  /* При относительно малых расстояних (V * dt << R_{Земли}), чтобы облегчить вычисления,
   * можем использовать (lon, lat) в качестве декартовых координат (x, y). */
  gdouble v_lat, v_lon;
  geo->lat = hyscan_navigation_model_extrapolate_value (&lat_params, dt, &v_lat);
  geo->lon = hyscan_navigation_model_extrapolate_value (&lon_params, dt, &v_lon);
  geo->h = atan2 (v_lon, v_lat / cos (geo->lat / 180 * G_PI));


  return TRUE;
}

/* Эмитирует сигналы "changed" через равные промежутки времени. */
static gboolean
hyscan_navigation_model_process (HyScanNavigationModel *model)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  HyScanGeoGeodetic geo;

  gdouble time;

  time = g_timer_elapsed (priv->timer, NULL) + priv->timer_offset;
  if (hyscan_navigation_model_extrapolate (model, time, &geo))
    {
      g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0,
                     time, &geo);
    }

  return TRUE;
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

  g_timeout_add (priv->interval, (GSourceFunc) hyscan_navigation_model_process, model);
}

static void
hyscan_navigation_model_object_finalize (GObject *object)
{
  HyScanNavigationModel *model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = model->priv;

  g_signal_handlers_disconnect_by_data (priv->sensor, model);

  g_mutex_lock (&priv->fixes_lock);
  g_list_free_full (priv->fixes, (GDestroyNotify) hyscan_navigation_model_fix_free);
  g_list_free_full (priv->param_list, g_free);
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
