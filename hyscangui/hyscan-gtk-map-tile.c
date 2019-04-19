/* hyscan-gtk-map-tile.c
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

/**
 * SECTION: hyscan-gtk-map-tile
 * @Short_description: класс для работы с тайлами и тайловой сеткой
 * @Title: HyScanGtkMapTile
 *
 * Тайловая сетка — это разбиение некоторой области логической системы координат
 * на равные по размеру квадраты - тайлы. Тайловая сетка может иметь
 * несколько таких разбиений с разной степенью детализации (зумы). При
 * минимальной детализации вся область может покрываться одним тайлом, а при
 * максимальной - тысячами и более.
 *
 * Направление осей логической и тайловой СК
 *
 *   Ось | Логическая СК  | Тайловая СК
 *  +----+----------------+--------------+
 *    OX | слева направо  | слева направо
 *  +----+----------------+--------------+
 *    OY | снизу вверх    | сверху вниз
 *  +----+----------------+--------------+
 *       |  ↑ Y           |  +-----> X
 *       |  |             |  |
 *       |  +-----> X     |  v Y
 *
 * Классы #HyScanGtkMapTile и #HyScanGtkMapTileGrid предназначены для рисования
 * на поверхности #GtkCifroArea, поэтому используют этот виджет для получения
 * пределов логической СК и видимой области.
 *
 * Чтобы создать тайловую сетку #HyScanGtkMapTileGrid необходимо указать ее
 * границы в логической СК и размер тайла в пикселях. Для этого доступны две
 * функции:
 * - hyscan_gtk_map_tile_grid_new(),
 * - hyscan_gtk_map_tile_grid_new_from_cifro().
 *
 * Чтобы установить доступные степени детализации можно использовать одну из
 * двух функций:
 * - hyscan_gtk_map_tile_grid_set_scales(),
 * - hyscan_gtk_map_tile_grid_set_xnums().
 *
 * Каждый тайл #HyScanGtkMapTile однозначно определяется тремя параметрами:
 * координаты x, y и zoom. Для создания тайла используется функция
 * hyscan_gtk_map_tile_new(). Тайл содержит в себе изображение покрываемой им
 * области со степенью детализации zoom. Для установки этого изображения доступны
 * несколько функций:
 * - hyscan_gtk_map_tile_set_surface(),
 * - hyscan_gtk_map_tile_set_surface_data(),
 * - hyscan_gtk_map_tile_set_pixbuf().
 *
 * Для получения изображения - hyscan_gtk_map_tile_get_surface().
 *
 * Класс #HyScanGtkMapTileGrid позволяет переводить координаты из логической
 * системы координат в тайловую и обратно, а также определять покрываемую тайлами
 * область - hyscan_gtk_map_tile_get_bounds() - и, наоборот, какие тайлы
 * необходимы для покрытия указанной области - hyscan_gtk_map_tile_grid_get_view().
 *
 */

#include "hyscan-gtk-map-tile.h"
#include <cairo.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_X,
  PROP_Y,
  PROP_ZOOM,
  PROP_GRID,
};

struct _HyScanGtkMapTileGridPrivate
{
  /* Границы логической СК. */
  gdouble                      min_x;          /* Минимальное значение координаты x. */
  gdouble                      min_y;          /* Минимальное значение координаты y. */
  gdouble                      max_x;          /* Максимальное значение координаты x. */
  gdouble                      max_y;          /* Максимальное значение координаты y. */

  /* Размеры тайла. */
  guint                        tile_size;      /* Размер тайла в пикселях. */
  gdouble                     *scales;         /* Доступные масштабы (размер стороны тайла в логических единицах). */
  gsize                        scales_len;     /* Длина массива scales. */
  guint                        min_zoom;       /* Зум, соответствующий нулевому индексу масштаба. */
};

struct _HyScanGtkMapTilePrivate
{
  guint                        x;              /* Координата x в тайловой СК. */
  guint                        y;              /* Координата y в тайловой СК. */
  guint                        zoom;           /* Зум (порядковый номер масштаба). */

  HyScanGtkMapTileGrid        *grid;           /* Параметры сетки тайла. */

  cairo_surface_t             *surface;        /* Изображение тайла. */
};

static void    hyscan_gtk_map_tile_set_property             (GObject               *object,
                                                             guint                  prop_id,
                                                             const GValue          *value,
                                                             GParamSpec            *pspec);
