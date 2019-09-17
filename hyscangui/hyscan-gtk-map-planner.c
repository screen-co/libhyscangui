/* hyscan-gtk-map-planner.c
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
 * SECTION: hyscan-gtk-map-planner
 * @Short_description: Слой для планирования схемы галсов.
 * @Title: HyScanGtkMapPlanner
 * @See_also: #HyScanGtkLayer, #HyScanGtkMape
 *
 * Данный слой позволяет отображать и редактировать схему плановых галсов. Модель
 * плановых галсов передаётся при создании слоя функцией:
 * - hyscan_gtk_map_planner_new()
 *
 * Слой имеет несколько режимов работы, которые определяют, как слой реагирует на
 * нажатие кнопки мыши:
 *
 * - создание перимертра полигона,
 * - создание плановых галсов,
 * - выделение объектов,
 * - создание параллельных галсов.
 *
 * Установить режим работы можно при помощи функции hyscan_gtk_map_planner_set_mode().
 *
 */

#include "hyscan-gtk-map-planner.h"
#include "hyscan-gtk-map.h"
#include "hyscan-list-store.h"
#include "hyscan-cairo.h"
#include <hyscan-planner.h>
#include <math.h>
#include <hyscan-cartesian.h>

#define HANDLE_TYPE_NAME             "map_planner"
#define HANDLE_RADIUS                3.0
#define HANDLE_HOVER_RADIUS          8.0
#define AXIS_LENGTH                  50.0
#define AXIS_ARROW_LENGTH            5.0
#define MAX_PARALLEL_TRACKS          100

#define DEFAULT_ZONE_WIDTH           0.5                    /* Толщина периметра полигона. */
#define DEFAULT_ZONE_COLOR           "rgba(0,50,0,0.8)"     /* Цвет периметра полигона. */
#define DEFAULT_TRACK_WIDTH          1.0                    /* Толщина галсов. */
#define DEFAULT_TRACK_COLOR          "rgba(0,128,0,1.0)"    /* Цвет галсов. */
#define DEFAULT_ORIGIN_WIDTH         1.0                    /* Толщина галсов. */
#define DEFAULT_ORIGIN_COLOR         "rgba(80,120,180,0.5)" /* Цвет осей координат. */
#define DEFAULT_TRACK_SELECTED_COLOR "rgba(128,0,128,1.0)"  /* Цвет выделенных галсов. */

#define IS_INSIDE(x, a, b) ((a) < (b) ? (a) < (x) && (x) < (b) : (b) < (x) && (x) < (a))

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
  PROP_PARALLEL_DISTANCE,
  PROP_PARALLEL_ALTER,
};

enum
{
  SIGNAL_TRACK_CHANGED,
  SIGNAL_LAST
};

typedef enum
{
  STATE_NONE,              /* Режим просмотра. */
  STATE_DRAG_START,        /* Режим редактирования начала галса. */
  STATE_DRAG_MIDDLE,       /* Режим редактирования за середину галса. */
  STATE_DRAG_END,          /* Режим редактирования конца галса. */
  STATE_ZONE_CREATE,       /* Режим создания новой зоны. */
  STATE_ZONE_DRAG,         /* Режим перемещения вершины зоны. */
  STATE_ZONE_INSERT,       /* Режим вставки вершины зоны. */
  STATE_ORIGIN_CREATE,     /* Режим редактирования точки отсчёта. */
  STATE_SELECTION,         /* Режим выбора объектов. */
} HyScanGtkMapPlannerState;

typedef struct
{
  gchar                *id;       /* Идентификатор галса. */
  HyScanPlannerTrack   *object;   /* Структура с исходными данными галса. */
  HyScanGeoCartesian2D  start;    /* Проекция начала галса на карту. */
  HyScanGeoCartesian2D  end;      /* Проекция конца галса на карту. */
} HyScanGtkMapPlannerTrack;

typedef struct
{
  gchar                *id;       /* Идентификатор галса. */
  HyScanPlannerOrigin  *object;   /* Структура с исходными данными точки отсчёта. */
  HyScanGeoCartesian2D  origin;   /* Проекция точки начала отсчёта на карту. */
} HyScanGtkMapPlannerOrigin;

typedef struct
{
  gchar                *id;         /* Идентификатор зоны. */
  HyScanPlannerZone    *object;     /* Структура с исходными данными зоны полигона. */
  GList                *points;     /* Проекция вершин зоны на карту. */
  GList                *tracks;     /* Список галсов HyScanGtkMapPlannerTrack, лежащих внутри этой зоны. */
} HyScanGtkMapPlannerZone;

typedef struct
{
  gdouble  line_width;              /* Толщина линии. */
  GdkRGBA  color;                   /* Цвет линии. */
} HyScanGtkMapPlannerStyle;

struct _HyScanGtkMapPlannerPrivate
{
  HyScanGtkMap                      *map;                  /* Виджет карты. */
  HyScanPlannerModel                *model;                /* Модель объектов планировщика. */
  HyScanListStore                   *selection;            /* Список ИД выбранных объектов. */
  gboolean                           visible;              /* Признак видимости слоя. */
  GHashTable                        *zones;                /* Таблица зон полигона. */
  GList                             *tracks;               /* Список галсов, которые не связаны ни с одной из зон. */
  HyScanGtkMapPlannerOrigin         *origin;               /* Точка начала отсчёта. */

  HyScanGtkMapPlannerMode            mode;                 /* Режим добавления новых элементов на слой. */
  HyScanGtkMapPlannerState           cur_state;            /* Текущий состояние слоя, показывает что редактируется. */
  HyScanGtkMapPlannerOrigin         *cur_origin;           /* Копия начала отсчёта, редактируется в данный момент. */
  HyScanGtkMapPlannerTrack          *cur_track;            /* Копия галса, который редактируется в данный момент. */
  HyScanGtkMapPlannerZone           *cur_zone;             /* Копия зоны, которая редактируется в данный момент. */
  GList                             *cur_vertex;           /* Указатель на редактируемую вершину. */

  gchar                             *hover_track_id;       /* Идентификатор подсвечиваемого галса. */
  gchar                             *hover_zone_id;        /* Идентификатор подсвечиваемой зоны. */
  gint                               hover_vertex;         /* Номер вершины под курсором. */
  HyScanGeoCartesian2D               hover_edge_point;     /* Координаты точки на ребре под указателем мыши. */
  HyScanGtkMapPlannerState           hover_state;          /* Состояние, которое будет установлено при выборе хэндла. */

  const HyScanGtkMapPlannerTrack    *found_track;          /* Указатель на галс под курсором мыши. */
  const HyScanGtkMapPlannerZone     *found_zone;           /* Указатель на зону, чья вершина под курсором мыши. */
  gint                               found_vertex;         /* Номер вершины под курсором мыши. */
  HyScanGtkMapPlannerState           found_state;          /* Состояние, которое будет установлено при выборе хэндла. */

  gdouble                            parallel_distance;    /* Расстояние между параллельными галсами. */
  gboolean                           parallel_alternate;   /* Признак смены направления паралельных галсов. */
  GList                             *parallel_tracks;      /* Список параллельных галсов для добавления. */

  HyScanGeoCartesian2D               selection_start;      /* Координаты одного угла выбранной области. */
  HyScanGeoCartesian2D               selection_end;        /* Координаты второго угла выбранной области. */
  GHashTable                        *selection_keep;       /* Таблица с галсами, которые надо оставить выбранными. */

  PangoLayout                       *pango_layout;         /* Шрифт. */
  HyScanGtkMapPlannerStyle           track_style;          /* Стиль оформления галса. */
  HyScanGtkMapPlannerStyle           track_style_selected; /* Стиль оформления выбранного галса. */
  HyScanGtkMapPlannerStyle           zone_style;           /* Стиль оформления периметра полигона. */
  HyScanGtkMapPlannerStyle           origin_style;         /* Стиль оформления начала координат. */
};

static void                       hyscan_gtk_map_planner_interface_init        (HyScanGtkLayerInterface   *iface);
static void                       hyscan_gtk_map_planner_set_property          (GObject                   *object,
                                                                                guint                      prop_id,
                                                                                const GValue              *value,
                                                                                GParamSpec                *pspec);
static void                       hyscan_gtk_map_planner_get_property          (GObject                   *object,
                                                                                guint                      prop_id,
                                                                                GValue                    *value,
                                                                                GParamSpec                *pspec);
static void                       hyscan_gtk_map_planner_object_constructed    (GObject                   *object);
static void                       hyscan_gtk_map_planner_object_finalize       (GObject                   *object);
static void                       hyscan_gtk_map_planner_model_changed         (HyScanGtkMapPlanner       *planner);
static HyScanGtkMapPlannerZone *  hyscan_gtk_map_planner_zone_create           (void);
static HyScanGtkMapPlannerZone *  hyscan_gtk_map_planner_zone_copy             (const HyScanGtkMapPlannerZone
                                                                                                          *zone);
