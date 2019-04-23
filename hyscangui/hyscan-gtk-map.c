/* hyscan-gtk-map.c
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
 * SECTION: hyscan-gtk-map
 * @Short_description: Виджет карты
 * @Title: HyScanGtkMap
 *
 * Класс расширяет #GtkCifroArea, позволяя отображать картографическую проекцию
 * местности в виджете.
 *
 * Картографическая проекция #HyScanGeoProjection определяет перевод координат из географических
 * в координаты на плоскости и указывается при создании виджета карты в
 * функции hyscan_gtk_map_new().
 *
 * Для размещения тайлового слоя с изображением карты необходимо воспользоваться
 * классом #HyScanGtkMapTiles.
 *
 * Данный класс корректно работает только с GMainLoop.
 *
 */

#include "hyscan-gtk-map.h"
#include <hyscan-pseudo-mercator.h>
#include <math.h>

#define DEFAULT_PPI 96.0
#define MM_PER_INCH 25.4

enum
{
  PROP_O,
  PROP_INIT_CENTER,
  PROP_PROJECTION,
  PROP_LAST
};

struct _HyScanGtkMapPrivate
{
  HyScanGeoProjection     *projection;           /* Картографическая проекция поверхности Земли на плоскость карты. */
  gfloat                   ppi;                  /* PPI экрана. */

  HyScanGeoGeodetic       init_center;           /* Географическая координата центра карты при инициализации. */

  gdouble                 *scales;               /* Массив доступных масштабов, упорядоченный по возрастанию. */
  gsize                    scales_len;           /* Длина массива scales. */
};

static void     hyscan_gtk_map_set_property             (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void     hyscan_gtk_map_object_constructed       (GObject               *object);
static void     hyscan_gtk_map_object_finalize          (GObject               *object);
static gboolean hyscan_gtk_map_configure                (GtkWidget             *widget,
                                                         GdkEventConfigure     *event);
static void     hyscan_gtk_map_get_limits               (GtkCifroArea          *carea,
                                                         gdouble               *min_x,
                                                         gdouble               *max_x,
                                                         gdouble               *min_y,
                                                         gdouble               *max_y);
static void     hyscan_gtk_map_get_border               (GtkCifroArea          *carea,
                                                         guint                 *border_top,
                                                         guint                 *border_bottom,
                                                         guint                 *border_left,
                                                         guint                 *border_right);
static void     hyscan_gtk_map_check_scale              (GtkCifroArea          *carea,
                                                         gdouble               *scale_x,
                                                         gdouble               *scale_y);

static GParamSpec* hyscan_gtk_map_properties[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, HYSCAN_TYPE_GTK_LAYER_CONTAINER)

static void
hyscan_gtk_map_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  switch ( prop_id )
    {
    case PROP_PROJECTION:
      g_value_set_object (value, priv->projection);
      break;

    default:
      break;
    }
}

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = hyscan_gtk_map_set_property;
  object_class->get_property = hyscan_gtk_map_get_property;
  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

  /* Реализация виртуальных функций GtkCifroArea. */
  carea_class->get_limits = hyscan_gtk_map_get_limits;
  carea_class->get_border = hyscan_gtk_map_get_border;
  carea_class->check_scale = hyscan_gtk_map_check_scale;

  /* Сигналы GTKWidget. */
  widget_class->configure_event = hyscan_gtk_map_configure;

  pspec = g_param_spec_object ("projection", "Projection", "HyScanGeoProjection", HYSCAN_TYPE_GEO_PROJECTION,
                               G_PARAM_WRITABLE | G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_PROJECTION, pspec);
  hyscan_gtk_map_properties[PROP_PROJECTION] = pspec;

  pspec = g_param_spec_pointer ("init-center", "Initial Center", "HyScanGeoGeodetic",
                                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_INIT_CENTER, pspec);
}

