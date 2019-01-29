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

  /* Обработка перемещения карты мышью. */
  gboolean               move_area;            /* Признак перемещения при нажатой клавише мыши. */
  gdouble                start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                start_to_y;           /* Начальная граница отображения по оси Y. */
  gint                   move_from_x;          /* Начальная координата перемещения. */
  gint                   move_from_y;          /* Начальная координата перемещения. */
};

static void     hyscan_gtk_map_set_property             (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void     hyscan_gtk_map_object_constructed       (GObject               *object);
static void     hyscan_gtk_map_object_finalize          (GObject               *object);
static gboolean hyscan_gtk_map_button_press_release     (GtkWidget             *widget,
                                                         GdkEventButton        *event);
static gboolean hyscan_gtk_map_motion                   (GtkWidget             *widget,
                                                         GdkEventMotion        *event);
static gboolean hyscan_gtk_map_scroll                   (GtkWidget             *widget,
                                                         GdkEventScroll        *event);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_set_property;
  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

  /* Обработчики взаимодействия мышки пользователя с картой. */
  widget_class->button_press_event = hyscan_gtk_map_button_press_release;
  widget_class->button_release_event = hyscan_gtk_map_button_press_release;
  widget_class->motion_notify_event = hyscan_gtk_map_motion;
  widget_class->scroll_event = hyscan_gtk_map_scroll;
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

/* Обработчик событий прокрутки колёсика мышки. */
static gboolean
hyscan_gtk_map_scroll (GtkWidget      *widget,
                       GdkEventScroll *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  if (event->direction == GDK_SCROLL_UP)
    hyscan_gtk_map_set_zoom (map, priv->zoom + 1);
  else if (event->direction == GDK_SCROLL_DOWN)
    hyscan_gtk_map_set_zoom (map, priv->zoom - 1);

  return FALSE;
}

/* Обработчик перемещений курсора мыши. */
static gboolean
hyscan_gtk_map_motion (GtkWidget      *widget,
                       GdkEventMotion *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  /* Режим перемещения - сдвигаем область. */
  if (priv->move_area)
    {
      gdouble x0, y0;
      gdouble xd, yd;
      gdouble dx, dy;

      gtk_cifro_area_point_to_value (carea, priv->move_from_x, priv->move_from_y, &x0, &y0);
      gtk_cifro_area_point_to_value (carea, event->x, event->y, &xd, &yd);
      dx = x0 - xd;
      dy = y0 - yd;

      gtk_cifro_area_set_view (carea, priv->start_from_x + dx, priv->start_to_x + dx,
                                      priv->start_from_y + dy, priv->start_to_y + dy);

      gdk_event_request_motions (event);
    }

  return FALSE;
}

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_map_button_press_release (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  /* Нажата левая клавиша мышки в видимой области - переходим в режим перемещения. */
  if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1))
    {
      guint widget_width, widget_height;
      guint border_top, border_bottom;
      guint border_left, border_right;
      gint clip_width, clip_height;

      gtk_cifro_area_get_size (carea, &widget_width, &widget_height);
      gtk_cifro_area_get_border (carea, &border_top, &border_bottom, &border_left, &border_right);

      clip_width = widget_width - border_left - border_right;
      clip_height = widget_height - border_top - border_bottom;
      if ((clip_width <= 0 ) || (clip_height <=0 ))
        return FALSE;

      if ((event->x > border_left) && (event->x < (border_left + clip_width)) &&
          (event->y > border_top) && (event->y < (border_top + clip_height)))
        {
          priv->move_area = TRUE;
          priv->move_from_x = event->x;
          priv->move_from_y = event->y;
          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  /* Выключаем режим перемещения. */
  if ((event->type == GDK_BUTTON_RELEASE) && (event->button == 1))
    {
      priv->move_area = FALSE;
    }

  return FALSE;
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

  /* todo: откуда брать эти значения? */
  if (zoom < 1 || zoom > 19)
    return;
  
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