static void                       hyscan_gtk_map_planner_zone_free             (HyScanGtkMapPlannerZone   *zone);
static void                       hyscan_gtk_map_planner_track_free            (HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_origin_free           (HyScanGtkMapPlannerOrigin *origin);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_create          (void);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_copy            (const HyScanGtkMapPlannerTrack
                                                                                                          *track);
static HyScanGtkMapPlannerOrigin *hyscan_gtk_map_planner_origin_create         (void);
static void                       hyscan_gtk_map_planner_track_project         (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_zone_set_object       (HyScanGtkMapPlannerZone   *zone,
                                                                                HyScanPlannerZone         *object);
static void                       hyscan_gtk_map_planner_zone_project          (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone);
static void                       hyscan_gtk_map_planner_origin_project        (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerOrigin *origin);
static void                       hyscan_gtk_map_planner_track_remove          (HyScanGtkMapPlanner       *planner,
                                                                                const gchar               *track_id);
static void                       hyscan_gtk_map_planner_set_cur_track         (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_selection_remove      (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_vertex_remove         (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_zone_save             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone);
static void                       hyscan_gtk_map_planner_draw                  (GtkCifroArea              *carea,
                                                                                cairo_t                   *cairo,
                                                                                HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_zone_draw             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone,
                                                                                gboolean                   skip_current,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_track_draw            (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track,
                                                                                gboolean                   skip_current,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_selection_draw        (HyScanGtkMapPlanner       *planner,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_origin_draw           (HyScanGtkMapPlanner       *planner,
                                                                                cairo_t                   *cairo);
static gboolean                   hyscan_gtk_map_planner_middle_drag           (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static gboolean                   hyscan_gtk_map_planner_edge_drag             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D      *end_2d,
                                                                                HyScanGeoCartesian2D      *start_2d,
                                                                                HyScanGeoGeodetic         *end_geo,
                                                                                GdkEventMotion            *event);
static gboolean                   hyscan_gtk_map_planner_vertex_drag           (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static gboolean                   hyscan_gtk_map_planner_origin_drag           (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static gboolean                   hyscan_gtk_map_planner_motion_notify         (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static void                       hyscan_gtk_map_planner_projection            (HyScanGtkMapPlanner       *planner,
                                                                                GParamSpec                *pspec);
static gboolean                   hyscan_gtk_map_planner_configure             (HyScanGtkMapPlanner       *planner,
                                                                                GdkEvent                  *screen);
static gboolean                   hyscan_gtk_map_planner_key_press             (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventKey               *event);
static void                       hyscan_gtk_map_planner_handle_click_select   (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventButton            *event);
static gboolean                   hyscan_gtk_map_planner_handle_show_track     (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_show_zone      (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_find_track     (HyScanGtkMapPlanner       *planner,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGeoCartesian2D      *handle_coord);
static gboolean                   hyscan_gtk_map_planner_handle_find_zone      (HyScanGtkMapPlanner       *planner,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGeoCartesian2D      *handle_coord);
static gboolean                   hyscan_gtk_map_planner_release_zone          (HyScanGtkMapPlanner       *planner,
                                                                                gboolean                   stop_secondary,
                                                                                GdkEventButton            *event);
static gboolean                   hyscan_gtk_map_planner_release_track         (HyScanGtkMapPlanner       *planner);
static gboolean                   hyscan_gtk_map_planner_handle_create_zone    (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D       point);
static gboolean                   hyscan_gtk_map_planner_handle_create_track   (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D       point);
static void                       hyscan_gtk_map_planner_cur_zone_insert_drag  (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkLayerHandle      *handle);
static inline gboolean            hyscan_gtk_map_planner_is_state_track        (HyScanGtkMapPlannerState   state);
static inline gboolean            hyscan_gtk_map_planner_is_state_vertex       (HyScanGtkMapPlannerState   state);
static HyScanPlannerTrack *       hyscan_gtk_map_planner_track_object_new      (void);
static void                       hyscan_gtk_map_planner_save_parallel         (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_save_current          (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_cancel                (HyScanGtkMapPlanner       *planner);
static gboolean                   hyscan_gtk_map_planner_accept_btn            (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventButton            *event);

static guint hyscan_gtk_map_planner_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPlanner, hyscan_gtk_map_planner, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapPlanner)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_planner_interface_init))

static void
hyscan_gtk_map_planner_class_init (HyScanGtkMapPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_planner_set_property;
  object_class->get_property = hyscan_gtk_map_planner_get_property;
  object_class->constructed = hyscan_gtk_map_planner_object_constructed;
  object_class->finalize = hyscan_gtk_map_planner_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Planner model", HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanListStore", "List of selected ids", HYSCAN_TYPE_LIST_STORE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PARALLEL_DISTANCE,
    g_param_spec_double ("parallel-distance", "Parallel distance", "Distance between two parallel tracks",
                         0, G_MAXDOUBLE, 30.0,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_PARALLEL_ALTER,
    g_param_spec_boolean ("parallel-alternate", "Parallel alternate", "Alternate parallel tracks",
                          TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * HyScanGtkMapPlanner::track-changed:
   * @planner: указатель на #HyScanGtkMapPlanner
   * @track: (nullable): указатель на галс #HyScanPlannerTrack, который в данный момент редактируется
   *
   * Сигнал посылается при изменении редактируемого галса.
   */
  hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED] =
    g_signal_new ("track-changed", HYSCAN_TYPE_GTK_MAP_PLANNER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
hyscan_gtk_map_planner_init (HyScanGtkMapPlanner *gtk_map_planner)
{
  gtk_map_planner->priv = hyscan_gtk_map_planner_get_instance_private (gtk_map_planner);
}

static void
hyscan_gtk_map_planner_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    case PROP_PARALLEL_DISTANCE:
      priv->parallel_distance = g_value_get_double (value);
      break;

    case PROP_PARALLEL_ALTER:
      priv->parallel_alternate = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_planner_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  switch (prop_id)
    {
    case PROP_PARALLEL_DISTANCE:
      g_value_set_double (value, priv->parallel_distance);
      break;

    case PROP_PARALLEL_ALTER:
      g_value_set_boolean (value, priv->parallel_alternate);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_planner_object_constructed (GObject *object)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->constructed (object);

  priv->mode = HYSCAN_GTK_MAP_PLANNER_MODE_TRACK;
  /* Не задаём функцию удаления ключей, т.к. ими владеет сама структура зоны, поле id. */
  priv->zones = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                       (GDestroyNotify) hyscan_gtk_map_planner_zone_free);

  priv->selection_keep = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Стиль оформления. */
  priv->zone_style.line_width = DEFAULT_ZONE_WIDTH;
  gdk_rgba_parse (&priv->zone_style.color, DEFAULT_ZONE_COLOR);
  priv->track_style.line_width = DEFAULT_TRACK_WIDTH;
  gdk_rgba_parse (&priv->track_style.color, DEFAULT_TRACK_COLOR);
  priv->track_style_selected.line_width = DEFAULT_TRACK_WIDTH;
  gdk_rgba_parse (&priv->track_style_selected.color, DEFAULT_TRACK_SELECTED_COLOR);
  priv->origin_style.line_width = DEFAULT_ORIGIN_WIDTH;
  gdk_rgba_parse (&priv->origin_style.color, DEFAULT_ORIGIN_COLOR);

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_map_planner_model_changed), planner);
}

static void
hyscan_gtk_map_planner_object_finalize (GObject *object)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  g_signal_handlers_disconnect_by_data (priv->model, gtk_map_planner);

  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_free (priv->hover_track_id);
  g_free (priv->hover_zone_id);
  g_clear_pointer (&priv->origin, hyscan_gtk_map_planner_origin_free);
  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_list_free_full (priv->parallel_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_hash_table_destroy (priv->zones);
  g_hash_table_destroy (priv->selection_keep);

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->finalize (object);
}

/* Обработчик сигнала "changed" модели.
 * Копирует информацию о схеме плановых галсов во внутренние структуры слоя. */
static void
hyscan_gtk_map_planner_model_changed (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  GHashTable *objects;
  gchar *key;
  HyScanPlannerObject *object;
  gchar *found_track_id, *found_zone_id;   /* Идентификаторы найденных галса и зоны. */
  gint found_vertex;

  /* Получаем список всех объектов планировщика. */
  objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));

  /* Загружаем точку отсчёта. */
  {
    HyScanPlannerOrigin *origin;

    g_clear_pointer (&priv->origin, hyscan_gtk_map_planner_origin_free);

    origin = hyscan_planner_model_get_origin (priv->model);
    if (origin != NULL)
      {
        priv->origin = hyscan_gtk_map_planner_origin_create ();
        priv->origin->object = origin;
        hyscan_gtk_map_planner_origin_project (planner, priv->origin);
      }
  }

  /* Загружаем зоны. */
  found_zone_id = priv->found_zone != NULL ? g_strdup (priv->found_zone->id) : NULL;
  found_vertex = priv->found_vertex;
  priv->found_zone = NULL;
  priv->found_vertex = 0;

  g_hash_table_remove_all (priv->zones);
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanGtkMapPlannerZone *zone;

      if (object->type != HYSCAN_PLANNER_ZONE)
        continue;

      /* Воруем ключ и объект из одной хэш-таблицы и добавляем их в другую. */
      g_hash_table_iter_steal (&iter);
      zone = hyscan_gtk_map_planner_zone_create ();
      zone->id = key;
      hyscan_gtk_map_planner_zone_set_object (zone, &object->zone);
      hyscan_gtk_map_planner_zone_project (planner, zone);
      g_hash_table_insert (priv->zones, key, zone);

      /* Восстанавливаем указатель на интересную зону. */
      if (g_strcmp0 (found_zone_id, zone->id) == 0)
        {
          priv->found_zone = zone;
          priv->found_vertex = found_vertex;
        }
    }

  /* Загружаем плановые галсы по каждой зоне. */
  found_track_id = priv->found_track != NULL ? g_strdup (priv->found_track->id) : NULL;
  priv->found_track = NULL;

  g_list_free_full (priv->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->tracks = NULL;
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanGtkMapPlannerTrack *track;
      HyScanGtkMapPlannerZone *zone = NULL;
      GList **tracks;

      if (object->type != HYSCAN_PLANNER_TRACK)
        continue;

      /* Находим зону, из которой этот галс. */
      if (object->track.zone_id != NULL)
        zone = g_hash_table_lookup (priv->zones, object->track.zone_id);

      /* Если зона не найдена, то добавляем галс в общий список. */
      if (zone == NULL)
        tracks = &priv->tracks;
      else
        tracks = &zone->tracks;

      /* Воруем ключ и объект из хэш-таблицы - ключ удаляем, а объект помещаем в зону. */
      g_hash_table_iter_steal (&iter);
      track = hyscan_gtk_map_planner_track_create ();
      track->id = key;
      track->object = &object->track;
      hyscan_gtk_map_planner_track_project (planner, track);
      *tracks = g_list_append (*tracks, track);

      /* Восстанавливаем указатель на интересный галс. */
      if (g_strcmp0 (found_track_id, track->id))
        priv->found_track = track;
    }

  g_hash_table_unref (objects);
  g_free (found_track_id);
  g_free (found_zone_id);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_track_free (HyScanGtkMapPlannerTrack *track)
{
  hyscan_planner_track_free (track->object);
  g_free (track->id);
  g_slice_free (HyScanGtkMapPlannerTrack, track);
}

static void
hyscan_gtk_map_planner_origin_free (HyScanGtkMapPlannerOrigin *origin)
{
  hyscan_planner_origin_free (origin->object);
  g_free (origin->id);
  g_slice_free (HyScanGtkMapPlannerOrigin, origin);
}

static void
hyscan_gtk_map_planner_zone_free (HyScanGtkMapPlannerZone *zone)
{
  if (zone == NULL)
    return;

  g_free (zone->id);
  g_list_free_full (zone->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_list_free_full (zone->tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  hyscan_planner_zone_free (zone->object);
  g_slice_free (HyScanGtkMapPlannerZone, zone);
}

static HyScanGtkMapPlannerZone *
hyscan_gtk_map_planner_zone_create (void)
{
  return g_slice_new0 (HyScanGtkMapPlannerZone);
}

static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_track_create (void)
{
  return g_slice_new0 (HyScanGtkMapPlannerTrack);
}

static HyScanGtkMapPlannerOrigin *
hyscan_gtk_map_planner_origin_create (void)
{
  return g_slice_new0 (HyScanGtkMapPlannerOrigin);
}

static HyScanGtkMapPlannerTrack *
hyscan_gtk_map_planner_track_copy (const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerTrack *copy;

  if (track == NULL)
    return NULL;

  copy = hyscan_gtk_map_planner_track_create ();
  copy->id = g_strdup (track->id);
  copy->object = hyscan_planner_track_copy (track->object);
  copy->start = track->start;
  copy->end = track->end;

  return copy;
}

static HyScanGtkMapPlannerZone *
hyscan_gtk_map_planner_zone_copy (const HyScanGtkMapPlannerZone *zone)
{
  HyScanGtkMapPlannerZone *copy;
  GList *link;

  if (zone == NULL)
    return NULL;

  copy = hyscan_gtk_map_planner_zone_create ();
  copy->id = g_strdup (zone->id);
  copy->object = hyscan_planner_zone_copy (zone->object);
  for (link = zone->points; link != NULL; link = link->next)
    {
      HyScanGtkMapPoint *point = link->data;

      copy->points = g_list_append (copy->points, hyscan_gtk_map_point_copy (point));
    }

  /* todo: move copy->tracks out of zone -- keep them into the common track list. */

  return copy;
}

/* Проецирует плановый галс на карту. */
static void
hyscan_gtk_map_planner_track_project (HyScanGtkMapPlanner       *planner,
                                      HyScanGtkMapPlannerTrack  *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->map == NULL || track == NULL)
    return;

  hyscan_gtk_map_geo_to_value (priv->map, track->object->start, &track->start);
  hyscan_gtk_map_geo_to_value (priv->map, track->object->end, &track->end);
}

/* Проецирует зону полигона на карту. */
static void
hyscan_gtk_map_planner_zone_project (HyScanGtkMapPlanner     *planner,
                                     HyScanGtkMapPlannerZone *zone)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *link;

  if (priv->map == NULL)
    return;

  for (link = zone->points; link != NULL; link = link->next)
    {
      HyScanGtkMapPoint *point = link->data;

      hyscan_gtk_map_geo_to_value (priv->map, point->geo, &point->c2d);
    }
}

/* Проецирует начало отсчёта на карту. */
static void
hyscan_gtk_map_planner_origin_project (HyScanGtkMapPlanner       *planner,
                                       HyScanGtkMapPlannerOrigin *origin)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->map == NULL)
    return;

  hyscan_gtk_map_geo_to_value (priv->map, origin->object->origin, &origin->origin);
}

static void
hyscan_gtk_map_planner_zone_set_object (HyScanGtkMapPlannerZone *zone,
                                        HyScanPlannerZone       *object)
{
  gsize i;

  zone->object = object;
  g_list_free_full (zone->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  zone->points = NULL;

  for (i = 0; i < object->points_len; ++i)
    {
      HyScanGtkMapPoint point;

      point.geo = zone->object->points[i];
      zone->points = g_list_append (zone->points, hyscan_gtk_map_point_copy (&point));
    }
}

/* Обработчик сигнала смены проекции. */
static void
hyscan_gtk_map_planner_projection (HyScanGtkMapPlanner *planner,
                                   GParamSpec          *pspec)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  GList *link;
  HyScanGtkMapPlannerZone *zone;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &zone))
    {
      hyscan_gtk_map_planner_zone_project (planner, zone);

      for (link = zone->tracks; link != NULL; link = link->next)
        hyscan_gtk_map_planner_track_project (planner, link->data);
    }

  for (link = priv->tracks; link != NULL; link = link->next)
    hyscan_gtk_map_planner_track_project (planner, link->data);

  if (priv->origin != NULL)
    hyscan_gtk_map_planner_origin_project (planner, priv->origin);
}

/* Обработчик события "visible-draw".
 * Рисует слой планировщика. */
static void
hyscan_gtk_map_planner_draw (GtkCifroArea        *carea,
                             cairo_t             *cairo,
                             HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  gchar *key;
  HyScanGtkMapPlannerZone *zone;
  GList *list;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return;

  /* Рисуем начало отсчёта и направление осей координат. */
  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN)
    hyscan_gtk_map_planner_origin_draw (planner, cairo);

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    hyscan_gtk_map_planner_zone_draw (planner, zone, TRUE, cairo);

  /* Рисуем текущую редактируемую зону. */
  if (priv->cur_zone != NULL)
    hyscan_gtk_map_planner_zone_draw (planner, priv->cur_zone, FALSE, cairo);

  for (list = priv->tracks; list != NULL; list = list->next)
    hyscan_gtk_map_planner_track_draw (planner, list->data, TRUE, cairo);

  /* Параллельные галсы в режиме создания параллельных галсов. */
  for (list = priv->parallel_tracks; list != NULL; list = list->next)
    hyscan_gtk_map_planner_track_draw (planner, list->data, TRUE, cairo);

  /* Рисуем текущий редактируемый галс. */
  if (priv->cur_track != NULL)
    hyscan_gtk_map_planner_track_draw (planner, priv->cur_track, FALSE, cairo);

  /* Выделение. */
  if (priv->cur_state == STATE_SELECTION)
    hyscan_gtk_map_planner_selection_draw (planner, cairo);
}

/* Редактирование зоны при захвате за вершину. */
static gboolean
hyscan_gtk_map_planner_vertex_drag (HyScanGtkMapPlanner *planner,
                                    GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D cursor;
  HyScanGtkMapPlannerZone *cur_zone = priv->cur_zone;
  GList *link;
  HyScanGtkMapPoint *point;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  link = priv->cur_vertex == NULL ? g_list_last (cur_zone->points) : priv->cur_vertex;
  point = link->data;

  point->c2d = cursor;
  hyscan_gtk_map_value_to_geo (priv->map, &point->geo, point->c2d);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

/* Редактирование точки отсчёта при захвате за вершину. */
static gboolean
hyscan_gtk_map_planner_origin_drag (HyScanGtkMapPlanner *planner,
                                    GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D cursor;
  HyScanGtkMapPlannerOrigin *cur_origin = priv->cur_origin;
  gdouble angle;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  angle = G_PI_2 - atan2 (cursor.y - cur_origin->origin.y, cursor.x - cur_origin->origin.x);
  cur_origin->object->origin.h = angle / G_PI * 180.0;
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

/* Изменение списка выбранных галсов при изменении области выделения. */
static gboolean
hyscan_gtk_map_planner_selection_drag (HyScanGtkMapPlanner *planner,
                                       GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D *end = &priv->selection_end;
  HyScanGtkMapPlannerTrack *track;
  GList *link;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &end->x, &end->y);

  for (link = priv->tracks; link != NULL; link = link->next)
    {
      gboolean selected_before, selected;
      track = link->data;

      if (g_hash_table_contains (priv->selection_keep, track->id))
        continue;

      selected_before = hyscan_list_store_contains (priv->selection, track->id);
      selected = hyscan_cartesian_is_inside (&track->start, &track->end, &priv->selection_start, &priv->selection_end);

      if (selected && !selected_before)
        hyscan_list_store_append (priv->selection, track->id);
      else if (!selected && selected_before)
        hyscan_list_store_remove (priv->selection, track->id);
    }

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

/* Редактирование галса при захвате за центр. */
static gboolean
hyscan_gtk_map_planner_middle_drag (HyScanGtkMapPlanner *planner,
                                    GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D cursor;
  gdouble dx, dy;
  HyScanGtkMapPlannerTrack *cur_track = priv->cur_track;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  /* Сохраняем длину, меняем толко направление. */
  if (event->state & GDK_CONTROL_MASK)
    {
      gdouble angle;
      gdouble length;
      HyScanGeoCartesian2D middle;

      angle = atan2 (cursor.y - cur_track->start.y, cursor.x - cur_track->start.x);
      length = hypot (cur_track->end.y - cur_track->start.y, cur_track->end.x - cur_track->start.x) / 2.0;

      middle.x = (cur_track->end.x + cur_track->start.x) / 2.0;
      middle.y = (cur_track->end.y + cur_track->start.y) / 2.0;

      cur_track->start.x = middle.x - length * cos (angle);
      cur_track->start.y = middle.y - length * sin (angle);
      cur_track->end.x = middle.x + length * cos (angle);
      cur_track->end.y = middle.y + length * sin (angle);
    }

  /* Перемещаем центр галса в новую точку. */
  else
    {
      dx = (priv->cur_track->end.x - priv->cur_track->start.x) / 2.0;
      dy = (priv->cur_track->end.y - priv->cur_track->start.y) / 2.0;

      priv->cur_track->start.x = cursor.x - dx;
      priv->cur_track->start.y = cursor.y - dy;
      priv->cur_track->end.x = cursor.x + dx;
      priv->cur_track->end.y = cursor.y + dy;
    }

  hyscan_gtk_map_value_to_geo (priv->map, &priv->cur_track->object->start, priv->cur_track->start);
  hyscan_gtk_map_value_to_geo (priv->map, &priv->cur_track->object->end, priv->cur_track->end);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_signal_emit (planner, hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED], 0, priv->cur_track->object);

  return GDK_EVENT_STOP;
}

/* Редактирование галса при захвате за край. */
static gboolean
hyscan_gtk_map_planner_edge_drag (HyScanGtkMapPlanner  *planner,
                                  HyScanGeoCartesian2D *end_2d,
                                  HyScanGeoCartesian2D *start_2d,
                                  HyScanGeoGeodetic    *end_geo,
                                  GdkEventMotion       *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D cursor;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  /* Сохраняем длину, меняем толко направление. */
  if (event->state & GDK_CONTROL_MASK)
    {
      gdouble angle;
      gdouble length;

      angle = atan2 (cursor.y - start_2d->y, cursor.x - start_2d->x);
      length = hypot (end_2d->y - start_2d->y, end_2d->x - start_2d->x);
      end_2d->x = start_2d->x + length * cos (angle);
      end_2d->y = start_2d->y + length * sin (angle);
    }

  /* Сохраняем направление, меняем только длину. */
  else if (event->state & GDK_SHIFT_MASK)
    {
      gdouble angle;
      gdouble cursor_angle;
      gdouble d_angle;
      gdouble length;
      gdouble dx, dy, cursor_dx, cursor_dy;

      dy = end_2d->y - start_2d->y;
      dx = end_2d->x - start_2d->x;
      cursor_dy = cursor.y - start_2d->y;
      cursor_dx = cursor.x - start_2d->x;
      angle = atan2 (dy, dx);
      cursor_angle = atan2 (cursor_dy, cursor_dx);
      length = hypot (cursor_dx, cursor_dy);

      /* Проверяем, не сменилось ли направление. */
      d_angle = ABS (cursor_angle - angle);
      if (d_angle > G_PI_2 && d_angle < G_PI + G_PI_2)
        length = -length;

      end_2d->x = start_2d->x + length * cos (angle);
      end_2d->y = start_2d->y + length * sin (angle);
    }

  /* Свободно перемещаем. */
  else
    {
      *end_2d = cursor;
    }

  /* Переносим конец галса в новую точку. */
  hyscan_gtk_map_value_to_geo (priv->map, end_geo, *end_2d);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_signal_emit (planner, hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED], 0, priv->cur_track->object);

  return GDK_EVENT_STOP;
}

static gboolean
hyscan_gtk_map_planner_parallel_create (HyScanGtkMapPlanner *planner,
                                        GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *cur_track = priv->cur_track;

  HyScanGeoCartesian2D cursor;
  HyScanGeoCartesian2D normal;

  gdouble scale, distance;
  gdouble step;
  gint i, tracks_num;

  g_list_free_full (priv->parallel_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->parallel_tracks = NULL;

  scale = hyscan_gtk_map_get_scale_value (priv->map, cur_track->object->start);
  step = priv->parallel_distance / scale;

  if (step == 0.0)
    return GDK_EVENT_STOP;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);


  distance = hyscan_cartesian_distance_to_line (&cur_track->start,
                                                &cur_track->end,
                                                &cursor, NULL);

  hyscan_cartesian_normal (&cur_track->start, &cur_track->end, &normal);
  /* Меняем направление нормали так, чтобы она была по направлению к курсору. */
  {
    gdouble cursor_side, normal_side;

    cursor_side = (cursor.x - cur_track->start.x) * (cur_track->end.y - cur_track->start.y) -
                  (cursor.y - cur_track->start.y) * (cur_track->end.x - cur_track->start.x);
    normal_side = normal.x * (cur_track->end.y - cur_track->start.y) -
                  normal.y * (cur_track->end.x - cur_track->start.x);

    if (normal_side * cursor_side < 0)
      {
        normal.x = - normal.x;
        normal.y = - normal.y;
      }
  }

  tracks_num = MIN (MAX_PARALLEL_TRACKS, distance / step);
  for (i = 1; i <= tracks_num; ++i)
    {
      HyScanGtkMapPlannerTrack *track;
      HyScanGeoCartesian2D *start, *end;
      gdouble invert;

      invert = priv->parallel_alternate && (i % 2 == 1);
      start = invert ? &cur_track->end : &cur_track->start;
      end = invert ? &cur_track->start : &cur_track->end;

      track = hyscan_gtk_map_planner_track_create ();
      track->start.x = start->x + normal.x * step * i;
      track->start.y = start->y + normal.y * step * i;
      track->end.x = end->x + normal.x * step * i;
      track->end.y = end->y + normal.y * step * i;
      track->object = hyscan_gtk_map_planner_track_object_new ();
      hyscan_gtk_map_value_to_geo (priv->map, &track->object->start, track->start);
      hyscan_gtk_map_value_to_geo (priv->map, &track->object->end, track->end);

      priv->parallel_tracks = g_list_append (priv->parallel_tracks, track);
    }

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

/* Обработка события "motion-notify".
 * Перемещает галс в новую точку. */
static gboolean
hyscan_gtk_map_planner_motion_notify (HyScanGtkMapPlanner *planner,
                                      GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track = priv->cur_track;

  /* Режим создания параллельных галсов. */
  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_TRACK_PARALLEL &&
      hyscan_gtk_map_planner_is_state_track (priv->cur_state))
    {
      return hyscan_gtk_map_planner_parallel_create (planner, event);
    }

  switch (priv->cur_state)
  {
    case STATE_DRAG_START:
      return hyscan_gtk_map_planner_edge_drag (planner, &track->start, &track->end, &track->object->start, event);

    case STATE_DRAG_END:
      return hyscan_gtk_map_planner_edge_drag (planner, &track->end, &track->start, &track->object->end, event);

    case STATE_DRAG_MIDDLE:
      return hyscan_gtk_map_planner_middle_drag (planner, event);

    case STATE_ZONE_CREATE:
    case STATE_ZONE_DRAG:
      return hyscan_gtk_map_planner_vertex_drag (planner, event);

    case STATE_ORIGIN_CREATE:
      return hyscan_gtk_map_planner_origin_drag (planner, event);

    case STATE_SELECTION:
      return hyscan_gtk_map_planner_selection_drag (planner, event);

    default:
      return GDK_EVENT_PROPAGATE;
  }
}

static void
hyscan_gtk_map_planner_track_remove (HyScanGtkMapPlanner *planner,
                                     const gchar         *track_id)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *link;

  /* Удаляем из модели. */
  hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (priv->model), track_id);

  /* Удаляем из внутреннего списка. */
  for (link = priv->tracks; link != NULL; link = link->next)
    {
      HyScanGtkMapPlannerTrack *track = link->data;

      if (g_strcmp0 (track->id, track_id) != 0)
        continue;

      priv->tracks = g_list_remove_link (priv->tracks, link);
      return;
    }
}

static void
hyscan_gtk_map_planner_vertex_remove (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  /* Удаляем из модели. */
  priv->cur_zone->points = g_list_remove_link (priv->cur_zone->points, priv->cur_vertex);
  hyscan_gtk_map_planner_zone_save (planner, priv->cur_zone);
}

/* Возвращае %TRUE, если в указанном состоянии редактируется галс. */
static inline gboolean
hyscan_gtk_map_planner_is_state_track (HyScanGtkMapPlannerState state)
{
  return state == STATE_DRAG_START || state == STATE_DRAG_END || state == STATE_DRAG_MIDDLE;
}

/* Возвращае %TRUE, если в указанном состоянии редактируется вершина периметра зоны. */
static inline gboolean
hyscan_gtk_map_planner_is_state_vertex (HyScanGtkMapPlannerState state)
{
  return state == STATE_ZONE_DRAG;
}

/* Проверяет, будет ли слой реагировать на нажатие кнопки мыши. */
static gboolean
hyscan_gtk_map_planner_accept_btn (HyScanGtkMapPlanner *planner,
                                   GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkLayerContainer *container;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);

  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return FALSE;

  /* Проверяем, что у нас есть право ввода. */
  if (hyscan_gtk_layer_container_get_input_owner (container) != planner)
    return FALSE;

  /* Если слой не видно, то никак не реагируем. */
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return FALSE;

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_configure (HyScanGtkMapPlanner *planner,
                                  GdkEvent            *screen)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_planner_handle_click_select (HyScanGtkMapPlanner *planner,
                                            GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  const gchar *id = NULL;

  if (!(event->state & GDK_SHIFT_MASK))
    hyscan_list_store_remove_all (priv->selection);

  if (hyscan_gtk_map_planner_is_state_track (priv->found_state) && priv->found_track != NULL)
    id = priv->found_track->id;
  else if (hyscan_gtk_map_planner_is_state_vertex (priv->found_state) && priv->found_zone != NULL)
    id = priv->found_zone->id;

  if (id == NULL)
    return;

  if (hyscan_list_store_contains (priv->selection, id))
    hyscan_list_store_remove (priv->selection, id);
  else
    hyscan_list_store_append (priv->selection, id);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_cancel (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  hyscan_gtk_map_planner_set_cur_track (planner, NULL);
  g_clear_pointer (&priv->cur_zone, hyscan_gtk_map_planner_zone_free);
  g_clear_pointer (&priv->cur_origin, hyscan_gtk_map_planner_origin_free);
  g_list_free_full (priv->parallel_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->parallel_tracks = NULL;
  priv->cur_vertex = NULL;
  priv->cur_state = STATE_NONE;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_selection_remove (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gint n_items, i;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->selection));
  for (i = 0; i < n_items; ++i)
    {
      gchar *selected_id;

      selected_id = g_list_model_get_item (G_LIST_MODEL (priv->selection), i);
      hyscan_gtk_map_planner_track_remove (planner, selected_id);

      g_free (selected_id);
    }

  hyscan_list_store_remove_all (priv->selection);
}

/* Обработка события "key-press-event". */
static gboolean
hyscan_gtk_map_planner_key_press (HyScanGtkMapPlanner *planner,
                                  GdkEventKey         *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return GDK_EVENT_PROPAGATE;

  /* Удаляем активные или выбранные объекты. */
  if (event->keyval == GDK_KEY_Delete)
    {
      if (hyscan_gtk_map_planner_is_state_track (priv->cur_state))
        hyscan_gtk_map_planner_track_remove (planner, priv->cur_track->id);
      else if (hyscan_gtk_map_planner_is_state_vertex (priv->cur_state))
        hyscan_gtk_map_planner_vertex_remove (planner);
      else
        hyscan_gtk_map_planner_selection_remove (planner);
    }

  /* Отменяем текущее изменение. */
  if (priv->cur_state != STATE_NONE && (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_Delete))
    hyscan_gtk_map_planner_cancel (planner);

  return GDK_EVENT_PROPAGATE;
}

static inline void
hyscan_gtk_map_planner_cairo_style (cairo_t                  *cairo,
                                    HyScanGtkMapPlannerStyle *style)
{
  cairo_set_line_width (cairo, style->line_width);
  gdk_cairo_set_source_rgba (cairo, &style->color);
}

/* Рисует зону полигона и галсы внутри неё. */
static void
hyscan_gtk_map_planner_origin_draw (HyScanGtkMapPlanner     *planner,
                                    cairo_t                 *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerOrigin *origin;
  gdouble x, y;
  gdouble angle;

  origin = priv->cur_origin != NULL ? priv->cur_origin : priv->origin;
  if (origin == NULL)
    return;

  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, origin->origin.x, origin->origin.y);

  angle = origin->object->origin.h / 180.0 * G_PI;

  cairo_save (cairo);
  gdk_cairo_set_source_rgba (cairo, &priv->origin_style.color);
  cairo_set_line_width (cairo, priv->origin_style.line_width);

  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, angle - G_PI_2);

  cairo_arc (cairo, 0.0, 0.0, HANDLE_RADIUS, -G_PI, G_PI);

  /* Ось Y. */
  cairo_move_to (cairo, 0, -HANDLE_RADIUS);
  cairo_line_to (cairo, 0, -AXIS_LENGTH);
  cairo_line_to (cairo, -AXIS_ARROW_LENGTH, -(AXIS_LENGTH - AXIS_ARROW_LENGTH));
  cairo_move_to (cairo, 0, -AXIS_LENGTH);
  cairo_line_to (cairo, AXIS_ARROW_LENGTH, -(AXIS_LENGTH - AXIS_ARROW_LENGTH));

  cairo_move_to (cairo, 2.0 * AXIS_ARROW_LENGTH, - AXIS_LENGTH);
  pango_layout_set_text (priv->pango_layout, "y", 1);
  pango_cairo_show_layout (cairo, priv->pango_layout);

  /* Ось X. */
  cairo_move_to (cairo, HANDLE_RADIUS, 0);
  cairo_line_to (cairo, AXIS_LENGTH, 0);
  cairo_line_to (cairo, AXIS_LENGTH - AXIS_ARROW_LENGTH, AXIS_ARROW_LENGTH);
  cairo_move_to (cairo, AXIS_LENGTH, 0);
  cairo_line_to (cairo, AXIS_LENGTH - AXIS_ARROW_LENGTH, -AXIS_ARROW_LENGTH);

  cairo_move_to (cairo, AXIS_LENGTH + AXIS_ARROW_LENGTH, 0);
  pango_layout_set_text (priv->pango_layout, "x", 1);
  pango_cairo_show_layout (cairo, priv->pango_layout);

  cairo_stroke (cairo);

  cairo_restore (cairo);
}

static void
hyscan_gtk_map_planner_selection_draw (HyScanGtkMapPlanner *planner,
                                       cairo_t             *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble sx, sy, ex, ey;

  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &sx, &sy, priv->selection_start.x, priv->selection_start.y);
  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &ex, &ey, priv->selection_end.x, priv->selection_end.y);

  cairo_rectangle (cairo, sx, sy, ex - sx, ey - sy);
  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.8, 0.1);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, 0.5);
  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.8, 0.8);
  cairo_stroke (cairo);
}

