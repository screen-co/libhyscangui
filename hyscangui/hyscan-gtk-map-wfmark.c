/* hyscan-gtk-map-wfmark.c
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
 * SECTION: hyscan-gtk-map-wfmark
 * @Short_description: Класс слоя с метками водопада
 * @Title: HyScanGtkMapWfmark
 * @See_also: #HyScanGtkLayer, #HyScanGtkMapTiled, #HyScanGtkMap, #GtkCifroArea
 *
 * Слой #HyScanGtkMapWfmark изображает на карте контуры меток водопада
 * указанного проекта.
 *
 * - hyscan_gtk_map_wfmark_new() - создание нового слоя;
 * - hyscan_gtk_map_wfmark_mark_view() - переход к указанной метке.
 *
 * Стиль слоя может быть настроен с помощью ini-файла:
 * - "mark-color" - цвет контура метки
 * - "hover-mark-color" - цвет контура выделенной метки
 * - "line-width" - толщина линий
 */

#include "hyscan-gtk-map-wfmark.h"
#include "hyscan-gtk-map.h"
#include <hyscan-cartesian.h>
#include <math.h>

/* Оформление по умолчанию. */
#define MARK_COLOR              "#61B243"                     /* Цвет обводки меток. */
#define MARK_COLOR_HOVER        "#9443B2"                     /* Цвет обводки меток при наведении мыши. */
#define LINE_WIDTH              1.0                           /* Толщина линии обводки. */

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
} HyScanGtkMapWfmarkLocation;

struct _HyScanGtkMapWfmarkPrivate
{
  HyScanGtkMap                          *map;             /* Карта. */
  gboolean                               visible;         /* Признак видимости слоя. */

  HyScanMarkLocModel                    *model;           /* Модель данных. */

  GRWLock                                mark_lock;       /* Блокировка данных по меткам. .*/
  GHashTable                            *marks;           /* Хэш-таблица меток #HyScanGtkMapWfmarkLocation. */

  const HyScanGtkMapWfmarkLocation      *location_hover;  /* Метка, над которой находится курсор мыши. */
  gchar                                 *active_mark_id;  /* Выбранная метка. */

  /* Стиль отображения. */
  GdkRGBA                                color_default;   /* Цвет обводки меток. */
  GdkRGBA                                color_hover;     /* Цвет обводки метки при наведении курсора мыши. */
  gdouble                                line_width;      /* Толщина обводки. */
};

static void    hyscan_gtk_map_wfmark_interface_init           (HyScanGtkLayerInterface    *iface);
static void    hyscan_gtk_map_wfmark_set_property             (GObject                    *object,
                                                               guint                       prop_id,
                                                               const GValue               *value,
                                                               GParamSpec                 *pspec);
static void    hyscan_gtk_map_wfmark_object_constructed       (GObject                    *object);
static void    hyscan_gtk_map_wfmark_object_finalize          (GObject                    *object);
static void    hyscan_gtk_map_wfmark_model_changed            (HyScanGtkMapWfmark         *wfm_layer);
static void    hyscan_gtk_map_wfmark_location_free            (HyScanGtkMapWfmarkLocation *location);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapWfmark, hyscan_gtk_map_wfmark, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapWfmark)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_wfmark_interface_init))

static void
hyscan_gtk_map_wfmark_class_init (HyScanGtkMapWfmarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_wfmark_set_property;
  object_class->constructed = hyscan_gtk_map_wfmark_object_constructed;
  object_class->finalize = hyscan_gtk_map_wfmark_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("mark-loc-model", "Mark location model",
                         "The HyScanMarkLocModel containing information about marks and its locations",
                         HYSCAN_TYPE_MARK_LOC_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_gtk_map_wfmark_init (HyScanGtkMapWfmark *gtk_map_wfmark)
{
  gtk_map_wfmark->priv = hyscan_gtk_map_wfmark_get_instance_private (gtk_map_wfmark);
}

static void
hyscan_gtk_map_wfmark_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanGtkMapWfmark *gtk_map_wfmark = HYSCAN_GTK_MAP_WFMARK (object);
  HyScanGtkMapWfmarkPrivate *priv = gtk_map_wfmark->priv;

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
hyscan_gtk_map_wfmark_object_constructed (GObject *object)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (object);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_wfmark_parent_class)->constructed (object);

  g_rw_lock_init (&priv->mark_lock);
  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_wfmark_location_free);

  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_model_changed), wfm_layer);

  /* Стиль оформления. */
  gdk_rgba_parse (&priv->color_default, MARK_COLOR);
  gdk_rgba_parse (&priv->color_hover, MARK_COLOR_HOVER);
  priv->line_width = LINE_WIDTH;
}

static void
hyscan_gtk_map_wfmark_object_finalize (GObject *object)
{
  HyScanGtkMapWfmark *gtk_map_wfmark = HYSCAN_GTK_MAP_WFMARK (object);
  HyScanGtkMapWfmarkPrivate *priv = gtk_map_wfmark->priv;

  g_rw_lock_clear (&priv->mark_lock);

  g_hash_table_unref (priv->marks);
  g_object_unref (priv->model);
  g_free (priv->active_mark_id);

  G_OBJECT_CLASS (hyscan_gtk_map_wfmark_parent_class)->finalize (object);
}

