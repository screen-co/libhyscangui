/* hyscan-gtk-map-nav.c
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
 * SECTION: hyscan-gtk-map-nav
 * @Short_description: слой карты с траекторией движения объекта
 * @Title: HyScanGtkMapNav
 * @See_also: #HyScanGtkLayer
 *
 * Класс позволяет выводить на карте #HyScanGtkMap слой с изображением движения
 * объекта в режиме реального времени.
 *
 * Слой изображет траекторию движения в виде линии пройденного маршрута, а текущее
 * положение объекта в виде стрелки, направленной по курсу движения. Стиль
 * оформления можно задать параметрами конфигурации (см. hyscan_gtk_layer_get_param()):
 *
 * - "line-width" - толщина линии траектории,
 * - "line-color" - цвет линии траектории,
 * - "arrow-color" - цвет стрелки,
 * - "arrow-stroke-color" - цвет обводки стрелки,
 * - "lost-line-color" - цвет линии при потере сигнала,
 * - "lost-arrow-color" - цвет стрелки при потере сигнала,
 * - "lost-arrow-stroke-color" - цвет обводки стрелки при потере сигнала,
 * - "text-color" - цвет текста,
 * - "bg-color" - цвет фона текста.
 *
 * Выводимые данные получаются из модели #HyScanNavState, которая указывается
 * при создании слоя в hyscan_gtk_map_nav_new().
 *
 */

#include "hyscan-gtk-map-nav.h"
#include "hyscan-gtk-map.h"
#include "hyscan-gtk-layer-param.h"
#include <hyscan-cartesian.h>
#include <hyscan-nav-model.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <hyscan-param-controller.h>

#define ARROW_SIZE            40                           /* Размер маркера, изображающего движущийся объект. */
#define LIFETIME              600                          /* Время жизни путевых точек, секунды. */

/* Раскомментируйте строку ниже для вывода отладочной информации о скорости отрисовки слоя. */
// #define HYSCAN_GTK_MAP_DEBUG_FPS

enum
{
  PROP_O,
  PROP_NAV_MODEL,
  PROP_USE_CACHE
};

typedef struct
{
  HyScanGtkMapPoint                    coord;          /* Путевая точка. */
  gdouble                              speed;          /* Скорость, м/с. */
  gdouble                              cog;            /* Курс COG. */
  gdouble                              heading;        /* Истинный курс HDG. */
  gboolean                             true_heading;   /* Курс истинный? */
  gdouble                              time;           /* Время фиксации точки. */
  gboolean                             start;          /* Признак того, что точка является началом куска. */
} HyScanGtkMapNavPoint;

typedef struct
{
  gdouble                              line_width;                 /* Ширина линии трека. */
  GdkRGBA                              line_color;                 /* Цвет линии трека. */
  GdkRGBA                              line_lost_color;            /* Цвет линии при обрыве связи. */
  GdkRGBA                              arrow_color;                /* Цвет заливки стрелки. */
  GdkRGBA                              arrow_stroke_color;         /* Цвет обводки стрелки. */
  GdkRGBA                              lost_arrow_color;           /* Цвет заливки стрелки при обрыве связи. */
  GdkRGBA                              lost_arrow_stroke_color;    /* Цвет заливки обводки при обрыве связи. */
  GdkRGBA                              text_color;                 /* Цвет текса на панели навигационной информации. */
  GdkRGBA                              bg_color;                   /* Фоновый цвет панели навигационной информации. */
} HyScanGtkMapNavStyle;

typedef struct
{
  cairo_surface_t            *surface;   /* Поверхность с изображением стрелки. */
  gdouble                     x;         /* Координата x положения судна на поверхности surface. */
  gdouble                     y;         /* Координата y положения судна на поверхности surface. */
} HyScanGtkMapNavArrow;

struct _HyScanGtkMapNavPrivate
{
  HyScanGtkMap                 *map;                        /* Виджет карты, на котором размещен слой. */
  gboolean                      has_cache;                  /* Флаг, используется ли кэш? */
  HyScanNavState               *nav_model;                  /* Модель навигационных данных, которые отображаются. */
  guint64                       life_time;                  /* Время жизни точки трека, секунды. */
  gdouble                       cog_line_time;              /* Время предсказания движения по COG, сек. */
  gdouble                       hdg_line_length;            /* Длина линии истинного курса HDG, м. */
  PangoLayout                  *pango_layout;               /* Раскладка шрифта. */

  gboolean                      visible;                    /* Признак видимости слоя. */

  /* Информация о треке. */
  GQueue                       *track;                      /* Список точек трека. */
  guint                         track_mod_count;            /* Номер изменения точек трека. */
  gboolean                      track_lost;                 /* Признак того, что сигнал потерян. */
  GMutex                        track_lock;                 /* Блокировка доступа к точкам трека.  */

