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
 * @Short_description: классы для работы с тайлами и тайловой сеткой
 * @Title: HyScanMapTile
 *
 * Тайловая сетка — это разбиение некоторой области логической системы координат
 * на равные по размеру квадраты - тайлы. Тайловая сетка может иметь
 * несколько таких разбиений с разной степенью детализации (зумы). При
 * минимальной детализации вся область может покрываться одним тайлом, а при
 * максимальной - тысячами и более.
 *
 * Направление осей логической и тайловой СК различаются:
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
 * Функции для создания тайловой сетки:
 * - hyscan_map_tile_grid_new() - границы логической СК передаются в виде
 *   аргументов,
 * - hyscan_map_tile_grid_new_from_cifro() - границы логической СК берутся из
 *   виджета #GtkCifroArea
 *
 * Кроме того при инициализации тайловой сетки следует установить доступные
 * степени детализации (число зумов) с помощью одной из двух функций:
 * - hyscan_map_tile_grid_set_scales(),
 * - hyscan_map_tile_grid_set_xnums().
 *
 * Прочие функции тайловой сетки:
 * - hyscan_map_tile_grid_get_zoom_range() - диапазон доступных зумов,
 * - hyscan_map_tile_grid_get_scale() - масштаб, соответствующий указанному зуму,
 * - hyscan_map_tile_grid_adjust_zoom() - наиболее подходящий зум для указанного масштаба,
 * - hyscan_map_tile_grid_get_view() - координаты тайлов, полность покрывающих указанную
 *   область в логической СК
 * - hyscan_map_tile_grid_get_view_cifro() - координаты тайлов, полностью покрывающих
 *   видимую область виджета #GtkCifroArea
 *
 * Каждый тайл #HyScanMapTile однозначно определяется тремя параметрами:
 * координаты x, y и zoom. Для создания тайла используется функция:
 * - hyscan_map_tile_new().
 *
 * Тайл содержит в себе изображение покрываемой им области со степенью
 * детализации zoom. Для установки этого изображения доступны несколько функций:
 * - hyscan_map_tile_set_surface(),
 * - hyscan_map_tile_set_surface_data(),
 * - hyscan_map_tile_set_pixbuf().
 *
 * Для получения изображения используется функция:
 * - hyscan_map_tile_get_surface().
 *
 * Чтобы обойти тайлы, покрывающие некотороую область, можно использовать
 * итератор #HyScanMapTileIter. Для этого доступны функции:
 * - hyscan_map_tile_iter_init(),
 * - hyscan_map_tile_iter_next().
 *
 * Класс #HyScanMapTileGrid позволяет переводить координаты из логической
 * системы координат в тайловую и обратно, а также определять покрываемую тайлами
 * область - hyscan_map_tile_get_bounds() - и, наоборот, какие тайлы
 * необходимы для покрытия указанной области - hyscan_map_tile_grid_get_view().
 *
 */

#include "hyscan-map-tile.h"
#include <cairo.h>
#include <string.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_X,
  PROP_Y,
  PROP_ZOOM,
  PROP_GRID,
};

struct _HyScanMapTileGridPrivate
{
  /* Параметры логической СК. */
  gboolean                     invert_ox;      /* Признак того, что ось OX направлена влево. */
  gboolean                     invert_oy;      /* Признак того, что ось OY направлена вниз. */
  gdouble                      min_x;          /* Минимальное значение координаты x. */
  gdouble                      min_y;          /* Минимальное значение координаты y. */
  gdouble                      max_x;          /* Максимальное значение координаты x. */
  gdouble                      max_y;          /* Максимальное значение координаты y. */

  /* Размеры тайла. */
  guint                        tile_size;      /* Размер тайла в пикселях. */
  gdouble                     *scales;         /* Доступные масштабы (размер стороны тайла в логических единицах). */
  guint                       *xnums;          /* Количество тайлов для каждого масштаба. */
  gsize                        scales_len;     /* Длина массива scales и xnums. */
  guint                        min_zoom;       /* Зум, соответствующий нулевому индексу масштаба. */
};

struct _HyScanMapTilePrivate
{
  guint                        x;              /* Координата x в тайловой СК. */
  guint                        y;              /* Координата y в тайловой СК. */
  guint                        zoom;           /* Зум (порядковый номер масштаба). */

  HyScanMapTileGrid           *grid;           /* Параметры сетки тайла. */

