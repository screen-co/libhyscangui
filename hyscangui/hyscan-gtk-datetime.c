/* hyscan-gtk-datetime.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui.
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
 * SECTION: hyscan-gtk-datetime
 * @Title: HyScanGtkDatetime
 * @Short_description: виджет даты и времени
 *
 * Виджет отображает заданное время и дату или что-то одно.
 * Временная зона всегда UTC.
 */
#include "hyscan-gtk-datetime.h"

#define HYSCAN_GTK_DATETIME_STYLE "hyscan-gtk-datetime"

enum
{
  PROP_0,
  PROP_MODE,
  PROP_TIME
};

struct _HyScanGtkDateTimePrivate
{
  GDateTime       *dt;         /* Дата и время. */

  HyScanGtkDateTimeMode mode;  /* Режим отображения. */

  /* Виджеты. */
  GtkWidget       *popover;    /* Всплывающее окошко. */

  GtkAdjustment   *hour;       /* Часы. */
  GtkAdjustment   *minute;     /* Минуты. */
  GtkAdjustment   *second;     /* Секунды. */
  GtkCalendar     *calendar;   /* Дата. */
};

static void        hyscan_gtk_datetime_object_set_property  (GObject           *object,
                                                             guint              prop_id,
                                                             const GValue      *value,
                                                             GParamSpec        *pspec);
static void        hyscan_gtk_datetime_object_get_property  (GObject           *object,
                                                             guint              prop_id,
                                                             GValue            *value,
                                                             GParamSpec        *pspec);
static void        hyscan_gtk_datetime_object_constructed   (GObject           *object);
static void        hyscan_gtk_datetime_object_finalize      (GObject           *object);

static void        hyscan_gtk_datetime_start_edit           (HyScanGtkDateTime *self);
static void        hyscan_gtk_datetime_end_edit             (HyScanGtkDateTime *self,
                                                             GtkWidget         *popover);

static GtkWidget * hyscan_gtk_datetime_make_popover         (HyScanGtkDateTime *self);
static GtkWidget * hyscan_gtk_datetime_make_time_spin       (HyScanGtkDateTime *self,
                                                             GtkAdjustment     *adjustment);
static void        hyscan_gtk_datetime_make_clock           (HyScanGtkDateTime *self,
                                                             GtkGrid           *grid);
static void        hyscan_gtk_datetime_make_calendar        (HyScanGtkDateTime *self,
                                                             GtkGrid           *grid);

static void        hyscan_gtk_datetime_changed              (HyScanGtkDateTime *self,
                                                             GDateTime         *dt);
static gboolean    hyscan_gtk_datetime_output               (GtkSpinButton     *button);

static GParamSpec * hyscan_gtk_datetime_prop_time;

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkDateTime, hyscan_gtk_datetime, GTK_TYPE_TOGGLE_BUTTON);

