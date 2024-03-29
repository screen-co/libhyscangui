/* hyscan-gtk-map-wfmark.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 * Copyright 2019 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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
 * Слой #HyScanGtkMapWfmark изображает на карте метки "водопада" с аккустическими изображениями
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
#include "hyscan-gtk-layer-param.h"
#include <hyscan-cartesian.h>
#include <math.h>
#include <string.h>
#include <hyscan-factory-amplitude.h>
#include <hyscan-factory-depth.h>
#include <hyscan-tile-queue.h>
#include <hyscan-tile-color.h>
#include <hyscan-types.h>
#include <glib/gi18n-lib.h>

/* Раскомментируйте строку ниже для отладки положения меток относительно галса. */
/* #define DEBUG_TRACK_POINTS */
/* Раскомментируйте строку ниже для отладки отображения аккустических изображений меток. */
/* #define DEBUG_GRAPHIC_MARK */
#ifdef DEBUG_GRAPHIC_MARK
  #define VISIBLE_AREA_PADDING 100
#else
  #define VISIBLE_AREA_PADDING 0
#endif

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_CACHE,
  PROP_DB,
  PROP_UNITS,
};
/* Режим отображения меток. */
enum
{
  SHOW_ACOUSTIC_IMAGE, /*Отображать акустическое изображение метки.*/
  SHOW_ONLY_BORDER     /*Отображать только границу метки.*/
};

typedef struct
{
  HyScanMarkLocation          *mloc;            /* Указатель на оригинальную метку (принадлежит priv->marks). */

  /* Поля ниже определяются переводом географических координат метки в СК карты с учетом текущей проекции. */
  HyScanGeoCartesian2D         antenna_c2d;     /* Координаты антенны в СК карт. */
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

typedef struct
{
  const HyScanGtkMapWfmarkLocation    *location;           /* Информация о метке. */
  HyScanGeoCartesian2D                 area_rect_from;     /* Границы региона для рисования, от. */
  HyScanGeoCartesian2D                 area_rect_to;       /* Границы региона для рисования, до. */
  gdouble                              scale;              /* Масштаб GtkCifroArea. */
  gdouble                              scale_px;           /* Масштаб изображения карты в пикселах на метр. */
  gfloat                               ppi;                /* PPI дисплея. */
} HyScanGtkMapWfmarkDrawMark;

struct _HyScanGtkMapWfmarkPrivate
{
  HyScanGtkMap                          *map;             /* Карта. */
  gboolean                               visible;         /* Признак видимости слоя. */

  HyScanMarkLocModel                    *model;           /* Модель данных. */
  HyScanDB                              *db;              /* Модель данных. */
  HyScanCache                           *cache;           /* Модель данных. */
  HyScanUnits                           *units;           /* Единицы измерения. */
  gchar                                 *project;         /* Название проекта. */

  HyScanFactoryAmplitude                *factory_amp;     /* Фабрика объектов акустических данных. */
  HyScanFactoryDepth                    *factory_dpt;     /* Фабрика объектов глубины. */
  HyScanTileQueue                       *tile_queue;      /* Очередь для работы с аккустическими изображениями. */

  GHashTable                            *marks;           /* Хэш-таблица меток #HyScanGtkMapWfmarkLocation. */

  const HyScanGtkMapWfmarkLocation      *hover_location;  /* Метка, над которой находится курсор мыши. */
  const HyScanGtkMapWfmarkLocation      *hover_candidate; /* Метка, над которой находится курсор мыши. */
  gchar                                 *active_mark_id;  /* Выбранная метка. */

  /* Стиль отображения. */
  HyScanGtkLayerParam                   *param;           /* Параметры отображения. */
  GdkRGBA                                color_default,   /* Цвет обводки меток. */
                                         color_hover,     /* Цвет обводки метки при наведении курсора мыши. */
                                         color_bg,        /* Фоновый цвет текста. */
                                         color_ai_border, /* Цвет рамки вокруг акустического изображения метки. */
                                         color_echo_mark, /* Цвет эхолотной метки и стрелки указывающей направление. */
                                         color_lb_arrow,  /* Цвет стрелки для метки левого борта. */
                                         color_rb_arrow;  /* Цвет стрелки для метки правого борта. */
  gdouble                                line_width;      /* Толщина обводки. */
  PangoLayout                           *pango_layout;    /* Шрифт. */

  gint                                   show_mode;       /* Режим отображения меток. */
};

static void     hyscan_gtk_map_wfmark_interface_init           (HyScanGtkLayerInterface    *iface);
static void     hyscan_gtk_map_wfmark_set_property             (GObject                    *object,
                                                                guint                       prop_id,
                                                                const GValue               *value,
                                                                GParamSpec                 *pspec);
static void     hyscan_gtk_map_wfmark_object_constructed       (GObject                    *object);
static void     hyscan_gtk_map_wfmark_object_finalize          (GObject                    *object);
static void     hyscan_gtk_map_wfmark_model_changed            (HyScanGtkMapWfmark         *wfm_layer);
static void     hyscan_gtk_map_wfmark_location_free            (HyScanGtkMapWfmarkLocation *location);
static gboolean hyscan_gtk_map_wfmark_redraw                   (gpointer                    data);
static void     hyscan_gtk_map_wfmark_tile_loaded              (HyScanGtkMapWfmark         *wfm_layer,
                                                                HyScanTile                 *tile,
                                                                gfloat                     *img,
                                                                gint                        size,
                                                                gulong                      hash);


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
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The link to main cache with frequency used stafs",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Data base",
                         "The link to data base",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_UNITS,
    g_param_spec_object ("units", "Measurement units",
                         "Measurement units",
                         HYSCAN_TYPE_UNITS,
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

    case PROP_DB:
      priv->db  = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache  = g_value_dup_object (value);
      break;

    case PROP_UNITS:
      priv->units  = g_value_dup_object (value);
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

  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_gtk_map_wfmark_location_free);

  g_signal_connect_swapped (priv->model, "changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_model_changed), wfm_layer);

  /* Стиль оформления. */
  priv->param = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-wfmark");
  hyscan_gtk_layer_param_add_rgba (priv->param, "/color", &priv->color_default);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/hover-color", &priv->color_hover);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/ai-border-color", &priv->color_ai_border);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/echo-mark-color", &priv->color_echo_mark);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/port-color", &priv->color_lb_arrow);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/starboard-color", &priv->color_rb_arrow);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/bg-color", &priv->color_bg);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (priv->param), "/line-width", &priv->line_width);
  hyscan_gtk_layer_param_set_default (priv->param);

  /* Создаём фабрику объектов доступа к данным амплитуд. */
  priv->factory_amp = hyscan_factory_amplitude_new (priv->cache);

  /* Создаём фабрику объектов доступа к данным глубины. */
  priv->factory_dpt = hyscan_factory_depth_new (priv->cache);

  /* Создаём очередь для генерации тайлов. */
  priv->tile_queue = hyscan_tile_queue_new (g_get_num_processors (),
                                            priv->cache,
                                            priv->factory_amp,
                                            priv->factory_dpt);
  /* Отображаем акустические изображения меток.*/
  priv->show_mode = SHOW_ACOUSTIC_IMAGE;

  /* Соединяем сигнал готовности тайла с функцией-обработчиком. */
  g_signal_connect_swapped (priv->tile_queue, "tile-queue-image",
                            G_CALLBACK (hyscan_gtk_map_wfmark_tile_loaded), wfm_layer);
}

