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
#include <hyscan-nmea-parser.h>
#include "hyscan-navigation-model.h"
#include "hyscan-gui-marshallers.h"

#define FIX_MIN_DELTA              0.01      /* Минимальное время между двумя фиксациями положения. */
#define FIX_MAX_DELTA              1.3       /* Время между двумя фиксами, которое считается обрывом. */
#define DELAY_TIME                 1.0       /* Время задержки вывода данных по умолчанию. */
#define FIXES_N                    10        /* Количество последних хранимых фиксов. */

#define MERIDIAN_LENGTH            20003930  /* Длина меридиана, метры. */
#define NAUTICAL_MILE              1852      /* Морская миля, метры. */

#define DEG2RAD(deg)             (deg / 180.0 * G_PI)
#define KNOTS2METER(knots)       ((knots) * NAUTICAL_MILE / 3600)
#define KNOTS2ANGLE(knots, arc)  (180.0 / (arc) * (knots) * NAUTICAL_MILE / 3600)
#define KNOTS2LAT(knots)         KNOTS2ANGLE (knots, MERIDIAN_LENGTH)
#define KNOTS2LON(knots, lat)    KNOTS2ANGLE (knots, MERIDIAN_LENGTH * cos (DEG2RAD (lat)))

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
  gboolean           true_heading;
  gdouble            heading;            /* Направление носа судна. */
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

  HyScanNMEAParser             *parser_time;   /* Парсер времени. */
  HyScanNMEAParser             *parser_date;   /* Парсер даты. */
  HyScanNMEAParser             *parser_lat;    /* Парсер широты. */
  HyScanNMEAParser             *parser_lon;    /* Парсер долготы. */
  HyScanNMEAParser             *parser_track;  /* Парсер курса. */
  HyScanNMEAParser             *parser_heading;/* Парсер истинного курса. */
  HyScanNMEAParser             *parser_speed;  /* Парсер скорости. */

  guint                        interval;       /* Желаемая частота эмитирования сигналов "changed", милисекунды. */
  guint                        process_tag;    /* ID таймера отправки сигналов "changed". */

  GTimer                      *timer;          /* Внутренний таймер. */
  gdouble                      delay_time;     /* Время задержки вывода данных. */
  gdouble                      timer_offset;   /* Разница во времени между таймером и датчиком. */
  gboolean                     timer_set;      /* Признак того, что timer_offset установлен. */

  GList                       *fixes;          /* Список последних положений объекта, зафиксированных датчиком. */
  guint                        fixes_len;      /* Количество элементов в списке. */
  HyScanNavigationModelParams  params;         /* Параметры модели для экстраполяции на последнем участке. */
  gboolean                     params_set;     /* Признак того, что параметры установлены. */
  GMutex                       fixes_lock;     /* Блокировка доступа к перемееным этой группы. */
  gboolean                     extrapolate;    /* Стратегия получения актальных данных (экстраполировать или нет). */
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
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_POINTER);
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

/* Копирует структуру HyScanNavigationModelFix. */
static HyScanNavigationModelFix *
hyscan_navigation_model_fix_copy (HyScanNavigationModelFix *fix)
{
  HyScanNavigationModelFix *copy;

  copy = g_slice_new (HyScanNavigationModelFix);
  *copy = *fix;

  return copy;
}

/* Освобождает память, занятую структурой HyScanNavigationModelFix. */
static void
hyscan_navigation_model_fix_free (HyScanNavigationModelFix *fix)
{
  g_slice_free (HyScanNavigationModelFix, fix);
}

static void
hyscan_navigation_model_remove_all (HyScanNavigationModel *model)
{
  HyScanNavigationModelPrivate *priv = model->priv;

  g_list_free_full (priv->fixes, (GDestroyNotify) hyscan_navigation_model_fix_free);
  priv->fixes_len = 0;
  priv->fixes = NULL;
  priv->params_set = FALSE;
}

