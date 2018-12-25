/* hyscan-gtk-datetime.h
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

#ifndef __HYSCAN_GTK_DATETIME_H__
#define __HYSCAN_GTK_DATETIME_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_DATETIME             (hyscan_gtk_datetime_get_type ())
#define HYSCAN_GTK_DATETIME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_DATETIME, HyScanGtkDateTime))
#define HYSCAN_IS_GTK_DATETIME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_DATETIME))
#define HYSCAN_GTK_DATETIME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_DATETIME, HyScanGtkDateTimeClass))
#define HYSCAN_IS_GTK_DATETIME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_DATETIME))
#define HYSCAN_GTK_DATETIME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_DATETIME, HyScanGtkDateTimeClass))

typedef struct _HyScanGtkDateTime HyScanGtkDateTime;
typedef struct _HyScanGtkDateTimePrivate HyScanGtkDateTimePrivate;
typedef struct _HyScanGtkDateTimeClass HyScanGtkDateTimeClass;

/**
 * HyScanGtkDateTimeMode:
 * @HYSCAN_GTK_DATETIME_TIME: время (часы, минуты, секунды)
 * @HYSCAN_GTK_DATETIME_DATE: дата (календарь)
 * @HYSCAN_GTK_DATETIME_BOTH: время и дата
 *
 * Определяет, как отображать временную метку.
 */
typedef enum
{
  HYSCAN_GTK_DATETIME_TIME = 1 << 0,
  HYSCAN_GTK_DATETIME_DATE = 1 << 1,
  HYSCAN_GTK_DATETIME_BOTH = HYSCAN_GTK_DATETIME_TIME | HYSCAN_GTK_DATETIME_DATE
} HyScanGtkDateTimeMode;

struct _HyScanGtkDateTime
{
  GtkToggleButton parent_instance;

  HyScanGtkDateTimePrivate *priv;
};

struct _HyScanGtkDateTimeClass
{
  GtkToggleButtonClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_datetime_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_datetime_new              (HyScanGtkDateTimeMode mode);

HYSCAN_API
void                   hyscan_gtk_datetime_set_time         (HyScanGtkDateTime    *datetime,
                                                             gint64                unixtime);

HYSCAN_API
gint64                 hyscan_gtk_datetime_get_time         (HyScanGtkDateTime    *datetime);

G_END_DECLS

#endif /* __HYSCAN_GTK_DATETIME_H__ */
