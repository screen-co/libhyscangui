/*
 * hyscan-gtk-mark-manager-view.h
 *
 *  Created on: 16 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_GTK_MARK_MANAGER_VIEW_H__
#define __HYSCAN_GTK_MARK_MANAGER_VIEW_H__

#include <gtk/gtk.h>

#include "../../libhyscancore/hyscancore/hyscan-object-data-label.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER_VIEW             (hyscan_mark_manager_view_get_type ())
#define HYSCAN_MARK_MANAGER_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerView))
#define HYSCAN_IS_MARK_MANAGER_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW))
#define HYSCAN_MARK_MANAGER_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerViewClass))
#define HYSCAN_IS_MARK_MANAGER_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER_VIEW))
#define HYSCAN_MARK_MANAGER_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER_VIEW, HyScanMarkManagerViewClass))

typedef struct _HyScanMarkManagerView        HyScanMarkManagerView;
typedef struct _HyScanMarkManagerViewPrivate HyScanMarkManagerViewPrivate;
typedef struct _HyScanMarkManagerViewClass   HyScanMarkManagerViewClass;

/* Тип представления. */
typedef enum
{
  UNGROUPED,       /* Табличное. */
  BY_GROUPS,       /* Древовидный с группировкой по группам. */
  BY_TYPES,        /* Древовидный с группировкой по типам. */
  N_VIEW_TYPES     /* Количество типов представления. */
}HyScanMarkManagerGrouping;

struct _HyScanMarkManagerView
{
  GtkScrolledWindow parent_instance;

  HyScanMarkManagerViewPrivate *priv;
};

struct _HyScanMarkManagerViewClass
{
  GtkScrolledWindowClass parent_class;
};

GType                     hyscan_mark_manager_view_get_type         (void);

GtkWidget*                hyscan_mark_manager_view_new              (void);

void                      hyscan_mark_manager_view_set_grouping     (HyScanMarkManagerView     *self,
                                                                     HyScanMarkManagerGrouping  grouping);

HyScanMarkManagerGrouping hyscan_mark_manager_view_get_grouping     (HyScanMarkManagerView     *self);

void                      hyscan_mark_manager_view_update_labels    (HyScanMarkManagerView     *self,
                                                                     GHashTable                *labels);

void                      hyscan_mark_manager_view_update_geo_marks (HyScanMarkManagerView     *self,
                                                                     GHashTable                *geo_marks);

void                      hyscan_mark_manager_view_update_wf_marks  (HyScanMarkManagerView     *self,
                                                                     GHashTable                *wf_marks);

void                      hyscan_mark_manager_view_update_tracks    (HyScanMarkManagerView     *self,
                                                                     GHashTable                *tracks);

GtkTreeSelection*         hyscan_mark_manager_view_get_selection    (HyScanMarkManagerView     *self);

gboolean                  hyscan_mark_manager_view_has_selected     (HyScanMarkManagerView     *self);

void                      hyscan_mark_manager_view_expand_all       (HyScanMarkManagerView     *self);

void                      hyscan_mark_manager_view_collapse_all     (HyScanMarkManagerView     *self);

void                      hyscan_mark_manager_view_select_all       (HyScanMarkManagerView      *self);

void                      hyscan_mark_manager_view_unselect_all     (HyScanMarkManagerView      *self);

void                      hyscan_mark_manager_view_expand_to_path   (HyScanMarkManagerView      *self,
                                                                     GtkTreePath                *path);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_VIEW_H__ */
