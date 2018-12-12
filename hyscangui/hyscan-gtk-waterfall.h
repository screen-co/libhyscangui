/**
 * \file hyscan-gtk-waterfall.h
 *
 * \brief Виджет "водопад"
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfall HyScanGtkWaterfall - отображение гидролокационных данных в режиме водопад
 *
 * Данный виджет занимается всем, что связано с отрисовкой тайлов.
 * Он содержит объекты HyScanTileQueue и HyScanTileColor, которые занимаются
 * генерацией и раскрашиванием тайлов.
 *
 * Помимо этого он берет на себя ещё две задачи: организация масштабов и автосдвижка.
 * Метод #hyscan_gtk_waterfall_get_scale позволяет получить
 * список масштабов и номер текущего масштаба.
 * При смене масштаба отсылается сигнал "waterfall-zoom".
 * \code
 * void
 * waterfall_zoom_cb (HyScanGtkWaterfall *wfall,
 *                    gint                index,      // Индекс текущего зума.
 *                    gdouble             zoom,       // Текущий зум в виде 1:[zoom]
 *                    gpointer            user_data);
 * \endcode
 *
 * Метод #hyscan_gtk_waterfall_set_upsample позволяет задать
 * величину передискретизации. Это единственная публичная настройка
 * генератора тайлов. Остальные параметры (тип отображения, источники)
 * берутся из родителя.
 *
 * Настройка раскрашивания тайлов выполняется следующими функциями:
 * - #hyscan_gtk_waterfall_set_colormap для установки цветовой
 *    схемы для конкретного источника данных;
 * - #hyscan_gtk_waterfall_set_colormap_for_all для установки
 *    цветовой схемы для всех источников сразу;
 * - #hyscan_gtk_waterfall_set_levels для установки уровней
 *    черной и белой точки и гамма-коррекции для конкретного источника
 *    данных;
 * - #hyscan_gtk_waterfall_set_levels_for_all для установки
 *    уровней для всех источников сразу.
 *
 * Автосдвижка:
 * - #hyscan_gtk_waterfall_automove включает и выключает автосдвижку;
 * - #hyscan_gtk_waterfall_set_automove_period задает период
 *    автосдвижки.
 *
 * Помимо этого автосдвижка будет отключена, если пользователь переместит
 * указатель мыши на 10% от высоты окна с зажатой левой кнопкой.
 * Учитываются только перемещения к началу галса. Всякий раз при изменении
 * состояния автосдвижки эмиттируется сигнал "automove-state"
 * \code
 * void
 * automove_state_cb (HyScanGtkWaterfall *wfall,
 *                    gboolean            is_on,      // TRUE, если автосдвижка включена
 *                    gpointer            user_data);
 * \endcode
 *
 * Для снижения нагрузки на систему можно использовать метод
 * #hyscan_gtk_waterfall_set_regeneration_period. Все тайлы,
 * которые не были обнаружены в кэше, будут в любом случае отправлены
 * на генерацию. Те тайлы, которые в кэше есть, но помечены как требующие
 * перегенерации, будут отправлены не раньше, чем закончится указанное
 * время. При этом дистанция на изображении будет отличаться от реальной
 * на величину, соответствующую скорости судна умноженной на период
 * перегенерации.
 *
 * #hyscan_gtk_waterfall_new создает новый виджет HyScanGtkWaterfall.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_H__
#define __HYSCAN_GTK_WATERFALL_H__


#include <hyscan-gtk-waterfall-state.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL             (hyscan_gtk_waterfall_get_type ())
#define HYSCAN_GTK_WATERFALL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfall))
#define HYSCAN_IS_GTK_WATERFALL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))
#define HYSCAN_IS_GTK_WATERFALL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL))
#define HYSCAN_GTK_WATERFALL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL, HyScanGtkWaterfallClass))

typedef struct _HyScanGtkWaterfall HyScanGtkWaterfall;
typedef struct _HyScanGtkWaterfallPrivate HyScanGtkWaterfallPrivate;
typedef struct _HyScanGtkWaterfallClass HyScanGtkWaterfallClass;

struct _HyScanGtkWaterfall
{
  HyScanGtkWaterfallState parent_instance;

  HyScanGtkWaterfallPrivate *priv;
};

struct _HyScanGtkWaterfallClass
{
  HyScanGtkWaterfallStateClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_get_type                   (void);

/**
 *
 * Функция создает новый виджет \link HyScanGtkWaterfall \endlink
 *
 */
