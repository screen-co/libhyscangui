/* hyscan-gtk-waterfall-grid.h
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-gtk-waterfall-grid
 * @Title: HyScanGtkWaterfallGrid
 * @Short_description: Координатная сетка для водопада
 *
 * Слой #HyScanGtkWaterfallGrid рисует координатную сетку для отображаемой области.
 */
#include "hyscan-gtk-waterfall-grid.h"
#include "hyscan-gtk-waterfall-tools.h"
#include <hyscan-tile-color.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define SIGN(x) ((x)/(ABS(x)))

enum
{
  VALUE_X,
  VALUE_Y,
  UNITS_X,
  UNITS_Y,
  TEXT_LAST
};

struct _HyScanGtkWaterfallGridPrivate
{
  HyScanGtkWaterfall *wfall;
  gboolean            layer_visibility;

  PangoLayout        *font;              /* Раскладка шрифта. */
  gint                text_height;       /* Максимальная высота текста. */
  gint                virtual_border;    /* Виртуальная граница изображения. */

  GdkRGBA             grid_color;        /* Цвет осей. */
  GdkRGBA             text_color;        /* Цвет подписей. */
  GdkRGBA             shad_color;        /* Цвет подложки. */

  gchar              *x_axis_name;       /* Подпись горизонтальной оси. */
  gchar              *y_axis_name;       /* Подпись вертикальной оси. */

  gboolean            show_info;         /* Рисовать ли информационное окошко. */
  gint                info_coordinates;  /* Как вычислять координаты окошка. */
  gint                info_x;            /* Абсолютная координата x информационного окошка. */
  gint                info_y;            /* Абсолютная координата y информационного окошка. */
  gdouble             info_x_perc;       /* Процентная координата x информационного окошка. */
  gdouble             info_y_perc;       /* Процентная координата y информационного окошка. */

  gint                mouse_x;           /* Координата x мышки. */
  gint                mouse_y;           /* Координата y мышки. */

  gboolean            draw_grid;         /* Рисовать ли линии сетки. */
  gdouble             x_grid_step;       /* Шаг горизонтальных линий в метрах. 0 для автоматического рассчета. */
  gdouble             y_grid_step;       /* Шаг вертикальных линий в метрах. 0 для автоматического рассчета. */

  HyScanWaterfallDisplayType display_type;

  gdouble condence;
};

