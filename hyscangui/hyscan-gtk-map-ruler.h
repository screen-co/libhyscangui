#ifndef __HYSCAN_GTK_MAP_RULER_H__
#define __HYSCAN_GTK_MAP_RULER_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_RULER             (hyscan_gtk_map_ruler_get_type ())
#define HYSCAN_GTK_MAP_RULER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_RULER, HyScanGtkMapRuler))
#define HYSCAN_IS_GTK_MAP_RULER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_RULER))
#define HYSCAN_GTK_MAP_RULER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_RULER, HyScanGtkMapRulerClass))
#define HYSCAN_IS_GTK_MAP_RULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_RULER))
#define HYSCAN_GTK_MAP_RULER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_RULER, HyScanGtkMapRulerClass))

typedef struct _HyScanGtkMapRuler HyScanGtkMapRuler;
typedef struct _HyScanGtkMapRulerPrivate HyScanGtkMapRulerPrivate;
typedef struct _HyScanGtkMapRulerClass HyScanGtkMapRulerClass;

struct _HyScanGtkMapRuler
{
  GObject parent_instance;

  HyScanGtkMapRulerPrivate *priv;
};

struct _HyScanGtkMapRulerClass
{
  GObjectClass parent_class;
};

GType                  hyscan_gtk_map_ruler_get_type         (void);

HYSCAN_API
void                   hyscan_gtk_map_ruler_clear            (HyScanGtkMapRuler *ruler);

HYSCAN_API
void                   hyscan_gtk_map_ruler_set_active       (HyScanGtkMapRuler *ruler,
                                                              gboolean           active);

HYSCAN_API
gboolean               hyscan_gtk_map_ruler_is_active       (HyScanGtkMapRuler *ruler);

HYSCAN_API
HyScanGtkMapRuler *    hyscan_gtk_map_ruler_new              (HyScanGtkMap *map);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_RULER_H__ */