static void    hyscan_gtk_map_tile_object_finalize          (GObject               *object);
static void    hyscan_gtk_map_tile_grid_object_finalize     (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapTile, hyscan_gtk_map_tile, G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapTileGrid, hyscan_gtk_map_tile_grid, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tile_grid_class_init (HyScanGtkMapTileGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_gtk_map_tile_grid_object_finalize;
}

static void
hyscan_gtk_map_tile_grid_init (HyScanGtkMapTileGrid *gtk_map_tile_grid)
{
  HyScanGtkMapTileGridPrivate *priv;
  priv = gtk_map_tile_grid->priv = hyscan_gtk_map_tile_grid_get_instance_private (gtk_map_tile_grid);

  /* Устанавливаем масштабы по умолчанию. */
  priv->scales_len = 1;
  priv->scales = g_new (gdouble, 1);
  priv->scales[0] = 1;
}

static void
hyscan_gtk_map_tile_class_init (HyScanGtkMapTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tile_set_property;

  object_class->finalize = hyscan_gtk_map_tile_object_finalize;

  g_object_class_install_property (object_class, PROP_GRID,
    g_param_spec_object ("grid", "Grid", "HyScanGtkMapTileGrid", HYSCAN_TYPE_GTK_MAP_TILE_GRID,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_X,
    g_param_spec_uint ("x", "X", "X coordinate", 0, G_MAXUINT32, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_Y,
    g_param_spec_uint ("y", "Y", "Y coordinate", 0, G_MAXUINT32, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_ZOOM,
    g_param_spec_uint ("z", "Z", "Zoom", 0, G_MAXUINT, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tile_init (HyScanGtkMapTile *gtk_map_tile)
{
  gtk_map_tile->priv = hyscan_gtk_map_tile_get_instance_private (gtk_map_tile);
}

static void
hyscan_gtk_map_tile_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  switch (prop_id)
    {
    case PROP_X:
      priv->x = g_value_get_uint (value);
      break;

    case PROP_Y:
      priv->y = g_value_get_uint (value);
      break;

    case PROP_ZOOM:
      priv->zoom = g_value_get_uint (value);
      break;

    case PROP_GRID:
      priv->grid = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tile_grid_object_finalize (GObject *object)
{
  HyScanGtkMapTileGrid *grid = HYSCAN_GTK_MAP_TILE_GRID (object);
  HyScanGtkMapTileGridPrivate *priv = grid->priv;

  g_free (priv->scales);
}

static void
hyscan_gtk_map_tile_object_finalize (GObject *object)
{
  HyScanGtkMapTile *gtk_map_tile = HYSCAN_GTK_MAP_TILE (object);
  HyScanGtkMapTilePrivate *priv = gtk_map_tile->priv;

  g_clear_object (&priv->grid);
  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  G_OBJECT_CLASS (hyscan_gtk_map_tile_parent_class)->finalize (object);
}

/**
 * hyscan_gtk_map_tile_new:
 * @grid: указатель на тайловую сетку #HyScanGtkMapTileGrid
 * @x: координата тайла по x
 * @y: координата тайла по y
 * @z: индекс тайла
 * @size: размер тайла
 *
 * Returns: указатель на #HyScanGtkMapTile. Для удаления g_object_unref().
 */
HyScanGtkMapTile *
hyscan_gtk_map_tile_new (HyScanGtkMapTileGrid *grid,
                         guint                 x,
                         guint                 y,
                         guint                 z)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILE,
                       "grid", grid,
                       "x", x,
                       "y", y,
                       "z", z, NULL);
}

/**
 * hyscan_gtk_map_tile_grid_new_from_cifro:
 * @carea: указатель на #GtkCifroArea
 * @min_zoom: зум, соответствующий минимальному масштабу
 * @tile_size: размер тайла в пикселях
 *
 * Создает тайловую сетку по границам @carea с указанным размером тайла.
 *
 * Returns: указатель на #HyScanGtkMapTileGrid. Для удаления g_object_unref()
 */
HyScanGtkMapTileGrid *
hyscan_gtk_map_tile_grid_new_from_cifro (GtkCifroArea  *carea,
                                         guint          min_zoom,
                                         guint          tile_size)
{
  HyScanGtkMapTileGrid *grid;
  HyScanGtkMapTileGridPrivate *priv;

  grid = g_object_new (HYSCAN_TYPE_GTK_MAP_TILE_GRID, NULL);
  priv = grid->priv;

  gtk_cifro_area_get_limits (carea, &priv->min_x, &priv->max_x, &priv->min_y, &priv->max_y);

  priv->min_zoom = min_zoom;
  priv->tile_size = tile_size;

  return grid;
}

/**
 * hyscan_gtk_map_tile_grid_new:
 * @min_x: минимальное значение X области логической СК
 * @max_x: максимальное значение X области логической СК
 * @min_y: минимальное значение Y области логической СК
 * @max_y: максимальное значение Y области логической СК
 * @min_zoom: зум, соответствующий минимальному масштабу
 * @tile_size: размер тайла в пикселях
 *
 * Returns: указатель на #HyScanGtkMapTileGrid. Для удаления g_object_unref()
 */
HyScanGtkMapTileGrid *
hyscan_gtk_map_tile_grid_new (gdouble min_x,
                              gdouble max_x,
                              gdouble min_y,
                              gdouble max_y,
                              guint   min_zoom,
                              guint   tile_size)
{
  HyScanGtkMapTileGrid *grid;
  HyScanGtkMapTileGridPrivate *priv;

  grid = g_object_new (HYSCAN_TYPE_GTK_MAP_TILE_GRID, NULL);
  priv = grid->priv;

  priv->min_x = min_x;
  priv->max_x = max_x;
  priv->min_y = min_y;
  priv->max_y = max_y;
  priv->min_zoom = min_zoom;
  priv->tile_size = tile_size;

  return grid;
}

/**
 * hyscan_gtk_map_tile_grid_set_xnums:
 * @grid: указатель на #HyScanGtkMapTileGrid
 * @xnums: (array length=xnums_len): варианты масштабов: число тайлов вдоль оси X
 * @xnums_len: количество вариантов масштабов
 *
 * Устанавливает масштабы тайловой сетки в формате числа тайлов вдоль оси X.
 */
void
hyscan_gtk_map_tile_grid_set_xnums (HyScanGtkMapTileGrid *grid,
                                    const gdouble        *xnums,
                                    gsize                 xnums_len)
{
  HyScanGtkMapTileGridPrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE_GRID (grid));
  priv = grid->priv;

  g_free (priv->scales);
  priv->scales = g_new (gdouble, xnums_len);

  for (i = 0; i < xnums_len; ++i)
    priv->scales[i] = (priv->max_x - priv->min_x) / xnums[i];
}

/**
 * hyscan_gtk_map_tile_grid_set_scales:
 * @grid: указатель на #HyScanGtkMapTileGrid
 * @scales: (array length=scales_len): варианты масштабов: длина стороны тайла
 *    в логических единицах
 * @scales_len: количество масштабов
 *
 * Устанавливает масштабы тайловой сетки в формате длины стороны тайла в
 * логических единицах.
 */
void
hyscan_gtk_map_tile_grid_set_scales (HyScanGtkMapTileGrid *grid,
                                     const gdouble        *scales,
                                     gsize                 scales_len)
{
  HyScanGtkMapTileGridPrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE_GRID (grid));
  priv = grid->priv;

  g_free (priv->scales);

  priv->scales = g_new (gdouble, scales_len);

  for (i = 0; i < scales_len; ++i)
    priv->scales[i] = scales[i];
}

/**
 * hyscan_gtk_map_tile_grid_get_scale:
 * @param grid
 *
 * Returns: размер одного пикселя тайла в логических единицах
 */
gdouble
hyscan_gtk_map_tile_grid_get_scale (HyScanGtkMapTileGrid *grid,
                                    guint                 zoom)
{
  HyScanGtkMapTileGridPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_GRID (grid), -1);

  priv = grid->priv;

  return priv->scales[zoom - priv->min_zoom] / priv->tile_size;
}

/**
 * hyscan_gtk_map_tile_get_x:
 * @tile
 *
 * Returns: координата x тайла.
 */
guint
hyscan_gtk_map_tile_get_x (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->x;
}

/**
 * hyscan_gtk_map_tile_get_y:
 * @tile
 *
 * Returns: координата y тайла.
 */
guint
hyscan_gtk_map_tile_get_y (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->y;
}

/**
 * hyscan_gtk_map_tile_get_bounds:
 * @tile: указатель на #HyScanGtkMapTile
 * @from_x: координата x верхнего правого угла
 * @to_x: координата x нижнего правого угла
 * @from_y: координата y верхнего левого угла
 * @to_y: координата y нижнего правого угла
 *
 * Определяет область в логических координатах, которую покрывает тайл @tile.
 */
void
hyscan_gtk_map_tile_get_bounds (HyScanGtkMapTile *tile,
                                gdouble          *from_x,
                                gdouble          *to_x,
                                gdouble          *from_y,
                                gdouble          *to_y)
{
  HyScanGtkMapTilePrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile));

  priv = tile->priv;

  hyscan_gtk_map_tile_grid_tile_to_value (priv->grid, priv->zoom, priv->x, priv->y, from_x, from_y);
  hyscan_gtk_map_tile_grid_tile_to_value (priv->grid, priv->zoom, priv->x + 1, priv->y + 1, to_x, to_y);
}

/**
 * hyscan_gtk_map_tile_get_zoom:
 * @tile:
 *
 * Returns: номер масштаба тайла
 */
guint
hyscan_gtk_map_tile_get_zoom (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);

  return tile->priv->zoom;
}