  /* Внешний вид. */
  HyScanGtkLayerParam          *param;                      /* Параметры оформления. */
  HyScanGtkMapNavStyle          new_style;                  /* Новый стиль оформления, который надо применить. */
  HyScanGtkMapNavStyle          style;                      /* Стиль оформления. */
  HyScanGtkMapNavArrow          arrow_default;              /* Маркер объекта в обычном режиме. */
  HyScanGtkMapNavArrow          arrow_lost;                 /* Маркер объекта, если сигнал потерян. */
  guint                         margin;                     /* Внешний отступ панели навигационной информации. */
  guint                         padding;                    /* Внутренний отступ панели навигационной информации. */
};

static void              hyscan_gtk_map_nav_interface_init          (HyScanGtkLayerInterface       *iface);
static void              hyscan_gtk_map_nav_set_property            (GObject                       *object,
                                                                     guint                          prop_id,
                                                                     const GValue                  *value,
                                                                     GParamSpec                    *pspec);
static void              hyscan_gtk_map_nav_object_constructed      (GObject                       *object);
static void              hyscan_gtk_map_nav_object_finalize         (GObject                       *object);
static void              hyscan_gtk_map_nav_param_set               (HyScanGtkLayer                *layer,
                                                                     HyScanParamList               *list);
static HyScanParam *     hyscan_gtk_map_nav_get_param               (HyScanGtkLayer                *layer);
static void              hyscan_gtk_map_nav_removed                 (HyScanGtkLayer                *layer);
static void              hyscan_gtk_map_nav_draw                    (HyScanGtkMap                  *map,
                                                                     cairo_t                       *cairo,
                                                                     HyScanGtkMapNav               *nav_layer);
static void              hyscan_gtk_map_area_draw                   (HyScanGtkMap                  *map,
                                                                     cairo_t                       *cairo,
                                                                     HyScanGtkMapNav               *nav_layer);
static void              hyscan_gtk_map_nav_proj_notify             (HyScanGtkMapNav               *nav_layer,
                                                                     GParamSpec                    *pspec);
static void              hyscan_gtk_map_nav_added                   (HyScanGtkLayer                *layer,
                                                                     HyScanGtkLayerContainer       *container);
static gboolean          hyscan_gtk_map_nav_get_visible             (HyScanGtkLayer                *layer);
static void              hyscan_gtk_map_nav_set_visible             (HyScanGtkLayer                *layer,
                                                                     gboolean                       visible);
static void              hyscan_gtk_map_nav_create_arrow            (HyScanGtkMapNavArrow          *arrow,
                                                                     GdkRGBA                       *color_fill,
                                                                     GdkRGBA                       *color_stroke);
static void              hyscan_gtk_map_nav_model_changed           (HyScanGtkMapNav               *nav_layer,
                                                                    HyScanNavStateData             *data);
static void              hyscan_gtk_map_nav_point_free              (HyScanGtkMapNavPoint          *point);
static void              hyscan_gtk_map_nav_fill_tile               (HyScanGtkMapTiled             *tiled_layer,
                                                                     HyScanMapTile                 *tile);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapNav, hyscan_gtk_map_nav, HYSCAN_TYPE_GTK_MAP_TILED,
                         G_ADD_PRIVATE (HyScanGtkMapNav)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_nav_interface_init))

