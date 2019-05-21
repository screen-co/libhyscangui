/* hyscan-gtk-map-wfmark-layer.c
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
 * SECTION: hyscan-gtk-map-wfmark-layer
 * @Short_description: Класс слоя с метками водопада
 * @Title: HyScanGtkMapWfmarkLayer
 * @See_also: #HyScanGtkLayer, #HyScanGtkMapTiledLayer, #HyScanGtkMap, #GtkCifroArea
 *
 * Слой #HyScanGtkMapWfmarkLayer изображает на карте контуры меток водопада
 * указанного проекта.
 *
 * - hyscan_gtk_map_wfmark_layer_new() - создание нового слоя;
 * - hyscan_gtk_map_wfmark_layer_mark_view() - выделение указанной метки.
 *
 * Стиль слоя может быть настроен с помощью ini-файла:
 * - "mark-color" - цвет контура метки
 * - "mark-color-hover" - цвет контура выделенной метки
 * - "line-width" - толщина линий
 * - "text-color" - цвет текста подписи
 * - "bg-color" - цвет подложки текста
 * - "text-margin" - отступ текста от границ подложки
 * - "tooltip-margin" - отступ всплывающей подсказки от курсора мыши
 */

#include "hyscan-gtk-map-wfmark-layer.h"
#include <hyscan-cartesian.h>
#include <hyscan-gtk-map.h>
#include <math.h>

#define DISTANCE_TO_METERS      0.001                /* Коэффициент перевода размеров метки в метры. */

/* Оформление по умолчанию. */
#define MARK_COLOR              "#5A22F5"            /* Цвет обводки меток. */
#define MARK_COLOR_HOVER        "#F55A22"            /* Цвет обводки меток при наведении мыши. */
#define BG_COLOR                "rgba(0,0,0,0.4)"    /* Цвет фона подписи. */
#define TEXT_COLOR              "#FFFFFF"            /* Цвет текста подписи. */
#define LINE_WIDTH              2.0                  /* Толщина линии обводки. */
#define TEXT_MARGIN             5.0                  /* Отступт текста от границ. */
#define TOOLTIP_MARGIN          5.0                  /* Отступ всплывающей подсказки от указателя мыши. */

/* Раскомментируйте строку ниже для отладки положения меток относительно галса. */
// #define DEBUG_TRACK_POINTS

enum
{
  PROP_O,
  PROP_MODEL
};

typedef struct
{
  HyScanMarkLocation          *mloc;            /* Указатель на оригинальную метку (принадлежит priv->marks). */

  /* Поля ниже определяются переводом географических координат метки в СК карты с учетом текущей проекции. */
  HyScanGeoCartesian2D         track_c2d;       /* Координаты положения судна в СК карт. */
  HyScanGeoCartesian2D         center_c2d;      /* Координаты центра метки в СК карты. */

  gdouble                      angle;           /* Угол курса в радианах. */
  gdouble                      width;           /* Ширина метки (вдоль линии галса) в единицах СК карты. */
  gdouble                      height;          /* Длина метки (поперёк линии галса) в единицах СК карты. */

  /* Rect - прямоугольник метки, повернутый на угол курса так, что стороны прямоугольника вдоль осей координат. */
  HyScanGeoCartesian2D         rect_from;       /* Координаты левой верхней вершины прямоугольника rect. */
  HyScanGeoCartesian2D         rect_to;         /* Координаты правой нижней вершины прямоугольника rect. */

  /* Extent - прямоугольник, внутри которого находится метка; стороны прямоугольника вдоль осей координат. */
  HyScanGeoCartesian2D         extent_from;     /* Координаты левой верхней вершины прямоугольника extent. */
  HyScanGeoCartesian2D         extent_to;       /* Координаты правой нижней вершины прямоугольника extent. */
} HyScanGtkMapWfmarkLayerLocation;

struct _HyScanGtkMapWfmarkLayerPrivate
{
  HyScanGtkMap       *map;                                /* Карта. */
  gboolean            visible;                            /* Признак видимости слоя. */

  HyScanMarkLocModel *model;                              /* Модель данных. */

  GRWLock             mark_lock;                          /* Блокировка данных по меткам. .*/
  GHashTable         *marks;                              /* Хэш-таблица меток #HyScanGtkMapWfmarkLayerLocation. */