/* Добавляет новый фикс в список. */
static void
hyscan_navigation_model_add_fix (HyScanNavigationModel    *model,
                                 HyScanNavigationModelFix *fix)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  GList *last_fix_l;
  HyScanNavigationModelFix *last_fix;

  g_mutex_lock (&priv->fixes_lock);

  /* Находим последний фикс. */
  last_fix_l = g_list_last (priv->fixes);
  if (last_fix_l != NULL)
    last_fix = last_fix_l->data;
  else
    last_fix = NULL;

  /* Обрыв: удаляем из списка старые данные. */
  if (last_fix != NULL && fix->time - last_fix->time > FIX_MAX_DELTA)
    {
      hyscan_navigation_model_remove_all (model);
      last_fix = NULL;
    }

  /* Фиксируем данные только если они для нового момента времени. */
  if (last_fix == NULL || fix->time - last_fix->time > FIX_MIN_DELTA)
    {
      priv->fixes = g_list_append (priv->fixes, hyscan_navigation_model_fix_copy (fix));
      priv->fixes_len++;

      /* Если timer_offset не инициализирован, делаем это. */
      if (g_atomic_int_compare_and_exchange (&priv->timer_set, FALSE, TRUE)) {
        priv->timer_offset = fix->time - g_timer_elapsed (priv->timer, NULL) - priv->delay_time;
      }
    }

  /* Удаляем из списка старые данные. */
  if (priv->fixes_len >FIXES_N)
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

/* Парсит NMEA-строку. */
static gboolean
hyscan_navigation_model_read_sentence (HyScanNavigationModel    *model,
                                       const gchar              *sentence,
                                       HyScanNavigationModelFix *fix)
{
  HyScanNavigationModelPrivate *priv = model->priv;

  gboolean parsed;
  gdouble fix_time, fix_date;

  g_return_val_if_fail (sentence != NULL, FALSE);

  parsed  = hyscan_nmea_parser_parse_string (priv->parser_time, sentence, &fix_time);
  parsed &= hyscan_nmea_parser_parse_string (priv->parser_date, sentence, &fix_date);
  parsed &= hyscan_nmea_parser_parse_string (priv->parser_lat, sentence, &fix->coord.lat);
  parsed &= hyscan_nmea_parser_parse_string (priv->parser_lon, sentence, &fix->coord.lon);
  parsed &= hyscan_nmea_parser_parse_string (priv->parser_track, sentence, &fix->coord.h);
  parsed &= hyscan_nmea_parser_parse_string (priv->parser_speed, sentence, &fix->speed);

  if (!parsed)
    return FALSE;

  fix->time = fix_date + fix_time;

  /* Расчитываем скорости по широте и долготе. */
  if (fix->speed > 0)
    {
      gdouble bearing = DEG2RAD (fix->coord.h);

      fix->speed_lat = KNOTS2LAT (fix->speed * cos (bearing));
      fix->speed_lon = KNOTS2LON (fix->speed * sin (bearing), fix->coord.lat);
    }
  else
    {
      fix->speed_lat = 0;
      fix->speed_lon = 0;
    }

  return TRUE;
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

  msg = hyscan_buffer_get (data, NULL, &msg_size);

  sentences = hyscan_nmea_data_split_sentence (msg, msg_size);
  for (i = 0; sentences[i] != NULL; i++)
    {
      HyScanNavigationModelFix fix;

      if (hyscan_navigation_model_read_sentence (model, sentences[i], &fix))
        {
          fix.heading = fix.coord.h;
          fix.true_heading = FALSE;

          /* Пробуем считать истинный курс. */
          while (sentences[i + 1] != NULL &&
                 !hyscan_nmea_parser_parse_string (priv->parser_time, sentences[i + 1], NULL))
          {
            if (hyscan_nmea_parser_parse_string (priv->parser_heading, sentences[++i], &fix.heading))
              fix.true_heading = TRUE;
          }

          hyscan_navigation_model_add_fix (model, &fix);
        }
    }

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

/* Возвращает последние полученные данные. */
static gboolean
hyscan_navigation_model_latest (HyScanNavigationModel     *model,
                                HyScanNavigationModelData *data,
                                gdouble                   *time_delta)
{
  HyScanNavigationModelPrivate *priv = model->priv;
  GList *last_fix_l;
  HyScanNavigationModelFix *last_fix;

  g_mutex_lock (&priv->fixes_lock);

  last_fix_l = g_list_last (priv->fixes);

  if (last_fix_l == NULL)
    {
      g_mutex_unlock (&priv->fixes_lock);
      return FALSE;
    }

  last_fix = last_fix_l->data;

  data->coord = last_fix->coord;
  data->coord.h = DEG2RAD (data->coord.h);
  data->heading = DEG2RAD (last_fix->heading);
  data->true_heading = last_fix->true_heading;
  data->speed = KNOTS2METER (last_fix->speed);
  *time_delta = data->time - last_fix->time;

  g_mutex_unlock (&priv->fixes_lock);

  if (*time_delta > FIX_MAX_DELTA)
    return FALSE;

  return TRUE;
}

/* Экстраполирует реальные данные на указанный момент времени. */
static gboolean
hyscan_navigation_model_extrapolate (HyScanNavigationModel     *model,
                                     HyScanNavigationModelData *data,
                                     gdouble                   *time_delta)
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
    dt = data->time - priv->params.t0;

    last_fix = last_fix_l->data;
    time_end = last_fix->time;
    data->heading = DEG2RAD (last_fix->heading);
    data->true_heading = last_fix->true_heading;
    data->speed = KNOTS2METER (last_fix->speed);

    g_mutex_unlock (&priv->fixes_lock);
  }

  *time_delta = dt;

  if (data->time > time_end)
    return FALSE;

  /* При относительно малых расстояних (V * dt << R_{Земли}), чтобы облегчить вычисления,
   * можем использовать (lon, lat) в качестве декартовых координат (x, y). */
  data->coord.lat = hyscan_navigation_model_extrapolate_value (&lat_params, dt, &v_lat);
  data->coord.lon = hyscan_navigation_model_extrapolate_value (&lon_params, dt, &v_lon);
  data->coord.h = atan2 (v_lon, v_lat / cos (DEG2RAD (data->coord.lat)));


  return TRUE;
}

