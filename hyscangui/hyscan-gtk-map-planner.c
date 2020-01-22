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
#include "hyscan-cairo.h"
#include "hyscan-planner-selection.h"
#include "hyscan-gtk-layer-param.h"
#include "hyscan-gtk-planner-shift.h"
#include <hyscan-planner.h>
#include <math.h>
#include <hyscan-cartesian.h>
#include <glib/gi18n-lib.h>

#define HANDLE_TYPE_NAME             "map_planner"
#define HANDLE_RADIUS                3.0
#define HANDLE_HOVER_RADIUS          8.0
#define AXIS_LENGTH                  50.0
#define AXIS_ARROW_LENGTH            5.0

#define IS_INSIDE(x, a, b) ((a) < (b) ? (a) < (x) && (x) < (b) : (b) < (x) && (x) < (a))
#define COPYSTR_IF_NOT_EQUAL(a, b) do { if (g_strcmp0 (a, b) != 0) { g_free (a); (a) = g_strdup (b); } } while (FALSE)

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

enum
{
  SIGNAL_TRACK_CHANGED,
  SIGNAL_LAST
};

enum
{
  GROUP_HNDL_NONE,
  GROUP_HNDL_C,
  GROUP_HNDL_UL,
  GROUP_HNDL_UR,
  GROUP_HNDL_BL,
  GROUP_HNDL_BR,
  GROUP_HNDL_LAST,
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
  STATE_GROUP_EDIT,        /* Режим редактирования точки отсчёта. */
} HyScanGtkMapPlannerState;

typedef struct
{
  gchar                *id;       /* Идентификатор галса. */
  gboolean              disabled; /* Признак того, что с объектом нельзя провзаимодействовать. */
  HyScanPlannerTrack   *object;   /* Структура с исходными данными галса. */
  HyScanGeoCartesian2D  start;    /* Проекция начала галса на карту. */
  HyScanGeoCartesian2D  end;      /* Проекция конца галса на карту. */
} HyScanGtkMapPlannerTrack;

typedef struct
{
  HyScanGtkMapPlannerTrack *original;
  HyScanGtkMapPlannerTrack *current;
} HyScanGtkMapPlannerTrackEdit;

typedef struct
{
  gchar                *id;       /* Идентификатор галса. */
  HyScanPlannerOrigin  *object;   /* Структура с исходными данными точки отсчёта. */
  HyScanGeoCartesian2D  origin;   /* Проекция точки начала отсчёта на карту. */
} HyScanGtkMapPlannerOrigin;

typedef struct
{
  gchar                *id;         /* Идентификатор зоны. */
  gboolean              disabled;   /* Признак того, что с объектом нельзя провзаимодействовать. */
  HyScanPlannerZone    *object;     /* Структура с исходными данными зоны полигона. */
  GList                *points;     /* Проекция вершин зоны на карту, HyScanGtkMapPoint. */
} HyScanGtkMapPlannerZone;

typedef struct
{
  gdouble  line_width;              /* Толщина линии. */
  GdkRGBA  color;                   /* Цвет линии. */
} HyScanGtkMapPlannerStyle;

typedef struct
{
  GHashTable                    *tracks;                   /* Таблица галсов группы. */
  gboolean                       one_zone;                 /* Признак того, что все галсы из одной зоны. */
  gchar                         *zone_id;                  /* Идентификатор зоны галсов. */
  gchar                        **ids;                      /* Идентификаторы галсов внутри группы. */
  gint                           handle_hover;             /* Подсвеченный хэндл. */
  gint                           handle_found;             /* Найденный хэндл. */
  gint                           handle_active;            /* Активных хэндл. */
  HyScanGeoCartesian2D           orig_center;              /* Исходные координаты центра группы (центр поворота). */
  HyScanGeoCartesian2D           orig_handle;              /* Исходные координаты активного хэндла. */
  HyScanGeoCartesian2D           handles[GROUP_HNDL_LAST]; /* Координаты хэндлов. */
  gboolean                       visible;                  /* Признак видимости группы. */
} HyScanGtkMapPlannerGroup;

typedef struct
{
  gint          num;         /* Номер вершины. */
  gchar        *zone_id;     /* Идентификатор зоны. */
} HyScanGtkMapPlannerVertex;

struct _HyScanGtkMapPlannerPrivate
{
  HyScanGtkMap                      *map;                  /* Виджет карты. */
  HyScanPlannerModel                *model;                /* Модель объектов планировщика. */
  HyScanPlannerSelection            *selection;            /* Список ИД выбранных объектов. */
  gboolean                           visible;              /* Признак видимости слоя. */
  GHashTable                        *zones;                /* Таблица зон полигона. */
  GHashTable                        *tracks;               /* Таблица плановых галсов. */
  HyScanGtkMapPlannerOrigin         *origin;               /* Точка начала отсчёта. */

  HyScanGtkMapPlannerMode            mode;                 /* Режим добавления новых элементов на слой. */
  HyScanGtkMapPlannerState           cur_state;            /* Текущий состояние слоя, показывает что редактируется. */
  HyScanGtkMapPlannerOrigin         *cur_origin;           /* Копия начала отсчёта, редактируется в данный момент. */
  HyScanGtkMapPlannerTrack          *cur_track;            /* Копия галса, который редактируется в данный момент. */
  HyScanGtkMapPlannerZone           *cur_zone;             /* Копия зоны, которая редактируется в данный момент. */
  GList                             *cur_vertex;           /* Указатель на редактируемую вершину. */
  HyScanGtkMapPlannerGroup           cur_group;

  gchar                             *hover_track_id;       /* Идентификатор подсвечиваемого галса. */
  gchar                             *hover_zone_id;        /* Идентификатор подсвечиваемой зоны. */
  HyScanGtkMapPlannerVertex          hover_vertex;         /* Вершина под курсором мыши. */
  HyScanGeoCartesian2D               hover_edge_point;     /* Координаты точки на ребре под указателем мыши. */
  HyScanGtkMapPlannerState           hover_state;          /* Состояние, которое будет установлено при выборе хэндла. */

  gchar                             *highlight_zone;       /* Подсвечиваемая зона. */
  gint                               highlight_vertex;     /* Подсвечиваемая вершина. */
  gchar                             *active_track_id;      /* Активный галс. */

  GString                           *found_track_id;       /* ИД галса под курсором мыши. */
  GString                           *found_zone_id;        /* ИД зоны, чья вершина под курсором мыши. */
  gint                               found_vertex;         /* Номер вершины под курсором мыши. */
  HyScanGtkMapPlannerState           found_state;          /* Состояние, которое будет установлено при выборе хэндла. */

  struct
  {
    GtkWidget                       *shift;                /* Виджет для генерации параллельных галсов. */
    gchar                           *track_id;             /* ИД базового галса. */
  }                                  parallel_opts;        /* Параметры параллельных галсов. */

  GList                             *preview_tracks;       /* Список галсов для предпросмотра. */

  GHashTable                        *selection_keep;       /* Таблица с галсами, которые надо оставить выбранными. */

  HyScanGtkLayerParam               *param;                /* Параметры оформления. */
  PangoLayout                       *pango_layout;         /* Шрифт. */
  HyScanGtkMapPlannerStyle           track_style;          /* Стиль оформления галса. */
  HyScanGtkMapPlannerStyle           track_style_selected; /* Стиль оформления выбранного галса. */
  HyScanGtkMapPlannerStyle           track_style_active;   /* Стиль оформления активного галса. */
  HyScanGtkMapPlannerStyle           zone_style;           /* Стиль оформления периметра полигона. */
  HyScanGtkMapPlannerStyle           zone_style_hover;     /* Стиль оформления периметра активного полигона. */
  HyScanGtkMapPlannerStyle           origin_style;         /* Стиль оформления начала координат. */

  struct
  {
    HyScanGtkMapPlannerTrack        *track;                /* Копия галса, на котором открыто меню. */
    GtkMenu                         *menu;                 /* Виджет меню. */
    GtkWidget                       *nav;                  /* Виджет пункта меню "Навигация". */
    GtkWidget                       *inv;                  /* Виджет пункта меню "Сменить направление". */
    GtkWidget                       *del;                  /* Виджет пункта меню "Удалить". */
    GtkWidget                       *parallel;             /* Виджет пункта меню "Добавить параллельный". */
    GtkWidget                       *extend;               /* Виджет пункта меню "Расятнуть до границ". */
    GtkWidget                       *num;                  /* Виджет пункта меню "Начать обход с этого галса." */
  } track_menu;                                            /* Контекстное меню галса. */

  struct
  {
    GtkMenu                         *menu;                 /* Виджет меню. */
    GtkWidget                       *del_vertex;           /* Виджет пункта меню "Удалить вершину". */
    GtkWidget                       *del_zone;             /* Виджет пункта меню "Удалить зону". */
  } zone_menu;                                             /* Контекстное меню зоны. */

  struct
  {
    GtkMenu                         *menu;                 /* Виджет меню. */
    GtkWidget                       *del;                  /* Виджет пункта меню "Удалить". */
    GtkWidget                       *extend;               /* Виджет пункта меню "Расятнуть до границ". */
  } group_menu;                                            /* Контекстное меню группы. */
};

static void                       hyscan_gtk_map_planner_interface_init        (HyScanGtkLayerInterface   *iface);
static void                       hyscan_gtk_map_planner_set_property          (GObject                   *object,
                                                                                guint                      prop_id,
                                                                                const GValue              *value,
                                                                                GParamSpec                *pspec);
static void                       hyscan_gtk_map_planner_object_constructed    (GObject                   *object);
static void                       hyscan_gtk_map_planner_object_finalize       (GObject                   *object);
static void                       hyscan_gtk_map_planner_add_style_param       (HyScanGtkLayerParam       *param,
                                                                                HyScanGtkMapPlannerStyle  *style,
                                                                                const gchar               *color_key,
                                                                                const gchar               *width_key);
