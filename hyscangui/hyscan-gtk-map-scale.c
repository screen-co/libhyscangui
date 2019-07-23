/* hyscan-gtk-map-scale.c
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
 * SECTION: hyscan-gtk-map-scale
 * @Short_description: Слой карты #HyScanGtkMap
 * @Title: HyScanGtkMapScale
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Слой с изображением текущего масштаба карты. Масштаб карты выводится в двух видах:
 * 1. численный масштаб в виде дроби, показывающий степень уменьшения проекции,
 *    например, 1:10000; степень уменьшения рассчитывается на основе PPI дисплея;
 * 2. графическое изображение линейки с подписью соответствующей её длины на местности.
 *
 * Стиль оформления слоя можно задать с помощью функций класса или свойств из файла
 * конфигурации:
 *
 * - hyscan_gtk_map_scale_set_bg_color()      "bg-color"
 * - hyscan_gtk_map_scale_set_label_color()   "label-color"
 * - hyscan_gtk_map_scale_set_scale_width()
 *
 * Загрузить конфигурационный файл можно функциями hyscan_gtk_layer_load_key_file()
 * или hyscan_gtk_layer_container_load_key_file().
 */

#include "hyscan-gtk-map-scale.h"
#include <glib/gi18n-lib.h>
#include <math.h>

#define MIN_SCALE_SIZE_PX   50.0      /* Минимальная длина линейки масштаба. */

#define SCALE_WIDTH         2.0       /* Толщина линии линейки масштаба. */

#define LINE_COLOR_DEFAULT  "rgba( 80, 120, 180, 0.5)"   /* Цвет линий по умолчанию. */
#define LABEL_COLOR_DEFAULT "rgba( 33,  33,  33, 1.0)"   /* Цвет текста подписей по умолчанию. */
#define BG_COLOR_DEFAULT    "rgba(255, 255, 255, 0.6)"   /* Цвет фона подписей по умолчанию. */

enum
{
  PROP_O,
};

struct _HyScanGtkMapScalePrivate
{
  HyScanGtkMap                     *map;                /* Виджет карты. */

  PangoLayout                      *pango_layout;       /* Раскладка шрифта. */
  guint                             min_scale_size;     /* Минимальная длина линейки масштаба. */

  GdkRGBA                           label_color;        /* Цвет подписей. */
  GdkRGBA                           bg_color;           /* Фоновый цвет подписей. */
  guint                             label_padding;      /* Отступы подписей от края видимой области. */
  gdouble                           scale_width;        /* Ширина линии линейки масштаба. */

  gboolean                          visible;            /* Признак того, что слой отображается. */

};

static void     hyscan_gtk_map_scale_object_constructed      (GObject                 *object);
static void     hyscan_gtk_map_scale_object_finalize         (GObject                 *object);
static void     hyscan_gtk_map_scale_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_map_scale_draw                    (HyScanGtkMap            *map,
                                                              cairo_t                 *cairo,
                                                              HyScanGtkMapScale       *scale);
static gboolean hyscan_gtk_map_scale_configure               (HyScanGtkMapScale       *scale,
                                                              GdkEvent                *screen);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapScale, hyscan_gtk_map_scale, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapScale)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_scale_interface_init))

static void
hyscan_gtk_map_scale_class_init (HyScanGtkMapScaleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_scale_object_constructed;
  object_class->finalize = hyscan_gtk_map_scale_object_finalize;
}

static void
hyscan_gtk_map_scale_init (HyScanGtkMapScale *gtk_map_scale)
{
  gtk_map_scale->priv = hyscan_gtk_map_scale_get_instance_private (gtk_map_scale);
}

static void
hyscan_gtk_map_scale_object_constructed (GObject *object)
{
  HyScanGtkMapScale *gtk_map_scale = HYSCAN_GTK_MAP_SCALE (object);
  HyScanGtkMapScalePrivate *priv = gtk_map_scale->priv;
  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_scale_parent_class)->constructed (object);

  priv->label_padding = 2;
  priv->min_scale_size = MIN_SCALE_SIZE_PX;

  gdk_rgba_parse (&color, LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_scale_set_label_color (gtk_map_scale, color);

  gdk_rgba_parse (&color, BG_COLOR_DEFAULT);
  hyscan_gtk_map_scale_set_bg_color (gtk_map_scale, color);

  hyscan_gtk_map_scale_set_scale_width (gtk_map_scale, SCALE_WIDTH);
}