static void
hyscan_gtk_map_wfmark_object_finalize (GObject *object)
{
  HyScanGtkMapWfmark *gtk_map_wfmark = HYSCAN_GTK_MAP_WFMARK (object);
  HyScanGtkMapWfmarkPrivate *priv = gtk_map_wfmark->priv;

  /*  Отключаемся от сигнала готовности тайла. */
  g_signal_handlers_disconnect_by_data (priv->tile_queue, gtk_map_wfmark);

  g_hash_table_unref (priv->marks);
  g_object_unref (priv->param);
  g_object_unref (priv->pango_layout);
  g_object_unref (priv->model);
  g_object_unref (priv->db);
  g_object_unref (priv->cache);

  g_object_unref (priv->tile_queue);
  g_object_unref (priv->factory_dpt);
  g_object_unref (priv->factory_amp);

  g_free (priv->project);
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
  scale = hyscan_gtk_map_get_scale_value (priv->map, location->mloc->center_geo);
  if (location->mloc->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    location->width = 0.0;
  else
    location->width = location->mloc->mark->width / scale;

  location->height = location->mloc->mark->height / scale;
  offset = location->mloc->offset / scale;

  /* Определяем координаты центра метки в СК проекции. */
  hyscan_gtk_map_geo_to_value (priv->map, location->mloc->center_geo, &location->antenna_c2d);

  location->angle = location->mloc->antenna_course / 180.0 * G_PI;
  location->center_c2d.x = location->antenna_c2d.x - offset * cos (location->angle);
  location->center_c2d.y = location->antenna_c2d.y + offset * sin (location->angle);

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
hyscan_gtk_map_wfmark_insert_mark (gpointer key,
                                   gpointer value,
                                   gpointer user_data)
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

  priv->hover_location = NULL;
  g_hash_table_remove_all (priv->marks);
  g_hash_table_foreach_steal (marks, hyscan_gtk_map_wfmark_insert_mark, wfm_layer);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_hash_table_unref (marks);
}

/* Функция рисует отдельную метку с параметрами mark. Если в результате тайл
 * с изображением метки был отправлен на генерацию, то будет поднят флаг queue_added. */
static void
hyscan_gtk_map_wfmark_draw_mark (HyScanGtkMapWfmark         *wfm_layer,
                                 cairo_t                    *cairo,
                                 const gchar                *mark_id,
                                 HyScanGtkMapWfmarkDrawMark *mark,
                                 gboolean                   *queue_added)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  gboolean selected;

  const HyScanGtkMapWfmarkLocation *location = mark->location;
  gdouble scale = mark->scale, scale_px = mark->scale_px, ppi = mark->ppi;
  HyScanGeoCartesian2D area_rect_from = mark->area_rect_from, area_rect_to = mark->area_rect_to;

  HyScanTile *tile = NULL;
  HyScanTileCacheable tile_cacheable;
  cairo_surface_t *surface = NULL;
  gdouble current_sin, current_cos,
          width, height,
          new_width, new_height,
          tmp;
  GdkRGBA *color = NULL;
  gfloat *image = NULL;
  guint32 size = 0;

  HyScanGeoCartesian2D position, new_position, offset,
                       m0, m2, b0, b2,
#ifdef DEBUG_GRAPHIC_MARK
                       m1, m3, b1, b3,
#endif
                       border_from, border_to;
  HyScanTileSurface tile_surface;

  current_sin = sin (location->angle);
  current_cos = cos (location->angle);

  width = (location->mloc->direction == HYSCAN_MARK_LOCATION_BOTTOM)? 5.0 : location->width / scale;
  height = location->height / scale;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map),
                                         &position.x, &position.y,
                                         location->center_c2d.x, location->center_c2d.y);

  new_width  = ( (area_rect_to.x - area_rect_from.x) / 2.0) * fabs (current_cos) +
               ( (area_rect_to.y - area_rect_from.y) / 2.0) * fabs (current_sin);
  new_height = ( (area_rect_to.x - area_rect_from.x) / 2.0) * fabs (current_sin) +
               ( (area_rect_to.y - area_rect_from.y) / 2.0) * fabs (current_cos);

  b0.x = -new_width;
  b0.y = -new_height;
  b2.x =  new_width;
  b2.y =  new_height;

  tmp = b0.x;
  b0.x = current_cos * tmp - current_sin * b0.y;
  b0.y = current_sin * tmp + current_cos * b0.y;

  tmp = b2.x;
  b2.x = current_cos * tmp - current_sin * b2.y;
  b2.y = current_sin * tmp + current_cos * b2.y;

  tmp = (area_rect_to.x + area_rect_from.x) / 2.0;
  b0.x += tmp;
  b2.x += tmp;

  tmp = (area_rect_to.y + area_rect_from.y) / 2.0;
  b0.y += tmp;
  b2.y += tmp;

  m0.x = -width;
  m0.y = -height;
  m2.x =  width;
  m2.y =  height;

  b0.x -= position.x;
  b0.y -= position.y;
  b2.x -= position.x;
  b2.y -= position.y;

  tmp = b0.x;
  b0.x = current_cos * tmp + current_sin * b0.y;
  b0.y = current_cos * b0.y - current_sin * tmp;

  tmp = b2.x;
  b2.x = current_cos * tmp + current_sin * b2.y;
  b2.y = current_cos * b2.y - current_sin * tmp;

