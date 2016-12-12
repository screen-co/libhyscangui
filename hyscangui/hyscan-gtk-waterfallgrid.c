/*
 * \file hyscan-gtk-waterfallgrid.c
 *
 * \brief Исходный файл сетки для виджета водопад.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-gtk-waterfallgrid.h"
#include <hyscan-tile-color.h>
#include <math.h>
#include <glib/gi18n.h>

enum
{
  INFOBOX_AUTO,
  INFOBOX_ABSOLUTE,
  INFOBOX_PERCENT
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
  PangoLayout      *font;              /* Раскладка шрифта. */
  gint              text_height;       /* Максимальная высота текста. */
  gint              virtual_border;    /* Виртуальная граница изображения. */

  GdkRGBA           grid_color;        /* Цвет осей. */
  GdkRGBA           text_color;        /* Цвет подписей. */

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

  gboolean          draw_x_grid;       /* Рисовать ли горизонтальные линии сетки. */
  gboolean          draw_y_grid;       /* Рисовать ли вертикальные линии сетки. */
  gdouble           x_grid_step;       /* Шаг горизонтальных линий в метрах. 0 для автоматического рассчета. */
  gdouble           y_grid_step;       /* Шаг вертикальных линий в метрах. 0 для автоматического рассчета. */

};

static void    hyscan_gtk_waterfallgrid_object_constructed       (GObject               *object);
static void    hyscan_gtk_waterfallgrid_object_finalize          (GObject               *object);

static gboolean hyscan_gtk_waterfallgrid_configure               (GtkWidget             *widget,
                                                                  GdkEventConfigure     *event);
static gboolean hyscan_gtk_waterfallgrid_motion_notify           (GtkWidget             *widget,
                                                                  GdkEventMotion        *event);
static gboolean hyscan_gtk_waterfallgrid_leave_notify            (GtkWidget             *widget,
                                                                  GdkEventCrossing      *event);
static void    hyscan_gtk_waterfallgrid_visible_draw             (GtkWidget             *widget,
                                                                  cairo_t               *cairo);

static void    hyscan_gtk_waterfallgrid_hruler_drawer            (GtkWidget             *widget,
                                                                  cairo_t               *cairo);
static void    hyscan_gtk_waterfallgrid_vruler_drawer            (GtkWidget             *widget,
                                                                  cairo_t               *cairo);
static void    hyscan_gtk_waterfallgrid_info_drawer              (GtkWidget             *widget,
                                                                  cairo_t               *cairo);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfallGrid, hyscan_gtk_waterfallgrid, HYSCAN_TYPE_GTK_WATERFALL);

static void
hyscan_gtk_waterfallgrid_class_init (HyScanGtkWaterfallGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfallgrid_object_constructed;
  object_class->finalize = hyscan_gtk_waterfallgrid_object_finalize;
}

static void
hyscan_gtk_waterfallgrid_init (HyScanGtkWaterfallGrid *gtk_waterfallgrid)
{
  gtk_waterfallgrid->priv = hyscan_gtk_waterfallgrid_get_instance_private (gtk_waterfallgrid);
}

static void
hyscan_gtk_waterfallgrid_object_constructed (GObject *object)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (object);
  HyScanGtkWaterfallGridPrivate *priv = wfgrid->priv;
  GdkRGBA text_tile_color_default = {0.33, 0.85, 0.95, 1.0},
          grid_tile_color_default = {1.0, 1.0, 1.0, 0.25};

  priv->x_axis_name = g_strdup ("↔");
  priv->y_axis_name = g_strdup ("↕");
  priv->draw_x_grid = TRUE;
  priv->draw_y_grid = TRUE;
  priv->show_info = TRUE;
  priv->info_coordinates = INFOBOX_AUTO;

  G_OBJECT_CLASS (hyscan_gtk_waterfallgrid_parent_class)->constructed (object);

  g_signal_connect (wfgrid, "visible-draw", G_CALLBACK (hyscan_gtk_waterfallgrid_visible_draw), NULL);
  g_signal_connect (wfgrid, "configure-event", G_CALLBACK (hyscan_gtk_waterfallgrid_configure), NULL);
  g_signal_connect (wfgrid, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfallgrid_motion_notify), NULL);
  g_signal_connect (wfgrid, "leave-notify-event", G_CALLBACK (hyscan_gtk_waterfallgrid_leave_notify), NULL);

  priv->text_color = text_tile_color_default;
  priv->grid_color = grid_tile_color_default;
}

