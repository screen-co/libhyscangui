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
  HyScanMercator          *projection;           /* Проекция поверхности Земли на плоскость карты. */

  /* Обработка перемещения карты мышью. */
  gboolean                 move_area;            /* Признак перемещения при нажатой клавише мыши. */
  gdouble                  start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                  start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                  start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                  start_to_y;           /* Начальная граница отображения по оси Y. */
  gint                     move_from_x;          /* Начальная координата перемещения. */
  gint                     move_from_y;          /* Начальная координата перемещения. */
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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);

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
  hyscan_mercator_value_to_geo (priv->projection, coords, x_val, y_val);
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

  hyscan_mercator_get_limits (priv->projection, min_x, max_x, min_y, max_y);
}

GtkWidget *
hyscan_gtk_map_new ()
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP, NULL);
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
  hyscan_mercator_geo_to_value (priv->projection, center, &center_x, &center_y);

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
  dist_metres = hyscan_mercator_get_scale (priv->projection, geo_center);

  /* Определяем размер единичного отрезка на проекции в пикселах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  dist_pixels = 1 / scale;

  return dist_pixels / dist_metres;
}