/**
 * hyscan_gtk_map_tile_get_size:
 * @tile: указатель на #HyScanGtkMapTile
 *
 * Returns: размер стороны тайла в пикселях.
 */
guint
hyscan_gtk_map_tile_get_size (HyScanGtkMapTile *tile)
{
  HyScanGtkMapTileGrid *grid;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), 0);
  grid = tile->priv->grid;

  return grid->priv->tile_size;
}

/**
 * hyscan_gtk_map_tile_get_scale:
 * @tile: указатель на #HyScanGtkMapTile
 *
 * Returns: масштаб тайла или -1.0 в случае ошибки
 */
gdouble
hyscan_gtk_map_tile_get_scale (HyScanGtkMapTile *tile)
{
  HyScanGtkMapTilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), -1.0);
  priv = tile->priv;

  return hyscan_gtk_map_tile_grid_get_scale (priv->grid, priv->zoom);
}

/**
 * hyscan_gtk_map_tile_set_surface_data:
 * @tile
 * @data
 * @size
 *
 * Устанавливает поверхность данные @data в качестве поверхности тайла.
 *
 * Область памяти @data должна существовать, пока не будет установлена другая
 * поверхность или пока тайл не будет уничтожен. Поэтому после использования
 * поверхности рекомендуется установить %NULL в hyscan_gtk_map_tile_set_surface().
 */
