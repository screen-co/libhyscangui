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
 * ## Картографическая проекция
 *
 * Картографическая проекция #HyScanGeoProjection определяет перевод координат
 * из географических в координаты на плоскости. Для получения и установки текущей
 * проекции используйте функции:
 * - hyscan_gtk_map_set_projection()
 * - hyscan_gtk_map_get_projection()
 *
 * Для удобного перевода географических координат в координаты на плоскости и
 * обратно существуют функции:
 * - hyscan_gtk_map_value_to_geo()
 * - hyscan_gtk_map_geo_to_value()
 * - hyscan_gtk_map_point_to_geo()
 *
 * ## Масштабы
 *
 * В классе используется несколько единиц измерения масштаба:
 *
 * - численный масштаб, например, 1:100. Показывает, сколько сантиметров на местности
 *   соотвествует одному сантиметру на карте. Зависит от PPI монитора.
 * - метры на пиксель (м/px). Показывает, сколько метров на местности соответствует
 *   одному пикселю в системе координат виджета.
 * - метры на единицу проекции (м/ед). Показывает, сколько метров на местности соотвествует
 *   единичному отрезку в системе координат картографической проекции,
 * - единицы проекции на пиксель (ед/px). Показывает, сколько единиц проекции соответствует
 *   одному пикселю в системе координат виджета. Аналогичен масштабу #GtkCifroArea.
 *
 * Масштаб карты меняется дискретно и может принимать только заранее заданные
 * пользователем значения. Для работы с масштабом существуют следующие функции:
 *
 * - hyscan_gtk_map_create_scales2() - создаёт массив масштабов,
 * - hyscan_gtk_map_set_scales_meter() - устанавливает допустимые масштабы (м/px),
 * - hyscan_gtk_map_get_scales() - возвращает массив допустимых масштабов (ед/px),
 * - hyscan_gtk_map_get_scale_idx() - возвращает индекс текущего масштаба,
 * - hyscan_gtk_map_set_scale_idx() - устанавливает текущий масштаб по его индексу.
 *
 * Для установки масштаба и изменения видимой области можно использовать функции
 * класса #GtkCifroArea или воспользоваться специальнымим функциями карты:
 *
 * - hyscan_gtk_map_move_to() - перемещение центра карты в указанную точку,
 * - hyscan_gtk_map_set_scale_px() - устанавливает масштаб в центре карты (px/м),
 * - hyscan_gtk_map_get_scale_px() - получает масштаб в центре карты (px/м),
 * - hyscan_gtk_map_get_scale_ratio() - получает численный масштаб в центре карты с учётом PPI экрана,
 * - hyscan_gtk_map_get_scale_value() - получает масштаб для указанной точки (м/ед).
 *
 * Для размещения тайлового слоя с изображением карты (подложки) необходимо
 * воспользоваться классом #HyScanGtkMapBase.
 *
 * Данный класс корректно работает только с GMainLoop.
 *
 */

#include "hyscan-gtk-map.h"
#include <hyscan-proj.h>
#include <math.h>

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
static void     hyscan_gtk_map_check_scale              (GtkCifroArea          *carea,
                                                         gdouble               *scale_x,
                                                         gdouble               *scale_y);
static void     hyscan_gtk_map_zoom                     (GtkCifroArea          *carea,
                                                         GtkCifroAreaZoomType   direction_x,
                                                         GtkCifroAreaZoomType   direction_y,
                                                         gdouble                center_x,
                                                         gdouble                center_y);

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

  if (prop_id == PROP_PROJECTION)
    g_value_set_object (value, priv->projection);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
  carea_class->check_scale = hyscan_gtk_map_check_scale;
  carea_class->zoom = hyscan_gtk_map_zoom;

  /* Реализация виртуальных функций GTKWidget. */
  widget_class->configure_event = hyscan_gtk_map_configure;

  pspec = g_param_spec_object ("projection", "Projection", "HyScanGeoProjection",
                               HYSCAN_TYPE_GEO_PROJECTION,
                               G_PARAM_WRITABLE | G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_PROJECTION, pspec);
  hyscan_gtk_map_properties[PROP_PROJECTION] = pspec;

  pspec = g_param_spec_pointer ("init-center", "Initial Center", "HyScanGeoGeodetic",
                                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  hyscan_gtk_map_properties[PROP_INIT_CENTER] = pspec;
  g_object_class_install_property (object_class, PROP_INIT_CENTER, pspec);
}

