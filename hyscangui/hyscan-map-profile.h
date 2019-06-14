/* hyscan-gtk-map-profile.h
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

#ifndef __HYSCAN_MAP_PROFILE_H__
#define __HYSCAN_MAP_PROFILE_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_PROFILE             (hyscan_map_profile_get_type ())
#define HYSCAN_MAP_PROFILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfile))
#define HYSCAN_IS_MAP_PROFILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_PROFILE))
#define HYSCAN_MAP_PROFILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfileClass))
#define HYSCAN_IS_MAP_PROFILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_PROFILE))
#define HYSCAN_MAP_PROFILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfileClass))

typedef struct _HyScanMapProfile HyScanMapProfile;
typedef struct _HyScanMapProfilePrivate HyScanMapProfilePrivate;
typedef struct _HyScanMapProfileClass HyScanMapProfileClass;

struct _HyScanMapProfile
{
  GObject parent_instance;

  HyScanMapProfilePrivate *priv;
};

struct _HyScanMapProfileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_profile_get_type         (void);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new              (const gchar        *cache_dir);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new_default      (const gchar        *cache_dir);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new_full         (const gchar        *title,
                                                            const gchar        *url_format,
                                                            const gchar        *cache_dir,
                                                            const gchar        *cache_subdir,
                                                            const gchar        *projection,
                                                            guint               min_zoom,
                                                            guint               max_zoom);

HYSCAN_API
gchar *                hyscan_map_profile_get_title        (HyScanMapProfile   *profile);

HYSCAN_API
gboolean               hyscan_map_profile_read             (HyScanMapProfile   *profile,
                                                            const gchar        *filename);

HYSCAN_API
gboolean               hyscan_map_profile_apply            (HyScanMapProfile   *profile,
                                                            HyScanGtkMap       *map);

G_END_DECLS

#endif /* __HYSCAN_MAP_PROFILE_H__ */
