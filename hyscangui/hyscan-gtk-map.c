/**
 * Класс карты
 */

#include "hyscan-gtk-map.h"
#include <math.h>

enum
{
  PROP_O,
};

struct _HyScanGtkMapPrivate
{
  guint             zoom;                      /* Текущий масштаб карты. */

  /* Обработка перемещения карты мышью. */
  gboolean               move_area;            /* Признак перемещения при нажатой клавише мыши. */
  gdouble                start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                start_to_y;           /* Начальная граница отображения по оси Y. */
  gint                   move_from_x;          /* Начальная координата перемещения. */
  gint                   move_from_y;          /* Начальная координата перемещения. */

  gboolean               view_initialized;     /* Флаг о том, что область отображения инициализирована. */
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
static guint    hyscan_gtk_map_count_columns            (guint                  zoom);
static void     hyscan_gtk_map_tile_to_merc             (HyScanGtkMap          *map,
                                                         gdouble               *x,
                                                         gdouble               *y,
                                                         gdouble                x_tile,
                                                         gdouble                y_tile);
static void     hyscan_gtk_map_merc_to_tile             (HyScanGtkMap          *map,
                                                         gdouble                x,
                                                         gdouble                y,
                                                         gdouble               *x_tile,
                                                         gdouble               *y_tile);
static gboolean hyscan_gtk_map_configure                (GtkWidget             *widget,
                                                         GdkEventConfigure     *event,
                                                         gpointer               user_data);
static void     hyscan_gtk_map_tile_to_value            (HyScanGtkMap          *map,
                                                         gdouble               *x_val,
                                                         gdouble               *y_val,
                                                         gdouble                x_tile,
                                                         gdouble                y_tile);
static void     hyscan_gtk_map_value_to_tile            (HyScanGtkMap          *map,
                                                         gdouble                x,
                                                         gdouble                y,
                                                         gdouble               *x_tile,
                                                         gdouble               *y_tile);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  HyScanGtkMapClass *map_class = HYSCAN_GTK_MAP_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_set_property;
  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

  /* Обработчики взаимодействия мышки пользователя с картой. */
  widget_class->button_press_event = hyscan_gtk_map_button_press_release;
  widget_class->button_release_event = hyscan_gtk_map_button_press_release;
  widget_class->motion_notify_event = hyscan_gtk_map_motion;
  widget_class->scroll_event = hyscan_gtk_map_scroll;

  map_class->tile_to_point = hyscan_gtk_map_tile_to_merc;
  map_class->point_to_tile = hyscan_gtk_map_merc_to_tile;
}

/* Обработчик сигнала "configure-event". */
static gboolean
hyscan_gtk_map_configure (GtkWidget          *widget,
                          GdkEventConfigure  *event,
                          gpointer            user_data)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);

  /* Инициализирует границы видимой области, чтобы размер тайла был 256 пикселей. */
  if (!map->priv->view_initialized)
    {
      guint width, height;
      gdouble x, y;

      gtk_cifro_area_get_size (carea, &width, &height);
      gtk_cifro_area_get_view (carea, &x, NULL, &y, NULL);
      gtk_cifro_area_set_view (carea, x, x + width / 256.0, y, y + height / 256.0);
      map->priv->view_initialized = TRUE;
    }

  return FALSE;
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
  g_signal_connect_after (gtk_map, "configure-event", G_CALLBACK (hyscan_gtk_map_configure), NULL);

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
          priv->move_from_x = (gint) event->x;
          priv->move_from_y = (gint) event->y;
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

/* Определяет общее количество тайлов по выстое при указанном масштабе. */
static guint
hyscan_gtk_map_count_columns (guint zoom)
{
  return (guint) ((zoom != 0) ? 2 << (zoom - 1) : 1);
}

/* Переводит географические координаты в логическую СК. */
static void
hyscan_gtk_map_geo_to_value (HyScanGtkMap      *map,
                             HyScanGeoGeodetic  coords,
                             gdouble           *x_val,
                             gdouble           *y_val)
{
  gdouble x_tile, y_tile;

  hyscan_gtk_map_geo_to_tile (map->priv->zoom, coords, &x_tile, &y_tile);
  hyscan_gtk_map_tile_to_value (map, x_val, y_val, x_tile, y_tile);
}

/* Переводит координаты логической СК в географическую. */
static void
hyscan_gtk_map_value_to_geo (HyScanGtkMap       *map,
                             HyScanGeoGeodetic  *coords,
                             gdouble             x_val,
                             gdouble             y_val)
{
  gdouble x_tile, y_tile;

  hyscan_gtk_map_value_to_tile (map, x_val, y_val, &x_tile, &y_tile);
  hyscan_gtk_map_tile_to_geo (map->priv->zoom, coords, x_tile, y_tile);
}

