

#ifndef __HYSCAN_GTK_WATERFALL_DRAWER_H__
#define __HYSCAN_GTK_WATERFALL_DRAWER_H__

#include <hyscan-gtk-waterfall.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_DRAWER             (hyscan_gtk_waterfall_drawer_get_type ())
#define HYSCAN_GTK_WATERFALL_DRAWER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_DRAWER, HyScanGtkWaterfallDrawer))
#define HYSCAN_IS_GTK_WATERFALL_DRAWER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_DRAWER))
#define HYSCAN_GTK_WATERFALL_DRAWER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_DRAWER, HyScanGtkWaterfallDrawerClass))
#define HYSCAN_IS_GTK_WATERFALL_DRAWER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_DRAWER))
#define HYSCAN_GTK_WATERFALL_DRAWER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_DRAWER, HyScanGtkWaterfallDrawerClass))

typedef struct _HyScanGtkWaterfallDrawer HyScanGtkWaterfallDrawer;
typedef struct _HyScanGtkWaterfallDrawerPrivate HyScanGtkWaterfallDrawerPrivate;
typedef struct _HyScanGtkWaterfallDrawerClass HyScanGtkWaterfallDrawerClass;

struct _HyScanGtkWaterfallDrawer
{
  HyScanGtkWaterfall parent_instance;

  HyScanGtkWaterfallDrawerPrivate *priv;
};

struct _HyScanGtkWaterfallDrawerClass
{
  HyScanGtkWaterfallClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_drawer_get_type         (void);

/**
 *
 * Функция создает новый виджет \link HyScanGtkWaterfallDrawer \endlink
 *
 */
GtkWidget              *hyscan_gtk_waterfall_drawer_new              (void);


HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_upsample      (HyScanGtkWaterfallDrawer *waterfall,
                                                                       gint                upsample);

/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 * Фактически, это обертка над \link hyscan_tile_color_set_colormap \endlink.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param сolormap - массив значений цветов точек;
 * \param length - размер массива;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_colormap     (HyScanGtkWaterfallDrawer *waterfall,
                                                                      guint32            *colormap,
                                                                      gint                length,
                                                                      guint32             background);

/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 * Это обертка над \link hyscan_tile_color_set_levels \endlink.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_levels       (HyScanGtkWaterfallDrawer *waterfall,
                                                                      gdouble             black,
                                                                      gdouble             gamma,
                                                                      gdouble             white);
/**
 *
 * Функция возвращает масштабы и индекс текущего масштаба.
 *
 * \param waterfall - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param zooms - указатель на массив масштабов.
 * \param num - число масштабов.
 *
 * \return номер масштаба; -1 в случае ошибки.
 */
HYSCAN_API
gint                    hyscan_gtk_waterfall_drawer_get_scale        (HyScanGtkWaterfallDrawer *waterfall,
                                                                      const gdouble     **zooms,
                                                                      gint               *num);

HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_zoom             (HyScanGtkWaterfallDrawer *drawer,
                                                                      gboolean                  zoom_in);

HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_automove         (HyScanGtkWaterfallDrawer *drawer,
                                                                      gboolean                  automove);

HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_automove_period (HyScanGtkWaterfallDrawer *drawer,
                                                                         gint64                    usecs);
HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_regeneration_period (HyScanGtkWaterfallDrawer *drawer,
                                                                         gint64                    usecs);
G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