HYSCAN_API
GtkWidget              *hyscan_gtk_waterfall_new                        (HyScanCache        *cache);

/**
 *
 * Функция потокобезопасно инициирует перерисовку виджета.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_queue_draw                 (HyScanGtkWaterfall *wfall);


/**
 *
 * Функция устанавливает величину передискретизации.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param upsample - величина передискретизации.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_upsample               (HyScanGtkWaterfall *wfall,
                                                                         gint                upsample);
/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param source - тип источника, для которого будет применена цветовая схема;
 * \param colormap - цветовая схема;
 * \param length - количество элементов в цветовой схеме;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_colormap               (HyScanGtkWaterfall *wfall,
                                                                         HyScanSourceType    source,
                                                                         guint32            *colormap,
                                                                         guint               length,
                                                                         guint32             background);
/**
 *
 * Функция обновляет цветовую схему объекта \link HyScanTileColor \endlink.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param colormap - цветовая схема;
 * \param length - количество элементов в цветовой схеме;
 * \param background - цвет фона.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_colormap_for_all       (HyScanGtkWaterfall *wfall,
                                                                         guint32            *colormap,
                                                                         guint               length,
                                                                         guint32             background);

/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param source - тип источника, для которого будет применена цветовая схема;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_levels                 (HyScanGtkWaterfall *wfall,
                                                                         HyScanSourceType    source,
                                                                         gdouble             black,
                                                                         gdouble             gamma,
                                                                         gdouble             white);
/**
 *
 * Функция обновляет параметры объекта \link HyScanTileColor \endlink.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param black - уровень черной точки;
 * \param gamma - гамма;
 * \param white - уровень белой точки.
 *
 * \return TRUE, если параметры успешно скопированы.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_set_levels_for_all         (HyScanGtkWaterfall *wfall,
                                                                         gdouble             black,
                                                                         gdouble             gamma,
                                                                         gdouble             white);
/**
 *
 * Функция возвращает масштабы и индекс текущего масштаба.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param zooms - указатель на массив масштабов;
 * \param num - число масштабов.
 *
 * \return номер масштаба; -1 в случае ошибки.
 */
HYSCAN_API
gint                    hyscan_gtk_waterfall_get_scale                  (HyScanGtkWaterfall *wfall,
                                                                         const gdouble     **zooms,
                                                                         gint               *num);
/**
 *
 * Функция включает и выключает автосдвижку изображения.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param automove - TRUE для включения и FALSE для отключения автосдвижки.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_automove                   (HyScanGtkWaterfall *wfall,
                                                                         gboolean            automove);
/**
 * Функция устанавливает период автосдвижки.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param usecs - время в микросекундах между сдвижками.
 */

HYSCAN_API
void                    hyscan_gtk_waterfall_set_automove_period        (HyScanGtkWaterfall *wfall,
                                                                         gint64              usecs);
/**
 * Функция устанавливает период перегенерации тайлов.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param usecs - время в микросекундах между перегенерацией.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_regeneration_period    (HyScanGtkWaterfall *wfall,
                                                                         gint64              usecs);

/**
 * Функция устанавливает цвет подложки.
 *
 * \param wfall - указатель на объект \link HyScanGtkWaterfall \endlink;
 * \param substrate - цвет подложки.
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_set_substrate              (HyScanGtkWaterfall *wfall,
                                                                         guint32             substrate);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_H__ */