/* Рисует зону полигона и галсы внутри неё. */
static void
hyscan_gtk_map_planner_zone_draw (HyScanGtkMapPlanner     *planner,
                                  HyScanGtkMapPlannerZone *zone,
                                  gboolean                 skip_current,
                                  cairo_t                 *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *link;
  gint vertex_num = 0;
  gboolean hovered, hovered_vertex;
  HyScanGtkMapPoint *point;

  gint i = 0, j;
  gdouble x[2], y[2], x0 = 0, y0 = 0;

  /* Пропускаем активную зону. */
  if (skip_current && priv->cur_zone != NULL && g_strcmp0 (priv->cur_zone->id, zone->id) == 0)
    return;

  if (zone->points == NULL || zone->points->next == NULL)
    return;

  cairo_save (cairo);

  cairo_new_path (cairo);

  hovered = (g_strcmp0 (priv->hover_zone_id, zone->id) == 0);
  hovered_vertex = hovered && priv->hover_state == STATE_ZONE_DRAG;

  /* Хэндл над ребром зоны для создания новой вершины. */
  if (hovered && priv->hover_state == STATE_ZONE_INSERT)
    {
      gdouble edge_x, edge_y;

      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &edge_x, &edge_y,
                                     priv->hover_edge_point.x, priv->hover_edge_point.y);
      cairo_new_sub_path (cairo);
      cairo_arc (cairo, edge_x, edge_y, HANDLE_HOVER_RADIUS, -G_PI, G_PI);
    }

  for (link = zone->points, j = 0; link != NULL; link = link->next, j++)
    {
      gdouble radius;

      point = link->data;

      i = j % 2;
      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x[i], &y[i], point->c2d.x, point->c2d.y);
      if (j == 0)
        /* Запоминаем координаты первой вершины. */
        {
          x0 = x[i];
          y0 = y[i];
        }

      /* Рисуем вершину. */
      radius = (hovered_vertex && ++vertex_num == priv->hover_vertex) ? HANDLE_HOVER_RADIUS : HANDLE_RADIUS;
      cairo_new_sub_path (cairo);
      cairo_arc (cairo, x[i], y[i], radius, -G_PI, G_PI);

      /* Рисуем ребро к предыдущей вершине. */
      if (j != 0)
        hyscan_cairo_line_to (cairo, x[0], y[0], x[1], y[1]);
    }

  /* Замыкаем периметр - добавляем ребро от последней вершины к первой. */
  hyscan_cairo_line_to (cairo, x[i], y[i], x0, y0);
  hyscan_gtk_map_planner_cairo_style (cairo, &priv->zone_style);
  cairo_stroke (cairo);

  cairo_restore (cairo);

  for (link = zone->tracks; link != NULL; link = link->next)
    hyscan_gtk_map_planner_track_draw (planner, (HyScanGtkMapPlannerTrack *) link->data, TRUE, cairo);
}

