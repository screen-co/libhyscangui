/**
 * Класс карты
 */

#include <math.h>
#include "hyscan-gtk-map.h"

enum
{
  PROP_O,
};

struct _HyScanGtkMapPrivate
{
  guint             zoom;     /* Текущий масштаб карты. */
};

static void    hyscan_gtk_map_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_gtk_map_object_constructed       (GObject               *object);
static void    hyscan_gtk_map_object_finalize          (GObject               *object);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, GTK_TYPE_CIFRO_AREA_CONTROL)

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_set_property;

  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

}

/* Функция рисования параметрической кривой. */
static void
hyscan_gtk_map_visible_draw (GtkWidget *widget,
                             cairo_t   *cairo)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;
  /* todo: ??? */
}

/* Функция обработки сигнала нажатия кнопки мыши. */
static gboolean
hyscan_gtk_map_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  return FALSE;
}


static gboolean
hyscan_gtk_map_button_release_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  return FALSE;
}

/* Функция обработки сигнала движения мышки. */
static gboolean
hyscan_gtk_map_motion_notify_event (GtkWidget      *widget,
                                    GdkEventMotion *event)
{
  return FALSE;
}

static void
hyscan_gtk_map_init (HyScanGtkMap *gtk_map)
{
  gtk_map->priv = hyscan_gtk_map_get_instance_private (gtk_map);

  /* Обработчики сигналов. */
  g_signal_connect (gtk_map, "visible-draw", G_CALLBACK (hyscan_gtk_map_visible_draw), NULL);
  g_signal_connect (gtk_map, "button-press-event", G_CALLBACK (hyscan_gtk_map_button_press_event), NULL);
  g_signal_connect (gtk_map, "button-release-event", G_CALLBACK (hyscan_gtk_map_button_release_event), NULL);
  g_signal_connect (gtk_map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_motion_notify_event), NULL);
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

  priv->zoom = 7;
}

static void
hyscan_gtk_map_object_finalize (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->finalize (object);
}

/* Перводит координаты тайла подвижной карты (Slippy map) в широту и долготу.
 * Источник: https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames. */
void
hyscan_gtk_map_tile_to_geo (guint              zoom,
                            HyScanGeoGeodetic *coords,
                            gdouble            tile_x,
                            gdouble            tile_y)
{
  gdouble n;
  gdouble lat_tmp;

  n = pow (2.0, zoom);

  coords->lon = tile_x / n * 360.0 - 180.0;

  lat_tmp = M_PI - 2.0 * M_PI * tile_y / n;
  coords->lat = 180.0 / M_PI * atan (0.5 * (exp (lat_tmp) - exp (-lat_tmp)));
}

/* Перводит координаты широты и долготы в координаты тайла подвижной карты (Slippy map).
 * Источник: https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames. */
void
hyscan_gtk_map_geo_to_tile (guint              zoom,
                            HyScanGeoGeodetic  coords,
                            gdouble           *tile_x,
                            gdouble           *tile_y)
{
  gint total_tiles;
  gdouble lat_rad;

  lat_rad = coords.lat / 180.0 * M_PI;

  total_tiles = (gint) pow (2, zoom);

  (tile_x != NULL) ? *tile_x = total_tiles * (coords.lon + 180.0) / 360.0 : 0;
  (tile_y != NULL) ? *tile_y = total_tiles * (1 - log (tan (lat_rad) + (1 / cos (lat_rad))) / M_PI) / 2 : 0;
}

/* Устанавливает границы видимой области в координатах тайлов. */
static void
hyscan_gtk_map_set_tile_view (HyScanGtkMap *map,
                              gdouble       from_tile_x,
                              gdouble       to_tile_x,
                              gdouble       from_tile_y,
                              gdouble       to_tile_y)
{
  gdouble total_tiles;
  
  total_tiles = pow (2, map->priv->zoom);
  
  /* Меняем направление по оси OY, так как нумерация тайлов идёт сверху вниз. */
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map),
                           from_tile_x, to_tile_x,
                           total_tiles - to_tile_y, total_tiles - from_tile_y);
}

GtkWidget *
hyscan_gtk_map_new ()
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP, NULL);
}

/**
 * hyscan_gtk_map_tile_to_point:
 * @map: указатель на карту #HyScanGtkMap
 * @x: (out) (nullable): координата x в видимой области
 * @y: (out) (nullable): координата y в видимой области
 * @x_tile: координата x тайла
 * @y_tile: координата y тайла
 *
 * Переводит координаты тайла в прямоугольную систему координат видимой области.
 */
void
hyscan_gtk_map_tile_to_point (HyScanGtkMap *map,
                              gdouble      *x,
                              gdouble      *y,
                              gdouble       x_tile,
                              gdouble       y_tile)
{
  HyScanGtkMapPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;

  /* todo: pow() с целыми числами вроде как-то не очень. */
  y_tile = pow (2, priv->zoom) - y_tile;
  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (map), x, y, x_tile, y_tile);
}