static void     hyscan_gtk_waterfall_grid_interface_init         (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_waterfall_grid_object_constructed     (GObject               *object);
static void     hyscan_gtk_waterfall_grid_object_finalize        (GObject               *object);

static void     hyscan_gtk_waterfall_grid_added                  (HyScanGtkLayer            *layer,
                                                                  HyScanGtkLayerContainer   *container);
static void     hyscan_gtk_waterfall_grid_removed                (HyScanGtkLayer            *layer);
static void     hyscan_gtk_waterfall_grid_set_visible            (HyScanGtkLayer          *layer,
                                                                  gboolean                 visible);
static const gchar* hyscan_gtk_waterfall_grid_get_icon_name      (HyScanGtkLayer          *layer);

static gboolean hyscan_gtk_waterfall_grid_configure              (GtkWidget             *widget,
                                                                  GdkEventConfigure     *event,
                                                                  HyScanGtkWaterfallGrid *self);
static gboolean hyscan_gtk_waterfall_grid_motion_notify          (GtkWidget             *widget,
                                                                  GdkEventMotion        *event,
                                                                  HyScanGtkWaterfallGrid *self);
static gboolean hyscan_gtk_waterfall_grid_leave_notify           (GtkWidget             *widget,
                                                                  GdkEventCrossing      *event,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_sources_changed        (HyScanGtkWaterfallState   *state,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_draw                   (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);

static void     hyscan_gtk_waterfall_grid_vertical               (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_horisontal             (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_info                   (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);


G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallGrid, hyscan_gtk_waterfall_grid, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallGrid)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_grid_interface_init));

static void
hyscan_gtk_waterfall_grid_class_init (HyScanGtkWaterfallGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_grid_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_grid_object_finalize;
}

static void
hyscan_gtk_waterfall_grid_init (HyScanGtkWaterfallGrid *self)
{
  self->priv = hyscan_gtk_waterfall_grid_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_grid_object_constructed (GObject *object)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (object);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_grid_parent_class)->constructed (object);

  priv->x_axis_name = g_strdup ("↔");
  priv->y_axis_name = g_strdup ("↕");
  priv->draw_grid = TRUE;
  priv->show_info = TRUE;
  priv->info_coordinates = HYSCAN_GTK_WATERFALL_GRID_TOP_RIGHT;

  gdk_rgba_parse (&priv->text_color, "#d6f8d6");
  gdk_rgba_parse (&priv->grid_color, FRAME_DEFAULT);
  gdk_rgba_parse (&priv->shad_color, SHADOW_DEFAULT);

  hyscan_gtk_waterfall_grid_set_grid_step (self, 100, 100);
  hyscan_gtk_waterfall_grid_set_condence (self, 1);

  /* Включаем видимость слоя. */
  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_grid_object_finalize (GObject *object)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (object);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  g_clear_pointer (&priv->font, g_object_unref);
  g_free (priv->x_axis_name);
  g_free (priv->y_axis_name);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_grid_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_grid_added (HyScanGtkLayer         *layer,
                                 HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfall *wfall;
  HyScanGtkWaterfallGrid *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_GRID (layer);
  wfall = HYSCAN_GTK_WATERFALL (container);
  self->priv->wfall = g_object_ref (wfall);

  /* Сигналы Gtk. */
  g_signal_connect (wfall, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_grid_draw), self);
  g_signal_connect (wfall, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_grid_configure), self);
  g_signal_connect (wfall, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_grid_motion_notify), self);
  g_signal_connect (wfall, "leave-notify-event", G_CALLBACK (hyscan_gtk_waterfall_grid_leave_notify), self);

  /* Сигналы модели. */
  g_signal_connect (wfall, "changed::sources", G_CALLBACK (hyscan_gtk_waterfall_grid_sources_changed), self);
}

static void
hyscan_gtk_waterfall_grid_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

static void
hyscan_gtk_waterfall_grid_set_visible (HyScanGtkLayer *layer,
                                       gboolean        visible)
{
  HyScanGtkWaterfallGridPrivate *priv = HYSCAN_GTK_WATERFALL_GRID (layer)->priv;

  priv->layer_visibility = visible;

  if (priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (priv->wfall);
}

static const gchar*
hyscan_gtk_waterfall_grid_get_icon_name (HyScanGtkLayer *iface)
{
  return "waterfall-grid";
}

/* Функция создает новый виджет HyScanGtkWaterfallGrid. */
HyScanGtkWaterfallGrid*
hyscan_gtk_waterfall_grid_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_GRID, NULL);
}

/* Функция обработки сигнала изменения параметров дисплея. */
static gboolean
hyscan_gtk_waterfall_grid_configure (GtkWidget            *widget,
                                     GdkEventConfigure    *event,
                                     HyScanGtkWaterfallGrid *self)
{
  HyScanGtkWaterfallGridPrivate *priv = self->priv;
  gint text_height, text_width;

  /* Текущий шрифт приложения. */
  g_clear_object (&priv->font);
  priv->font = gtk_widget_create_pango_layout (widget, NULL);

  pango_layout_set_text (priv->font, "0123456789.,m", -1);
  pango_layout_get_size (priv->font, NULL, &text_height);
  pango_layout_set_text (priv->font, "m", -1);
  pango_layout_get_size (priv->font, &text_width, NULL);

  priv->virtual_border = 1.2 * MAX (text_height, text_width) / PANGO_SCALE;
  priv->text_height = text_height / PANGO_SCALE;

  return GDK_EVENT_PROPAGATE;
}

