#include "hyscan-gtk-map-wfmark-layer.h"
#include <hyscan-cartesian.h>
#include <hyscan-gtk-map.h>
#include <hyscan-mark-model.h>
#include <hyscan-db-info.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-projector.h>
#include <math.h>

#define ACOUSTIC_CHANNEL        1                    /* Канал с акустическими данными. */
#define NMEA_RMC_CHANNEL        1                    /* Канал с навигационными данными. */
#define DISTANCE_TO_METERS      0.001                /* Коэффициент перевода размеров метки в метры. */

/* Оформление по умолчанию. */
#define MARK_COLOR              "#F55A22"            /* Цвет обводки меток. */
#define MARK_COLOR_HOVER        "#5A22F5"            /* Цвет обводки меток при наведении мыши. */
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
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT
};

typedef struct
{
  const HyScanWaterfallMark   *mark;            /* Указатель на оригинальную метку (принадлежит priv->marks). */

  gboolean                     loaded;          /* Признак того, что геолокационные данные загружены. */
  HyScanGeoGeodetic            center_geo;      /* Географические координаты центра метки. */
  HyScanGeoCartesian2D         center_c2d;      /* Координаты центра метки в СК карты. */
  HyScanGeoCartesian2D         track_c2d;       /* Координаты проекции центра метки на галс. */

  /* Rect - прямоугольник метки, повернутый на угол курса так, что стороны прямоугольника вдоль осей координат. */
  HyScanGeoCartesian2D         rect_from;       /* Координаты левой верхней вершины прямоугольника rect. */
  HyScanGeoCartesian2D         rect_to;         /* Координаты правой нижней вершины прямоугольника rect. */

  /* Extent - прямоугольник, внутри которого находится метка; стороны прямоугольника вдоль осей координат. */
  HyScanGeoCartesian2D         extent_from;     /* Координаты левой верхней вершины прямоугольника extent. */
  HyScanGeoCartesian2D         extent_to;       /* Координаты правой нижней вершины прямоугольника extent. */

  gint64                       time;            /* Время фиксации строки с меткой. */
  gdouble                      angle;           /* Угол курса в радианах. */
  gdouble                      offset;          /* Расстояние от линии галса до метки
                                                 * (положительные значения по левому борту). */

  gdouble                      width;           /* Ширина метки (вдоль линии галса) в единицах СК карты. */
  gdouble                      height;          /* Длина метки (поперёк линии галса) в единицах СК карты. */

} HyScanGtkMapWfmarkLayerLocation;

struct _HyScanGtkMapWfmarkLayerPrivate
{
  HyScanGtkMap    *map;                         /* Карта. */
  gboolean         visible;                     /* Признак видимости слоя. */

  HyScanDB        *db;                          /* База данных. */
  HyScanCache     *cache;                       /* Кэш. */
  gchar           *project;                     /* Название проекта. */

  HyScanDBInfo    *db_info;                     /* Модель галсов в БД. */
  HyScanMarkModel *mark_model;                  /* Модель меток. */

  GRWLock          mark_lock;                   /* Блокировка данных по меткам. .*/
  GHashTable      *track_names;                 /* Хэш-таблица соотвествтия id галса с его названием.*/
  GHashTable      *marks;                       /* Хэш-таблица с оригинальными метками.*/
  GList           *locations;                   /* Список меток, дополненный их координатами на карте.*/


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
static void    hyscan_gtk_map_wfmark_layer_db_changed               (HyScanGtkMapWfmarkLayer *wfm_layer);

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

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The HyScanCache for caching internal structures",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Database",
                         "The HyScanDb containing project with waterfall marks to be displayed",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project",
                         "The name of the project with waterfall marks",
                         NULL,
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
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
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
  priv->track_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* Модели данных. */
  priv->mark_model = hyscan_mark_model_new ();
  priv->db_info = hyscan_db_info_new (priv->db);

