/* hyscan-nmea-file-device.h
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

#ifndef __HYSCAN_NMEA_FILE_DEVICE_H__
#define __HYSCAN_NMEA_FILE_DEVICE_H__

#include <hyscan-sensor.h>
#include <hyscan-device.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_FILE_DEVICE             (hyscan_nmea_file_device_get_type ())
#define HYSCAN_NMEA_FILE_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_FILE_DEVICE, HyScanNmeaFileDevice))
#define HYSCAN_IS_NMEA_FILE_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_FILE_DEVICE))
#define HYSCAN_NMEA_FILE_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_FILE_DEVICE, HyScanNmeaFileDeviceClass))
#define HYSCAN_IS_NMEA_FILE_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_FILE_DEVICE))
#define HYSCAN_NMEA_FILE_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_FILE_DEVICE, HyScanNmeaFileDeviceClass))

typedef struct _HyScanNmeaFileDevice HyScanNmeaFileDevice;
typedef struct _HyScanNmeaFileDevicePrivate HyScanNmeaFileDevicePrivate;
typedef struct _HyScanNmeaFileDeviceClass HyScanNmeaFileDeviceClass;

struct _HyScanNmeaFileDevice
{
  GObject parent_instance;

  HyScanNmeaFileDevicePrivate *priv;
};

struct _HyScanNmeaFileDeviceClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_nmea_file_device_get_type (void);

HYSCAN_API
HyScanNmeaFileDevice * hyscan_nmea_file_device_new (const gchar *name,
                                                    const gchar *filename);


G_END_DECLS

#endif /* __HYSCAN_NMEA_FILE_DEVICE_H__ */
