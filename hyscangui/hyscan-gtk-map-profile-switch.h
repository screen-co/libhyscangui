/* hyscan-gtk-map-profile-switch.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_GTK_MAP_PROFILE_SWITCH_H__
#define __HYSCAN_GTK_MAP_PROFILE_SWITCH_H__

#include <hyscan-gtk-map.h>
#include <hyscan-profile-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH             (hyscan_gtk_map_profile_switch_get_type ())
#define HYSCAN_GTK_MAP_PROFILE_SWITCH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH, HyScanGtkMapProfileSwitch))
#define HYSCAN_IS_GTK_MAP_PROFILE_SWITCH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH))
#define HYSCAN_GTK_MAP_PROFILE_SWITCH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH, HyScanGtkMapProfileSwitchClass))
#define HYSCAN_IS_GTK_MAP_PROFILE_SWITCH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH))
#define HYSCAN_GTK_MAP_PROFILE_SWITCH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_PROFILE_SWITCH, HyScanGtkMapProfileSwitchClass))

typedef struct _HyScanGtkMapProfileSwitch HyScanGtkMapProfileSwitch;
typedef struct _HyScanGtkMapProfileSwitchPrivate HyScanGtkMapProfileSwitchPrivate;
typedef struct _HyScanGtkMapProfileSwitchClass HyScanGtkMapProfileSwitchClass;

struct _HyScanGtkMapProfileSwitch
{
  GtkBox parent_instance;

  HyScanGtkMapProfileSwitchPrivate *priv;
};

struct _HyScanGtkMapProfileSwitchClass
{
  GtkBoxClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_profile_switch_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_profile_switch_new              (HyScanGtkMap              *map,
                                                                       const gchar               *base_id,
                                                                       const gchar               *cache_dir);

HYSCAN_API
void                   hyscan_gtk_map_profile_switch_load_config      (HyScanGtkMapProfileSwitch *profile_switch);

HYSCAN_API
void                   hyscan_gtk_map_profile_switch_append           (HyScanGtkMapProfileSwitch *profile_switch,
                                                                       const gchar               *profile_id,
                                                                       HyScanProfileMap          *profile);

HYSCAN_API
gchar *                hyscan_gtk_map_profile_switch_get_id           (HyScanGtkMapProfileSwitch *profile_switch);

HYSCAN_API
void                   hyscan_gtk_map_profile_switch_set_id           (HyScanGtkMapProfileSwitch *profile_switch,
                                                                       const gchar               *profile_id);

HYSCAN_API
gboolean               hyscan_gtk_map_profile_switch_get_offline      (HyScanGtkMapProfileSwitch *profile_switch);

HYSCAN_API
void                   hyscan_gtk_map_profile_switch_set_offline      (HyScanGtkMapProfileSwitch *profile_switch,
                                                                       gboolean                   offline);
HYSCAN_API
void                   hyscan_gtk_map_profile_switch_set_cache_dir    (HyScanGtkMapProfileSwitch *profile_switch,
                                                                       const gchar               *cache_id);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_PROFILE_SWITCH_H__ */
