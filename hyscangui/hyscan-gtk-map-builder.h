#ifndef __HYSCAN_GTK_MAP_BUILDER_H__
#define __HYSCAN_GTK_MAP_BUILDER_H__

#include <hyscan-gtk-map.h>
#include <hyscan-control-model.h>
#include <hyscan-gtk-model-manager.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_BUILDER             (hyscan_gtk_map_builder_get_type ())
#define HYSCAN_GTK_MAP_BUILDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_BUILDER, HyScanGtkMapBuilder))
#define HYSCAN_IS_GTK_MAP_BUILDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_BUILDER))
#define HYSCAN_GTK_MAP_BUILDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_BUILDER, HyScanGtkMapBuilderClass))
#define HYSCAN_IS_GTK_MAP_BUILDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_BUILDER))
#define HYSCAN_GTK_MAP_BUILDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_BUILDER, HyScanGtkMapBuilderClass))

typedef struct _HyScanGtkMapBuilder HyScanGtkMapBuilder;
typedef struct _HyScanGtkMapBuilderPrivate HyScanGtkMapBuilderPrivate;
typedef struct _HyScanGtkMapBuilderClass HyScanGtkMapBuilderClass;

struct _HyScanGtkMapBuilder
{
  GObject parent_instance;

  HyScanGtkMapBuilderPrivate *priv;
};

struct _HyScanGtkMapBuilderClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_builder_get_type           (void);

HYSCAN_API
HyScanGtkMapBuilder *  hyscan_gtk_map_builder_new                (HyScanGtkModelManager     *model_manager);

HYSCAN_API
void                   hyscan_gtk_map_builder_set_offline        (HyScanGtkMapBuilder       *builder,
                                                                  gboolean                   offline);

HYSCAN_API
gboolean               hyscan_gtk_map_builder_get_offline        (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_run_planner_import (HyScanGtkMapBuilder       *builder);

HYSCAN_API
gchar *                hyscan_gtk_map_builder_get_selected_track (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_ruler          (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_pin            (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_grid            (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_marks          (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_tracks         (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_nav            (HyScanGtkMapBuilder       *builder,
                                                                  HyScanControlModel        *control_model,
                                                                  gboolean                   enable_autostart);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_planner        (HyScanGtkMapBuilder       *builder,
                                                                  gboolean                   write_records);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_planner_list   (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_layer_set_tools    (HyScanGtkMapBuilder       *builder,
                                                                  const gchar               *layer_id,
                                                                  GtkWidget                 *widget);

HYSCAN_API
void                   hyscan_gtk_map_builder_add_layer          (HyScanGtkMapBuilder       *builder,
                                                                  HyScanGtkLayer            *layer,
                                                                  gboolean                   visible,
                                                                  const gchar               *key,
                                                                  const gchar               *title);

HYSCAN_API
HyScanGtkMap *         hyscan_gtk_map_builder_get_map            (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_tools          (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_bar            (HyScanGtkMapBuilder        *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_layer_list     (HyScanGtkMapBuilder        *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_profile_switch (HyScanGtkMapBuilder      *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_mark_list      (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_planner_list   (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_track_list     (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_left_col       (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_right_col      (HyScanGtkMapBuilder       *builder);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_builder_get_central        (HyScanGtkMapBuilder       *builder);

HYSCAN_API
void                   hyscan_gtk_map_builder_file_read          (HyScanGtkMapBuilder       *builder,
                                                                  GKeyFile                  *file);

HYSCAN_API
void                   hyscan_gtk_map_builder_file_write         (HyScanGtkMapBuilder       *builder,
                                                                  GKeyFile                  *file);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_BUILDER_H__ */