static gboolean
hyscan_gtk_map_configure (GtkWidget         *widget,
                          GdkEventConfigure *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;
  GdkScreen *gdkscreen;
  GdkRectangle mon_geom;

  gint monitor_num, monitor_h, monitor_w;
  gfloat ppi, diagonal_mm, diagonal_pix;

  /* Получаем монитор, на котором расположено окно. */
  gdkscreen = gdk_window_get_screen (event->window);
  monitor_num = gdk_screen_get_monitor_at_window (gdkscreen, event->window);

  /* Диагональ в пикселях. */
  gdk_screen_get_monitor_geometry (gdkscreen, monitor_num, &mon_geom);
  diagonal_pix = sqrt (mon_geom.width * mon_geom.width + mon_geom.height * mon_geom.height);

  /* Диагональ в миллиметрах. */
  monitor_h = gdk_screen_get_monitor_height_mm (gdkscreen, monitor_num);
  monitor_w = gdk_screen_get_monitor_width_mm (gdkscreen, monitor_num);
  diagonal_mm = sqrt (monitor_w * monitor_w + monitor_h * monitor_h) / MM_PER_INCH;

  /* Вычисляем PPI. */
  ppi = diagonal_pix / diagonal_mm;
  if (isnan (ppi) || isinf(ppi) || ppi <= 0.0 || monitor_h <= 0 || monitor_w <= 0)
    ppi = DEFAULT_PPI;

  priv->ppi = ppi;

  return GTK_WIDGET_CLASS (hyscan_gtk_map_parent_class)->configure_event (widget, event);
}

static void
hyscan_gtk_map_init (HyScanGtkMap *gtk_map)
{
  gtk_map->priv = hyscan_gtk_map_get_instance_private (gtk_map);
}

/* Устанавливает координаты центра карты при ее загрузке. */
static void
hyscan_gtk_map_set_init_center (HyScanGtkMapPrivate *priv,
                                HyScanGeoGeodetic   *center)
{
  if (center == NULL)
    return;

  priv->init_center.lat = center->lat;
  priv->init_center.lon = center->lon;
}

static void
hyscan_gtk_map_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  switch (prop_id)
    {
    case PROP_PROJECTION:
      hyscan_gtk_map_set_projection (gtk_map, g_value_get_object (value));
      break;

    case PROP_INIT_CENTER:
      hyscan_gtk_map_set_init_center (priv, g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Инициализирует область просмотра карты и, следовательно, масштаб. */
static gboolean
hyscan_gtk_map_init_view (GtkWidget         *widget,
                          GdkEventConfigure *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  guint width, height;

  HyScanGeoGeodetic coord;
  gdouble from_x, to_x, from_y, to_y;
  gdouble view_size = .005;

  /* При нулевых размерах виджета инициализироваться не выйдет. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (map), &width, &height);
  if (width == 0 || height == 0)
    return FALSE;

  coord = priv->init_center;
  coord.lat -= view_size;
  coord.lon -= view_size;
  hyscan_gtk_map_geo_to_value (map, coord, &from_x, &from_y);

  coord = priv->init_center;
  coord.lat += view_size;
  coord.lon += view_size;
  hyscan_gtk_map_geo_to_value (map, coord, &to_x, &to_y);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), from_x, to_x, from_y, to_y);

  /* Отключаем этот обработчик, т.к. инициализируемся только раз. */
  g_signal_handlers_disconnect_by_func (widget, hyscan_gtk_map_init_view, NULL);

  return FALSE;
}

static void
hyscan_gtk_map_object_constructed (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;
  gdouble scales[] = { 1.0 };

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->constructed (object);

  g_signal_connect_after (gtk_map, "configure-event", G_CALLBACK (hyscan_gtk_map_init_view), NULL);

  priv->projection = hyscan_pseudo_mercator_new ();
  hyscan_gtk_map_set_scales_meter (gtk_map, scales, G_N_ELEMENTS (scales));
}

static void
hyscan_gtk_map_object_finalize (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  g_object_unref (priv->projection);
  g_free (priv->scales);

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->finalize (object);
}

/* Ищет ближайший по значению допустимый масштаб. */
static gdouble
hyscan_gtk_map_find_closest_scale (HyScanGtkMap *map,
                                   gdouble       scale,
                                   gint         *found_idx)
{
  HyScanGtkMapPrivate *priv = map->priv;

  gint l, r;
  gint found_idx_ret = -1;

  if (priv->scales_len <= 0)
    {
      *found_idx = -1;
      return scale;
    }

  l = 0;
  r = priv->scales_len - 1;

  while (l <= r && found_idx_ret < 0)
    {
      gint mid;

      mid = (l + r) / 2;
      if (scale < priv->scales[mid])
        r = mid - 1;
      else if (scale > priv->scales[mid])
        l = mid + 1;
      else
        found_idx_ret = mid;
    }

  if (found_idx_ret < 0)
    {
      l = CLAMP (l, 0, (gint) (priv->scales_len - 1));
      r = CLAMP (r, 0, (gint) (priv->scales_len - 1));

      found_idx_ret = (priv->scales[l] - scale) < (scale - priv->scales[r]) ? l : r;
    }

  (found_idx != NULL) ? *found_idx = found_idx_ret : 0;

  return priv->scales[found_idx_ret];
}

/* Реализация функции GtkCifroAreaClass.check_scale.
 * Сохраняет одинаковый масштаб по обеим осям. */
static void
hyscan_gtk_map_check_scale (GtkCifroArea *carea,
                            gdouble      *scale_x,
                            gdouble      *scale_y)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (carea);

  gdouble scale;

  scale = hyscan_gtk_map_find_closest_scale (map, *scale_x, NULL);

  *scale_x = scale;
  *scale_y = scale;
}

static void
hyscan_gtk_map_get_border (GtkCifroArea *carea,
                           guint        *border_top,
                           guint        *border_bottom,
                           guint        *border_left,
                           guint        *border_right)
{
  (border_bottom != NULL) ? *border_bottom = 0 : 0;
  (border_top != NULL) ? *border_top = 0 : 0;
  (border_left != NULL) ? *border_left = 0 : 0;
  (border_right != NULL) ? *border_right = 0 : 0;
}

/* Границы системы координат карты. */
static void
hyscan_gtk_map_get_limits (GtkCifroArea *carea,
                           gdouble      *min_x,
                           gdouble      *max_x,
                           gdouble      *min_y,
                           gdouble      *max_y)
{
  HyScanGtkMapPrivate *priv = HYSCAN_GTK_MAP (carea)->priv;

  hyscan_geo_projection_get_limits (priv->projection, min_x, max_x, min_y, max_y);
}

/**
 * hyscan_gtk_map_new:
 * @center: (nullable): указатель на структуру с геокоординатами центра карты
 *
 * Создаёт новый виджет #HyScanGtkMap для отображения географической карты.
 *
 * Returns: указатель на #HyScanGtkMap
 */
GtkWidget *
hyscan_gtk_map_new (HyScanGeoGeodetic   *center)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP,
                       "init-center", center, NULL);
}