static void
hyscan_gtk_map_nav_class_init (HyScanGtkMapNavClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapTiledClass *tiled_class = HYSCAN_GTK_MAP_TILED_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_nav_set_property;
  object_class->constructed = hyscan_gtk_map_nav_object_constructed;
  object_class->finalize = hyscan_gtk_map_nav_object_finalize;

  tiled_class->fill_tile = hyscan_gtk_map_nav_fill_tile;

  g_object_class_install_property (object_class, PROP_NAV_MODEL,
    g_param_spec_object ("nav-state", "Navigation model", "HyScanNavState",
                         HYSCAN_TYPE_NAV_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_USE_CACHE,
    g_param_spec_boolean ("use-cache", "Cache layer in tiles", "Set TRUE to draw layer with tiles", TRUE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_nav_init (HyScanGtkMapNav *gtk_map_nav)
{
  gtk_map_nav->priv = hyscan_gtk_map_nav_get_instance_private (gtk_map_nav);
}

static void
hyscan_gtk_map_nav_set_property (GObject     *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanGtkMapNav *gtk_map_nav = HYSCAN_GTK_MAP_NAV (object);
  HyScanGtkMapNavPrivate *priv = gtk_map_nav->priv;

  switch (prop_id)
    {
    case PROP_NAV_MODEL:
      priv->nav_model = g_value_dup_object (value);
      break;

    case PROP_USE_CACHE:
      priv->has_cache = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_nav_object_constructed (GObject *object)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (object);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_nav_parent_class)->constructed (object);

  priv->track = g_queue_new ();

  g_mutex_init (&priv->track_lock);

  /* Настройки внешнего вида. */
  priv->param = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-nav");
  hyscan_gtk_layer_param_add_rgba (priv->param, "/line-color", &priv->new_style.line_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/lost-line-color", &priv->new_style.line_lost_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/arrow-color", &priv->new_style.arrow_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/arrow-stroke-color", &priv->new_style.arrow_stroke_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/lost-arrow-color", &priv->new_style.lost_arrow_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/lost-arrow-stroke-color", &priv->new_style.lost_arrow_stroke_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/text-color", &priv->new_style.text_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/bg-color", &priv->new_style.bg_color);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (priv->param), "/line-width", &priv->new_style.line_width);
  g_signal_connect_swapped (priv->param, "set", G_CALLBACK (hyscan_gtk_map_nav_param_set), nav_layer);

  hyscan_gtk_layer_param_set_default (priv->param);

  priv->cog_line_time = 120.0;
  priv->hdg_line_length = 1000.0;
  hyscan_gtk_map_nav_set_lifetime (nav_layer, LIFETIME);

  g_signal_connect_swapped (priv->nav_model, "nav-changed", G_CALLBACK (hyscan_gtk_map_nav_model_changed), nav_layer);
}

static void
hyscan_gtk_map_nav_object_finalize (GObject *object)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (object);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->nav_model, nav_layer);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->param);
  g_clear_pointer (&priv->arrow_default.surface, cairo_surface_destroy);
  g_clear_pointer (&priv->arrow_lost.surface, cairo_surface_destroy);
  g_clear_object (&priv->pango_layout);

  g_mutex_lock (&priv->track_lock);
  g_queue_free_full (priv->track, (GDestroyNotify) hyscan_gtk_map_nav_point_free);
  g_mutex_unlock (&priv->track_lock);

  g_mutex_clear (&priv->track_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_nav_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_nav_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->added = hyscan_gtk_map_nav_added;
  iface->removed = hyscan_gtk_map_nav_removed;
  iface->set_visible = hyscan_gtk_map_nav_set_visible;
  iface->get_visible = hyscan_gtk_map_nav_get_visible;
  iface->get_param = hyscan_gtk_map_nav_get_param;
}

static void
hyscan_gtk_map_nav_param_set (HyScanGtkLayer  *layer,
                              HyScanParamList *list)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  /* Блокируем доступ к треку, пока не установим новые параметры. */
  g_mutex_lock (&priv->track_lock);

  /* Стиль оформления. */
  priv->style = priv->new_style;

  /* Маркер в обычном состоянии и при потери сигнала. */
  hyscan_gtk_map_nav_create_arrow (&priv->arrow_default, &priv->style.arrow_color, &priv->style.arrow_stroke_color);
  hyscan_gtk_map_nav_create_arrow (&priv->arrow_lost, &priv->style.lost_arrow_color, &priv->style.lost_arrow_stroke_color);

  g_mutex_unlock (&priv->track_lock);

  /* Фиксируем изменение параметров. */
  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (nav_layer));
  /* Ставим флаг о необходимости перерисовки. */
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (nav_layer));
}

/* Получает настройки оформления слоя. */
static HyScanParam *
hyscan_gtk_map_nav_get_param (HyScanGtkLayer *layer)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  return g_object_ref (priv->param);
}

/* Обрабатывает "истекшие" путевые точки, т.е. те, у которых время жизни закончилось:
 * - удаляет эти точки из трека,
 * - обновляет тайлы с удаленными точками.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_nav_set_expired_mod (HyScanGtkMapNav *nav_layer,
                                    gdouble          time)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  GList *track_l;
  GList *new_tail;

  gdouble delete_time;

  if (g_queue_is_empty (priv->track))
    return;

  /* 1. Находим new_tail - последнюю актуальную точку трека. */
  delete_time = time - priv->life_time;
  for (new_tail = priv->track->tail; new_tail != NULL; new_tail = new_tail->prev)
    {
      HyScanGtkMapNavPoint *cur_point = new_tail->data;

      if (cur_point->time > delete_time)
        break;
    }

  /* 2. Делаем недействительными всё тайлы, которые содержат устаревшие точки. */
  for (track_l = priv->track->tail; track_l != new_tail; track_l = track_l->prev)
    {
      HyScanGtkMapNavPoint *prev_point;
      HyScanGtkMapNavPoint *cur_point;

      if (track_l->prev == NULL)
        continue;

      prev_point = track_l->prev->data;
      cur_point  = track_l->data;
      hyscan_gtk_map_tiled_set_area_mod (HYSCAN_GTK_MAP_TILED (nav_layer),
                                               &cur_point->coord.c2d, &prev_point->coord.c2d);
    }

  /* 3. Удаляем устаревшие точки. */
  for (track_l = priv->track->tail; track_l != new_tail; )
    {
      GList *prev_l = track_l->prev;
      HyScanGtkMapNavPoint *cur_point = track_l->data;

      g_queue_delete_link (priv->track, track_l);
      hyscan_gtk_map_nav_point_free (cur_point);

      track_l = prev_l;
    }
}