  g_signal_connect_swapped (priv->mark_model, "changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_layer_model_changed), wfm_layer);
  g_signal_connect_swapped (priv->db_info, "tracks-changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_layer_db_changed), wfm_layer);

  hyscan_db_info_set_project (priv->db_info, priv->project);
  hyscan_mark_model_set_project (priv->mark_model, priv->db, priv->project);

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

  g_hash_table_destroy (priv->track_names);
  g_hash_table_destroy (priv->marks);
  g_list_free_full (priv->locations, g_free);

  g_object_unref (priv->mark_model);
  g_object_unref (priv->db_info);
  g_object_unref (priv->db);
  g_object_unref (priv->cache);
  g_free (priv->project);

  g_clear_object (&priv->pango_layout);

  G_OBJECT_CLASS (hyscan_gtk_map_wfmark_layer_parent_class)->finalize (object);
}

/* Возвращает %TRUE, если источник данных соответствует правому борту. */
static inline gboolean
hyscan_gtk_map_wfmark_layer_is_starboard (HyScanSourceType source_type)
{
  switch (source_type)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_LOW:
      return TRUE;

    default:
      return FALSE;
    }
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

  if (!location->loaded)
    return;

  /* Переводим из метров в единицы картографической проекции. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, &location->center_geo);
  location->width = DISTANCE_TO_METERS * location->mark->width / scale;
  location->height = DISTANCE_TO_METERS * location->mark->height / scale;
  offset = location->offset / scale;

  /* Определяем координаты центра метки в СК проекции. */
  hyscan_gtk_map_geo_to_value (priv->map, location->center_geo, &location->track_c2d);

  location->angle = location->center_geo.h / 180.0 * G_PI;
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

/* Загружает геолокационные данные по метке.
 * Функция должна вызываться за g_rw_lock_reader_lock (&priv->mark_lock). */
static gboolean
hyscan_gtk_map_wfmark_layer_load_location (HyScanGtkMapWfmarkLayer         *wfm_layer,
                                           HyScanGtkMapWfmarkLayerLocation *location)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  const HyScanWaterfallMark *mark = location->mark;
  const gchar *track_name;

  location->loaded = FALSE;

  /* Получаем название галса. */
  track_name = g_hash_table_lookup (priv->track_names, mark->track);
  if (track_name == NULL)
    return location->loaded;
  // todo: Cache objects: amplitude, projector, lat_data, lon_data...
  
  /* Получаем временную метку из канала акустических данных. */
  {
    gint32 project_id, track_id, channel_id;
    const gchar *acoustic_channel_name;

    acoustic_channel_name = hyscan_channel_get_name_by_types (mark->source0, HYSCAN_CHANNEL_DATA, ACOUSTIC_CHANNEL);
    project_id = hyscan_db_project_open (priv->db, priv->project);
    track_id = hyscan_db_track_open (priv->db, project_id, track_name);
    channel_id = hyscan_db_channel_open (priv->db, track_id, acoustic_channel_name);

    location->time = hyscan_db_channel_get_data_time (priv->db, channel_id, mark->index0);

    hyscan_db_close (priv->db, channel_id);
    hyscan_db_close (priv->db, track_id);
    hyscan_db_close (priv->db, project_id);
  }
  
  /* Находим географические координаты метки и курс. */
  {
    HyScanNavData *lat_data, *lon_data, *angle_data;
    guint32 lindex, rindex;
    HyScanGeoGeodetic lgeo, rgeo;
    gint64 ltime, rtime;

    // todo: определять, какой номер канала с навигационными данными. Сейчас захардкожен "NMEA_RMC_CHANNEL"
    lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                        priv->project, track_name, NMEA_RMC_CHANNEL,
                                                        HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
    lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                        priv->project, track_name, NMEA_RMC_CHANNEL,
                                                        HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
    angle_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                          priv->project, track_name, NMEA_RMC_CHANNEL,
                                                          HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));

    if (hyscan_nav_data_find_data (lat_data, location->time, &lindex, &rindex, &ltime, &rtime) == HYSCAN_DB_FIND_OK)
      {
        gboolean found;

        /* Пробуем распарсить навигационные данные по найденным индексам. */
        found =  hyscan_nav_data_get (lat_data, lindex, NULL, &lgeo.lat) &&
                 hyscan_nav_data_get (lon_data, lindex, NULL, &lgeo.lon) &&
                 hyscan_nav_data_get (angle_data, lindex, NULL, &lgeo.h);
        found &=  hyscan_nav_data_get (lat_data, rindex, NULL, &rgeo.lat) &&
                  hyscan_nav_data_get (lon_data, rindex, NULL, &rgeo.lon) &&
                  hyscan_nav_data_get (angle_data, rindex, NULL, &rgeo.h);

        if (found)
          {
            gint64 dtime = rtime - ltime;

            /* Ищем средневзвешанное значение. */
            if (dtime > 0)
              {
                gdouble rweight, lweight;

                rweight = (gdouble) (rtime - location->time) / dtime;
                lweight = (gdouble) (location->time - ltime) / dtime;

                location->center_geo.lat = lweight * lgeo.lat + rweight * rgeo.lat;
                location->center_geo.lon = lweight * lgeo.lon + rweight * rgeo.lon;
                location->center_geo.h = lweight * lgeo.h + rweight * rgeo.h;
              }
            else
              {
                location->center_geo = lgeo;
              }

            location->loaded = TRUE;
          }
        else
          {
            // todo: если в текущем индексе нет данных, то пробовать брать соседние
            g_warning ("HyScanGtkMapWfmarkLayer: mark location was not found");
          }
      }