#ifdef DEBUG_GRAPHIC_MARK
  {
    b1.x = -new_width;
    b1.y =  new_height;
    b3.x =  new_width;
    b3.y = -new_height;

    tmp = b1.x;
    b1.x = current_cos * tmp - current_sin * b1.y;
    b1.y = current_sin * tmp + current_cos * b1.y;

    tmp = b3.x;
    b3.x = current_cos * tmp - current_sin * b3.y;
    b3.y = current_sin * tmp + current_cos * b3.y;

    tmp = (area_rect_to.x + area_rect_from.x) / 2.0;
    b1.x += tmp;
    b3.x += tmp;

    tmp = (area_rect_to.y + area_rect_from.y) / 2.0;
    b1.y += tmp;
    b3.y += tmp;

    m1.x = -width;
    m1.y =  height;
    m3.x =  width;
    m3.y = -height;

    b1.x -= position.x;
    b1.y -= position.y;
    b3.x -= position.x;
    b3.y -= position.y;

    tmp = b1.x;
    b1.x = current_cos * tmp + current_sin * b1.y;
    b1.y = current_cos * b1.y - current_sin * tmp;

    tmp = b3.x;
    b3.x = current_cos * tmp + current_sin * b3.y;
    b3.y = current_cos * b3.y - current_sin * tmp;
  }
#endif

  border_from.x = fmax (b0.x, m0.x);
  border_from.y = fmax (b0.y, m0.y);
  border_to.x   = fmin (b2.x, m2.x);
  border_to.y   = fmin (b2.y, m2.y);

  width  = (border_to.x - border_from.x) / 2.0;
  if (width < 0.0)
    return;
  height = (border_to.y - border_from.y) / 2.0;
  if (height < 0.0)
    return;

  offset.x = (border_to.x + border_from.x) / 2.0;
  offset.y = (border_to.y + border_from.y) / 2.0;

  new_position.x = position.x + current_cos * offset.x - current_sin * offset.y;
  new_position.y = position.y + current_sin * offset.x + current_cos * offset.y;

  offset.x /= scale_px;
  offset.y /= scale_px;

