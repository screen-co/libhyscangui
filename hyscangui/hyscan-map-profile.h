#ifndef __HYSCAN_MAP_PROFILE_H__
#define __HYSCAN_MAP_PROFILE_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_PROFILE             (hyscan_map_profile_get_type ())
#define HYSCAN_MAP_PROFILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfile))
#define HYSCAN_IS_MAP_PROFILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_PROFILE))
#define HYSCAN_MAP_PROFILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfileClass))
#define HYSCAN_IS_MAP_PROFILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_PROFILE))
#define HYSCAN_MAP_PROFILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_PROFILE, HyScanMapProfileClass))

typedef struct _HyScanMapProfile HyScanMapProfile;
typedef struct _HyScanMapProfilePrivate HyScanMapProfilePrivate;
typedef struct _HyScanMapProfileClass HyScanMapProfileClass;

struct _HyScanMapProfile
{
  GObject parent_instance;

  HyScanMapProfilePrivate *priv;
};

struct _HyScanMapProfileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_profile_get_type         (void);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new              (void);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new_default      (void);

HYSCAN_API
HyScanMapProfile *     hyscan_map_profile_new_full         (const gchar        *title,
                                                            const gchar        *url_format,
                                                            const gchar        *cache_dir,
                                                            const gchar        *projection,
                                                            guint               min_zoom,
                                                            guint               max_zoom);

HYSCAN_API
gchar *                hyscan_map_profile_get_title        (HyScanMapProfile   *profile);

HYSCAN_API
gboolean               hyscan_map_profile_read             (HyScanMapProfile   *serializable,
                                                            const gchar        *name);

HYSCAN_API
gboolean               hyscan_map_profile_apply            (HyScanMapProfile   *profile,
                                                            HyScanGtkMap       *map);

G_END_DECLS

#endif /* __HYSCAN_MAP_PROFILE_H__ */