static void
hyscan_gtk_map_init (HyScanGtkMap *gtk_map)
{
  gtk_map->priv = hyscan_gtk_map_get_instance_private (gtk_map);
}

/* Обработка "configure-event". Определяет PPI монитора. */
static gboolean
hyscan_gtk_map_configure (GtkWidget         *widget,
                          GdkEventConfigure *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;
  GdkRectangle mon_geom;
  GdkDisplay *display;
  GdkMonitor *monitor;

  gint monitor_h, monitor_w;
  gfloat ppi, diagonal_mm, diagonal_pix;

  /* Получаем монитор, на котором расположено окно. */
  display = gdk_window_get_display (event->window);
  monitor = gdk_display_get_monitor_at_window (display, event->window);

  /* Диагональ в пикселях. */
  gdk_monitor_get_geometry (monitor, &mon_geom);
  diagonal_pix = sqrt (mon_geom.width * mon_geom.width + mon_geom.height * mon_geom.height);

  /* Диагональ в миллиметрах. */
  monitor_h = gdk_monitor_get_height_mm (monitor);
  monitor_w = gdk_monitor_get_width_mm (monitor);
  diagonal_mm = sqrt (monitor_w * monitor_w + monitor_h * monitor_h) / HYSCAN_GTK_MAP_MM_PER_INCH;

  /* Вычисляем PPI. */
  ppi = diagonal_pix / diagonal_mm;
  if (isnan (ppi) || isinf (ppi) || ppi <= 0.0 || monitor_h <= 0 || monitor_w <= 0)
    ppi = HYSCAN_GTK_MAP_DEFAULT_PPI;

  priv->ppi = ppi;

  return GTK_WIDGET_CLASS (hyscan_gtk_map_parent_class)->configure_event (widget, event);
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
      priv->init_center = *((HyScanGeoGeodetic *) g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Инициализирует видимую область карты и, следовательно, масштаб. */
static gboolean
hyscan_gtk_map_init_view (GtkWidget         *widget,
                          GdkEventConfigure *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  guint width, height;

  HyScanGeoGeodetic coord;
  HyScanGeoCartesian2D from, to;

  gdouble view_margin = .005;        /* Отступ от центра карты до границ видимой области в градусах широты и долготы. */

  /* При нулевых размерах виджета инициализироваться не выйдет. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (map), &width, &height);
  if (width == 0 || height == 0)
    return FALSE;

  coord = priv->init_center;
  coord.lat -= view_margin;
  coord.lon -= view_margin;
  hyscan_gtk_map_geo_to_value (map, coord, &from);

  coord = priv->init_center;
  coord.lat += view_margin;
  coord.lon += view_margin;
  hyscan_gtk_map_geo_to_value (map, coord, &to);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), from.x, to.x, from.y, to.y);

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

  /* Устанавливаем проекцию и масштабы по умолчанию. */
  priv->projection = hyscan_proj_new (HYSCAN_PROJ_WEBMERC);
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

/* Ищет ближайший по значению допустимый масштаб.
 * Возвращает -1 в случае ошибки. */
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

/* Реализация функции GtkCifroAreaClass.zoom.
 * Изменяет масштаб в указнном направлении. */
static void
hyscan_gtk_map_zoom (GtkCifroArea          *carea,
                     GtkCifroAreaZoomType   direction_x,
                     GtkCifroAreaZoomType   direction_y,
                     gdouble                center_x,
                     gdouble                center_y)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (carea);
  gint scale_idx;

  scale_idx = hyscan_gtk_map_get_scale_idx (map, NULL);
  if (direction_x == GTK_CIFRO_AREA_ZOOM_IN || direction_y == GTK_CIFRO_AREA_ZOOM_IN)
    --scale_idx;
  else if (direction_x == GTK_CIFRO_AREA_ZOOM_OUT || direction_y == GTK_CIFRO_AREA_ZOOM_OUT)
    ++scale_idx;
  else
    return;

  hyscan_gtk_map_set_scale_idx (map, scale_idx < 0 ? 0 : scale_idx, center_x, center_y);
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

  scale = MAX (*scale_x, *scale_y);

  scale = hyscan_gtk_map_find_closest_scale (map, scale, NULL);

  *scale_x = scale;
  *scale_y = scale;
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
  gsize i;

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
 * hyscan_gtk_map_new:
 * @center: (nullable): указатель на структуру с геокоординатами центра карты
 *
 * Создаёт новый виджет #HyScanGtkMap для отображения географической карты.
 *
 * Returns: указатель на #HyScanGtkMap
 */
GtkWidget *
hyscan_gtk_map_new (HyScanGeoGeodetic center)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP,
                       "init-center", &center, NULL);
}

/**
 * hyscan_gtk_map_set_projection:
 * @map: указатель на карту #HyScanGtkMap
 * @projection: картографическая проекция #HyScanGeoProjection
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
  HyScanGeoGeodetic from_geo, to_geo;
  HyScanGeoCartesian2D from, to;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));
  g_return_if_fail (projection != NULL);
  priv = map->priv;

  /* Получаем текущие границы просмотра в географических координатах. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from.x, &to.x, &from.y, &to.y);
  hyscan_gtk_map_value_to_geo (map, &from_geo, from);
  hyscan_gtk_map_value_to_geo (map, &to_geo, to);

  /* Меняем проекцию и восстанавливаем границы просмотра. */
  hyscan_gtk_map_set_projection_real (priv, projection);

  hyscan_gtk_map_geo_to_value (map, from_geo, &from);
  hyscan_gtk_map_geo_to_value (map, to_geo, &to);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), from.x, to.x, from.y, to.y);

  /* Оповещаем подписчиков об изменении проекции. */
  g_object_notify_by_pspec (G_OBJECT (map), hyscan_gtk_map_properties[PROP_PROJECTION]);
}