/* Определяет размер карты в единицах проекции. */
static gdouble
hyscan_gtk_map_get_logic_size (HyScanGtkMapPrivate *priv)
{
  gdouble min_x, max_x;

  hyscan_geo_projection_get_limits (priv->projection, &min_x, &max_x, NULL, NULL);
  return max_x - min_x;
}

/* Устанавливает проекцию на самом деле. */
static void
hyscan_gtk_map_set_projection_real (HyScanGtkMapPrivate *priv,
                                    HyScanGeoProjection *projection)
{
  gdouble prev_size;
  gdouble size;
  guint i;

  /* Размер проекции в логических единицах, чтобы сохранить доступные масштабы. */
  prev_size = hyscan_gtk_map_get_logic_size (priv);

  g_clear_object (&priv->projection);
  priv->projection = g_object_ref (projection);

  /* Обновляем доступные масштабы. */
  size = hyscan_gtk_map_get_logic_size (priv);
  for (i = 0; i < priv->scales_len; ++i)
    priv->scales[i] *= size / prev_size;
}

/**
 * hyscan_gtk_map_set_projection:
 * @map:
 * @projection:
 *
 * Устанавливает новую проекцию в карте, сохраняя географические координаты
 * видимой области карты. Все слои будут оповещены об этом событии с помощью
 * сигнала "notify::projection".
 */
void
hyscan_gtk_map_set_projection (HyScanGtkMap        *map,
                               HyScanGeoProjection *projection)
{
  HyScanGtkMapPrivate *priv;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGeoGeodetic from_geo, to_geo;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));
  g_return_if_fail (projection != NULL);
  priv = map->priv;

  /* Получаем текущие границы просмотра в географических координатах. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_geo (map, &from_geo, from_x, from_y);
  hyscan_gtk_map_value_to_geo (map, &to_geo, to_x, to_y);

  /* Меняем проекцию и восстанавливаем границы просмотра. */
  hyscan_gtk_map_set_projection_real (priv, projection);

  hyscan_gtk_map_geo_to_value (map, from_geo, &from_x, &from_y);
  hyscan_gtk_map_geo_to_value (map, to_geo, &to_x, &to_y);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), from_x, to_x, from_y, to_y);

  /* Оповещаем подписчиков об изменении проекции. */
  g_object_notify_by_pspec (G_OBJECT (map), hyscan_gtk_map_properties[PROP_PROJECTION]);
}