void
hyscan_gtk_map_tile_set_surface_data (HyScanGtkMapTile *tile,
                                      gpointer          data,
                                      guint32           size)
{
  cairo_surface_t *surface;
  gint stride;
  guint tile_size;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile));

  tile_size = hyscan_gtk_map_tile_get_size (tile);
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, tile_size);
  g_return_if_fail (size == tile_size * stride);

  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, tile_size, tile_size, stride);
  hyscan_gtk_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
}

/**
 * hyscan_gtk_map_tile_set_pixbuf:
 * @tile
 * @pixbuf
 *
 * Устанавливает изображение @pixbuf на поверхность тайла.
 *
 * Returns: %TRUE в случае успеха.
 */
gboolean 
hyscan_gtk_map_tile_set_pixbuf (HyScanGtkMapTile *tile,
                                GdkPixbuf        *pixbuf)
{
  cairo_t *cairo;
  cairo_surface_t *surface;
  guint tile_size;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), FALSE);
  g_return_val_if_fail (pixbuf != NULL, FALSE);

  tile_size = hyscan_gtk_map_tile_get_size (tile);

  if (gdk_pixbuf_get_width (pixbuf) != (gint) tile_size ||
      gdk_pixbuf_get_height (pixbuf) != (gint) tile_size)
    {
      return FALSE;
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile_size, tile_size);
  cairo = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cairo, pixbuf, 0, 0);
  cairo_paint (cairo);

  hyscan_gtk_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cairo);

  return TRUE;
}

/**
 * hyscan_gtk_map_tile_set_surface:
 * @tile:
 * @surface: (nullable):
 *
 * Устанавливает поверхность тайла.
 */
void
hyscan_gtk_map_tile_set_surface (HyScanGtkMapTile *tile,
                                 cairo_surface_t  *surface)
{
  HyScanGtkMapTilePrivate *priv;
  guint tile_size;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile));

  priv = tile->priv;

  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  if (surface == NULL)
    return;

  tile_size = hyscan_gtk_map_tile_get_size (tile);
  if (cairo_image_surface_get_width (surface) == (gint) tile_size ||
      cairo_image_surface_get_height (surface) == (gint) tile_size)
    {
      priv->surface = cairo_surface_reference (surface);
    }
}

/**
 * hyscan_gtk_map_tile_get_surface:
 * @tile
 *
 * Returns: (nullable): поверхность тайла
 */
cairo_surface_t *
hyscan_gtk_map_tile_get_surface (HyScanGtkMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (tile), NULL);

  if (tile->priv->surface == NULL)
    return NULL;

  return cairo_surface_reference (tile->priv->surface);
}

