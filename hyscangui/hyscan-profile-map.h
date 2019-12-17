/* hyscan-profile-map.h
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

#ifndef __HYSCAN_PROFILE_MAP_H__
#define __HYSCAN_PROFILE_MAP_H__

#include <hyscan-gtk-map.h>
#include <hyscan-profile.h>
#include "hyscan-map-tile-source.h"

G_BEGIN_DECLS

/**
 * HYSCAN_PROFILE_MAP_BASE_ID:
 * Идентификатор, используемый при добавлении слоя подложки на карту.
 *
 * Передав в hyscan_gtk_layer_container_lookup() в качестве аргумента этот
 * идентификатор можно получить слой подложки.
 */
#define HYSCAN_PROFILE_MAP_BASE_ID          "base-layer"

#define HYSCAN_TYPE_PROFILE_MAP             (hyscan_profile_map_get_type ())
#define HYSCAN_PROFILE_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_MAP, HyScanProfileMap))
#define HYSCAN_IS_PROFILE_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_MAP))
#define HYSCAN_PROFILE_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_MAP, HyScanProfileMapClass))
#define HYSCAN_IS_PROFILE_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_MAP))
#define HYSCAN_PROFILE_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_MAP, HyScanProfileMapClass))

typedef struct _HyScanProfileMap HyScanProfileMap;
typedef struct _HyScanProfileMapPrivate HyScanProfileMapPrivate;
typedef struct _HyScanProfileMapClass HyScanProfileMapClass;

struct _HyScanProfileMap
{
  HyScanProfile parent_instance;

  HyScanProfileMapPrivate *priv;
};

struct _HyScanProfileMapClass
{
  HyScanProfileClass parent_class;
};

HYSCAN_API
GType                  hyscan_profile_map_get_type         (void);

HYSCAN_API
HyScanProfileMap *     hyscan_profile_map_new              (const gchar        *cache_dir,
                                                            const gchar        *file_name);

HYSCAN_API
HyScanProfileMap *     hyscan_profile_map_new_default      (const gchar        *cache_dir);

HYSCAN_API
HyScanProfileMap *     hyscan_profile_map_new_full         (const gchar        *title,
                                                            const gchar        *url_format,
                                                            const gchar        *cache_dir,
                                                            const gchar        *cache_subdir,
                                                            gchar             **headers,
                                                            const gchar        *projection,
                                                            guint               min_zoom,
                                                            guint               max_zoom);

HYSCAN_API
HyScanMapTileSource *  hyscan_profile_map_get_source       (HyScanProfileMap   *profile);

HYSCAN_API
void                   hyscan_profile_map_set_offline      (HyScanProfileMap   *profile,
                                                            gboolean            offline);

HYSCAN_API
void                   hyscan_profile_map_set_cache_dir    (HyScanProfileMap   *profile,
                                                            const gchar        *cache_dir);

HYSCAN_API
gboolean               hyscan_profile_map_apply            (HyScanProfileMap   *profile,
                                                            HyScanGtkMap       *map,
                                                            const gchar        *base_layer_id);

HYSCAN_API
gboolean               hyscan_profile_map_write             (HyScanProfileMap   *profile,
                                                             HyScanGtkMap       *map,
                                                             const gchar        *file);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_MAP_H__ */