  const HyScanGtkMapWfmarkLayerLocation *location_hover;  /* Метка, над которой находится курсор мышин. */
  HyScanGeoCartesian2D                   cursor;          /* Координаты курсора в логической СК. */

  /* Стиль отображения. */
  PangoLayout     *pango_layout;                /* Раскладка шрифта. */
  GdkRGBA          color_default;               /* Цвет обводки меток. */
  GdkRGBA          color_hover;                 /* Цвет обводки метки при наведении курсора мыши. */
  GdkRGBA          color_bg;                    /* Цвет фона текста. */
  GdkRGBA          color_text;                  /* Цвет текста.*/
  gdouble          line_width;                  /* Толщина обводки. */
  gdouble          text_margin;                 /* Отступ текст от границ подложки. */
  gdouble          tooltip_margin;              /* Отступ всплывающей подсказки от указателя мыши. */
};

static void    hyscan_gtk_map_wfmark_layer_interface_init           (HyScanGtkLayerInterface *iface);
static void    hyscan_gtk_map_wfmark_layer_set_property             (GObject                 *object,
                                                                     guint                    prop_id,
                                                                     const GValue            *value,
                                                                     GParamSpec              *pspec);
static void    hyscan_gtk_map_wfmark_layer_object_constructed       (GObject                 *object);
static void    hyscan_gtk_map_wfmark_layer_object_finalize          (GObject                 *object);
static void    hyscan_gtk_map_wfmark_layer_model_changed            (HyScanGtkMapWfmarkLayer *wfm_layer);
static void    hyscan_gtk_map_wfmark_layer_location_free            (HyScanGtkMapWfmarkLayerLocation *location);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapWfmarkLayer, hyscan_gtk_map_wfmark_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapWfmarkLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_wfmark_layer_interface_init))

static void
hyscan_gtk_map_wfmark_layer_class_init (HyScanGtkMapWfmarkLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_wfmark_layer_set_property;
  object_class->constructed = hyscan_gtk_map_wfmark_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_wfmark_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("mark-loc-model", "Mark location model",
                         "The HyScanMarkLocModel containing information about marks and its locations",
                         HYSCAN_TYPE_MARK_LOC_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_gtk_map_wfmark_layer_init (HyScanGtkMapWfmarkLayer *gtk_map_wfmark_layer)
{
  gtk_map_wfmark_layer->priv = hyscan_gtk_map_wfmark_layer_get_instance_private (gtk_map_wfmark_layer);
}

static void
hyscan_gtk_map_wfmark_layer_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkMapWfmarkLayer *gtk_map_wfmark_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (object);
  HyScanGtkMapWfmarkLayerPrivate *priv = gtk_map_wfmark_layer->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_wfmark_layer_object_constructed (GObject *object)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (object);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_wfmark_layer_parent_class)->constructed (object);

  g_rw_lock_init (&priv->mark_lock);
  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_wfmark_layer_location_free);

  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_layer_model_changed), wfm_layer);

  /* Стиль оформления. */
  gdk_rgba_parse (&priv->color_default, MARK_COLOR);
  gdk_rgba_parse (&priv->color_hover, MARK_COLOR_HOVER);
  gdk_rgba_parse (&priv->color_bg, BG_COLOR);
  gdk_rgba_parse (&priv->color_text, TEXT_COLOR);
  priv->text_margin = TEXT_MARGIN;
  priv->tooltip_margin = TOOLTIP_MARGIN;
  priv->line_width = LINE_WIDTH;
}

static void
hyscan_gtk_map_wfmark_layer_object_finalize (GObject *object)
{
  HyScanGtkMapWfmarkLayer *gtk_map_wfmark_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (object);
  HyScanGtkMapWfmarkLayerPrivate *priv = gtk_map_wfmark_layer->priv;

  g_rw_lock_clear (&priv->mark_lock);

  g_hash_table_unref (priv->marks);
  g_object_unref (priv->model);

  g_clear_object (&priv->pango_layout);

  G_OBJECT_CLASS (hyscan_gtk_map_wfmark_layer_parent_class)->finalize (object);
}

