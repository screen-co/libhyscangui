/* hyscan-pseudo-mercator.c
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
 * SECTION: hyscan-pseudo-mercator
 * @Short_description: Картографическая проекция Меркатора для сферы
 * @Title: HyScanPseudoMercator
 *
 * Класс реализует интерфейс #HyScanGeoProjection.
 *
 * Псевдопроекция Меркатора — упрощенная версия проекции #HyScanMercator, где
 * принято считать, что Земля имеет форму сферы.
 *
 * Подобная проекция используется многими Web-сервисами: OSM, Google, Bing -
 * поскольку позволяет значительно упростить вычисления, не сильно теряя в
 * точности данных.
 *
 * Границы проекции по обеим осями (0, 1).
 */

#include "hyscan-pseudo-mercator.h"
#include <math.h>

#define EARTH_RADIUS 6378137.0
#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)

enum
{
  PROP_O,
};

struct _HyScanPseudoMercatorPrivate
{
  gdouble                      equator_length;             /* Длина экватора. */
};

static void    hyscan_pseudo_mercator_interface_init     (HyScanGeoProjectionInterface *iface);
static void    hyscan_pseudo_mercator_object_constructed (GObject                      *object);

static void    hyscan_pseudo_mercator_value_to_geo       (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic            *coords,
                                                          gdouble                       x,
                                                          gdouble                       y);

static void    hyscan_pseudo_mercator_geo_to_value       (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic             coords,
                                                          HyScanGeoCartesian2D         *c2d);

static void    hyscan_pseudo_mercator_get_limits         (HyScanGeoProjection          *projection,
                                                          gdouble                      *min_x,
                                                          gdouble                      *max_x,
                                                          gdouble                      *min_y,
                                                          gdouble                      *max_y);

static gdouble hyscan_pseudo_mercator_get_scale          (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic             coords);

G_DEFINE_TYPE_WITH_CODE (HyScanPseudoMercator, hyscan_pseudo_mercator, G_TYPE_OBJECT,                                        
                         G_ADD_PRIVATE (HyScanPseudoMercator)                                                         
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GEO_PROJECTION, hyscan_pseudo_mercator_interface_init))

static void
hyscan_pseudo_mercator_class_init (HyScanPseudoMercatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_pseudo_mercator_object_constructed;
}

static void
hyscan_pseudo_mercator_init (HyScanPseudoMercator *pseudo_mercator)
{
  pseudo_mercator->priv = hyscan_pseudo_mercator_get_instance_private (pseudo_mercator);
}

static guint
hyscan_pseudo_mercator_hash (HyScanGeoProjection *geo_projection)
{
  return g_str_hash ("pseudo_mercator");
}


/* Реализация интерфейса HyScanGeoProjectionInterface. */
static void
hyscan_pseudo_mercator_interface_init (HyScanGeoProjectionInterface *iface)
{
  iface->geo_to_value = hyscan_pseudo_mercator_geo_to_value;
  iface->value_to_geo = hyscan_pseudo_mercator_value_to_geo;
  iface->get_limits = hyscan_pseudo_mercator_get_limits;
  iface->get_scale = hyscan_pseudo_mercator_get_scale;
  iface->hash = hyscan_pseudo_mercator_hash;
}

static void
hyscan_pseudo_mercator_object_constructed (GObject *object)
{
  HyScanPseudoMercator *pseudo_mercator = HYSCAN_PSEUDO_MERCATOR (object);
  HyScanPseudoMercatorPrivate *priv = pseudo_mercator->priv;

  G_OBJECT_CLASS (hyscan_pseudo_mercator_parent_class)->constructed (object);

  priv->equator_length = 2.0 * M_PI * EARTH_RADIUS;
}

/* Переводит географические координаты @coords в координаты (@x, @y) проекции. */
static void
hyscan_pseudo_mercator_geo_to_value (HyScanGeoProjection  *projection,
                                     HyScanGeoGeodetic     coords,
                                     HyScanGeoCartesian2D *c2d)
{
  gdouble lat_rad;

  (void) projection;

  lat_rad = DEG2RAD (coords.lat);

  c2d->x = (coords.lon + 180.0) / 360.0;
  c2d->y = 1.0 - (1.0 - log (tan (lat_rad) + 1.0 / cos (lat_rad)) / M_PI) / 2.0;
}

/* Переводит координаты на карте (@x, @y) в географические координаты @coords. */
static void
hyscan_pseudo_mercator_value_to_geo (HyScanGeoProjection *projection,
                                     HyScanGeoGeodetic   *coords,
                                     gdouble              x,
                                     gdouble              y)
{
  (void) projection;

  /* Меняем направление оси OY. */
  y = 1.0 - y;

  coords->lon = x * 360.0 - 180.0 ;
  coords->lat = RAD2DEG (atan (sinh (M_PI * (1.0 - 2.0 * y))));
}

/* Определяет границы проекции. */
static void
hyscan_pseudo_mercator_get_limits (HyScanGeoProjection *projection,
                                   gdouble             *min_x,
                                   gdouble             *max_x,
                                   gdouble             *min_y,
                                   gdouble             *max_y)
{
  (void) projection;

  (min_x != NULL) ? *min_x = 0.0 : 0;
  (max_x != NULL) ? *max_x = 1.0 : 0;
  (min_y != NULL) ? *min_y = 0.0 : 0;
  (max_y != NULL) ? *max_y = 1.0 : 0;
}

/* Определяет масштаб проекции в указанной точке @coords. */
static gdouble
hyscan_pseudo_mercator_get_scale (HyScanGeoProjection *projection,
                                  HyScanGeoGeodetic    coords)
{
  HyScanPseudoMercatorPrivate *priv = HYSCAN_PSEUDO_MERCATOR (projection)->priv;

  return cos (DEG2RAD (coords.lat)) * priv->equator_length;
}

/**
 * hyscan_pseudo_mercator_new:
 *
 * Создаёт псевдопроекцию Мерктора
 *
 * Returns: указатель на #HyScanPseudoMercator. Для удаления g_object_unref()
 */
HyScanGeoProjection *
hyscan_pseudo_mercator_new (void)
{
  return g_object_new (HYSCAN_TYPE_PSEUDO_MERCATOR, NULL);
}
