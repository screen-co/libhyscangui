#include "hyscan-gtk-map-builder.h"
#include "hyscan-gtk-map-profile-switch.h"
#include "hyscan-gtk-layer-list.h"
#include "hyscan-gtk-map-track-list.h"
#include "hyscan-gtk-map-mark-list.h"
#include "hyscan-gtk-planner-status.h"
#include "hyscan-gtk-planner-origin.h"
#include "hyscan-gtk-planner-list.h"
#include "hyscan-gtk-planner-list-bar.h"
#include "hyscan-gtk-planner-editor.h"
#include "hyscan-gtk-planner-zeditor.h"
#include "hyscan-gtk-planner-import.h"
#include "hyscan-gtk-map-direction.h"
#include "hyscan-gtk-map-steer.h"

#include "hyscan-gtk-map-base.h"
#include "hyscan-gtk-map-control.h"
#include "hyscan-gtk-map-scale.h"
#include "hyscan-gtk-map-grid.h"
#include "hyscan-gtk-map-ruler.h"
#include "hyscan-gtk-map-track.h"
#include "hyscan-gtk-map-nav.h"
#include "hyscan-gtk-map-wfmark.h"
#include "hyscan-gtk-map-geomark.h"
#include "hyscan-gtk-map-planner.h"

#include <glib/gi18n-lib.h>
#include <hyscan-nav-model.h>
#include <hyscan-steer.h>
#include <hyscan-track-stats.h>
#include <hyscan-planner-stats.h>
#include <hyscan-gtk-paned.h>

#define PLANNER_TAB_ZEDITOR   "zeditor"
#define PLANNER_TAB_ORIGIN    "origin"
#define PLANNER_TAB_STATUS    "status"

#define LAYER_BASE     "base"
#define LAYER_RULER    "ruler"
#define LAYER_PIN      "pin"
#define LAYER_GRID     "grid"
#define LAYER_WFMARK   "wfmark"
#define LAYER_PLANNER  "planner"
#define LAYER_GEOMARK  "geomark"
#define LAYER_TRACK    "track"
#define LAYER_NAV      "nav"

#define PLANNER_TOOLS_MODE    "planner-mode"

enum
{
  PROP_O,
  PROP_MODEL_MANAGER,
  PROP_OFFLINE,
};

struct _HyScanGtkMapBuilderPrivate
{
  gchar                  *project_name;        /* Название текущего проекта. */

  /* Модели. */
  HyScanGtkMap           *map;                 /* Карта. */
  HyScanGtkModelManager  *model_manager;       /* Сервис локатор моделей.*/
  HyScanUnits            *units;               /* Единицы измерения. */
  HyScanCache            *cache;               /* Кэш. */
  HyScanNavModel         *nav_model;           /* Навигационные данные. */
  HyScanPlannerSelection *planner_selection;   /* Выбранные объекты планировщика. */
  HyScanPlannerModel     *planner_model;       /* Модель объектов планировщика. */
  HyScanDBInfo           *db_info;             /* Галсы БД. */
  HyScanSteer            *steer;               /* Модель навигации по выбранному плану. */
  HyScanObjectModel      *mark_geo_model;      /* Геометки. */
  HyScanMarkLocModel     *ml_model;            /* Местопололжение акустических меток. */
  HyScanMapTrackModel    *track_model;         /* Выбранные галсы карты и их параметры. */
  HyScanDB               *db;                  /* База данных. */

  /* Виджеты. */
  GtkWidget              *mark_list;           /* Список меток. */
  GtkWidget              *track_list;          /* Список галсов. */
  GtkWidget              *planner_list;        /* Список объектов планировщика. */
  GtkWidget              *planner_list_wrap;   /* Контейнер виджетов для редактирования объектов планировщика. */
  GtkWidget              *planner_stack;       /* Стек кнопок переключения режима слоя планирования. */
  GtkWidget              *locate_button;       /* Кнопка перехода к текущему положению на карте. */
  GtkWidget              *steer_box;           /* Контейнер виджетов навигации по плану. */
  GtkWidget              *profile_switch;      /* Виджет переключения и редактирования профилей карты. */
  GtkWidget              *statusbar;           /* Статусная строка. */
  GtkWidget              *stbar_offline;       /* Статус "оффлайн" в статусной строке. */
  GtkWidget              *stbar_coord;         /* Статус координат под курсором мыши в статусной строке.*/
  GtkWidget              *left_col;            /* Левая колонка. */
  GtkWidget              *right_col;           /* Правая колонка с инструментами.*/
  GtkWidget              *paned;               /* Контейнер с картой и списком объектов планировщика. */
  GtkWidget              *layer_list;          /* Виджет списка слоёв.*/

  /* Слои. */
  HyScanGtkMapPlanner    *layer_planner;       /* Слой планирования.*/
  HyScanGtkMapWfmark     *wfmark_layer;        /* Слой акустических меток.*/
  HyScanGtkMapGeomark    *geomark_layer;       /* Слой геометок.*/
  HyScanGtkMapTrack      *track_layer;         /* Слой галсов. */
};

static void        hyscan_gtk_map_builder_set_property             (GObject                   *object,
                                                                    guint                      prop_id,
                                                                    const GValue              *value,
                                                                    GParamSpec                *pspec);
static void        hyscan_gtk_map_builder_get_property             (GObject                   *object,
                                                                    guint                      prop_id,
                                                                    GValue                    *value,
                                                                    GParamSpec                *pspec);
static void        hyscan_gtk_map_builder_object_constructed       (GObject                   *object);
static void        hyscan_gtk_map_builder_object_finalize          (GObject                   *object);
static GtkWidget * hyscan_gtk_map_builder_grid_toolbox             (HyScanGtkMapBuilder       *builder);
static GtkWidget * hyscan_gtk_map_builder_pin_toolbox              (HyScanGtkLayer            *layer,
                                                                    const gchar               *label);
static GtkWidget * hyscan_gtk_map_builder_create_tools             (HyScanGtkMapBuilder       *builder);
static GtkWidget * hyscan_gtk_map_builder_create_stbar             (HyScanGtkMapBuilder       *builder);
static GtkWidget * hyscan_gtk_map_builder_planner_tools            (HyScanGtkMapBuilder       *builder);
static void        hyscan_gtk_map_builder_offline_toggle           (HyScanGtkMapProfileSwitch *profile_switch,
                                                                    GParamSpec                *pspec,
                                                                    GtkWidget                 *statusbar);
static gboolean    hyscan_gtk_map_builder_map_motion               (HyScanGtkMap              *map,
                                                                    GdkEventMotion            *event,
                                                                    HyScanGtkMapBuilder       *builder);
static void        hyscan_gtk_map_builder_scale                    (HyScanGtkMap              *map,
                                                                    gint                       steps);
