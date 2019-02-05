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
  guint                  zoom;                 /* Текущий масштаб карты. */
  guint                  max_zoom;             /* Максимальный масштаб (включительно). */
  guint                  min_zoom;             /* Минимальный масштаб (включительно). */
  gdouble                scale;                /* Увеличение размера тайлов. */
  gdouble                max_value;            /* Границы логической СК. */

  guint                  tile_size_real;       /* Реальный размер тайла. */

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
static void     hyscan_gtk_map_get_limits               (GtkCifroArea          *carea,
                                                         gdouble               *min_x,
                                                         gdouble               *max_x,
                                                         gdouble               *min_y,
                                                         gdouble               *max_y);
static void     hyscan_gtk_map_set_tile_view            (HyScanGtkMap          *map,
                                                         gdouble                from_tile_x,
                                                         gdouble                to_tile_x,
                                                         gdouble                from_tile_y,
                                                         gdouble                to_tile_y);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMap, hyscan_gtk_map, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_map_class_init (HyScanGtkMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);
  HyScanGtkMapClass *map_class = HYSCAN_GTK_MAP_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_set_property;
  object_class->constructed = hyscan_gtk_map_object_constructed;
  object_class->finalize = hyscan_gtk_map_object_finalize;

  /* Обработчики взаимодействия мышки пользователя с картой. */
  widget_class->button_press_event = hyscan_gtk_map_button_press_release;
  widget_class->button_release_event = hyscan_gtk_map_button_press_release;
  widget_class->motion_notify_event = hyscan_gtk_map_motion;
  widget_class->scroll_event = hyscan_gtk_map_scroll;

  /* Реализация виртуальных функций GtkCifroArea. */
  carea_class->get_limits = hyscan_gtk_map_get_limits;

  /* Перевод координат из тайлов в область рисования. todo: мб сделать из тайлов в гео, а карту прямоугольную? */
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
  HyScanGtkMapPrivate *priv = map->priv;

  /* Инициализирует границы видимой области, чтобы тайлы были правильного размера. */
  if (!priv->view_initialized)
    {
      guint width, height;
      gdouble x, y;
      gdouble tile_size;

      tile_size = priv->tile_size_real * priv->scale;

      gtk_cifro_area_get_size (carea, &width, &height);
      gtk_cifro_area_get_view (carea, &x, NULL, &y, NULL);
      if (FALSE) /* todo: cartesian map. */
        gtk_cifro_area_set_view (carea, x, x + 22200, y, y + 22200);
      else
        hyscan_gtk_map_set_tile_view (map, x, x + width / tile_size, y, y + height / tile_size);
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
  priv->min_zoom = 0;
  priv->max_zoom = 19;
  priv->max_value = pow (2, priv->max_zoom);
  priv->scale = 1;
  priv->tile_size_real = 256;
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
  gint dscale = 0;

  guint zoom;
  gdouble scale;

  if (event->direction == GDK_SCROLL_UP)
    dscale = 1;
  else if (event->direction == GDK_SCROLL_DOWN)
    dscale = -1;

  if (dscale == 0)
    return FALSE;

  zoom = priv->zoom;
  scale = priv->scale;

  /* Изменяем масштаб. При больших изменений масштаба меняем масштаб тайловой сетки. */
  scale *= pow (2, dscale * 0.2);
  if (scale > 1.5 && zoom < priv->max_zoom)
    {
      scale /= 2;
      zoom += 1;
    }
  else if (scale < .75 && zoom > priv->min_zoom)
    {
      scale *= 2;
      zoom -= 1;
    }

  g_message ("Zoom %d, scale %.2f", zoom, scale);
  hyscan_gtk_map_set_zoom (map, zoom);
  hyscan_gtk_map_set_tile_scaling (map, scale);

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
      if ((clip_width <= 0) || (clip_height <= 0))
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

/* Границы системы координат тайлов. */
static void
hyscan_gtk_map_get_limits (GtkCifroArea *carea,
                           gdouble      *min_x,
                           gdouble      *max_x,
                           gdouble      *min_y,
                           gdouble      *max_y)
{
  HyScanGtkMapPrivate *priv = HYSCAN_GTK_MAP (carea)->priv;

  *min_x = 0;
  *min_y = 0;
  *max_x = priv->max_value;
  *max_y = priv->max_value;
}

/* Переводит из логической СК GtkCifroArea в СК проекции меркатора. */
static void
hyscan_gtk_map_tile_to_merc (HyScanGtkMap *map,
                             gdouble      *x,
                             gdouble      *y,
                             gdouble       x_tile,
                             gdouble       y_tile)
{
  HyScanGtkMapPrivate *priv = map->priv;
  gdouble tile_size;

  /* Размер тайла в логических единицах. */
  tile_size = pow (2, priv->max_zoom - priv->zoom);

  (y != NULL) ? *y = (hyscan_gtk_map_count_columns (priv->zoom) - y_tile) * tile_size : 0;
  (x != NULL) ? *x = x_tile * tile_size: 0;
}

/* Переводит из СК проекции меркатора в логическую СК GtkCifroArea. */
static void
hyscan_gtk_map_merc_to_tile (HyScanGtkMap *map,
                             gdouble       x,
                             gdouble       y,
                             gdouble      *x_tile,
                             gdouble      *y_tile)
{
  HyScanGtkMapPrivate *priv = map->priv;
  gdouble tile_size;

  /* Размер тайла в логических единицах. */
  tile_size = pow (2, priv->max_zoom - priv->zoom);

  (y_tile != NULL) ? *y_tile = (priv->max_value - y) / tile_size : 0;
  (x_tile != NULL) ? *x_tile = x / tile_size : 0;
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
 * hyscan_gtk_map_tile_to_point:
 * @map: указатель на карту #HyScanGtkMap
 * @x: (out) (nullable): координата x в видимой области
 * @y: (out) (nullable): координата y в видимой области
 * @x_tile: координата x тайла
 * @y_tile: координата y тайла
 *
 * Переводит координаты точки видимой области в тайловую систему координат.
 */
void
hyscan_gtk_map_point_to_tile (HyScanGtkMap *map,
                              gdouble       x,
                              gdouble       y,
                              gdouble      *x_tile,
                              gdouble      *y_tile)
{
  gdouble x_val, y_val;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  gtk_cifro_area_visible_point_to_value (GTK_CIFRO_AREA (map), x, y, &x_val, &y_val);
  hyscan_gtk_map_value_to_tile (map, x_val, y_val, x_tile, y_tile);
}

/* Получает границы тайлов, полность покрывающих видимую области. */
static void
hyscan_gtk_map_set_tile_view (HyScanGtkMap *map,
                              gdouble       from_tile_x,
                              gdouble       to_tile_x,
                              gdouble       from_tile_y,
                              gdouble       to_tile_y)
{
  gdouble from_x, to_x, from_y, to_y;
  
  hyscan_gtk_map_tile_to_value (map, &from_x, &from_y, from_tile_x, from_tile_y);
  hyscan_gtk_map_tile_to_value (map, &to_x, &to_y, to_tile_x, to_tile_y);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map),
                           MIN(from_x, to_x), MAX(from_x, to_x),
                           MIN(from_y, to_y), MAX(from_y, to_y));
}

