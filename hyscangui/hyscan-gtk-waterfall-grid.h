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
 * - #hyscan_gtk_waterfall_grid_show_info - включение и выключение информационного окна;
 * - #hyscan_gtk_waterfall_grid_info_position_auto - автоматическое позиционирование информационного окна;
 * - #hyscan_gtk_waterfall_grid_info_position_abs - абсолютное позиционирование информационного окна;
 * - #hyscan_gtk_waterfall_grid_info_position_perc - процентное позиционирование информационного окна;
 * - #hyscan_gtk_waterfall_grid_set_grid_step - шаг сетки;
 * - #hyscan_gtk_waterfall_grid_set_grid_color - цвет сетки;
 * - #hyscan_gtk_waterfall_grid_set_main_color - цвет подписей.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_GRID_H__
#define __HYSCAN_GTK_WATERFALL_GRID_H__

#include <hyscan-gtk-waterfall-layer.h>
#include <hyscan-gtk-waterfall.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_WATERFALL_GRID             (hyscan_gtk_waterfall_grid_get_type ())
#define HYSCAN_GTK_WATERFALL_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGrid))
#define HYSCAN_IS_GTK_WATERFALL_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID))
#define HYSCAN_GTK_WATERFALL_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGridClass))
#define HYSCAN_IS_GTK_WATERFALL_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_WATERFALL_GRID))
#define HYSCAN_GTK_WATERFALL_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_WATERFALL_GRID, HyScanGtkWaterfallGridClass))

typedef struct _HyScanGtkWaterfallGrid HyScanGtkWaterfallGrid;
typedef struct _HyScanGtkWaterfallGridPrivate HyScanGtkWaterfallGridPrivate;
typedef struct _HyScanGtkWaterfallGridClass HyScanGtkWaterfallGridClass;

struct _HyScanGtkWaterfallGrid
{
  GObject parent_instance;

  HyScanGtkWaterfallGridPrivate *priv;
};

struct _HyScanGtkWaterfallGridClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_grid_get_type         (void);

/**
 *
 * Функция создает новый виджет \link HyScanGtkWaterfallGrid \endlink
 *
 */
HYSCAN_API
HyScanGtkWaterfallGrid *hyscan_gtk_waterfall_grid_new                   (HyScanGtkWaterfall     *waterfall);

/**
 *
 * Функция позволяет включить или отключить координатную сетку.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param draw_horisontal - TRUE, чтобы показать горизонтальные линии сетки, FALSE, чтобы скрыть.
 * \param draw_vertical - TRUE, чтобы показать вертикальные линии сетки, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_grid             (HyScanGtkWaterfallGrid *grid,
                                                                         gboolean                draw_horisontal,
                                                                         gboolean                draw_vertical);
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
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_auto    (HyScanGtkWaterfallGrid *grid);

/**
 *
 * Функция задает местоположение информационного окошка в абсолютных координатах.
 * Задается местоположение верхней левой точки окна.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param x_position - горизонтальная координата в пикселях;
 * \param y_position - вертикальная координата в пикселях.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_abs     (HyScanGtkWaterfallGrid *grid,
                                                                         gint                    x_position,
                                                                         gint                    y_position);

/**
 *
 * Функция задает местоположение информационного окошка в процентах.
 * Задается местоположение верхней левой точки окна.
 *
 * \param grid - указатель на объект \link HyScanGtkWaterfallGrid \endlink;
 * \param x_position - горизонтальная координата в процентах;
 * \param y_position - вертикальная координата в процентах.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_perc    (HyScanGtkWaterfallGrid *grid,
                                                                         gint                     x_position,
                                                                         gint                     y_position);

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

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_GRID_H__ */