static void        hyscan_gtk_map_builder_scale_down               (HyScanGtkMap              *map);
static void        hyscan_gtk_map_builder_scale_up                 (HyScanGtkMap              *map);
static void        hyscan_gtk_map_builder_units_changed            (GtkToggleButton           *button,
                                                                    GParamSpec                *pspec,
                                                                    HyScanGtkMapBuilder       *builder);
static void        hyscan_gtk_map_builder_track_draw_change        (GtkToggleButton           *button,
                                                                    GParamSpec                *pspec,
                                                                    HyScanGtkMapTrack         *track_layer);
static void        hyscan_gtk_map_builder_project_changed          (HyScanGtkMapBuilder       *builder);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapBuilder, hyscan_gtk_map_builder, G_TYPE_OBJECT)

static void
hyscan_gtk_map_builder_class_init (HyScanGtkMapBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_builder_set_property;
  object_class->get_property = hyscan_gtk_map_builder_get_property;

  object_class->constructed = hyscan_gtk_map_builder_object_constructed;
  object_class->finalize = hyscan_gtk_map_builder_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL_MANAGER,
    g_param_spec_object ("model-manager", "HyScanGtkModelManager", "Service locator", HYSCAN_TYPE_GTK_MODEL_MANAGER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_OFFLINE,
    g_param_spec_boolean ("offline", "Map Offline Mode", "Do not download maps", FALSE,
                          G_PARAM_READWRITE));
}

static void
hyscan_gtk_map_builder_init (HyScanGtkMapBuilder *gtk_map_builder)
{
  gtk_map_builder->priv = hyscan_gtk_map_builder_get_instance_private (gtk_map_builder);
}

static void
hyscan_gtk_map_builder_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkMapBuilder *gtk_map_builder = HYSCAN_GTK_MAP_BUILDER (object);
  HyScanGtkMapBuilderPrivate *priv = gtk_map_builder->priv;

  switch (prop_id)
    {
    case PROP_MODEL_MANAGER:
      priv->model_manager = g_value_dup_object (value);
      break;

    case PROP_OFFLINE:
      hyscan_gtk_map_profile_switch_set_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH(priv->profile_switch),
                                                 g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }


}static void
hyscan_gtk_map_builder_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  HyScanGtkMapBuilder *gtk_map_builder = HYSCAN_GTK_MAP_BUILDER (object);
  HyScanGtkMapBuilderPrivate *priv = gtk_map_builder->priv;

  switch (prop_id)
    {
    case PROP_OFFLINE:
      g_value_set_boolean (value,
                           hyscan_gtk_map_profile_switch_get_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_builder_object_constructed (GObject *object)
{
  HyScanGtkMapBuilder *builder = HYSCAN_GTK_MAP_BUILDER (object);
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  HyScanGtkLayer *base_layer;

  HyScanGeoPoint center = { 0 };
  gdouble *scales;
  gint scales_len;
  HyScanGtkLayer *control, *scale;

  G_OBJECT_CLASS (hyscan_gtk_map_builder_parent_class)->constructed (object);

  priv->cache = hyscan_gtk_model_manager_get_cache (priv->model_manager);
  priv->db = hyscan_gtk_model_manager_get_db (priv->model_manager);
  priv->db_info = hyscan_gtk_model_manager_get_track_model (priv->model_manager);
  priv->track_model = hyscan_map_track_model_new (priv->db, priv->cache);
  priv->project_name = g_strdup (hyscan_gtk_model_manager_get_project_name (priv->model_manager));
  priv->units = hyscan_gtk_model_manager_get_units (priv->model_manager);
  priv->mark_geo_model = hyscan_gtk_model_manager_get_geo_mark_model (priv->model_manager),
  priv->ml_model = hyscan_gtk_model_manager_get_acoustic_mark_loc_model (priv->model_manager);

  hyscan_map_track_model_set_project (priv->track_model, priv->project_name);

  /* Создаём виджет карты. */
  priv->map = HYSCAN_GTK_MAP (hyscan_gtk_map_new (center));

  /* Чтобы виджет карты занял всё доступное место. */
  gtk_widget_set_hexpand (GTK_WIDGET (priv->map), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (priv->map), TRUE);

  /* Устанавливаем допустимые масштабы. */
  scales = hyscan_gtk_map_create_scales2 (1.0 / 1000, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
  hyscan_gtk_map_set_scales_meter (priv->map, scales, scales_len);
  g_free (scales);

  /* По умолчанию добавляем слой управления картой и масштаб. */
  control = hyscan_gtk_map_control_new ();
  scale = hyscan_gtk_map_scale_new ();
  hyscan_gtk_layer_set_visible (scale, TRUE);
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (priv->map), control, "control");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (priv->map), scale,   "scale");

  /* Создаём виджет списка слоёв. */
  priv->layer_list = hyscan_gtk_layer_list_new (HYSCAN_GTK_LAYER_CONTAINER (priv->map));

  /* Прочие виджеты. */
  priv->statusbar = hyscan_gtk_map_builder_create_stbar (builder);
  priv->right_col = hyscan_gtk_map_builder_create_tools (builder);
  priv->left_col = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_end (priv->left_col, 6);

  /* Слой подложки. */
  base_layer = hyscan_gtk_map_base_new (priv->cache);
  hyscan_gtk_map_builder_add_layer (builder, base_layer, FALSE, LAYER_BASE,  _("Base Map"));

  /* Связываем используемую проекцию на карте и в модели галсов карты.
   * todo: надо создать модель карты в libhyscanmodel, которая бы хранила текущую проекцию. */
  g_object_bind_property (priv->map, "projection", priv->track_model, "projection", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (priv->model_manager,
                            "notify::project-name",
                            G_CALLBACK (hyscan_gtk_map_builder_project_changed),
                            builder);
}

static void
hyscan_gtk_map_builder_object_finalize (GObject *object)
{
  HyScanGtkMapBuilder *gtk_map_builder = HYSCAN_GTK_MAP_BUILDER (object);
  HyScanGtkMapBuilderPrivate *priv = gtk_map_builder->priv;

  g_free (priv->project_name);
  g_object_unref (priv->units);
  g_object_unref (priv->model_manager);
  g_object_unref (priv->cache);
  g_clear_object (&priv->planner_selection);
  g_clear_object (&priv->planner_model);
  g_object_unref (priv->db_info);
  g_object_unref (priv->mark_geo_model);
  g_object_unref (priv->ml_model);
  g_object_unref (priv->track_model);
  g_object_unref (priv->db);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->steer);

  G_OBJECT_CLASS (hyscan_gtk_map_builder_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_builder_project_changed (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;

  /* Очищаем список активных галсов, только если уже был открыт другой проект. */
  if (priv->project_name != NULL)
    hyscan_map_track_model_set_tracks (priv->track_model, (gchar **) { NULL });

  g_free (priv->project_name);
  priv->project_name = g_strdup (hyscan_gtk_model_manager_get_project_name (priv->model_manager));

  if (priv->track_model != NULL)
    hyscan_map_track_model_set_project (priv->track_model, priv->project_name);
  if (priv->wfmark_layer != NULL)
    hyscan_gtk_map_wfmark_set_project (HYSCAN_GTK_MAP_WFMARK (priv->wfmark_layer), priv->project_name);
}

/* Обрабатывает включение режима оффлайн. */
static void
hyscan_gtk_map_builder_offline_toggle (HyScanGtkMapProfileSwitch *profile_switch,
                                       GParamSpec                *pspec,
                                       GtkWidget                 *statusbar)
{
  gboolean offline;

  offline = hyscan_gtk_map_profile_switch_get_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH (profile_switch));
  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, offline ? _("Offline") : _("Online"));
}

/* Обновляет координаты курсора мыши в статусной строке. */
static gboolean
hyscan_gtk_map_builder_map_motion (HyScanGtkMap        *map,
                                   GdkEventMotion      *event,
                                   HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  HyScanGeoPoint geo;
  gchar text[255];
  gchar *lat, *lon;

  hyscan_gtk_map_point_to_geo (map, &geo, event->x, event->y);
  lat = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LAT, geo.lat, 6);
  lon = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LON, geo.lon, 6);
  g_snprintf (text, sizeof (text), "%s, %s", lat, lon);
  gtk_statusbar_push (GTK_STATUSBAR (priv->stbar_coord), 0, text);

  g_free (lat);
  g_free (lon);

  return FALSE;
}