/* Проецирует метку на текущую картографическую проекцию. */
static void
hyscan_gtk_map_wfmark_layer_project_location (HyScanGtkMapWfmarkLayer         *wfm_layer,
                                              HyScanGtkMapWfmarkLayerLocation *location)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  gdouble scale;
  gdouble offset;
  HyScanGeoCartesian2D mark_from, mark_to;

  if (!location->mloc->loaded)
    return;

  /* Переводим из метров в единицы картографической проекции. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, &location->mloc->center_geo);
  location->width = DISTANCE_TO_METERS * location->mloc->mark->width / scale;
  location->height = DISTANCE_TO_METERS * location->mloc->mark->height / scale;
  offset = location->mloc->offset / scale;

  /* Определяем координаты центра метки в СК проекции. */
  hyscan_gtk_map_geo_to_value (priv->map, location->mloc->center_geo, &location->track_c2d);

  location->angle = location->mloc->center_geo.h / 180.0 * G_PI;
  location->center_c2d.x = location->track_c2d.x - offset * cos (location->angle);
  location->center_c2d.y = location->track_c2d.y + offset * sin (location->angle);

  location->rect_from.x = location->center_c2d.x - location->width;
  location->rect_to.x = location->center_c2d.x + location->width;
  location->rect_from.y = location->center_c2d.y - location->height;
  location->rect_to.y = location->center_c2d.y + location->height;

  /* Определяем границы extent. */
  mark_from.x = location->center_c2d.x - location->width;
  mark_from.y = location->center_c2d.y - location->height;
  mark_to.x = location->center_c2d.x + location->width;
  mark_to.y = location->center_c2d.y + location->height;
  hyscan_cartesian_rotate_area (&mark_from, &mark_to, &location->center_c2d, location->angle,
                                &location->extent_from, &location->extent_to);
}

static HyScanGtkMapWfmarkLayerLocation *
hyscan_gtk_map_wfmark_layer_location_new (void)
{
  return g_slice_new (HyScanGtkMapWfmarkLayerLocation);
}

static void
hyscan_gtk_map_wfmark_layer_location_free (HyScanGtkMapWfmarkLayerLocation *location)
{
  hyscan_mark_location_free (location->mloc);
  g_slice_free (HyScanGtkMapWfmarkLayerLocation, location);
}

static gboolean
hyscan_gtk_map_wfmark_layer_insert_mark (gpointer  key,
                                         gpointer  value,
                                         gpointer  user_data)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (user_data);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  HyScanGtkMapWfmarkLayerLocation *location;

  location = hyscan_gtk_map_wfmark_layer_location_new ();
  location->mloc = value;

  hyscan_gtk_map_wfmark_layer_project_location (wfm_layer, location);
  g_hash_table_insert (priv->marks, key, location);

  return TRUE;
}

/* Обработчик сигнала HyScanMarkLocModel::changed.
 * Обновляет список меток. */
static void
hyscan_gtk_map_wfmark_layer_model_changed (HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  GList *list;
  GHashTable *marks;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Загружаем гео-данные по меткам. */
  // todo: сделать обновление текущего списка меток priv->marks, а не полную перезагрузку
  marks = hyscan_mark_loc_model_get (priv->model);

  g_rw_lock_writer_lock (&priv->mark_lock);

  g_hash_table_remove_all (priv->marks);
  g_hash_table_foreach_steal (marks, hyscan_gtk_map_wfmark_layer_insert_mark, wfm_layer);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_hash_table_unref (marks);
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_wfmark_layer_configure (HyScanGtkMapWfmarkLayer *wfm_layer,
                                       GdkEvent                *screen)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);

  return FALSE;
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_wfmark_layer_draw (HyScanGtkMap            *map,
                                  cairo_t                 *cairo,
                                  HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapWfmarkLayerLocation *location;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (wfm_layer)))
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    {
      gdouble x, y;
      gdouble width, height;
      gdouble scale;

      if (!location->mloc->loaded)
        continue;

      /* Переводим размеры метки из логической СК в пиксельные. */
      gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
      width = location->width / scale;
      height = location->height / scale;

      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y,
                                             location->center_c2d.x, location->center_c2d.y);