/* Рисует галс. */
static void
hyscan_gtk_map_planner_track_draw (HyScanGtkMapPlanner      *planner,
                                   HyScanGtkMapPlannerTrack *track,
                                   gboolean                  skip_current,
                                   cairo_t                  *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble start_x, start_y, end_x, end_y;
  gdouble start_size, end_size, mid_size;

  /* Пропускаем активный галс. */
  if (skip_current && priv->cur_track != NULL && g_strcmp0 (priv->cur_track->id, track->id) == 0)
    return;

  if (hyscan_list_store_contains (priv->selection, track->id))
    hyscan_gtk_map_planner_cairo_style (cairo, &priv->track_style_selected);
  else
    hyscan_gtk_map_planner_cairo_style (cairo, &priv->track_style);

  /* Определяем размеры хэндлов. */
  start_size = mid_size = end_size = HANDLE_RADIUS;
  if (priv->hover_track_id != NULL && g_strcmp0 (priv->hover_track_id, track->id) == 0)
    {
      if (priv->hover_state == STATE_DRAG_START)
        start_size = HANDLE_HOVER_RADIUS;
      else if (priv->hover_state == STATE_DRAG_MIDDLE)
        mid_size = HANDLE_HOVER_RADIUS;
      else if (priv->hover_state == STATE_DRAG_END)
        end_size = HANDLE_HOVER_RADIUS;
    }

  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &start_x, &start_y, track->start.x, track->start.y);
  gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &end_x, &end_y, track->end.x, track->end.y);

  /* Точка в начале галса. */
  cairo_arc (cairo, start_x, start_y, start_size, -G_PI, G_PI);
  cairo_fill (cairo);

  /* Точка в середние галса. */
  cairo_arc (cairo, (start_x + end_x) / 2.0, (start_y + end_y) / 2.0, mid_size, -G_PI, G_PI);
  cairo_stroke (cairo);

  /* Стрелка в конце галса. */
  cairo_save (cairo);
  cairo_translate (cairo, end_x, end_y);
  cairo_rotate (cairo, atan2 (start_y - end_y, start_x - end_x) - G_PI_2);
  cairo_move_to (cairo, 0, 0);
  cairo_line_to (cairo,  end_size, 2 * end_size);
  cairo_line_to (cairo, -end_size, 2 * end_size);
  cairo_fill (cairo);
  cairo_restore (cairo);

  hyscan_cairo_line_to (cairo, start_x, start_y, end_x, end_y);
  cairo_stroke (cairo);
}