/**
 * hyscan_gtk_map_get_projection:
 * @map
 *
 * Returns: указатель на #HyScanGeoProjection. Для удаления g_object_unref().
 */
HyScanGeoProjection *
hyscan_gtk_map_get_projection (HyScanGtkMap *map)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), NULL);

  return g_object_ref (map->priv->projection);
}

/**
 * hyscan_gtk_map_move_to:
 * @map: указатель на объект #HyScanGtkMap
 * @center: новые координаты центра
 *
 * Перемещает центр карты в точку @center.
 */
void
hyscan_gtk_map_move_to (HyScanGtkMap      *map,
                        HyScanGeoGeodetic  center)
{
  HyScanGtkMapPrivate *priv;
  GtkCifroArea *carea;
  gdouble center_x, center_y;
  gdouble width, height;

  gdouble from_x, to_x, from_y, to_y;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  carea = GTK_CIFRO_AREA (map);

  /* Определяем текущие размеры показываемой области. */
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  width = to_x - from_x;
  height = to_y - from_y;

  /* Получаем координаты геоточки в проекции карты. */
  hyscan_geo_projection_geo_to_value (priv->projection, center, &center_x, &center_y);

  /* Перемещаем показываемую область в новый центр. */
  gtk_cifro_area_set_view (carea,
                           center_x - width / 2.0, center_x + width / 2.0,
                           center_y - height / 2.0, center_y + height / 2.0);
}

/**
 * hyscan_gtk_map_get_value_scale:
 * @map:
 * @coord:
 *
 * Определяет размер единичного отрезка на проекции в метрах в точке с
 * координатами @coord.
 *
 * Returns: размер единичного отрезка на проекции в метрах
 */
gdouble
hyscan_gtk_map_get_value_scale (HyScanGtkMap      *map,
                                HyScanGeoGeodetic *coord)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  return hyscan_geo_projection_get_scale (map->priv->projection, *coord);
}

/**
 * hyscan_gtk_map_get_pixel_scale:
 * @map: указатель на #HyScanGtkMap
 *
 * Определяет масштаб в центре карты.
 *
 * Returns: количество пикселов в одном метре вдоль ширины виджета
 */
gdouble
hyscan_gtk_map_get_pixel_scale (HyScanGtkMap *map)
{
  HyScanGtkMapPrivate *priv;
  gdouble scale;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGeoGeodetic geo_center;

  gdouble dist_pixels;
  gdouble dist_metres;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  priv = map->priv;

  /* Определяем географические координаты центра видимой области. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_geo (map, &geo_center, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  /* Определяем размер единичного отрезка на проекции в метрах. */
  dist_metres = hyscan_geo_projection_get_scale (priv->projection, geo_center);

  /* Определяем размер единичного отрезка на проекции в пикселях. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  dist_pixels = 1 / scale;

  return dist_pixels / dist_metres;
}

/**
 * hyscan_gtk_map_get_scale:
 * @map: указатель на #HyScanGtkMap
 *
 * Определяет маштаб карты с учетом разрешающей способности монитора: во сколько
 * раз изображение на карте меньше по сравнению с его реальным размером.
 *
 * Returns: масштаб карты с учетом PPI-монитора или отрицательное число в случае
 *    ошибки.
 */
gdouble
hyscan_gtk_map_get_scale (HyScanGtkMap *map)
{
  HyScanGtkMapPrivate *priv;
  gdouble ppm;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);
  priv = map->priv;

  /* Pixels per meter. */
  ppm = priv->ppi / (1e-3 * MM_PER_INCH);

  return hyscan_gtk_map_get_pixel_scale (map) / ppm;
}

/**
 * hyscan_gtk_map_set_pixel_scale:
 * @map: указатель на #HyScanGtkMap
 * @scale: масштаб (количество пикселов в одном метре вдоль ширины виджета)
 *
 * Устанавливает указанный масштаб в центре карты.
 */
