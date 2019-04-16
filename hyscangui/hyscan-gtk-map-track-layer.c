/* hyscan-gtk-map-track-layer.c
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
 * SECTION: hyscan-gtk-map-track-layer
 * @Short_description: слой карты с изображением галсов
 * @Title: HyScanGtkMapTrackLayer
 * @See_also: #HyScanGtkLayer
 *
 */

#include "hyscan-gtk-map-track-layer.h"
#include <hyscan-nmea-data.h>
#include <hyscan-gtk-map.h>
#include <hyscan-nmea-parser.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_CACHE
};

typedef struct {
  HyScanNavData            *lat_data;         /* Навигационные данные - широта. */
  HyScanNavData            *lon_data;         /* Навигационные данные - долгота. */
} HyScanGtkMapTrackLayerTrack;

struct _HyScanGtkMapTrackLayerPrivate
{
  HyScanGtkMap              *map;             /* Карта. */
  gboolean                   visible;         /* Признак видимости слоя. */

  HyScanDB                  *db;              /* База данных. */
  const gchar               *project;         /* Название проекта. */
  HyScanCache               *cache;           /* Кэш для навигационных данных. */

  GHashTable                *tracks;          /* Таблица отображаемых галсов:
                                               * ключ - название трека, значение - HyScanGtkMapTrackLayerTrack. */
};

static void    hyscan_gtk_map_track_layer_interface_init           (HyScanGtkLayerInterface *iface);
static void    hyscan_gtk_map_track_layer_set_property             (GObject                *object,
                                                                    guint                   prop_id,
                                                                    const GValue           *value,
                                                                    GParamSpec             *pspec);
static void    hyscan_gtk_map_track_layer_object_constructed       (GObject                *object);
static void    hyscan_gtk_map_track_layer_object_finalize          (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackLayer, hyscan_gtk_map_track_layer, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_track_layer_interface_init))

static void
hyscan_gtk_map_track_layer_class_init (HyScanGtkMapTrackLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_layer_set_property;

  object_class->constructed = hyscan_gtk_map_track_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Db", "HyScanDb", HYSCAN_TYPE_DB,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name", NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache", HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_layer_init (HyScanGtkMapTrackLayer *gtk_map_track_layer)
{
  gtk_map_track_layer->priv = hyscan_gtk_map_track_layer_get_instance_private (gtk_map_track_layer);
}

static void
hyscan_gtk_map_track_layer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

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

/* Особождает память, занятую структурой HyScanGtkMapTrackLayerTrack. */
static void
hyscan_gtk_map_track_layer_track_free (gpointer data)
{
  HyScanGtkMapTrackLayerTrack *track = data;

  g_object_unref (track->lat_data);
  g_object_unref (track->lon_data);
  g_free (track);
}

static void
hyscan_gtk_map_track_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->constructed (object);

  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, hyscan_gtk_map_track_layer_track_free);
}

static void
hyscan_gtk_map_track_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  g_clear_object (&priv->db);
  g_hash_table_destroy (priv->tracks);

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->finalize (object);
}

/* Рисует отдельный галс. */
static void
hyscan_gtk_map_track_layer_draw_track (HyScanGtkMapTrackLayer      *track_layer,
                                       HyScanGtkMapTrackLayerTrack *track,
                                       cairo_t                     *cairo)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  guint32 index, rindex;

  if (!hyscan_nav_data_get_range (track->lat_data, &index, &rindex))
    return;

  cairo_new_path (cairo);
  for (; index < rindex; ++index)
    {
      gdouble x_val, y_val;
      gdouble x, y;
      gint64 time;
      HyScanGeoGeodetic coords;

      if (!hyscan_nav_data_get (track->lat_data, index, &time, &coords.lat))
        continue;
      
      if (!hyscan_nav_data_get (track->lon_data, index, &time, &coords.lon))
        continue;

      hyscan_gtk_map_geo_to_value (priv->map, coords, &x_val, &y_val);
      gtk_cifro_area_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, x_val, y_val);
      cairo_line_to (cairo, x, y);
    }

  cairo_set_line_width (cairo, 2);
  cairo_set_source_rgb (cairo, 0, 0, 0);
  cairo_stroke (cairo);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_track_layer_draw (HyScanGtkMap            *map,
                                 cairo_t                 *cairo,
                                 HyScanGtkMapTrackLayer  *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GHashTableIter iter;

  HyScanGtkMapTrackLayerTrack *track;
  gchar *key;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    return;

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
    hyscan_gtk_map_track_layer_draw_track (track_layer, track, cairo);
}

/* Реализация HyScanGtkLayerInterface.added.
 * Обрабатывает добавление слой на карту. */
static void
hyscan_gtk_map_track_layer_added (HyScanGtkLayer          *gtk_layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_track_layer_draw), track_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Обрабатывает удаление слоя с карты. */
static void
hyscan_gtk_map_track_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, track_layer);
  g_clear_object (&priv->map);
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_track_layer_set_visible (HyScanGtkLayer *layer,
                                        gboolean        visible)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_track_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  return priv->visible;
}

static void
hyscan_gtk_map_track_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_track_layer_added;
  iface->removed = hyscan_gtk_map_track_layer_removed;
  iface->set_visible = hyscan_gtk_map_track_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_track_layer_get_visible;
}

/**
 * hyscan_gtk_map_track_layer_new:
 * @db
 * @project
 * @cache
 *
 * Returns: указатель на новый слой #HyScanGtkMapTrackLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_track_layer_new (HyScanDB    *db,
                                const gchar *project,
                                HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LAYER,
                       "db", db,
                       "project", project,
                       "cache", cache, NULL);
}

/**
 * hyscan_gtk_map_track_layer_track_enable:
 * @track_layer
 * @track_name
 * @enable
 *
 * Включает или отключает показ галса с названием @track_name.
 */
void
hyscan_gtk_map_track_layer_track_enable (HyScanGtkMapTrackLayer *track_layer,
                                         const gchar            *track_name,
                                         gboolean                enable)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  HyScanGtkMapTrackLayerTrack *track;
  
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));
  priv = track_layer->priv;

  track = g_hash_table_lookup (priv->tracks, track_name);

  if ((enable && track != NULL) || (!enable && track == NULL))
    return;

  if (enable)
    {
      track = g_new (HyScanGtkMapTrackLayerTrack, 1);
      track->lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
      track->lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
      g_hash_table_insert (priv->tracks, g_strdup (track_name), track);
    }
  else
    {
      g_hash_table_remove (priv->tracks, track_name);
    }

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}