static void                       hyscan_gtk_map_planner_model_changed         (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_activated             (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_zone_changed          (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_tracks_changed        (HyScanGtkMapPlanner       *planner,
                                                                                const gchar               *tracks);
static HyScanGtkMapPlannerZone *  hyscan_gtk_map_planner_zone_create           (void);
static HyScanGtkMapPlannerZone *  hyscan_gtk_map_planner_zone_copy             (const HyScanGtkMapPlannerZone
                                                                                                          *zone);
static void                       hyscan_gtk_map_planner_zone_free             (HyScanGtkMapPlannerZone   *zone);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_create          (void);
static HyScanGtkMapPlannerTrack * hyscan_gtk_map_planner_track_copy            (const HyScanGtkMapPlannerTrack
                                                                                                          *track);
static void                       hyscan_gtk_map_planner_track_free            (HyScanGtkMapPlannerTrack  *track);
static HyScanGtkMapPlannerTrackEdit *
                                  hyscan_gtk_map_planner_track_edit_new        (void);
static void                       hyscan_gtk_map_planner_track_edit_free       (HyScanGtkMapPlannerTrackEdit *track_edit);
static HyScanGtkMapPlannerOrigin *hyscan_gtk_map_planner_origin_create         (void);
static void                       hyscan_gtk_map_planner_origin_free           (HyScanGtkMapPlannerOrigin *origin);
static void                       hyscan_gtk_map_planner_track_project         (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_zone_project          (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone);
static void                       hyscan_gtk_map_planner_origin_project        (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerOrigin *origin);
static void                       hyscan_gtk_map_planner_zone_set_object       (HyScanGtkMapPlannerZone   *zone,
                                                                                HyScanPlannerZone         *object);
static void                       hyscan_gtk_map_planner_set_cur_track         (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_set_cur_zone          (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone,
                                                                                gint                       vertex);
static void                       hyscan_gtk_map_planner_clear_cur_track       (HyScanGtkMapPlanner       *planner);

static void                       hyscan_gtk_map_planner_parallel_window       (HyScanGtkMapPlanner       *planner,
                                                                                const HyScanGtkMapPlannerTrack *track);
static void                       hyscan_gtk_map_planner_parallel_save         (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_parallel_close        (HyScanGtkMapPlanner       *planner);

static void                       hyscan_gtk_map_planner_track_remove          (HyScanGtkMapPlanner       *planner,
                                                                                const gchar               *track_id);
static void                       hyscan_gtk_map_planner_selection_remove      (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_vertex_remove         (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone,
                                                                                GList                     *vertex);
static void                       hyscan_gtk_map_planner_vertex_remove_found   (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_zone_save             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *pzone);
static void                       hyscan_gtk_map_planner_zone_remove           (HyScanGtkMapPlanner       *planner,
                                                                                const gchar               *zone_id);

static void                       hyscan_gtk_map_planner_zone_draw             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerZone   *zone,
                                                                                gboolean                   skip_current,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_track_draw            (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track,
                                                                                gboolean                   skip_current,
                                                                                gboolean                   is_preview,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_origin_draw           (HyScanGtkMapPlanner       *planner,
                                                                                cairo_t                   *cairo);
static void                       hyscan_gtk_map_planner_group_draw            (HyScanGtkMapPlanner       *planner,
                                                                                cairo_t                   *cairo);

static inline void                hyscan_gtk_map_planner_cairo_style           (cairo_t                   *cairo,
                                                                                HyScanGtkMapPlannerStyle  *style);
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
static gboolean                   hyscan_gtk_map_planner_group_drag            (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);

static void                       hyscan_gtk_map_planner_draw                  (GtkCifroArea              *carea,
                                                                                cairo_t                   *cairo,
                                                                                HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_projection            (HyScanGtkMapPlanner       *planner,
                                                                                GParamSpec                *pspec);
static gboolean                   hyscan_gtk_map_planner_configure             (HyScanGtkMapPlanner       *planner,
                                                                                GdkEvent                  *screen);
static gboolean                   hyscan_gtk_map_planner_key_press             (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventKey               *event);
static gboolean                   hyscan_gtk_map_planner_motion_notify         (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);

static void                       hyscan_gtk_map_planner_handle_click          (HyScanGtkLayer            *layer,
                                                                                GdkEventButton            *event,
                                                                                HyScanGtkLayerHandle      *handle);
static void                       hyscan_gtk_map_planner_handle_click_menu     (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventButton            *event);
static void                       hyscan_gtk_map_planner_handle_click_select   (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventButton            *event);


static void                       hyscan_gtk_map_planner_cur_zone_insert_drag  (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkLayerHandle      *handle);
static inline gboolean            hyscan_gtk_map_planner_is_state_track        (HyScanGtkMapPlannerState   state);
static inline gboolean            hyscan_gtk_map_planner_is_state_vertex       (HyScanGtkMapPlannerState   state);
static HyScanPlannerTrack *       hyscan_gtk_map_planner_track_object_new      (void);
static void                       hyscan_gtk_map_planner_save_current          (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_cancel                (HyScanGtkMapPlanner       *planner);
static gboolean                   hyscan_gtk_map_planner_accept_btn            (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventButton            *event);

static void                       hyscan_gtk_map_planner_menu_activate         (GtkButton                 *button,
                                                                                gpointer                   user_data);
static GtkWidget *                hyscan_gtk_map_planner_menu_add              (HyScanGtkMapPlanner       *planner,
                                                                                GtkMenuShell              *menu,
                                                                                const gchar               *title);
static inline gboolean            hyscan_gtk_map_planner_track_is_grouped      (HyScanGtkMapPlanner       *planner,
                                                                                const HyScanGtkMapPlannerTrack *track);
static gboolean                   hyscan_gtk_map_planner_track_is_cur          (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGtkMapPlannerTrack  *track);
static void                       hyscan_gtk_map_planner_track_invert          (HyScanGtkMapPlanner            *planner,
                                                                                const HyScanGtkMapPlannerTrack *track);
static void                       hyscan_gtk_map_planner_track_extend          (HyScanGtkMapPlanner            *planner,
                                                                                const HyScanGtkMapPlannerTrack *track);
static void                       hyscan_gtk_map_planner_selection_extend      (HyScanGtkMapPlanner       *planner);

static gboolean                   hyscan_gtk_map_planner_handle_release        (HyScanGtkLayer            *layer,
                                                                                GdkEventButton            *event,
                                                                                gconstpointer              howner);
static gboolean                   hyscan_gtk_map_planner_release_zone          (HyScanGtkMapPlanner       *planner,
                                                                                gboolean                   stop_secondary,
                                                                                GdkEventButton            *event);
static gboolean                   hyscan_gtk_map_planner_release_track         (HyScanGtkMapPlanner       *planner);
static gboolean                   hyscan_gtk_map_planner_release_origin        (HyScanGtkMapPlanner       *planner);
static gboolean                   hyscan_gtk_map_planner_release_group         (HyScanGtkMapPlanner       *planner);

static gboolean                   hyscan_gtk_map_planner_handle_create         (HyScanGtkLayer            *layer,
                                                                                GdkEventButton            *event);
static gboolean                   hyscan_gtk_map_planner_handle_create_zone    (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D       point);
static gboolean                   hyscan_gtk_map_planner_handle_create_track   (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D       point);
static gboolean                   hyscan_gtk_map_planner_handle_create_origin  (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D       point);

static gboolean                   hyscan_gtk_map_planner_handle_find           (HyScanGtkLayer            *layer,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_find_track     (HyScanGtkMapPlanner       *planner,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGeoCartesian2D      *handle_coord);
static gboolean                   hyscan_gtk_map_planner_handle_find_zone      (HyScanGtkMapPlanner       *planner,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGeoCartesian2D      *handle_coord);
static gboolean                   hyscan_gtk_map_planner_handle_find_group     (HyScanGtkMapPlanner       *planner,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                HyScanGeoCartesian2D      *handle_coord);

static void                       hyscan_gtk_map_planner_handle_show           (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_show_track     (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_show_zone      (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);
static gboolean                   hyscan_gtk_map_planner_handle_show_group     (HyScanGtkLayer            *layer,
                                                                                HyScanGtkLayerHandle      *handle);

static gboolean                   hyscan_gtk_map_planner_select_change         (HyScanGtkLayer            *layer,
                                                                                gdouble                    from_x,
                                                                                gdouble                    to_x,
                                                                                gdouble                    from_y,
                                                                                gdouble                    to_y);

static void                       hyscan_gtk_map_planner_group_boundary        (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_group_clear_cur       (HyScanGtkMapPlanner       *planner);
static void                       hyscan_gtk_map_planner_group_scale           (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static void                       hyscan_gtk_map_planner_group_rotate          (HyScanGtkMapPlanner       *planner,
                                                                                GdkEventMotion            *event);
static inline void                hyscan_gtk_map_planner_group_rotate_point    (HyScanGeoCartesian2D      *origin,
                                                                                HyScanGeoCartesian2D      *center,
                                                                                gdouble                    angle,
                                                                                HyScanGeoCartesian2D      *rotated);
static const gchar *              hyscan_gtk_map_planner_zone_find             (HyScanGtkMapPlanner       *planner,
                                                                                HyScanGeoCartesian2D      *point);

static guint hyscan_gtk_map_planner_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPlanner, hyscan_gtk_map_planner, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapPlanner)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_planner_interface_init))

static void
hyscan_gtk_map_planner_class_init (HyScanGtkMapPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_planner_set_property;
  object_class->constructed = hyscan_gtk_map_planner_object_constructed;
  object_class->finalize = hyscan_gtk_map_planner_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "Planner model", HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanListStore", "List of selected ids", HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

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
  GtkMenuShell *menu;

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->constructed (object);

  priv->mode = HYSCAN_GTK_MAP_PLANNER_MODE_TRACK;
  priv->found_zone_id = g_string_new (NULL);
  priv->found_track_id = g_string_new (NULL);

  /* Не задаём функцию удаления ключей, т.к. ими владеет сама структура зоны, поле id. */
  priv->zones = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                       (GDestroyNotify) hyscan_gtk_map_planner_zone_free);
  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                        (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->cur_group.tracks = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                                  (GDestroyNotify) hyscan_gtk_map_planner_track_edit_free);

  priv->selection_keep = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Стиль оформления. */
  priv->param = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-planner");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->zone_style, "/zone-color", "/zone-width");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->zone_style_hover,
                                          "/zone-hover-color", "/zone-hover-width");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->track_style, "/track-color", "/track-width");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->origin_style, "/origin-color", "/origin-width");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->track_style_selected,
                                          "/track-selected-color", "/track-selected-width");
  hyscan_gtk_map_planner_add_style_param (priv->param, &priv->track_style_active,
                                          "/track-active-color", "/track-active-width");
  hyscan_gtk_layer_param_set_default (priv->param);

  menu = GTK_MENU_SHELL (gtk_menu_new ());
  priv->track_menu.menu = GTK_MENU (menu);
  priv->track_menu.nav = hyscan_gtk_map_planner_menu_add (planner, menu, _("Navigate"));
  priv->track_menu.inv = hyscan_gtk_map_planner_menu_add (planner, menu, _("Invert"));
  priv->track_menu.num = hyscan_gtk_map_planner_menu_add (planner, menu, _("Number tracks starting from here"));
  priv->track_menu.parallel = hyscan_gtk_map_planner_menu_add (planner, menu, _("Create parallel tracks..."));
  priv->track_menu.extend = hyscan_gtk_map_planner_menu_add (planner, menu, _("Adjust length to zone boundary"));
  priv->track_menu.del = hyscan_gtk_map_planner_menu_add (planner, menu, _("Delete"));

  menu = GTK_MENU_SHELL (gtk_menu_new ());
  priv->zone_menu.menu = GTK_MENU (menu);
  priv->zone_menu.del_vertex = hyscan_gtk_map_planner_menu_add (planner, menu, _("Delete vertex"));
  priv->zone_menu.del_zone = hyscan_gtk_map_planner_menu_add (planner, menu, _("Delete zone"));

  menu = GTK_MENU_SHELL (gtk_menu_new ());
  priv->group_menu.menu = GTK_MENU (menu);
  priv->group_menu.extend = hyscan_gtk_map_planner_menu_add (planner, menu, _("Adjust length to zone boundary"));
  priv->group_menu.del = hyscan_gtk_map_planner_menu_add (planner, menu, _("Delete"));

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_map_planner_model_changed), planner);
  g_signal_connect_swapped (priv->selection, "zone-changed", G_CALLBACK (hyscan_gtk_map_planner_zone_changed), planner);
  g_signal_connect_swapped (priv->selection, "tracks-changed", G_CALLBACK (hyscan_gtk_map_planner_tracks_changed), planner);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_planner_activated), planner);
}

static void
hyscan_gtk_map_planner_object_finalize (GObject *object)
{
  HyScanGtkMapPlanner *gtk_map_planner = HYSCAN_GTK_MAP_PLANNER (object);
  HyScanGtkMapPlannerPrivate *priv = gtk_map_planner->priv;

  g_signal_handlers_disconnect_by_data (priv->model, gtk_map_planner);

  g_clear_pointer (&priv->track_menu.track, hyscan_gtk_map_planner_track_free);
  g_clear_object (&priv->param);
  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_string_free (priv->found_track_id, TRUE);
  g_string_free (priv->found_zone_id, TRUE);
  g_free (priv->active_track_id);
  g_free (priv->hover_track_id);
  g_free (priv->hover_zone_id);
  g_free (priv->hover_vertex.zone_id);
  g_free (priv->parallel_opts.track_id);
  g_clear_pointer (&priv->origin, hyscan_gtk_map_planner_origin_free);
  g_hash_table_destroy (priv->tracks);
  g_list_free_full (priv->preview_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  g_hash_table_destroy (priv->zones);
  g_hash_table_destroy (priv->selection_keep);

  g_clear_pointer (&priv->cur_group.ids, g_strfreev);
  g_free (priv->cur_group.zone_id);
  g_hash_table_destroy (priv->cur_group.tracks);

  G_OBJECT_CLASS (hyscan_gtk_map_planner_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_planner_add_style_param (HyScanGtkLayerParam      *param,
                                        HyScanGtkMapPlannerStyle *style,
                                        const gchar              *color_key,
                                        const gchar              *width_key)
{
  hyscan_gtk_layer_param_add_rgba (param, color_key, &style->color);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (param), width_key, &style->line_width);
}

static void
hyscan_gtk_map_planner_zone_changed (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gchar *prev_zone;
  gint prev_vertex;

  prev_zone = priv->highlight_zone;
  prev_vertex = priv->highlight_vertex;

  priv->highlight_zone = hyscan_planner_selection_get_zone (priv->selection, &priv->highlight_vertex);

  if (g_strcmp0 (prev_zone, priv->highlight_zone) != 0 || prev_vertex != priv->highlight_vertex)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_free (prev_zone);
}

/* Определяет размер группы и координаты её хэндлов на основе её содержимого. */
static void
hyscan_gtk_map_planner_group_boundary (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &planner->priv->cur_group;
  HyScanGtkMapPlannerTrack *track;
  GHashTableIter iter;
  gpointer value;
  gboolean iterate_all;
  gdouble from_x = G_MAXDOUBLE, from_y = G_MAXDOUBLE, to_x = -G_MAXDOUBLE, to_y = -G_MAXDOUBLE;

  if (group->ids == NULL)
    return;

  iterate_all = group->handle_active == GROUP_HNDL_NONE;
  g_hash_table_iter_init (&iter, iterate_all ? priv->tracks : group->tracks);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (iterate_all)
        {
          track = value;
          if (!g_strv_contains ((const gchar *const *) group->ids, track->id))
            continue;
        }
      else
        {
          track = ((HyScanGtkMapPlannerTrackEdit *) value)->current;
        }

      from_x = MIN (from_x, MIN (track->start.x, track->end.x));
      to_x = MAX (to_x, MAX (track->start.x, track->end.x));
      from_y = MIN (from_y, MIN (track->start.y, track->end.y));
      to_y = MAX (to_y, MAX (track->start.y, track->end.y));
    }

  group->handles[GROUP_HNDL_UL].x = from_x;
  group->handles[GROUP_HNDL_UL].y = from_y;
  group->handles[GROUP_HNDL_UR].x = to_x;
  group->handles[GROUP_HNDL_UR].y = from_y;
  group->handles[GROUP_HNDL_BL].x = from_x;
  group->handles[GROUP_HNDL_BL].y = to_y;
  group->handles[GROUP_HNDL_BR].x = to_x;
  group->handles[GROUP_HNDL_BR].y = to_y;
  group->handles[GROUP_HNDL_C].x = (from_x + to_x) / 2.0;
  group->handles[GROUP_HNDL_C].y = (from_y + to_y) / 2.0;
}

static void
hyscan_gtk_map_planner_tracks_changed (HyScanGtkMapPlanner *planner,
                                       const gchar         *tracks)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_clear_pointer (&priv->cur_group.ids, g_strfreev);
  priv->cur_group.ids = hyscan_planner_selection_get_tracks (priv->selection);
  priv->cur_group.visible = (g_strv_length (priv->cur_group.ids) > 1);
  hyscan_gtk_map_planner_group_boundary (planner);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_activated (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gchar *active_id;
  gboolean redraw;

  active_id = hyscan_planner_selection_get_active_track (priv->selection);

  redraw = (g_strcmp0 (active_id, priv->active_track_id) != 0);
  g_free (priv->active_track_id);
  priv->active_track_id = active_id;

  if (redraw)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
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
  HyScanObject *object;

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
  g_hash_table_remove_all (priv->zones);
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanPlannerZone *orig_zone;
      HyScanGtkMapPlannerZone *zone;

      if (!HYSCAN_IS_PLANNER_ZONE (object))
        continue;

      orig_zone = (HyScanPlannerZone *) object;

      /* Воруем ключ и объект из одной хэш-таблицы и добавляем их в другую. */
      g_hash_table_iter_steal (&iter);
      zone = hyscan_gtk_map_planner_zone_create ();
      zone->id = key;
      hyscan_gtk_map_planner_zone_set_object (zone, orig_zone);
      hyscan_gtk_map_planner_zone_project (planner, zone);
      g_hash_table_insert (priv->zones, key, zone);
    }

  /* Удаляем found_zone, если она больше не существует. */
  if (priv->found_zone_id->len > 0 && !g_hash_table_contains (priv->zones, priv->found_zone_id->str))
    {
      g_string_truncate (priv->found_zone_id, 0);
      priv->found_vertex = 0;
    }

  g_hash_table_remove_all (priv->tracks);
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &object))
    {
      HyScanPlannerTrack *orig_track;
      HyScanGtkMapPlannerTrack *track;

      if (!HYSCAN_IS_PLANNER_TRACK (object))
        continue;

      orig_track = (HyScanPlannerTrack *) object;

      /* Воруем ключ и объект из хэш-таблицы - ключ удаляем, а объект помещаем в зону. */
      g_hash_table_iter_steal (&iter);
      track = hyscan_gtk_map_planner_track_create ();
      track->id = key;
      track->object = orig_track;
      hyscan_gtk_map_planner_track_project (planner, track);
      g_hash_table_insert (priv->tracks, key, track);

      /* Обновляем базовый галс для параллельных галсов. */
      if (g_strcmp0 (priv->parallel_opts.track_id, track->id) == 0)
        {
          HyScanGtkMapPlannerZone *track_zone;
          const gchar *track_zone_id;

          track_zone_id = track->object->zone_id;
          track_zone = track_zone_id != NULL ? g_hash_table_lookup (priv->zones, track_zone_id) : NULL;

          hyscan_gtk_planner_shift_set_track (HYSCAN_GTK_PLANNER_SHIFT (priv->parallel_opts.shift),
                                              track->object, track_zone != NULL ? track_zone->object : NULL);
        }
    }

  /* Удаляем found_track, если он больше не существует. */
  if (priv->found_track_id->len != 0 && !g_hash_table_contains (priv->tracks, priv->found_track_id->str))
    g_string_truncate (priv->found_track_id, 0);

  /* Обновляем границы выбранной группы галсов. */
  hyscan_gtk_map_planner_group_boundary (planner);

  g_hash_table_unref (objects);

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
hyscan_gtk_map_planner_track_edit_free (HyScanGtkMapPlannerTrackEdit *track_edit)
{
  hyscan_gtk_map_planner_track_free (track_edit->current);
  hyscan_gtk_map_planner_track_free (track_edit->original);
  g_slice_free (HyScanGtkMapPlannerTrackEdit, track_edit);
}

static HyScanGtkMapPlannerTrackEdit *
hyscan_gtk_map_planner_track_edit_new (void)
{
  return g_slice_new0 (HyScanGtkMapPlannerTrackEdit);
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

  hyscan_gtk_map_geo_to_value (priv->map, track->object->plan.start, &track->start);
  hyscan_gtk_map_geo_to_value (priv->map, track->object->plan.end, &track->end);
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
  HyScanGtkMapPlannerZone *zone;
  HyScanGtkMapPlannerTrack *track;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &zone))
    hyscan_gtk_map_planner_zone_project (planner, zone);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    hyscan_gtk_map_planner_track_project (planner, track);

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
  HyScanGtkMapPlannerTrack *track;
  GList *list;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return;

  cairo_save (cairo);

  /* Рисуем начало отсчёта и направление осей координат. */
  if (priv->mode == HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN)
    hyscan_gtk_map_planner_origin_draw (planner, cairo);

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    hyscan_gtk_map_planner_zone_draw (planner, zone, TRUE, cairo);

  /* Рисуем текущую редактируемую зону. */
  if (priv->cur_zone != NULL)
    hyscan_gtk_map_planner_zone_draw (planner, priv->cur_zone, FALSE, cairo);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    hyscan_gtk_map_planner_track_draw (planner, track, TRUE, FALSE, cairo);

  /* Параллельные галсы в режиме создания параллельных галсов. */
  for (list = priv->preview_tracks; list != NULL; list = list->next)
    hyscan_gtk_map_planner_track_draw (planner, list->data, TRUE, TRUE, cairo);

  /* Рисуем текущий редактируемый галс. */
  if (priv->cur_track != NULL)
    hyscan_gtk_map_planner_track_draw (planner, priv->cur_track, FALSE, FALSE, cairo);

  if (priv->cur_group.visible)
    hyscan_gtk_map_planner_group_draw (planner, cairo);

  cairo_restore (cairo);
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

  return GDK_EVENT_PROPAGATE;
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

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_planner_track_move (HyScanGtkMapPlanner          *planner,
                                   HyScanGtkMapPlannerTrackEdit *track_edit,
                                   HyScanGeoCartesian2D         *movement)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble dx, dy;
  gdouble center_x, center_y;
  HyScanGtkMapPlannerTrack *track = track_edit->current;

  dx = (track->end.x - track->start.x) / 2.0;
  dy = (track->end.y - track->start.y) / 2.0;
  center_x = (track->start.x + track->end.x) / 2.0;
  center_y = (track->start.y + track->end.y) / 2.0;

  track->start.x = movement->x + center_x - dx;
  track->start.y = movement->y + center_y - dy;
  track->end.x = movement->x + center_x + dx;
  track->end.y = movement->y + center_y + dy;

  hyscan_gtk_map_value_to_geo (priv->map, &track->object->plan.start, track->start);
  hyscan_gtk_map_value_to_geo (priv->map, &track->object->plan.end, track->end);
}

