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

#ifndef __HYSCAN_MAP_TILE_H__
#define __HYSCAN_MAP_TILE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TILE             (hyscan_map_tile_get_type ())
#define HYSCAN_MAP_TILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE, HyScanMapTile))
#define HYSCAN_IS_MAP_TILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TILE))
#define HYSCAN_MAP_TILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TILE, HyScanMapTileClass))
#define HYSCAN_IS_MAP_TILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TILE))
#define HYSCAN_MAP_TILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TILE, HyScanMapTileClass))

#define HYSCAN_TYPE_MAP_TILE_GRID             (hyscan_map_tile_grid_get_type ())
#define HYSCAN_MAP_TILE_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE_GRID, HyScanMapTileGrid))
#define HYSCAN_IS_MAP_TILE_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TILE_GRID))
#define HYSCAN_MAP_TILE_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TILE_GRID, HyScanMapTileGridClass))
#define HYSCAN_IS_MAP_TILE_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TILE_GRID))
#define HYSCAN_MAP_TILE_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TILE_GRID, HyScanMapTileGridClass))

typedef struct _HyScanMapTile HyScanMapTile;
typedef struct _HyScanMapTilePrivate HyScanMapTilePrivate;
typedef struct _HyScanMapTileClass HyScanMapTileClass;

typedef struct _HyScanMapTileGrid HyScanMapTileGrid;
typedef struct _HyScanMapTileGridPrivate HyScanMapTileGridPrivate;
typedef struct _HyScanMapTileGridClass HyScanMapTileGridClass;
typedef struct _HyScanMapTileIter HyScanMapTileIter;

/**
 * HyScanMapTileIter:
 *
 * Итератор тайлов. Позволяет обходить тайлы указанной области в порядке от центра
 * области к её краям.
 *
 * Структура обычно создаётся в стеке и инициализируется через hyscan_map_tile_iter_init().
 *
 */
struct _HyScanMapTileIter
{
  /*< private > - поля структуры не предназначены для прямого доступа */
  gint from_x;   /* Минимальная координата по оси x. */
  gint to_x;     /* Максимальная координата по оси x. */
  gint from_y;   /* Минимальная координата по оси x. */
  gint to_y;     /* Максимальная координата по оси x. */
  gint xc;       /* Координата x центра. */
  gint yc;       /* Координата y центра. */
  gint max_r;    /* Максимальное расстояние до центра по каждой из координат. */
  gint x;        /* Текущая координата по оси x. */
  gint y;        /* Текущая координата по оси x. */
  gint r;        /* Текущее расстояние до центра по каждой из координат. */
};

struct _HyScanMapTile
{
  GObject parent_instance;

  HyScanMapTilePrivate *priv;
};

struct _HyScanMapTileClass
{
  GObjectClass parent_class;
};

struct _HyScanMapTileGrid
{
  GInitiallyUnowned parent_instance;

  HyScanMapTileGridPrivate *priv;
};

struct _HyScanMapTileGridClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_tile_grid_get_type          (void);

HYSCAN_API
HyScanMapTileGrid *    hyscan_map_tile_grid_new_from_cifro    (GtkCifroArea         *carea,
                                                               guint                 min_zoom,
                                                               guint                 tile_size);

HYSCAN_API
HyScanMapTileGrid *    hyscan_map_tile_grid_new               (gdouble               min_x,
                                                               gdouble               max_x,
                                                               gdouble               min_y,
                                                               gdouble               max_y,
                                                               guint                 min_zoom,
                                                               guint                 tile_size);

HYSCAN_API
void                   hyscan_map_tile_grid_set_xnums         (HyScanMapTileGrid    *grid,
                                                               const guint          *xnums,
                                                               gsize                 xnums_len);

HYSCAN_API
void                   hyscan_map_tile_grid_set_scales        (HyScanMapTileGrid    *grid,
                                                               const gdouble        *scales,
                                                               gsize                 scales_len);

HYSCAN_API
guint                  hyscan_map_tile_grid_get_tile_size     (HyScanMapTileGrid    *grid);

HYSCAN_API
gdouble                hyscan_map_tile_grid_get_scale         (HyScanMapTileGrid    *grid,
                                                               guint                 zoom);