    g_object_unref (angle_data);
    g_object_unref (lat_data);
    g_object_unref (lon_data);
  }

  /* Определяем дальность расположения метки от тракетории. */
  {
    HyScanProjector *projector;
    HyScanAcousticData *acoustic_data;


    acoustic_data = hyscan_acoustic_data_new (priv->db, priv->cache,
                                              priv->project, track_name,
                                              mark->source0, ACOUSTIC_CHANNEL, FALSE);
    projector = hyscan_projector_new (HYSCAN_AMPLITUDE (acoustic_data));
    hyscan_projector_count_to_coord (projector, mark->count0, &location->offset, 0);
    if (hyscan_gtk_map_wfmark_layer_is_starboard(mark->source0))
      location->offset *= -1.0;

    g_object_unref (acoustic_data);
    g_object_unref (projector);
  }

  return location->loaded;
}

/* Перезагружает данные priv->locations.
 * Функция должна вызываться за g_rw_lock_writer_lock (&priv->mark_lock). */
static void
hyscan_gtk_map_wfmark_layer_reload_locations (HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  GHashTableIter iter;
  HyScanWaterfallMark *mark;

  g_list_free_full (priv->locations, g_free);
  priv->locations = NULL;

  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &mark))
    {
      HyScanGtkMapWfmarkLayerLocation *location;

      location = g_new0 (HyScanGtkMapWfmarkLayerLocation, 1);
      location->mark = mark;
      hyscan_gtk_map_wfmark_layer_load_location (wfm_layer, location);
      hyscan_gtk_map_wfmark_layer_project_location (wfm_layer, location);

      priv->locations = g_list_append (priv->locations, location);
    }
}

/* Обработчик сигнала HyScanDBInfo::tracks-changed.
 * Составляет таблицу соответствия ИД - название по каждому галсу. */
static void
hyscan_gtk_map_wfmark_layer_db_changed (HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;
  GHashTable *tracks;
  GHashTableIter iter;
  HyScanTrackInfo *track_info;

  tracks = hyscan_db_info_get_tracks (priv->db_info);

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Загружаем информацию по названиям галсов и их ИД. */
  g_hash_table_remove_all (priv->track_names);
  g_hash_table_iter_init (&iter, tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_info))
    g_hash_table_insert (priv->track_names, g_strdup (track_info->id), g_strdup (track_info->name));

  /* Перезагружаем геолокационную информацию. */
  hyscan_gtk_map_wfmark_layer_reload_locations (wfm_layer);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  g_hash_table_destroy (tracks);
}

/* Обработчик сигнала HyScanMarkModel::changed.
 * Обновляет список меток. */
static void
hyscan_gtk_map_wfmark_layer_model_changed (HyScanGtkMapWfmarkLayer *wfm_layer)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Получаем список меток. */
  g_clear_pointer (&priv->marks, g_hash_table_unref);
  priv->marks = hyscan_mark_model_get (priv->mark_model);

  /* Загружаем гео-данные по меткам. */
  // todo: сделать обновление текущего списка меток priv->marks, а не полную перезагрузку
  hyscan_gtk_map_wfmark_layer_reload_locations (wfm_layer);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_rw_lock_writer_unlock (&priv->mark_lock);
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
  GList *list;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (wfm_layer)))
    return;

  g_rw_lock_reader_lock (&priv->mark_lock);

  for (list = priv->locations; list != NULL; list = list->next)
    {
      HyScanGtkMapWfmarkLayerLocation *location = list->data;
      gdouble x, y;
      gdouble width, height;
      gdouble scale;

      if (!location->loaded)
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
      const HyScanGtkMapWfmarkLayerLocation *location = priv->location_hover;
      gint text_width, text_height;
      gdouble text_x, text_y;

      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &text_x, &text_y,
                                             priv->cursor.x, priv->cursor.y);

      pango_layout_set_text (priv->pango_layout, location->mark->name, -1);
      pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
      text_height /= PANGO_SCALE;
      text_width /= PANGO_SCALE;

      /* Вариант размещения текста под прямоугольником. */
      // cairo_save (cairo);
      // cairo_translate (cairo, x, y);
      // gdouble bounding_height = fabs (width * sin (location->angle)) + fabs (height * cos(location->angle));
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

  GList *list;

  for (list = priv->locations; list != NULL; list = list->next)
    {
      HyScanGtkMapWfmarkLayerLocation *location = list->data;
      HyScanGeoCartesian2D rotated;

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
  GList *list;

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Обновляем координаты меток согласно новой проекции. */
  for (list = priv->locations; list != NULL; list = list->next)
    hyscan_gtk_map_wfmark_layer_project_location (wfm_layer, list->data);

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

HyScanGtkLayer *
hyscan_gtk_map_wfmark_layer_new (HyScanDB    *db,
                                 const gchar *project,
                                 HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER,
                       "db", db,
                       "project", project,
                       "cache", cache,
                       NULL);
}