static inline gint
hyscan_gtk_map_planner_group_opposite (gint     handle,
                                       gboolean opposite_x,
                                       gboolean opposite_y)
{
  guint index;

  index = (opposite_x ? 1 : 0) + (opposite_y ? 2 : 0);

  switch (handle)
    {
    case GROUP_HNDL_UR:
      return ( (gint[]) { GROUP_HNDL_UR, GROUP_HNDL_UL, GROUP_HNDL_BR, GROUP_HNDL_BL })[index];

    case GROUP_HNDL_UL:
      return ( (gint[]) { GROUP_HNDL_UL, GROUP_HNDL_UR, GROUP_HNDL_BL, GROUP_HNDL_BR })[index];

    case GROUP_HNDL_BR:
      return ( (gint[]) { GROUP_HNDL_BR, GROUP_HNDL_BL, GROUP_HNDL_UR, GROUP_HNDL_UL })[index];

    case GROUP_HNDL_BL:
      return ( (gint[]) { GROUP_HNDL_BL, GROUP_HNDL_BR, GROUP_HNDL_UL, GROUP_HNDL_UR })[index];

    default:
      g_return_val_if_reached (handle);
    }
}

static inline void
hyscan_gtk_map_planner_group_rotate_point (HyScanGeoCartesian2D *origin,
                                           HyScanGeoCartesian2D *center,
                                           gdouble               angle,
                                           HyScanGeoCartesian2D *rotated)
{
  gdouble point_angle;
  gdouble distance;

  distance = hypot (origin->y - center->y, origin->x - center->x);
  point_angle = atan2 (origin->y - center->y, origin->x - center->x);
  rotated->x = center->x + cos (point_angle + angle) * distance;
  rotated->y = center->y + sin (point_angle + angle) * distance;
}