/* Переводит из СК тайла в СК проекции меркатора. */
static void
hyscan_gtk_map_tile_to_merc (HyScanGtkMap *map,
                             gdouble      *x,
                             gdouble      *y,
                             gdouble       x_tile,
                             gdouble       y_tile)
{
  HyScanGtkMapPrivate *priv = map->priv;

  *y = hyscan_gtk_map_count_columns (priv->zoom) - y_tile;
  *x = x_tile;
}

/* Переводит из СК проекции меркатора в СК тайлов. */
static void
hyscan_gtk_map_merc_to_tile (HyScanGtkMap *map,
                             gdouble       x,
                             gdouble       y,
                             gdouble      *x_tile,
                             gdouble      *y_tile)
{
  HyScanGtkMapPrivate *priv = map->priv;

  *y_tile = hyscan_gtk_map_count_columns (priv->zoom) - y;
  *x_tile = x;
}

/* Переводит из СК тайлов в логическую СК. */
static void
hyscan_gtk_map_tile_to_value (HyScanGtkMap *map,
                              gdouble      *x_val,
                              gdouble      *y_val,
                              gdouble       x_tile,
                              gdouble       y_tile)
{
  HyScanGtkMapClass *klass;

  klass = HYSCAN_GTK_MAP_GET_CLASS (map);
  g_return_if_fail (klass->tile_to_point != NULL);
  klass->tile_to_point (map, x_val, y_val, x_tile, y_tile);
}

/* Переводит из логической СК в СК тайлов. */
static void
hyscan_gtk_map_value_to_tile (HyScanGtkMap *map,
                              gdouble       x,
                              gdouble       y,
                              gdouble      *x_tile,
                              gdouble      *y_tile)
{
  HyScanGtkMapClass *klass;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  klass = HYSCAN_GTK_MAP_GET_CLASS (map);
  g_return_if_fail (klass->point_to_tile != NULL);
  klass->point_to_tile (map, x, y, x_tile, y_tile);
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
 * Переводит координаты тайла в систему координат видимой области.
 */
void
hyscan_gtk_map_tile_to_point (HyScanGtkMap *map,
                              gdouble      *x,
                              gdouble      *y,
                              gdouble       x_tile,
                              gdouble       y_tile)
{
  gdouble x_val, y_val;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  hyscan_gtk_map_tile_to_value (map, &x_val, &y_val, x_tile, y_tile);
  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (map), x, y, x_val, y_val);
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
  gdouble from_x, to_x, from_y, to_y;
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;
  gboolean invert;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  
  /* Переводим из логической СК в тайловую. */
  hyscan_gtk_map_value_to_tile (map, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_gtk_map_value_to_tile (map, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  invert = (from_tile_y_d > to_tile_y_d);
  (to_tile_y != NULL) ? *to_tile_y = (guint) (invert ? from_tile_y_d : to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = (guint) (invert ? to_tile_y_d : from_tile_y_d) : 0;
    
  invert = (from_tile_x_d > to_tile_x_d);
  (to_tile_x != NULL) ? *to_tile_x = (guint) (invert ? from_tile_x_d : to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = (guint) (invert ? to_tile_x_d : from_tile_x_d) : 0;
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
  GtkCifroArea *carea;
  gdouble center_x, center_y;
  gdouble width, height;

  gdouble from_x, to_x, from_y, to_y;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  carea = GTK_CIFRO_AREA (map);

  /* Определяем текущие размеры показываемой области. */
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  width = to_x - from_x;
  height = to_y - from_y;

  /* Получаем координаты точки в логической СК. */
  hyscan_gtk_map_geo_to_value (map, center, &center_x, &center_y);

  /* Перемещаем показываемую область в новый центр. */
  gtk_cifro_area_set_view (carea,
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
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &x0_value, &xn_value, &y0_value, &yn_value);
  hyscan_gtk_map_value_to_geo (map, &center, (xn_value + x0_value) / 2, (yn_value + y0_value) / 2);

  /* Меняем масштаб. */
  priv->zoom = zoom;

  /* Перемещаем карту в старый центр. */
  hyscan_gtk_map_move_to (map, center);
}

/* Перводит координаты тайла подвижной карты (Slippy map) в широту и долготу.
 * Источник: https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames. */
void
hyscan_gtk_map_tile_to_geo (guint              zoom,
                            HyScanGeoGeodetic *coords,
                            gdouble            tile_x,
                            gdouble            tile_y)
{
  guint n;
  gdouble lat_tmp;

  n = hyscan_gtk_map_count_columns (zoom);

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

  total_tiles = hyscan_gtk_map_count_columns (zoom);

  (tile_x != NULL) ? *tile_x = total_tiles * (coords.lon + 180.0) / 360.0 : 0;
  (tile_y != NULL) ? *tile_y = total_tiles * (1 - log (tan (lat_rad) + (1 / cos (lat_rad))) / M_PI) / 2 : 0;
}