/* Присваивает тайлам с новыми точками актуальный номер изменения mod_count.
 * Выполняется в потоке, откуда пришел сигнал "changed", т.е. в Main Loop.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_nav_set_head_mod (HyScanGtkMapNav *nav_layer)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  GList *head_l;
  HyScanGtkMapNavPoint *track_point0, *track_point1 = NULL;

  if (!priv->has_cache || priv->map == NULL)
    return;

  /* Определяем концы последнего отрезка: point0 и point1. */
  head_l = priv->track->head;
  if (head_l == NULL || head_l->next == NULL)
    return;

  track_point0 = head_l->data;
  track_point1 = head_l->next->data;

  /* Актуализируем тайлы, содержащие найденный отрезок. */
  hyscan_gtk_map_tiled_set_area_mod (HYSCAN_GTK_MAP_TILED (nav_layer),
                                           &track_point0->coord.c2d, &track_point1->coord.c2d);
}

/* Освобождает память, занятую структурой HyScanGtkMapNavPoint. */
static void
hyscan_gtk_map_nav_point_free (HyScanGtkMapNavPoint *point)
{
  g_slice_free (HyScanGtkMapNavPoint, point);
}

/* Копирует структуру HyScanGtkMapNavPoint. */
static HyScanGtkMapNavPoint *
hyscan_gtk_map_nav_point_copy (HyScanGtkMapNavPoint *point)
{
  return g_slice_dup (HyScanGtkMapNavPoint, point);
}

/* Обработчик сигнала "changed" модели. */
static void
hyscan_gtk_map_nav_model_changed (HyScanGtkMapNav    *nav_layer,
                                  HyScanNavStateData *data)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  g_mutex_lock (&priv->track_lock);

  /* Фиксируем изменение состояния трека. */
  priv->track_mod_count++;

  /* Добавляем новую точку в трек. */
  if (data->loaded)
    {
      HyScanGtkMapNavPoint point;

      point.time = data->time;
      point.start = priv->track_lost;
      point.coord.geo.lat = data->coord.lat;
      point.coord.geo.lon = data->coord.lon;
      point.coord.geo.h = data->heading;
      point.speed = data->speed;
      point.heading = data->heading;
      point.cog = data->coord.h;
      point.true_heading = data->true_heading;
      if (priv->map != NULL)
        hyscan_gtk_map_geo_to_value (priv->map, point.coord.geo, &point.coord.c2d);

      g_queue_push_head (priv->track, hyscan_gtk_map_nav_point_copy (&point));

      hyscan_gtk_map_nav_set_head_mod (nav_layer);
    }

  /* Фиксируем обрыв. */
  priv->track_lost = !data->loaded;

  /* Удаляем устаревшие по life_time точки. */
  hyscan_gtk_map_nav_set_expired_mod (nav_layer, data->time);

  g_mutex_unlock (&priv->track_lock);

  if (hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (nav_layer)))
    hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (nav_layer));
}

/* Создаёт cairo-поверхность с изображением стрелки в голове трека. */
static void
hyscan_gtk_map_nav_create_arrow (HyScanGtkMapNavArrow *arrow,
                                 GdkRGBA              *color_fill,
                                 GdkRGBA              *color_stroke)
{
  cairo_t *cairo;
  guint line_width = 1;

  g_clear_pointer (&arrow->surface, cairo_surface_destroy);

  arrow->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                               ARROW_SIZE + 2 * line_width, ARROW_SIZE + 2 * line_width);
  cairo = cairo_create (arrow->surface);

  cairo_translate (cairo, line_width, line_width);

  /* Стрелка. */
  cairo_line_to (cairo, .5 * ARROW_SIZE, .0);
  cairo_line_to (cairo, .8 * ARROW_SIZE, ARROW_SIZE);
  cairo_line_to (cairo, .5 * ARROW_SIZE, .8 * ARROW_SIZE);
  cairo_line_to (cairo, .2 * ARROW_SIZE, ARROW_SIZE);
  cairo_line_to (cairo, .5 * ARROW_SIZE, .0);
  cairo_close_path (cairo);

  gdk_cairo_set_source_rgba (cairo, color_fill);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, line_width);
  gdk_cairo_set_source_rgba (cairo, color_stroke);
  cairo_stroke (cairo);

  cairo_destroy (cairo);

  /* Координаты начала отсчета на поверхности. */
  arrow->x = -.5 * ARROW_SIZE - (gdouble) line_width;
  arrow->y = - (.8 * ARROW_SIZE + (gdouble) line_width);
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_nav_set_visible (HyScanGtkLayer *layer,
                                gboolean        visible)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  priv->visible = visible;

  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (nav_layer));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_nav_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  return priv->visible;
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Отключается от сигналов карты при удалении слоя с карты. */
static void
hyscan_gtk_map_nav_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  hyscan_gtk_layer_parent_interface->removed (layer);

  /* Отключаемся от карты. */
  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
}

