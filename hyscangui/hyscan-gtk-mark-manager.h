/*
 * hyscan-gtk-mark-manager.h
 *
 *  Created on: 14 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_GTK_MARK_MANAGER_H__
#define __HYSCAN_GTK_MARK_MANAGER_H__

#include <gtk/gtk.h>
#include "hyscan-gtk-mark-manager-view.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER             (hyscan_mark_manager_get_type ())
#define HYSCAN_MARK_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManager))
#define HYSCAN_IS_MARK_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))
#define HYSCAN_IS_MARK_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))

typedef struct _HyScanMarkManager        HyScanMarkManager;
typedef struct _HyScanMarkManagerPrivate HyScanMarkManagerPrivate;
typedef struct _HyScanMarkManagerClass   HyScanMarkManagerClass;

struct _HyScanMarkManager
{
  GtkBox parent_instance;

  HyScanMarkManagerPrivate *priv;
};

struct _HyScanMarkManagerClass
{
  GtkBoxClass parent_class;
};

GType                    hyscan_mark_manager_get_type         (void);

GtkWidget*               hyscan_mark_manager_new              (HyScanModelManager *model_manager);

G_END_DECLS

#endif /* __HYSCAN_GTK_MARK_MANAGER_H__ */
