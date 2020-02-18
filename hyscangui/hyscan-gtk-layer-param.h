/* hyscan-gtk-layer-param.h
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

#ifndef __HYSCAN_GTK_LAYER_PARAM_H__
#define __HYSCAN_GTK_LAYER_PARAM_H__

#include <hyscan-param-controller.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER_PARAM             (hyscan_gtk_layer_param_get_type ())
#define HYSCAN_GTK_LAYER_PARAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER_PARAM, HyScanGtkLayerParam))
#define HYSCAN_IS_GTK_LAYER_PARAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER_PARAM))
#define HYSCAN_GTK_LAYER_PARAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_LAYER_PARAM, HyScanGtkLayerParamClass))
#define HYSCAN_IS_GTK_LAYER_PARAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_LAYER_PARAM))
#define HYSCAN_GTK_LAYER_PARAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_LAYER_PARAM, HyScanGtkLayerParamClass))

typedef struct _HyScanGtkLayerParam HyScanGtkLayerParam;
typedef struct _HyScanGtkLayerParamClass HyScanGtkLayerParamClass;

struct _HyScanGtkLayerParam
{
  HyScanParamController parent_instance;
};

struct _HyScanGtkLayerParamClass
{
  HyScanParamControllerClass parent_class;
};

HYSCAN_API
GType                 hyscan_gtk_layer_param_get_type         (void);

HYSCAN_API
HyScanGtkLayerParam * hyscan_gtk_layer_param_new              (void);

HYSCAN_API
void                  hyscan_gtk_layer_param_set_stock_schema (HyScanGtkLayerParam *layer_param,
                                                               const gchar         *schema_id);

HYSCAN_API
gboolean              hyscan_gtk_layer_param_add_rgba         (HyScanGtkLayerParam *layer_param,
                                                               const gchar         *name,
                                                               GdkRGBA             *rgba);

HYSCAN_API
void                  hyscan_gtk_layer_param_set_default      (HyScanGtkLayerParam *param);

HYSCAN_API
void                  hyscan_gtk_layer_param_file_to_list     (GKeyFile            *key_file,
                                                               HyScanParamList     *list,
                                                               HyScanDataSchema    *schema);

HYSCAN_API
void                  hyscan_gtk_layer_param_list_to_file     (GKeyFile            *key_file,
                                                               HyScanParamList     *list,
                                                               HyScanDataSchema    *schema);

G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_PARAM_H__ */