/* Текущие координаты. */
static GtkWidget *
hyscan_gtk_map_builder_create_stbar (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *statusbar_box;

  statusbar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->stbar_offline = gtk_statusbar_new ();
  priv->stbar_coord = gtk_statusbar_new ();
  g_object_set (priv->stbar_offline, "margin", 0, NULL);
  g_object_set (priv->stbar_coord, "margin", 0, NULL);

  g_signal_connect (priv->map, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_builder_map_motion), builder);

  gtk_box_pack_start (GTK_BOX (statusbar_box), priv->stbar_offline, FALSE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (statusbar_box), priv->stbar_coord,   FALSE, TRUE, 10);

  return statusbar_box;
}

/* Изменяет масштаб карты на @steps шагов. */
static void
hyscan_gtk_map_builder_scale (HyScanGtkMap *map,
                              gint          steps)
{
  gdouble from_x, to_x, from_y, to_y;
  gint scale_idx;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);

  scale_idx = hyscan_gtk_map_get_scale_idx (map, NULL);
  hyscan_gtk_map_set_scale_idx (map, scale_idx + steps, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
}

/* Уменьшает масштаб карты на 1 шаг. */
static void
hyscan_gtk_map_builder_scale_down (HyScanGtkMap *map)
{
  hyscan_gtk_map_builder_scale (map, 1);
}

/* Увеличивает масштаб карты на 1 шаг. */
static void
hyscan_gtk_map_builder_scale_up (HyScanGtkMap *map)
{
  hyscan_gtk_map_builder_scale (map, -1);
}

/* Кнопки управления виджетом. */
static GtkWidget *
hyscan_gtk_map_builder_create_tools (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  gint t = 1;
  GtkWidget *toolbox;

  toolbox = gtk_grid_new ();
  gtk_grid_set_column_homogeneous (GTK_GRID (toolbox), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (toolbox), 5);
  gtk_grid_set_row_spacing (GTK_GRID (toolbox), 3);

  /* Выпадающий список с профилями. */
  priv->profile_switch = hyscan_gtk_map_profile_switch_new (HYSCAN_GTK_MAP (priv->map), LAYER_BASE, NULL);
  hyscan_gtk_map_profile_switch_load_config (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch));
  g_signal_connect (priv->profile_switch , "notify::offline", G_CALLBACK (hyscan_gtk_map_builder_offline_toggle), priv->stbar_offline);

  gtk_grid_attach (GTK_GRID (toolbox), priv->profile_switch, 0, ++t, 5, 1);
  gtk_grid_attach (GTK_GRID (toolbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);

  {
    GtkWidget *scale_up, *scale_down;

    scale_down = gtk_button_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign (scale_down, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (scale_down, GTK_ALIGN_CENTER);

    scale_up = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign (scale_up, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (scale_up, GTK_ALIGN_CENTER);

    g_signal_connect_swapped (scale_up,   "clicked", G_CALLBACK (hyscan_gtk_map_builder_scale_up), priv->map);
    g_signal_connect_swapped (scale_down, "clicked", G_CALLBACK (hyscan_gtk_map_builder_scale_down), priv->map);

    gtk_grid_attach (GTK_GRID (toolbox), gtk_label_new (_("Scale")), 0, ++t, 3, 1);
    gtk_grid_attach (GTK_GRID (toolbox), scale_down,                 3,   t, 1, 1);
    gtk_grid_attach (GTK_GRID (toolbox), scale_up,                   4,   t, 1, 1);
    gtk_grid_attach (GTK_GRID (toolbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);
  }

  /* Слои. */
  {
    GtkWidget *scrolled_wnd;

    /* Контейнер с прокруткой для списка слоёв. */
    scrolled_wnd = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled_wnd), priv->layer_list);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_wnd), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scrolled_wnd), TRUE);
    gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scrolled_wnd), TRUE);

    gtk_grid_attach (GTK_GRID (toolbox), scrolled_wnd, 0, ++t, 5, 1);
  }

  gtk_grid_attach (GTK_GRID (toolbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);

  return toolbox;
}

/* Обрабатывает переключение единиц измерения. */
static void
hyscan_gtk_map_builder_units_changed (GtkToggleButton     *button,
                                      GParamSpec          *pspec,
                                      HyScanGtkMapBuilder *builder)
{
  HyScanUnitsGeo geo_units;
  HyScanGtkMapBuilderPrivate *priv = builder->priv;

  if (!gtk_toggle_button_get_active (button))
    return;

  geo_units = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "unit"));
  hyscan_units_set_geo (priv->units, geo_units);
}

