/* hyscan-gtk-map-tile.h
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

#ifndef __HYSCAN_GTK_MAP_TILE_H__
#define __HYSCAN_GTK_MAP_TILE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILE             (hyscan_gtk_map_tile_get_type ())
#define HYSCAN_GTK_MAP_TILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTile))
#define HYSCAN_IS_GTK_MAP_TILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILE))
#define HYSCAN_GTK_MAP_TILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTileClass))
#define HYSCAN_IS_GTK_MAP_TILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILE))
#define HYSCAN_GTK_MAP_TILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTileClass))

typedef struct _HyScanGtkMapTile HyScanGtkMapTile;
typedef struct _HyScanGtkMapTilePrivate HyScanGtkMapTilePrivate;
typedef struct _HyScanGtkMapTileClass HyScanGtkMapTileClass;

/**
 * HyScanGtkMapTileGrid:
 * @min_x: минимальное значение координаты x
 * @max_x: максимальное значение координаты x
 * @min_y: минимальное значение координаты y
 * @max_y: максимальное значение координаты y
 * @tiles_num: число тайлов вдоль каждой из осей
 *
 * Структура с информацией о квадратной тайловой сетке размера @tiles_num.
 * Используется для перевода логических координат в тайловую СК и обратно.
 */
typedef struct
{
  gdouble min_x;
  gdouble min_y;
  gdouble max_x;
  gdouble max_y;
  gdouble tiles_num;
} HyScanGtkMapTileGrid;

struct _HyScanGtkMapTile
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapTilePrivate *priv;
};

struct _HyScanGtkMapTileClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_tile_get_type           (void);

HYSCAN_API
HyScanGtkMapTile *     hyscan_gtk_map_tile_new                (guint                 x,
                                                               guint                 y,
                                                               guint                 z,
                                                               guint                 size);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_x              (HyScanGtkMapTile     *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_y              (HyScanGtkMapTile     *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_zoom           (HyScanGtkMapTile     *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_size           (HyScanGtkMapTile     *tile);

HYSCAN_API
gboolean               hyscan_gtk_map_tile_set_pixbuf         (HyScanGtkMapTile     *tile,
                                                               GdkPixbuf            *pixbuf);

HYSCAN_API
GdkPixbuf *            hyscan_gtk_map_tile_get_pixbuf         (HyScanGtkMapTile     *tile);

HYSCAN_API
gint                   hyscan_gtk_map_tile_compare            (HyScanGtkMapTile     *a,
                                                               HyScanGtkMapTile     *b);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_bound         (HyScanGtkMapTileGrid *grid,
                                                               HyScanGtkMapRect     *region,
                                                               gint                 *from_tile_x,
                                                               gint                 *to_tile_x,
                                                               gint                 *from_tile_y,
                                                               gint                 *to_tile_y);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_tile_to_value (HyScanGtkMapTileGrid *grid,
                                                               gdouble               tile_x,
                                                               gdouble               tile_y,
                                                               gdouble              *x_val,
                                                               gdouble              *y_val);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_value_to_tile (HyScanGtkMapTileGrid *grid,
                                                               gdouble               x_val,
                                                               gdouble               y_val,
                                                               gdouble              *x_tile,
                                                               gdouble              *y_tile);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILE_H__ */