/* Функция ловит сигнал на движение мыши. */
static gboolean
hyscan_gtk_waterfall_grid_motion_notify (GtkWidget              *widget,
                                         GdkEventMotion         *event,
                                         HyScanGtkWaterfallGrid *self)
{
  guint area_width, area_height;
  gint x = event->x;
  gint y = event->y;

  /* Проверяем, что координаты курсора находятся в рабочей области. */
  gtk_cifro_area_get_size (GTK_CIFRO_AREA (widget), &area_width, &area_height);

  self->priv->mouse_x = x;
  self->priv->mouse_y = y;

  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (widget));
  return GDK_EVENT_PROPAGATE;
}

/* Функция обработки сигнала выхода курсора за пределы окна. */
static gboolean
hyscan_gtk_waterfall_grid_leave_notify (GtkWidget            *widget,
                                        GdkEventCrossing     *event,
                                        HyScanGtkWaterfallGrid *self)
{
  self->priv->mouse_x = -1;
  self->priv->mouse_y = -1;

  hyscan_gtk_waterfall_queue_draw (HYSCAN_GTK_WATERFALL (widget));
  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_waterfall_grid_sources_changed (HyScanGtkWaterfallState   *state,
                                           HyScanGtkWaterfallGrid *self)
{
  self->priv->display_type = hyscan_gtk_waterfall_state_get_sources (state, NULL, NULL);
}

/* Обработчик сигнала visible-draw. */
static void
hyscan_gtk_waterfall_grid_draw (GtkWidget *widget,
                                cairo_t   *cairo,
                                HyScanGtkWaterfallGrid *self)
{
  HyScanGtkWaterfallGridPrivate *priv = self->priv;

  /* Проверяем видимость слоя. */
  if (!priv->layer_visibility)
    return;

  /* Информационное окошко. */
  if (priv->show_info)
    {
      hyscan_gtk_waterfall_grid_info (widget, cairo, self);
    }

  /* Сетка и подписи. */
  if (priv->draw_grid)
    {
      hyscan_gtk_waterfall_grid_vertical (widget, cairo, self);
      hyscan_gtk_waterfall_grid_horisontal (widget, cairo, self);
    }
}

/* Функция рисует сетку по горизонтальной оси. */
static void
hyscan_gtk_waterfall_grid_vertical (GtkWidget *widget,
                                    cairo_t   *cairo,
                                    HyScanGtkWaterfallGrid *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;
  PangoLayout *font = priv->font;

  guint area_width, area_height;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale, step;
  gdouble axis, axis_to, axis_pos, axis_from, axis_step, axis_scale;
  guint axis_range;
  gint axis_power;

  gchar text_format[128];
  gchar text_str[128];

  gint text_width;
  gint text_height = priv->text_height;

  gtk_cifro_area_get_size (carea, &area_width, &area_height);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (carea, &scale, &scale);

  /* Проверяем состояние объекта. */
  if (priv->font == NULL)
    return;

  /* TODO: некрасивое решение. Сейчас можно вручную установить шаг {1, 2, 5}*10^x. Аналогично и в У-осях. */
  if (priv->x_grid_step == 0.0)
    step = priv->virtual_border * 6.0;
  else
    step = priv->x_grid_step;

  axis_to = to_x;
  axis_from = from_x;
  axis_scale = scale;

  /* Шаг оцифровки. */
  gtk_cifro_area_get_axis_step (axis_scale, step, &axis_from, &axis_step, &axis_range, &axis_power);

  /* Линии. */
  {
    hyscan_cairo_set_source_gdk_rgba (cairo, &priv->grid_color);
    cairo_set_line_width (cairo, 1.0);

    axis = axis_from - axis_step;
    while (axis <= axis_to)
      {
        gtk_cifro_area_value_to_point (carea, &axis_pos, NULL, axis, 0.0);

        axis += axis_step;
        if (axis_pos <= priv->virtual_border)
          continue;
        if (axis_pos >= area_width - 1)
          continue;

        cairo_move_to (cairo, (gint) axis_pos + 0.5, text_height * 1.2);
        cairo_line_to (cairo, (gint) axis_pos + 0.5, area_height);
      }
    cairo_stroke (cairo);
  }

  /* Рисуем подписи на оси. */
  {
    if (axis_power > 0)
      axis_power = 0;
    g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) ABS (axis_power));

    axis = axis_from;
    while (axis <= axis_to)
      {
        gdouble x, y;
        g_ascii_formatd (text_str, sizeof(text_str), text_format, axis * priv->condence);
        pango_layout_set_text (font, text_str, -1);
        pango_layout_get_size (font, &text_width, NULL);
        text_width /= PANGO_SCALE;

        gtk_cifro_area_value_to_point (carea, &axis_pos, NULL, axis, 0.0);
        axis += axis_step;

        if (axis_pos <= priv->virtual_border)
          continue;

        axis_pos -= text_width / 2;

        if (axis_pos <= priv->virtual_border)
          {
            x = priv->virtual_border;
            y = text_height * 0.1;
          }
        else
          {
            x = axis_pos;
            y = text_height * 0.1;
          }

          /* Подложка под осью. */
          {
            cairo_rectangle (cairo, x - text_width * 0.1, y, text_width * 1.2, text_height);
            hyscan_cairo_set_source_gdk_rgba (cairo, &priv->shad_color);
            cairo_fill (cairo);
            hyscan_cairo_set_source_gdk_rgba (cairo, &priv->grid_color);
            cairo_rectangle (cairo, x - text_width * 0.1, y, text_width * 1.2, text_height);
            cairo_stroke (cairo);
          }

        hyscan_cairo_set_source_gdk_rgba (cairo, &priv->text_color);
        cairo_move_to (cairo, x, y);
        pango_cairo_show_layout (cairo, font);
      }
  }

  /* Рисуем название оси. */
  pango_layout_set_text (font, "m", -1);

  pango_layout_get_size (font, &text_width, &text_height);
  text_width /= PANGO_SCALE;
  text_height /= PANGO_SCALE;

  cairo_move_to (cairo, text_width * 0.1, text_height * 0.1);
  pango_cairo_show_layout (cairo, font);
}

