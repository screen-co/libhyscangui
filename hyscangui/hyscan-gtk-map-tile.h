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

#define HYSCAN_TYPE_GTK_MAP_TILE_GRID             (hyscan_gtk_map_tile_grid_get_type ())
#define HYSCAN_GTK_MAP_TILE_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILE_GRID, HyScanGtkMapTileGrid))
#define HYSCAN_IS_GTK_MAP_TILE_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILE_GRID))
#define HYSCAN_GTK_MAP_TILE_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILE_GRID, HyScanGtkMapTileGridClass))
#define HYSCAN_IS_GTK_MAP_TILE_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILE_GRID))
#define HYSCAN_GTK_MAP_TILE_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILE_GRID, HyScanGtkMapTileGridClass))

typedef struct _HyScanGtkMapTile HyScanGtkMapTile;
typedef struct _HyScanGtkMapTilePrivate HyScanGtkMapTilePrivate;
typedef struct _HyScanGtkMapTileClass HyScanGtkMapTileClass;

typedef struct _HyScanGtkMapTileGrid HyScanGtkMapTileGrid;
typedef struct _HyScanGtkMapTileGridPrivate HyScanGtkMapTileGridPrivate;
typedef struct _HyScanGtkMapTileGridClass HyScanGtkMapTileGridClass;

struct _HyScanGtkMapTile
{
  GObject parent_instance;

  HyScanGtkMapTilePrivate *priv;
};

struct _HyScanGtkMapTileClass
{
  GObjectClass parent_class;
};

struct _HyScanGtkMapTileGrid
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapTileGridPrivate *priv;
};

struct _HyScanGtkMapTileGridClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_tile_get_type           (void);

HYSCAN_API
HyScanGtkMapTile *     hyscan_gtk_map_tile_new                (HyScanGtkMapTileGrid *grid,
                                                               guint                 x,
                                                               guint                 y,
                                                               guint                 z);

HYSCAN_API
HyScanGtkMapTileGrid * hyscan_gtk_map_tile_grid_new           (GtkCifroArea         *carea,
                                                               guint                 min_zoom,
                                                               guint                 tile_size,
                                                               const gdouble        *scales,
                                                               gsize                 scales_len);

HYSCAN_API
HyScanGtkMapTileGrid * hyscan_gtk_map_tile_grid_new_from_num  (GtkCifroArea         *carea,
                                                               guint                 min_zoom,
                                                               guint                 tile_size,
                                                               const gdouble        *xnums,
                                                               gsize                 xnums_len);

HYSCAN_API
gdouble                hyscan_gtk_map_tile_grid_get_scale     (HyScanGtkMapTileGrid *grid,
                                                               guint                 zoom);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_x              (HyScanGtkMapTile     *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_y              (HyScanGtkMapTile     *tile);

HYSCAN_API
void                   hyscan_gtk_map_tile_get_bounds         (HyScanGtkMapTile     *tile,
                                                               gdouble              *from_x,
                                                               gdouble              *to_x,
                                                               gdouble              *from_y,
                                                               gdouble              *to_y);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_zoom           (HyScanGtkMapTile     *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_size           (HyScanGtkMapTile     *tile);

HYSCAN_API
gdouble                hyscan_gtk_map_tile_get_scale          (HyScanGtkMapTile     *tile);

HYSCAN_API
void                   hyscan_gtk_map_tile_set_surface_data   (HyScanGtkMapTile *tile,
                                                               gpointer          data,
                                                               guint32           size);

HYSCAN_API
gboolean               hyscan_gtk_map_tile_set_pixbuf         (HyScanGtkMapTile     *tile,
                                                               GdkPixbuf            *pixbuf);

HYSCAN_API
void                   hyscan_gtk_map_tile_set_surface        (HyScanGtkMapTile     *tile,
                                                               cairo_surface_t      *surface);

HYSCAN_API
cairo_surface_t *      hyscan_gtk_map_tile_get_surface        (HyScanGtkMapTile *tile);

HYSCAN_API
gint                   hyscan_gtk_map_tile_compare            (HyScanGtkMapTile     *a,
                                                               HyScanGtkMapTile     *b);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_get_view      (HyScanGtkMapTileGrid *grid,
                                                               GtkCifroArea         *carea,
                                                               guint                 zoom,
                                                               gint                 *from_tile_x,
                                                               gint                 *to_tile_x,
                                                               gint                 *from_tile_y,
                                                               gint                 *to_tile_y);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_tile_to_value (HyScanGtkMapTileGrid *grid,
                                                               guint                 zoom,
                                                               gdouble               tile_x,
                                                               gdouble               tile_y,
                                                               gdouble              *x_val,
                                                               gdouble              *y_val);

HYSCAN_API
void                   hyscan_gtk_map_tile_grid_value_to_tile (HyScanGtkMapTileGrid *grid,
                                                               guint                 zoom,
                                                               gdouble               x_val,
                                                               gdouble               y_val,
                                                               gdouble              *x_tile,
                                                               gdouble              *y_tile);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILE_H__ */
