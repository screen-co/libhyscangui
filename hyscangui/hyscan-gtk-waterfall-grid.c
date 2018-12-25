/*
 * \file hyscan-gtk-waterfall-grid.c
 *
 * \brief Исходный файл сетки для виджета водопад
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfall-grid.h"
#include <hyscan-gtk-waterfall-tools.h>
#include <hyscan-tile-color.h>
#include <math.h>
#include <glib/gi18n.h>

#define SIGN(x) ((x)/(ABS(x)))
enum
{
  PROP_WATERFALL = 1
};

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
  HyScanGtkWaterfallState *wf_state;
  HyScanGtkWaterfall      *wfall;
  gboolean                 layer_visibility;

  GtkAdjustment    *hadjustment;
  GtkAdjustment    *vadjustment;

  PangoLayout      *font;              /* Раскладка шрифта. */
  gint              text_height;       /* Максимальная высота текста. */
  gint              virtual_border;    /* Виртуальная граница изображения. */

  GdkRGBA           grid_color;        /* Цвет осей. */
  GdkRGBA           text_color;        /* Цвет подписей. */
  GdkRGBA           shad_color;        /* Цвет подложки. */

  gchar            *x_axis_name;       /* Подпись горизонтальной оси. */
  gchar            *y_axis_name;       /* Подпись вертикальной оси. */

  gboolean          show_info;         /* Рисовать ли информационное окошко. */
  gint              info_coordinates;  /* Как вычислять координаты окошка. */
  gint              info_x;            /* Абсолютная координата x информационного окошка. */
  gint              info_y;            /* Абсолютная координата y информационного окошка. */
  gdouble           info_x_perc;       /* Процентная координата x информационного окошка. */
  gdouble           info_y_perc;       /* Процентная координата y информационного окошка. */

  gint              mouse_x;           /* Координата x мышки. */
  gint              mouse_y;           /* Координата y мышки. */

  gboolean          draw_grid;         /* Рисовать ли линии сетки. */
  gdouble           x_grid_step;       /* Шаг горизонтальных линий в метрах. 0 для автоматического рассчета. */
  gdouble           y_grid_step;       /* Шаг вертикальных линий в метрах. 0 для автоматического рассчета. */

  HyScanWaterfallDisplayType display_type;

  gdouble condence;
};

static void     hyscan_gtk_waterfall_grid_interface_init         (HyScanGtkWaterfallLayerInterface *iface);
static void     hyscan_gtk_waterfall_grid_set_property           (GObject               *object,
                                                                  guint                  prop_id,
                                                                  const GValue          *value,
                                                                  GParamSpec            *pspec);
static void     hyscan_gtk_waterfall_grid_object_constructed     (GObject               *object);
static void     hyscan_gtk_waterfall_grid_object_finalize        (GObject               *object);

static void     hyscan_gtk_waterfall_grid_set_visible            (HyScanGtkWaterfallLayer *layer,
                                                                  gboolean                 visible);
static const gchar* hyscan_gtk_waterfall_grid_get_mnemonic       (HyScanGtkWaterfallLayer *layer);

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

static void     hyscan_gtk_waterfall_grid_changed                (HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_vertical               (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_horisontal             (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);
static void     hyscan_gtk_waterfall_grid_info                   (GtkWidget             *widget,
                                                                  cairo_t               *cairo,
                                                                  HyScanGtkWaterfallGrid *self);


G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallGrid, hyscan_gtk_waterfall_grid, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkWaterfallGrid)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER, hyscan_gtk_waterfall_grid_interface_init));

static void
hyscan_gtk_waterfall_grid_class_init (HyScanGtkWaterfallGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_grid_set_property;
  object_class->constructed = hyscan_gtk_waterfall_grid_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_grid_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "GtkWaterfall object",
                         HYSCAN_TYPE_GTK_WATERFALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_grid_init (HyScanGtkWaterfallGrid *self)
{
  self->priv = hyscan_gtk_waterfall_grid_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_grid_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (object);

  if (prop_id == PROP_WATERFALL)
    {
      self->priv->wfall = g_value_dup_object (value);
      self->priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_grid_object_constructed (GObject *object)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (object);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;
  GdkRGBA text_color, grid_color, shad_color;

  priv->x_axis_name = g_strdup ("↔");
  priv->y_axis_name = g_strdup ("↕");
  priv->draw_grid = TRUE;
  priv->show_info = TRUE;
  priv->info_coordinates = HYSCAN_GTK_WATERFALL_GRID_TOP_RIGHT;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_grid_parent_class)->constructed (object);

  g_signal_connect (priv->wf_state, "visible-draw", G_CALLBACK (hyscan_gtk_waterfall_grid_draw), self);
  g_signal_connect (priv->wf_state, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_grid_configure), self);
  g_signal_connect (priv->wf_state, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_grid_motion_notify), self);
  g_signal_connect (priv->wf_state, "leave-notify-event", G_CALLBACK (hyscan_gtk_waterfall_grid_leave_notify), self);

  /* Сигналы модели. */
  g_signal_connect (priv->wf_state, "changed::sources", G_CALLBACK (hyscan_gtk_waterfall_grid_sources_changed), self);

  gdk_rgba_parse (&text_color, "#d6f8d6");
  gdk_rgba_parse (&grid_color, FRAME_DEFAULT);
  gdk_rgba_parse (&shad_color, SHADOW_DEFAULT);

  hyscan_gtk_waterfall_grid_set_grid_step (self, 100, 100);

  priv->text_color = text_color;
  priv->grid_color = grid_color;
  priv->shad_color = shad_color;

  priv->hadjustment = gtk_adjustment_new (0, 0, 0, 0, 1, 1);
  priv->vadjustment = gtk_adjustment_new (0, 0, 0, 0, 1, 1);

  g_signal_connect_swapped (priv->hadjustment, "changed", G_CALLBACK (hyscan_gtk_waterfall_grid_changed), self);
  g_signal_connect_swapped (priv->hadjustment, "value-changed", G_CALLBACK (hyscan_gtk_waterfall_grid_changed), self);
  g_signal_connect_swapped (priv->vadjustment, "changed", G_CALLBACK (hyscan_gtk_waterfall_grid_changed), self);
  g_signal_connect_swapped (priv->vadjustment, "value-changed", G_CALLBACK (hyscan_gtk_waterfall_grid_changed), self);

  /* Включаем видимость слоя. */
  hyscan_gtk_waterfall_layer_set_visible (HYSCAN_GTK_WATERFALL_LAYER (self), TRUE);
}

