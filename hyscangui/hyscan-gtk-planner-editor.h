#ifndef __HYSCAN_GTK_PLANNER_EDITOR_H__
#define __HYSCAN_GTK_PLANNER_EDITOR_H__

#include <gtk/gtk.h>
#include <hyscan-planner-model.h>
#include <hyscan-planner-selection.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PLANNER_EDITOR             (hyscan_gtk_planner_editor_get_type ())
#define HYSCAN_GTK_PLANNER_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PLANNER_EDITOR, HyScanGtkPlannerEditor))
#define HYSCAN_IS_GTK_PLANNER_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PLANNER_EDITOR))
#define HYSCAN_GTK_PLANNER_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PLANNER_EDITOR, HyScanGtkPlannerEditorClass))
#define HYSCAN_IS_GTK_PLANNER_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PLANNER_EDITOR))
#define HYSCAN_GTK_PLANNER_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PLANNER_EDITOR, HyScanGtkPlannerEditorClass))

typedef struct _HyScanGtkPlannerEditor HyScanGtkPlannerEditor;
typedef struct _HyScanGtkPlannerEditorPrivate HyScanGtkPlannerEditorPrivate;
typedef struct _HyScanGtkPlannerEditorClass HyScanGtkPlannerEditorClass;

struct _HyScanGtkPlannerEditor
{
  GtkGrid parent_instance;

  HyScanGtkPlannerEditorPrivate *priv;
};

struct _HyScanGtkPlannerEditorClass
{
  GtkGridClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_planner_editor_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_planner_editor_new              (HyScanPlannerModel      *model,
                                                                   HyScanPlannerSelection  *selection);

G_END_DECLS

#endif /* __HYSCAN_GTK_PLANNER_EDITOR_H__ */