/* Получает границы тайлов, полность покрывающих видимую области. */
static void
hyscan_gtk_map_get_tile_view (HyScanGtkMap *map,
                              gdouble      *from_tile_x,
                              gdouble      *to_tile_x,
                              gdouble      *from_tile_y,
                              gdouble      *to_tile_y)
{
  gdouble from_x, to_x, from_y, to_y;
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;
  gboolean invert;

  /* Переводим из логической СК в тайловую. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_tile (map, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_gtk_map_value_to_tile (map, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  invert = (from_tile_y_d > to_tile_y_d);
  (to_tile_y != NULL) ? *to_tile_y = (invert ? from_tile_y_d : to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = (invert ? to_tile_y_d : from_tile_y_d) : 0;

  invert = (from_tile_x_d > to_tile_x_d);
  (to_tile_x != NULL) ? *to_tile_x = (invert ? from_tile_x_d : to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = (invert ? to_tile_x_d : from_tile_x_d) : 0;
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
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  hyscan_gtk_map_get_tile_view (map, &from_tile_x_d, &to_tile_x_d, &from_tile_y_d, &to_tile_y_d);

  (to_tile_y != NULL) ? *to_tile_y = (guint) to_tile_y_d : 0;
  (from_tile_y != NULL) ? *from_tile_y = (guint) from_tile_y_d : 0;
  (to_tile_x != NULL) ? *to_tile_x = (guint) to_tile_x_d : 0;
  (from_tile_x != NULL) ? *from_tile_x = (guint) from_tile_x_d : 0;
}

gdouble
hyscan_gtk_map_get_tile_scaling (HyScanGtkMap *map)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), 0);

  return map->priv->scale;
}

guint
hyscan_gtk_map_get_tile_size (HyScanGtkMap *map)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), 0);

  return map->priv->tile_size_real;
}

/**
 * hyscan_gtk_map_set_tile_scaling:
 * @map: указатель на #HyScanGtkMap
 * @scaling: степень увеличения тайла (1.0 - без изменения)
 *
 * Изменяет масштаб за счёт изменения размера тайла в @scaling раз при том же
 * размере тайловой сетки (zoom). Увеличение тайла в 2 раза соответствует
 * масштабу следующей тайловой сетки (zoom + 1).
 *
 * Для реального изменения детализации карты используйте hyscan_gtk_map_set_zoom().
 */
void
hyscan_gtk_map_set_tile_scaling (HyScanGtkMap *map,
                                 gdouble       scaling)
{
  GtkCifroArea *carea;
  guint widget_width, widget_height;
  gdouble width, height;
  gdouble from_x, from_y, to_x, to_y;
  gdouble x, y;
  gdouble tile_size;

  carea = GTK_CIFRO_AREA (map);

  map->priv->scale = scaling;
  tile_size = map->priv->tile_size_real * map->priv->scale;

  /* Определяем центр видимой области. */
  hyscan_gtk_map_get_tile_view (map, &from_x, &to_x, &from_y, &to_y);
  x = (from_x + to_x) / 2;
  y = (from_y + to_y) / 2;

  /* Определяем размер видимой области в растянутых тайлах. */
  gtk_cifro_area_get_size (carea, &widget_width, &widget_height);
  width = widget_width / tile_size;
  height = widget_height / tile_size;

  /* Меняем границу видимой области согласно новым размерам, сохраняя центр. */
  hyscan_gtk_map_set_tile_view (map, x - width / 2.0,  x + width / 2.0,
                                     y - height / 2.0, y + height / 2.0);
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
  gdouble x, y;
  gdouble width, height;
  gdouble diff_factor;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  if (zoom < priv->min_zoom || zoom > priv->max_zoom)
    {
      g_warning ("HyScanGtkMap: Zoom must be in interval from %d to %d", priv->min_zoom, priv->max_zoom);
      return;
    }

  /* Получаем координаты центра в логических координатах. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &x0_value, &xn_value, &y0_value, &yn_value);
  x = (x0_value + xn_value) / 2;
  y = (y0_value + yn_value) / 2;

  /* Определяем размер новой области так, чтобы количество видимых тайлов не изменилось. */
  diff_factor = pow (2, (gdouble) priv->zoom - (gdouble) zoom);
  width = (xn_value - x0_value) * diff_factor;
  height = (yn_value - y0_value) * diff_factor;

  /* Меняем масштаб. */
  priv->zoom = zoom;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), x - width / 2.0, x + width / 2.0, y - height / 2.0, y + height / 2.0);
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
  guint width, height;
  guint dx = 10;
  gdouble x_tile1, y_tile1, x_tile2, y_tile2;
  gdouble distance;
  HyScanGeoGeodetic coord1, coord2;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  priv = map->priv;

  /* Берём две точки в видимой области на небольшом расстоянии в dx пикселей. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (map), &width, &height);
  hyscan_gtk_map_point_to_tile (map, width / 2.0 - dx / 2.0, height / 2.0, &x_tile1, &y_tile1);
  hyscan_gtk_map_point_to_tile (map, width / 2.0 + dx / 2.0, height / 2.0, &x_tile2, &y_tile2);

  /* Определяем географические координаты точек. */
  hyscan_gtk_map_tile_to_geo (priv->zoom, &coord1, x_tile1, y_tile1);
  hyscan_gtk_map_tile_to_geo (priv->zoom, &coord2, x_tile2, y_tile2);

  /* Считаем расстояние между coord1 и coord2: Haversine formula */
  {
    gdouble R = 6.371e6;
    gdouble c;
    gdouble a;
    gdouble deg2rad = M_PI / 180;
    gdouble d_lat = deg2rad * (coord2.lat - coord1.lat);
    gdouble d_lon = deg2rad * (coord2.lon - coord1.lon);

    a = pow (sin (d_lat / 2), 2) + cos (deg2rad * coord1.lat) * cos (deg2rad * coord2.lat) * pow (sin (d_lon / 2), 2);
    c = 2 * atan2 (sqrt (a), sqrt (1.0 - a));
    distance = R * c;
  }

  return dx / distance;
}