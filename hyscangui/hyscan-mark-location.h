/* hyscan-mark-location.h
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
#ifndef __HYSCAN_MARK_LOCATION_H__
#define __HYSCAN_MARK_LOCATION_H__


#include <hyscan-mark.h>
#include <hyscan-geo.h>

/**
 * HyScanMarkLocationDirection:
 * @HYSCAN_MARK_LOCATION_STARBOARD: правый борт
 * @HYSCAN_MARK_LOCATION_BOTTOM: под собой
 * @HYSCAN_MARK_LOCATION_PORT: левый борт
 *
 * Направление излучения передающей антенны, с которой сделана метка
 */
typedef enum
{
  HYSCAN_MARK_LOCATION_STARBOARD = -1,
  HYSCAN_MARK_LOCATION_BOTTOM = 0,
  HYSCAN_MARK_LOCATION_PORT = 1,
} HyScanMarkLocationDirection;

/**
 * HyScanMarkLocation:
 * @mark: указатель на метку водопада #HyScanMarkWaterfall
 * @loaded: признак того, что геолокационные данные по метке загружены
 * @track_name: имя галса
 * @time: время фиксации строки с меткой
 * @center_geo: географические координаты и курс антенны в момент фиксации метки
 * @mark_geo: географические координаты центра метки
 * @offset: горизонтальное расстояние от антенны до метки в метрах (положительные значения по левому борту)
 * @direction: направление излучения передающей антенны
 *
 * Местоположение метки, содержит в себе географические координаты метки.
 */
typedef struct
{
  HyScanMarkWaterfall          *mark;
  gboolean                      loaded;
  gchar                        *track_name;
  gint64                        time;
  HyScanGeoGeodetic             center_geo;
  HyScanGeoGeodetic             mark_geo;
  gdouble                       offset;
  HyScanMarkLocationDirection   direction;
} HyScanMarkLocation;

HYSCAN_API
HyScanMarkLocation * hyscan_mark_location_new  (void);

HYSCAN_API
HyScanMarkLocation * hyscan_mark_location_copy (const HyScanMarkLocation *mark_location);

HYSCAN_API
void                 hyscan_mark_location_free (HyScanMarkLocation       *mark_location);

#endif //__HYSCAN_MARK_LOCATION_H__
