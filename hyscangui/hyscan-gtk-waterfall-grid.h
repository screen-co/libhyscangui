/* hyscan-gtk-waterfall-grid.h
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
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
 * hyscan_gtk_waterfall_grid_new:
 * Функция создает новый виджет #HyScanGtkWaterfallGrid
 *
 * \param waterfall указатель на виджет #HyScanGtkWaterfall
 *
 * \return новый виджет #HyScanGtkWaterfallGrid
 */
HYSCAN_API
HyScanGtkWaterfallGrid *hyscan_gtk_waterfall_grid_new                   (void);
/**
 * hyscan_gtk_waterfall_grid_get_hadjustment:
 * Функция возвращает GtkAdjusment для горизонтальной оси.
 *
 * \param grid указатель на слой #HyScanGtkWaterfallGrid
 *
 * \return GtkAdjusment для горизонтальной оси
 */
HYSCAN_API
GtkAdjustment          *hyscan_gtk_waterfall_grid_get_hadjustment       (HyScanGtkWaterfallGrid  *grid);
/**
 * hyscan_gtk_waterfall_grid_get_vadjustment:
 * Функция возвращает GtkAdjusment для вертикальной оси.
 *
 * \param grid указатель на слой #HyScanGtkWaterfallGrid
 *
 * \return GtkAdjusment для вертикальной оси
 */
HYSCAN_API
GtkAdjustment          *hyscan_gtk_waterfall_grid_get_vadjustment       (HyScanGtkWaterfallGrid  *grid);

/**
 * hyscan_gtk_waterfall_grid_show_grid:
 * Функция позволяет включить или отключить координатную сетку.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param draw_grid - TRUE, чтобы показать линии сетки, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_grid             (HyScanGtkWaterfallGrid *grid,
                                                                         gboolean                draw_grid);

/**
 * hyscan_gtk_waterfall_grid_show_info:
 * Функция позволяет включить или отключить информационное окошко.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param show_info - TRUE, чтобы показать информацию, FALSE, чтобы скрыть.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_show_info             (HyScanGtkWaterfallGrid *grid,
                                                                         gboolean                show_info);

/**
 * hyscan_gtk_waterfall_grid_set_info_position:
 * Функция задает местоположение информационного окошка по умолчанию.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param position - местоположение информационного окошка;
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_info_position    (HyScanGtkWaterfallGrid             *grid,
                                                                        HyScanGtkWaterfallGridInfoPosition  position);

/**
 * hyscan_gtk_waterfall_grid_set_grid_step:
 * Функция позволяет задать шаг предпочтительный шаг координатной сетки в пикселях.
 * Если передать значение 0, сетка рассчитывается автоматически. При этом не гарантируется,
 * что расстояние будет строго равно запрошенному.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
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
 * hyscan_gtk_waterfall_grid_set_grid_color:
 * Функция позволяет задать цвет координатной сетки.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_grid_color        (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);
/**
 * hyscan_gtk_waterfall_grid_set_main_color:
 * Функция позволяет задать цвет подписей.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_main_color       (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);
/**
 * hyscan_gtk_waterfall_grid_set_shadow_color:
 * Функция позволяет задать цвет подложки под подписями.
 *
 * \param grid - указатель на объект #HyScanGtkWaterfallGrid;
 * \param color - цвет.
 *
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_shadow_color      (HyScanGtkWaterfallGrid *grid,
                                                                         guint32                 color);
/**
 *hyscan_gtk_waterfall_grid_set_condence:
 */
HYSCAN_API
void                    hyscan_gtk_waterfall_grid_set_condence          (HyScanGtkWaterfallGrid *grid,
                                                                         gdouble                 condence);
/**
 *hyscan_gtk_waterfall_grid_make_grid:
 */
HYSCAN_API
GtkWidget *             hyscan_gtk_waterfall_grid_make_grid             (HyScanGtkWaterfallGrid *grid,
                                                                         GtkWidget              *child);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_GRID_H__ */