void
hyscan_gtk_map_set_pixel_scale (HyScanGtkMap *map,
                                gdouble       scale)
{
  HyScanGtkMapPrivate *priv;
  gdouble scale_px;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGeoGeodetic geo_center;

  gdouble scale_geo;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;

  /* Определяем географические координаты центра видимой области. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_geo (map, &geo_center, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  /* Определяем размер единичного отрезка на проекции в метрах. */
  scale_geo = hyscan_geo_projection_get_scale (priv->projection, geo_center);

  scale_px = scale_geo / scale;
  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (map), scale_px, scale_px, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
}

/**
 * hyscan_gtk_map_point_to_geo:
 * @map: указатель на #HyScanGtkMap
 * @coords: географические координаты
 * @x: координата x в системе координат виджета
 * @y: координата y в системе координат виджета
 *
 * Преобразовывает координаты из системы координат виджета в географические
 * координаты.
 */
void
hyscan_gtk_map_point_to_geo (HyScanGtkMap      *map,
                             HyScanGeoGeodetic *coords,
                             gdouble            x,
                             gdouble            y)
{
  HyScanGtkMapPrivate *priv;
  gdouble x_val;
  gdouble y_val;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (map), x, y, &x_val, &y_val);
  hyscan_geo_projection_value_to_geo (priv->projection, coords, x_val, y_val);
}

/**
 * hyscan_gtk_map_point_to_geo:
 * @map: указатель на #HyScanGtkMap
 * @coords: географические координаты
 * @x: координата x на плоскости
 * @y: координата y на плоскости
 *
 * Преобразовывает координаты из картографической проекции в географические
 * координаты.
 */
void
hyscan_gtk_map_value_to_geo (HyScanGtkMap       *map,
                             HyScanGeoGeodetic  *coords,
                             gdouble             x_val,
                             gdouble             y_val)
{
  HyScanGtkMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  hyscan_geo_projection_value_to_geo (priv->projection, coords, x_val, y_val);
}

/**
 * hyscan_gtk_map_geo_to_value:
 * @map: указатель на #HyScanGtkMap
 * @coords: географические координаты
 * @x: координата x в системе координат виджета
 * @y: координата y в системе координат виджета
 *
 * Преобразовывает координаты из географических в координаты на плоскости
 * картографической проекции.
 */
void
hyscan_gtk_map_geo_to_value (HyScanGtkMap        *map,
                             HyScanGeoGeodetic    coords,
                             gdouble             *x_val,
                             gdouble             *y_val)
{
  HyScanGtkMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  hyscan_geo_projection_geo_to_value (priv->projection, coords, x_val, y_val);
}

/**
 * hyscan_gtk_map_set_scales:
 * @map: указатель на карту #HyScanGtkMap
 * @scales: (array length=scales_len): допустимые масштабы в метр на пиксель
 *          на экваторе
 * @scales_len:
 *
 * Устанавливает допустимые масштабы карты. Сгенерировать масштабы можно с
 * помощью функции hyscan_gtk_map_create_scales2().
 */
void
hyscan_gtk_map_set_scales_meter (HyScanGtkMap  *map,
                                 const gdouble *scales,
                                 gsize          scales_len)
{
  HyScanGtkMapPrivate *priv;

  guint i = 0;
  gdouble size;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));
  g_return_if_fail (scales_len > 0);

  priv = map->priv;

  /* Делаем поправку на размер проекции. */
  size = hyscan_gtk_map_get_logic_size (priv);

  priv->scales_len = scales_len;

  g_free (priv->scales);
  priv->scales = g_malloc (priv->scales_len * sizeof (*priv->scales));
  for (i = 0; i < priv->scales_len; i++)
    priv->scales[i] = scales[i] * size / HYSCAN_GTK_MAP_EQUATOR_LENGTH;
}

/**
 * hyscan_gtk_map_get_scales_cifro:
 * @map: указатель на виджет карты #HyScanGtkMap
 * @scales_len: длина массива масштабов
 *
 * Возвращает массив допустимых масштабов #GtkCifroArea.
 *
 * Returns: (array length=scales_len): массив масштабов. Для удаления g_free()
 */