/* Создаёт панель инструментов для слоя сетки. */
static GtkWidget *
hyscan_gtk_map_builder_grid_toolbox (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *box, *btn_widget = NULL;
  HyScanUnitsGeo geo_units;
  guint i;
  struct {
    HyScanUnitsGeo  value;
    const gchar    *label;
    GtkWidget      *widget;
  } btn[] = {
      { HYSCAN_UNITS_GEO_DD,     N_("Decimal Degrees"), NULL },
      { HYSCAN_UNITS_GEO_DDMM,   N_("Degree Minute"), NULL },
      { HYSCAN_UNITS_GEO_DDMMSS, N_("Degree Minute Second"), NULL },
  };

  geo_units = hyscan_units_get_geo (priv->units);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new (_("Coordinate Units")), FALSE, TRUE, 0);

  for (i = 0; i < G_N_ELEMENTS (btn); i++)
    {
      btn_widget = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (btn_widget), _(btn[i].label));
      if (geo_units == btn[i].value)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn_widget), TRUE);

      g_object_set_data (G_OBJECT (btn_widget), "unit", GINT_TO_POINTER (btn[i].value));
      g_signal_connect (btn_widget, "notify::active", G_CALLBACK (hyscan_gtk_map_builder_units_changed), builder);
      gtk_box_pack_start (GTK_BOX (box), btn_widget, FALSE, TRUE, 0);
    }

  return box;
}

/* Создаёт панель инструментов для слоя булавок и линейки. */
static GtkWidget *
hyscan_gtk_map_builder_pin_toolbox (HyScanGtkLayer      *layer,
                                    const gchar         *label)
{
  GtkWidget *widget;

  widget = gtk_button_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label (GTK_BUTTON (widget), label);
  g_signal_connect_swapped (widget, "clicked", G_CALLBACK (hyscan_gtk_map_pin_clear), layer);

  return widget;
}

/* Изменяет режим отрисовки слоя галсов. */
static void
hyscan_gtk_map_builder_track_draw_change (GtkToggleButton   *button,
                                          GParamSpec        *pspec,
                                          HyScanGtkMapTrack *track_layer)
{
  HyScanGtkMapTrackDrawType draw_type;

  if (!gtk_toggle_button_get_active (button))
    return;

  draw_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "draw-type"));
  hyscan_gtk_map_track_set_draw_type (track_layer, draw_type);
}

/* Обрабатывает переход к текущему положению на карте. */
static void
hyscan_gtk_map_builder_locate_click (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  HyScanNavStateData data;

  if (hyscan_nav_state_get (HYSCAN_NAV_STATE (priv->nav_model), &data, NULL))
    hyscan_gtk_map_move_to (priv->map, data.coord);
}

/* Добавляет виджет навигации. */
static void
hyscan_gtk_map_builder_add_steer (HyScanGtkMapBuilder *builder,
                                  HyScanControlModel  *control_model,
                                  gboolean             enable_autostart)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *direction;
  GtkWidget *autostart;
  GtkWidget *autoselect;

  priv->steer = hyscan_steer_new (HYSCAN_NAV_STATE (priv->nav_model), priv->planner_selection, control_model);


  direction = hyscan_gtk_map_direction_new (HYSCAN_GTK_MAP (priv->map), priv->planner_selection,
                                            HYSCAN_NAV_STATE (priv->nav_model));

  if (enable_autostart)
    {
      autostart = gtk_check_button_new_with_mnemonic (_("_Autostart recording"));
      g_object_bind_property (autostart, "active", priv->steer, "autostart", G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (direction), autostart);
    }
  else
    {
      hyscan_steer_set_autostart (priv->steer, FALSE);
    }

  autoselect = gtk_check_button_new_with_mnemonic (_("_Select track automatically"));
  g_object_bind_property (autoselect, "active", priv->steer, "autoselect", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (direction), autoselect);

  gtk_box_pack_start (GTK_BOX (priv->steer_box), hyscan_gtk_map_steer_new (priv->steer), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->steer_box), direction,   TRUE, TRUE, 0);
}

/* Обрабатывает изменения режима работы. */
static void
hyscan_gtk_map_planner_mode_changed (GtkToggleButton     *togglebutton,
                                     HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  HyScanGtkMapPlanner *planner = priv->layer_planner;
  GtkWidget *planner_stack = priv->planner_stack;

  HyScanGtkMapPlannerMode mode;
  const gchar *child_name;

  if (!gtk_toggle_button_get_active (togglebutton))
    return;

  mode = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton), PLANNER_TOOLS_MODE));
  if (mode == HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN)
    child_name = PLANNER_TAB_ORIGIN;
  else if (mode == HYSCAN_GTK_MAP_PLANNER_MODE_ZONE)
    child_name = PLANNER_TAB_ZEDITOR;
  else
    child_name = PLANNER_TAB_STATUS;

  hyscan_gtk_map_planner_set_mode (planner, mode);
  gtk_stack_set_visible_child_name (GTK_STACK (planner_stack), child_name);
}

static GtkWidget*
create_planner_mode_btn (HyScanGtkMapBuilder     *builder,
                         GtkWidget               *from,
                         const gchar             *icon,
                         const gchar             *tooltip,
                         HyScanGtkMapPlannerMode  mode)
{
  GtkWidget *button;

  button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (from));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));
  gtk_button_set_image_position (GTK_BUTTON (button), GTK_POS_TOP);
  gtk_button_set_label (GTK_BUTTON (button), tooltip);

  g_object_set_data (G_OBJECT (button), PLANNER_TOOLS_MODE, GUINT_TO_POINTER (mode));
  g_signal_connect (button, "toggled", G_CALLBACK (hyscan_gtk_map_planner_mode_changed), builder);

  return button;
}

/* Создаёт панель инструментов для слоя планировщика. */
static GtkWidget *
hyscan_gtk_map_builder_planner_tools (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *box;
  GtkWidget *tab_switch;
  GtkWidget *origin_label;
  GtkWidget *tab_status, *tab_origin, *tab_zeditor;
  GtkWidget *zone_mode, *track_mode, *origin_mode;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  priv->planner_stack = gtk_stack_new ();
  gtk_stack_set_vhomogeneous (GTK_STACK (priv->planner_stack), FALSE);

  tab_switch = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  track_mode = create_planner_mode_btn (builder, NULL, "document-new-symbolic", _("Track"),
                                        HYSCAN_GTK_MAP_PLANNER_MODE_TRACK);
  zone_mode = create_planner_mode_btn (builder, track_mode, "folder-new-symbolic", _("Zone"),
                                       HYSCAN_GTK_MAP_PLANNER_MODE_ZONE);
  origin_mode = create_planner_mode_btn (builder, zone_mode, "go-home-symbolic", _("Origin"),
                                         HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN);

  gtk_style_context_add_class (gtk_widget_get_style_context (tab_switch), GTK_STYLE_CLASS_LINKED);
  gtk_box_pack_start (GTK_BOX (tab_switch), track_mode, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_switch), zone_mode, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_switch), origin_mode, TRUE, TRUE, 0);

  tab_zeditor = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  tab_status = hyscan_gtk_planner_status_new (priv->layer_planner);
  tab_origin = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  origin_label = gtk_label_new (_("Enter parameters of coordinate system or pick it on the map"));
  gtk_label_set_max_width_chars (GTK_LABEL (origin_label), 10);
  gtk_label_set_line_wrap (GTK_LABEL (origin_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (origin_label), 0.0);
  gtk_box_pack_start (GTK_BOX (tab_origin), origin_label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_origin), hyscan_gtk_planner_origin_new (priv->planner_model), FALSE, TRUE, 0);

  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_status, PLANNER_TAB_STATUS);
  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_zeditor, PLANNER_TAB_ZEDITOR);
  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_origin, PLANNER_TAB_ORIGIN);

  gtk_box_pack_start (GTK_BOX (box), tab_switch, TRUE, FALSE, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->planner_stack, TRUE, FALSE, 6);

  return box;
}

