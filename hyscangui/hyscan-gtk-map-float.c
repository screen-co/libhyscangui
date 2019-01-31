#include "hyscan-gtk-map-float.h"
#include <gtk-cifro-area.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP
};

struct _HyScanGtkMapFloatPrivate
{
  HyScanGtkMap                *map;
};

static void    hyscan_gtk_map_float_set_property             (GObject                *object,
                                                              guint                   prop_id,
                                                              const GValue           *value,
                                                              GParamSpec             *pspec);
static void    hyscan_gtk_map_float_object_constructed       (GObject                *object);
static void    hyscan_gtk_map_float_object_finalize          (GObject                *object);
static void    hyscan_gtk_map_float_draw                     (HyScanGtkMapFloat      *layer,
                                                              cairo_t                *cairo);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapFloat, hyscan_gtk_map_float, G_TYPE_OBJECT)

static void
hyscan_gtk_map_float_class_init (HyScanGtkMapFloatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_float_set_property;

  object_class->constructed = hyscan_gtk_map_float_object_constructed;
  object_class->finalize = hyscan_gtk_map_float_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "Map", "GtkMap object", HYSCAN_TYPE_GTK_MAP,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_float_init (HyScanGtkMapFloat *gtk_map_float)
{
  gtk_map_float->priv = hyscan_gtk_map_float_get_instance_private (gtk_map_float);
}

static void
hyscan_gtk_map_float_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapFloat *gtk_map_float = HYSCAN_GTK_MAP_FLOAT (object);
  HyScanGtkMapFloatPrivate *priv = gtk_map_float->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_float_object_constructed (GObject *object)
{
  HyScanGtkMapFloat *gtk_map_float = HYSCAN_GTK_MAP_FLOAT (object);
  HyScanGtkMapFloatPrivate *priv = gtk_map_float->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_float_parent_class)->constructed (object);

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_float_draw), gtk_map_float);
}

static void
hyscan_gtk_map_float_object_finalize (GObject *object)
{
  HyScanGtkMapFloat *gtk_map_float = HYSCAN_GTK_MAP_FLOAT (object);
  HyScanGtkMapFloatPrivate *priv = gtk_map_float->priv;

  /* Отключаемся от сигналов карты. */
  g_signal_handlers_disconnect_by_data (priv->map, gtk_map_float);

  g_object_unref (priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_float_parent_class)->finalize (object);
}

/* Рисует слой на карте по сигналу "visible-draw". */
static void
hyscan_gtk_map_float_draw (HyScanGtkMapFloat *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapFloatPrivate *priv = layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  /* Размеры линейки. */
  gint margin = 6;
  gint height = 5;
  gdouble max_ruler = 100;

  guint visible_width, visible_height;
  gdouble x0, y0;

  gdouble scale;
  gdouble metres;

  /* Координаты размещения линейки. */
  gtk_cifro_area_get_visible_size (carea, &visible_width, &visible_height);
  x0 = visible_width - max_ruler - margin;
  y0 = visible_height - margin;

  /* Определяем размер линейки, чтобы он был круглым числом метров. */
  scale = hyscan_gtk_map_get_scale (priv->map);
  metres = 80 / scale;
  metres = pow (10, floor (log10 (metres)));
  while (metres * scale < max_ruler / 2)
    metres *= 2;

  /* Рисуем линейку и подпись к ней. */
  {
    gchar label[256];
    cairo_text_extents_t extents;
    gdouble text_width;

    /* Формируем текст надписи и вычисляем её размер. */
    if (metres < 1000)
      g_snprintf (label, 256, "%.0f m", metres);
    else
      g_snprintf (label, 256, "%.0f km", metres / 1000);
    cairo_text_extents (cairo, label, &extents);
    text_width = extents.width + 10;

    /* Фоновый цвет линейки, чтобы не сливалась с картой. */
    cairo_rectangle (cairo, x0 - text_width - margin, y0 - margin,
                     2 * margin + text_width + max_ruler, 2 * margin + height);
    cairo_set_source_rgba (cairo, 1, 1, 1, 0.80);
    cairo_fill (cairo);

    /* Рисуем линейку. */
    cairo_set_source_rgb (cairo, 0, 0, 0);
    cairo_set_line_width (cairo, 2);
    cairo_move_to (cairo, x0, y0);
    cairo_line_to (cairo, x0, y0 + height);
    cairo_line_to (cairo, x0 + metres * scale, y0 + height);
    cairo_line_to (cairo, x0 + metres * scale, y0);
    cairo_stroke (cairo);

    /* Рисуем надпись. */
    cairo_move_to (cairo, x0 - text_width, y0 + height);
    cairo_show_text (cairo, label);
  }
}

HyScanGtkMapFloat *
hyscan_gtk_map_float_new (HyScanGtkMap *map)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_FLOAT,
                       "map", map, NULL);
}