static void
hyscan_gtk_map_planner_group_rotate (HyScanGtkMapPlanner *planner,
                                     GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;
  GHashTableIter iter;
  HyScanGtkMapPlannerTrackEdit *track_edit;
  HyScanGtkMapPlannerTrack *current, *original;
  HyScanGeoCartesian2D *center;
  HyScanGeoCartesian2D cursor;
  gdouble angle;

  center = &group->orig_center;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);
  angle = atan2 (cursor.y - center->y, cursor.x - center->x) -
          atan2 (group->orig_handle.y - center->y, group->orig_handle.x - center->x);

  g_hash_table_iter_init (&iter, group->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_edit))
    {
      current = track_edit->current;
      original = track_edit->original;

      hyscan_gtk_map_planner_group_rotate_point (&original->start, center, angle, &current->start);
      hyscan_gtk_map_planner_group_rotate_point (&original->end, center, angle, &current->end);

      hyscan_gtk_map_value_to_geo (priv->map, &current->object->plan.start, current->start);
      hyscan_gtk_map_value_to_geo (priv->map, &current->object->plan.end, current->end);
    }
}

static void
hyscan_gtk_map_planner_group_scale (HyScanGtkMapPlanner *planner,
                                    GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;
  gdouble scale_x, scale_y;
  GHashTableIter iter;
  HyScanGtkMapPlannerTrack *track;
  HyScanGtkMapPlannerTrackEdit *track_edit;
  HyScanGeoCartesian2D *center, *handle;
  HyScanGeoCartesian2D cursor;
  gint opposite_index;

  opposite_index = hyscan_gtk_map_planner_group_opposite (group->handle_active, TRUE, TRUE);
  handle = &group->handles[group->handle_active];
  center = &group->handles[opposite_index];
  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  gdouble cifro_scale_x, cifro_scale_y;
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &cifro_scale_x, &cifro_scale_y);

  /* Чтобы избежать нулевых масштабов. */
  if (ABS (cursor.x - center->x) < cifro_scale_x || ABS (cursor.y - center->y) < cifro_scale_y)
    return;

  scale_x = (cursor.x - center->x) / (handle->x - center->x);
  scale_y = (cursor.y - center->y) / (handle->y - center->y);
  if (event->state & GDK_SHIFT_MASK)
    {
      gint sign_x, sign_y;
      sign_x = scale_x < 0 ? -1 : 1;
      sign_y = scale_y < 0 ? -1 : 1;

      if (ABS (scale_x) < ABS (scale_y))
        scale_y = ABS (scale_x) * sign_y;
      else
        scale_x = ABS (scale_y) * sign_x;
    }

  /* При отрицательном изменении масштаба активный хэндл меняется. */
  group->handle_active = hyscan_gtk_map_planner_group_opposite (group->handle_active, scale_x < 0, scale_y < 0);

  g_hash_table_iter_init (&iter, group->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_edit))
    {
      track = track_edit->current;

      track->start.x = (track->start.x - center->x) * scale_x + center->x;
      track->start.y = (track->start.y - center->y) * scale_y + center->y;
      track->end.x = (track->end.x - center->x) * scale_x + center->x;
      track->end.y = (track->end.y - center->y) * scale_y + center->y;

      hyscan_gtk_map_value_to_geo (priv->map, &track->object->plan.start, track->start);
      hyscan_gtk_map_value_to_geo (priv->map, &track->object->plan.end, track->end);
    }
}

/* Редактирование точки отсчёта при захвате за вершину. */
static gboolean
hyscan_gtk_map_planner_group_drag (HyScanGtkMapPlanner *planner,
                                   GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;
  HyScanGeoCartesian2D cursor;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &cursor.x, &cursor.y);

  /* Перемещение. */
  if (group->handle_active == GROUP_HNDL_C)
    {
      GHashTableIter iter;
      HyScanGtkMapPlannerTrackEdit *track;
      HyScanGeoCartesian2D movement;
      const gchar *parent_zone_id;

      movement.x = cursor.x - group->handles[GROUP_HNDL_C].x;
      movement.y = cursor.y - group->handles[GROUP_HNDL_C].y;

      parent_zone_id = hyscan_gtk_map_planner_zone_find (planner, &cursor);
      COPYSTR_IF_NOT_EQUAL (group->zone_id, parent_zone_id);
      g_hash_table_iter_init (&iter, group->tracks);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
        {
          hyscan_gtk_map_planner_track_move (planner, track, &movement);
          COPYSTR_IF_NOT_EQUAL (track->current->object->zone_id, parent_zone_id);
        }
    }
  else if (event->state & GDK_CONTROL_MASK)
    {
      hyscan_gtk_map_planner_group_rotate (planner, event);
    }
  else
    {
      hyscan_gtk_map_planner_group_scale (planner, event);
    }

  hyscan_gtk_map_planner_group_boundary (planner);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_PROPAGATE;
}

/* Изменение списка выбранных галсов при изменении области выделения. */
static gboolean
hyscan_gtk_map_planner_select_change (HyScanGtkLayer       *layer,
                                      gdouble               from_x,
                                      gdouble               to_x,
                                      gdouble               from_y,
                                      gdouble               to_y)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGeoCartesian2D selection_start, selection_end;
  HyScanGtkMapPlannerTrack *track;
  gint total = 0;
  GHashTableIter iter;

  selection_start.x = from_x;
  selection_start.y = from_y;
  selection_end.x = to_x;
  selection_end.y = to_y;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      gboolean selected_before, selected;

      if (g_hash_table_contains (priv->selection_keep, track->id))
        continue;

      selected_before = hyscan_planner_selection_contains (priv->selection, track->id);
      selected = hyscan_cartesian_is_inside (&track->start, &track->end, &selection_start, &selection_end);
      total += selected ? 1 : 0;

      if (selected && !selected_before)
        hyscan_planner_selection_append (priv->selection, track->id);
      else if (!selected && selected_before)
        hyscan_planner_selection_remove (priv->selection, track->id);
    }

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return total;
}

static gboolean
hyscan_gtk_map_planner_is_point_in_zone (HyScanGeoCartesian2D     *point,
                                         HyScanGtkMapPlannerZone  *zone)
{
  HyScanGeoCartesian2D *vertices;
  gint i, n;
  GList *link;
  gboolean result;

  n = zone->object->points_len;
  vertices = g_new (HyScanGeoCartesian2D, n);
  for (link = zone->points, i = 0; link != NULL; link = link->next)
    {
      HyScanGtkMapPoint *vertex = link->data;

      vertices[i++] = vertex->c2d;
    }

  result = hyscan_cartesian_is_inside_polygon (vertices, n, point);
  g_free (vertices);

  return result;
}

static const gchar *
hyscan_gtk_map_planner_zone_find (HyScanGtkMapPlanner  *planner,
                                  HyScanGeoCartesian2D *point)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  HyScanGtkMapPlannerZone *zone;
  gchar *zone_id;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, (gpointer *) &zone_id, (gpointer *) &zone))
    {
      if (hyscan_gtk_map_planner_is_point_in_zone (point, zone))
        return zone_id;
    }

  return NULL;
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
      const gchar *parent_zone_id;

      dx = (priv->cur_track->end.x - priv->cur_track->start.x) / 2.0;
      dy = (priv->cur_track->end.y - priv->cur_track->start.y) / 2.0;

      priv->cur_track->start.x = cursor.x - dx;
      priv->cur_track->start.y = cursor.y - dy;
      priv->cur_track->end.x = cursor.x + dx;
      priv->cur_track->end.y = cursor.y + dy;

      parent_zone_id = hyscan_gtk_map_planner_zone_find (planner, &cursor);
      COPYSTR_IF_NOT_EQUAL (priv->cur_track->object->zone_id, parent_zone_id);
    }

  hyscan_gtk_map_value_to_geo (priv->map, &priv->cur_track->object->plan.start, priv->cur_track->start);
  hyscan_gtk_map_value_to_geo (priv->map, &priv->cur_track->object->plan.end, priv->cur_track->end);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_signal_emit (planner, hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED], 0, priv->cur_track->object);

  return GDK_EVENT_PROPAGATE;
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

  return GDK_EVENT_PROPAGATE;
}

/* Обработка события "motion-notify".
 * Перемещает галс в новую точку. */
static gboolean
hyscan_gtk_map_planner_motion_notify (HyScanGtkMapPlanner *planner,
                                      GdkEventMotion      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track = priv->cur_track;

  switch (priv->cur_state)
  {
    case STATE_DRAG_START:
      return hyscan_gtk_map_planner_edge_drag (planner, &track->start, &track->end, &track->object->plan.start, event);

    case STATE_DRAG_END:
      return hyscan_gtk_map_planner_edge_drag (planner, &track->end, &track->start, &track->object->plan.end, event);

    case STATE_DRAG_MIDDLE:
      return hyscan_gtk_map_planner_middle_drag (planner, event);

    case STATE_ZONE_CREATE:
    case STATE_ZONE_DRAG:
      return hyscan_gtk_map_planner_vertex_drag (planner, event);

    case STATE_ORIGIN_CREATE:
      return hyscan_gtk_map_planner_origin_drag (planner, event);

    case STATE_GROUP_EDIT:
      return hyscan_gtk_map_planner_group_drag (planner, event);

    default:
      return GDK_EVENT_PROPAGATE;
  }
}