/* Рисует кусок трека от chunk_start до chunk_end. */
static void
hyscan_gtk_map_nav_draw_chunk (HyScanGtkMapNav *nav_layer,
                               cairo_t         *cairo,
                               gdouble          from_x,
                               gdouble          to_x,
                               gdouble          from_y,
                               gdouble          to_y,
                               gdouble          scale,
                               GList           *chunk_start,
                               GList           *chunk_end)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  GList *chunk_l;
  gboolean finish;

  cairo_set_line_width (cairo, priv->style.line_width);

  /* Рисуем линию по всем точкам от chunk_start до chunk_end.
   * В момент потери сигнала рисуем линию другим цветом (line_lost_color). */
  cairo_new_path (cairo);
  chunk_l = chunk_start;
  finish = (chunk_l == NULL);
  while (!finish)
    {
      HyScanGtkMapNavPoint *track_point = chunk_l->data;
      HyScanGeoCartesian2D *point = &track_point->coord.c2d;
      gdouble x, y;

      /* Координаты точки на поверхности. */
      x = (point->x - from_x) / scale;
      y = (from_y - to_y) / scale - (point->y - to_y) / scale;

      /* Точка из нового отрезка - завершаем рисование прошлого отрезка. */
      if (track_point->start && cairo_has_current_point (cairo))
        {
          gdouble prev_x, prev_y;

          cairo_get_current_point (cairo, &prev_x, &prev_y);
          gdk_cairo_set_source_rgba (cairo, &priv->style.line_color);
          cairo_stroke (cairo);

          cairo_move_to (cairo, prev_x, prev_y);
        }

      cairo_line_to (cairo, x, y);

      /* Точка после потери сигнала - рисуем lost-линию. */
      if (track_point->start)
        {
          gdk_cairo_set_source_rgba (cairo, &priv->style.line_lost_color);
          cairo_stroke (cairo);

          cairo_move_to (cairo, x, y);
        }

      finish = (chunk_l == chunk_end || chunk_l->next == NULL);
      chunk_l = chunk_l->next;
    }

  gdk_cairo_set_source_rgba (cairo, &priv->style.line_color);
  cairo_stroke (cairo);
}

/* Рисует трек в указанном регионе. Возвращает mod_count нарисованного трека.  */
static guint
hyscan_gtk_map_nav_draw_region (HyScanGtkMapNav *nav_layer,
                                cairo_t         *cairo,
                                gdouble          from_x,
                                gdouble          to_x,
                                gdouble          from_y,
                                gdouble          to_y,
                                gdouble          scale)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;
  GList *track_l;

  GList *chunk_start = NULL;
  GList *chunk_end = NULL;

  guint mod_count;

  GList *point1_l = NULL, *point0_l = NULL;
  HyScanGeoCartesian2D area_from = {.x = from_x, .y = from_y};
  HyScanGeoCartesian2D area_to   = {.x = to_x, .y = to_y};

  /* Блокируем доступ к треку на время прорисовки. */
  g_mutex_lock (&priv->track_lock);

  mod_count = priv->track_mod_count;
  if (g_queue_is_empty (priv->track))
    {
      g_mutex_unlock (&priv->track_lock);
      return mod_count;
    }

  /* Ищем куски трека, которые полностью попадают в указанный регион и рисуем покусочно. */
  for (track_l = priv->track->head; track_l != NULL; track_l = track_l->next, point1_l = point0_l)
    {
      HyScanGtkMapNavPoint *track_point0, *track_point1;
      HyScanGeoCartesian2D *point0, *point1;
      gboolean is_inside;

      point0_l = track_l;

      if (point1_l == NULL)
        continue;

      /* Проверяем, находится ли отрезок (point1, point0) в указанной области. */
      track_point0 = point0_l->data;
      track_point1 = point1_l->data;
      point0 = &track_point0->coord.c2d;
      point1 = &track_point1->coord.c2d;

      is_inside = hyscan_cartesian_is_inside (point0, point1, &area_from, &area_to);

      if (is_inside && chunk_start == NULL)
        /* Фиксируем начало куска. */
        {
          chunk_start = point1_l;
        }

      else if (!is_inside && chunk_start != NULL)
        /* Фиксируем конец куска. */
        {
          chunk_end = point0_l;
          hyscan_gtk_map_nav_draw_chunk (nav_layer, cairo,
                                               from_x, to_x, from_y, to_y, scale,
                                               chunk_start, chunk_end);

          chunk_start = NULL;
          chunk_end = NULL;
        }
    }

  /* Рисуем последний кусок. */
  if (chunk_start != NULL)
    {
      hyscan_gtk_map_nav_draw_chunk (nav_layer, cairo,
                                           from_x, to_x, from_y, to_y, scale,
                                           chunk_start, chunk_end);
    }

  g_mutex_unlock (&priv->track_lock);

  return mod_count;
}

