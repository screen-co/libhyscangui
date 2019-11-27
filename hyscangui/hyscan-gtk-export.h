#ifndef __HYSCAN_GTK_EXPORT_H__
#define __HYSCAN_GTK_EXPORT_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <hyscan-api.h>
#include <hyscan-db.h>
#include <hyscan-nmea-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_EXPORT             (hyscan_gtk_export_get_type ())
#define HYSCAN_GTK_EXPORT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_EXPORT, HyScanGtkExport))
#define HYSCAN_IS_GTK_EXPORT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_EXPORT))
#define HYSCAN_GTK_EXPORT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_EXPORT, HyScanGtkExportClass))
#define HYSCAN_IS_GTK_EXPORT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_EXPORT))
#define HYSCAN_GTK_EXPORT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_EXPORT, HyScanGtkExportClass))

typedef struct _HyScanGtkExport HyScanGtkExport;
typedef struct _HyScanGtkExportPrivate HyScanGtkExportPrivate;
typedef struct _HyScanGtkExportClass HyScanGtkExportClass;

struct _HyScanGtkExport
{
  GtkGrid parent_instance;

  HyScanGtkExportPrivate *priv;
};

struct _HyScanGtkExportClass
{
  GtkGridClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_export_get_type         (void);

HYSCAN_API
GtkWidget*             hyscan_gtk_export_new              (HyScanDB             *db,
                                                           const gchar          *project_name,
                                                           const gchar          *tracks);

HYSCAN_API
void                   hyscan_gtk_export_set_watch_period (HyScanGtkExport      *self,
                                                           guint                 period);

HYSCAN_API
void                   hyscan_gtk_export_set_max_ampl     (HyScanGtkExport      *self,
                                                           guint                 ampl_val);

HYSCAN_API
void                   hyscan_gtk_export_set_image_prm    (HyScanGtkExport      *self,
                                                           gfloat                black,
                                                           gfloat                white,
                                                           gfloat                gamma);

HYSCAN_API
void                   hyscan_gtk_export_set_velosity     (HyScanGtkExport      *self,
                                                           gfloat                velosity);

HYSCAN_API
gboolean               hyscan_gtk_export_init_crs         (HyScanGtkExport      *self,
                                                           const gchar          *src_projection_id,
                                                           const gchar          *src_datum_id);

G_END_DECLS

#endif /* __HYSCAN_GTK_EXPORT_H__ */