static void
hyscan_gtk_map_planner_track_remove (HyScanGtkMapPlanner *planner,
                                     const gchar         *track_id)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  /* Удаляем из модели. */
  hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (priv->model), track_id);

  /* Удаляем из внутреннего списка. */
  g_hash_table_remove (priv->tracks, track_id);
}

/* Меняет направление галса. */
static void
hyscan_gtk_map_planner_track_invert (HyScanGtkMapPlanner           *planner,
                                     const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack *modified_track;
  HyScanGeoGeodetic tmp;

  modified_track = hyscan_planner_track_copy (track->object);
  tmp = modified_track->plan.start;
  modified_track->plan.start = modified_track->plan.end;
  modified_track->plan.end = tmp;

  hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), track->id,
                                     (const HyScanObject *) modified_track);
  hyscan_planner_track_free (modified_track);
}

/* Растягивает все выбранные галсы до границ зоны. */
static void
hyscan_gtk_map_planner_selection_extend (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *track;
  gchar **ids;
  gint i;

  ids = hyscan_planner_selection_get_tracks (priv->selection);
  if (ids == NULL)
    return;

  for (i = 0; ids[i] != NULL; i++)
    {
      track = g_hash_table_lookup (priv->tracks, ids[i]);
      if (track == NULL)
        continue;

      hyscan_gtk_map_planner_track_extend (planner, track);
    }

  g_strfreev (ids);
}

/* Расширяет галс до границ зоны. */
static void
hyscan_gtk_map_planner_track_extend (HyScanGtkMapPlanner           *planner,
                                     const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack *modified_track;
  HyScanGtkMapPlannerZone *zone;

  if (track->object->zone_id == NULL)
    return;

  zone = g_hash_table_lookup (priv->zones, track->object->zone_id);
  if (zone == NULL)
    return;

  modified_track = hyscan_planner_track_extend (track->object, zone->object);
  hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), track->id,
                                     (const HyScanObject *) modified_track);
  hyscan_planner_track_free (modified_track);
}

/* Удаляет указанную вершину. */
static void
hyscan_gtk_map_planner_vertex_remove (HyScanGtkMapPlanner     *planner,
                                      HyScanGtkMapPlannerZone *zone,
                                      GList                   *vertex)
{
  hyscan_gtk_map_point_free (vertex->data);
  zone->points = g_list_delete_link (zone->points, vertex);
  hyscan_gtk_map_planner_zone_save (planner, zone);
}

/* Удаляет вершину под курсором мыши. */
static void
hyscan_gtk_map_planner_vertex_remove_found (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerZone *zone;
  GList *vertex;

  if (priv->found_zone_id->len == 0)
    return;

  zone = g_hash_table_lookup (priv->zones, priv->found_zone_id->str);
  if (zone == NULL)
    return;

  vertex = g_list_nth (zone->points, priv->found_vertex - 1);
  if (vertex == NULL)
    return;

  hyscan_gtk_map_planner_vertex_remove (planner, zone, vertex);
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
hyscan_gtk_map_planner_parallel_close (HyScanGtkMapPlanner *planner)
{
  g_clear_object (&planner->priv->parallel_opts.shift);
  g_clear_pointer (&planner->priv->parallel_opts.track_id, g_free);
}

static void
hyscan_gtk_map_planner_parallel_save (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  hyscan_gtk_planner_shift_save (HYSCAN_GTK_PLANNER_SHIFT (priv->parallel_opts.shift),
                                 HYSCAN_OBJECT_MODEL (planner->priv->model));
}

/* Показывает виджет для добавления параллельных галсов. */
static void
hyscan_gtk_map_planner_parallel_window (HyScanGtkMapPlanner            *planner,
                                        const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerZone *zone;
  const gchar *zone_id;
  GtkWidget *window, *box, *planner_shift, *buttons;
  GtkWidget *toplevel;

  if (priv->parallel_opts.shift != NULL)
    return;

  zone_id = track->object->zone_id;
  zone = zone_id != NULL ? g_hash_table_lookup (priv->zones, zone_id) : NULL;

  /* Окно настроек. */
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), _("Add parallel tracks"));
    gtk_window_set_default_size (GTK_WINDOW (window), 450, -1);
    gtk_window_set_attached_to (GTK_WINDOW (window), GTK_WIDGET (priv->map));
    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->map));
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));
    g_signal_connect_swapped (window, "destroy", G_CALLBACK (hyscan_gtk_map_planner_parallel_close), planner);
  }

  /* Виджет параллельных галсов. */
  {
    planner_shift = hyscan_gtk_planner_shift_new ();
    hyscan_gtk_planner_shift_set_viewer (HYSCAN_GTK_PLANNER_SHIFT (planner_shift), planner);
    hyscan_gtk_planner_shift_set_track (HYSCAN_GTK_PLANNER_SHIFT (planner_shift),
                                        track->object, zone != NULL ? zone->object : NULL);
    g_object_set (planner_shift, "margin", 6, NULL);

    priv->parallel_opts.shift = g_object_ref (planner_shift);
    priv->parallel_opts.track_id = g_strdup (track->id);
  }

  /* Кнопки "Сохранить" и "Отмена". */
  {
    GtkWidget *save, *cancel;
    GtkSizeGroup *size;

    save = gtk_button_new_with_label (_("Save"));
    cancel = gtk_button_new_with_label (_("Cancel"));

    g_signal_connect_swapped (save,   "clicked", G_CALLBACK (hyscan_gtk_map_planner_parallel_save), planner);
    g_signal_connect_swapped (save,   "clicked", G_CALLBACK (gtk_widget_destroy), window);
    g_signal_connect_swapped (cancel, "clicked", G_CALLBACK (gtk_widget_destroy), window);

    size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    gtk_size_group_add_widget (size, save);
    gtk_size_group_add_widget (size, cancel);

    buttons = gtk_action_bar_new ();
    gtk_action_bar_pack_end (GTK_ACTION_BAR (buttons), cancel);
    gtk_action_bar_pack_end (GTK_ACTION_BAR (buttons), save);
  }

  /* Компонуем всё вместе. */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), planner_shift, TRUE, TRUE, 6);
  gtk_box_pack_start (GTK_BOX (box), buttons, TRUE, FALSE, 6);
  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_widget_show_all (window);
}

/* Обработчик выбора пункта контекстного меню. */
static void
hyscan_gtk_map_planner_menu_activate (GtkButton *button,
                                      gpointer   user_data)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (user_data);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (GTK_WIDGET (button) == priv->track_menu.inv && priv->track_menu.track != NULL)
    hyscan_gtk_map_planner_track_invert (planner, priv->track_menu.track);

  if (GTK_WIDGET (button) == priv->track_menu.num && priv->track_menu.track != NULL)
    hyscan_planner_model_assign_number (priv->model, priv->track_menu.track->id);

  else if (GTK_WIDGET (button) == priv->track_menu.del && priv->track_menu.track != NULL)
    hyscan_gtk_map_planner_track_remove (planner, priv->track_menu.track->id);

  else if (GTK_WIDGET (button) == priv->track_menu.nav && priv->track_menu.track != NULL)
    hyscan_planner_selection_activate (priv->selection, priv->track_menu.track->id);

  else if (GTK_WIDGET (button) == priv->track_menu.parallel && priv->track_menu.track != NULL)
    hyscan_gtk_map_planner_parallel_window (planner, priv->track_menu.track);

  else if (GTK_WIDGET (button) == priv->track_menu.extend && priv->track_menu.track != NULL)
    hyscan_gtk_map_planner_track_extend (planner, priv->track_menu.track);

  else if (GTK_WIDGET (button) == priv->zone_menu.del_vertex)
    hyscan_gtk_map_planner_vertex_remove_found (planner);

  else if (GTK_WIDGET (button) == priv->zone_menu.del_zone)
    hyscan_gtk_map_planner_zone_remove (planner, priv->found_zone_id->str);

  else if (GTK_WIDGET (button) == priv->group_menu.del)
    hyscan_gtk_map_planner_selection_remove (planner);

  else if (GTK_WIDGET (button) == priv->group_menu.extend)
    hyscan_gtk_map_planner_selection_extend (planner);
}

/* Добавляет пункт в контекстное меню. */
static GtkWidget *
hyscan_gtk_map_planner_menu_add (HyScanGtkMapPlanner *planner,
                                 GtkMenuShell        *menu,
                                 const gchar         *title)
{
  GtkWidget *menu_item;

  menu_item = gtk_menu_item_new_with_label (title);
  g_signal_connect (menu_item, "activate", G_CALLBACK (hyscan_gtk_map_planner_menu_activate), planner);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu, menu_item);

  return menu_item;
}

/* Открывет контекстное меню объекта под курсором. */
static void
hyscan_gtk_map_planner_handle_click_menu (HyScanGtkMapPlanner *planner,
                                          GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrack *found_track;

  /* Контекстное меню галса. */
  if (priv->found_track_id->len != 0)
    {
      found_track = g_hash_table_lookup (priv->tracks, priv->found_track_id->str);

      g_clear_pointer (&priv->track_menu.track, hyscan_gtk_map_planner_track_free);
      priv->track_menu.track = hyscan_gtk_map_planner_track_copy (found_track);
      gtk_widget_set_sensitive (priv->track_menu.extend, priv->track_menu.track->object->zone_id != NULL);
      gtk_menu_popup_at_pointer (priv->track_menu.menu, (const GdkEvent *) event);
    }

  /* Контекстное меню зоны. */
  else if (priv->found_state == STATE_ZONE_DRAG)
    {
      gtk_menu_popup_at_pointer (priv->zone_menu.menu, (const GdkEvent *) event);
    }

  /* Контекстное меню группы галсов. */
  else if (priv->found_state == STATE_GROUP_EDIT)
    {
      gtk_menu_popup_at_pointer (priv->group_menu.menu, (const GdkEvent *) event);
    }
}