#ifdef DEBUG_GRAPHIC_MARK
  {
    m0.x += new_position.x;
    m0.y += new_position.y;
    m1.x += new_position.x;
    m1.y += new_position.y;
    m2.x += new_position.x;
    m2.y += new_position.y;
    m3.x += new_position.x;
    m3.y += new_position.y;

    b0.x += new_position.x;
    b0.y += new_position.y;
    b1.x += new_position.x;
    b1.y += new_position.y;
    b2.x += new_position.x;
    b2.y += new_position.y;
    b3.x += new_position.x;
    b3.y += new_position.y;
  }
#endif

  new_width  = width * fabs (current_cos) + height * fabs (current_sin);
  new_height = width * fabs (current_sin) + height * fabs (current_cos);

  border_from.x = new_position.x - new_width;
  border_from.y = new_position.y - new_height;

  border_to.x = new_position.x + new_width;
  border_to.y = new_position.y + new_height;

  if (border_from.x > area_rect_to.x ||
      border_from.y > area_rect_to.y ||
      border_to.x < area_rect_from.x ||
      border_to.y < area_rect_from.y)
    return;

  if (location->mloc->direction != HYSCAN_MARK_LOCATION_BOTTOM &&
      priv->show_mode == SHOW_ACOUSTIC_IMAGE)
   {
      gdouble current_tile_width  = width  / scale_px,
              current_tile_height = height / scale_px;

#ifdef DEBUG_GRAPHIC_MARK

      HyScanGeoCartesian2D p0 = {-width, -height},
                           p1 = {-width,  height},
                           p2 = { width,  height},
                           p3 = { width, -height};

      tmp = p0.x;
      p0.x = current_cos * tmp - current_sin * p0.y;
      p0.y = current_sin * tmp + current_cos * p0.y;

      tmp = p1.x;
      p1.x = current_cos * tmp - current_sin * p1.y;
      p1.y = current_sin * tmp + current_cos * p1.y;

      tmp = p2.x;
      p2.x = current_cos * tmp - current_sin * p2.y;
      p2.y = current_sin * tmp + current_cos * p2.y;

      tmp = p3.x;
      p3.x = current_cos * tmp - current_sin * p3.y;
      p3.y = current_sin * tmp + current_cos * p3.y;

      p0.x += new_position.x;
      p0.y += new_position.y;
      p1.x += new_position.x;
      p1.y += new_position.y;
      p2.x += new_position.x;
      p2.y += new_position.y;
      p3.x += new_position.x;
      p3.y += new_position.y;

#endif

      tile = hyscan_tile_new (location->mloc->track_name);

      tile->info.source = hyscan_source_get_type_by_id (location->mloc->mark->source);

      if (tile->info.source != HYSCAN_SOURCE_INVALID)
        {
          gdouble along, across;

          along = location->mloc->along;
          across = location->mloc->across;

          along -= offset.y;
          /* Для левого борта тайл надо отразить по оси X. */
          if (location->mloc->direction == HYSCAN_MARK_LOCATION_PORT)
            {
              across -= offset.x;
              /* Поэтому значения отрицательные, и start и end меняются местами. */
              tile->info.across_start = -round ( (across + current_tile_width) * 1000.0);
              tile->info.across_end   = -round ( (across - current_tile_width) * 1000.0);
            }
          else
            {
              across += offset.x;
              tile->info.across_start = round ( (across - current_tile_width) * 1000.0);
              tile->info.across_end   = round ( (across + current_tile_width) * 1000.0);
            }

          tile->info.along_start = round ( (along - current_tile_height) * 1000.0);
          tile->info.along_end   = round ( (along + current_tile_height) * 1000.0);

          tile->info.scale    = 1.0f;
          tile->info.ppi      = ppi;
          tile->info.upsample = 2;
          tile->info.rotate   = FALSE;
          tile->info.flags    = location->mloc->has_course ? HYSCAN_TILE_PROFILER : HYSCAN_TILE_GROUND;

          tile_cacheable.w =      /* Будет заполнено генератором. */
          tile_cacheable.h = 0;   /* Будет заполнено генератором. */
          tile_cacheable.finalized = FALSE;   /* Будет заполнено генератором. */

          if (hyscan_tile_queue_check (priv->tile_queue, tile, &tile_cacheable, NULL))
            {
              if (hyscan_tile_queue_get (priv->tile_queue, tile, &tile_cacheable, &image, &size))
                {
                  /* Тайл найден в кэше. */

                  HyScanTileColor *tile_color;

                  tile_color = hyscan_tile_color_new (priv->cache);
                  /* 1.0 / 2.2 = 0.454545... */
                  hyscan_tile_color_set_levels (tile_color, tile->info.source, 0.0, 0.454545, 1.0);

                  tile_surface.width  = tile_cacheable.w;
                  tile_surface.height = tile_cacheable.h;
                  tile_surface.stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, tile_surface.width);
                  tile_surface.data   = g_malloc0 (tile_surface.height * tile_surface.stride);

                  hyscan_tile_color_add (tile_color, tile, image, size, &tile_surface);

                  if (hyscan_tile_color_check (tile_color, tile, &tile_cacheable))
                    {
                      hyscan_tile_color_get (tile_color, tile, &tile_cacheable, &tile_surface);
                      surface = cairo_image_surface_create_for_data ( (guchar*)tile_surface.data,
                                            CAIRO_FORMAT_ARGB32, tile_surface.width,
                                            tile_surface.height, tile_surface.stride);
                    }

                  hyscan_tile_color_close (tile_color);
                  g_object_unref (tile_color);
               }
            }
          else
            {
              HyScanCancellable *cancellable;
              cancellable = hyscan_cancellable_new ();
              /* Добавляем тайл в очередь на генерацию. */
              hyscan_tile_queue_add (priv->tile_queue, tile, cancellable);
              g_object_unref (cancellable);
              *queue_added = TRUE;
            }
        }
      g_object_unref (tile);
    }