/* Эмитирует сигналы "changed" через равные промежутки времени. */
static gboolean
hyscan_navigation_model_process (HyScanNavigationModel *model)
{
  HyScanNavigationModelData data;

  gdouble time_delta;

  hyscan_navigation_model_get (model, &data, &time_delta);
  if (data.loaded || time_delta > FIX_MAX_DELTA)
    g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0, &data);

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
  hyscan_navigation_model_set_delay (model, DELAY_TIME);

  priv->parser_time = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TIME);
  priv->parser_date = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_DATE);
  priv->parser_lat = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT);
  priv->parser_lon = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON);
  priv->parser_track = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK);
  priv->parser_heading = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_HDT, HYSCAN_NMEA_FIELD_HEADING);
  priv->parser_speed = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_SPEED);

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

  g_object_unref (priv->parser_time);
  g_object_unref (priv->parser_date);
  g_object_unref (priv->parser_lat);
  g_object_unref (priv->parser_lon);
  g_object_unref (priv->parser_track);
  g_object_unref (priv->parser_heading);
  g_object_unref (priv->parser_speed);

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

/**
 * hyscan_navigation_model_set_delay:
 * @model: указатель на #HyScanNavigationModel
 * @delay: время задержки выдачи данных, секунды
 *
 * Устанавливает время задержки @delay между получением данных от датчика и выдачей
 * этих данных пользователю класса. Задержка необходима для сглаживания данных
 * (интерполяции) между соседними фиксами.
 *
 * Интерполяция работает корректно, только если время задержки не больше интервала
 * выдачи сигнала датчиком. Например, для датчика с частотой 1 Гц следует установить
 * @delay = 1.0.
 *
 * Если сигнал "changed" испускается реже или не намного чаще получения данных от
 * GPS-приёмника (приёмник с большой частотой), то можно установить @delay = 0.0.
 * В этом случае пользователь получит точные данные, а сглаживание будет отключено.
 */
void
hyscan_navigation_model_set_delay (HyScanNavigationModel *model,
                                   gdouble                delay)
{
  HyScanNavigationModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_NAVIGATION_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->fixes_lock);

  priv->delay_time = delay;
  priv->extrapolate = (priv->delay_time > 0);

  g_atomic_int_set (&priv->timer_set, FALSE);
  hyscan_navigation_model_remove_all (model);

  g_mutex_unlock (&priv->fixes_lock);
}

/**
 * hyscan_navigation_model_get:
 * @model: указатель на #HyScanNavigationModel
 * @data: (out): данные модели
 * @time_delta: (out): (nullable): возраст в секундах данных @data.
 *
 * Записывает текущие данные модели в @data. Возраст @time_delta показывает
 * время в секундах, прошедшее с того момента, когда данные @data были
 * актуальными.
 *
 * Returns: %TRUE, если данные получены успешно.
 */
gboolean
hyscan_navigation_model_get (HyScanNavigationModel     *model,
                             HyScanNavigationModelData *data,
                             gdouble                   *time_delta)
{
  HyScanNavigationModelPrivate *priv;
  gdouble time_delta_ret;

  g_return_val_if_fail (HYSCAN_IS_NAVIGATION_MODEL (model), FALSE);
  priv = model->priv;

  data->time = g_timer_elapsed (priv->timer, NULL) + priv->timer_offset;
  if (priv->extrapolate)
    data->loaded = hyscan_navigation_model_extrapolate (model, data, &time_delta_ret);
  else
    data->loaded = hyscan_navigation_model_latest (model, data, &time_delta_ret);

  if (time_delta != NULL)
    *time_delta = time_delta_ret;

  return data->loaded;
}