/* Проецирует метку на текущую картографическую проекцию. */
static void
hyscan_gtk_map_wfmark_project_location (HyScanGtkMapWfmark         *wfm_layer,
                                        HyScanGtkMapWfmarkLocation *location)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  gdouble scale;
  gdouble offset;
  HyScanGeoCartesian2D mark_from, mark_to;

  if (!location->mloc->loaded)
    return;

  /* Переводим из метров в единицы картографической проекции. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, location->mloc->center_geo);
  location->width = location->mloc->mark->width / scale;
  location->height = location->mloc->mark->height / scale;
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
  {
    gdouble extent_margin;

    mark_from.x = location->center_c2d.x - location->width;
    mark_from.y = location->center_c2d.y - location->height;
    mark_to.x = location->center_c2d.x + location->width;
    mark_to.y = location->center_c2d.y + location->height;
    hyscan_cartesian_rotate_area (&mark_from, &mark_to, &location->center_c2d, location->angle,
                                  &location->extent_from, &location->extent_to);

    /* Добавляем небольшой отступ. */
    extent_margin = 0.1 * (location->extent_to.x - location->extent_from.x);
    location->extent_to.x   += extent_margin;
    location->extent_from.x -= extent_margin;

    extent_margin = 0.1 * (location->extent_to.y - location->extent_from.y);
    location->extent_to.y   += extent_margin;
    location->extent_from.y -= extent_margin;
  }

}

static HyScanGtkMapWfmarkLocation *
hyscan_gtk_map_wfmark_location_new (void)
{
  return g_slice_new (HyScanGtkMapWfmarkLocation);
}

static void
hyscan_gtk_map_wfmark_location_free (HyScanGtkMapWfmarkLocation *location)
{
  hyscan_mark_location_free (location->mloc);
  g_slice_free (HyScanGtkMapWfmarkLocation, location);
}

static gboolean
hyscan_gtk_map_wfmark_insert_mark (gpointer  key,
                                   gpointer  value,
                                   gpointer  user_data)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (user_data);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  HyScanGtkMapWfmarkLocation *location;

  location = hyscan_gtk_map_wfmark_location_new ();
  location->mloc = value;

  hyscan_gtk_map_wfmark_project_location (wfm_layer, location);
  g_hash_table_insert (priv->marks, key, location);

  return TRUE;
}

/* Обработчик сигнала HyScanMarkLocModel::changed.
 * Обновляет список меток. */
static void
hyscan_gtk_map_wfmark_model_changed (HyScanGtkMapWfmark *wfm_layer)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  GHashTable *marks;

  /* Загружаем гео-данные по меткам. */
  marks = hyscan_mark_loc_model_get (priv->model);

  g_rw_lock_writer_lock (&priv->mark_lock);

  priv->location_hover = NULL;
  g_hash_table_remove_all (priv->marks);
  g_hash_table_foreach_steal (marks, hyscan_gtk_map_wfmark_insert_mark, wfm_layer);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_hash_table_unref (marks);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_wfmark_draw (HyScanGtkMap       *map,
                            cairo_t            *cairo,
                            HyScanGtkMapWfmark *wfm_layer)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapWfmarkLocation *location;
  gchar *mark_id;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (wfm_layer)))
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &location))
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

      if (mark_id != NULL && g_strcmp0 (mark_id, priv->active_mark_id) == 0)
        cairo_set_line_width (cairo, 2.0 * priv->line_width);
      else
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

  g_rw_lock_reader_unlock (&priv->mark_lock);
}

/* Находит метку под курсором мыши. */
static const HyScanGtkMapWfmarkLocation *
hyscan_gtk_map_wfmark_find_hover (HyScanGtkMapWfmark   *wfm_layer,
                                  HyScanGeoCartesian2D *cursor,
                                  gdouble              *distance)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  HyScanGtkMapWfmarkLocation *hover = NULL;
  gdouble min_distance = G_MAXDOUBLE;

  GHashTableIter iter;
  HyScanGtkMapWfmarkLocation *location;

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    {
      HyScanGeoCartesian2D rotated;

      if (!location->mloc->loaded)
        continue;

      hyscan_cartesian_rotate (cursor, &location->center_c2d, location->angle, &rotated);
      if (hyscan_cartesian_is_point_inside (&rotated, &location->rect_from, &location->rect_to))
        {
          gdouble mark_distance;

          /* Среди всех меток под курсором выбираем ту, чей центр ближе к курсору. */
          mark_distance = hyscan_cartesian_distance (&location->center_c2d, cursor);
          if (mark_distance < min_distance) {
            min_distance = mark_distance;
            hover = location;
          }
        }
    }

  *distance = min_distance;

  return hover;
}