  cairo_surface_t             *surface;        /* Изображение тайла. */
};

static void    hyscan_map_tile_set_property             (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void    hyscan_map_tile_object_finalize          (GObject               *object);
static void    hyscan_map_tile_grid_object_finalize     (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapTile, hyscan_map_tile, G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapTileGrid, hyscan_map_tile_grid, G_TYPE_OBJECT)

static void
hyscan_map_tile_grid_class_init (HyScanMapTileGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_map_tile_grid_object_finalize;
}

static void
hyscan_map_tile_grid_init (HyScanMapTileGrid *gtk_map_tile_grid)
{
  HyScanMapTileGridPrivate *priv;
  priv = gtk_map_tile_grid->priv = hyscan_map_tile_grid_get_instance_private (gtk_map_tile_grid);

  /* Устанавливаем масштабы по умолчанию. */
  priv->scales_len = 1;
  priv->scales = g_new (gdouble, 1);
  priv->scales[0] = 1;

  /* Устанавливаем направления осей координат по умолчанию. */
  priv->invert_ox = FALSE;
  priv->invert_oy = TRUE;
}

static void
hyscan_map_tile_class_init (HyScanMapTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_tile_set_property;

  object_class->finalize = hyscan_map_tile_object_finalize;

  g_object_class_install_property (object_class, PROP_GRID,
    g_param_spec_object ("grid", "Grid", "HyScanMapTileGrid", HYSCAN_TYPE_MAP_TILE_GRID,
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
hyscan_map_tile_init (HyScanMapTile *gtk_map_tile)
{
  gtk_map_tile->priv = hyscan_map_tile_get_instance_private (gtk_map_tile);
}

static void
hyscan_map_tile_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HyScanMapTile *gtk_map_tile = HYSCAN_MAP_TILE (object);
  HyScanMapTilePrivate *priv = gtk_map_tile->priv;

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
hyscan_map_tile_grid_object_finalize (GObject *object)
{
  HyScanMapTileGrid *grid = HYSCAN_MAP_TILE_GRID (object);
  HyScanMapTileGridPrivate *priv = grid->priv;

  g_free (priv->scales);
  g_free (priv->xnums);
}

static void
hyscan_map_tile_object_finalize (GObject *object)
{
  HyScanMapTile *gtk_map_tile = HYSCAN_MAP_TILE (object);
  HyScanMapTilePrivate *priv = gtk_map_tile->priv;

  g_clear_object (&priv->grid);
  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  G_OBJECT_CLASS (hyscan_map_tile_parent_class)->finalize (object);
}

/* Возвращает количество тайлов вдоль оси X на масштабе zoom. */
static guint
hyscan_map_tile_grid_get_xnums (HyScanMapTileGrid *grid,
                                guint              zoom)
{
  HyScanMapTileGridPrivate *priv;
  gint index;

  priv = grid->priv;
  index = zoom - priv->min_zoom;

  return priv->xnums[index];
}

/**
 * hyscan_map_tile_new:
 * @grid: указатель на тайловую сетку #HyScanMapTileGrid
 * @x: координата тайла по x
 * @y: координата тайла по y
 * @z: индекс тайла
 * @size: размер тайла
 *
 * Returns: указатель на #HyScanMapTile. Для удаления g_object_unref().
 */
HyScanMapTile *
hyscan_map_tile_new (HyScanMapTileGrid *grid,
                     guint              x,
                     guint              y,
                     guint              z)
{
  return g_object_new (HYSCAN_TYPE_MAP_TILE,
                       "grid", grid,
                       "x", x,
                       "y", y,
                       "z", z, NULL);
}

/**
 * hyscan_map_tile_grid_new_from_cifro:
 * @carea: указатель на #GtkCifroArea
 * @min_zoom: зум, соответствующий минимальному масштабу
 * @tile_size: размер тайла в пикселях
 *
 * Создает тайловую сетку по границам @carea с указанным размером тайла.
 *
 * Returns: указатель на #HyScanMapTileGrid. Для удаления g_object_unref()
 */
HyScanMapTileGrid *
hyscan_map_tile_grid_new_from_cifro (GtkCifroArea  *carea,
                                     guint          min_zoom,
                                     guint          tile_size)
{
  HyScanMapTileGrid *grid;
  HyScanMapTileGridPrivate *priv;

  grid = g_object_new (HYSCAN_TYPE_MAP_TILE_GRID, NULL);
  priv = grid->priv;

  gtk_cifro_area_get_limits (carea, &priv->min_x, &priv->max_x, &priv->min_y, &priv->max_y);

  priv->min_zoom = min_zoom;
  priv->tile_size = tile_size;

  return grid;
}

/**
 * hyscan_map_tile_grid_new:
 * @min_x: минимальное значение X области логической СК
 * @max_x: максимальное значение X области логической СК
 * @min_y: минимальное значение Y области логической СК
 * @max_y: максимальное значение Y области логической СК
 * @min_zoom: зум, соответствующий минимальному масштабу
 * @tile_size: размер тайла в пикселях
 *
 * Returns: указатель на #HyScanMapTileGrid. Для удаления g_object_unref()
 */
HyScanMapTileGrid *
hyscan_map_tile_grid_new (gdouble min_x,
                          gdouble max_x,
                          gdouble min_y,
                          gdouble max_y,
                          guint   min_zoom,
                          guint   tile_size)
{
  HyScanMapTileGrid *grid;
  HyScanMapTileGridPrivate *priv;

  grid = g_object_new (HYSCAN_TYPE_MAP_TILE_GRID, NULL);
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
 * hyscan_map_tile_grid_set_xnums:
 * @grid: указатель на #HyScanMapTileGrid
 * @xnums: (array length=xnums_len): варианты масштабов: число тайлов вдоль оси X
 * @xnums_len: количество вариантов масштабов
 *
 * Устанавливает масштабы тайловой сетки в формате числа тайлов вдоль оси X.
 */
void
hyscan_map_tile_grid_set_xnums (HyScanMapTileGrid *grid,
                                const guint       *xnums,
                                gsize              xnums_len)
{
  HyScanMapTileGridPrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));
  priv = grid->priv;

  g_free (priv->scales);
  g_free (priv->xnums);

  priv->scales = g_new (gdouble, xnums_len);
  priv->xnums = g_new (guint , xnums_len);
  priv->scales_len = xnums_len;

  for (i = 0; i < xnums_len; ++i)
    {
      priv->scales[i] = (priv->max_x - priv->min_x) / xnums[i];
      priv->xnums[i] = xnums[i];
    }
}

/**
 * hyscan_map_tile_grid_set_scales:
 * @grid: указатель на #HyScanMapTileGrid
 * @scales: (array length=scales_len): варианты масштабов: длина стороны тайла
 *    в логических единицах
 * @scales_len: количество масштабов
 *
 * Устанавливает масштабы тайловой сетки. Каждый элемент массива @scales показвает
 * длину стороны тайла в логических единицах на соответствующему уровне детализации.
 */
void
hyscan_map_tile_grid_set_scales (HyScanMapTileGrid *grid,
                                 const gdouble     *scales,
                                 gsize              scales_len)
{
  HyScanMapTileGridPrivate *priv;
  gsize i;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));
  priv = grid->priv;

  g_free (priv->scales);
  g_free (priv->xnums);

  priv->scales = g_new (gdouble, scales_len);
  priv->xnums = g_new (guint , scales_len);
  priv->scales_len = scales_len;

  for (i = 0; i < scales_len; ++i)
    {
      priv->scales[i] = scales[i];
      priv->xnums[i] = lround ((priv->max_x - priv->min_x) / scales[i]);
    }
}

/**
 * hyscan_map_tile_grid_get_scale:
 * @grid: указатель на тайловую сетку #HyScanMapTileGrid
 *
 * Возвращает размер одного пикселя в логической системе координат.
 *
 * Returns: размер одного пикселя тайла в логических единицах
 */
gdouble
hyscan_map_tile_grid_get_scale (HyScanMapTileGrid *grid,
                                guint              zoom)
{
  HyScanMapTileGridPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid), -1);

  priv = grid->priv;

  return priv->scales[zoom - priv->min_zoom] / priv->tile_size;
}