static void
hyscan_gtk_datetime_class_init (HyScanGtkDateTimeClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_gtk_datetime_object_set_property;
  oclass->get_property = hyscan_gtk_datetime_object_get_property;
  oclass->constructed = hyscan_gtk_datetime_object_constructed;
  oclass->finalize = hyscan_gtk_datetime_object_finalize;

  g_object_class_install_property (oclass, PROP_MODE,
    g_param_spec_int ("mode", "Mode", "Time, date or both",
                      HYSCAN_GTK_DATETIME_TIME,
                      HYSCAN_GTK_DATETIME_BOTH,
                      HYSCAN_GTK_DATETIME_BOTH,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /* Это специально вынесено в отдельную переменную,
   * чтобы рассылать уведомления об изменении. */
  hyscan_gtk_datetime_prop_time =
    g_param_spec_int64 ("time", "Time", "Current time",
                        G_MININT64, G_MAXINT64, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (oclass, PROP_TIME,
                                   hyscan_gtk_datetime_prop_time);

  // hyscan_gtk_datetime_prop_time = g_object_class_find_property (oclass, "time");
}

static void
hyscan_gtk_datetime_init (HyScanGtkDateTime *self)
{
  self->priv = hyscan_gtk_datetime_get_instance_private (self);
}

static void
hyscan_gtk_datetime_object_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkDateTime *self = HYSCAN_GTK_DATETIME (object);

  switch (prop_id)
    {
    case PROP_MODE:
      self->priv->mode = g_value_get_int (value);
      break;

    case PROP_TIME:
      hyscan_gtk_datetime_set_time (self, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_datetime_object_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  HyScanGtkDateTime *self = HYSCAN_GTK_DATETIME (object);

  switch (prop_id)
    {
    case PROP_TIME:
      g_value_set_int64 (value, hyscan_gtk_datetime_get_time (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_datetime_object_constructed (GObject *object)
{
  HyScanGtkDateTime *self = HYSCAN_GTK_DATETIME (object);
  HyScanGtkDateTimePrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_datetime_parent_class)->constructed (object);

  if (priv->dt == NULL)
    hyscan_gtk_datetime_set_time (self, 0);

  priv->popover = hyscan_gtk_datetime_make_popover (self);

  g_signal_connect (self, "toggled", G_CALLBACK (hyscan_gtk_datetime_start_edit), NULL);
  g_signal_connect_swapped (priv->popover, "closed",
                            G_CALLBACK (hyscan_gtk_datetime_end_edit), self);
}

static void
hyscan_gtk_datetime_object_finalize (GObject *object)
{
  HyScanGtkDateTime *self = HYSCAN_GTK_DATETIME (object);
  HyScanGtkDateTimePrivate *priv = self->priv;

  g_clear_pointer (&priv->dt, g_date_time_unref);
  gtk_widget_destroy (priv->popover);

  G_OBJECT_CLASS (hyscan_gtk_datetime_parent_class)->finalize (object);
}

/* Функция преднастраивает виджеты popover'а. */
static void
hyscan_gtk_datetime_start_edit (HyScanGtkDateTime *self)
{
  HyScanGtkDateTimePrivate *priv = self->priv;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self)))
      return;

  if (priv->mode & HYSCAN_GTK_DATETIME_DATE)
    {
      gint y, m, d;
      g_date_time_get_ymd (priv->dt, &y, &m, &d);
      gtk_calendar_select_month (priv->calendar, m - 1, y);
      gtk_calendar_select_day (priv->calendar, d);
    }

  if (priv->mode & HYSCAN_GTK_DATETIME_TIME)
    {
      gint h, m, s;
      h = g_date_time_get_hour (priv->dt);
      m = g_date_time_get_minute (priv->dt);
      s = g_date_time_get_second (priv->dt);
      gtk_adjustment_set_value (priv->hour, h);
      gtk_adjustment_set_value (priv->minute, m);
      gtk_adjustment_set_value (priv->second, s);
    }

  gtk_widget_show_all (priv->popover);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self), TRUE);
}

/* Функция вызывается, когда popover прячется. */
static void
hyscan_gtk_datetime_end_edit (HyScanGtkDateTime *self,
                              GtkWidget         *popover)
{
  HyScanGtkDateTimePrivate *priv = self->priv;
  GDateTime *dt;
  guint hour, min, sec;
  guint day, month, year;

  hour = min = sec = 0;
  day = 1;
  month = 1;
  year = 1970;

  if (priv->mode & HYSCAN_GTK_DATETIME_TIME)
    {
      hour = gtk_adjustment_get_value (priv->hour);
      min = gtk_adjustment_get_value (priv->minute);
      sec = gtk_adjustment_get_value (priv->second);
    }
  if (priv->mode & HYSCAN_GTK_DATETIME_DATE)
    {
      gtk_calendar_get_date (priv->calendar, &year, &month, &day);
      month += 1;
    }

  dt = g_date_time_new_utc (year, month, day, hour, min, sec);
  hyscan_gtk_datetime_changed (self, dt);
  g_date_time_unref (dt);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self), FALSE);
}

/* Создаем всплывающий виджет с календарем и часами. */
static GtkWidget *
hyscan_gtk_datetime_make_popover (HyScanGtkDateTime *self)
{
  HyScanGtkDateTimePrivate *priv = self->priv;
  GtkWidget *popover, *grid;

  grid = gtk_grid_new ();

  popover = gtk_popover_new (GTK_WIDGET (self));
  gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);
  gtk_container_add (GTK_CONTAINER (popover), grid);

  if (priv->mode & HYSCAN_GTK_DATETIME_DATE)
    {
      hyscan_gtk_datetime_make_calendar (self, GTK_GRID (grid));

      /* Двойное нажатие на дату приводит к закрытию всплывающего окошка. */
      g_signal_connect_swapped (priv->calendar, "day-selected-double-click",
                                G_CALLBACK (gtk_widget_hide), popover);
    }
  if (priv->mode & HYSCAN_GTK_DATETIME_TIME)
    hyscan_gtk_datetime_make_clock (self, GTK_GRID (grid));

  return popover;
}

/* Функция создает SpinButton для отображения часов, минут или секунд. */
static GtkWidget *
hyscan_gtk_datetime_make_time_spin (HyScanGtkDateTime *self,
                                    GtkAdjustment     *adjustment)
{
  GtkWidget *spin = g_object_new (GTK_TYPE_SPIN_BUTTON,
                                  "adjustment", adjustment,
                                  "valign", GTK_ALIGN_CENTER, "xalign", 0.5,
                                  "max-width-chars", 2, "wrap", TRUE,
                                  NULL);

  g_signal_connect (spin, "output",
                    G_CALLBACK (hyscan_gtk_datetime_output), self);

  return spin;
}