/* Находит хэндл галса в точке x, y. */
static gboolean
hyscan_gtk_map_planner_handle_find_track (HyScanGtkMapPlanner  *planner,
                                          gdouble               x,
                                          gdouble               y,
                                          HyScanGeoCartesian2D *handle_coord)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GList *list;
  gdouble scale_x, scale_y;
  gdouble max_x, max_y;

  HyScanGtkMapPlannerTrack *track = NULL;
  gdouble handle_x = 0.0, handle_y = 0.0;
  gboolean handle_found = FALSE;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, &scale_y);
  max_x = HANDLE_HOVER_RADIUS * scale_x;
  max_y = HANDLE_HOVER_RADIUS * scale_y;

  for (list = priv->tracks; list != NULL; list = list->next)
    {
      track = list->data;

      handle_x = track->start.x;
      handle_y = track->start.y;
      if (ABS (handle_x - x) < max_x && ABS (handle_y - y) < max_y)
        {
          priv->found_state = STATE_DRAG_START;
          handle_found = TRUE;
          break;
        }

      handle_x = track->end.x;
      handle_y = track->end.y;
      if (ABS (handle_x - x) < max_x && ABS (handle_y - y) < max_y)
        {
          priv->found_state = STATE_DRAG_END;
          handle_found = TRUE;
          break;
        }

      handle_x = (track->end.x + track->start.x) / 2.0;
      handle_y = (track->end.y + track->start.y) / 2.0;
      if (ABS (handle_x - x) < max_x && ABS (handle_y - y) < max_y)
        {
          priv->found_state = STATE_DRAG_MIDDLE;
          handle_found = TRUE;
          break;
        }
    }

  if (handle_found)
    {
      handle_coord->x = handle_x;
      handle_coord->y = handle_y;
      priv->found_track = track;
    }

  return handle_found;
}

