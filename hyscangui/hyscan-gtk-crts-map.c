#include "hyscan-gtk-crts-map.h"

enum
{
  PROP_O,
  PROP_GEO
};

struct _HyScanGtkCrtsMapPrivate
{
  HyScanGeo *geo;
};

static void    hyscan_gtk_crts_map_set_property             (GObject            *object,
                                                             guint               prop_id,
                                                             const GValue       *value,
                                                             GParamSpec         *pspec);
static void    hyscan_gtk_crts_map_object_constructed       (GObject            *object);
static void    hyscan_gtk_crts_map_object_finalize          (GObject            *object);
static void    hyscan_gtk_crts_map_point_to_tile            (HyScanGtkMap       *map,
                                                             gdouble             x,
                                                             gdouble             y,
                                                             gdouble            *x_tile,
                                                             gdouble            *y_tile);
static void    hyscan_gtk_crts_map_tile_to_point            (HyScanGtkMap       *map,
                                                             gdouble            *x,
                                                             gdouble            *y,
                                                             gdouble             x_tile,
                                                             gdouble             y_tile);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkCrtsMap, hyscan_gtk_crts_map, HYSCAN_TYPE_GTK_MAP)

static void
hyscan_gtk_crts_map_class_init (HyScanGtkCrtsMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapClass *map_class = HYSCAN_GTK_MAP_CLASS (klass);

  object_class->set_property = hyscan_gtk_crts_map_set_property;

  object_class->constructed = hyscan_gtk_crts_map_object_constructed;
  object_class->finalize = hyscan_gtk_crts_map_object_finalize;

  g_object_class_install_property (object_class, PROP_GEO,
    g_param_spec_object ("geo", "Geo translator", "HyScanGeo", HYSCAN_TYPE_GEO,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  map_class->point_to_tile = hyscan_gtk_crts_map_point_to_tile;
  map_class->tile_to_point = hyscan_gtk_crts_map_tile_to_point;
}

static void
hyscan_gtk_crts_map_init (HyScanGtkCrtsMap *gtk_crts_map)
{
  gtk_crts_map->priv = hyscan_gtk_crts_map_get_instance_private (gtk_crts_map);
}

static void
hyscan_gtk_crts_map_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanGtkCrtsMap *gtk_crts_map = HYSCAN_GTK_CRTS_MAP (object);
  HyScanGtkCrtsMapPrivate *priv = gtk_crts_map->priv;

  switch (prop_id)
    {
    case PROP_GEO:
      priv->geo= g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_crts_map_object_constructed (GObject *object)
{
  HyScanGtkCrtsMap *gtk_crts_map = HYSCAN_GTK_CRTS_MAP (object);
  HyScanGtkCrtsMapPrivate *priv = gtk_crts_map->priv;

  G_OBJECT_CLASS (hyscan_gtk_crts_map_parent_class)->constructed (object);

}

static void
hyscan_gtk_crts_map_object_finalize (GObject *object)
{
  HyScanGtkCrtsMap *gtk_crts_map = HYSCAN_GTK_CRTS_MAP (object);
  HyScanGtkCrtsMapPrivate *priv = gtk_crts_map->priv;

  g_object_unref (priv->geo);

  G_OBJECT_CLASS (hyscan_gtk_crts_map_parent_class)->finalize (object);
}

/* Переводит координаты из картезианской СК в тайловую. */
static void
hyscan_gtk_crts_map_point_to_tile (HyScanGtkMap *map,
                                   gdouble       x,
                                   gdouble       y,
                                   gdouble      *x_tile,
                                   gdouble      *y_tile)
{
  HyScanGtkCrtsMap *cmap = HYSCAN_GTK_CRTS_MAP (map);
  HyScanGtkCrtsMapPrivate *priv = cmap->priv;

  HyScanGeoGeodetic geodetic;
  HyScanGeoCartesian2D cartesian;

  cartesian.x = x;
  cartesian.y = y;

  /* todo: сколько итераций? */
  hyscan_geo_topoXY2geo (priv->geo, &geodetic, cartesian, 0, 1);

  hyscan_gtk_map_geo_to_tile (hyscan_gtk_map_get_zoom (map), geodetic, x_tile, y_tile);
}

/* Переводит координаты из тайловой СК в картезианскую. */
static void
hyscan_gtk_crts_map_tile_to_point (HyScanGtkMap *map,
                                   gdouble      *x,
                                   gdouble      *y,
                                   gdouble       x_tile,
                                   gdouble       y_tile)
{
  HyScanGtkCrtsMap *cmap = HYSCAN_GTK_CRTS_MAP (map);
  HyScanGtkCrtsMapPrivate *priv = cmap->priv;

  HyScanGeoGeodetic geodetic;
  HyScanGeoCartesian2D cartesian;

  hyscan_gtk_map_tile_to_geo (hyscan_gtk_map_get_zoom (map), &geodetic, x_tile, y_tile);
  hyscan_geo_geo2topoXY (priv->geo, &cartesian, geodetic);

  (x != NULL) ? *x = cartesian.x : 0;
  (y != NULL) ? *y = cartesian.y : 0;
}


GtkWidget *
hyscan_gtk_crts_map_new (HyScanGeo *geo)
{
  return g_object_new (HYSCAN_TYPE_GTK_CRTS_MAP, "geo", geo, NULL);
}