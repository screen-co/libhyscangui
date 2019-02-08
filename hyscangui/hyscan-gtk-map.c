/**
 * SECTION: hyscan-gtk-map
 * @Short_description: Виджет карты
 * @Title: HyScanGtkMap
 *
 * Класс расширяет #GtkCifroArea, позволяя отображать картографическую проекцию
 * местности в виджете.
 *
 * Проекция #HyScanGeoProjection определяет перевод координат из географических
 * в координаты на плоскости и указывается при создании виджета карты в
 * функции hyscan_gtk_map_new().
 *
 * Для размещения тайлового слоя с изображением карты необходимо воспользоваться
 * классом #HyScanGtkMapTiles.
 *
 */

#include "hyscan-gtk-map.h"
#include <math.h>

enum
{
  PROP_O,
  PROP_PROJECTION,
};

struct _HyScanGtkMapPrivate
{
  HyScanGeoProjection     *projection;           /* Проекция поверхности Земли на плоскость карты. */
  gconstpointer            howner;               /* Кто в данный момент обрабатывает взаимодействие с картой. */
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


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, GTK_TYPE_CIFRO_AREA)

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

/* Функция рисования чего???. */
static void
hyscan_gtk_map_visible_draw (GtkWidget *widget,
                             cairo_t   *cairo)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;
  /* todo: ??? */
}

static void
hyscan_gtk_map_init (HyScanGtkMap *gtk_map)
{
  gtk_map->priv = hyscan_gtk_map_get_instance_private (gtk_map);
  gint event_mask = 0;

  /* Обработчики сигналов. */
  g_signal_connect (gtk_map, "visible-draw", G_CALLBACK (hyscan_gtk_map_visible_draw), NULL);

  /* Список событий GTK, которые принимает виджет. */
  event_mask |= GDK_KEY_PRESS_MASK;
  event_mask |= GDK_KEY_RELEASE_MASK;
  event_mask |= GDK_BUTTON_PRESS_MASK;
  event_mask |= GDK_BUTTON_RELEASE_MASK;
  event_mask |= GDK_POINTER_MOTION_MASK;
  event_mask |= GDK_POINTER_MOTION_HINT_MASK;
  event_mask |= GDK_SCROLL_MASK;
  gtk_widget_add_events (GTK_WIDGET (gtk_map), event_mask);
  gtk_widget_set_can_focus (GTK_WIDGET (gtk_map), TRUE);
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

/* Переводит координаты логической СК в географическую. */
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
  guint border = 25;

  *border_bottom = border;
  *border_top = border;
  *border_left = border;
  *border_right = border;
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

gconstpointer
hyscan_gtk_map_get_howner (HyScanGtkMap *map)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), NULL);

  return map->priv->howner;
}

void
hyscan_gtk_map_set_howner (HyScanGtkMap  *map,
                           gconstpointer  howner)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  map->priv->howner = howner;
}

gboolean
hyscan_gtk_map_is_howner (HyScanGtkMap  *map,
                          gconstpointer  howner)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), FALSE);

  return howner == map->priv->howner;
}

void
hyscan_gtk_map_release_input (HyScanGtkMap  *map,
                              gconstpointer  howner)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  if (howner == map->priv->howner)
    map->priv->howner = NULL;
}

gboolean
hyscan_gtk_map_grab_input (HyScanGtkMap  *map,
                           gconstpointer  howner)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), FALSE);

  if (map->priv->howner == NULL)
    map->priv->howner = howner;

  return howner == map->priv->howner;
}