static void
on_cog_len_change (GtkSpinButton   *widget,
                   GParamSpec      *pspec,
                   HyScanGtkMapNav *nav_layer)
{
  gdouble minutes_value;

  minutes_value = gtk_spin_button_get_value (widget);
  hyscan_gtk_map_nav_set_cog_len (nav_layer, minutes_value * 60.0);
}

static void
on_hdg_len_change (GtkSpinButton   *widget,
                   GParamSpec      *pspec,
                   HyScanGtkMapNav *nav_layer)
{
  gdouble km_value;

  km_value = gtk_spin_button_get_value (widget);
  hyscan_gtk_map_nav_set_hdg_len (nav_layer, km_value * 1000.0);
}

static GtkWidget*
nav_tools (HyScanGtkMapNav *nav_layer)
{
  GtkWidget *grid, *cog_spin, *hdg_spin;
  gint t = -1;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);

  cog_spin = gtk_spin_button_new_with_range (0, 60, 1);
  hdg_spin = gtk_spin_button_new_with_range (0, 100, 0.1);

  g_signal_connect (cog_spin, "notify::value", G_CALLBACK (on_cog_len_change), nav_layer);
  g_signal_connect (hdg_spin, "notify::value", G_CALLBACK (on_hdg_len_change), nav_layer);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (cog_spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (hdg_spin), 0.2);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (_("COG Predictor, min")), 0, ++t, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), cog_spin, 1, t, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new (_("HDG Predictor, km")), 0, ++t, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), hdg_spin, 1, t, 1, 1);

  return grid;
}

/* Функция-обработчик передёт режим отображения меток с акустическим изображением в соответствующий слой. */
static void
hyscan_gtk_map_builder_on_changed_combo_box (GtkComboBox    *combo,
                                         HyScanGtkLayer *layer)
{
  hyscan_gtk_map_wfmark_set_show_mode (HYSCAN_GTK_MAP_WFMARK (layer),
                                       gtk_combo_box_get_active (combo));
}

/* Функция создаёт панель инструментов для слоя меток с акустическим изображением. */
static GtkWidget *
create_wfmark_layer_toolbox (HyScanGtkMapWfmark *layer)
{
  GtkWidget *combo;

  combo = gtk_combo_box_text_new();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), NULL, _("Show acoustic image"));
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), NULL, _("Show mark border only"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  g_signal_connect (GTK_COMBO_BOX (combo), "changed",
                    G_CALLBACK (hyscan_gtk_map_builder_on_changed_combo_box), layer);

  return combo;
}

static GtkWidget *
create_planner_zeditor (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *box, *scroll_wnd, *zeditor, *topo_checkbox;

  scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_wnd), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_wnd), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  zeditor = hyscan_gtk_planner_zeditor_new (priv->planner_model, priv->planner_selection);
  gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (zeditor), GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_container_add (GTK_CONTAINER (scroll_wnd), zeditor);

  topo_checkbox = gtk_check_button_new_with_label(_("Topo coordinates (x, y)"));
  g_object_bind_property (topo_checkbox, "active",
                          zeditor, "geodetic", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), topo_checkbox, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), scroll_wnd, TRUE, TRUE, 0);

  return box;
}

static void
planner_stack_switch_tracks (HyScanPlannerSelection  *selection,
                             const gchar * const     *tracks,
                             GtkStack                *stack)
{
  gchar *zone_id;
  gboolean track_selected;

  track_selected = (tracks[0] != NULL);
  if (track_selected)
    {
      gtk_stack_set_visible_child_name (stack, "track-editor");
      return;
    }

  zone_id = hyscan_planner_selection_get_zone (selection, NULL);
  if (zone_id != NULL)
    {
      gtk_stack_set_visible_child_name (stack, "zone-editor");
      g_free (zone_id);
    }
}

static void
planner_stack_switch_zone (HyScanPlannerSelection  *selection,
                           GtkStack                *stack)
{
  gchar **tracks;

  tracks = hyscan_planner_selection_get_tracks (selection);
  planner_stack_switch_tracks (selection, (const gchar *const *) tracks, stack);
  g_strfreev (tracks);
}

HyScanGtkMapBuilder *
hyscan_gtk_map_builder_new (HyScanGtkModelManager *model_manager)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_BUILDER,
                       "model-manager", model_manager,
                       NULL);
}


void
hyscan_gtk_map_builder_set_offline (HyScanGtkMapBuilder *builder,
                                    gboolean             offline)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  hyscan_gtk_map_profile_switch_set_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch), offline);
}

gboolean
hyscan_gtk_map_builder_get_offline (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), FALSE);
  priv = builder->priv;

  return hyscan_gtk_map_profile_switch_get_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch));
}

void
hyscan_gtk_map_builder_add_planner (HyScanGtkMapBuilder    *builder,
                                    gboolean                write_records)
{
  HyScanGtkMapBuilderPrivate *priv;
  HyScanGtkLayer *planner_layer;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  g_return_if_fail (priv->planner_model == NULL);

  /* Модель данных планировщика. */
  priv->planner_model = hyscan_gtk_model_manager_get_planner_model (priv->model_manager);
  priv->planner_selection = hyscan_planner_selection_new (priv->planner_model);

  /* Слой планировщика. */
  planner_layer = hyscan_gtk_map_planner_new (priv->planner_model, priv->planner_selection);
  hyscan_gtk_map_builder_add_layer (builder, planner_layer, FALSE, LAYER_PLANNER, _("Planner"));
  priv->layer_planner = HYSCAN_GTK_MAP_PLANNER (planner_layer);

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_PLANNER,
                                   hyscan_gtk_map_builder_planner_tools (builder));

  if (write_records)
    hyscan_planner_selection_watch_records (priv->planner_selection, priv->db_info);
}