/**
 * hyscan_map_tile_grid_get_zoom_range:
 * @grid: указатель на тайловую сетку #HyScanMapTileGrid
 * @min_zoom: (out) (nullable): минимальный номер масштаба
 * @max_zoom: (out) (nullable): максимальный номер масштаба
 *
 * Возвращает диапазон степеней детализаци тайловой сетки: номер минимального
 * и номер максимального масштабов.
 */
void
hyscan_map_tile_grid_get_zoom_range (HyScanMapTileGrid *grid,
                                     guint             *min_zoom,
                                     guint             *max_zoom)
{
  HyScanMapTileGridPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));

  priv = grid->priv;

  g_return_if_fail (priv->scales_len > 0);

  (min_zoom != NULL) ? *min_zoom = priv->min_zoom : 0;
  (max_zoom != NULL) ? *max_zoom = priv->min_zoom + priv->scales_len - 1 : 0;
}

/**
 * hyscan_map_tile_grid_adjust_zoom:
 * @grid: указатель на тайловую сетку #HyScanMapTileGrid
 * @scale: требуемый масштаб
 *
 * Выбирает допустимый масштаб, наиболее подходящий по значению к @scale, и
 * возвращает его номер.
 *
 * Returns: номер масштаба, наиболее подходящий для заданного масштаба @scale.
 */