#ifdef DEBUG_TRACK_POINTS
      {
        gdouble track_x, track_y;
        gdouble ext_x, ext_y, ext_width, ext_height;

        const gdouble dashes[] = { 12.0, 2.0 };

        gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &track_x, &track_y,
                                               location->track_c2d.x, location->track_c2d.y);

        cairo_set_source_rgb (cairo, 0.1, 0.7, 0.1);
        cairo_set_line_width (cairo, 1.0);

        cairo_new_sub_path (cairo);
        cairo_arc (cairo, x, y, 4.0, 0, 2 * G_PI);
        cairo_fill (cairo);

        cairo_new_sub_path (cairo);
        cairo_arc (cairo, track_x, track_y, 4.0, 0, 2 * G_PI);
        cairo_fill (cairo);

        cairo_move_to (cairo, x, y);
        cairo_line_to (cairo, track_x, track_y);
        cairo_set_dash (cairo, dashes, G_N_ELEMENTS (dashes), 0.0);
        cairo_stroke (cairo);

        gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &ext_x, &ext_y,
                                               location->extent_from.x, location->extent_from.y);
        ext_width = (location->extent_to.x - location->extent_from.x) / scale;
        ext_height = (location->extent_to.y - location->extent_from.y) / scale;
        cairo_rectangle (cairo, ext_x, ext_y, ext_width, -ext_height);
        cairo_stroke (cairo);

        cairo_set_dash (cairo, NULL, 0, 0.0);
      }
#endif

      cairo_set_line_width (cairo, priv->line_width);
      if (priv->location_hover == location)
        gdk_cairo_set_source_rgba (cairo, &priv->color_hover);
      else
        gdk_cairo_set_source_rgba (cairo, &priv->color_default);

      cairo_save (cairo);
      cairo_translate (cairo, x, y);
      cairo_rotate (cairo, location->angle);

      cairo_rectangle (cairo, -width, -height, 2.0 * width, 2.0 * height);
      cairo_stroke (cairo);

      cairo_restore (cairo);
    }

  if (priv->location_hover)
    {
      const HyScanGtkMapWfmarkLayerLocation *hover = priv->location_hover;
      gint text_width, text_height;
      gdouble text_x, text_y;

      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &text_x, &text_y,
                                             priv->cursor.x, priv->cursor.y);

      pango_layout_set_text (priv->pango_layout, hover->mloc->mark->name, -1);
      pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
      text_height /= PANGO_SCALE;
      text_width /= PANGO_SCALE;

      /* Вариант размещения текста под прямоугольником. */
      // cairo_save (cairo);
      // cairo_translate (cairo, x, y);
      // gdouble bounding_height = fabs (width * sin (hover->angle)) + fabs (height * cos(hover->angle));
      // cairo_move_to (cairo, -text_width / 2.0, bounding_height);

      text_y -= text_height + priv->text_margin + priv->tooltip_margin;
      gdk_cairo_set_source_rgba (cairo, &priv->color_bg);
      cairo_rectangle (cairo, text_x - priv->text_margin, text_y - priv->text_margin,
                       text_width + 2 * priv->text_margin, text_height + 2 * priv->text_margin);
      cairo_fill (cairo);

      cairo_move_to (cairo, text_x, text_y);
      gdk_cairo_set_source_rgba (cairo, &priv->color_text);
      pango_cairo_show_layout (cairo, priv->pango_layout);
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
}

/* Находит метку под курсором мыши. */
static HyScanGtkMapWfmarkLayerLocation *
hyscan_gtk_map_wfmark_layer_find_hover (HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  HyScanGtkMapWfmarkLayerLocation *hover = NULL;
  gdouble min_distance = G_MAXDOUBLE;

  GHashTableIter iter;
  HyScanGtkMapWfmarkLayerLocation *location;

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    {
      HyScanGeoCartesian2D rotated;

      if (!location->mloc->loaded)
        continue;

      hyscan_cartesian_rotate (&priv->cursor, &location->center_c2d, location->angle, &rotated);
      if (hyscan_cartesian_is_point_inside (&rotated, &location->rect_from, &location->rect_to))
        {
          gdouble distance;

          /* Среди всех меток под курсором выбираем ту, чей центр ближе к курсору. */
          distance = hyscan_cartesian_distance (&location->center_c2d, &priv->cursor);
          if (distance < min_distance) {
            min_distance = distance;
            hover = location;
          }
        }
    }

  return hover;
}

/* Обработчки ::motion-notify-event.
 * Определяет метку под курсором мыши. */
static void
hyscan_gtk_map_wfmark_layer_motion_notify (GtkWidget      *widget,
                                           GdkEventMotion *event,
                                           gpointer        user_data)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (user_data);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  HyScanGtkMapWfmarkLayerLocation *hover;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (wfm_layer)))
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &priv->cursor.x, &priv->cursor.y);
  hover = hyscan_gtk_map_wfmark_layer_find_hover (wfm_layer);

  if (hover != NULL || hover != priv->location_hover)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  priv->location_hover = hover;

  g_rw_lock_reader_unlock (&priv->mark_lock);
}