void
hyscan_gtk_map_builder_add_planner_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;
  GtkWidget *vbox, *hbox, *stack;
  GtkWidget *list_bar, *track_editor, *zone_editor, *scroll_wnd;
  HyScanTrackStats *track_stats;
  HyScanPlannerStats *planner_stats;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  track_stats = hyscan_track_stats_new (priv->db_info, priv->planner_model, priv->cache);
  planner_stats = hyscan_planner_stats_new (track_stats, priv->planner_model);
  g_object_unref (track_stats);

  /* Список схемы галсов помещаем в GtkScrolledWindow. */
  priv->planner_list = hyscan_gtk_planner_list_new (priv->planner_model, priv->planner_selection, planner_stats,
                                                    priv->layer_planner);
  hyscan_gtk_planner_list_enable_binding (HYSCAN_GTK_PLANNER_LIST (priv->planner_list), priv->db_info);

  scroll_wnd = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scroll_wnd, FALSE);
  gtk_widget_set_vexpand (scroll_wnd, FALSE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_wnd), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_wnd), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll_wnd), 50);
  gtk_container_add (GTK_CONTAINER (scroll_wnd), priv->planner_list);

  /* Панель управления списком схемы галсов. */
  list_bar = hyscan_gtk_planner_list_bar_new (HYSCAN_GTK_PLANNER_LIST (priv->planner_list));

  /* Стек виджетов для редактирования параметров галсов и зон. */
  stack = gtk_stack_new ();
  gtk_widget_set_margin_top (stack, 6);

  track_editor = hyscan_gtk_planner_editor_new (priv->planner_model, priv->planner_selection);
  zone_editor = create_planner_zeditor (builder);
  gtk_stack_add_named (GTK_STACK (stack), track_editor, "track-editor");
  gtk_stack_add_named (GTK_STACK (stack), zone_editor, "zone-editor");

  /* Переключение виджетов в стеке в зависимости от выбранного объекта. */
  g_signal_connect (priv->planner_selection, "tracks-changed", G_CALLBACK (planner_stack_switch_tracks), stack);
  g_signal_connect (priv->planner_selection, "zone-changed", G_CALLBACK (planner_stack_switch_zone), stack);

  /* Компануем все виджеты вместе. */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), list_bar, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), scroll_wnd, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), stack, FALSE, TRUE, 0);

  priv->planner_list_wrap = hbox;

  priv->paned = hyscan_gtk_paned_new ();
  hyscan_gtk_paned_add (HYSCAN_GTK_PANED (priv->paned), priv->planner_list_wrap, "tracks", _("Planned tracks"), NULL);
  hyscan_gtk_paned_set_center_widget (HYSCAN_GTK_PANED (priv->paned), GTK_WIDGET (priv->map));
}

void
hyscan_gtk_map_builder_layer_set_tools (HyScanGtkMapBuilder *builder,
                                        const gchar         *layer_id,
                                        GtkWidget           *widget)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), layer_id, widget);
}

/* Добавляет @layer на карту и в список слоёв. */
void
hyscan_gtk_map_builder_add_layer (HyScanGtkMapBuilder *builder,
                                  HyScanGtkLayer      *layer,
                                  gboolean             visible,
                                  const gchar         *key,
                                  const gchar         *title)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;

  /* Регистрируем слой в карте. */
  if (layer != NULL)
    {
      hyscan_gtk_layer_set_visible (layer, visible);
      hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer, key);
    }
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (priv->layer_list), layer, key, title);

  /* Применяем профиль, чтобы загрузить его параметры. */
  hyscan_gtk_map_profile_switch_set_id (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch), NULL);
}

void
hyscan_gtk_map_builder_add_ruler (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;
  HyScanGtkLayer *ruler;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  ruler = hyscan_gtk_map_ruler_new ();
  hyscan_gtk_map_builder_add_layer (builder, ruler, TRUE, LAYER_RULER, _("Ruler"));

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_RULER,
                                   hyscan_gtk_map_builder_pin_toolbox (ruler, _("Remove ruler")));
}

void
hyscan_gtk_map_builder_add_pin (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;
  HyScanGtkLayer *pin_layer;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  pin_layer = hyscan_gtk_map_pin_new ();

  hyscan_gtk_map_builder_add_layer (builder, pin_layer, TRUE, LAYER_PIN, _("Pin"));

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_PIN,
                                   hyscan_gtk_map_builder_pin_toolbox (pin_layer, _("Remove all pins")));
}

void
hyscan_gtk_map_builder_add_grid (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;
  HyScanGtkLayer *map_grid;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  map_grid = hyscan_gtk_map_grid_new (priv->units);
  hyscan_gtk_map_builder_add_layer (builder, map_grid,   TRUE,  LAYER_GRID,  _("Grid"));
  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_GRID,
                                   hyscan_gtk_map_builder_grid_toolbox (builder));
}

void
hyscan_gtk_map_builder_add_marks (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *scrolled_window;

  /* Слой с метками. */
  priv->wfmark_layer = HYSCAN_GTK_MAP_WFMARK (hyscan_gtk_map_wfmark_new (priv->ml_model, priv->db, priv->cache, priv->units));
  hyscan_gtk_map_wfmark_set_project (HYSCAN_GTK_MAP_WFMARK (priv->wfmark_layer), priv->project_name);
  hyscan_gtk_map_builder_add_layer (builder, HYSCAN_GTK_LAYER (priv->wfmark_layer),
                                    FALSE, LAYER_WFMARK, _("Waterfall Marks"));
  hyscan_gtk_map_builder_layer_set_tools (builder, LAYER_WFMARK, create_wfmark_layer_toolbox (priv->wfmark_layer));

  /* Слой с геометками. */
  priv->geomark_layer = HYSCAN_GTK_MAP_GEOMARK (hyscan_gtk_map_geomark_new (priv->mark_geo_model));
  hyscan_gtk_map_builder_add_layer (builder, HYSCAN_GTK_LAYER (priv->geomark_layer), FALSE, LAYER_GEOMARK, _("Geo Marks"));

  /* Список меток слева. */
  priv->mark_list = hyscan_gtk_map_mark_list_new (priv->mark_geo_model,
                                                  priv->ml_model,
                                                  priv->wfmark_layer,
                                                  priv->geomark_layer);

  /* Область прокрутки со списком меток. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scrolled_window, FALSE);
  gtk_widget_set_vexpand (scrolled_window, FALSE);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scrolled_window), FALSE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled_window), 150);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 120);
  gtk_container_add (GTK_CONTAINER (scrolled_window), priv->mark_list);

  /* Помещаем в панель навигации. */
  gtk_box_pack_start (GTK_BOX (priv->left_col), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->left_col), scrolled_window, TRUE, TRUE, 0);
}