static void
hyscan_gtk_map_scale_object_finalize (GObject *object)
{
  HyScanGtkMapScale *gtk_map_scale = HYSCAN_GTK_MAP_SCALE (object);
  HyScanGtkMapScalePrivate *priv = gtk_map_scale->priv;

  g_clear_object (&priv->pango_layout);
  g_object_unref (priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_scale_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_scale_set_visible (HyScanGtkLayer *layer,
                                 gboolean        visible)
{
  HyScanGtkMapScalePrivate *priv = HYSCAN_GTK_MAP_SCALE (layer)->priv;
  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_scale_get_visible (HyScanGtkLayer *layer)
{
  return HYSCAN_GTK_MAP_SCALE (layer)->priv->visible;
}

static void
hyscan_gtk_map_scale_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapScalePrivate *priv = HYSCAN_GTK_MAP_SCALE (gtk_layer)->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));

  g_signal_connect_after (priv->map, "area-draw",
                          G_CALLBACK (hyscan_gtk_map_scale_draw), gtk_layer);
  g_signal_connect_swapped (priv->map, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_scale_configure), gtk_layer);
}

static gboolean
hyscan_gtk_map_scale_load_key_file (HyScanGtkLayer          *gtk_layer,
                                    GKeyFile                *key_file,
                                    const gchar             *group)
{
  HyScanGtkMapScale *scale_layer = HYSCAN_GTK_MAP_SCALE (gtk_layer);
  HyScanGtkMapScalePrivate *priv = scale_layer->priv;

  GdkRGBA color;

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "bg-color", BG_COLOR_DEFAULT);
  hyscan_gtk_map_scale_set_bg_color (scale_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "label-color", LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_scale_set_label_color (scale_layer, color);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_scale_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->set_visible = hyscan_gtk_map_scale_set_visible;
  iface->get_visible = hyscan_gtk_map_scale_get_visible;
  iface->added = hyscan_gtk_map_scale_added;
  iface->load_key_file = hyscan_gtk_map_scale_load_key_file;
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_scale_configure (HyScanGtkMapScale *scale,
                                GdkEvent         *screen)
{
  HyScanGtkMapScalePrivate *priv = scale->priv;
  gint width;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), "01234 km");

  pango_layout_get_size (priv->pango_layout, &width, NULL);
  if (width < 0)
    width = 0;

  width /= PANGO_SCALE;
  priv->label_padding = width / 20;
  priv->min_scale_size = 2 * priv->label_padding + MAX (MIN_SCALE_SIZE_PX, width);

  return GDK_EVENT_PROPAGATE;
}

