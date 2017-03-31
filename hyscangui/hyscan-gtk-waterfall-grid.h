/**
 *
 * \file hyscan-gtk-waterfall-grid.h
 *
 * \brief Виджет "водопад с координатной сеткой".
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallGrid HyScanGtkWaterfallGrid - виджет "водопад".
 *
 * Виджет создается методом #hyscan_gtk_waterfall_grid_new. Виджет не будет работать до
 * тех пор, пока не будет вызывана функция #hyscan_gtk_waterfall_open. Такая логика
 * нужна для того, чтобы не пересоздавать виджет при любых изменениях.
 *
 * Виджет обрабатывает сигнал "area-draw" от \link GtkCifroArea \endlink.
 * В этом обработчике он рисует координатную сетку для отображаемой области.
 *
 */

#ifndef __HYSCAN_GTK_WATERFALL_GRID_H__
#define __HYSCAN_GTK_WATERFALL_GRID_H__

#include <hyscan-gtk-waterfall-drawer.h>
#include <hyscan-gtk-waterfall-private.h>

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
  HyScanGtkWaterfallDrawer parent_instance;

  HyScanGtkWaterfallGridPrivate *priv;
};

struct _HyScanGtkWaterfallGridClass
{
  HyScanGtkWaterfallDrawerClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_waterfall_grid_get_type         (void);

/**
 *
 * Функция создает новый виджет #HyScanGtkWaterfallGrid
 *
 */
HYSCAN_API
GtkWidget              *hyscan_gtk_waterfall_grid_new              (void);

/**
 *
 * Функция позволяет включить или отключить координатную сетку.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param draw_horisontal - TRUE, чтобы показать горизонтальные линии сетки, FALSE, чтобы скрыть.
 * \param draw_vertical - TRUE, чтобы показать вертикальные линии сетки, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_grid        (GtkWidget              *widget,
                                                                   gboolean                draw_horisontal,
                                                                   gboolean                draw_vertical);
/**
 *
 * Функция позволяет включить или отключить информационное окошко.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param show_info - TRUE, чтобы показать информацию, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_info        (GtkWidget              *widget,
                                                                   gboolean                show_info);

/**
 *
 * Функция задает местоположение информационного окошка по умолчанию.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_auto (GtkWidget              *widget);

/**
 *
 * Функция задает местоположение информационного окошка в абсолютных координатах.
 * Задается местоположение верхней левой точки окна.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param x_position - горизонтальная координата в пикселях;
 * \param y_position - вертикальная координата в пикселях.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_abs (GtkWidget              *widget,
                                                                    gint                    x_position,
                                                                    gint                    y_position);

/**
 *
 * Функция задает местоположение информационного окошка в процентах.
 * Задается местоположение верхней левой точки окна.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param x_position - горизонтальная координата в процентах;
 * \param y_position - вертикальная координата в процентах.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_info_position_perc (GtkWidget              *widget,
                                                                     gint                     x_position,
                                                                     gint                     y_position);

/**
 *
 * Функция позволяет задать шаг координатной сетки в метрах.
 * Если передать значение 0, сетка рассчитывается автоматически.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param step_horisontal - шаг горизонтальных координатных линий в метрах.
 * \param step_vertical - шаг вертикальных координатных линий в метрах.
 *
 * \return FALSE, если шаг отрицательный. Иначе TRUE.
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_grid_set_grid_step    (GtkWidget              *widget,
                                                                   gdouble                 step_horisontal,
                                                                   gdouble                 step_vertical);

/**
 *
 * Функция позволяет задать цвет координатной сетки.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_grid_color    (GtkWidget              *widget,
                                                                    guint32                 color);
/**
 *
 * Функция позволяет задать цвет подписей.
 *
 * \param wfgrid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_label_color   (GtkWidget              *widget,
                                                                    guint32                 color);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_GRID_H__ */