/* Выбирает элемент под курсором мыши. */
static void
hyscan_gtk_map_planner_handle_click_select (HyScanGtkMapPlanner *planner,
                                            GdkEventButton      *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  const gchar *id = NULL;

  if (!(event->state & GDK_SHIFT_MASK))
    hyscan_planner_selection_remove_all (priv->selection);

  if (hyscan_gtk_map_planner_is_state_track (priv->found_state) && priv->found_track_id->len != 0)
    {
      id = priv->found_track_id->str;

      if (hyscan_planner_selection_contains (priv->selection, id))
        hyscan_planner_selection_remove (priv->selection, id);
      else
        hyscan_planner_selection_append (priv->selection, id);
    }

  else if (hyscan_gtk_map_planner_is_state_vertex (priv->found_state) && priv->found_zone_id->len != 0)
    {
      id = priv->found_zone_id->str;
      hyscan_planner_selection_set_zone (priv->selection, id, priv->found_vertex - 1);
    }

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static void
hyscan_gtk_map_planner_cancel (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  hyscan_gtk_map_planner_clear_cur_track (planner);
  hyscan_gtk_map_planner_group_clear_cur (planner);
  g_clear_pointer (&priv->cur_zone, hyscan_gtk_map_planner_zone_free);
  g_clear_pointer (&priv->cur_origin, hyscan_gtk_map_planner_origin_free);
  priv->cur_vertex = NULL;
  priv->cur_state = STATE_NONE;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
}

static void
hyscan_gtk_map_planner_selection_remove (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gint i;
  gchar **ids;

  ids = hyscan_planner_selection_get_tracks (priv->selection);
  if (ids == NULL)
    return;

  for (i = 0; ids[i] != NULL; ++i)
    hyscan_gtk_map_planner_track_remove (planner, ids[i]);

  hyscan_planner_selection_remove_all (priv->selection);
  g_strfreev (ids);
}

/* Обработка события "key-press-event". */
static gboolean
hyscan_gtk_map_planner_key_press (HyScanGtkMapPlanner *planner,
                                  GdkEventKey         *event)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (planner)))
    return GDK_EVENT_PROPAGATE;

  /* Отменяем текущее изменение. */
  if (priv->cur_state != STATE_NONE && (event->keyval == GDK_KEY_Escape))
    {
      hyscan_gtk_map_planner_cancel (planner);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_PROPAGATE;
    }

  /* Удаление объектов происходит только если слой активен, чтобы ничего случайно не удалить. */
  if (hyscan_gtk_layer_container_get_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map)) != planner)
    return GDK_EVENT_PROPAGATE;

  /* Удаляем активные или выбранные объекты. */
  if (event->keyval == GDK_KEY_Delete)
    {
      if (hyscan_gtk_map_planner_is_state_track (priv->cur_state))
        hyscan_gtk_map_planner_track_remove (planner, priv->cur_track->id);
      else if (hyscan_gtk_map_planner_is_state_vertex (priv->cur_state))
        hyscan_gtk_map_planner_vertex_remove (planner, priv->cur_zone, priv->cur_vertex);
      else
        hyscan_gtk_map_planner_selection_remove (planner);

      hyscan_gtk_map_planner_cancel (planner);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }

  return GDK_EVENT_PROPAGATE;
}

static inline void
hyscan_gtk_map_planner_cairo_style (cairo_t                  *cairo,
                                    HyScanGtkMapPlannerStyle *style)
{
  cairo_set_line_width (cairo, style->line_width);
  gdk_cairo_set_source_rgba (cairo, &style->color);
}

static void
hyscan_gtk_map_planner_group_draw (HyScanGtkMapPlanner *planner,
                                   cairo_t             *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerTrackEdit *track_edit;
  GHashTableIter iter;
  gdouble from_x, from_y;
  gdouble to_x, to_y;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &from_x, &from_y,
                                         priv->cur_group.handles[GROUP_HNDL_UL].x,
                                         priv->cur_group.handles[GROUP_HNDL_UL].y);
  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &to_x, &to_y,
                                         priv->cur_group.handles[GROUP_HNDL_BR].x,
                                         priv->cur_group.handles[GROUP_HNDL_BR].y);

  cairo_save (cairo);

  /* Рисуем галсы. */
  g_hash_table_iter_init (&iter, priv->cur_group.tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_edit))
    hyscan_gtk_map_planner_track_draw (planner, track_edit->current, FALSE, FALSE, cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->track_style_selected.color);
  cairo_set_line_width (cairo, priv->track_style_selected.line_width);

  /* Рисуем контур группы. */
  cairo_save (cairo);
  cairo_rectangle (cairo, from_x, from_y, to_x - from_x, to_y - from_y);
  cairo_set_dash (cairo, (gdouble []) {3.0}, 1, 0.0);
  cairo_stroke (cairo);
  cairo_restore (cairo);

  /* Риусем хэндлы. */
  gint i;
  for (i = 1; i < GROUP_HNDL_LAST; ++i)
    {
      gdouble radius;
      gdouble x, y;

      radius = planner->priv->cur_group.handle_hover == i ? HANDLE_HOVER_RADIUS : HANDLE_RADIUS;
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y,
                                             priv->cur_group.handles[i].x,
                                             priv->cur_group.handles[i].y);
      cairo_arc (cairo, x, y, radius, -G_PI, G_PI);
      cairo_stroke (cairo);
    }

  cairo_restore (cairo);
}

/* Рисует обозначение точки отсчёта. */
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

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, origin->origin.x, origin->origin.y);

  angle = origin->object->origin.h / 180.0 * G_PI;

  cairo_save (cairo);
  gdk_cairo_set_source_rgba (cairo, &priv->origin_style.color);
  cairo_set_line_width (cairo, priv->origin_style.line_width);

  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, angle - G_PI_2);

  cairo_new_path (cairo);
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

/* Рисует зону полигона и галсы внутри неё. */
static void
hyscan_gtk_map_planner_zone_draw (HyScanGtkMapPlanner     *planner,
                                  HyScanGtkMapPlannerZone *zone,
                                  gboolean                 skip_current,
                                  cairo_t                 *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;
  GList *link;
  gint vertex_num = 0;
  const gchar *hover_zone_id;
  gboolean hovered, hovered_vertex, hovered_section, highlighted_vertex;
  HyScanGtkMapPoint *point;
  HyScanGtkMapPlannerStyle *style;

  guint i = 0, j;
  gdouble x[2], y[2], x0 = 0, y0 = 0;
  gdouble highlight_x = 0, highlight_y = 0;

  /* Пропускаем активную зону. */
  if (skip_current && priv->cur_zone != NULL && g_strcmp0 (priv->cur_zone->id, zone->id) == 0)
    return;

  if (zone->points == NULL || zone->points->next == NULL)
    return;

  cairo_save (cairo);

  cairo_new_path (cairo);

  /* Определяем зону, над которой находится курсор - её надо подсветить. */
  if (priv->cur_track != NULL)
    hover_zone_id = priv->cur_track->object->zone_id;
  else if (g_hash_table_size (group->tracks) > 0 && group->one_zone)
    hover_zone_id = group->zone_id;
  else
    hover_zone_id = NULL;

  hovered = (g_strcmp0 (priv->hover_vertex.zone_id, zone->id) == 0);
  hovered_section = hovered && priv->hover_state == STATE_ZONE_INSERT;
  hovered_vertex = hovered && priv->hover_state == STATE_ZONE_DRAG;
  highlighted_vertex = (g_strcmp0 (priv->highlight_zone, zone->id) == 0 && priv->highlight_vertex >= 0);

  /* Хэндл над ребром зоны для создания новой вершины. */
  if (hovered_section)
    {
      gdouble edge_x, edge_y;

      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &edge_x, &edge_y,
                                     priv->hover_edge_point.x, priv->hover_edge_point.y);
      cairo_new_sub_path (cairo);
      cairo_arc (cairo, edge_x, edge_y, HANDLE_HOVER_RADIUS, -G_PI, G_PI);
    }

  for (link = zone->points, j = 0; link != NULL; link = link->next, j++)
    {
      gdouble radius;

      point = link->data;

      i = j % 2;
      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x[i], &y[i], point->c2d.x, point->c2d.y);
      if (j == 0)
        /* Запоминаем координаты первой вершины. */
        {
          x0 = x[i];
          y0 = y[i];
        }

      if (highlighted_vertex && (gint) j == priv->highlight_vertex)
        /* Запоминаем координаты подсвеченной вершины. */
        {
          highlight_x = x[i];
          highlight_y = y[i];
        }

      /* Рисуем вершину. */
      radius = (hovered_vertex && ++vertex_num == priv->hover_vertex.num) ? HANDLE_HOVER_RADIUS : HANDLE_RADIUS;
      cairo_new_sub_path (cairo);
      cairo_arc (cairo, x[i], y[i], radius, -G_PI, G_PI);

      /* Рисуем ребро к предыдущей вершине. */
      if (j != 0)
        hyscan_cairo_line_to (cairo, x[0], y[0], x[1], y[1]);
    }

  /* Замыкаем периметр - добавляем ребро от последней вершины к первой. */
  hyscan_cairo_line_to (cairo, x[i], y[i], x0, y0);
  if (zone->id != NULL && g_strcmp0 (hover_zone_id, zone->id) == 0)
    style = &priv->zone_style_hover;
  else
    style = &priv->zone_style;
  hyscan_gtk_map_planner_cairo_style (cairo, style);
  cairo_stroke (cairo);

  if (highlighted_vertex)
    {
      cairo_arc (cairo, highlight_x, highlight_y, 5.0, -G_PI, G_PI);
      cairo_fill (cairo);
    }

  cairo_restore (cairo);
}

static inline gboolean
hyscan_gtk_map_planner_track_is_grouped (HyScanGtkMapPlanner            *planner,
                                         const HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerGroup *group = &planner->priv->cur_group;

  return group->visible && track->id != NULL && g_strv_contains ((const gchar *const *) group->ids, track->id);
}

static gboolean
hyscan_gtk_map_planner_track_is_cur (HyScanGtkMapPlanner      *planner,
                                     HyScanGtkMapPlannerTrack *track)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;

  if (track->id == NULL)
    return FALSE;

  /* Проверяем отдельно редактируемый галс. */
  if (priv->cur_track != NULL && g_strcmp0 (priv->cur_track->id, track->id) == 0)
    return TRUE;

  if (!priv->cur_group.visible)
    return FALSE;

  /* Проверяем галсы внутри группы. */
  if (g_hash_table_contains (group->tracks, track->id))
    return TRUE;

  return FALSE;
}

