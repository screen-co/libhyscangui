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

enum
{
  PROP_O,
  PROP_SENSOR
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST,
};

struct _HyScanNavigationModelPrivate
{
  HyScanSensor           *sensor;         /* Датчик навигационных данных (GPS-приёмник). */
};

static void    hyscan_navigation_model_set_property             (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);
static void    hyscan_navigation_model_object_constructed       (GObject               *object);
static void    hyscan_navigation_model_object_finalize          (GObject               *object);

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Парсит время из NMEA-строки.
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

/* Парсит GGA-строку. */
static void
hyscan_navigation_model_read_gga (HyScanNavigationModel  *model,
                                  gchar                 **words)
{
  HyScanGeoGeodetic geo;
  gdouble time;

  if (g_strv_length (words) != 15)
    return;

  /* Время. */
  if (!hyscan_nmea_parser_parse_time (words[1], &time))
    return;

  /* Широта. */
  if (!hyscan_navigation_model_parse_lat_lon (words[2], &geo.lat))
    return;

  /* Долгота. */
  if (!hyscan_navigation_model_parse_lat_lon (words[4], &geo.lon))
    return;

  g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0, time, &geo);
}

/* Парсит RMC-строку. */
static void
hyscan_navigation_model_read_rmc (HyScanNavigationModel  *model,
                                  gchar                 **words)
{
  HyScanGeoGeodetic geo;
  gdouble time;

  if (g_strv_length (words) != 13)
    return;

  if (!hyscan_nmea_parser_parse_time (words[1], &time))
    return;

  if (!hyscan_navigation_model_parse_lat_lon (words[3], &geo.lat))
    return;

  if (!hyscan_navigation_model_parse_lat_lon (words[5], &geo.lon))
    return;

  g_signal_emit (model, hyscan_navigation_model_signals[SIGNAL_CHANGED], 0, time, &geo);
}

/* Парсит NMEA-строку. */
static void
hyscan_navigation_model_read_sentence (HyScanNavigationModel *model,
                                       const gchar           *sentence)
{
  gchar **words;

  g_return_if_fail (sentence != NULL);

  words = g_strsplit (sentence, ",", -1);

  if (g_str_equal (words[0], "$GPGGA"))
    hyscan_navigation_model_read_gga (model, words);
  else if (g_str_equal (words[0], "$GNRMC"))
    hyscan_navigation_model_read_rmc (model, words);

  g_strfreev (words);
}

/* Обработчик сигнала "sensor-data".
 * Парсит полученное от датчика @sensor сообщение. */
static void
hyscan_navigation_model_sensor_data (HyScanSensor          *sensor,
                                     const gchar           *name,
                                     HyScanSourceType       source,
                                     gint64                 time,
                                     HyScanBuffer          *data,
                                     HyScanNavigationModel *model)
{
  const gchar *msg;
  guint32 msg_size;
  gchar **sentences;
  gint i;

  msg = hyscan_buffer_get_data (data, &msg_size);

  sentences = hyscan_nmea_data_split_sentence (msg, msg_size);
  for (i = 0; sentences[i] != NULL; i++)
    hyscan_navigation_model_read_sentence (model, sentences[i]);

  g_strfreev (sentences);
}

static void
hyscan_navigation_model_object_constructed (GObject *object)
{
  HyScanNavigationModel *model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = model->priv;

  G_OBJECT_CLASS (hyscan_navigation_model_parent_class)->constructed (object);

  g_signal_connect (priv->sensor, "sensor-data", G_CALLBACK (hyscan_navigation_model_sensor_data), model);
}

static void
hyscan_navigation_model_object_finalize (GObject *object)
{
  HyScanNavigationModel *model = HYSCAN_NAVIGATION_MODEL (object);
  HyScanNavigationModelPrivate *priv = model->priv;

  g_signal_handlers_disconnect_by_data (priv->sensor, model);
  g_clear_object (&priv->sensor);

  G_OBJECT_CLASS (hyscan_navigation_model_parent_class)->finalize (object);
}

/**
 * hyscan_navigation_model_new:
 * @sensor: указатель на #HyScanSensor
 *
 * Создает модель навигационных данных, которая обрабатывает NMEA-строки
 * датчика @sensor.
 *
 * Returns: указатель на #HyScanNavigationModel. Для удаления g_object_unref()
 */
HyScanNavigationModel *
hyscan_navigation_model_new (HyScanSensor *sensor)
{
  return g_object_new (HYSCAN_TYPE_NAVIGATION_MODEL,
                       "sensor", sensor, NULL);
}
