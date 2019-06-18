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
 * HyScanMarkLocation:
 *
 * Местоположение метки, содержит в себе географические координаты метки
 *
 * @mark: указатель на метку водопада #HyScanMarkWaterfall
 * @loaded: признак того, что геолокационные данные по метке загружены
 * @time: время фиксации строки с меткой
 * @center_geo: географические координаты и курс судна в момент фиксации метки
 * @offset: расстояние от местоположения судна до метки в метрах (положительные значения по левому борту)
 */
typedef struct
{
  HyScanMarkWaterfall  *mark;
  gboolean              loaded;
  gint64                time;
  HyScanGeoGeodetic     center_geo;
  HyScanGeoGeodetic     mark_geo;
  gdouble               offset;
} HyScanMarkLocation;

HYSCAN_API
HyScanMarkLocation * hyscan_mark_location_new  (void);

HYSCAN_API
HyScanMarkLocation * hyscan_mark_location_copy (const HyScanMarkLocation *mark_location);

HYSCAN_API
void                 hyscan_mark_location_free (HyScanMarkLocation       *mark_location);

#endif //__HYSCAN_MARK_LOCATION_H__
