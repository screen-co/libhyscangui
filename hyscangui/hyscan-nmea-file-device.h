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