/* Рисует галс. */
static void
hyscan_gtk_map_planner_track_draw (HyScanGtkMapPlanner      *planner,
                                   HyScanGtkMapPlannerTrack *track,
                                   gboolean                  skip_current,
                                   gboolean                  is_preview,
                                   cairo_t                  *cairo)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerStyle *style;
  gdouble start_x, start_y, mid_x, mid_y, end_x, end_y;
  gdouble start_size, end_size, mid_size;
  guint width, height;
  gdouble length;
  gboolean is_labeled;

  /* Пропускаем активный галс. */
  if (skip_current && hyscan_gtk_map_planner_track_is_cur (planner, track))
    return;

  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &start_x, &start_y, track->start.x, track->start.y);
  gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &end_x, &end_y, track->end.x, track->end.y);
  length = hypot (start_x - end_x, start_y - end_y);

  /* Пропускаем галсы, которые плохо видно на текущем масштабе. */
  if (length < HANDLE_HOVER_RADIUS)
    return;

  mid_x = (start_x + end_x) / 2.0;
  mid_y = (start_y + end_y) / 2.0;

  if (hyscan_planner_selection_contains (priv->selection, track->id))
    style = &priv->track_style_selected;
  else if (priv->active_track_id != NULL && g_strcmp0 (priv->active_track_id, track->id) == 0)
    style = &priv->track_style_active;
  else
    style = &priv->track_style;
  hyscan_gtk_map_planner_cairo_style (cairo, style);

  /* Определяем размеры хэндлов, с поправкой на их состояние и установленную толщину линий. */
  start_size = mid_size = end_size = HANDLE_RADIUS + style->line_width;
  if (priv->hover_track_id != NULL && g_strcmp0 (priv->hover_track_id, track->id) == 0)
    {
      if (priv->hover_state == STATE_DRAG_START)
        start_size = HANDLE_HOVER_RADIUS + style->line_width;
      else if (priv->hover_state == STATE_DRAG_MIDDLE)
        mid_size = HANDLE_HOVER_RADIUS + style->line_width;
      else if (priv->hover_state == STATE_DRAG_END)
        end_size = HANDLE_HOVER_RADIUS + style->line_width;
    }

  /* Определяем наличие подписи. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->map), &width, &height);
  is_labeled = track->object->number > 0 &&                                     /* (1) имеет номер, */
               length > 40.0 &&                                                 /* (2) достаточно крупный, */
               (0 < mid_x && mid_x < width) && (0 < mid_y && mid_y < height);   /* (3) попадает в видимую область. */

  /* Переходим в СК, где (0, 0) совпадает с началом галса, а направление оси Y в сторону конца галса. */
  cairo_save (cairo);
  cairo_translate (cairo, start_x, start_y);
  cairo_rotate (cairo, atan2 (start_x - end_x, end_y - start_y));

  /* Точка в начале галса. */
  cairo_move_to (cairo, -start_size, style->line_width / 2.0);
  cairo_line_to (cairo,  start_size, style->line_width / 2.0);
  cairo_stroke (cairo);

  /* Линия галса. */
  cairo_move_to (cairo, 0, length);
  cairo_line_to (cairo, -end_size, length - 2 * end_size);
  cairo_line_to (cairo,  end_size, length - 2 * end_size);
  cairo_close_path (cairo);
  cairo_fill (cairo);

  /* Стрелка в конце галса. */
  cairo_move_to (cairo, 0, 0);
  cairo_line_to (cairo, 0, length - end_size);
  if (is_preview)
    cairo_set_dash (cairo, (gdouble[]) {3.0}, 1, 0);
  cairo_stroke (cairo);

  cairo_restore (cairo);

  /* Подпись галса в качестве хэндла. */
  if (is_labeled)
    {
      gchar label[16];
      gint label_width, label_height;

      g_snprintf (label, sizeof (label), "%u", track->object->number);
      pango_layout_set_text (priv->pango_layout, label, -1);
      pango_layout_get_pixel_size (priv->pango_layout, &label_width, &label_height);
      cairo_rectangle (cairo, mid_x - label_width / 2, mid_y - label_height / 2, label_width, label_height);
      if (mid_size == HANDLE_HOVER_RADIUS)
        cairo_stroke_preserve (cairo);
      cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);
      cairo_fill (cairo);

      cairo_move_to (cairo, mid_x - label_width / 2, mid_y - label_height / 2);
      hyscan_gtk_map_planner_cairo_style (cairo, style);
      pango_cairo_show_layout (cairo, priv->pango_layout);
    }

  /* Хэндл в центре галса, если нет подписи. */
  else if (!is_preview && !hyscan_gtk_map_planner_track_is_grouped (planner, track))
    {
      cairo_new_path (cairo);
      cairo_arc (cairo, mid_x, mid_y, mid_size, -G_PI, G_PI);
      cairo_stroke (cairo);
    }
}

/* Находит хэндл галса в точке x, y. */
static gboolean
hyscan_gtk_map_planner_handle_find_track (HyScanGtkMapPlanner  *planner,
                                          gdouble               x,
                                          gdouble               y,
                                          HyScanGeoCartesian2D *handle_coord)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble scale_x, scale_y;
  gdouble max_x, max_y;

  GHashTableIter iter;
  HyScanGtkMapPlannerTrack *track = NULL;
  gdouble handle_x = 0.0, handle_y = 0.0;
  gboolean handle_found = FALSE;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, &scale_y);
  max_x = HANDLE_HOVER_RADIUS * scale_x;
  max_y = HANDLE_HOVER_RADIUS * scale_y;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      if (track->disabled)
        continue;

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
      g_string_insert (priv->found_track_id, 0, track->id);
    }

  return handle_found;
}

/* Находит хэндл группы выбранных галсов в точке x, y. */
static gboolean
hyscan_gtk_map_planner_handle_find_group (HyScanGtkMapPlanner  *planner,
                                          gdouble               x,
                                          gdouble               y,
                                          HyScanGeoCartesian2D *handle_coord)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gdouble max_x;
  gdouble scale_x;
  HyScanGeoCartesian2D cursor = {.x = x, .y = y};

  if (!priv->cur_group.visible)
    return FALSE;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, NULL);
  max_x = HANDLE_HOVER_RADIUS * scale_x;

  gint i;
  for (i = 1; i < GROUP_HNDL_LAST; ++i)
    {
      gdouble dist;

      dist = hyscan_cartesian_distance (&priv->cur_group.handles[i], &cursor);
      if (dist < max_x)
        {
          priv->cur_group.handle_found = i;

          *handle_coord = priv->cur_group.handles[i];
          priv->found_state = STATE_GROUP_EDIT;

          return TRUE;
        }
    }

  return FALSE;
}

/* Находит хэндл границы полигона в точке x, y. */
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
  gdouble scale_x;
  gdouble max_x;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale_x, NULL);
  max_x = HANDLE_HOVER_RADIUS * scale_x;

  /* Проверяем расстояния до вершин. */
  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &zone))
    {
      gint vertex_num = 0;

      if (zone->disabled)
        continue;

      for (list = zone->points; list != NULL; list = list->next)
        {
          gdouble distance;
          point = list->data;

          vertex_num++;

          distance = hyscan_cartesian_distance (&point->c2d, &cursor);
          if (distance < max_x)
            {
              handle_coord->x = point->c2d.x;
              handle_coord->y = point->c2d.y;
              g_string_insert (priv->found_zone_id, 0, zone->id);
              priv->found_vertex = vertex_num;
              priv->found_state = STATE_ZONE_DRAG;

              return TRUE;
            }
        }
    }

  /* Проверяем расстояния до отрезков. */
  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &zone))
    {
      gint vertex_num = 0;

      if (zone->disabled)
        continue;

      for (list = zone->points; list != NULL; list = list->next)
        {
          gdouble distance;
          point = list->data;

          vertex_num++;

          next_point = list->next != NULL ? list->next->data : zone->points->data;
          distance = hyscan_cartesian_distance_to_line (&point->c2d, &next_point->c2d, &cursor, &edge_point);
          if (distance < max_x &&
              IS_INSIDE (edge_point.x, point->c2d.x, next_point->c2d.x) &&
              IS_INSIDE (edge_point.y, point->c2d.y, next_point->c2d.y))
            {
              handle_coord->x = edge_point.x;
              handle_coord->y = edge_point.y;
              g_string_insert (priv->found_zone_id, 0, zone->id);
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
  HyScanPlannerTrack *track;

  track = hyscan_planner_track_new ();
  track->plan.velocity = 1.0;

  return track;
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

static void
hyscan_gtk_map_planner_clear_cur_track (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  g_clear_pointer (&priv->cur_track, hyscan_gtk_map_planner_track_free);
  priv->cur_track = NULL;

  g_signal_emit (planner, hyscan_gtk_map_planner_signals[SIGNAL_TRACK_CHANGED], 0, NULL);
}

/* Отменяет редактирование группы галсов. */
static void
hyscan_gtk_map_planner_group_clear_cur (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;

  group->handle_active = GROUP_HNDL_NONE;
  g_clear_pointer (&priv->cur_group.zone_id, g_free);
  g_hash_table_remove_all (group->tracks);

  hyscan_gtk_map_planner_group_boundary (planner);
}

/* Устанавливает текущую выбранную группу. */
static void
hyscan_gtk_map_planner_set_cur_group (HyScanGtkMapPlanner  *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;
  HyScanGtkMapPlannerTrack *track;
  HyScanGtkMapPlannerTrackEdit *track_edit;
  gint i;

  if (priv->cur_state != STATE_GROUP_EDIT)
    return;

  /* Запоминаем захваченный хэндл. */
  group->handle_active = group->handle_found;
  group->orig_handle = group->handles[group->handle_active];
  group->orig_center = group->handles[GROUP_HNDL_C];

  /* Копируем галсы на момент начала редактирования. */
  g_hash_table_remove_all (group->tracks);
  g_clear_pointer (&group->zone_id, g_free);
  group->one_zone = TRUE;
  for (i = 0; group->ids[i] != NULL; i++)
    {
      track = g_hash_table_lookup (priv->tracks, group->ids[i]);
      if (track == NULL)
        continue;

      track_edit = hyscan_gtk_map_planner_track_edit_new ();
      track_edit->original = hyscan_gtk_map_planner_track_copy (track);
      track_edit->current = hyscan_gtk_map_planner_track_copy (track);

      g_hash_table_replace (group->tracks, track_edit->original->id, track_edit);

      if (group->zone_id == NULL && track->object->zone_id != NULL)
        group->zone_id = g_strdup (track->object->zone_id);
      group->one_zone = group->one_zone && (g_strcmp0 (group->zone_id, track->object->zone_id) == 0);
    }
}

/* Устанавливает текущую выбранную зону и вершину на ней. */
static void
hyscan_gtk_map_planner_set_cur_zone (HyScanGtkMapPlanner      *planner,
                                     HyScanGtkMapPlannerZone  *zone,
                                     gint                      vertex)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  priv->cur_zone = zone;
  priv->cur_vertex = vertex >= 0 ? g_list_nth (priv->cur_zone->points, (guint) vertex) : NULL;

  if (priv->cur_state == STATE_ZONE_DRAG)
    hyscan_planner_selection_set_zone (priv->selection, priv->cur_zone->id, vertex);
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
  track_proj->object->zone_id = g_strdup (hyscan_gtk_map_planner_zone_find (planner, &point));
  track_proj->start = point;
  track_proj->end = point;

  hyscan_gtk_map_value_to_geo (priv->map, &track_proj->object->plan.start, track_proj->start);
  track_proj->object->plan.end = track_proj->object->plan.start;

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
static gint
hyscan_gtk_map_planner_select_mode (HyScanGtkLayer        *layer,
                                    HyScanGtkLayerSelMode  mode)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gint i;

  /* Формируем таблицу из ИД объектов, к которым добавляется выбор. */
  g_hash_table_remove_all (priv->selection_keep);

  i = 0;
  if (mode == HYSCAN_GTK_LAYER_SEL_MODE_ADD)
    {
      gchar **ids;

      ids = hyscan_planner_selection_get_tracks (priv->selection);
      for (; ids[i] != NULL; i++)
        g_hash_table_add (priv->selection_keep, ids[i]);
    }
  else
    {
      hyscan_planner_selection_remove_all (priv->selection);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }

  return i;
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
  priv->cur_group.handle_found = GROUP_HNDL_NONE;
  g_string_truncate (priv->found_track_id, 0);
  g_string_truncate (priv->found_zone_id, 0);
  priv->found_vertex = 0;

  if (hyscan_gtk_map_planner_handle_find_group (planner, x, y, &handle_coord) ||
      hyscan_gtk_map_planner_handle_find_track (planner, x, y, &handle_coord) ||
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

  if (event->button == GDK_BUTTON_SECONDARY)
    hyscan_gtk_map_planner_handle_click_menu (planner, event);

  if (event->button == GDK_BUTTON_PRIMARY)
    hyscan_gtk_map_planner_handle_click_select (planner, event);
}

static void
hyscan_gtk_map_planner_handle_drag (HyScanGtkLayer       *layer,
                                    GdkEventButton       *event,
                                    HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanGtkMapPlannerZone *found_zone;
  HyScanGtkMapPlannerTrack *found_track;

  g_return_if_fail (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0);

  /* В момент захвата хэндла мы предполагаем, что текущий хэндл не выбран. Иначе это ошибка в логике программы. */
  g_assert (priv->cur_track == NULL && priv->cur_zone == NULL);

  found_track = priv->found_track_id->len != 0 ? g_hash_table_lookup (priv->tracks, priv->found_track_id->str) : NULL;
  if (hyscan_gtk_map_planner_is_state_track (priv->found_state) &&
      hyscan_gtk_map_planner_track_is_grouped (planner, found_track))
    {
      return;
    }

  priv->cur_state = priv->found_state;
  hyscan_gtk_map_planner_set_cur_track (planner, hyscan_gtk_map_planner_track_copy (found_track));
  found_zone = priv->found_zone_id->len > 0 ? g_hash_table_lookup (priv->zones, priv->found_zone_id->str) : NULL;
  hyscan_gtk_map_planner_set_cur_zone (planner, hyscan_gtk_map_planner_zone_copy (found_zone), priv->found_vertex - 1);
  hyscan_gtk_map_planner_set_cur_group (planner);

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

  if (handle != NULL && priv->found_track_id->len != 0)
    {
      priv->hover_state = priv->found_state;
      hover_id = priv->found_track_id->str;

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
hyscan_gtk_map_planner_handle_show_group (HyScanGtkLayer       *layer,
                                          HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  gboolean state_set;
  gint new_handle;

  if (handle != NULL && priv->cur_group.handle_found != GROUP_HNDL_NONE)
    {
      priv->hover_state = priv->found_state;
      new_handle = priv->cur_group.handle_found;
      state_set = TRUE;
    }
  else
    {
      new_handle = GROUP_HNDL_NONE;
      state_set = FALSE;
    }

  if (priv->cur_group.handle_hover != priv->cur_group.handle_found)
    {
      priv->cur_group.handle_hover = new_handle;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
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
  if (handle != NULL && priv->found_zone_id->len != 0)
    {
      hover_id = priv->found_zone_id->str;
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
    zone_changed = (g_strcmp0 (priv->hover_vertex.zone_id, hover_id) != 0);
    vertex_changed = (hover_vertex != priv->hover_vertex.num);
    if (prev_state != priv->hover_state || zone_changed || vertex_changed || priv->hover_state == STATE_ZONE_INSERT)
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
  }

  priv->hover_vertex.num = hover_vertex;
  if (zone_changed)
    {
      g_free (priv->hover_vertex.zone_id);
      priv->hover_vertex.zone_id = g_strdup (hover_id);
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

  if (hyscan_gtk_map_planner_handle_show_group (layer, handle))
    state_set = TRUE;

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

  return FALSE;
}

static void
hyscan_gtk_map_planner_zone_remove (HyScanGtkMapPlanner *planner,
                                    const gchar         *zone_id)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (priv->model), zone_id);
  g_hash_table_remove (priv->zones, zone_id);
}

/* Сохраняет текущую зону в БД. */
static void
hyscan_gtk_map_planner_zone_save (HyScanGtkMapPlanner     *planner,
                                  HyScanGtkMapPlannerZone *pzone)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  HyScanPlannerZone *zone;

  /* Завершаем редактирование зоны. */
  zone = pzone->object;
  zone->points_len = g_list_length (pzone->points);
  if (zone->points_len > 2)
    {
      HyScanGtkMapPlannerZone *zone_copy;
      GList *link;
      gint i = 0;

      g_free (zone->points);
      zone->points = g_new (HyScanGeoGeodetic, zone->points_len);
      for (link = pzone->points; link != NULL; link = link->next)
        {
          HyScanGtkMapPoint *point = link->data;

          zone->points[i++] = point->geo;
        }

      /* Обновляем параметры зоны в БД. */
      if (pzone->id != NULL)
        hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), pzone->id, (HyScanObject *) zone);
      else
        hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (priv->model), (HyScanObject *) zone);

      /* Обновляем внутреннее состояние, пока не пришло уведомление из БД. */
      zone_copy = hyscan_gtk_map_planner_zone_copy (pzone);
      zone_copy->disabled = TRUE;
      g_hash_table_replace (priv->zones, pzone->id != NULL ? pzone->id : "new-zone", zone_copy);
    }
  else if (pzone->id != NULL)
    {
      hyscan_gtk_map_planner_zone_remove (planner, pzone->id);
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
  HyScanGtkMapPlannerTrack *track_copy;
  HyScanObject *object;

  g_return_if_fail (cur_track != NULL);
  object = (HyScanObject *) cur_track->object;

  if (priv->cur_track->id == NULL)
    hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (priv->model), object);
  else
    hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), cur_track->id, object);

  /* Обновляем внутреннюю таблицу галсов. */
  track_copy = hyscan_gtk_map_planner_track_copy (cur_track);
  track_copy->disabled = TRUE;
  g_hash_table_replace (priv->tracks, cur_track->id != NULL ? cur_track->id : "new-track", track_copy);
}