HYSCAN_API
void                   hyscan_map_tile_grid_get_zoom_range    (HyScanMapTileGrid    *grid,
                                                               guint                *min_zoom,
                                                               guint                *max_zoom);

HYSCAN_API
guint                  hyscan_map_tile_grid_adjust_zoom       (HyScanMapTileGrid    *grid,
                                                               gdouble               scale);

HYSCAN_API
void                   hyscan_map_tile_grid_get_view_cifro    (HyScanMapTileGrid    *grid,
                                                               GtkCifroArea         *carea,
                                                               guint                 zoom,
                                                               gint                 *from_tile_x,
                                                               gint                 *to_tile_x,
                                                               gint                 *from_tile_y,
                                                               gint                 *to_tile_y);

HYSCAN_API
void                   hyscan_map_tile_grid_get_view          (HyScanMapTileGrid    *grid,
                                                               guint                 zoom,
                                                               gdouble               from_x,
                                                               gdouble               to_x,
                                                               gdouble               from_y,
                                                               gdouble               to_y,
                                                               gint                 *from_tile_x,
                                                               gint                 *to_tile_x,
                                                               gint                 *from_tile_y,
                                                               gint                 *to_tile_y);

HYSCAN_API
void                   hyscan_map_tile_grid_tile_to_value     (HyScanMapTileGrid    *grid,
                                                               guint                 zoom,
                                                               gdouble               tile_x,
                                                               gdouble               tile_y,
                                                               gdouble              *x_val,
                                                               gdouble              *y_val);

HYSCAN_API
void                   hyscan_map_tile_grid_value_to_tile     (HyScanMapTileGrid    *grid,
                                                               guint                 zoom,
                                                               gdouble               x_val,
                                                               gdouble               y_val,
                                                               gdouble              *x_tile,
                                                               gdouble              *y_tile);

HYSCAN_API
GType                  hyscan_map_tile_get_type               (void);

HYSCAN_API
HyScanMapTile *        hyscan_map_tile_new                    (HyScanMapTileGrid    *grid,
                                                               guint                 x,
                                                               guint                 y,
                                                               guint                 z);

HYSCAN_API
guint                  hyscan_map_tile_get_x                  (HyScanMapTile        *tile);

HYSCAN_API
guint                  hyscan_map_tile_get_y                  (HyScanMapTile        *tile);

HYSCAN_API
guint                  hyscan_map_tile_inv_y                  (HyScanMapTile        *tile);

HYSCAN_API
void                   hyscan_map_tile_get_bounds             (HyScanMapTile        *tile,
                                                               HyScanGeoCartesian2D *from,
                                                               HyScanGeoCartesian2D *to);

HYSCAN_API
guint                  hyscan_map_tile_get_zoom               (HyScanMapTile        *tile);

HYSCAN_API
guint                  hyscan_map_tile_get_size               (HyScanMapTile        *tile);

HYSCAN_API
gdouble                hyscan_map_tile_get_scale              (HyScanMapTile        *tile);

HYSCAN_API
void                   hyscan_map_tile_set_surface_data       (HyScanMapTile        *tile,
                                                               gpointer              data,
                                                               guint32               size);

HYSCAN_API
gboolean               hyscan_map_tile_set_pixbuf             (HyScanMapTile        *tile,
                                                               GdkPixbuf            *pixbuf);

HYSCAN_API
void                   hyscan_map_tile_set_surface            (HyScanMapTile        *tile,
                                                               cairo_surface_t      *surface);

HYSCAN_API
cairo_surface_t *      hyscan_map_tile_get_surface            (HyScanMapTile        *tile);

HYSCAN_API
gint                   hyscan_map_tile_compare                (HyScanMapTile        *a,
                                                               HyScanMapTile        *b);

HYSCAN_API
void                   hyscan_map_tile_iter_init              (HyScanMapTileIter    *iter,
                                                               guint                 from_x,
                                                               guint                 to_x,
                                                               guint                 from_y,
                                                               guint                 to_y);

HYSCAN_API
gboolean               hyscan_map_tile_iter_next              (HyScanMapTileIter    *iter,
                                                               guint                 *x,
                                                               guint                 *y);

G_END_DECLS

#endif /* __HYSCAN_MAP_TILE_H__ */