/* Функция создает составной виджет часов (часы, минуты и секунды). */
static void
hyscan_gtk_datetime_make_clock (HyScanGtkDateTime *self,
                                GtkGrid           *grid)
{
  HyScanGtkDateTimePrivate *priv = self->priv;
  GtkWidget *hour;
  GtkWidget *min;
  GtkWidget *sec;

  priv->hour = gtk_adjustment_new (0, 0, 23, 1.0, 5.0, 0.0);
  priv->minute = gtk_adjustment_new (0, 0, 59, 1.0, 5.0, 0.0);
  priv->second = gtk_adjustment_new (0, 0, 59, 1.0, 5.0, 0.0);

  hour = hyscan_gtk_datetime_make_time_spin (self, priv->hour);
  min = hyscan_gtk_datetime_make_time_spin (self, priv->minute);
  sec = hyscan_gtk_datetime_make_time_spin (self, priv->second);

  gtk_grid_attach (grid, hour, 0, 1, 1, 1);
  gtk_grid_attach (grid, min,  1, 1, 1, 1);
  gtk_grid_attach (grid, sec,  2, 1, 1, 1);
}

/* Функция создает виджет календаря. */
static void
hyscan_gtk_datetime_make_calendar (HyScanGtkDateTime *self,
                                   GtkGrid           *grid)
{
  HyScanGtkDateTimePrivate *priv = self->priv;
  priv->calendar = GTK_CALENDAR (gtk_calendar_new ());

  g_object_set (priv->calendar,
                "hexpand", TRUE, "halign", GTK_ALIGN_FILL,
                "vexpand", FALSE, "valign", GTK_ALIGN_CENTER,
                NULL);

  gtk_grid_attach (grid, GTK_WIDGET (priv->calendar), 0, 0, 3, 1);
}

/* Функция устанавливает новый GDateTime. */
static void
hyscan_gtk_datetime_changed (HyScanGtkDateTime *self,
                             GDateTime         *dt)
{
  HyScanGtkDateTimePrivate *priv = self->priv;
  gchar *label = NULL;
  gchar *date = NULL;
  gchar *time = NULL;

  /* Ничего не делаю, если время не изменилось. */
  if (priv->dt != NULL && g_date_time_equal (dt, priv->dt))
    return;

  /* Иначе обновляю datetime... */
  g_clear_pointer (&priv->dt, g_date_time_unref);
  priv->dt = g_date_time_ref (dt);

  /* Обновляю текст... */
  if (priv->mode & HYSCAN_GTK_DATETIME_TIME)
    time = g_date_time_format (dt, "%T");
  if (priv->mode & HYSCAN_GTK_DATETIME_DATE)
    date = g_date_time_format (dt, "%d.%m.%Y");

  label = g_strdup_printf ("%s%s%s",
                           date != NULL ? date : "",
                           date != NULL ? " " : "",
                           time != NULL ? time : "");
  gtk_button_set_label (GTK_BUTTON (self), label);
  g_free (label);
  g_free (date);
  g_free (time);

  /* И рассылаю уведомление. */
  g_object_notify_by_pspec (G_OBJECT (self), hyscan_gtk_datetime_prop_time);
}

/* Функция форматирует вывод в SpinButtonы. */
static gboolean
hyscan_gtk_datetime_output (GtkSpinButton *button)
{
  GtkAdjustment *adjustment;
  gchar *text;
  gint value;

  adjustment = gtk_spin_button_get_adjustment (button);
  value = gtk_adjustment_get_value (adjustment);

  text = g_strdup_printf ("%02d", value);
  gtk_entry_set_text (GTK_ENTRY (button), text);
  g_free (text);

  return TRUE;
}

/**
 * hyscan_gtk_datetime_new:
 * @mode: режим отображения
 *
 * Функция создаёт новый виджет #HyScanGtkDateTime, отображающий данные в
 * соответствии с заданным режимом
 *
 * Returns: #HyScanGtkDateTime.
 */
GtkWidget *
hyscan_gtk_datetime_new (HyScanGtkDateTimeMode mode)
{
  return g_object_new (HYSCAN_TYPE_GTK_DATETIME, "mode", mode, NULL);
}

/**
 * hyscan_gtk_datetime_set_time:
 * @self: Виджет #HyScanGtkDateTime
 * @unixtime: временная метка UNIX-time
 *
 * Функция задает текущую отображаемую метку времени.
 */
void
hyscan_gtk_datetime_set_time (HyScanGtkDateTime *self,
                              gint64             unixtime)
{
  GDateTime *dt;

  g_return_if_fail (HYSCAN_IS_GTK_DATETIME (self));

  dt = g_date_time_new_from_unix_utc (unixtime);
  hyscan_gtk_datetime_changed (self, dt);
  g_date_time_unref (dt);
}

/**
 * hyscan_gtk_datetime_get_time:
 * @self: Виджет #HyScanGtkDateTime
 * @unixtime:
 *
 * Функция возвращает метку времени.
 * @Returns: временная метка UNIX-time
 */
gint64
hyscan_gtk_datetime_get_time (HyScanGtkDateTime *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_DATETIME (self), -1);

  return g_date_time_to_unix (self->priv->dt);
}