/* Функция рисует сетку по вертикальной оси. */
static void
hyscan_gtk_waterfall_grid_horisontal (GtkWidget *widget,
                                      cairo_t   *cairo,
                                      HyScanGtkWaterfallGrid *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;
  PangoLayout *font = priv->font;

  guint area_width, area_height;

  gdouble from_x, to_x, from_y, to_y;
  gdouble scale, step;
  gdouble axis, axis_to, axis_pos, axis_from, axis_step, axis_scale;
  guint axis_range;
  gint axis_power;

  gchar text_format[128];
  gchar text_str[128];

  gint text_width;
  gint text_height = priv->text_height;

  gtk_cifro_area_get_size (carea, &area_width, &area_height);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (carea, &scale, NULL);

  /* Проверяем состояние объекта. */
  if (priv->font == NULL)
    return;

  if (priv->y_grid_step == 0.0)
    step = priv->virtual_border * 6.0;
  else
    step = priv->y_grid_step;

  axis_to = to_y;
  axis_from = from_y;
  axis_scale = scale;

  /* Шаг оцифровки. */
  gtk_cifro_area_get_axis_step (axis_scale, step, &axis_from, &axis_step, &axis_range, &axis_power);

  /* Рисуем линии сетки. */
  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->grid_color);
  cairo_set_line_width (cairo, 1.0);

  axis = axis_from - axis_step;
  while (axis <= axis_to)
    {
      gtk_cifro_area_value_to_point (carea, NULL, &axis_pos, 0.0, axis);

      axis += axis_step;
      if (axis_pos <= priv->virtual_border)
        continue;
      if (axis_pos >= area_height - 1)
        continue;

      cairo_move_to (cairo, 0, (gint) axis_pos + 0.5);
      cairo_line_to (cairo, area_width, (gint) axis_pos + 0.5);
    }

  cairo_stroke (cairo);

  /* Рисуем подписи на оси. */
  if (axis_power > 0)
    axis_power = 0;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) ABS (axis_power));

  axis = axis_from;
  while (axis <= axis_to)
    {
      gdouble x, y;

      if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
        g_ascii_formatd (text_str, sizeof(text_str), text_format, axis);
      else
        g_ascii_formatd (text_str, sizeof(text_str), text_format, ABS (axis));

      pango_layout_set_text (font, text_str, -1);
      pango_layout_get_size (font, &text_width, &text_height);

      text_width /= PANGO_SCALE;
      text_height /= PANGO_SCALE;

      gtk_cifro_area_value_to_point (carea, NULL, &axis_pos, 0.0, axis);
      axis += axis_step;

      if (axis_pos <= priv->virtual_border)
        continue;


      if (axis_pos - text_height <= priv->virtual_border)
        {
          x = text_height * 0.1;
          y = priv->virtual_border;
        }
      else
        {
          x = text_height * 0.1;
          y = (gint)axis_pos + 0.5 - text_height;
        }

      /* Подложка под осью. */
      {
        cairo_rectangle (cairo, x - text_width * 0.1, y, text_width * 1.2, text_height);
        hyscan_cairo_set_source_gdk_rgba (cairo, &priv->shad_color);
        cairo_fill (cairo);

        cairo_rectangle (cairo, x - text_width * 0.1, y, text_width * 1.2, text_height);
        hyscan_cairo_set_source_gdk_rgba (cairo, &priv->grid_color);
        cairo_stroke (cairo);
      }

      cairo_move_to (cairo, x, y);

      hyscan_cairo_set_source_gdk_rgba (cairo, &priv->text_color);
      pango_cairo_show_layout (cairo, font);
    }

  /* Рисуем название оси. */
  pango_layout_set_text (font, _("m"), -1);
  pango_layout_get_size (font, &text_width, &text_height);
  text_width /= PANGO_SCALE;
  text_height /= PANGO_SCALE;

  cairo_move_to (cairo, text_width * 0.1, text_height * 0.1);
  pango_cairo_show_layout (cairo, font);
}