/* Обработчик сигнала HyScanGtkMap::notify::projection.
 * Пересчитывает координаты меток, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_wfmark_layer_proj_notify (HyScanGtkMap *map,
                                         GParamSpec   *pspec,
                                         gpointer      user_data)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (user_data);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapWfmarkLayerLocation *location;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Обновляем координаты меток согласно новой проекции. */
  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_gtk_map_wfmark_layer_project_location (wfm_layer, location);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_wfmark_layer_added (HyScanGtkLayer          *gtk_layer,
                                   HyScanGtkLayerContainer *container)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (gtk_layer);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  g_signal_connect (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_wfmark_layer_motion_notify), gtk_layer);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_wfmark_layer_draw), wfm_layer);
  g_signal_connect_swapped (priv->map, "configure-event", G_CALLBACK (hyscan_gtk_map_wfmark_layer_configure), wfm_layer);
  g_signal_connect (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_wfmark_layer_proj_notify), gtk_layer);
}

static void
hyscan_gtk_map_wfmark_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (gtk_layer);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, wfm_layer);
  g_clear_object (&priv->map);
}

static void
hyscan_gtk_map_wfmark_layer_set_visible (HyScanGtkLayer *layer,
                                         gboolean        visible)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (layer);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_wfmark_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (layer);

  return wfm_layer->priv->visible;
}

/* Загружает настройки слоя из ini-файла. */
static gboolean
hyscan_gtk_map_wfmark_layer_load_key_file (HyScanGtkLayer *layer,
                                           GKeyFile       *key_file,
                                           const gchar    *group)
{
  HyScanGtkMapWfmarkLayer *wfm_layer = HYSCAN_GTK_MAP_WFMARK_LAYER (layer);
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  gdouble value;

  /* Блокируем доступ к меткам, пока не установим новые параметры. */
  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Внешний вид линии. */
  value = g_key_file_get_double (key_file, group, "line-width", NULL);
  priv->line_width = value > 0 ? value : LINE_WIDTH ;
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_default, key_file, group, "mark-color", MARK_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_hover, key_file, group, "mark-color-hover", MARK_COLOR_HOVER);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_bg, key_file, group, "bg-color", BG_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_text, key_file, group, "text-color", TEXT_COLOR);

  value = g_key_file_get_double (key_file, group, "text-margin", NULL);
  priv->text_margin = value > 0 ? value : TEXT_MARGIN;

  value = g_key_file_get_double (key_file, group, "tooltip-margin", NULL);
  priv->text_margin = value > 0 ? value : TOOLTIP_MARGIN;

  g_rw_lock_writer_unlock (&priv->mark_lock);

  /* Перерисовываем. */
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_wfmark_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->removed = hyscan_gtk_map_wfmark_layer_removed;
  iface->added = hyscan_gtk_map_wfmark_layer_added;
  iface->set_visible = hyscan_gtk_map_wfmark_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_wfmark_layer_get_visible;
  iface->load_key_file = hyscan_gtk_map_wfmark_layer_load_key_file;
}

/**
 * hyscan_gtk_map_wfmark_layer_new:
 * @model
 *
 * Returns: создает новый объект #HyScanGtkMapWfmarkLayer. Для удаления g_object_unref().
 */
HyScanGtkLayer *
hyscan_gtk_map_wfmark_layer_new (HyScanMarkLocModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER,
                       "mark-loc-model", model,
                       NULL);
}

/**
 * hyscan_gtk_map_wfmark_layer_mark_view:
 * @wfm_layer
 * @mark_id
 *
 * Перемещает область видимости карты в положение метки @mark_id и выделяет
 * эту метку.
 */
void
hyscan_gtk_map_wfmark_layer_mark_view (HyScanGtkMapWfmarkLayer *wfm_layer,
                                       const gchar             *mark_id)
{
  HyScanGtkMapWfmarkLayerPrivate *priv;
  HyScanGtkMapWfmarkLayerLocation *location;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK_LAYER (wfm_layer));
  priv = wfm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  location = g_hash_table_lookup (priv->marks, mark_id);
  if (location != NULL)
    {
      gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map),
                               location->extent_from.x, location->extent_to.x,
                               location->extent_from.y, location->extent_to.y);
      priv->location_hover = location;
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
}
