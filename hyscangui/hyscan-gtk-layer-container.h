/* hyscan-gtk-layer-container.h
 *
 * Copyright 2017 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#ifndef __HYSCAN_GTK_LAYER_CONTAINER_H__
#define __HYSCAN_GTK_LAYER_CONTAINER_H__

#include <hyscan-param.h>
#include <hyscan-gtk-layer.h>
#include <hyscan-gtk-layer-common.h>
#include <gtk-cifro-area.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER_CONTAINER             (hyscan_gtk_layer_container_get_type ())
#define HYSCAN_GTK_LAYER_CONTAINER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainer))
#define HYSCAN_IS_GTK_LAYER_CONTAINER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER))
#define HYSCAN_GTK_LAYER_CONTAINER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainerClass))
#define HYSCAN_IS_GTK_LAYER_CONTAINER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_LAYER_CONTAINER))
#define HYSCAN_GTK_LAYER_CONTAINER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_LAYER_CONTAINER, HyScanGtkLayerContainerClass))

typedef struct _HyScanGtkLayerContainerPrivate HyScanGtkLayerContainerPrivate;
typedef struct _HyScanGtkLayerContainerClass HyScanGtkLayerContainerClass;

struct _HyScanGtkLayerContainer
{
  GtkCifroArea parent_instance;

  HyScanGtkLayerContainerPrivate *priv;
};

struct _HyScanGtkLayerContainerClass
{
  GtkCifroAreaClass parent_class;
};

HYSCAN_API
GType                     hyscan_gtk_layer_container_get_type            (void);

HYSCAN_API
void                      hyscan_gtk_layer_container_add                 (HyScanGtkLayerContainer *container,
                                                                          HyScanGtkLayer          *layer,
                                                                          const gchar             *key);

HYSCAN_API
void                      hyscan_gtk_layer_container_remove              (HyScanGtkLayerContainer *container,
                                                                          const gchar             *key);

HYSCAN_API
void                      hyscan_gtk_layer_container_remove_all          (HyScanGtkLayerContainer *container);

HYSCAN_API
HyScanParam *             hyscan_gtk_layer_container_get_param           (HyScanGtkLayerContainer *container);

HYSCAN_API
HyScanGtkLayer *          hyscan_gtk_layer_container_lookup              (HyScanGtkLayerContainer *container,
                                                                          const gchar             *key);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_changes_allowed (HyScanGtkLayerContainer *container,
                                                                          gboolean                 changes_allowed);
HYSCAN_API
gboolean                  hyscan_gtk_layer_container_get_changes_allowed (HyScanGtkLayerContainer *container);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_input_owner     (HyScanGtkLayerContainer *container,
                                                                          gconstpointer            instance);

HYSCAN_API
gconstpointer             hyscan_gtk_layer_container_get_input_owner     (HyScanGtkLayerContainer *container);

HYSCAN_API
void                      hyscan_gtk_layer_container_set_handle_grabbed  (HyScanGtkLayerContainer *container,
                                                                          gconstpointer            instance);

HYSCAN_API
gconstpointer             hyscan_gtk_layer_container_get_handle_grabbed  (HyScanGtkLayerContainer *container);


G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_CONTAINER_H__ */