/**
 * hyscan_gtk_map_get_projection:
 * @map: указатель на карту #HyScanGtkMap
 *
 * Returns: (transfer full): указатель на #HyScanGeoProjection. Для удаления g_object_unref().
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
  HyScanGeoCartesian2D center_2d;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  carea = GTK_CIFRO_AREA (map);

  /* Получаем координаты центра в проекции карты. */
  hyscan_geo_projection_geo_to_value (priv->projection, center, &center_2d);

  /* Перемещаем видимую область в новый центр. */
  gtk_cifro_area_set_view_center (carea, center_2d.x, center_2d.y);

  /* Если функция была вызвана до момента инициализации, то надо обновить init_center. */
  priv->init_center = center;
}

/**
 * hyscan_gtk_map_get_scale_value:
 * @map: указатель на объект #HyScanGtkMap
 * @coord: координаты точки на карте
 *
 * Определяет размер единичного отрезка на проекции в метрах в точке с
 * координатами @coord.
 *
 * Returns: размер единичного отрезка на проекции в метрах или -1.0 в случае ошибки.
 */
gdouble
hyscan_gtk_map_get_scale_value (HyScanGtkMap      *map,
                                HyScanGeoGeodetic  coord)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  return hyscan_geo_projection_get_scale (map->priv->projection, coord);
}

/**
 * hyscan_gtk_map_get_scale_px:
 * @map: указатель на #HyScanGtkMap
 *
 * Определяет масштаб в центре карты в пикселах на метр.
 *
 * Returns: количество пикселов в одном метре вдоль ширины виджета или -1.0 в случае ошибки.
 */
gdouble
hyscan_gtk_map_get_scale_px (HyScanGtkMap *map)
{
  HyScanGtkMapPrivate *priv;
  gdouble scale;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGeoGeodetic geo_center;
  HyScanGeoCartesian2D center;

  gdouble dist_pixels;
  gdouble dist_metres;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  priv = map->priv;

  /* Определяем географические координаты центра видимой области. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  center.x = (from_x + to_x) / 2.0;
  center.y = (from_y + to_y) / 2.0;
  hyscan_gtk_map_value_to_geo (map, &geo_center, center);

  /* Определяем размер единичного отрезка на проекции в метрах. */
  dist_metres = hyscan_geo_projection_get_scale (priv->projection, geo_center);

  /* Определяем размер единичного отрезка на проекции в пикселях. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  dist_pixels = 1.0 / scale;

  return dist_pixels / dist_metres;
}

/**
 * hyscan_gtk_map_get_scale_ratio:
 * @map: указатель на #HyScanGtkMap
 *
 * Определяет маштаб карты с учетом разрешающей способности монитора: во сколько
 * раз изображение на экране меньше по сравнению с его реальным размером.
 *
 * Returns: масштаб карты с учетом PPI-монитора или отрицательное число в случае
 *    ошибки.
 */
gdouble
hyscan_gtk_map_get_scale_ratio (HyScanGtkMap *map)
{
  HyScanGtkMapPrivate *priv;
  gdouble ppm;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);
  priv = map->priv;

  /* Pixels per meter. */
  ppm = priv->ppi / (1e-3 * HYSCAN_GTK_MAP_MM_PER_INCH);

  return hyscan_gtk_map_get_scale_px (map) / ppm;
}