/* Рисует линейку масштаба карты по сигналу "area-draw". */
static void
hyscan_gtk_map_scale_draw (HyScanGtkMap      *map,
                           cairo_t           *cairo,
                           HyScanGtkMapScale *scale_layer)
{
  HyScanGtkMapScalePrivate *priv = scale_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  guint width, height;
  gdouble x0, y0;

  gdouble scale;
  gdouble metres;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (scale_layer)))
    return;

  /* Координаты размещения линейки. */
  gtk_cifro_area_get_visible_size (carea, &width, &height);
  x0 = width - priv->label_padding;
  y0 = height - priv->label_padding;

  /* Определяем размер линейки, чтобы он был круглым числом метров. */
  scale = hyscan_gtk_map_get_scale_px (priv->map);
  metres = 1.6 * priv->min_scale_size / scale;
  metres = pow (10, floor (log10 (metres)));
  while (metres * scale < priv->min_scale_size)
    metres *= 2.0;

  /* Рисуем линейку и подпись к ней. */
  {
    PangoRectangle inc_rect, logical_rect;

    gchar label[128];
    gint label_width, label_height;

    gchar scale_name[128];
    gint scale_name_width, scale_name_height;

    gdouble ruler_width;

    gdouble spacing;

    gdouble real_scale;

    real_scale = 1.0 / hyscan_gtk_map_get_scale_ratio (priv->map);

    /* Название масштаба. */
    if (real_scale > 10.0)
      g_snprintf (scale_name, sizeof (scale_name), "1:%.0f", real_scale);
    else if (real_scale > 1.0)
      g_snprintf (scale_name, sizeof (scale_name), "1:%.1f", real_scale);
    else if (real_scale > 0.1)
      g_snprintf (scale_name, sizeof (scale_name), "%.1f:1", 1 / real_scale);
    else
      g_snprintf (scale_name, sizeof (scale_name), "%.0f:1", 1 / real_scale);

    /* Различный форматы записи длины линейки масштаба. */
    if (metres > 1000.0)
      g_snprintf (label, sizeof (label), "%.0f %s", metres / 1000.0, _("km"));
    else if (metres > 1.0)
      g_snprintf (label, sizeof (label), "%.0f %s", metres, _("m"));
    else if (metres > 0.01)
      g_snprintf (label, sizeof (label), "%.0f %s", metres * 100.0, _("cm"));
    else
      g_snprintf (label, sizeof (label), "%.0f %s", metres * 1000.0, _("mm"));

    /* Вычисляем размеры подписи линейки. */
    pango_layout_set_text (priv->pango_layout, label, -1);
    pango_layout_get_pixel_extents (priv->pango_layout, &inc_rect, &logical_rect);
    label_width = logical_rect.width;
    label_height = logical_rect.height;
    spacing = label_height;

    /* Вычисляем размеры названия масштаба. */
    pango_layout_set_text (priv->pango_layout, scale_name, -1);
    pango_layout_get_pixel_extents (priv->pango_layout, &inc_rect, &logical_rect);
    scale_name_width = logical_rect.width;
    scale_name_height = logical_rect.height;

    ruler_width = metres * scale;

    /* Рисуем подложку. */
    gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
    cairo_rectangle (cairo, x0 + priv->label_padding, y0 + priv->label_padding,
                     - (scale_name_width + ruler_width + spacing + 2.0 * priv->label_padding),
                     - (label_height + 2.0 * priv->label_padding + priv->scale_width));
    cairo_fill (cairo);

    /* Рисуем линейку масштаба. */
    cairo_move_to (cairo, x0 - priv->scale_width / 2.0, y0 - label_height / 2.0);
    cairo_rel_line_to (cairo, 0, label_height / 2.0);
    cairo_rel_line_to (cairo, -ruler_width, 0);
    cairo_rel_line_to (cairo, 0, -label_height / 2.0);
    gdk_cairo_set_source_rgba (cairo, &priv->label_color);
    cairo_set_line_width (cairo, priv->scale_width);
    cairo_stroke (cairo);

    /* Рисуем название масштаба. */
    pango_layout_set_text (priv->pango_layout, scale_name, -1);
    cairo_move_to (cairo,
                   x0 - ruler_width - spacing - scale_name_width,
                   y0 - scale_name_height - priv->scale_width);
    pango_cairo_show_layout (cairo, priv->pango_layout);

    /* Рисуем подпись линейке. */
    {
      pango_layout_set_text (priv->pango_layout, label, -1);
      cairo_move_to (cairo,
                     x0 - label_width / 2.0 - ruler_width / 2.0,
                     y0 - label_height - priv->scale_width);
      pango_cairo_show_layout (cairo, priv->pango_layout);
    }
  }
}

static void
hyscan_gtk_map_scale_queue_draw (HyScanGtkMapScale *scale)
{
  if (scale->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (scale)))
    gtk_widget_queue_draw (GTK_WIDGET (scale->priv->map));
}

/**
 * hyscan_gtk_map_scale_new:
 *
 * Создаёт слой с информацией о масштабе.
 *
 * Returns: указатель на созданный слой
 */
HyScanGtkLayer *
hyscan_gtk_map_scale_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_SCALE, NULL);
}

/**
 * hyscan_gtk_map_scale_set_bg_color:
 * @scale: указатель на #HyScanGtkMapScale
 * @color: цвет подложки подписей
 *
 * Устанавливает цвет подложки подписей
 */
void
hyscan_gtk_map_scale_set_bg_color (HyScanGtkMapScale *scale,
                                   GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_SCALE (scale));

  scale->priv->bg_color = color;
  hyscan_gtk_map_scale_queue_draw (scale);
}

/**
 * hyscan_gtk_map_scale_set_scale_width:
 * @scale: указатель на #HyScanGtkMapScale
 * @width: толщина линии в изображении линейки масштаба
 *
 * Устанавливает толщину линии в изображении линейки масштаба.
 */
void
hyscan_gtk_map_scale_set_scale_width (HyScanGtkMapScale *scale,
                                      gdouble           width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_SCALE (scale));
  g_return_if_fail (width > 0);

  scale->priv->scale_width = width;
  hyscan_gtk_map_scale_queue_draw (scale);
}

/**
 * hyscan_gtk_map_scale_set_label_color:
 * @scale: указатель на #HyScanGtkMapScale
 * @color: цвет текста
 *
 * Устанавливает цвет текста
 */
void
hyscan_gtk_map_scale_set_label_color (HyScanGtkMapScale *scale,
                                      GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_SCALE (scale));

  scale->priv->label_color = color;
  hyscan_gtk_map_scale_queue_draw (scale);
}