guint
hyscan_map_tile_grid_adjust_zoom (HyScanMapTileGrid *grid,
                                  gdouble            scale)
{
  HyScanMapTileGridPrivate *priv;

  guint optimal_zoom;
  gdouble optimal_err = G_MAXDOUBLE;

  gsize i;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid), 0);
  priv = grid->priv;

  optimal_zoom = priv->min_zoom;
  for (i = 0; i < priv->scales_len; ++i)
    {
      gdouble tile_px;
      gdouble err;

      /* Размер тайла в пиксела  при масштабе i. */
      tile_px = scale * priv->scales[i];
      err = fabs (tile_px - priv->tile_size);
      if (err < optimal_err)
        {
          optimal_zoom = priv->min_zoom + i;
          optimal_err = err;
        }
    }

  return optimal_zoom;
}

/**
 * hyscan_map_tile_get_x:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: координата x тайла.
 */
guint
hyscan_map_tile_get_x (HyScanMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), 0);

  return tile->priv->x;
}

/**
 * hyscan_map_tile_get_y:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: координата y тайла.
 */
guint
hyscan_map_tile_get_y (HyScanMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), 0);

  return tile->priv->y;
}

/**
 * hyscan_map_tile_inv_y:
 * @tile: указатель на #HyScanMapTile
 *
 * Возвращает координату тайла по оси OY, направленной в обратную сторону:
 * inv_y = max_y - y. Функция полезна в случаях, когда отсчёт тайлов идёт не
 * сверху вниз, а наоборот.
 *
 * Returns: возвращает координату Y тайла в системе координат с инвертированной
 *          осью OY.
 */
guint
hyscan_map_tile_inv_y (HyScanMapTile *tile)
{
  HyScanMapTilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), 0);
  priv= tile->priv;

  return hyscan_map_tile_grid_get_xnums (priv->grid, priv->zoom) - 1 - priv->y;
}

/**
 * hyscan_map_tile_get_bounds:
 * @tile: указатель на #HyScanMapTile
 * @from: координаты верхнего правого угла
 * @to: координаты нижнего правого угла
 *
 * Определяет область в логических координатах, которую покрывает тайл @tile.
 */
void
hyscan_map_tile_get_bounds (HyScanMapTile        *tile,
                            HyScanGeoCartesian2D *from,
                            HyScanGeoCartesian2D *to)
{
  HyScanMapTilePrivate *priv;
  g_return_if_fail (HYSCAN_IS_MAP_TILE (tile));

  priv = tile->priv;

  if (from != NULL)
    hyscan_map_tile_grid_tile_to_value (priv->grid, priv->zoom, priv->x, priv->y, &from->x, &from->y);

  if (to != NULL)
    hyscan_map_tile_grid_tile_to_value (priv->grid, priv->zoom, priv->x + 1, priv->y + 1, &to->x, &to->y);
}

/**
 * hyscan_map_tile_get_zoom:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: номер масштаба тайла
 */
guint
hyscan_map_tile_get_zoom (HyScanMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), 0);

  return tile->priv->zoom;
}

/**
 * hyscan_map_tile_get_size:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: длина стороны тайла в пикселях.
 */
guint
hyscan_map_tile_get_size (HyScanMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), 0);

  return hyscan_map_tile_grid_get_tile_size (tile->priv->grid);
}

/**
 * hyscan_map_tile_get_scale:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: масштаб тайла или -1.0 в случае ошибки
 */