#ifdef DEBUG_TRACK_POINTS
  {
    gdouble track_x, track_y;
    gdouble ext_x, ext_y, ext_width, ext_height;

    const gdouble dashes[] = { 12.0, 2.0 };

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &track_x, &track_y,
                                           location->antenna_c2d.x, location->antenna_c2d.y);

    cairo_set_source_rgb (cairo, 0.1, 0.7, 0.1);
    cairo_set_line_width (cairo, 1.0);

    cairo_new_sub_path (cairo);
    cairo_arc (cairo, position.x, position.y, 4.0, 0, 2 * G_PI);
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

  selected = (mark_id != NULL && g_strcmp0 (mark_id, priv->active_mark_id) == 0);
  color = priv->hover_location == location ? &priv->color_hover : &priv->color_default;

  cairo_save (cairo);
  cairo_translate (cairo, new_position.x, new_position.y);

  if (surface != NULL)
    {
      /* Аккустическое изображение метки. */
      cairo_save (cairo);
      cairo_rotate (cairo, location->angle);
      cairo_set_source_surface (cairo, surface, -width, -height);
      cairo_paint (cairo);
      cairo_surface_destroy (surface);
      g_free (tile_surface.data);

      cairo_set_line_width (cairo, selected ? 2.0 * priv->line_width : priv->line_width);

      gdk_cairo_set_source_rgba (cairo, &priv->color_ai_border);

      cairo_move_to (cairo, -width, -height);
      cairo_line_to (cairo, -width,  height);
      cairo_line_to (cairo,  width,  height);
      cairo_line_to (cairo,  width, -height);
      cairo_close_path (cairo);

      cairo_stroke (cairo);

      cairo_restore (cairo);

#ifdef DEBUG_GRAPHIC_MARK
      {
        HyScanGeoCartesian2D p0 = {-width, -height},
                             p1 = {-width,  height},
                             p2 = { width,  height},
                             p3 = { width, -height};

        cairo_save (cairo);
        cairo_translate (cairo, -new_position.x, -new_position.y);

        cairo_set_line_width (cairo, 3);

        cairo_set_source_rgb (cairo, 0.0, 0.0, 1.0);

        cairo_rectangle (cairo,
                         border_from.x,
                         border_from.y,
                         border_to.x - border_from.x,
                         border_to.y - border_from.y);

        cairo_stroke (cairo);

        cairo_set_source_rgb (cairo, 1.0, 1.0, 0.0);

        cairo_move_to (cairo, p0.x, p0.y);
        cairo_line_to (cairo, p1.x, p1.y);
        cairo_line_to (cairo, p2.x, p2.y);
        cairo_line_to (cairo, p3.x, p3.y);
        cairo_close_path (cairo);

        cairo_stroke (cairo);

        cairo_set_source_rgb (cairo, 0.0, 0.0, 1.0);
        cairo_arc (cairo, position.x, position.y, 5.0, 0.0, 2.0 * G_PI);
        cairo_fill (cairo);

        cairo_set_source_rgb (cairo, 1.0, 1.0, 0.0);
        cairo_arc (cairo, new_position.x, new_position.y, 5.0, 0.0, 2.0 * G_PI);
        cairo_fill (cairo);

        cairo_translate (cairo, new_position.x, new_position.y);
        cairo_restore (cairo);
      }
#endif

    }
  else if (location->mloc->direction == HYSCAN_MARK_LOCATION_BOTTOM)
    {
      cairo_save (cairo);
      cairo_rotate (cairo, location->angle);

      cairo_set_line_width (cairo, 5);

      gdk_cairo_set_source_rgba (cairo, &priv->color_echo_mark);

      cairo_move_to (cairo, 0.0, -height);
      cairo_line_to (cairo, 0.0,  height);

      cairo_stroke (cairo);

      cairo_restore (cairo);
    }
  else if (priv->show_mode == SHOW_ONLY_BORDER)
    {
      cairo_save (cairo);
      cairo_rotate (cairo, location->angle);

      cairo_set_line_width (cairo, selected ? 2.0 * priv->line_width : priv->line_width);

      gdk_cairo_set_source_rgba (cairo, &priv->color_default);

      cairo_rectangle (cairo, -width, -height, 2.0 * width, 2.0 * height);

      cairo_stroke (cairo);

      cairo_restore (cairo);
    }

  /* Стрелка направления движения судна с цветом борта. Рисуется только для меток под курсором мыши. */
  if (priv->hover_location == location)
    {
      cairo_save (cairo);
      cairo_rotate (cairo, priv->hover_location->angle);
      cairo_set_line_width (cairo, 5);

      switch (location->mloc->direction)
      {
        /* Левый борт. */
        case HYSCAN_MARK_LOCATION_PORT:
          {
            gdk_cairo_set_source_rgba (cairo, &priv->color_lb_arrow);

            cairo_move_to (cairo, width, -height);
            cairo_line_to (cairo, width,  height);

            cairo_stroke (cairo);

            cairo_move_to (cairo, width + 10, -height     );
            cairo_line_to (cairo, width - 10, -height     );
            cairo_line_to (cairo, width,      -height - 20);
          }
          break;

        /* Правый борт. */
        case HYSCAN_MARK_LOCATION_STARBOARD:
          {
            gdk_cairo_set_source_rgba (cairo, &priv->color_rb_arrow);

            cairo_move_to (cairo, -width, -height);
            cairo_line_to (cairo, -width,  height);

            cairo_stroke (cairo);

            cairo_move_to (cairo, -width + 10, -height     );
            cairo_line_to (cairo, -width - 10, -height     );
            cairo_line_to (cairo, -width,      -height - 20);
          }
          break;

        /* Под судном. */
        case HYSCAN_MARK_LOCATION_BOTTOM:
          {
            gdk_cairo_set_source_rgba (cairo, &priv->color_echo_mark);

            cairo_move_to (cairo, 0.0, -height);
            cairo_line_to (cairo, 0.0,  height);

            cairo_stroke (cairo);

            cairo_move_to (cairo,  10.0, -height     );
            cairo_line_to (cairo, -10.0, -height     );
            cairo_line_to (cairo,   0.0, -height - 20);
          }
          break;

        default:
          break;
      }

      cairo_close_path (cairo);
      cairo_fill (cairo);
      cairo_restore (cairo);
    }

  if (location->mloc->mark->name != NULL)
    {
      /* Название метки. */
      gint text_width, text_height;

      pango_layout_set_text (priv->pango_layout, location->mloc->mark->name, -1);
      pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
      text_width /= PANGO_SCALE;
      text_height /= PANGO_SCALE;

      cairo_rectangle (cairo, -text_width / 2.0, new_height + text_height / 2.0, text_width, text_height);
      gdk_cairo_set_source_rgba (cairo, &priv->color_bg);
      cairo_fill (cairo);

      cairo_move_to (cairo, -text_width / 2.0, new_height + text_height / 2.0);
      gdk_cairo_set_source_rgba (cairo, color);
      pango_cairo_show_layout (cairo, priv->pango_layout);
    }

  cairo_restore (cairo);
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
  static guint id = 0;
  gboolean queue_added = FALSE;
  guint area_width, area_height;
  HyScanGtkMapWfmarkDrawMark draw_mark;
  const gchar *hover_id = NULL;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (wfm_layer)))
    return;

  /* Переводим размеры метки из логической СК в пиксельные. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &draw_mark.scale, NULL);

  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->map), &area_width, &area_height);

  draw_mark.area_rect_from.x = VISIBLE_AREA_PADDING;
  draw_mark.area_rect_from.y = VISIBLE_AREA_PADDING;
  draw_mark.area_rect_to.x = area_width - VISIBLE_AREA_PADDING;
  draw_mark.area_rect_to.y = area_height - VISIBLE_AREA_PADDING;

  draw_mark.scale_px = hyscan_gtk_map_get_scale_px (priv->map);

  draw_mark.ppi = 1e-3 * HYSCAN_GTK_MAP_MM_PER_INCH * draw_mark.scale_px;

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &mark_id, (gpointer *) &location))
    {
      gboolean hovered = (priv->hover_location == location) ? TRUE : FALSE;

      if (hovered)
        hover_id = mark_id;

      if (!location->mloc->loaded || hovered)
        continue;

      draw_mark.location = location;
      hyscan_gtk_map_wfmark_draw_mark (wfm_layer, cairo, mark_id, &draw_mark, &queue_added);
    }

#ifdef DEBUG_GRAPHIC_MARK
    {
      cairo_set_line_width (cairo, 2);
      cairo_set_source_rgb (cairo, 1.0, 0.0, 0.0);
      cairo_rectangle (cairo,
                       draw_mark.area_rect_from.x,
                       draw_mark.area_rect_from.y,
                       draw_mark.area_rect_to.x - draw_mark.area_rect_from.x,
                       draw_mark.area_rect_to.y - draw_mark.area_rect_from.y);
      cairo_stroke (cairo);
    }
#endif

  if (priv->hover_location != NULL)
    {
      draw_mark.location = priv->hover_location;
      hyscan_gtk_map_wfmark_draw_mark (wfm_layer, cairo, hover_id, &draw_mark, &queue_added);
    }

  if (queue_added)
    hyscan_tile_queue_add_finished (priv->tile_queue, id++);
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
      gboolean is_inside;           /* Признак того, что курсор внутри метки. */

      if (!location->mloc->loaded)
        continue;

      hyscan_cartesian_rotate (cursor, &location->center_c2d, location->angle, &rotated);
      if (location->rect_from.x != location->rect_to.x)
        /* В случае, если метка имеет ненулевую ширину, проверяем,
         * что курсор мыши попал во внутреннюю область метки .*/
        {
          is_inside = hyscan_cartesian_is_point_inside (&rotated, &location->rect_from, &location->rect_to);
        }
      else
        /* В случае, если метка с нулевой шириной, проверяем,
         * что курсор мыши на расстоянии 5px от линии метки. */
        {
          gdouble scale;

          gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
          is_inside = location->rect_from.y < rotated.y && rotated.y < location->rect_to.y &&
                      ABS (location->rect_from.x - rotated.x) < 5.0 * scale;
        }

      if (is_inside)
        {
          gdouble mark_distance;

          /* Среди всех меток под курсором выбираем ту, чей центр ближе к курсору. */
          mark_distance = hyscan_cartesian_distance (&location->center_c2d, cursor);
          if (mark_distance < min_distance)
            {
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

  /* Обновляем координаты меток согласно новой проекции. */
  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_gtk_map_wfmark_project_location (wfm_layer, location);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_wfmark_configure (HyScanGtkMapWfmark *wfmark,
                                 GdkEvent           *event)
{
  HyScanGtkMapWfmarkPrivate *priv = wfmark->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);

  return GDK_EVENT_PROPAGATE;
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
  g_signal_connect_swapped (priv->map, "configure-event", G_CALLBACK (hyscan_gtk_map_wfmark_configure), wfm_layer);
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

/* Получает настройки оформления слоя. */
static HyScanParam *
hyscan_gtk_map_wfmark_get_param (HyScanGtkLayer *layer)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  return g_object_ref (HYSCAN_PARAM (priv->param));
}

static void
hyscan_gtk_map_wfmark_hint_shown (HyScanGtkLayer *layer,
                                  gboolean        shown)
{
  HyScanGtkMapWfmark *wfm_layer = HYSCAN_GTK_MAP_WFMARK (layer);
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;

  if (!shown)
    priv->hover_candidate = NULL;

  if (priv->hover_location == priv->hover_candidate)
    return;

  priv->hover_location = priv->hover_candidate;
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
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
  HyScanMarkLocation *mloc;
  HyScanGeoCartesian2D cursor;
  gchar *text;
  gchar *lat_text, *lon_text;
  gdouble depth;
  GDateTime *date_time;
  GString *hint_string;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), x, y, &cursor.x, &cursor.y);

  priv->hover_candidate = hyscan_gtk_map_wfmark_find_hover (wfm_layer, &cursor, distance);

  if (priv->hover_candidate == NULL)
    return NULL;

  mloc = priv->hover_candidate->mloc;

  hint_string = g_string_new (NULL);
  g_string_append_printf (hint_string, "%s.\n", mloc->mark->name);

  date_time = g_date_time_new_from_unix_local (mloc->mark->ctime / G_TIME_SPAN_SECOND);
  text = g_date_time_format (date_time, "%d.%m.%Y %H:%M:%S");
  g_string_append_printf (hint_string, "%s: %s.\n", _("Created"), text);
  g_free (text);
  g_date_time_unref (date_time);

  date_time = g_date_time_new_from_unix_local (mloc->mark->mtime / G_TIME_SPAN_SECOND);
  text = g_date_time_format (date_time, "%d.%m.%Y %H:%M:%S");
  g_string_append_printf (hint_string, "%s: %s.\n", _("Edited"), text);
  g_free (text);
  g_date_time_unref (date_time);

  lat_text = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LAT, mloc->mark_geo.lat, 6);
  lon_text = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LON, mloc->mark_geo.lon, 6);
  g_string_append_printf (hint_string, "%s: %s, %s.\n", _("Location"), lat_text, lon_text);
  g_free (lat_text);
  g_free (lon_text);

  depth = mloc->depth;

  if (depth < 0)
    g_string_append_printf (hint_string, "%s: %s.\n", _("Depth"), _("Empty"));
  else
    g_string_append_printf (hint_string, "%s: %.f %s.\n", _("Depth"), depth, _("m"));

  g_string_append_printf (hint_string, "%s: %s.", _("Track"), mloc->track_name);

  if (mloc->mark->description != NULL && 0 != g_strcmp0 (mloc->mark->description, ""))
    {
      gchar *tmp = NULL;
      gchar **list = NULL;
      guint array_size, index = 0;

      tmp = g_strconcat (_("Note: "), mloc->mark->description, (gchar*) NULL);
      list = g_strsplit (tmp, " ", -1);
      array_size = g_strv_length (list);

      while (index < array_size)
        {
          guint size = 0;
          g_string_append_c (hint_string, '\n');
          while (size < 48 && index < array_size)
            {
              g_string_append (hint_string, list[index]);
              g_string_append_c (hint_string, ' ');
              size += g_utf8_strlen (list[index], -1) - 1;
              index++;
            }
        }
      g_strfreev (list);
      g_free (tmp);
    }


  return g_string_free (hint_string, FALSE);
}