/* Функция рисует информационное окошко. */
static void
hyscan_gtk_waterfall_grid_info (GtkWidget *widget,
                                cairo_t   *cairo,
                                HyScanGtkWaterfallGrid *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;

  PangoLayout *font = priv->font;

  guint area_width, area_height;
  gdouble scale, from_x, to_x, from_y, to_y;
  gint units_width, value_width, font_height;
  gint info_width, info_height, x, y;
  gdouble current_x, current_y;
  gdouble value, value_x, value_y;

  gint value_power;
  gchar text_format[128];
  gchar text_str[TEXT_LAST][128];

  gint text_width, text_height;
  gdouble text_spacing = priv->text_height / 8.0;
  gint border = priv->virtual_border;

  /* Значения под курсором. */
  if (priv->mouse_x < 0 || priv->mouse_y < 0)
    return;

  gtk_cifro_area_get_size (carea, &area_width, &area_height);
  gtk_cifro_area_get_scale (carea, &scale, NULL);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  gtk_cifro_area_point_to_value (carea, priv->mouse_x, priv->mouse_y, &value_x, &value_y);

  /* Составляем строки максимальной длины. */
  /* Для координаты Х. */
  value = value_x;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;

  g_snprintf (text_format, sizeof(text_format), "-%%.%df", (gint) ABS (value_power));
  g_snprintf (text_str[VALUE_X], sizeof(text_str[VALUE_X]), text_format, MAX( ABS( from_x * priv->condence ), ABS( to_x * priv->condence ) ));

  /* Для координаты У. */
  value = value_y;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;
  g_snprintf (text_str[VALUE_Y], sizeof(text_str[VALUE_Y]), text_format, MAX( ABS( from_y ), ABS( to_y ) ));

  /* Для единиц измерения. */
  g_snprintf (text_str[UNITS_X], sizeof(text_str[UNITS_X]), " %s", _(priv->x_axis_name));
  g_snprintf (text_str[UNITS_Y], sizeof(text_str[UNITS_Y]), " %s", _(priv->y_axis_name));

  /* Вычисляем максимальную ширину и высоту строки с текстом. */
  pango_layout_set_text (font, text_str[VALUE_X], -1);
  pango_layout_get_size (font, &text_width, &text_height);
  value_width = text_width;
  font_height = text_height;

  pango_layout_set_text (font, text_str[VALUE_Y], -1);
  pango_layout_get_size (font, &text_width, &text_height);
  value_width = MAX (text_width, value_width);
  font_height = MAX (text_height, font_height);

  pango_layout_set_text (font, text_str[UNITS_X], -1);
  pango_layout_get_size (font, &text_width, &text_height);
  font_height = MAX (text_height, font_height);
  units_width = text_width;

  pango_layout_set_text (font, text_str[UNITS_Y], -1);
  pango_layout_get_size (font, &text_width, &text_height);
  font_height = MAX (text_height, font_height);
  units_width = MAX (units_width, text_width);

  /* Ширина текста с названием величины, с её значением и высота строки. */
  value_width /= PANGO_SCALE;
  units_width /= PANGO_SCALE;
  font_height /= PANGO_SCALE;

  /* Размер места для отображения информации. */
  info_width = value_width + units_width + 2 * text_spacing;
  info_width /= 2;
  info_height = 2 * font_height + text_spacing * 3;
  info_height /= 2;

  /* Центр окошка. */
  y = info_height + text_spacing + border;

  if (priv->info_coordinates == HYSCAN_GTK_WATERFALL_GRID_BOTTOM_LEFT ||
      priv->info_coordinates == HYSCAN_GTK_WATERFALL_GRID_BOTTOM_RIGHT)
    {
      y = area_height - y;
    }

  x = info_width + text_spacing + border;

  if (priv->info_coordinates == HYSCAN_GTK_WATERFALL_GRID_TOP_RIGHT ||
      priv->info_coordinates == HYSCAN_GTK_WATERFALL_GRID_BOTTOM_RIGHT)
    {
      x = area_width - x;
    }

  /* Проверяем размеры области отображения. */
  if (x - info_width < 0 || y + info_height > (gint)area_height)
    return;

  /* Рисуем подложку окна. */
  cairo_set_line_width (cairo, 1.0);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->shad_color);
  cairo_rectangle (cairo, x + 0.5 - info_width, y + 0.5 - info_height, 2* info_width, 2* info_height);
  cairo_fill (cairo);

  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->grid_color);
  cairo_rectangle (cairo, x + 0.5 - info_width, y + 0.5 - info_height, 2* info_width, 2* info_height);
  cairo_stroke (cairo);

  /* Теперь составляем строки с фактическими координатами. */
  x = x + 0.5 + info_width;
  y = y + 0.5 - info_height;

  value = value_x;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) ABS (value_power));
  g_ascii_formatd (text_str[VALUE_X], sizeof(text_str[VALUE_X]), text_format, value_x * priv->condence);

  value = value_y;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) ABS (value_power));
  g_ascii_formatd (text_str[VALUE_Y], sizeof(text_str[VALUE_Y]), text_format, value_y);

  /* Всё. Теперь можно рисовать само окошко. */
  hyscan_cairo_set_source_gdk_rgba (cairo, &priv->text_color);

  /* Пишем единицу измерения Х. */
  current_y = y + text_spacing;
  current_x = x - (2.0 * text_spacing + units_width);

  pango_layout_set_text (font, text_str[UNITS_X], -1);
  cairo_move_to (cairo, current_x, current_y);
  pango_cairo_show_layout (cairo, font);

  /* Пишем значение Х. */
  pango_layout_set_text (font, text_str[VALUE_X], -1);
  pango_layout_get_size (font, &text_width, &text_height);
  cairo_move_to (cairo, current_x - text_width / PANGO_SCALE, current_y);
  pango_cairo_show_layout (cairo, font);

  /* Пишем единицу измерения У. */
  current_y = y + 2 * text_spacing + text_height / PANGO_SCALE;
  current_x = x - (2.0 * text_spacing + units_width);
  pango_layout_set_text (font, text_str[UNITS_Y], -1);
  cairo_move_to (cairo, current_x, current_y);
  pango_cairo_show_layout (cairo, font);

  /* Пишем значение У. */
  pango_layout_set_text (font, text_str[VALUE_Y], -1);
  pango_layout_get_size (font, &text_width, NULL);
  cairo_move_to (cairo, current_x - text_width / PANGO_SCALE, current_y);
  pango_cairo_show_layout (cairo, font);

}