/* Заполняет поверхность тайла. Возвращает номер состояния трека на момент рисования */
static void
hyscan_gtk_map_nav_fill_tile (HyScanGtkMapTiled *tiled_layer,
                              HyScanMapTile     *tile)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (tiled_layer);
  cairo_t *tile_cairo;

  HyScanGeoCartesian2D from, to;
  cairo_surface_t *surface;
  gint width;
  gdouble scale;

  width = hyscan_map_tile_get_size (tile);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, width);
  tile_cairo = cairo_create (surface);

  /* Заполняем поверхность тайла. */
  hyscan_map_tile_get_bounds (tile, &from, &to);
  scale = hyscan_map_tile_get_scale (tile);
  hyscan_gtk_map_nav_draw_region (nav_layer, tile_cairo, from.x, to.x, from.y, to.y, scale);

  /* Записываем поверхность в тайл. */
  hyscan_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (tile_cairo);
}

/* Рисует трек сразу на всю видимую область.
 * Альтернатива рисованию тайлами hyscan_gtk_map_nav_draw_tiles().
 * При небольшом треке экономит время, потому что не надо определять тайлы и
 * лезть в кэш. */
static void
hyscan_gtk_map_nav_draw_full (HyScanGtkMapNav *nav_layer,
                              cairo_t         *cairo)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale;

  cairo_t *layer_cairo;
  cairo_surface_t *layer_surface;
  guint width, height;

  /* Создаем поверхность слоя. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->map), &width, &height);
  layer_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  layer_cairo = cairo_create (layer_surface);

  /* Рисуем трек на слое. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_get_scale_idx (priv->map, &scale);
  hyscan_gtk_map_nav_draw_region (nav_layer, layer_cairo, from_x, to_x, from_y, to_y, scale);

  /* Переносим слой на поверхность виджета. */
  cairo_set_source_surface (cairo, layer_surface, 0, 0);
  cairo_paint (cairo);

  cairo_destroy (layer_cairo);
  cairo_surface_destroy (layer_surface);
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_nav_draw (HyScanGtkMap    *map,
                         cairo_t         *cairo,
                         HyScanGtkMapNav *nav_layer)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  gdouble x, y;
  HyScanGtkMapNavPoint last_track_point;
  gdouble heading, cog, speed;
  gboolean true_heading;
  gdouble scale_proj, scale_px, cog_len;
  HyScanGtkMapPoint *last_point;

  HyScanGtkMapNavArrow *arrow;

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  static GTimer *debug_timer;
  static gdouble frame_time[25];
  static guint64 frame_idx = 0;

  if (debug_timer == NULL)
    debug_timer = g_timer_new ();
  g_timer_start (debug_timer);
  frame_idx++;