static void
hyscan_gtk_map_wfmark_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->removed = hyscan_gtk_map_wfmark_removed;
  iface->added = hyscan_gtk_map_wfmark_added;
  iface->set_visible = hyscan_gtk_map_wfmark_set_visible;
  iface->get_visible = hyscan_gtk_map_wfmark_get_visible;
  iface->get_param = hyscan_gtk_map_wfmark_get_param;
  iface->hint_find = hyscan_gtk_map_wfmark_hint_find;
  iface->hint_shown = hyscan_gtk_map_wfmark_hint_shown;
}

/* Функция обновляет виджет и перерисовывает акустические изображения меток.
 * data - указатель на виджет требующий перерисовки. */
static gboolean
hyscan_gtk_map_wfmark_redraw (gpointer data)
{
  gtk_widget_queue_draw (GTK_WIDGET (data));
  return FALSE;
}

/* Функция-обработчик завершения гененрации тайла. По завершении генерации обновляет виджет.
 * wfm_layer - указатель на объект;
 * tile - указатель на структуру содержащую информацию о сгененрированном тайле. Не связана
 * с тайлом, передаваемым в HyScanTileQueue для генерации
 * img - указатель на данные аккустического изображения
 * size - размер данных аккустического изображения
 * hash - хэш состояния
 */
