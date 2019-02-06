/**
 * Класс карты
 */

#include "hyscan-gtk-map.h"
#include "hyscan-mercator.h"
#include <math.h>

enum
{
  PROP_O,
};

struct _HyScanGtkMapPrivate
{
  guint                    zoom;                 /* Текущий масштаб карты. */
  guint                    max_zoom;             /* Максимальный масштаб (включительно). */
  guint                    min_zoom;             /* Минимальный масштаб (включительно). */
  gdouble                  max_value;            /* Границы логической СК. */

  HyScanMercator          *projection;           /* Проекция поверхности Земли на плоскость карты. */

  guint                    tile_size_real;       /* Реальный размер тайла в пискселах. */
  HyScanGeoEllipsoidParam  ellipsoid;            /* Параметры эллипсоида для проекции Меркатора. */

  /* Обработка перемещения карты мышью. */
  gboolean                 move_area;            /* Признак перемещения при нажатой клавише мыши. */
  gdouble                  start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                  start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                  start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                  start_to_y;           /* Начальная граница отображения по оси Y. */
  gint                     move_from_x;          /* Начальная координата перемещения. */
  gint                     move_from_y;          /* Начальная координата перемещения. */

  gboolean                 view_initialized;     /* Флаг о том, что область отображения инициализирована. */
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
static void     hyscan_gtk_map_get_border               (GtkCifroArea          *carea,
                                                         guint                 *border_top,
                                                         guint                 *border_bottom,
                                                         guint                 *border_left,
                                                         guint                 *border_right);
static void     hyscan_gtk_map_check_scale              (GtkCifroArea          *carea,
                                                         gdouble               *scale_x,
                                                         gdouble               *scale_y);
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
  carea_class->get_border = hyscan_gtk_map_get_border;
  carea_class->check_scale = hyscan_gtk_map_check_scale;

  /* Перевод координат из тайлов в область рисования. todo: мб сделать из тайлов в гео, а карту прямоугольную? */
  map_class->tile_to_point = hyscan_gtk_map_tile_to_merc;
  map_class->point_to_tile = hyscan_gtk_map_merc_to_tile;
}

/* Устанавливает подходящий zoom в зависимости от выбранного масштаба. */
static void
hyscan_gtk_map_set_optimal_zoom (HyScanGtkMap *map)
{
  HyScanGtkMapPrivate *priv = map->priv;
  gdouble scale;
  gdouble optimal_zoom;
  guint izoom;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);

  /* Подбираем зум, чтобы тайлы не растягивались...
   * scale = pow (2, max_zoom - optimal_zoom) / tile_size_real */
  optimal_zoom = priv->max_zoom - log2 (scale * priv->tile_size_real);

  /* ... но поскольку zoom должен быть целочисленным, то окргуляем его (желательно вверх). */
  izoom = (guint) optimal_zoom;
  priv->zoom = (optimal_zoom - izoom) > 0.2 ? izoom + 1 : izoom;
}

/* Обработчик сигнала "configure-event". */
static gboolean
hyscan_gtk_map_configure (GtkWidget          *widget,
                          GdkEventConfigure  *event,
                          gpointer            user_data)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);
  HyScanGtkMapPrivate *priv = map->priv;

  /* Инициализирует границы видимой области, чтобы тайлы были правильного размера. */
  if (!priv->view_initialized)
    {
      gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), 0, map->priv->max_value, 0, map->priv->max_value);
      hyscan_gtk_map_set_optimal_zoom (map);
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
  priv->tile_size_real = 256;
  {
    HyScanGeoEllipsoidParam p;
    hyscan_geo_init_ellipsoid_user (&p, 6372.7982e3, 0.0);
    priv->projection = hyscan_mercator_new (p);
  }
}

static void
hyscan_gtk_map_object_finalize (GObject *object)
{
  HyScanGtkMap *gtk_map = HYSCAN_GTK_MAP (object);
  HyScanGtkMapPrivate *priv = gtk_map->priv;

  g_object_unref (priv->projection);

  G_OBJECT_CLASS (hyscan_gtk_map_parent_class)->finalize (object);
}

/* Обработчик событий прокрутки колёсика мышки. */
static gboolean
hyscan_gtk_map_scroll (GtkWidget      *widget,
                       GdkEventScroll *event)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (widget);

  gdouble dscale;
  gdouble scale;

  gdouble from_x, to_x, from_y, to_y;

  if (event->direction == GDK_SCROLL_UP)
    dscale = -1;
  else if (event->direction == GDK_SCROLL_DOWN)
    dscale = 1;
  else
    return FALSE;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  /* За 4 колесика изменяем масштаб в 2 раза. */
  scale *= pow (2, dscale * .25);

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (map), scale, scale, (from_x + to_x) / 2, (from_y + to_y) / 2);
  hyscan_gtk_map_set_optimal_zoom (map);

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
  HyScanGtkMapPrivate *priv = map->priv;
  gdouble x_tile, y_tile;

  hyscan_mercator_geo_to_tile (priv->projection, priv->zoom, coords, &x_tile, &y_tile);
  hyscan_gtk_map_tile_to_value (map, x_val, y_val, x_tile, y_tile);
}

