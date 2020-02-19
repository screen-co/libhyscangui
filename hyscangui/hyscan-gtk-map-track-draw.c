/* hyscan-gtk-map-track-draw.c
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
 * SECTION: hyscan-gtk-map-track-draw
 * @Short_description: интерфейс для отрисовки галса на планшете
 * @Title: HyScanGtkMapTrackDraw
 *
 * Интерфейс рисования галса на карте, используется для генерации тайлов карты.
 *
 * - hyscan_gtk_map_track_draw_region() - делает изображение галса,
 * - hyscan_gtk_map_track_draw_get_param() - получает параметры отрисовки,
 *   например, стиль оформления.
 */

#include "hyscan-gtk-map-track-draw.h"

G_DEFINE_INTERFACE (HyScanGtkMapTrackDraw, hyscan_gtk_map_track_draw, G_TYPE_OBJECT)

static void
hyscan_gtk_map_track_draw_default_init (HyScanGtkMapTrackDrawInterface *iface)
{
  /**
   * HyScanGtkMapTrackDraw::param-changed:
   * @sensor: указатель на #HyScanGtkMapTrackDraw
   *
   * Данный сигнал посылается при изменении параметров стиля.
   *
   * После отправки данного сигнала объект.
   */
  g_signal_new ("param-changed", HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, G_SIGNAL_RUN_LAST, 0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

void
hyscan_gtk_map_track_draw_region (HyScanGtkMapTrackDraw *track_draw,
                                  GList                 *points,
                                  cairo_t               *cairo,
                                  gdouble                scale,
                                  HyScanGeoCartesian2D  *from,
                                  HyScanGeoCartesian2D  *to)
{
  HyScanGtkMapTrackDrawInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_DRAW (track_draw));

  iface = HYSCAN_GTK_MAP_TRACK_DRAW_GET_IFACE (track_draw);
  if (iface->draw_region != NULL)
    (* iface->draw_region) (track_draw, points, cairo, scale, from, to);
}

HyScanParam *
hyscan_gtk_map_track_draw_get_param (HyScanGtkMapTrackDraw *track_draw)
{
  HyScanGtkMapTrackDrawInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_DRAW (track_draw), NULL);

  iface = HYSCAN_GTK_MAP_TRACK_DRAW_GET_IFACE (track_draw);
  if (iface->get_param != NULL)
    return (* iface->get_param) (track_draw);

  return NULL;
}