static void
hyscan_gtk_map_wfmark_tile_loaded (HyScanGtkMapWfmark *wfm_layer,
                                   HyScanTile         *tile,
                                   gfloat             *img,
                                   gint                size,
                                   gulong              hash)
{
  HyScanGtkMapWfmarkPrivate *priv = wfm_layer->priv;
  g_idle_add (hyscan_gtk_map_wfmark_redraw, GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_wfmark_new:
 * @model: указатель на модель данных положения меток
 *
 * Returns: создает новый объект #HyScanGtkMapWfmark. Для удаления g_object_unref().
 */
HyScanGtkLayer *
hyscan_gtk_map_wfmark_new (HyScanMarkLocModel *model,
                           HyScanDB           *db,
                           HyScanCache        *cache,
                           HyScanUnits        *units)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_WFMARK,
                       "mark-loc-model", model,
                       "db", db,
                       "cache", cache,
                       "units", units,
                       NULL);
}

/**
 * hyscan_gtk_map_wfmark_mark_highlight:
 * @wfm_layer: указатель на объект
 * @mark_id: идентификатор метки
 *
 * Подсвечивает метку с указанным идентификатором
 */
void
hyscan_gtk_map_wfmark_mark_highlight (HyScanGtkMapWfmark *wfm_layer,
                                      const gchar        *mark_id)
{
  HyScanGtkMapWfmarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK (wfm_layer));
  priv = wfm_layer->priv;

  if (priv->map == NULL)
    return;

  g_free (priv->active_mark_id);
  priv->active_mark_id = g_strdup (mark_id);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_wfmark_mark_view:
 * @wfm_layer: указатель на объект
 * @mark_id: идентификатор метки
 * @zoom_in: изменить масштаб видимой области до размеров метки
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
}