/* Находит хэндл границы полинона в точке x, y. */
static gboolean
hyscan_gtk_map_planner_handle_find_zone (HyScanGtkMapPlanner  *planner,
                                         gdouble               x,
                                         gdouble               y,
                                         HyScanGeoCartesian2D *handle_coord)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  HyScanGtkMapPlannerZone *zone;
  HyScanGtkMapPoint *point, *next_point;
  HyScanGeoCartesian2D cursor = {.x = x, .y = y};
  HyScanGeoCartesian2D edge_point;

  GList *list;
  gdouble scale_x, scale_y;
  gdouble max_x, max_y;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, &scale_y);
  max_x = HANDLE_HOVER_RADIUS * scale_x;
  max_y = HANDLE_HOVER_RADIUS * scale_y;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &zone))
    {
      gint vertex_num = 0;

      for (list = zone->points; list != NULL; list = list->next)
        {
          point = list->data;

          vertex_num++;

          if (ABS (point->c2d.x - x) < max_x && ABS (point->c2d.y - y) < max_y)
            {
              handle_coord->x = point->c2d.x;
              handle_coord->y = point->c2d.y;
              priv->found_zone = zone;
              priv->found_vertex = vertex_num;
              priv->found_state = STATE_ZONE_DRAG;

              return TRUE;
            }

          next_point = list->next != NULL ? list->next->data : zone->points->data;
          if (hyscan_cartesian_distance_to_line (&point->c2d, &next_point->c2d, &cursor, &edge_point) < max_x &&
              IS_INSIDE (edge_point.x, point->c2d.x, next_point->c2d.x) &&
              IS_INSIDE (edge_point.y, point->c2d.y, next_point->c2d.y))
            {
              handle_coord->x = edge_point.x;
              handle_coord->y = edge_point.y;
              priv->found_zone = zone;
              priv->found_vertex = vertex_num;
              priv->found_state = STATE_ZONE_INSERT;

              return TRUE;
            }
        }
    }

  return FALSE;
}

/* Создаёт галс с параметрами по умолчанию. */
static HyScanPlannerTrack *
hyscan_gtk_map_planner_track_object_new (void)
{
  HyScanPlannerTrack track;

  track.type = HYSCAN_PLANNER_TRACK;
  track.number = 0;
  track.name = "Track";
  track.zone_id = NULL;
  track.speed = 1.0;

  return hyscan_planner_track_copy (&track);
}

static void
hyscan_gtk_map_planner_set_cur_track (HyScanGtkMapPlanner      *planner,
                                      HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gboolean emit_signal;

  emit_signal = (track != priv->cur_track);

  g_clear_pointer (&priv->cur_track, hyscan_gtk_map_planner_track_free);
  priv->cur_track = track;

  if (emit_signal)
    {
      g_signal_emit (planner, hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED], 0,
                     track != NULL ? track->object : NULL);
    }
}

/* Создаёт галс с началом в точке point и начинает перетаскиваение его конца. */
static gboolean
hyscan_gtk_map_planner_handle_create_track (HyScanGtkMapPlanner  *planner,
                                            HyScanGeoCartesian2D  point)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track_proj;

  /* Создаём новый галс только в слое, в БД пока не добавляем. */
  track_proj = hyscan_gtk_map_planner_track_create ();
  track_proj->object = hyscan_gtk_map_planner_track_object_new ();
  track_proj->start = point;
  track_proj->end = point;

  hyscan_gtk_map_value_to_geo (priv->map, &track_proj->object->start, track_proj->start);
  track_proj->object->end = track_proj->object->start;

  /* И начинаем перетаскивание конца галса. */
  priv->cur_state = STATE_DRAG_END;
  hyscan_gtk_map_planner_set_cur_track (planner, track_proj);
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), planner);

  return TRUE;
}

