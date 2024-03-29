/* hyscan-map-tile-source-file.h
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

#ifndef __HYSCAN_MAP_TILE_SOURCE_FILE_H__
#define __HYSCAN_MAP_TILE_SOURCE_FILE_H__

#include <hyscan-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TILE_SOURCE_FILE             (hyscan_map_tile_source_file_get_type ())
#define HYSCAN_MAP_TILE_SOURCE_FILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE_FILE, HyScanMapTileSourceFile))
#define HYSCAN_IS_MAP_TILE_SOURCE_FILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE_FILE))
#define HYSCAN_MAP_TILE_SOURCE_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TILE_SOURCE_FILE, HyScanMapTileSourceFileClass))
#define HYSCAN_IS_MAP_TILE_SOURCE_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TILE_SOURCE_FILE))
#define HYSCAN_MAP_TILE_SOURCE_FILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE_FILE, HyScanMapTileSourceFileClass))

typedef struct _HyScanMapTileSourceFile HyScanMapTileSourceFile;
typedef struct _HyScanMapTileSourceFilePrivate HyScanMapTileSourceFilePrivate;
typedef struct _HyScanMapTileSourceFileClass HyScanMapTileSourceFileClass;

struct _HyScanMapTileSourceFile
{
  GObject parent_instance;

  HyScanMapTileSourceFilePrivate *priv;
};

struct _HyScanMapTileSourceFileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                      hyscan_map_tile_source_file_get_type  (void);

HYSCAN_API
HyScanMapTileSourceFile *  hyscan_map_tile_source_file_new       (const gchar              *dir,
                                                                  HyScanMapTileSource      *fb_source);

HYSCAN_API
void                       hyscan_map_tile_source_file_fb_enable (HyScanMapTileSourceFile  *fs_source,
                                                                  gboolean                  enable);

G_END_DECLS

#endif /* __HYSCAN_MAP_TILE_SOURCE_FILE_H__ */