/**
 * hyscan_gtk_map_tile_compare:
 * @a
 * @b
 *
 * Сравнивает тайлы a и b.
 *
 * Returns: Возвращает 0, если тайлы одинаковые.
 */
gint
hyscan_gtk_map_tile_compare (HyScanGtkMapTile *a,
                             HyScanGtkMapTile *b)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE (a) && HYSCAN_IS_GTK_MAP_TILE (b), 0);

  if (a->priv->x == b->priv->x && a->priv->y == b->priv->y && a->priv->zoom == b->priv->zoom)
    return 0;
  else
    return 1;
}

/**
 * hyscan_gtk_map_tile_grid_get_view:
 * @grid: указатель на #HyScanGtkMapTileGrid
 * @carea: указатель на #GtkCifroArea
 * @from_tile_x: (nullable): минимальное значение координаты x тайла
 * @to_tile_x: (nullable): максимальное значение координаты x тайла
 * @from_tile_y: (nullable): минимальное значение координаты y тайла
 * @to_tile_y: (nullable): максимальное значение координаты y тайла
 *
 * Определяет координаты тайлов сетки @grid, полностью покрывающих область @region.
 */
void
hyscan_gtk_map_tile_grid_get_view (HyScanGtkMapTileGrid *grid,
                                   GtkCifroArea         *carea,
                                   guint                 zoom,
                                   gint                 *from_tile_x,
                                   gint                 *to_tile_x,
                                   gint                 *from_tile_y,
                                   gint                 *to_tile_y)
{
  gdouble from_x, from_y, to_x, to_y;
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  /* Получаем координаты тайлов, соответствующие границам запрошенной области. */
  hyscan_gtk_map_tile_grid_value_to_tile (grid, zoom, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_gtk_map_tile_grid_value_to_tile (grid, zoom, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  (to_tile_y != NULL) ? *to_tile_y = (gint) MAX (from_tile_y_d, to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = (gint) MIN (from_tile_y_d, to_tile_y_d) : 0;

  (to_tile_x != NULL) ? *to_tile_x = (gint) MAX (from_tile_x_d, to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = (gint) MIN (from_tile_x_d, to_tile_x_d) : 0;
}

/**
 * hyscan_gtk_map_tile_grid_tile_to_value:
 * @grid: указатель на #HyScanGtkMapTileGrid
 * @zoom: индекс масштаба
 * @x_tile: координата x в тайловой СК
 * @y_tile: координата y в тайловой СК
 * @x_val: (nullable): координата x в логической СК
 * @y_val: (nullable): координата y в логической СК
 *
 * Переводит координаты из тайловой СК в логическую.
 */
void
hyscan_gtk_map_tile_grid_tile_to_value (HyScanGtkMapTileGrid *grid,
                                        guint                 zoom,
                                        gdouble               x_tile,
                                        gdouble               y_tile,
                                        gdouble              *x_val,
                                        gdouble              *y_val)
{
  HyScanGtkMapTileGridPrivate *priv;
  gdouble scale;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE_GRID (grid));
  priv = grid->priv;

  scale = priv->scales[zoom-priv->min_zoom];
  if (x_val != NULL)
    *x_val = priv->min_x + x_tile * scale;

  if (y_val != NULL)
    *y_val = priv->max_y - y_tile * scale;
}

/**
 * hyscan_gtk_map_tile_grid_tile_to_value:
 * @grid: указатель на #HyScanGtkMapTileGrid
 * @x_val: координата x в логической СК
 * @y_val: координата y в логической СК
 * @x_tile: (nullable): координата x в тайловой СК
 * @y_tile: (nullable): координата y в тайловой СК
 *
 * Переводит координаты из логической СК в тайловую.
 */
void
hyscan_gtk_map_tile_grid_value_to_tile (HyScanGtkMapTileGrid *grid,
                                        guint                 zoom,
                                        gdouble               x_val,
                                        gdouble               y_val,
                                        gdouble              *x_tile,
                                        gdouble              *y_tile)
{
  HyScanGtkMapTileGridPrivate *priv;
  gdouble tile_size_x;
  gdouble tile_size_y;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE_GRID (grid));
  priv = grid->priv;

  /* Размер тайла в логических единицах. */
  tile_size_x = priv->scales[zoom - priv->min_zoom];
  tile_size_y = priv->scales[zoom - priv->min_zoom];

  (y_tile != NULL) ? *y_tile = (priv->max_y - y_val) / tile_size_y : 0;
  (x_tile != NULL) ? *x_tile = (x_val - priv->min_x) / tile_size_x : 0;
}