static void
hyscan_gtk_waterfallgrid_object_finalize (GObject *object)
{
  HyScanGtkWaterfallGrid *gtk_waterfallgrid = HYSCAN_GTK_WATERFALLGRID (object);
  HyScanGtkWaterfallGridPrivate *priv = gtk_waterfallgrid->priv;

  g_clear_pointer (&priv->font, g_object_unref);
  g_free (priv->x_axis_name);
  g_free (priv->y_axis_name);
  G_OBJECT_CLASS (hyscan_gtk_waterfallgrid_parent_class)->finalize (object);
}

GtkWidget*
hyscan_gtk_waterfallgrid_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALLGRID, NULL);
}

/* Функция обработки сигнала изменения параметров дисплея. */
static gboolean
hyscan_gtk_waterfallgrid_configure (GtkWidget            *widget,
                                    GdkEventConfigure    *event)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  HyScanGtkWaterfallGridPrivate *priv = wfgrid->priv;

  gint text_height, text_width;

  /* Текущий шрифт приложения. */
  g_clear_pointer (&priv->font, g_object_unref);
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
hyscan_gtk_waterfallgrid_motion_notify (GtkWidget             *widget,
                                        GdkEventMotion        *event)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);

  gint area_width, area_height;
  gint x = event->x;
  gint y = event->y;

  /* Проверяем, что координаты курсора находятся в рабочей области. */
  gtk_cifro_area_get_size (GTK_CIFRO_AREA (wfgrid), &area_width, &area_height);

  wfgrid->priv->mouse_x = x;
  wfgrid->priv->mouse_y = y;

  gtk_widget_queue_draw (widget);
  return FALSE;
}

/* Функция обработки сигнала выхода курсора за пределы окна. */
static gboolean
hyscan_gtk_waterfallgrid_leave_notify (GtkWidget            *widget,
                                       GdkEventCrossing     *event)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  wfgrid->priv->mouse_x = -1;
  wfgrid->priv->mouse_y = -1;

  gtk_widget_queue_draw (widget);
  return FALSE;
}

/* Обработчик сигнала area-draw. */
static void
hyscan_gtk_waterfallgrid_visible_draw (GtkWidget *widget,
                                       cairo_t   *cairo)
{
  if (HYSCAN_GTK_WATERFALLGRID(widget)->priv->draw_x_grid)
    hyscan_gtk_waterfallgrid_hruler_drawer (widget, cairo);
  if (HYSCAN_GTK_WATERFALLGRID(widget)->priv->draw_y_grid)
    hyscan_gtk_waterfallgrid_vruler_drawer (widget, cairo);
  if (HYSCAN_GTK_WATERFALLGRID(widget)->priv->show_info)
    hyscan_gtk_waterfallgrid_info_drawer (widget, cairo);
}

