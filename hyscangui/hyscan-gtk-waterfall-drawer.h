/**
 * \file hyscan-gtk-waterfall-drawer.h
 *
 * \brief Виджет "водопад".
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallDrawer HyScanGtkWaterfallDrawer - отображение гидролокационных данных в режиме водопад.
 *
 * Данный виджет занимается всем, что связано с отрисовкой тайлов.
 * Он содержит объекты HyScanTileQueue и HyScanTileColor, которые занимаются
 * генерацией и раскрашиванием тайлов.
 *
 * Помимо этого он берет на себя ещё две задачи: организация масштабов и автосдвижка.
 * Метод #hyscan_gtk_waterfall_drawer_get_scale позволяет получить
 * список масштабов и номер текущего масштаба.
 * При смене масштаба отсылается сигнал "waterfall-zoom".
 * \code
 * void
 * waterfall_zoom_cb (HyScanGtkWaterfallDrawer *drawer,
 *                    gint                      index,      // Индекс текущего зума.
 *                    gdouble                   zoom,       // Текущий зум в виде 1:[zoom]
 *                    gpointer                  user_data);
 * \endcode
 *
 * Метод #hyscan_gtk_waterfall_drawer_set_upsample позволяет задать
 * величину передискретизации. Это единственная публичная настройка
 * генератора тайлов. Остальные параметры (тип отображения, источники)
 * берутся из родителя.
 *
 * Настройка раскрашивания тайлов выполняется следующими функциями:
 * - #hyscan_gtk_waterfall_drawer_set_colormap для установки цветовой
 *    схемы для конкретного источника данных;
 * - #hyscan_gtk_waterfall_drawer_set_colormap_for_all для установки
 *    резервной цветовой схемы;
 * - #hyscan_gtk_waterfall_drawer_set_levels для установки уровней
 *    черной и белой точки и гамма-коррекции для конкретного источника
 *    данных;
 * - #hyscan_gtk_waterfall_drawer_set_levels_for_all для установки
 *    резервных уровней.
 *
 * Автосдвижка:
 * - #hyscan_gtk_waterfall_drawer_automove включает и выключает автосдвижку;
 * - #hyscan_gtk_waterfall_drawer_set_automove_period задает период
 *    автосдвижки.
 *
 * Помимо этого автосдвижка будет отключена, если пользователь переместит
 * указатель мыши на 10% от высоты окна с зажатой левой кнопкой.
 * Учитываются только перемещения к началу галса. Всякий раз при изменении
 * состояния автосдвижки эмиттируется сигнал "automove-state"
 * \code
 * void
 * automove_state_cb (HyScanGtkWaterfallDrawer *drawer,
 *                    gboolean                  is_on,      // TRUE, если автосдвижка включена
 *                    gpointer                  user_data);
 * \endcode
 *
 * Для снижения нагрузки на систему можно использовать метод
 * #hyscan_gtk_waterfall_drawer_set_regeneration_period. Все тайлы,
 * которые не были обнаружены в кэше, будут в любом случае отправлены
 * на генерацию. Те тайлы, которые в кэше есть, но помечены как требующие
 * перегенерации, будут отправлены не раньше, чем закончится указанное
 * время. При этом дистанция на изображении будет отличаться от реальной
 * на величину, соответствующую скорости судна умноженной на период
 * перегенерации.
 *
 * #hyscan_gtk_waterfall_drawer_new создает новый виджет HyScanGtkWaterfallDrawer.
 */

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
HYSCAN_API
GtkWidget              *hyscan_gtk_waterfall_drawer_new              (void);

/**
 *
 * Функция устанавливает величину передискретизации.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param upsample - величина передискретизации.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_upsample      (HyScanGtkWaterfallDrawer *drawer,
                                                                       gint                      upsample);

/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param source - тип источника, для которого будет применена цветовая схема;
 * \param colormap - цветовая схема;
 * \param length - количество элементов в цветовой схеме;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_colormap     (HyScanGtkWaterfallDrawer *drawer,
                                                                      HyScanSourceType          source,
                                                                      guint32                  *colormap,
                                                                      guint                     length,
                                                                      guint32                   background);
/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param colormap - цветовая схема;
 * \param length - количество элементов в цветовой схеме;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_colormap_for_all (HyScanGtkWaterfallDrawer *drawer,
                                                                          guint32                  *colormap,
                                                                          guint                     length,
                                                                          guint32                   background);

/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param source - тип источника, для которого будет применена цветовая схема;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_levels       (HyScanGtkWaterfallDrawer *drawer,
                                                                      HyScanSourceType          source,
                                                                      gdouble                   black,
                                                                      gdouble                   gamma,
                                                                      gdouble                   white);
/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_drawer_set_levels_for_all  (HyScanGtkWaterfallDrawer *drawer,
                                                                         gdouble                   black,
                                                                         gdouble                   gamma,
                                                                         gdouble                   white);
/**
 *
 * Функция возвращает масштабы и индекс текущего масштаба.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param zooms - указатель на массив масштабов;
 * \param num - число масштабов.
 *
 * \return номер масштаба; -1 в случае ошибки.
 */
HYSCAN_API
gint                    hyscan_gtk_waterfall_drawer_get_scale        (HyScanGtkWaterfallDrawer *drawer,
                                                                      const gdouble           **zooms,
                                                                      gint                     *num);
/**
 *
 * Функция включает и выключает автосдвижку изображения.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param automove - TRUE для включения и FALSE для отключения автосдвижки.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_automove         (HyScanGtkWaterfallDrawer *drawer,
                                                                      gboolean                  automove);
/**
 * Функция устанавливает период автосдвижки.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param usecs - время в микросекундах между сдвижками.
 */

HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_automove_period (HyScanGtkWaterfallDrawer *drawer,
                                                                         gint64                    usecs);
/**
 * Функция устанавливает период перегенерации тайлов.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param usecs - время в микросекундах между перегенерацией.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_regeneration_period (HyScanGtkWaterfallDrawer *drawer,
                                                                             gint64                    usecs);

/**
 * Функция устанавливает цвет подложки.
 *
 * \param drawer - указатель на объект \link HyScanGtkWaterfallDrawer \endlink;
 * \param substrate - цвет подложки.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_drawer_set_substrate           (HyScanGtkWaterfallDrawer *drawer,
                                                                             guint32                   substrate);
G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
