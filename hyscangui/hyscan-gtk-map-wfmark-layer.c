#include "hyscan-gtk-map-wfmark-layer.h"
#include <hyscan-gtk-map.h>
#include <hyscan-mark-model.h>
#include <hyscan-db-info.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-projector.h>
#include <math.h>

#define ACOUSTIC_CHANNEL        1          /* Канал с акустическими данными. */
#define NMEA_RMC_CHANNEL        1          /* Канал с навигационными данными. */
#define DISTANCE_TO_METERS      0.001      /* Коэффициент перевода размеров метки в метры. */

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

  HyScanGeoGeodetic            center_geo;      /* Географические координаты центра метки. */
  HyScanGeoCartesian2D         center_c2d;      /* Координаты центра метки в СК карты. */
  HyScanGeoCartesian2D         track_c2d;       /* Географические координаты проекции центра метки на галс. */

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

  HyScanDB        *db;                          /* База данных. */
  HyScanCache     *cache;                       /* Кэш. */
  gchar           *project;                     /* Название проекта. */

  HyScanDBInfo    *db_info;                     /* Модель галсов в БД. */
  HyScanMarkModel *mark_model;                  /* Модель меток. */

  GRWLock          mark_lock;                   /* Блокировка данных по меткам. .*/
  GHashTable      *marks;                       /* Хэш-таблица с оригинальными метками.*/
  GList           *locations;                   /* Список меток, дополненный их координатами на карте.*/
  GHashTable      *track_names;                 /* Хэш-таблица соотвествтия id галса с его названием.*/
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

  priv->mark_model = hyscan_mark_model_new ();
  priv->db_info = hyscan_db_info_new (priv->db);

  g_signal_connect_swapped (priv->mark_model, "changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_layer_model_changed), wfm_layer);
  g_signal_connect_swapped (priv->db_info, "tracks-changed",
                            G_CALLBACK (hyscan_gtk_map_wfmark_layer_db_changed), wfm_layer);

  hyscan_db_info_set_project (priv->db_info, priv->project);
  hyscan_mark_model_set_project (priv->mark_model, priv->db, priv->project);
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

  /* Переводим из метров в единицы картографической проекции. */
  scale = hyscan_gtk_map_get_value_scale (priv->map, &location->center_geo);
  location->width = DISTANCE_TO_METERS * location->mark->width / scale;
  location->height = DISTANCE_TO_METERS * location->mark->height / scale;
  offset = location->offset / scale;

  /* Определяем координаты центра метки в СК проекции. */
  hyscan_gtk_map_geo_to_value (priv->map, location->center_geo, &location->track_c2d);
  g_message ("Mark %s angle %f", location->mark->name, location->center_geo.h);
  location->angle = location->center_geo.h / 180.0 * G_PI;
  location->center_c2d.x = location->track_c2d.x - offset * cos (location->angle);
  location->center_c2d.y = location->track_c2d.y + offset * sin (location->angle);
}

/* Загружает геолокациаонные данные по метке.
 * Функция должна вызываться за g_rw_lock_reader_lock (&priv->mark_lock). */
static gboolean
hyscan_gtk_map_wfmark_layer_load_location (HyScanGtkMapWfmarkLayer         *wfm_layer,
                                           HyScanGtkMapWfmarkLayerLocation *location)
{
  HyScanGtkMapWfmarkLayerPrivate *priv = wfm_layer->priv;

  const HyScanWaterfallMark *mark = location->mark;
  const gchar *track_name;

  /* Получаем название галса. */
  track_name = g_hash_table_lookup (priv->track_names, mark->track);
  if (track_name == NULL)
    return FALSE;
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

    if (hyscan_nav_data_find_data (lat_data, location->time, &lindex, &rindex, NULL, NULL) == HYSCAN_DB_FIND_OK)
      {
        gboolean found;

        found =  hyscan_nav_data_get (lat_data, lindex, NULL, &location->center_geo.lat) &&
                 hyscan_nav_data_get (lon_data, lindex, NULL, &location->center_geo.lon) &&
                 hyscan_nav_data_get (angle_data, lindex, NULL, &location->center_geo.h);

        // todo: если в текущем индексе нет данных, то пробовать брать соседние и интерполировать
        if (!found)
          g_warning ("HyScanGtkMapWfmarkLayer: mark location was not found");
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

  return TRUE;
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

  g_rw_lock_writer_unlock (&priv->mark_lock);
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

  cairo_set_source_rgb (cairo, 0.1, 0.7, 0.1);
  cairo_set_line_width (cairo, 2.0);
  cairo_set_font_size (cairo, 14);

  for (list = priv->locations; list != NULL; list = list->next)
    {
      HyScanGtkMapWfmarkLayerLocation *location = list->data;
      gdouble x, y;
      gdouble width, height;
      gdouble scale;


      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y,
                                             location->track_c2d.x, location->track_c2d.y);
      cairo_arc (cairo, x, y, 4.0, 0, 2 * G_PI);
      cairo_stroke (cairo);

      cairo_move_to (cairo, x, y);

      /* Переводим размеры метки из логической СК в пиксельные. */
      gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
      width = location->width / scale;
      height = location->height / scale;

      gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y,
                                             location->center_c2d.x, location->center_c2d.y);

      cairo_line_to (cairo, x, y);
      cairo_stroke (cairo);

      cairo_save (cairo);


      cairo_translate (cairo, x, y);
      cairo_rotate (cairo, location->angle);

      cairo_move_to (cairo, 0, -2);
      cairo_show_text (cairo, location->mark->name);
      cairo_rectangle (cairo, -width, -height, 2.0 * width, 2.0 * height);

      cairo_stroke (cairo);

      cairo_restore (cairo);
    }

  g_rw_lock_reader_unlock (&priv->mark_lock);
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
  g_signal_connect_after (priv->map, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_wfmark_layer_draw), wfm_layer);
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
hyscan_gtk_map_wfmark_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->removed = hyscan_gtk_map_wfmark_layer_removed;
  iface->added = hyscan_gtk_map_wfmark_layer_added;
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
