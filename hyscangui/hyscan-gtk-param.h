/* hyscan-gtk-param.h
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

#ifndef __HYSCAN_GTK_PARAM_H__
#define __HYSCAN_GTK_PARAM_H__

#include <hyscan-param.h>
#include <hyscan-gtk-param-key.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PARAM             (hyscan_gtk_param_get_type ())
#define HYSCAN_GTK_PARAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PARAM, HyScanGtkParam))
#define HYSCAN_IS_GTK_PARAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PARAM))
#define HYSCAN_GTK_PARAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PARAM, HyScanGtkParamClass))
#define HYSCAN_IS_GTK_PARAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PARAM))
#define HYSCAN_GTK_PARAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PARAM, HyScanGtkParamClass))

typedef struct _HyScanGtkParam HyScanGtkParam;
typedef struct _HyScanGtkParamPrivate HyScanGtkParamPrivate;
typedef struct _HyScanGtkParamClass HyScanGtkParamClass;

struct _HyScanGtkParam
{
  GtkGrid parent_instance;

  HyScanGtkParamPrivate *priv;
};

struct _HyScanGtkParamClass
{
  GtkGridClass  parent_class;

  void        (*update) (HyScanGtkParam *self);
};

HYSCAN_API
GType                   hyscan_gtk_param_get_type         (void);

HYSCAN_API
void                    hyscan_gtk_param_set_param        (HyScanGtkParam  *gtk_param,
                                                           HyScanParam     *param,
                                                           const gchar     *root,
                                                           gboolean         hidden);
HYSCAN_API
HyScanDataSchema *      hyscan_gtk_param_get_schema       (HyScanGtkParam  *gtk_param);

HYSCAN_API
const HyScanDataSchemaNode *
                        hyscan_gtk_param_get_nodes        (HyScanGtkParam  *gtk_param);

HYSCAN_API
gboolean                hyscan_gtk_param_get_show_hidden  (HyScanGtkParam  *gtk_param);

HYSCAN_API
void                    hyscan_gtk_param_set_watch_list   (HyScanGtkParam  *gtk_param,
                                                           HyScanParamList *plist);

HYSCAN_API
void                    hyscan_gtk_param_apply            (HyScanGtkParam  *gtk_param);

HYSCAN_API
void                    hyscan_gtk_param_discard          (HyScanGtkParam  *gtk_param);

HYSCAN_API
GHashTable *            hyscan_gtk_param_get_widgets      (HyScanGtkParam  *gtk_param);

HYSCAN_API
void                    hyscan_gtk_param_set_watch_period (HyScanGtkParam  *gtk_param,
                                                           guint            period);

HYSCAN_API
gchar *                 hyscan_gtk_param_get_node_name    (const HyScanDataSchemaNode *node);

HYSCAN_API
const HyScanDataSchemaNode * hyscan_gtk_param_find_node   (const HyScanDataSchemaNode *node,
                                                           const gchar                *path);


HYSCAN_API
gboolean                hyscan_gtk_param_node_has_visible_keys (const HyScanDataSchemaNode *node,
                                                                gboolean                    show_hidden);

HYSCAN_API
void                    hyscan_gtk_param_clear_container  (GtkContainer    *container);


G_END_DECLS

#endif /* __HYSCAN_GTK_PARAM_H__ */