/* Функция рисует сетку по горизонтальной оси. */
static void
hyscan_gtk_waterfallgrid_hruler_drawer (GtkWidget *widget,
                                        cairo_t   *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  HyScanGtkWaterfallGridPrivate *priv = wfgrid->priv;
  PangoLayout *font = priv->font;

  gint area_width, area_height;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale, step;
  gdouble axis, axis_to, axis_pos, axis_from, axis_step, axis_scale;
  gint axis_range, axis_power;

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
    step = priv->x_grid_step/scale;

  axis_to = to_x;
  axis_from = from_x;
  axis_scale = scale;

  /* Шаг оцифровки. */
  gtk_cifro_area_get_axis_step (axis_scale, step, &axis_from, &axis_step, &axis_range, &axis_power);

  /* Линии. */
  {
    cairo_set_source_rgba (cairo, priv->grid_color.red, priv->grid_color.green, priv->grid_color.blue, priv->grid_color.alpha);
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
    cairo_set_source_rgba (cairo, priv->text_color.red, priv->text_color.green, priv->text_color.blue, priv->text_color.alpha);

    if (axis_power > 0)
      axis_power = 0;
    g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) fabs (axis_power));

    axis = axis_from;
    while (axis <= axis_to)
      {
        g_ascii_formatd (text_str, sizeof(text_str), text_format, axis);
        pango_layout_set_text (font, text_str, -1);
        pango_layout_get_size (font, &text_width, NULL);
        text_width /= PANGO_SCALE;

        gtk_cifro_area_value_to_point (carea, &axis_pos, NULL, axis, 0.0);
        axis += axis_step;

        if (axis_pos <= priv->virtual_border)
          continue;

        cairo_save (cairo);

        axis_pos -= text_width / 2;

        if (axis_pos <= priv->virtual_border)
          cairo_move_to (cairo, priv->virtual_border, text_height * 0.1);
        else
          cairo_move_to (cairo, axis_pos, text_height * 0.1);

        pango_cairo_show_layout (cairo, font);
        cairo_restore (cairo);
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
hyscan_gtk_waterfallgrid_vruler_drawer (GtkWidget *widget,
                                        cairo_t   *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  HyScanGtkWaterfallGridPrivate *priv = wfgrid->priv;
  PangoLayout *font = priv->font;

  gint area_width, area_height;

  gdouble from_x, to_x, from_y, to_y;
  gdouble scale, step;
  gdouble axis, axis_to, axis_pos, axis_from, axis_step, axis_scale;
  gint axis_range, axis_power;

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
    step = priv->y_grid_step/scale;

  axis_to = to_y;
  axis_from = from_y;
  axis_scale = scale;

  /* Шаг оцифровки. */
  gtk_cifro_area_get_axis_step (axis_scale, step, &axis_from, &axis_step, &axis_range, &axis_power);

  /* Рисуем линии сетки. */
  cairo_set_source_rgba (cairo, priv->grid_color.red, priv->grid_color.green, priv->grid_color.blue, priv->grid_color.alpha);
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
  cairo_set_source_rgba (cairo, priv->text_color.red, priv->text_color.green, priv->text_color.blue, priv->text_color.alpha);

  if (axis_power > 0)
    axis_power = 0;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) fabs (axis_power));

  axis = axis_from;
  while (axis <= axis_to)
    {
      g_ascii_formatd (text_str, sizeof(text_str), text_format, axis);
      pango_layout_set_text (font, text_str, -1);
      pango_layout_get_size (font, &text_width, &text_height);
      text_width /= PANGO_SCALE;
      text_height /= PANGO_SCALE;

      gtk_cifro_area_value_to_point (carea, NULL, &axis_pos, 0.0, axis);
      axis += axis_step;

      if (axis_pos <= priv->virtual_border)
        continue;

      cairo_save (cairo);

      if (axis_pos - text_height <= priv->virtual_border)
        cairo_move_to (cairo, text_height * 0.1, priv->virtual_border);
      else
        cairo_move_to (cairo, text_height * 0.1, axis_pos - text_height);

      pango_cairo_show_layout (cairo, font);
      cairo_restore (cairo);
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
hyscan_gtk_waterfallgrid_info_drawer (GtkWidget *widget,
                                      cairo_t   *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  HyScanGtkWaterfallGridPrivate *priv = wfgrid->priv;

  PangoLayout *font = priv->font;

  gint area_width, area_height;
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

  g_snprintf (text_format, sizeof(text_format), "-%%.%df", (gint) fabs (value_power));
  g_snprintf (text_str[VALUE_X], sizeof(text_str[VALUE_X]), text_format, MAX( ABS( from_x ), ABS( to_x ) ));

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
  info_height = 2 * font_height + text_spacing * 3;

  /* Прыгающее окошко. */
  /*if (priv->mouse_x > area_width - 8 * text_spacing - info_width && priv->mouse_y < 8 * text_spacing + info_height)
    x = 6 * text_spacing;
  else
    x = area_width - 6 * text_spacing - info_width;
  y = 6 * text_spacing;
  */
  /* Правый верхний угол окошка. Либо надо рассчитать, либо координаты уже есть. */
  switch (priv->info_coordinates)
    {
    case INFOBOX_ABSOLUTE:
      x = priv->info_x + info_width / 2;
      y = priv->info_y - info_height / 2;
      break;
    case INFOBOX_PERCENT:
      x = area_width * priv->info_x_perc / 100.0 + info_width / 2;
      y = area_height * priv->info_y_perc / 100.0 - info_height / 2;
      break;
    case INFOBOX_AUTO:
    default:
      x = area_width - border * 1.2;
      y = border * 1.2;
    }

  /* Проверяем размеры области отображения. */
  if (x - info_width < 0 || y + info_height > area_height)
    return;
  /* Рисуем подложку окна. */
  cairo_set_line_width (cairo, 1.0);

  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.0, 0.25);
  cairo_rectangle (cairo, x + 0.5, y + 0.5, -info_width, info_height);
  cairo_fill (cairo);

  cairo_set_source_rgba (cairo, priv->grid_color.red, priv->grid_color.green, priv->grid_color.blue, priv->grid_color.alpha);
  cairo_rectangle (cairo, x + 0.5, y + 0.5, -info_width, info_height);
  cairo_stroke (cairo);

  /* Теперь составляем строки с фактическими координатами. */
  value = value_x;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) fabs (value_power));
  g_ascii_formatd (text_str[VALUE_X], sizeof(text_str[VALUE_X]), text_format, value_x);

  value = value_y;
  gtk_cifro_area_get_axis_step (scale, 1, &value, NULL, NULL, &value_power);
  value_power = (value_power > 0) ? 0 : value_power;
  g_snprintf (text_format, sizeof(text_format), "%%.%df", (gint) fabs (value_power));
  g_ascii_formatd (text_str[VALUE_Y], sizeof(text_str[VALUE_Y]), text_format, value_y);

  /* Всё. Теперь можно рисовать само окошко. */
  cairo_set_source_rgba (cairo, priv->text_color.red, priv->text_color.green, priv->text_color.blue, priv->text_color.alpha);

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