/**
 * hyscan_gtk_map_set_scale_px:
 * @map: указатель на #HyScanGtkMap
 * @scale: масштаб (количество пикселов в одном метре вдоль ширины виджета)
 *
 * Устанавливает указанный масштаб в центре карты.
 */
void
hyscan_gtk_map_set_scale_px (HyScanGtkMap *map,
                             gdouble       scale)
{
  HyScanGtkMapPrivate *priv;
  gdouble scale_px;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGeoGeodetic geo_center;
  HyScanGeoCartesian2D center;

  gdouble scale_geo;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;

  /* Определяем географические координаты центра видимой области. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  center.x = (from_x + to_x) / 2.0;
  center.y = (from_y + to_y) / 2.0;
  hyscan_gtk_map_value_to_geo (map, &geo_center, center);

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
hyscan_gtk_map_value_to_geo (HyScanGtkMap         *map,
                             HyScanGeoGeodetic    *coords,
                             HyScanGeoCartesian2D  c2d)
{
  HyScanGtkMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  hyscan_geo_projection_value_to_geo (priv->projection, coords, c2d.x, c2d.y);
}

/**
 * hyscan_gtk_map_geo_to_value:
 * @map: указатель на #HyScanGtkMap
 * @coords: географические координаты
 * @c2d: координата в системе координат виджета
 *
 * Преобразовывает координаты из географических в координаты на плоскости
 * картографической проекции.
 */
void
hyscan_gtk_map_geo_to_value (HyScanGtkMap         *map,
                             HyScanGeoGeodetic     coords,
                             HyScanGeoCartesian2D *c2d)
{
  HyScanGtkMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  hyscan_geo_projection_geo_to_value (priv->projection, coords, c2d);
}

/**
 * hyscan_gtk_map_set_scales:
 * @map: указатель на карту #HyScanGtkMap
 * @scales: (array length=scales_len): допустимые масштабы
 * @scales_len: длина массива scales
 *
 * Устанавливает допустимые масштабы карты - никакие другие масштабы не могут быть
 * выбраны пользоателем. Это значит, что при установке произвольного масштаба с
 * помошью gtk_cifro_area_set_scale() будет установлено ближайшее значение из массива
 * @scales.
 *
 * Масштабы указываются в метрах на пиксель для экватора. Поэтому установленный масштаб
 * 1 м/px в средних широтах для проекции меркатора будет равен примерно 0.5 м/px.
 *
 * Сгенерировать масштабы можно с помощью функции hyscan_gtk_map_create_scales2().
 */
void
hyscan_gtk_map_set_scales_meter (HyScanGtkMap  *map,
                                 const gdouble *scales,
                                 gsize          scales_len)
{
  HyScanGtkMapPrivate *priv;

  gsize i;
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
 * hyscan_gtk_map_get_scales:
 * @map: указатель на виджет карты #HyScanGtkMap
 * @scales_len: длина массива масштабов
 *
 * Возвращает массив допустимых масштабов #GtkCifroArea.
 *
 * Returns: (array length=scales_len): массив масштабов. Для удаления g_free()
 */
gdouble *
hyscan_gtk_map_get_scales (HyScanGtkMap *map,
                           guint        *scales_len)
{
  HyScanGtkMapPrivate *priv;
  gdouble *scales;
  gsize i;

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
 * Определяет индекс текущего масштаба карты. См. hyscan_gtk_map_set_scale_idx().
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
 * Составляет массив допустимых масштабов от @min_scale до @max_scale, при этом
 * увеличение в два раза происходит за @steps шагов.
 *
 * Такой набор масштабов наиболее соответствует тайловым картам web-сервисов,
 * где доступны масштабы 2ᶻ * %HYSCAN_GTK_MAP_EQUATOR_LENGTH / 256px, z = 0..19.
 *
 * Returns: (array length=scales_len): массив из допустимых масштабов. Для удаления
 *    g_free().
 */
gdouble *
hyscan_gtk_map_create_scales2 (gdouble  min_scale,
                               gdouble  max_scale,
                               gint     steps,
                               gint    *scales_len)
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
  new_point->c2d = point->c2d;

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