/**
 * hyscan_gtk_map_wfmark_mark_set_project:
 * @wfm_layer: указатель на объект
 * @project_name: указатель на название проекта
 *
 * Функция устанавливает проект для слоя.
 *
 **/
void
hyscan_gtk_map_wfmark_set_project (HyScanGtkMapWfmark *wfm_layer,
                                   const gchar        *project_name)
{
  HyScanGtkMapWfmarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK (wfm_layer));
  priv = wfm_layer->priv;

  g_free (priv->project);
  priv->project = g_strdup (project_name);
  hyscan_factory_amplitude_set_project (priv->factory_amp,
                                        priv->db,
                                        priv->project);
  hyscan_factory_depth_set_project (priv->factory_dpt,
                                    priv->db,
                                    priv->project);
}

/**
 * hyscan_gtk_map_wfmark_mark_set_project:
 * @wfm_layer: указатель на объект
 * @mode: идентификатор режима отображения меток
 *
 * Функция устанавливает режим отображения меток:
 * SHOW_ACOUSTIC_IMAGE - отображать акустическое изображение метки;
 * SHOW_ONLY_BORDER - отображать только границу метки.
 *
 **/
void
hyscan_gtk_map_wfmark_set_show_mode (HyScanGtkMapWfmark *wfm_layer,
                                     gint                mode)
{
  HyScanGtkMapWfmarkPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_WFMARK (wfm_layer));
  priv = wfm_layer->priv;

  priv->show_mode = mode;
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}