/* Выбор галсов проекта. */
static void
hyscan_gtk_map_builder_add_track_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *scrolled_window;

  priv->track_list = hyscan_gtk_map_track_list_new (priv->track_model, priv->db_info, priv->track_layer);

  /* Область прокрутки со списком галсов. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scrolled_window), FALSE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (scrolled_window, FALSE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled_window), priv->track_list);

  /* Помещаем в панель навигации. */
  gtk_box_pack_start (GTK_BOX (priv->left_col), scrolled_window, TRUE, TRUE, 0);
}

void
hyscan_gtk_map_builder_add_tracks (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;
  GtkWidget *bar, *beam, *box;
  
  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  priv->track_layer = HYSCAN_GTK_MAP_TRACK (hyscan_gtk_map_track_new (priv->track_model));
  hyscan_gtk_map_builder_add_layer (builder, HYSCAN_GTK_LAYER (priv->track_layer), FALSE, LAYER_TRACK, _("Tracks"));

  /* Создаём инструменты для слоя галсов. */
  bar = gtk_radio_button_new_with_label_from_widget (NULL, _("Distance bars"));
  g_object_set_data (G_OBJECT (bar), "draw-type", GINT_TO_POINTER (HYSCAN_GTK_MAP_TRACK_BAR));
  g_signal_connect (bar, "notify::active", G_CALLBACK (hyscan_gtk_map_builder_track_draw_change), priv->track_layer);

  beam = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (bar), _("Coverage"));
  g_object_set_data (G_OBJECT (beam), "draw-type", GINT_TO_POINTER (HYSCAN_GTK_MAP_TRACK_BEAM));
  g_signal_connect (beam, "notify::active", G_CALLBACK (hyscan_gtk_map_builder_track_draw_change), priv->track_layer);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), bar, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), beam, FALSE, TRUE, 0);

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_TRACK, box);
  hyscan_gtk_map_builder_add_track_list (builder);
}

static HyScanNavModel *
hyscan_gtk_map_builder_init_nav_model (HyScanControlModel *control_model)
{
  HyScanNavModel *nav_model;
  HyScanAntennaOffset offset = {0};
  HyScanControl *control = hyscan_control_model_get_control (control_model);
  const gchar *const *sensors;
  const gchar *sensor_name;
  gint i;

  sensors = hyscan_control_sensors_list (control);
  g_object_unref (control);
  if (sensors == NULL)
    return NULL;

  /* Находим датчик. */
  for (i = 0; sensors[i] != NULL; i++)
    {
      if (g_strstr_len (sensors[i], -1, "gnss") == NULL)
        continue;

      sensor_name = sensors[i];
      break;
    }

  if (sensor_name == NULL)
    return NULL;

  hyscan_sensor_set_enable (HYSCAN_SENSOR (control_model), sensor_name, TRUE);
  hyscan_sensor_state_antenna_get_offset (HYSCAN_SENSOR_STATE (control_model), sensor_name, &offset);
  nav_model = hyscan_nav_model_new ();
  hyscan_nav_model_set_sensor (nav_model, HYSCAN_SENSOR (control_model));
  hyscan_nav_model_set_sensor_name (nav_model, sensor_name);
  hyscan_nav_model_set_offset (nav_model, &offset);

  return nav_model;
}

gchar *
hyscan_gtk_map_builder_get_selected_track (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return hyscan_gtk_map_track_list_get_selected (HYSCAN_GTK_MAP_TRACK_LIST (priv->track_list));
}

/**
 * hyscan_gtk_map_builder_add_nav:
 * @builder: указатель на #HyScanGtkMapBuilder
 * @control_model: модель оборудования
 * @enable_autostart: включить возможность автостарта записи
 *
 * Функция добавляет слой и виджеты для судовождения.
 * Установите @enable_autostart = %TRUE, если планируется осуществлять запись галсов.
 */
void
hyscan_gtk_map_builder_add_nav (HyScanGtkMapBuilder *builder,
                                HyScanControlModel  *control_model,
                                gboolean             enable_autostart)
{
  HyScanGtkMapBuilderPrivate *priv;
  GtkWidget *box;
  HyScanGtkLayer *nav_layer;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  g_return_if_fail (control_model != NULL);
  priv = builder->priv;

  /* Навигационные данные. */
  priv->nav_model = hyscan_gtk_map_builder_init_nav_model (control_model);

  /* Определение местоположения. */
  priv->locate_button = gtk_button_new_from_icon_name ("network-wireless-signal-good-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label (GTK_BUTTON (priv->locate_button), _("My location"));
  g_signal_connect_swapped (priv->locate_button, "clicked", G_CALLBACK (hyscan_gtk_map_builder_locate_click), builder);

  /* Слой с траекторией движения судна. */
  nav_layer = hyscan_gtk_map_nav_new (HYSCAN_NAV_STATE (priv->nav_model));
  hyscan_gtk_map_builder_add_layer (builder, nav_layer, FALSE, LAYER_NAV, _("Navigation"));

  /* Контейнер для виджета навигации по галсу (сам виджет может быть, а может не быть). */
  priv->steer_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->steer_box, FALSE, TRUE, 6);
  gtk_box_pack_start (GTK_BOX (box), nav_tools (HYSCAN_GTK_MAP_NAV (nav_layer)), FALSE, TRUE, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->locate_button, FALSE, TRUE, 6);

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), LAYER_NAV, box);
  hyscan_gtk_map_builder_add_steer (builder, control_model, enable_autostart);
}

/**
 * hyscan_gtk_map_builder_run_planner_import:
 * @kit: указатель на HyScanGtkMapKit
 *
 * Создаёт окно с виджетом импорта данных планировщика
 */
void
hyscan_gtk_map_builder_run_planner_import (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv = builder->priv;
  GtkWidget *window, *toplevel;
  GtkWidget *box, *import, *abar;
  GtkWidget *start_btn, *close_btn;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Import Track Plan"));
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 200);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->map));
  if (GTK_IS_WINDOW (toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  import = hyscan_gtk_planner_import_new (priv->db, priv->project_name);
  g_object_set (import, "margin", 6, NULL);

  start_btn = gtk_button_new_with_mnemonic (_("_Import"));
  close_btn = gtk_button_new_with_mnemonic (_("_Close"));

  g_object_bind_property (import, "ready", start_btn, "sensitive", G_BINDING_SYNC_CREATE);
  g_signal_connect_swapped (start_btn, "clicked", G_CALLBACK (hyscan_gtk_planner_import_start), import);
  g_signal_connect_swapped (close_btn, "clicked", G_CALLBACK (hyscan_gtk_planner_import_stop), import);
  g_signal_connect_swapped (close_btn, "clicked", G_CALLBACK (gtk_widget_destroy), window);

  abar = gtk_action_bar_new ();
  gtk_action_bar_pack_start (GTK_ACTION_BAR (abar), start_btn);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), close_btn);

  gtk_box_pack_start (GTK_BOX (box), import, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), abar, FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), box);

  gtk_widget_show_all (window);
}