static void
hyscan_gtk_waterfall_grid_object_finalize (GObject *object)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (object);
  HyScanGtkWaterfallGridPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wf_state, self);

  g_clear_pointer (&priv->font, g_object_unref);
  g_free (priv->x_axis_name);
  g_free (priv->y_axis_name);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_grid_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_grid_set_visible (HyScanGtkWaterfallLayer *layer,
                                       gboolean                 visible)
{
  HyScanGtkWaterfallGrid *self = HYSCAN_GTK_WATERFALL_GRID (layer);

  self->priv->layer_visibility = visible;
  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

static const gchar*
hyscan_gtk_waterfall_grid_get_mnemonic (HyScanGtkWaterfallLayer *iface)
{
  return "waterfall-grid";
}


/* Функция создает новый виджет HyScanGtkWaterfallGrid. */
HyScanGtkWaterfallGrid*
hyscan_gtk_waterfall_grid_new (HyScanGtkWaterfall *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_GRID,
                       "waterfall", waterfall,
                       NULL);
}

GtkAdjustment*
hyscan_gtk_waterfall_grid_get_hadjustment (HyScanGtkWaterfallGrid *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self), NULL);

  return self->priv->hadjustment;
}

GtkAdjustment*
hyscan_gtk_waterfall_grid_get_vadjustment (HyScanGtkWaterfallGrid *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self), NULL);

  return self->priv->vadjustment;
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

  return FALSE;
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
  return FALSE;
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
  return FALSE;
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

  {
    gdouble from_x, to_x, from_y, to_y, min_x, max_x, min_y, max_y;
    gtk_cifro_area_get_view (GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
    gtk_cifro_area_get_limits (GTK_CIFRO_AREA (widget), &min_x, &max_x, &min_y, &max_y);

    gtk_adjustment_set_lower (priv->hadjustment, min_x);
    gtk_adjustment_set_upper (priv->hadjustment, max_x);
    gtk_adjustment_set_value (priv->hadjustment, from_x);
    gtk_adjustment_set_page_size (priv->hadjustment, ABS (to_x - from_x));

    gtk_adjustment_set_lower (priv->vadjustment, min_y);
    gtk_adjustment_set_upper (priv->vadjustment, max_y);
    gtk_adjustment_set_value (priv->vadjustment, from_y);
    gtk_adjustment_set_page_size (priv->vadjustment, ABS (to_y - from_y));
  }
}

static void
hyscan_gtk_waterfall_grid_changed (HyScanGtkWaterfallGrid *self)
{
  HyScanGtkWaterfallGridPrivate *priv = self->priv;
  gdouble from_x, to_x, from_y, to_y;
  g_object_get (priv->hadjustment,
                "value", &from_x,
                "page-size", &to_x,
                NULL);

  g_object_get (priv->vadjustment,
                "value", &from_y,
                "page-size", &to_y,
                NULL);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->wfall), from_x, from_x + to_x, from_y, from_y + to_y);
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

  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция позволяет включить и выключить отображение окошка с координатой. */
void
hyscan_gtk_waterfall_grid_show_info (HyScanGtkWaterfallGrid *self,
                                     gboolean                show_info)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->show_info = show_info;

  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

/* Функция задает координаты информационного окна по умолчанию. */
void
hyscan_gtk_waterfall_grid_set_info_position (HyScanGtkWaterfallGrid             *self,
                                             HyScanGtkWaterfallGridInfoPosition  position)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->info_coordinates = position;

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

  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}

void
hyscan_gtk_waterfall_grid_set_condence (HyScanGtkWaterfallGrid *self,
                                        gdouble                 condence)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_GRID (self));

  self->priv->condence = condence;

  hyscan_gtk_waterfall_queue_draw (self->priv->wfall);
}


static void
hyscan_gtk_waterfall_grid_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = NULL;
  iface->set_visible = hyscan_gtk_waterfall_grid_set_visible;
  iface->get_mnemonic = hyscan_gtk_waterfall_grid_get_mnemonic;
}

GtkWidget *
hyscan_gtk_waterfall_grid_make_grid (HyScanGtkWaterfallGrid *grid,
                                     GtkWidget              *child)
{
  GtkWidget * container;
  GtkWidget * hscroll, * vscroll;
  GtkAdjustment * hadj, * vadj;

  container = gtk_grid_new ();
  hadj = hyscan_gtk_waterfall_grid_get_hadjustment (grid);
  vadj = hyscan_gtk_waterfall_grid_get_vadjustment (grid);

  hscroll = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadj);
  vscroll = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadj);

  gtk_widget_set_hexpand (child, TRUE);
  gtk_widget_set_vexpand (child, TRUE);
  gtk_range_set_inverted (GTK_RANGE (vscroll), TRUE);

  gtk_grid_attach (GTK_GRID (container), child,   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (container), hscroll, 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (container), vscroll, 1, 0, 1, 1);

  return container;
}
