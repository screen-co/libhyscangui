#ifndef __HYSCAN_GTK_WATERFALL_METER_H__
#define __HYSCAN_GTK_WATERFALL_METER_H__

#include <hyscan-gtk-waterfall-layer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_METER             (hyscan_gtk_waterfall_meter_get_type ())
#define HYSCAN_GTK_WATERFALL_METER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeter))
#define HYSCAN_IS_GTK_WATERFALL_METER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))
#define HYSCAN_IS_GTK_WATERFALL_METER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_METER))
#define HYSCAN_GTK_WATERFALL_METER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_METER, HyScanGtkWaterfallMeterClass))

typedef struct _HyScanGtkWaterfallMeter HyScanGtkWaterfallMeter;
typedef struct _HyScanGtkWaterfallMeterPrivate HyScanGtkWaterfallMeterPrivate;
typedef struct _HyScanGtkWaterfallMeterClass HyScanGtkWaterfallMeterClass;

struct _HyScanGtkWaterfallMeter
{
  GObject parent_instance;

  HyScanGtkWaterfallMeterPrivate *priv;
};

struct _HyScanGtkWaterfallMeterClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_gtk_waterfall_meter_get_type (void);

HYSCAN_API
HyScanGtkWaterfallMeter        *hyscan_gtk_waterfall_meter_new              (HyScanGtkWaterfallState *waterfall);

HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_meter_color  (HyScanGtkWaterfallMeter *meter,
                                                                             GdkRGBA                  color);
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_color (HyScanGtkWaterfallMeter *meter,
                                                                             GdkRGBA                  color);
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_meter_width  (HyScanGtkWaterfallMeter *meter,
                                                                             gdouble                  width);
HYSCAN_API
void                            hyscan_gtk_waterfall_meter_set_shadow_width (HyScanGtkWaterfallMeter *meter,
                                                                             gdouble                  width);


G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_METER_H__ */