gdouble
hyscan_map_tile_get_scale (HyScanMapTile *tile)
{
  HyScanMapTilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), -1.0);
  priv = tile->priv;

  return hyscan_map_tile_grid_get_scale (priv->grid, priv->zoom);
}

/**
 * hyscan_map_tile_set_surface_data:
 * @tile: указатель на #HyScanMapTile
 * @data: пиксельные данные
 * @size: размер @data
 *
 * Устанавливает данные @data в качестве поверхности тайла.
 *
 * Область памяти @data должна существовать, пока не будет установлена другая
 * поверхность или пока тайл не будет уничтожен. Поэтому после использования
 * поверхности рекомендуется установить %NULL в hyscan_map_tile_set_surface().
 */
void
hyscan_map_tile_set_surface_data (HyScanMapTile *tile,
                                  gpointer       data,
                                  guint32        size)
{
  cairo_surface_t *surface;
  gint stride;
  guint tile_size;

  g_return_if_fail (HYSCAN_IS_MAP_TILE (tile));

  tile_size = hyscan_map_tile_get_size (tile);
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, tile_size);
  g_return_if_fail (size == tile_size * stride);

  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, tile_size, tile_size, stride);
  hyscan_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
}

/**
 * hyscan_map_tile_set_pixbuf:
 * @tile: указатель на #HyScanMapTile
 * @pixbuf: изображение тайла
 *
 * Устанавливает изображение @pixbuf на поверхность тайла.
 *
 * Returns: %TRUE в случае успеха.
 */
gboolean 
hyscan_map_tile_set_pixbuf (HyScanMapTile *tile,
                            GdkPixbuf     *pixbuf)
{
  cairo_t *cairo;
  cairo_surface_t *surface;
  guint tile_size;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), FALSE);
  g_return_val_if_fail (pixbuf != NULL, FALSE);

  tile_size = hyscan_map_tile_get_size (tile);

  if (gdk_pixbuf_get_width (pixbuf) != (gint) tile_size ||
      gdk_pixbuf_get_height (pixbuf) != (gint) tile_size)
    {
      return FALSE;
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile_size, tile_size);
  cairo = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cairo, pixbuf, 0, 0);
  cairo_paint (cairo);

  hyscan_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cairo);

  return TRUE;
}

/**
 * hyscan_map_tile_set_surface:
 * @tile: указатель на #HyScanMapTile
 * @surface: (nullable): поверхность с изображением тайла
 *
 * Устанавливает поверхность тайла. Если передан %NULL, то поверхность удаляется.
 */
void
hyscan_map_tile_set_surface (HyScanMapTile   *tile,
                             cairo_surface_t *surface)
{
  HyScanMapTilePrivate *priv;
  guint tile_size;

  g_return_if_fail (HYSCAN_IS_MAP_TILE (tile));

  priv = tile->priv;

  g_clear_pointer (&priv->surface, cairo_surface_destroy);

  if (surface == NULL)
    return;

  tile_size = hyscan_map_tile_get_size (tile);
  if (cairo_image_surface_get_width (surface) == (gint) tile_size ||
      cairo_image_surface_get_height (surface) == (gint) tile_size)
    {
      priv->surface = cairo_surface_reference (surface);
    }
}

/**
 * hyscan_map_tile_get_surface:
 * @tile: указатель на #HyScanMapTile
 *
 * Returns: (nullable): поверхность тайла
 */
cairo_surface_t *
hyscan_map_tile_get_surface (HyScanMapTile *tile)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (tile), NULL);

  if (tile->priv->surface == NULL)
    return NULL;

  return cairo_surface_reference (tile->priv->surface);
}

/**
 * hyscan_map_tile_compare:
 * @a: указатель на #HyScanMapTile
 * @b: указатель на #HyScanMapTile
 *
 * Сравнивает тайлы @a и @b. Тайлы считаются одинаковыми, если их координаты
 * и уровень детализации совпадают.
 *
 * Returns: Возвращает 0, если тайлы одинаковые.
 */
gint
hyscan_map_tile_compare (HyScanMapTile *a,
                         HyScanMapTile *b)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE (a) && HYSCAN_IS_MAP_TILE (b), 0);

  if (a->priv->x == b->priv->x && a->priv->y == b->priv->y && a->priv->zoom == b->priv->zoom)
    return 0;
  else
    return 1;
}