/* Создаёт зону полигона с вершиной в точке point и переводит слой в режим создания зоны. */
static gboolean
hyscan_gtk_map_planner_handle_create_zone (HyScanGtkMapPlanner  *planner,
                                           HyScanGeoCartesian2D  point)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerZone zone;
  HyScanGtkMapPlannerZone *zone_proj;
  HyScanGtkMapPoint vertex;

  /* Создаём новую зону только в слое, в БД пока не добавляем. */
  zone.type = HYSCAN_PLANNER_ZONE;
  zone.name = "Zone";
  zone.points_len = 0;
  zone.ctime = 0;
  zone.mtime = 0;

  zone_proj = hyscan_gtk_map_planner_zone_create ();
  hyscan_gtk_map_planner_zone_set_object (zone_proj, hyscan_planner_zone_copy (&zone));

  vertex.c2d = point;
  hyscan_gtk_map_value_to_geo (priv->map, &vertex.geo, vertex.c2d);

  zone_proj->points = g_list_append (zone_proj->points, hyscan_gtk_map_point_copy (&vertex));
  zone_proj->points = g_list_append (zone_proj->points, hyscan_gtk_map_point_copy (&vertex));

  /* И переходим в режим создания периметра. */
  priv->cur_state = STATE_ZONE_CREATE;
  priv->cur_zone = zone_proj;
  priv->cur_vertex = NULL;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), planner);

  return TRUE;
}

/* Переводит слой в режим создания точки отсчёта. */
static gboolean
hyscan_gtk_map_planner_handle_create_origin (HyScanGtkMapPlanner  *planner,
                                             HyScanGeoCartesian2D  point)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerOrigin origin;
  HyScanGtkMapPlannerOrigin *origin_proj;

  /* Создаём новую зону только в слое, в БД пока не добавляем. */
  origin.type = HYSCAN_PLANNER_ORIGIN;

  origin_proj = hyscan_gtk_map_planner_origin_create ();
  origin_proj->origin = point;
  origin_proj->object = hyscan_planner_origin_copy (&origin);
  hyscan_gtk_map_value_to_geo (priv->map, &origin_proj->object->origin, origin_proj->origin);

  /* И переходим в режим создания периметра. */
  priv->cur_state = STATE_ORIGIN_CREATE;
  priv->cur_origin = origin_proj;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), planner);

  return TRUE;
}
/* Переводит слой в режим создания точки отсчёта. */
static gboolean
hyscan_gtk_map_planner_handle_create_select (HyScanGtkMapPlanner  *planner,
                                             HyScanGeoCartesian2D  point,
                                             GdkEventButton       *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  /* Формируем таблицу из ИД объектов, к которым добавляется выбор. */
  g_hash_table_remove_all (priv->selection_keep);
  if (event->state & GDK_SHIFT_MASK)
    {
      gint i, n;

      n = g_list_model_get_n_items (G_LIST_MODEL (priv->selection));
      for (i = 0; i < n; i++)
        {
          gchar *id;

          id = g_list_model_get_item (G_LIST_MODEL (priv->selection), i);
          g_hash_table_add (priv->selection_keep, id);
        }
    }

  priv->selection_start = point;
  priv->selection_end = point;
  priv->cur_state = STATE_SELECTION;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), planner);

  return TRUE;
}

/* Добавляет ещё одну вершину в периметр и начинает её перетаскивание. */
static void
hyscan_gtk_map_planner_cur_zone_insert_drag (HyScanGtkMapPlanner  *planner,
                                             HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPoint vertex;

  /* Координаты новой вершины берём из хэндла. */
  vertex.c2d.x = handle->val_x;
  vertex.c2d.y = handle->val_y;
  hyscan_gtk_map_value_to_geo (priv->map, &vertex.geo, vertex.c2d);

  /* Вставляем вершину в список вершин текущей зоны и перетаскиваем. */
  priv->cur_zone->points = g_list_insert (priv->cur_zone->points,
                                          hyscan_gtk_map_point_copy (&vertex),
                                          priv->found_vertex);
  priv->cur_vertex = g_list_nth (priv->cur_zone->points, (guint) (priv->found_vertex));
  priv->cur_state = STATE_ZONE_DRAG;
}

/* Находит на слое хэндл в точке x, y. */
static gboolean
hyscan_gtk_map_planner_handle_find (HyScanGtkLayer       *layer,
                                    gdouble               x,
                                    gdouble               y,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D handle_coord;

  priv->found_state = STATE_NONE;
  priv->found_track = NULL;
  priv->found_zone = NULL;
  priv->found_vertex = 0;

  if (hyscan_gtk_map_planner_handle_find_track (planner, x, y, &handle_coord) ||
      hyscan_gtk_map_planner_handle_find_zone (planner, x, y, &handle_coord))
    {
      handle->val_x = handle_coord.x;
      handle->val_y = handle_coord.y;
      handle->type_name = HANDLE_TYPE_NAME;

      return TRUE;
    }

  return FALSE;
}

static void
hyscan_gtk_map_planner_handle_click (HyScanGtkLayer       *layer,
                                     GdkEventButton       *event,
                                     HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_return_if_fail (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0);

  /* В момент захвата хэндла мы предполагаем, что текущий хэндл не выбран. Иначе это ошибка в логике программы. */
  g_assert (priv->cur_track == NULL && priv->cur_zone == NULL);

  /* В режиме выбора ничего не захватываем. */
  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_SELECT)
    {
      hyscan_gtk_map_planner_handle_click_select (planner, event);
      return;
    }

  priv->cur_state = priv->found_state;
  hyscan_gtk_map_planner_set_cur_track (planner, hyscan_gtk_map_planner_track_copy (priv->found_track));
  priv->cur_zone = hyscan_gtk_map_planner_zone_copy (priv->found_zone);
  priv->cur_vertex = priv->found_vertex > 0 ? g_list_nth (priv->cur_zone->points, (guint) (priv->found_vertex - 1)) : NULL;

  /* Перходим от состояния STATE_ZONE_INSERT к STATE_ZONE_DRAG. */
  if (priv->cur_state == STATE_ZONE_INSERT)
    hyscan_gtk_map_planner_cur_zone_insert_drag (planner, handle);

  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);
}

static gboolean
hyscan_gtk_map_planner_handle_show_track (HyScanGtkLayer       *layer,
                                          HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  const gchar *hover_id;
  gboolean id_changed;
  HyScanGtkMapPlannerState prev_state;
  gboolean state_set;

  prev_state = priv->hover_state;

  if (handle != NULL && priv->found_track != NULL)
    {
      priv->hover_state = priv->found_state;
      hover_id = priv->found_track->id;

      state_set = TRUE;
    }
  else
    {
      hover_id = NULL;
      state_set = FALSE;
    }

  id_changed = g_strcmp0 (hover_id, priv->hover_track_id) != 0;
  if (prev_state != priv->hover_state || id_changed)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  if (id_changed)
    {
      g_free (priv->hover_track_id);
      priv->hover_track_id = g_strdup (hover_id);
    }

  return state_set;
}

static gboolean
hyscan_gtk_map_planner_handle_show_zone (HyScanGtkLayer       *layer,
                                         HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gboolean vertex_changed, zone_changed;
  HyScanGtkMapPlannerState prev_state;
  gint hover_vertex;
  gchar *hover_id;
  gboolean state_set;

  prev_state = priv->hover_state;
  if (handle != NULL && priv->found_zone != NULL)
    {
      hover_id = priv->found_zone->id;
      hover_vertex = priv->found_vertex;
      priv->hover_state = priv->found_state;
      if (priv->hover_state == STATE_ZONE_INSERT)
        {
          priv->hover_edge_point.x = handle->val_x;
          priv->hover_edge_point.y = handle->val_y;
        }
      state_set = TRUE;
    }
  else
    {
      hover_id = NULL;
      hover_vertex = 0;
      state_set = FALSE;
    }

  /* Перерисовка необходима в случаях:
   * 1. смена состояния.
   * 2. изменения выбранного объекта - (zone_changed || vertex_changed),
   * 3. в состоянии вставки вершины STATE_ZONE_INSERT - постоянно, т.к. её координаты меняются каждый раз.
   */
  {
    zone_changed = (g_strcmp0 (priv->hover_zone_id, hover_id) != 0);
    vertex_changed = (hover_vertex != priv->hover_vertex);
    if (prev_state != priv->hover_state || zone_changed || vertex_changed || priv->hover_state == STATE_ZONE_INSERT)
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
  }

  priv->hover_vertex = hover_vertex;
  if (zone_changed)
    {
      g_free (priv->hover_zone_id);
      priv->hover_zone_id = g_strdup (hover_id);
    }

  return state_set;
}

static void
hyscan_gtk_map_planner_handle_show (HyScanGtkLayer       *layer,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gboolean state_set = FALSE;

  g_return_if_fail (handle == NULL || g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0);

  if (hyscan_gtk_map_planner_handle_show_track (layer, handle))
    state_set = TRUE;

  if (hyscan_gtk_map_planner_handle_show_zone (layer, handle))
    state_set = TRUE;

  if (!state_set)
    priv->found_state = STATE_NONE;
}

static gboolean
hyscan_gtk_map_planner_handle_create (HyScanGtkLayer *layer,
                                      GdkEventButton *event)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D point;

  if (!hyscan_gtk_map_planner_accept_btn (planner, event))
    return GDK_EVENT_PROPAGATE;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.x, &point.y);

  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_TRACK)
    return hyscan_gtk_map_planner_handle_create_track (planner, point);
  else if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_ZONE)
    return hyscan_gtk_map_planner_handle_create_zone (planner, point);
  else if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN)
    return hyscan_gtk_map_planner_handle_create_origin (planner, point);
  else if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_SELECT)
    return hyscan_gtk_map_planner_handle_create_select (planner, point, event);

  return FALSE;
}