#endif

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (nav_layer)))
    return;

  /* Получаем координаты последней точки - маркера текущего местоположения. */
  {
    g_mutex_lock (&priv->track_lock);

    if (priv->track->head == NULL)
      {
        g_mutex_unlock (&priv->track_lock);
        return;
      }

    last_track_point = * (HyScanGtkMapNavPoint *) priv->track->head->data;

    g_mutex_unlock (&priv->track_lock);
  }

  last_point = &last_track_point.coord;
  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, last_point->c2d.x, last_point->c2d.y);
  heading = last_track_point.heading;
  cog = last_track_point.cog;
  speed = last_track_point.speed;
  true_heading = last_track_point.true_heading;

  /* Рисуем трек тайлами по одной из стратегий: _draw_full() или _draw_tiles(). */
  if (priv->has_cache)
    hyscan_gtk_map_tiled_draw (HYSCAN_GTK_MAP_TILED (nav_layer), cairo);
  else
    hyscan_gtk_map_nav_draw_full (nav_layer, cairo);

  /* Рисуем маркер движущегося объекта. */
  cairo_save (cairo);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, heading);
  arrow = priv->track_lost ? &priv->arrow_lost : &priv->arrow_default;
  cairo_set_source_surface (cairo, arrow->surface, arrow->x, arrow->y);
  cairo_paint (cairo);

  /* Линия COG и HDG. */
  scale_proj = hyscan_gtk_map_get_scale_value (priv->map, last_track_point.coord.geo);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_px, NULL);
  gdk_cairo_set_source_rgba (cairo, &priv->style.line_color);

  /* HDG. */
  if (true_heading)
    {
      gdouble hdg_length;

      hdg_length = priv->hdg_line_length / scale_proj / scale_px;
      if (hdg_length > ARROW_SIZE)
        {
          gdouble hdg_dash[] = { 2.0, 4.0 };

          cairo_move_to (cairo, 0, 0);
          cairo_line_to (cairo, 0, -hdg_length);
          cairo_set_dash (cairo, hdg_dash, G_N_ELEMENTS (hdg_dash), 0.0);
          cairo_stroke (cairo);

          cairo_set_dash (cairo, NULL, 0, 0.0);
        }
    }

  /* COG. */
  cairo_rotate (cairo, cog - heading);
  cog_len = priv->cog_line_time * speed / scale_proj / scale_px;
  if (cog_len > ARROW_SIZE)
    {
      gdouble cog_dash[] = { 10.0, 4.0 };

      /* Квадрат со стороной 10 и центром в конце линии COG. */
      cairo_move_to (cairo, -5, - (cog_len - 5)),
      cairo_rel_line_to (cairo,  10,   0);
      cairo_rel_line_to (cairo,   0, -10);
      cairo_rel_line_to (cairo, -10,   0);
      cairo_rel_line_to (cairo,   0,  10);
      cairo_stroke (cairo);

      /* Линия COG, укороченная на половину стороны квадрата. */
      cairo_move_to (cairo, 0, 0);
      cairo_line_to (cairo, 0, - (cog_len - 5 ));
      cairo_set_dash (cairo, cog_dash, G_N_ELEMENTS (cog_dash), 0.0);
      cairo_stroke (cairo);
    }
  cairo_restore (cairo);

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  {
    guint dbg_i = 0;
    gdouble dbg_time = 0;
    guint dbg_frames;

    dbg_frames = G_N_ELEMENTS (frame_time);

    frame_idx = (frame_idx + 1) % dbg_frames;
    frame_time[frame_idx] = g_timer_elapsed (debug_timer, NULL);
    for (dbg_i = 0; dbg_i < dbg_frames; ++dbg_i)
      dbg_time += frame_time[dbg_i];

    dbg_time /= (gdouble) dbg_frames;
    g_message ("hyscan_gtk_map_nav_draw: %.2f fps; length = %d",
               1.0 / dbg_time,
               g_queue_get_length (priv->track));
  }
#endif
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_area_draw (HyScanGtkMap    *map,
                          cairo_t         *cairo,
                          HyScanGtkMapNav *nav_layer)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;
  HyScanGtkMapNavPoint last_track_point;

  gdouble angle, speed;
  gboolean true_heading;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (nav_layer)))
    return;

  /* Получаем координаты последней точки - маркера текущего местоположения. */
  {
    g_mutex_lock (&priv->track_lock);

    if (priv->track->head == NULL)
      {
        g_mutex_unlock (&priv->track_lock);
        return;
      }

    last_track_point = * (HyScanGtkMapNavPoint *) priv->track->head->data;
    g_mutex_unlock (&priv->track_lock);
  }

  true_heading = last_track_point.true_heading;
  angle = true_heading ? last_track_point.heading : last_track_point.cog;
  speed = last_track_point.speed;

  /* Рисуем текущий курс, скорость и координаты. */
  {
    gchar *label;
    guint area_width;
    gint height, width;

    gtk_cifro_area_get_size (GTK_CIFRO_AREA (priv->map), &area_width, NULL);

    if (priv->track_lost)
      {
        label = g_strdup_printf ("<big>%s</big>", _("Signal lost"));
      }
    else
      {
        label = g_strdup_printf ("<big>%06.2f° (%s)\n%03.2f %s</big>",
                                 angle / G_PI * 180.0,
                                 true_heading ? C_("Heading", "HDG") : C_("Heading", "COG"),
                                 speed,
                                 _("m/s"));
      }

    pango_layout_set_markup (priv->pango_layout, label, -1);
    pango_layout_get_size (priv->pango_layout, &width, &height);
    width /= PANGO_SCALE;
    height /= PANGO_SCALE;

    cairo_save (cairo);

    /* Перемещаемся в начало фонового прямоугольника. */
    cairo_translate (cairo, area_width - priv->margin - (width + 2 * priv->padding), priv->margin);

    cairo_rectangle (cairo, 0, 0, width + 2 * priv->padding, height + 2 * priv->padding);
    gdk_cairo_set_source_rgba (cairo, &priv->style.bg_color);
    cairo_fill (cairo);

    gdk_cairo_set_source_rgba (cairo, &priv->style.text_color);
    cairo_move_to (cairo, priv->padding, priv->padding);
    pango_cairo_show_layout (cairo, priv->pango_layout);

    cairo_restore (cairo);

    g_free (label);
  }
}

