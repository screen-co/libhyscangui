/* hyscan-gtk-gliko-layer.h
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
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

#ifndef __HYSCAN_GTK_GLIKO_LAYER_H__
#define __HYSCAN_GTK_GLIKO_LAYER_H__

#include <gtk/gtk.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_GLIKO_LAYER                (hyscan_gtk_gliko_layer_get_type())
#define HYSCAN_GTK_GLIKO_LAYER(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),HYSCAN_TYPE_GTK_GLIKO_LAYER,HyScanGtkGlikoLayer))
#define HYSCAN_IS_GTK_GLIKO_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj),HYSCAN_TYPE_GTK_GLIKO_LAYER))
#define HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE((inst), HYSCAN_TYPE_GTK_GLIKO_LAYER, HyScanGtkGlikoLayerInterface))

typedef struct _HyScanGtkGlikoLayer HyScanGtkGlikoLayer;
typedef struct _HyScanGtkGlikoLayerInterface HyScanGtkGlikoLayerInterface;

struct _HyScanGtkGlikoLayerInterface
{
  GTypeInterface parent_iface;
  void (*realize)( HyScanGtkGlikoLayer *instance );
  void (*render)( HyScanGtkGlikoLayer *instance, GdkGLContext *context );
  void (*resize)( HyScanGtkGlikoLayer *instance, const int width, const int height );
};

HYSCAN_API
GType                   hyscan_gtk_gliko_layer_get_type                     (void);

HYSCAN_API
void                    hyscan_gtk_gliko_layer_realize                      (HyScanGtkGlikoLayer *instance);

HYSCAN_API
void                    hyscan_gtk_gliko_layer_render                       (HyScanGtkGlikoLayer *instance,
                                                                             GdkGLContext *context);

HYSCAN_API
void                    hyscan_gtk_gliko_layer_resize                       (HyScanGtkGlikoLayer *instance,
                                                                             const int width,
                                                                             const int height);

G_END_DECLS

#endif /* __GLIKO_LAYER_H__ */
