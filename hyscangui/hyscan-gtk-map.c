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
 */

#include "hyscan-gtk-map.h"
#include <math.h>
#include <hyscan-gui-marshallers.h>

enum
{
  PROP_O,
  PROP_PROJECTION,
};

struct _HyScanGtkMapPrivate
{
  HyScanGeoProjection     *projection;           /* Картографическая проекция поверхности Земли на плоскость карты. */
};

static void     hyscan_gtk_map_set_property             (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void     hyscan_gtk_map_object_constructed       (GObject               *object);
static void     hyscan_gtk_map_object_finalize          (GObject               *object);
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

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, HYSCAN_TYPE_GTK_LAYER_CONTAINER)

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_set_property;
  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

  /* Реализация виртуальных функций GtkCifroArea. */
  carea_class->get_limits = hyscan_gtk_map_get_limits;
  carea_class->get_border = hyscan_gtk_map_get_border;
  carea_class->check_scale = hyscan_gtk_map_check_scale;

  g_object_class_install_property (object_class, PROP_PROJECTION,
    g_param_spec_object ("projection", "Projection", "HyScanGeoProjection", HYSCAN_TYPE_GEO_PROJECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_gtk_map_init (HyScanGtkMap *gtk_map)
{
  gtk_map->priv = hyscan_gtk_map_get_instance_private (gtk_map);
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
      priv->projection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_object_constructed (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->constructed (object);
}

static void
hyscan_gtk_map_object_finalize (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  g_object_unref (priv->projection);

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->finalize (object);
}

/* Реализация функции check_scale GtkCifroArea.
 * Сохраняет одинаковый масштаб по обеим осям. */
static void
hyscan_gtk_map_check_scale (GtkCifroArea *carea,
                            gdouble      *scale_x,
                            gdouble      *scale_y)
{
  // todo: check min scale, max scale
  *scale_y = *scale_x;
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
 * @projection: указатель на картографическую проекцию #HyScanGeoProjection
 *
 * Создаёт новый виджет #HyScanGtkMap для отображения географической карты.
 *
 * Returns: указатель на #HyScanGtkMap
 */
GtkWidget *
hyscan_gtk_map_new (HyScanGeoProjection *projection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP,
                       "projection", projection, NULL);
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
 * hyscan_gtk_map_get_scale:
 * @map: указатель на #HyScanGtkMap
 *
 * Определяет масштаб в центре карты.
 *
 * Returns: количество пикселов в одном метре вдоль ширины виджета
 */
gdouble
hyscan_gtk_map_get_scale (HyScanGtkMap *map)
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

  /* Определяем размер единичного отрезка на проекции в пикселах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  dist_pixels = 1 / scale;

  return dist_pixels / dist_metres;
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