/* Функция указывает, какие линии рисовать. */
void
hyscan_gtk_waterfallgrid_show_grid (GtkWidget              *widget,
                                    gboolean                draw_horisontal,
                                    gboolean                draw_vertical)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  wfgrid->priv->draw_x_grid = draw_horisontal;
  wfgrid->priv->draw_y_grid = draw_vertical;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция позволяет включить и выключить отображение окошка с координатой. */
void
hyscan_gtk_waterfallgrid_show_info (GtkWidget              *widget,
                                    gboolean                show_info)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  wfgrid->priv->show_info = show_info;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция задает координаты информационного окна по умолчанию. */
void
hyscan_gtk_waterfallgrid_info_position_auto (GtkWidget              *widget)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  wfgrid->priv->info_coordinates = INFOBOX_AUTO;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция задает координаты информационного окна в пикселях. */
void
hyscan_gtk_waterfallgrid_info_position_abs (GtkWidget              *widget,
                                            gint                    x_position,
                                            gint                    y_position)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  wfgrid->priv->info_coordinates = INFOBOX_ABSOLUTE;
  wfgrid->priv->info_x = x_position;
  wfgrid->priv->info_y = y_position;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция задает координаты информационного окна в процентах. */
void
hyscan_gtk_waterfallgrid_info_position_perc (GtkWidget              *widget,
                                             gint                    x_position,
                                             gint                    y_position)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  wfgrid->priv->info_coordinates = INFOBOX_PERCENT;
  wfgrid->priv->info_x_perc = x_position;
  wfgrid->priv->info_y_perc = y_position;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция устанавливает шаг координатной сетки. */
gboolean
hyscan_gtk_waterfallgrid_set_grid_step (GtkWidget              *widget,
                                        gdouble                 step_horisontal,
                                        gdouble                 step_vertical)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid), FALSE);
  g_return_val_if_fail (step_horisontal >= 0.0, FALSE);
  g_return_val_if_fail (step_vertical >= 0.0, FALSE);

  wfgrid->priv->x_grid_step = step_horisontal;
  wfgrid->priv->y_grid_step = step_vertical;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
  return TRUE;
}

/* Функция устанавливает цвет координатной сетки. */
void
hyscan_gtk_waterfallgrid_set_grid_color (GtkWidget              *widget,
                                         guint32                 color)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  GdkRGBA rgba = {0};
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  hyscan_tile_color_converter_i2d (color, &rgba.red, &rgba.green, &rgba.blue, &rgba.alpha);
  wfgrid->priv->grid_color = rgba;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}

/* Функция устанавливает цвет подписей. */
void
hyscan_gtk_waterfallgrid_set_label_color (GtkWidget              *widget,
                                          guint32                 color)
{
  HyScanGtkWaterfallGrid *wfgrid = HYSCAN_GTK_WATERFALLGRID (widget);
  GdkRGBA rgba = {0};
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALLGRID (wfgrid));

  hyscan_tile_color_converter_i2d (color, &rgba.red, &rgba.green, &rgba.blue, &rgba.alpha);
  wfgrid->priv->text_color = rgba;

  gtk_widget_queue_draw (GTK_WIDGET (wfgrid));
}