gdouble *
hyscan_gtk_map_get_scales_cifro (HyScanGtkMap *map,
                                 guint        *scales_len)
{
  HyScanGtkMapPrivate *priv;
  gdouble *scales;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), NULL);
  priv = map->priv;

  scales = g_new (gdouble, map->priv->scales_len);
  for (i = 0; i < priv->scales_len; ++i)
    scales[i] = priv->scales[i];

  *scales_len = priv->scales_len;

  return scales;
}

/**
 * hyscan_gtk_map_get_scale_idx:
 * @map: указатель на виджет карты #HyScanGtkMap
 *
 * Определяет индекс текущего масштаба карта. См. hyscan_gtk_map_set_scale_idx().
 *
 * Returns: индекс текущего масштаба или -1 в случае ошибки
 */
gint
hyscan_gtk_map_get_scale_idx (HyScanGtkMap *map,
                              gdouble      *scale)
{
  gdouble _scale;
  gint idx;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1);

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &_scale, NULL);
  hyscan_gtk_map_find_closest_scale (map, _scale, &idx);

  (scale != NULL) ? *scale = _scale : 0;

  return idx;
}

/**
 * hyscan_gtk_map_set_scale_idx:
 * @map: указатель на #HyScanGtkMap
 * @idx: индекс масштаба
 * @center_x: центр изменения масштаба по оси X
 * @center_y: центр изменения масштаба по оси Y
 *
 * Устанавливает один из допустимых масштабов карты по его индексу @idx. Чтобы
 * установить допустимые масштабы используйте hyscan_gtk_map_set_scales().
 */
void
hyscan_gtk_map_set_scale_idx (HyScanGtkMap *map,
                              guint         idx,
                              gdouble       center_x,
                              gdouble       center_y)
{
  gdouble scale;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  idx = MIN (idx, map->priv->scales_len - 1);

  scale = map->priv->scales[idx];
  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (map), scale, scale, center_x, center_y);
}

/**
 * hyscan_gtk_map_create_scales2:
 * @min_scale: минимальный желаемый масштаб, метр на пиксель на экваторе
 * @max_scale: максимальный желаемый масштаб, метр на пиксель на экваторе
 * @steps: количество шагов масштаба между увеличением в 2 раза
 * @scales_len: длина массива масштабов
 *
 * Составляет массив допустимых масштабов от @min_scale до @max_scale в виде
 * степеней двойки.
 *
 * Такой набор масштабов наиболее соответствует тайловым картам web-сервисов,
 * где доступны масштабы 2ᶻ * %HYSCAN_GTK_MAP_EQUATOR_LENGTH / 256px, z = 0..19.
 *
 * Returns: (array length=scales_len): массив из допустимых масштабов. Для удаления
 *    g_free().
 */
gdouble *
hyscan_gtk_map_create_scales2 (gdouble min_scale,
                               gdouble max_scale,
                               gint    steps,
                               gint   *scales_len)
{
  gint exp_start;
  gint exp_end;

  gint i;

  gint len;
  gdouble *scales;

  g_return_val_if_fail (min_scale < max_scale, NULL);

  /* Приводим масштабы к степени двойки. */
  exp_start = (gint) round (log2 (min_scale));
  exp_end   = (gint) round (log2 (max_scale));

  len = (exp_end - exp_start) * steps + 1;
  scales = g_new (gdouble, len);
  for (i = 0; i < len; i++)
    scales[i] = pow (2, exp_start + (gdouble) i / steps);

  *scales_len = len;

  return scales;
}

/**
 * hyscan_gtk_map_point_copy:
 * @point: указатель на #HyScanGtkMapPoint
 *
 * Создаёт копию структуры точки @point.
 *
 * Returns: указатель на новую структуру #HyScanGtkMapPoint.
 * Для удаления hyscan_gtk_map_point_free()
 */
HyScanGtkMapPoint *
hyscan_gtk_map_point_copy (HyScanGtkMapPoint *point)
{
  HyScanGtkMapPoint *new_point;

  new_point = g_slice_new (HyScanGtkMapPoint);
  new_point->geo = point->geo;
  new_point->x = point->x;
  new_point->y = point->y;

  return new_point;
}

/**
 * hyscan_gtk_map_point_free:
 * @point: указатель на #HyScanGtkMapPoint
 *
 * Освобождает память из-под структуры HyScanGtkMapPoint.
 */
void
hyscan_gtk_map_point_free (HyScanGtkMapPoint *point)
{
  g_slice_free (HyScanGtkMapPoint, point);
}