/* Функция включает и отключает линии сетки. */
void
hyscan_gtk_waterfall_grid_show_grid (HyScanGtkWaterfallGrid *self,
                                     gboolean                draw)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->draw_grid = draw;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция позволяет включить и выключить отображение окошка с координатой. */
void
hyscan_gtk_waterfall_grid_show_info (HyScanGtkWaterfallGrid *self,
                                     gboolean                show_info)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->show_info = show_info;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает координаты информационного окна по умолчанию. */
void
hyscan_gtk_waterfall_grid_set_info_position (HyScanGtkWaterfallGrid             *self,
                                             HyScanGtkWaterfallGridInfoPosition  position)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->info_coordinates = position;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция устанавливает шаг координатной сетки. */
gboolean
hyscan_gtk_waterfall_grid_set_grid_step (HyScanGtkWaterfallGrid *self,
                                         gdouble    step_horisontal,
                                         gdouble    step_vertical)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self), FALSE);
  g_return_val_if_fail (step_horisontal >= 0.0, FALSE);
  g_return_val_if_fail (step_vertical >= 0.0, FALSE);

  self->priv->x_grid_step = step_horisontal;
  self->priv->y_grid_step = step_vertical;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
  return TRUE;
}

/* Функция устанавливает цвет координатной сетки. */
void
hyscan_gtk_waterfall_grid_set_grid_color (HyScanGtkWaterfallGrid *self,
                                          guint32                 color)
{
  GdkRGBA rgba;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  hyscan_tile_color_converter_i2d (color, &rgba.red, &rgba.green, &rgba.blue, &rgba.alpha);
  self->priv->grid_color = rgba;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция устанавливает цвет подписей. */
void
hyscan_gtk_waterfall_grid_set_main_color (HyScanGtkWaterfallGrid *self,
                                           guint32                 color)
{
  GdkRGBA rgba;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  hyscan_tile_color_converter_i2d (color, &rgba.red, &rgba.green, &rgba.blue, &rgba.alpha);
  self->priv->text_color = rgba;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция устанавливает цвет подписей. */
void
hyscan_gtk_waterfall_grid_set_shadow_color (HyScanGtkWaterfallGrid *self,
                                            guint32                 color)
{
  GdkRGBA rgba;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  hyscan_tile_color_converter_i2d (color, &rgba.red, &rgba.green, &rgba.blue, &rgba.alpha);
  self->priv->shad_color = rgba;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

void
hyscan_gtk_waterfall_grid_set_condence (HyScanGtkWaterfallGrid *self,
                                        gdouble                 condence)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->condence = condence;

  if (self->priv->wfall != NULL)
    hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static void
hyscan_gtk_waterfall_grid_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_waterfall_grid_added;
  iface->removed = hyscan_gtk_waterfall_grid_removed;
  iface->set_visible = hyscan_gtk_waterfall_grid_set_visible;
  iface->get_icon_name = hyscan_gtk_waterfall_grid_get_icon_name;
}