/**
 * hyscan_map_tile_grid_get_view:
 * @grid: указатель на #HyScanMapTileGrid
 * @zoom: номер масштаба
 * @from_x: минимальное значение координаты x интересующей области
 * @to_x: минимальное значение координаты x интересующей области
 * @from_y: минимальное значение координаты x интересующей области
 * @to_y: максимально значение координаты y интересующей области
 * @from_tile_x: (nullable): минимальное значение координаты x тайла
 * @to_tile_x: (nullable): максимальное значение координаты x тайла
 * @from_tile_y: (nullable): минимальное значение координаты y тайла
 * @to_tile_y: (nullable): максимальное значение координаты y тайла
 *
 * Определяет координаты тайлов сетки @grid, полностью покрывающих интересующую область.
 */
void
hyscan_map_tile_grid_get_view (HyScanMapTileGrid *grid,
                               guint              zoom,
                               gdouble            from_x,
                               gdouble            to_x,
                               gdouble            from_y,
                               gdouble            to_y,
                               gint              *from_tile_x,
                               gint              *to_tile_x,
                               gint              *from_tile_y,
                               gint              *to_tile_y)
{
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));

  /* Получаем координаты тайлов, соответствующие границам запрошенной области. */
  hyscan_map_tile_grid_value_to_tile (grid, zoom, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_map_tile_grid_value_to_tile (grid, zoom, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  (to_tile_y != NULL) ? *to_tile_y = (gint) MAX (from_tile_y_d, to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = (gint) MIN (from_tile_y_d, to_tile_y_d) : 0;

  (to_tile_x != NULL) ? *to_tile_x = (gint) MAX (from_tile_x_d, to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = (gint) MIN (from_tile_x_d, to_tile_x_d) : 0;
}

/**
 * hyscan_map_tile_grid_get_view_cifro:
 * @grid: указатель на #HyScanMapTileGrid
 * @carea: указатель на #GtkCifroArea
 * @from_tile_x: (nullable): минимальное значение координаты x тайла
 * @to_tile_x: (nullable): максимальное значение координаты x тайла
 * @from_tile_y: (nullable): минимальное значение координаты y тайла
 * @to_tile_y: (nullable): максимальное значение координаты y тайла
 *
 * Определяет координаты тайлов сетки @grid, полностью покрывающих видимую область @carea.
 */
void
hyscan_map_tile_grid_get_view_cifro (HyScanMapTileGrid *grid,
                                     GtkCifroArea      *carea,
                                     guint              zoom,
                                     gint              *from_tile_x,
                                     gint              *to_tile_x,
                                     gint              *from_tile_y,
                                     gint              *to_tile_y)
{
  gdouble from_x, from_y, to_x, to_y;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  hyscan_map_tile_grid_get_view (grid, zoom, from_x, to_x, from_y, to_y,
                                 from_tile_x, to_tile_x, from_tile_y, to_tile_y);
}

/**
 * hyscan_map_tile_grid_get_tile_size:
 * @grid: указатель на #HyScanMapTileGrid
 *
 * Returns: длина стороны тайла в пикселах
 */
guint
hyscan_map_tile_grid_get_tile_size (HyScanMapTileGrid *grid)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid), 0);

  return grid->priv->tile_size;
}

/**
 * hyscan_map_tile_grid_tile_to_value:
 * @grid: указатель на #HyScanMapTileGrid
 * @zoom: индекс масштаба
 * @x_tile: координата x в тайловой СК
 * @y_tile: координата y в тайловой СК
 * @x_val: (nullable): координата x в логической СК
 * @y_val: (nullable): координата y в логической СК
 *
 * Переводит координаты из тайловой СК в логическую.
 */
void
hyscan_map_tile_grid_tile_to_value (HyScanMapTileGrid *grid,
                                    guint              zoom,
                                    gdouble            x_tile,
                                    gdouble            y_tile,
                                    gdouble           *x_val,
                                    gdouble           *y_val)
{
  HyScanMapTileGridPrivate *priv;
  gdouble scale;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));
  priv = grid->priv;

  scale = priv->scales[zoom-priv->min_zoom];
  if (x_val != NULL)
    *x_val = priv->invert_ox ? priv->max_x - x_tile * scale : priv->min_x + x_tile * scale;

  if (y_val != NULL)
    *y_val = priv->invert_oy ? priv->max_y - y_tile * scale : priv->min_y + y_tile * scale;
}