/**
 * hyscan_gtk_map_get_tile_view:
 * @map: указатель на карту #HyScanGtkMap
 * @from_tile_x: (out) (nullable): координаты верхнего левого угла
 * @to_tile_x: (out) (nullable): координаты нижнего правого угла
 * @from_tile_y: (out) (nullable): координаты верхнего левого угла
 * @to_tile_y: (out) (nullable): координаты нижнего правого угла
 *
 * Получает границы видимой области в системе координат тайлов.
 */
void
hyscan_gtk_map_get_tile_view (HyScanGtkMap *map,
                              gdouble      *from_tile_x,
                              gdouble      *to_tile_x,
                              gdouble      *from_tile_y,
                              gdouble      *to_tile_y)
{
  gdouble total_tiles;

  total_tiles = pow (2, map->priv->zoom);

  /* Меняем направление по оси OY, так как нумерация тайлов идёт сверху вниз. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map),
                           from_tile_x, to_tile_x,
                           to_tile_y, from_tile_y);

  (to_tile_y != NULL) ? *to_tile_y = total_tiles - *to_tile_y : 0;
  (from_tile_y != NULL) ? *from_tile_y = total_tiles - *from_tile_y : 0;
}

/**
 * hyscan_gtk_map_get_tile_view_i:
 * @map: указатель на карту #HyScanGtkMap
 * @from_tile_x: (out) (nullable): координаты верхнего левого тайла по x
 * @to_tile_x: (out) (nullable): координаты нижнего правого тайла по x
 * @from_tile_y: (out) (nullable): координаты верхнего левого тайла по x
 * @to_tile_y: (out) (nullable): координаты нижнего правого тайла по y
 *
 * Получает границы тайлов, полность покрывающих видимую области.
 *
 */
void
hyscan_gtk_map_get_tile_view_i (HyScanGtkMap *map,
                                guint        *from_tile_x,
                                guint        *to_tile_x,
                                guint        *from_tile_y,
                                guint        *to_tile_y)
{
  guint total_tiles;
  gdouble from_x, to_x, from_y, to_y;

  total_tiles = (guint) pow (2, map->priv->zoom);

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map),
                           &from_x, &to_x,
                           &from_y, &to_y);

  /* Меняем направление по оси OY, так как нумерация тайлов идёт сверху вниз.
   * y округляем вверх, x округляем вниз. */
  (to_tile_y != NULL) ? *to_tile_y = total_tiles - (guint) ceil (from_y) : 0;
  (from_tile_y != NULL) ? *from_tile_y = total_tiles - (guint) ceil (to_y) : 0;
  (to_tile_x != NULL) ? *to_tile_x = (guint) to_x : 0;
  (from_tile_x != NULL) ? *from_tile_x = (guint) from_x : 0;
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

  guint visible_width, visible_height;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  carea = GTK_CIFRO_AREA (map);

  /* Определяем текущие размеры показываемой области. */
  gtk_cifro_area_get_visible_size (carea, &visible_width, &visible_height);
  gtk_cifro_area_get_size (carea, &visible_width, &visible_height);
  width = visible_width / 256.0;
  height = visible_height / 256.0;

  hyscan_gtk_map_geo_to_tile (priv->zoom, center, &center_x, &center_y);

  hyscan_gtk_map_set_tile_view (map,
                                center_x - width / 2.0, center_x + width / 2.0,
                                center_y - height / 2.0, center_y + height / 2.0);
}

/**
 * hyscan_gtk_map_get_zoom:
 * @map: указатель на #HyScanGtkMap
 *
 * Возвращает текущий масштаб карты.
 *
 * Returns: текущий масштаб карты
 */
guint
hyscan_gtk_map_get_zoom (HyScanGtkMap *map)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), 0);

  return map->priv->zoom;
}

/**
 * hyscan_gtk_map_set_zoom:
 * @map: указатель на #HyScanGtkMap
 * @zoom: масштаб карты (от 1 до 19)
 *
 * Устанавливает новый масштаб карты.
 */
void
hyscan_gtk_map_set_zoom (HyScanGtkMap *map,
                         guint         zoom)
{
  HyScanGtkMapPrivate *priv;

  gdouble x0_value, y0_value, xn_value, yn_value;
  HyScanGeoGeodetic center;
  
  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));
  g_return_if_fail (zoom > 0 && zoom < 20);
  
  priv = map->priv;
  
  /* Получаем координаты центрального тайла в (lat, lon). */
  hyscan_gtk_map_get_tile_view (map, &x0_value, &xn_value, &y0_value, &yn_value);
  hyscan_gtk_map_tile_to_geo (priv->zoom, &center,
                              (xn_value + x0_value) / 2,
                              (yn_value + y0_value) / 2);

  /* Меняем масштаб. */
  priv->zoom = zoom;

  /* Перемещаем карту в старый центр. */
  hyscan_gtk_map_move_to (map, center);
}