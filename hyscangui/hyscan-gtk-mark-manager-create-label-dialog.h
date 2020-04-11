/*
 * hyscan-gtk-mark-manager-create-label-dialog.h
 *
 *  Created on: 10 апр. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include <gtk/gtk.h>
#include <hyscan-object-model.h>

#ifndef __HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG_H__
#define __HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG_H__

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG             (hyscan_mark_manager_create_label_dialog_get_type ())
#define HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG, HyScanMarkManagerCreateLabelDialog))
#define HYSCAN_IS_MARK_MANAGER_CREATE_LABEL_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG))
#define HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG, HyScanMarkManagerCreateLabelDialogClass))
#define HYSCAN_IS_MARK_MANAGER_CREATE_LABEL_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG))
#define HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER_CREATE_LABEL_DIALOG, HyScanMarkManagerCreateLabelDialogClass))

typedef struct _HyScanMarkManagerCreateLabelDialog        HyScanMarkManagerCreateLabelDialog;
typedef struct _HyScanMarkManagerCreateLabelDialogPrivate HyScanMarkManagerCreateLabelDialogPrivate;
typedef struct _HyScanMarkManagerCreateLabelDialogClass   HyScanMarkManagerCreateLabelDialogClass;

struct _HyScanMarkManagerCreateLabelDialog
{
  GtkDialog parent_instance;

  HyScanMarkManagerCreateLabelDialogPrivate *priv;
};

struct _HyScanMarkManagerCreateLabelDialogClass
{
  GtkDialogClass parent_class;
};

GType      hyscan_mark_manager_create_label_dialog_get_type (void);

GtkWidget* hyscan_mark_manager_create_label_dialog_new      (GtkWindow         *parent,
                                                             HyScanObjectModel *model);

G_END_DECLS

#endif /* __HYSCAN_MARK_MANAGER_CREATE_LABEL_DIALOG_H__ */