static gboolean
hyscan_gtk_map_planner_release_track (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  hyscan_gtk_map_planner_save_current (planner);

  priv->cur_state = STATE_NONE;
  hyscan_gtk_map_planner_clear_cur_track (planner);

  return TRUE;
}

static gboolean
hyscan_gtk_map_planner_release_group (HyScanGtkMapPlanner *planner)
{
  HyScanGtkMapPlannerPrivate *priv = planner->priv;
  GHashTableIter iter;
  HyScanGtkMapPlannerTrackEdit *track_edit;
  HyScanGtkMapPlannerTrack *track;
  HyScanGtkMapPlannerGroup *group = &priv->cur_group;

  priv->cur_state = STATE_NONE;
  g_hash_table_iter_init (&iter, group->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_edit))
    {
      track = hyscan_gtk_map_planner_track_copy (track_edit->current);
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), track->id,
                                         (const HyScanObject *) track->object);

      /* Обновляем внутренний список галсов. */
      track->disabled = TRUE;
      g_hash_table_replace (priv->tracks, track->id, track);
    }

  hyscan_gtk_map_planner_group_clear_cur (planner);

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
  else if (priv->cur_state == STATE_GROUP_EDIT)
    return hyscan_gtk_map_planner_release_group (planner);
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

  gtk_menu_attach_to_widget (priv->track_menu.menu, GTK_WIDGET (priv->map), NULL);
  gtk_menu_attach_to_widget (priv->group_menu.menu, GTK_WIDGET (priv->map), NULL);

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

static HyScanParam *
hyscan_gtk_map_planner_get_param (HyScanGtkLayer *layer)
{
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (layer);
  HyScanGtkMapPlannerPrivate *priv = planner->priv;

  return g_object_ref (priv->param);
}

static void
hyscan_gtk_map_planner_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->handle_find = hyscan_gtk_map_planner_handle_find;
  iface->handle_click = hyscan_gtk_map_planner_handle_click;
  iface->handle_drag = hyscan_gtk_map_planner_handle_drag;
  iface->handle_release = hyscan_gtk_map_planner_handle_release;
  iface->handle_create = hyscan_gtk_map_planner_handle_create;
  iface->handle_show = hyscan_gtk_map_planner_handle_show;
  iface->select_area = hyscan_gtk_map_planner_select_change;
  iface->select_mode = hyscan_gtk_map_planner_select_mode;
  iface->set_visible = hyscan_gtk_map_planner_set_visible;
  iface->get_visible = hyscan_gtk_map_planner_get_visible;
  iface->grab_input = hyscan_gtk_map_planner_grab_input;
  iface->added = hyscan_gtk_map_planner_added;
  iface->removed = hyscan_gtk_map_planner_removed;
  iface->get_param = hyscan_gtk_map_planner_get_param;
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
hyscan_gtk_map_planner_new (HyScanPlannerModel     *model,
                            HyScanPlannerSelection *selection)
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
 * hyscan_gtk_map_planner_set_preview:
 * @planner: указатель на HyScanGtkMapPlanner
 * @tracks: (element-type HyScanPlannerTrack): список галсов для предпросмотра
 *
 * Устанавливает галсы для предпросмотра
 */
void
hyscan_gtk_map_planner_set_preview (HyScanGtkMapPlanner *planner,
                                    GList               *tracks)
{
  HyScanGtkMapPlannerPrivate *priv;
  GList *link;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PLANNER (planner));
  priv = planner->priv;

  if (tracks == NULL && priv->preview_tracks == NULL)
    return;

  g_list_free_full (priv->preview_tracks, (GDestroyNotify) hyscan_gtk_map_planner_track_free);
  priv->preview_tracks = NULL;
  for (link = tracks; link != NULL; link = link->next)
    {
      HyScanGtkMapPlannerTrack *track;

      track = hyscan_gtk_map_planner_track_create ();
      track->object = hyscan_planner_track_copy (link->data);
      hyscan_gtk_map_planner_track_project (planner, track);
      priv->preview_tracks = g_list_append (priv->preview_tracks, track);
    }

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

/**
 * hyscan_gtk_map_planner_track_view:
 * @planner: указатель на #HyScanGtkMapPlanner
 * @track_id: идентификатор галса
 * @zoom_in: признак того, что надо также изменить масштаб
 *
 * Перемещает видимую область карты, чтобы галс с идентификатором @track_id
 * находился в её центре. Если @zoom_id равно %TRUE, то также будет установлен
 * максимальный масштаб, при котором галс помещается в выбранную область.
 */
void
hyscan_gtk_map_planner_track_view (HyScanGtkMapPlanner *planner,
                                   const gchar         *track_id,
                                   gboolean             zoom_in)
{
  HyScanGtkMapPlannerPrivate *priv;
  HyScanGtkMapPlannerTrack *track;
  gdouble prev_scale;
  gdouble from_x, to_x, from_y, to_y;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PLANNER (planner));
  priv = planner->priv;

  if (priv->map == NULL)
    return;

  track = g_hash_table_lookup (priv->tracks, track_id);
  if (track == NULL)
    return;

  from_x = MIN (track->start.x, track->end.x);
  to_x = MAX (track->start.x, track->end.x);
  from_y = MIN (track->start.y, track->end.y);
  to_y = MAX (track->start.y, track->end.y);

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &prev_scale, NULL);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map), from_x, to_x, from_y, to_y);

  /* Возвращаем прежний масштаб. */
  if (!zoom_in)
    {
      gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), prev_scale, prev_scale,
                                (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
    }
}
