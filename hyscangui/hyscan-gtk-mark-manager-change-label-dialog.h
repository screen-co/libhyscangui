/*
 * hyscan-gtk-mark-manager-change-label-dialog.h
 *
 *  Created on: 13 апр. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include <gtk/gtk.h>
#include <hyscan-object-model.h>

#ifndef __HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_H__
#define __HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_H__

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG             (hyscan_gtk_mark_manager_change_label_dialog_get_type ())
#define HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG, HyScanGtkMarkManagerChangeLabelDialog))
#define HYSCAN_IS_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG))
#define HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG, HyScanGtkMarkManagerChangeLabelDialogClass))
#define HYSCAN_IS_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG))
#define HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG, HyScanGtkMarkManagerChangeLabelDialogClass))

typedef struct _HyScanGtkMarkManagerChangeLabelDialog        HyScanGtkMarkManagerChangeLabelDialog;
typedef struct _HyScanGtkMarkManagerChangeLabelDialogPrivate HyScanGtkMarkManagerChangeLabelDialogPrivate;
typedef struct _HyScanGtkMarkManagerChangeLabelDialogClass   HyScanGtkMarkManagerChangeLabelDialogClass;

struct _HyScanGtkMarkManagerChangeLabelDialog
{
  GtkDialog parent_instance;

  HyScanGtkMarkManagerChangeLabelDialogPrivate *priv;
};

struct _HyScanGtkMarkManagerChangeLabelDialogClass
{
  GtkDialogClass parent_class;
};

GType      hyscan_gtk_mark_manager_change_label_dialog_get_type (void);

GtkWidget* hyscan_gtk_mark_manager_change_label_dialog_new      (GtkWindow         *parent,
                                                                 HyScanObjectModel *model);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_CHANGE_LABEL_DIALOG_H__ */