HyScanGtkMap *
hyscan_gtk_map_builder_get_map (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->map;
}

GtkWidget *
hyscan_gtk_map_builder_get_tools (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->right_col;
}

GtkWidget *
hyscan_gtk_map_builder_get_bar (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->statusbar;
}

GtkWidget *
hyscan_gtk_map_builder_get_layer_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->layer_list;
}

GtkWidget *
hyscan_gtk_map_builder_get_profile_switch (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->profile_switch;
}

GtkWidget *
hyscan_gtk_map_builder_get_mark_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->mark_list;
}

GtkWidget *
hyscan_gtk_map_builder_get_planner_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->planner_list_wrap;
}

GtkWidget *
hyscan_gtk_map_builder_get_track_list (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->track_list;
}

GtkWidget *
hyscan_gtk_map_builder_get_left_col (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->left_col;
}

GtkWidget *
hyscan_gtk_map_builder_get_right_col (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->right_col;
}

GtkWidget *
hyscan_gtk_map_builder_get_central (HyScanGtkMapBuilder *builder)
{
  HyScanGtkMapBuilderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder), NULL);
  priv = builder->priv;

  return priv->paned != NULL ? priv->paned : GTK_WIDGET (priv->map);
}

/**
 * hyscan_gtk_map_builder_read_write:
 * @builder: указатель на #HyScanGtkMapBuilder
 * @file: #GKeyFile для чтения
 *
 * Загружает настройки карты из файла @file
 */
void
hyscan_gtk_map_builder_file_read (HyScanGtkMapBuilder *builder,
                                  GKeyFile            *file)
{
  HyScanGtkMapBuilderPrivate *priv;
  gchar *profile_name = NULL;
  gboolean offline;
  HyScanGeoPoint center;
  gchar **layers, **tracks;
  gchar *cache_dir;
  gint planner_cols;
  HyScanGtkMapProfileSwitch *profile_switch;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  profile_switch = HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch);

  /* Считываем настройки из файла конфигурации. */
  center.lat   = g_key_file_get_double      (file, "evo-map", "lat", NULL);
  center.lon   = g_key_file_get_double      (file, "evo-map", "lon", NULL);
  profile_name = g_key_file_get_string      (file, "evo-map", "profile", NULL);
  layers       = g_key_file_get_string_list (file, "evo-map", "layers", NULL, NULL);
  tracks       = g_key_file_get_string_list (file, "evo-map", "tracks", NULL, NULL);
  cache_dir    = g_key_file_get_string      (file, "evo-map", "cache", NULL);
  planner_cols = g_key_file_has_key         (file, "evo-map", "planner-cols", NULL) ?
                 g_key_file_get_integer     (file, "evo-map", "planner-cols", NULL) :
                 HYSCAN_GTK_PLANNER_LIST_INVALID;

  offline      = g_key_file_has_key         (file, "evo-map", "offline", NULL) ?
                 g_key_file_get_boolean     (file, "evo-map", "offline", NULL) :
                 TRUE;

  /* Применяем настройки. */
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (priv->map), center);

  if (cache_dir != NULL)
    {
      hyscan_gtk_map_profile_switch_set_cache_dir (profile_switch, cache_dir);
      g_free (cache_dir);
    }

  if (profile_name != NULL)
    {
      hyscan_gtk_map_profile_switch_set_id (profile_switch, profile_name);
      g_free (profile_name);
    }

  if (layers != NULL)
    {
      hyscan_gtk_layer_list_set_visible_ids (HYSCAN_GTK_LAYER_LIST (priv->layer_list), layers);
      g_strfreev (layers);
    }

  if (tracks != NULL)
    {
      hyscan_map_track_model_set_tracks (priv->track_model, tracks);
      g_strfreev (tracks);
    }

  hyscan_gtk_map_profile_switch_set_offline (profile_switch, offline);

  if (planner_cols != HYSCAN_GTK_PLANNER_LIST_INVALID && priv->planner_list != NULL)
    hyscan_gtk_planner_list_set_visible_cols (HYSCAN_GTK_PLANNER_LIST (priv->planner_list), planner_cols);
}

/**
 * hyscan_gtk_map_builder_file_write:
 * @builder: указатель на #HyScanGtkMapBuilder
 * @file: #GKeyFile для записи
 *
 * Сохраняет настройки карты в файл @file.
 */
void
hyscan_gtk_map_builder_file_write (HyScanGtkMapBuilder *builder,
                                   GKeyFile            *file)
{
  HyScanGtkMapBuilderPrivate *priv;
  gchar *profile;
  gchar **layers, **tracks;
  HyScanGeoPoint point;
  HyScanGeoCartesian2D  c2d;
  gdouble from_x, to_x, from_y, to_y;
  gint planner_cols;
  gboolean offline;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_BUILDER (builder));
  priv = builder->priv;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  c2d.x = (to_x + from_x) / 2.0;
  c2d.y = (to_y + from_y) / 2.0;
  hyscan_gtk_map_value_to_geo (priv->map, &point, c2d);

  profile = hyscan_gtk_map_profile_switch_get_id (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch));
  layers = hyscan_gtk_layer_list_get_visible_ids (HYSCAN_GTK_LAYER_LIST (priv->layer_list));
  if (priv->planner_list != NULL)
    planner_cols = hyscan_gtk_planner_list_get_visible_cols (HYSCAN_GTK_PLANNER_LIST (priv->planner_list));
  else
    planner_cols = 0;
  tracks = hyscan_map_track_model_get_tracks (priv->track_model);
  offline = hyscan_gtk_map_profile_switch_get_offline (HYSCAN_GTK_MAP_PROFILE_SWITCH (priv->profile_switch));

  g_key_file_set_double      (file, "evo-map", "lat", point.lat);
  g_key_file_set_double      (file, "evo-map", "lon", point.lon);
  g_key_file_set_string      (file, "evo-map", "profile", profile);
  /* Не сохраняем "evo-map" > "cache", т.к. он настраивается через конфигуратор. */
  /* g_key_file_set_string      (kf, "evo-map", "cache",   kit->priv->tile_cache_dir); */
  g_key_file_set_boolean     (file, "evo-map", "offline", offline);
  g_key_file_set_string_list (file, "evo-map", "layers", (const gchar *const *) layers, g_strv_length (layers));
  g_key_file_set_string_list (file, "evo-map", "tracks", (const gchar *const *) tracks, g_strv_length (tracks));
  g_key_file_set_integer     (file, "evo-map", "planner-cols", planner_cols);

  g_free (profile);
  g_strfreev (tracks);
  g_strfreev (layers);
}