/* Переводит координаты путевых точек трека из географичекских в СК проекции карты. */
static void
hyscan_gtk_map_nav_update_points (HyScanGtkMapNavPrivate *priv)
{
  HyScanGtkMapNavPoint *point;
  GList *track_l;

  g_mutex_lock (&priv->track_lock);

  for (track_l = priv->track->head; track_l != NULL; track_l = track_l->next)
    {
      point = track_l->data;
      hyscan_gtk_map_geo_to_value (priv->map, point->coord.geo, &point->coord.c2d);
    }

  g_mutex_unlock (&priv->track_lock);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_nav_proj_notify (HyScanGtkMapNav *nav_layer,
                                GParamSpec      *pspec)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  /* Обновляем координаты точек согласно новой проекции. */
  hyscan_gtk_map_nav_update_points (priv);

  /* Фиксируем изменение параметров. */
  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (nav_layer));
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_nav_configure (HyScanGtkMapNav *nav_layer,
                              GdkEvent        *screen)
{
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;
  gint height;

  g_clear_object (&priv->pango_layout);

  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), "1234567890");
  pango_layout_get_size (priv->pango_layout, NULL, &height);

  height /= PANGO_SCALE;

  priv->margin = 1.5 * height;
  priv->padding = priv->margin / 4;

  return FALSE;
}

/* Реализация HyScanGtkLayerInterface.added.
 * Подключается к сигналам карты при добавлении слоя на карту. */
static void
hyscan_gtk_map_nav_added (HyScanGtkLayer          *layer,
                          HyScanGtkLayerContainer *container)
{
  HyScanGtkMapNav *nav_layer = HYSCAN_GTK_MAP_NAV (layer);
  HyScanGtkMapNavPrivate *priv = nav_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  /* Подключаемся к карте. */
  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  hyscan_gtk_map_nav_update_points (nav_layer->priv);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_nav_draw), nav_layer);
  g_signal_connect_after (priv->map, "area-draw",    G_CALLBACK (hyscan_gtk_map_area_draw), nav_layer);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_nav_proj_notify), nav_layer);
  g_signal_connect_swapped (priv->map, "configure-event", G_CALLBACK (hyscan_gtk_map_nav_configure), nav_layer);

  hyscan_gtk_layer_parent_interface->added (layer, container);
}

/**
 * hyscan_gtk_map_nav_new:
 * @nav_model: указатель на модель навигационных данных #HyScanNavState
 *
 * Создает новый слой с треком движения объекта.
 *
 * Returns: указатель на #HyScanGtkMapNav
 */
HyScanGtkLayer *
hyscan_gtk_map_nav_new (HyScanNavState *nav_state)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_NAV,
                       "nav-state", nav_state,
                       NULL);
}

/**
 * hyscan_gtk_map_nav_set_lifetime:
 * @nav_layer: указатель на #HyScanGtkMapNav
 * @lifetime: время жизни точек трека, секунды
 *
 * Устанавливает время жизни точек траектории. Все точки, которые были добавлены
 * более @lifetime секунд назад, не будут выводиться.
 */
void
hyscan_gtk_map_nav_set_lifetime (HyScanGtkMapNav *nav_layer,
                                 guint64          lifetime)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_NAV (nav_layer));

  nav_layer->priv->life_time = lifetime;
}

/**
 * hyscan_gtk_map_nav_set_cog_len:
 * @nav_layer: указатель на #HyScanGtkMapNav
 * @seconds: время, на которое продлевать путевую линия
 *
 * Устанавливает время, по которому определяется путевая линия. Путевая линия
 * показывает траекторию, по какой будет двигаться судно ближайшие @seconds секунд,
 * при текущей скорости и направлении движения.
 */
void
hyscan_gtk_map_nav_set_cog_len (HyScanGtkMapNav *nav_layer,
                                gdouble          seconds)
{

  g_return_if_fail (HYSCAN_IS_GTK_MAP_NAV (nav_layer));

  nav_layer->priv->cog_line_time = seconds;
  gtk_widget_queue_draw (GTK_WIDGET (nav_layer->priv->map));
}

/**
 * hyscan_gtk_map_nav_set_hdg_len:
 * @nav_layer: указатель на #HyScanGtkMapNav
 * @meters: длина линии истинного курса
 *
 * Устанавливает длину линии истинного курса.
 */
void
hyscan_gtk_map_nav_set_hdg_len  (HyScanGtkMapNav *nav_layer,
                                 gdouble          meters)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_NAV (nav_layer));

  nav_layer->priv->hdg_line_length = meters;
  gtk_widget_queue_draw (GTK_WIDGET (nav_layer->priv->map));
}
