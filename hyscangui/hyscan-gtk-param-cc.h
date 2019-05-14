/* hyscan-gtk-param-cc.h
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#ifndef __HYSCAN_GTK_PARAM_CC_H__
#define __HYSCAN_GTK_PARAM_CC_H__

#include <hyscan-gtk-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PARAM_CC             (hyscan_gtk_param_cc_get_type ())
#define HYSCAN_GTK_PARAM_CC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PARAM_CC, HyScanGtkParamCC))
#define HYSCAN_IS_GTK_PARAM_CC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PARAM_CC))
#define HYSCAN_GTK_PARAM_CC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PARAM_CC, HyScanGtkParamCCClass))
#define HYSCAN_IS_GTK_PARAM_CC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PARAM_CC))
#define HYSCAN_GTK_PARAM_CC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PARAM_CC, HyScanGtkParamCCClass))

typedef struct _HyScanGtkParamCC HyScanGtkParamCC;
typedef struct _HyScanGtkParamCCPrivate HyScanGtkParamCCPrivate;
typedef struct _HyScanGtkParamCCClass HyScanGtkParamCCClass;

struct _HyScanGtkParamCC
{
  HyScanGtkParam parent_instance;

  HyScanGtkParamCCPrivate *priv;
};

struct _HyScanGtkParamCCClass
{
  HyScanGtkParamClass parent_class;
};


HYSCAN_API
GType                  hyscan_gtk_param_cc_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_param_cc_new              (HyScanParam *param,
                                                             const gchar *root,
                                                             gboolean     show_hidden);

HYSCAN_API
GtkWidget *            hyscan_gtk_param_cc_new_default      (HyScanParam *param);

G_END_DECLS

#endif /* __HYSCAN_GTK_PARAM_CC_H__ */
