/* hyscan-navigation-model.h
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

#ifndef __HYSCAN_NAVIGATION_MODEL_H__
#define __HYSCAN_NAVIGATION_MODEL_H__

#include <hyscan-sensor.h>
#include <hyscan-geo.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAVIGATION_MODEL             (hyscan_navigation_model_get_type ())
#define HYSCAN_NAVIGATION_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAVIGATION_MODEL, HyScanNavigationModel))
#define HYSCAN_IS_NAVIGATION_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAVIGATION_MODEL))
#define HYSCAN_NAVIGATION_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NAVIGATION_MODEL, HyScanNavigationModelClass))
#define HYSCAN_IS_NAVIGATION_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NAVIGATION_MODEL))
#define HYSCAN_NAVIGATION_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NAVIGATION_MODEL, HyScanNavigationModelClass))

typedef struct _HyScanNavigationModel HyScanNavigationModel;
typedef struct _HyScanNavigationModelPrivate HyScanNavigationModelPrivate;
typedef struct _HyScanNavigationModelClass HyScanNavigationModelClass;

typedef struct
{
  gboolean          loaded;

  gdouble           time;
  HyScanGeoGeodetic coord;
  gboolean          true_heading; /* Истинный курс установлен по HDT. */
  gdouble           heading;      /* Истинный курс, радианы. */
  gdouble           speed;        /* Скорость, м/с. */
} HyScanNavigationModelData;

struct _HyScanNavigationModel
{
  GObject parent_instance;

  HyScanNavigationModelPrivate *priv;
};

struct _HyScanNavigationModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_navigation_model_get_type         (void);

HYSCAN_API
HyScanNavigationModel *  hyscan_navigation_model_new              (void);

HYSCAN_API
void                     hyscan_navigation_model_set_sensor       (HyScanNavigationModel     *model,
                                                                   HyScanSensor              *sensor);

HYSCAN_API
void                     hyscan_navigation_model_set_sensor_name  (HyScanNavigationModel     *model,
                                                                   const gchar               *name);

HYSCAN_API
void                     hyscan_navigation_model_set_delay        (HyScanNavigationModel     *model,
                                                                   gdouble                    delay);

HYSCAN_API
gboolean                 hyscan_navigation_model_get              (HyScanNavigationModel     *model,
                                                                   HyScanNavigationModelData *data,
                                                                   gdouble                   *time_delta);

G_END_DECLS

#endif /* __HYSCAN_NAVIGATION_MODEL_H__ */
