/**
 * \file hyscan-gtk-waterfall-grid.h
 *
 * \brief Координатная сетка для виджета водопад
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallGrid HyScanGtkWaterfallGrid - координатная сетка для водопада
 *
 * Виджет создается методом #hyscan_gtk_waterfall_grid_new.
 *
 * Виджет обрабатывает сигнал "visible-draw" от \link GtkCifroArea \endlink.
 * В этом обработчике он рисует координатную сетку для отображаемой области.
 *
 * Методы этого класса предоставляют богатые возможности по настройке отображения.
 *
 * - #hyscan_gtk_waterfall_grid_show_grid - включение и выключение сетки;
 * - #hyscan_gtk_waterfall_grid_show_position - включение и выключение линеек местоположения;
 * - #hyscan_gtk_waterfall_grid_show_info - включение и выключение информационного окна;
 * - #hyscan_gtk_waterfall_grid_set_info_position - задает местоположение информационного окна;
 * - #hyscan_gtk_waterfall_grid_set_grid_step - шаг сетки;
 * - #hyscan_gtk_waterfall_grid_set_grid_color - цвет сетки;
 * - #hyscan_gtk_waterfall_grid_set_main_color - цвет подписей.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_GRID_H__
#define __HYSCAN_GTK_WATERFALL_GRID_H__

#include "hyscan-gtk-layer.h"
#include "hyscan-gtk-waterfall.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_GRID             (hyscan_gtk_waterfall_grid_get_type ())
#define HYSCAN_GTK_WATERFALL_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGrid))
#define HYSCAN_IS_GTK_WATERFALL_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID))
#define HYSCAN_GTK_WATERFALL_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGridClass))
#define HYSCAN_IS_GTK_WATERFALL_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_GRID))
#define HYSCAN_GTK_WATERFALL_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGridClass))

typedef enum
{
  HYSCAN_GTK_WATERFALL_GRID_TOP_LEFT,
  HYSCAN_GTK_WATERFALL_GRID_TOP_RIGHT,
  HYSCAN_GTK_WATERFALL_GRID_BOTTOM_LEFT,
  HYSCAN_GTK_WATERFALL_GRID_BOTTOM_RIGHT,
} HyScanGtkWaterfallGridInfoPosition;

typedef struct _HyScanGtkWaterfallGrid HyScanGtkWaterfallGrid;
typedef struct _HyScanGtkWaterfallGridPrivate HyScanGtkWaterfallGridPrivate;
typedef struct _HyScanGtkWaterfallGridClass HyScanGtkWaterfallGridClass;

struct _HyScanGtkWaterfallGrid
{
  GInitiallyUnowned parent_instance;

  HyScanGtkWaterfallGridPrivate *priv;
};

struct _HyScanGtkWaterfallGridClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_grid_get_type              (void);

/**
 *
 * Функция создает новый виджет \link HyScanGtkWaterfallGrid \endlink
 *
 * \param waterfall указатель на виджет \link HyScanGtkWaterfall \endlink
 *
 * \return новый виджет \link HyScanGtkWaterfallGrid \endlink
 */
HYSCAN_API
HyScanGtkWaterfallGrid *hyscan_gtk_waterfall_grid_new                   (void);
/**
 *
 * Функция возвращает GtkAdjusment для горизонтальной оси.
 *
 * \param grid указатель на слой \link HyScanGtkWaterfallGrid \endlink
 *
 * \return GtkAdjusment для горизонтальной оси
 */
HYSCAN_API
GtkAdjustment          *hyscan_gtk_waterfall_grid_get_hadjustment       (HyScanGtkWaterfallGrid  *grid);
/**
 *
 * Функция возвращает GtkAdjusment для вертикальной оси.
 *
 * \param grid указатель на слой \link HyScanGtkWaterfallGrid \endlink
 *
 * \return GtkAdjusment для вертикальной оси
 */
HYSCAN_API
GtkAdjustment          *hyscan_gtk_waterfall_grid_get_vadjustment       (HyScanGtkWaterfallGrid  *grid);

/**
 *
 * Функция позволяет включить или отключить координатную сетку.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param draw_grid - TRUE, чтобы показать линии сетки, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_grid             (HyScanGtkWaterfallGrid *grid,
                                                                         gboolean                draw_grid);

/**
 *
 * Функция позволяет включить или отключить информационное окошко.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param show_info - TRUE, чтобы показать информацию, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_info             (HyScanGtkWaterfallGrid *grid,
                                                                         gboolean                show_info);

/**
 *
 * Функция задает местоположение информационного окошка по умолчанию.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param position - местоположение информационного окошка;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_info_position    (HyScanGtkWaterfallGrid             *grid,
                                                                        HyScanGtkWaterfallGridInfoPosition  position);

/**
 *
 * Функция позволяет задать шаг предпочтительный шаг координатной сетки в пикселях.
 * Если передать значение 0, сетка рассчитывается автоматически. При этом не гарантируется,
 * что расстояние будет строго равно запрошенному.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param step_horisontal - шаг горизонтальных координатных линий;
 * \param step_vertical - шаг вертикальных координатных линий.
 *
 * \return FALSE, если шаг отрицательный. Иначе TRUE.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_grid_set_grid_step         (HyScanGtkWaterfallGrid *grid,
                                                                         gdouble                 step_horisontal,
                                                                         gdouble                 step_vertical);

/**
 *
 * Функция позволяет задать цвет координатной сетки.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_grid_color        (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);
/**
 *
 * Функция позволяет задать цвет подписей.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_main_color       (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);
/**
 *
 * Функция позволяет задать цвет подложки под подписями.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_shadow_color      (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);

HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_condence          (HyScanGtkWaterfallGrid *grid,
                                                                         gdouble                 condence);

HYSCAN_API
GtkWidget *             hyscan_gtk_waterfall_grid_make_grid             (HyScanGtkWaterfallGrid *grid,
                                                                         GtkWidget              *child);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_GRID_H__ */