/* Сохраняет текущую зону в БД. */
static void
hyscan_gtk_map_planner_zone_save (HyScanGtkMapPlanner     *planner,
                                  HyScanGtkMapPlannerZone *zone)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerZone *object;

  /* Завершаем редактирование зоны. */
  object = zone->object;
  object->points_len = g_list_length (zone->points);
  if (object->points_len > 2)
    {
      GList *link;
      gint i = 0;

      g_free (object->points);
      object->points = g_new (HyScanGeoGeodetic, object->points_len);
      for (link = zone->points; link != NULL; link = link->next)
        {
          HyScanGtkMapPoint *point = link->data;

          object->points[i++] = point->geo;
        }

      if (zone->id != NULL)
        hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), zone->id, object);
      else
        hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (priv->model), object);
    }
  else if (zone->id != NULL)
    {
      hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (priv->model), zone->id);
    }
}

static gboolean
hyscan_gtk_map_planner_release_zone (HyScanGtkMapPlanner *planner,
                                     gboolean             stop_secondary,
                                     GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerZone *cur_zone;

  /* По какой-то причине хэндл у нас, но нет активного галса. Вернём хэндл, но ругнёмся. */
  g_return_val_if_fail (priv->cur_zone != NULL, TRUE);
  cur_zone = priv->cur_zone;

  /* При клике левой кнопкой продолжаем добавлять точки. */
  if (stop_secondary && event->button == GDK_BUTTON_PRIMARY)
    {
      GList *link;
      HyScanGtkMapPoint *point;

      link = g_list_last (cur_zone->points);
      point = link->data;
      cur_zone->points = g_list_append (cur_zone->points, hyscan_gtk_map_point_copy (point));

      return FALSE;
    }

  hyscan_gtk_map_planner_zone_save (planner, priv->cur_zone);

  g_clear_pointer (&priv->cur_zone, hyscan_gtk_map_planner_zone_free);
  priv->cur_state = STATE_NONE;

  return TRUE;
}

/* Сохраняет текущий галс в БД. */
static void
hyscan_gtk_map_planner_save_current (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *cur_track = priv->cur_track;

  g_return_if_fail (cur_track != NULL);

  if (priv->cur_track->id == NULL)
    hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (priv->model), cur_track->object);
  else
    hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), cur_track->id, cur_track->object);
}

/* Сохраняет параллельные галсы в БД. */
static void
hyscan_gtk_map_planner_save_parallel (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track;
  GList *link;

  for (link = priv->parallel_tracks; link != NULL; link = link->next)
    {
      track = link->data;
      hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (priv->model), track->object);
    }
}

static gboolean
hyscan_gtk_map_planner_release_track (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_TRACK_PARALLEL)
    hyscan_gtk_map_planner_save_parallel (planner);
  else
    hyscan_gtk_map_planner_save_current (planner);

  priv->cur_state = STATE_NONE;
  hyscan_gtk_map_planner_set_cur_track (planner, NULL);
  g_list_free_full (priv->parallel_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->parallel_tracks = NULL;

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_release_origin (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  hyscan_planner_model_set_origin (priv->model, &priv->cur_origin->object->origin);

  priv->cur_state = STATE_NONE;
  g_clear_pointer (&priv->cur_origin, hyscan_gtk_map_planner_origin_free);

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_release_select (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  priv->cur_state = STATE_NONE;
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_handle_release (HyScanGtkLayer *layer,
                                       GdkEventButton *event,
                                       gconstpointer   howner)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  /* Проверяем, что хэндл у нас. */
  if (howner != planner)
    return FALSE;

  if (priv->cur_state == STATE_ZONE_CREATE)
    return hyscan_gtk_map_planner_release_zone (planner, TRUE, event);
  else if (priv->cur_state == STATE_ZONE_DRAG)
    return hyscan_gtk_map_planner_release_zone (planner, FALSE, event);
  else if (priv->cur_state == STATE_ORIGIN_CREATE)
    return hyscan_gtk_map_planner_release_origin (planner);
  else if (priv->cur_state == STATE_SELECTION)
    return hyscan_gtk_map_planner_release_select (planner);
  else
    return hyscan_gtk_map_planner_release_track (planner);
}

static void
hyscan_gtk_map_planner_set_visible (HyScanGtkLayer *layer,
                                    gboolean        visible)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  priv->visible = visible;
  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_planner_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  return priv->visible;
}

static gboolean
hyscan_gtk_map_planner_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (priv->map == NULL)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);

  return TRUE;
}

static void
hyscan_gtk_map_planner_added (HyScanGtkLayer          *layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);

  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_planner_projection), planner);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_planner_draw), planner);
  g_signal_connect_swapped (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_planner_motion_notify), planner);
  g_signal_connect_swapped (priv->map, "key-press-event", G_CALLBACK (hyscan_gtk_map_planner_key_press), planner);
  g_signal_connect_swapped (priv->map, "configure-event", G_CALLBACK (hyscan_gtk_map_planner_configure), planner);
}

static void
hyscan_gtk_map_planner_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_return_if_fail (priv->map != NULL);

  g_object_unref (priv->map);

  g_signal_handlers_disconnect_by_data (priv->map, planner);
}

static gboolean
hyscan_gtk_map_planner_load_key_file (HyScanGtkLayer *layer,
                                      GKeyFile       *key_file,
                                      const gchar    *group)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble line_width;

  hyscan_gtk_layer_load_key_file_rgba (&priv->track_style.color, key_file, group,
                                       "track-color", DEFAULT_TRACK_COLOR);
  line_width = g_key_file_get_double (key_file, group, "track-width", NULL);
  priv->track_style.line_width = line_width > 0 ? line_width : DEFAULT_TRACK_WIDTH;

  hyscan_gtk_layer_load_key_file_rgba (&priv->track_style_selected.color, key_file, group,
                                       "track-selected-color", DEFAULT_TRACK_SELECTED_COLOR);
  line_width = g_key_file_get_double (key_file, group, "track-selected-width", NULL);
  priv->track_style_selected.line_width = line_width > 0 ? line_width : DEFAULT_TRACK_WIDTH;

  hyscan_gtk_layer_load_key_file_rgba (&priv->zone_style.color, key_file, group,
                                       "zone-color", DEFAULT_ZONE_COLOR);
  line_width = g_key_file_get_double (key_file, group, "zone-width", NULL);
  priv->track_style_selected.line_width = line_width > 0 ? line_width : DEFAULT_ZONE_WIDTH;

  hyscan_gtk_layer_load_key_file_rgba (&priv->origin_style.color, key_file, group,
                                       "origin-color", DEFAULT_ORIGIN_COLOR);
  line_width = g_key_file_get_double (key_file, group, "origin-width", NULL);
  priv->origin_style.line_width = line_width > 0 ? line_width : DEFAULT_ORIGIN_WIDTH;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->handle_find = hyscan_gtk_map_planner_handle_find;
  iface->handle_click = hyscan_gtk_map_planner_handle_click;
  iface->handle_release = hyscan_gtk_map_planner_handle_release;
  iface->handle_create = hyscan_gtk_map_planner_handle_create;
  iface->handle_show = hyscan_gtk_map_planner_handle_show;
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
  iface->grab_input = hyscan_gtk_map_planner_grab_input;
  iface->added = hyscan_gtk_map_planner_added;
  iface->removed = hyscan_gtk_map_planner_removed;
  iface->load_key_file = hyscan_gtk_map_planner_load_key_file;
}

/**
 * hyscan_gtk_map_planner_new:
 * @model: модель объектов планировщика #HyScanPlannerModel
 *
 * Создаёт слой планировщика схемы галсов.
 *
 * Returns: указатель на слой планировщика
 */
HyScanGtkLayer *
hyscan_gtk_map_planner_new (HyScanPlannerModel *model,
                            HyScanListStore    *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PLANNER,
                       "model", model,
                       "selection", selection,
                       NULL);
}

void
hyscan_gtk_map_planner_set_mode (HyScanGtkMapPlanner     *planner,
                                 HyScanGtkMapPlannerMode  mode)
{
  HyScanGtkMapPlannerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PLANNER (planner));
  priv = planner->priv;

  /* Завершаем редактирование, если у нас был хэндл. */
  if (priv->cur_state != STATE_NONE)
    hyscan_gtk_map_planner_cancel (planner);

  planner->priv->mode = mode;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_planner_get_model:
 * @planner: указатель на HyScanGtkMapPlanner
 *
 * Возвращает указатель на используемую слоем модель данных планировщика. Объект модели
 * передаётся без увеличения счётчика ссылок, поэтому при необходимости клиент
 * должен самостоятельно вызвать g_object_ref().
 *
 * Returns: (transfer-none): указатель на модель объектов планировщика
 */
HyScanPlannerModel *
hyscan_gtk_map_planner_get_model (HyScanGtkMapPlanner *planner)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PLANNER (planner), NULL);

  return planner->priv->model;
}