/* Переводит координаты логической СК в географическую. */
void
hyscan_gtk_map_value_to_geo (HyScanGtkMap       *map,
                             HyScanGeoGeodetic  *coords,
                             gdouble             x_val,
                             gdouble             y_val)
{
  HyScanGtkMapPrivate *priv;
  gdouble x_tile, y_tile;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (map));

  priv = map->priv;
  hyscan_gtk_map_value_to_tile (map, x_val, y_val, &x_tile, &y_tile);
  hyscan_mercator_tile_to_geo (priv->projection, priv->zoom, coords, x_tile, y_tile);
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

/* Получает границы видимой области в СК тайлов. */
static void
hyscan_gtk_map_get_tile_view (HyScanGtkMap *map,
                              gdouble      *from_tile_x,
                              gdouble      *to_tile_x,
                              gdouble      *from_tile_y,
                              gdouble      *to_tile_y)
{
  gdouble from_x, to_x, from_y, to_y;
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  /* Переводим из логической СК в тайловую. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_tile (map, from_x, from_y, &from_tile_x_d, &from_tile_y_d);
  hyscan_gtk_map_value_to_tile (map, to_x, to_y, &to_tile_x_d, &to_tile_y_d);

  /* Устанавливаем границы так, чтобы выполнялось from_* < to_*. */
  (to_tile_y != NULL) ? *to_tile_y = MAX (from_tile_y_d, to_tile_y_d) : 0;
  (from_tile_y != NULL) ? *from_tile_y = MIN (from_tile_y_d, to_tile_y_d) : 0;

  (to_tile_x != NULL) ? *to_tile_x = MAX (from_tile_x_d, to_tile_x_d) : 0;
  (from_tile_x != NULL) ? *from_tile_x = MIN (from_tile_x_d, to_tile_x_d) : 0;
}

/**
 * hyscan_gtk_map_get_tile_view_i:
 * @map: указатель на карту #HyScanGtkMap
 * @from_tile_x: (out) (nullable): координаты верхнего левого тайла по x
 * @to_tile_x: (out) (nullable): координаты нижнего правого тайла по x
 * @from_tile_y: (out) (nullable): координаты верхнего левого тайла по x
 * @to_tile_y: (out) (nullable): координаты нижнего правого тайла по y
 *
 * Получает целочисленные координаты верхнего левого и правого нижнего тайлов,
 * полностью покрывающих видимую область.
 *
 */
void
hyscan_gtk_map_get_tile_view_i (HyScanGtkMap *map,
                                gint         *from_tile_x,
                                gint         *to_tile_x,
                                gint         *from_tile_y,
                                gint         *to_tile_y)
{
  gdouble from_tile_x_d, from_tile_y_d, to_tile_x_d, to_tile_y_d;

  hyscan_gtk_map_get_tile_view (map, &from_tile_x_d, &to_tile_x_d, &from_tile_y_d, &to_tile_y_d);

  (to_tile_y != NULL) ? *to_tile_y = (guint) to_tile_y_d : 0;
  (from_tile_y != NULL) ? *from_tile_y = (guint) from_tile_y_d : 0;
  (to_tile_x != NULL) ? *to_tile_x = (guint) to_tile_x_d : 0;
  (from_tile_x != NULL) ? *from_tile_x = (guint) from_tile_x_d : 0;
}

/* Расятжение тайла при текущем масштабе и зуме. */
gdouble
hyscan_gtk_map_get_tile_scaling (HyScanGtkMap *map)
{
  gdouble pixel_size;
  gdouble tile_size;
  HyScanGtkMapPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), 0);

  priv = map->priv;

  /* Размер пиксела в логических единицах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &pixel_size, NULL);

  /* Размер тайла в логических единицах. */
  tile_size = pow (2, priv->max_zoom - priv->zoom);

  return tile_size / (pixel_size * map->priv->tile_size_real);
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
  gdouble from_x, from_y, to_x, to_y;
  HyScanGtkMapPrivate *priv = map->priv;

  /* Переводим map scaling в carea scale. */
  scaling = pow (2, priv->max_zoom - priv->zoom) / (scaling * priv->tile_size_real);

  /* Меняем масштаб, сохраняя текущий центр. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (map), scaling, scaling, (from_x + to_x) / 2, (from_y + to_y) / 2);
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

  /* Меняем размер видимой области пропорционально изменению масштаба. */
  diff_factor = pow (2, (gdouble) priv->zoom - (gdouble) zoom);
  width = (xn_value - x0_value) * diff_factor;
  height = (yn_value - y0_value) * diff_factor;

  /* Меняем зум. */
  priv->zoom = zoom;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), x - width / 2.0, x + width / 2.0, y - height / 2.0, y + height / 2.0);
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

  gdouble tile_px;
  gdouble tile_metres;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP (map), -1.0);

  priv = map->priv;

  /* Определяем географические координаты центра видимой области. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_value_to_geo (map, &geo_center, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  /* Определяем размер логического тайла в метрах на текущей широте. */
  tile_metres = hyscan_mercator_get_scale (priv->projection, priv->max_zoom, geo_center);

  /* Определяем размер логического тайла в пикселах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  tile_px = 1 / scale;

  return tile_px / tile_metres;
}