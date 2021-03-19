/* hyscan-gtk-layer.с
 *
 * Copyright 2020-2021 Screen LLC, Vladimir Sharov <sharovv@mail.ru>
 *
 * This file is part of HyScanGui.
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

#include "hyscan-gtk-gliko-layer.h"

G_DEFINE_INTERFACE (HyScanGtkGlikoLayer, hyscan_gtk_gliko_layer, G_TYPE_OBJECT);

static void
hyscan_gtk_gliko_layer_default_init (HyScanGtkGlikoLayerInterface *iface)
{
}

void
hyscan_gtk_gliko_layer_realize (HyScanGtkGlikoLayer *instance)
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail (HYSCAN_IS_GTK_GLIKO_LAYER (instance));
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE (instance);
  g_return_if_fail (iface->realize != NULL);
  iface->realize (instance);
}

void
hyscan_gtk_gliko_layer_render (HyScanGtkGlikoLayer *instance, GdkGLContext *context)
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail (HYSCAN_IS_GTK_GLIKO_LAYER (instance));
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE (instance);
  g_return_if_fail (iface->render != NULL);
  iface->render (instance, context);
}

void
hyscan_gtk_gliko_layer_resize (HyScanGtkGlikoLayer *instance, const int width, const int height)
{
  HyScanGtkGlikoLayerInterface *iface;
  g_return_if_fail (HYSCAN_IS_GTK_GLIKO_LAYER (instance));
  iface = HYSCAN_GTK_GLIKO_LAYER_GET_INTERFACE (instance);
  g_return_if_fail (iface->resize != NULL);
  iface->resize (instance, width, height);
}