/**
 * hyscan_map_tile_grid_tile_to_value:
 * @grid: указатель на #HyScanMapTileGrid
 * @x_val: координата x в логической СК
 * @y_val: координата y в логической СК
 * @x_tile: (nullable): координата x в тайловой СК
 * @y_tile: (nullable): координата y в тайловой СК
 *
 * Переводит координаты из логической СК в тайловую.
 */
void
hyscan_map_tile_grid_value_to_tile (HyScanMapTileGrid *grid,
                                    guint              zoom,
                                    gdouble            x_val,
                                    gdouble            y_val,
                                    gdouble           *x_tile,
                                    gdouble           *y_tile)
{
  HyScanMapTileGridPrivate *priv;
  gdouble tile_size_x;
  gdouble tile_size_y;

  g_return_if_fail (HYSCAN_IS_MAP_TILE_GRID (grid));
  priv = grid->priv;

  /* Размер тайла в логических единицах. */
  tile_size_x = priv->scales[zoom - priv->min_zoom];
  tile_size_y = priv->scales[zoom - priv->min_zoom];

  (y_tile != NULL) ? *y_tile = (priv->invert_oy ? priv->max_y - y_val : y_val - priv->min_x) / tile_size_y : 0;
  (x_tile != NULL) ? *x_tile = (priv->invert_ox ? priv->max_x - x_val : x_val - priv->min_x) / tile_size_x : 0;
}

/**
 * hyscan_map_tile_iter_init:
 * @iter: указатель на #HyScanMapTileIter
 * @from_x: минимальное значение координаты x
 * @to_x: максимальное значение координаты x
 * @from_y: минимальное значение координаты y
 * @to_y: максимальное значение координаты y
 * 
 * Инициализирует итератор тайлов. Полученный итератор будет проходить все тайлы
 * из указанной области начиная из её центра и двигаясь к краю.
 * В таком случае самые центральные тайлы, которые представляют больший интерес,
 * будут обработаны первыми.
 */
void
hyscan_map_tile_iter_init (HyScanMapTileIter *iter,
                           guint              from_x,
                           guint              to_x,
                           guint              from_y,
                           guint              to_y)
{
  iter->from_x = from_x;
  iter->to_x = to_x;
  iter->from_y = from_y;
  iter->to_y = to_y;
  iter->xc = (iter->to_x + iter->from_x + 1) / 2;
  iter->yc = (iter->to_y + iter->from_y + 1) / 2;
  iter->max_r = MAX (MAX (iter->to_x - iter->xc, iter->xc - iter->from_x),
                     MAX (iter->to_y - iter->yc, iter->yc - iter->from_y));

  iter->r = 0;
  iter->x = iter->from_x;
  iter->y = iter->from_y;
}

/**
 * hyscan_map_tile_iter_next:
 * @iter: указатель на #HyScanMapTileIter
 * @x: (out): координата тайла по оси x
 * @y: (out): координата тайла по оси y
 *
 * Получает координаты следующего тайла из итератора @iter. Итератор должен быть
 * предварительно инициализирован с помощью функции hyscan_map_tile_iter_init().
 *
 * Returns: %TRUE, если были возвращены координаты следующего тайла, или %FALSE,
 *   если все тайлы были проитерированы
 */
gboolean
hyscan_map_tile_iter_next (HyScanMapTileIter *iter, 
                           guint             *x,
                           guint             *y)
{
  for (; iter->r <= iter->max_r; ++iter->r)
    {
      for (; iter->x <= iter->to_x; ++iter->x)
        {
          for (; iter->y <= iter->to_y; ++iter->y)
            {
              /* Пока пропускаем тайлы снаружи радиуса. */
              if (ABS (iter->y - iter->yc) > iter->r || ABS (iter->x - iter->xc) > iter->r)
                continue;

              /* И рисуем тайлы на расстоянии r от центра по одной из координат. */
              if (ABS (iter->x - iter->xc) != iter->r && ABS (iter->y - iter->yc) != iter->r)
                continue;

              *x = iter->x;
              *y = iter->y;
              ++iter->y;

              return TRUE;
            }
          iter->y = iter->from_y;
        }
        iter->x = iter->from_x;
    }

  return FALSE;
}