/* Обработчик сигнала HyScanGtkMap::notify::projection.
 * Пересчитывает координаты меток, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_wfmark_proj_notify (HyScanGtkMap *map,
                                   GParamSpec   *pspec,
                                   gpointer      user_data)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (user_data);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  GHashTableIter iter;
  HyScanGtkMapWfmarkLocation *location;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Обновляем координаты меток согласно новой проекции. */
  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_gtk_map_wfmark_project_location (wfm_layer, location);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_wfmark_added (HyScanGtkLayer          *gtk_layer,
                             HyScanGtkLayerContainer *container)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (gtk_layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_wfmark_draw), wfm_layer);
  g_signal_connect (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_wfmark_proj_notify), wfm_layer);
}

static void
hyscan_gtk_map_wfmark_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (gtk_layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, wfm_layer);
  g_clear_object (&priv->map);
}

static void
hyscan_gtk_map_wfmark_set_visible (HyScanGtkLayer *layer,
                                   gboolean        visible)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_wfmark_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);

  return wfm_layer->priv->visible;
}

/* Загружает настройки слоя из ini-файла. */
static gboolean
hyscan_gtk_map_wfmark_load_key_file (HyScanGtkLayer *layer,
                                     GKeyFile       *key_file,
                                     const gchar    *group)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  gdouble value;

  /* Блокируем доступ к меткам, пока не установим новые параметры. */
  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Внешний вид линии. */
  value = g_key_file_get_double (key_file, group, "line-width", NULL);
  priv->line_width = value > 0 ? value : LINE_WIDTH ;
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_default, key_file, group, "color",       MARK_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_hover,   key_file, group, "hover-color", MARK_COLOR_HOVER);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  /* Перерисовываем. */
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_wfmark_hint_shown (HyScanGtkLayer *layer,
                                  gboolean        shown)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  if (!shown)
    priv->location_hover = NULL;
}

/* Ищет, есть ли на слое метка в точке (x, y) */
static gchar *
hyscan_gtk_map_wfmark_hint_find (HyScanGtkLayer *layer,
                                 gdouble         x,
                                 gdouble         y,
                                 gdouble        *distance)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  HyScanGeoCartesian2D cursor;
  gchar *hint = NULL;

  g_rw_lock_reader_lock (&priv->mark_lock);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), x, y, &cursor.x, &cursor.y);
  priv->location_hover = hyscan_gtk_map_wfmark_find_hover (wfm_layer, &cursor, distance);
  if (priv->location_hover != NULL)
    hint = g_strdup (priv->location_hover->mloc->mark->name);

  g_rw_lock_reader_unlock (&priv->mark_lock);

  return hint;
}

static void
hyscan_gtk_map_wfmark_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->removed = hyscan_gtk_map_wfmark_removed;
  iface->added = hyscan_gtk_map_wfmark_added;
  iface->set_visible = hyscan_gtk_map_wfmark_set_visible;
  iface->get_visible = hyscan_gtk_map_wfmark_get_visible;
  iface->load_key_file = hyscan_gtk_map_wfmark_load_key_file;
  iface->hint_find = hyscan_gtk_map_wfmark_hint_find;
  iface->hint_shown = hyscan_gtk_map_wfmark_hint_shown;
}

/**
 * hyscan_gtk_map_wfmark_new:
 * @model: указатель на модель данных положения меток
 *
 * Returns: создает новый объект #HyScanGtkMapWfmark. Для удаления g_object_unref().
 */
HyScanGtkLayer *
hyscan_gtk_map_wfmark_new (HyScanMarkLocModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_WFMARK,
                       "mark-loc-model", model,
                       NULL);
}

void
hyscan_gtk_map_wfmark_mark_highlight (HyScanGtkMapWfmark *wfm_layer,
                                      const gchar        *mark_id)
{
  HyScanGtkMapWfmarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK (wfm_layer));
  priv = wfm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_writer_lock (&priv->mark_lock);
  g_free (priv->active_mark_id);
  priv->active_mark_id = g_strdup (mark_id);
  g_rw_lock_writer_unlock (&priv->mark_lock);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_wfmark_mark_view:
 * @wfm_layer
 * @mark_id
 * @keep_scale
 *
 * Перемещает область видимости карты в положение метки @mark_id и выделяет
 * эту метку.
 */
void
hyscan_gtk_map_wfmark_mark_view (HyScanGtkMapWfmark *wfm_layer,
                                 const gchar        *mark_id,
                                 gboolean            zoom_in)
{
  HyScanGtkMapWfmarkPrivate *priv;
  HyScanGtkMapWfmarkLocation *location;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK (wfm_layer));
  priv = wfm_layer->priv;

  if (priv->map == NULL)
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  location = g_hash_table_lookup (priv->marks, mark_id);
  if (location != NULL)
    {
      gdouble prev_scale;

      gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &prev_scale, NULL);
      gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map),
                               location->extent_from.x, location->extent_to.x,
                               location->extent_from.y, location->extent_to.y);

      /* Возвращаем прежний масштаб. */
      if (!zoom_in)
        {
          gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), prev_scale, prev_scale,
                                    location->center_c2d.x, location->center_c2d.y);
        }

    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
}
