/* hyscan-mark-location.c
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

#include "hyscan-mark-location.h"

/**
 * hyscan_mark_location_new:
 *
 * Создаёт структуру с информацией о положении метки.
 *
 * Returns: указатель на #HyScanMarkLocation. Для удаления hyscan_mark_location_free().
 */
HyScanMarkLocation *
hyscan_mark_location_new (void)
{
  return g_slice_new (HyScanMarkLocation);
}

/**
 * hyscan_mark_location_copy:
 * @mark_location: указатель на #HyScanMarkLocation
 *
 * Создаёт копию структуры @mark_location.
 *
 * Returns: указатель на #HyScanMarkLocation. Для удаления hyscan_mark_location_free().
 */
HyScanMarkLocation *
hyscan_mark_location_copy (const HyScanMarkLocation *mark_location)
{
  HyScanMarkLocation *copy;

  copy = hyscan_mark_location_new ();

  copy->mark = (HyScanMarkWaterfall *) hyscan_mark_copy ((HyScanMark *) mark_location->mark);
  copy->loaded = mark_location->loaded;
  copy->time = mark_location->time;
  copy->center_geo = mark_location->center_geo;
  copy->mark_geo = mark_location->mark_geo;
  copy->offset = mark_location->offset;

  return copy;
}

/**
 * hyscan_mark_location_free:
 * @mark_location: указатель на #HyScanMarkLocation
 *
 * Освобождает память, занятую структурой #HyScanMarkLocation
 */
void
hyscan_mark_location_free (HyScanMarkLocation *mark_location)
{
  hyscan_mark_free ((HyScanMark *) mark_location->mark);
  g_slice_free (HyScanMarkLocation, mark_location);
}
