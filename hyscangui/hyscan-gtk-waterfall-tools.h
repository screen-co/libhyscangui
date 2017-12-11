#ifndef __HYSCAN_GTK_WATERFALL_TOOLS_H__
#define __HYSCAN_GTK_WATERFALL_TOOLS_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHADOW_DEFAULT "rgba (0, 0, 0, 0.75)"
#define FRAME_DEFAULT "rgba (255, 255, 255, 0.25)"
#define HANDLE_RADIUS 9

typedef struct
{
  gdouble x;
  gdouble y;
} HyScanCoordinates;

/**
 *
 * Функция определяет расстояние между точками.
 *
 * \param start начало;
 * \param end конец.
 *
 * \return расстояние.
 *
 */
HYSCAN_API
gdouble                 hyscan_gtk_waterfall_tools_distance            (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

/**
 *
 * Функция определяет угол наклона отрезка.
 *
 * \param start начало отрезка;
 * \param end конец отрезка.
 *
 * \return угол.
 *
 */
HYSCAN_API
gdouble                 hyscan_gtk_waterfall_tools_angle               (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

/**
 *
 * Функция возвращает середину отрезка.
 *
 * \param start начало отрезка;
 * \param end конец отрезка.
 *
 * \return середина отрезка.
 *
 */
HYSCAN_API
HyScanCoordinates       hyscan_gtk_waterfall_tools_middle              (HyScanCoordinates *start,
                                                                        HyScanCoordinates *end);

/**
 *
 * Функция проверяет, попадает ли отрезок хоть одной точкой внутрь прямоугольника.
 *
 * \param line_start начало отрезка;
 * \param line_end конец отрезка;
 * \param square_start начальная координата прямоугольника;
 * \param square_end конечная координата прямоугольника.
 *
 * \return TRUE, если попадает.
 *
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_line_in_square      (HyScanCoordinates *line_start,
                                                                        HyScanCoordinates *line_end,
                                                                        HyScanCoordinates *square_start,
                                                                        HyScanCoordinates *square_end);

/**
 *
 * Функция подсчитывает коэффициеты k и b прямой, на которой лежит отрезок.
 *
 * Прямые описываются уравнением y = k * x + b. Эта функция
 * решает это уравнение, зная начало и конец отрезка. Единственный
 * случай, когда уравнение решить невозможно, это когда прямая вертикальна.
 *
 * \param line_start начало отрезка;
 * \param line_end конец отрезка;
 * \param k коэффициент k;
 * \param b коэффициент b.
 *
 * \return TRUE, если коэффициенты удалось определить.
 *
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_line_k_b_calc       (HyScanCoordinates *line_start,
                                                                        HyScanCoordinates *line_end,
                                                                        gdouble           *k,
                                                                        gdouble           *b);

/**
 *
 * Функция определяет, лежит ли точка внутри прямоугольника.
 *
 * \param point координаты точки;
 * \param square_start начальная координата прямоугольника;
 * \param square_end конечная координата прямоугольника.
 *
 * \return TRUE, если лежит внутри прямоугольника.
 *
 */
HYSCAN_API
gboolean                hyscan_gtk_waterfall_tools_point_in_square     (HyScanCoordinates *point,
                                                                        HyScanCoordinates *square_start,
                                                                        HyScanCoordinates *square_end);

/**
 *
 * Функция создает паттерн для хэндлов.
 *
 * \param radius радиус;
 * \param inner внутренний цвет;
 * \param outer внешний цвет.
 *
 * \return паттерн.
 *
 */
HYSCAN_API
cairo_pattern_t*        hyscan_gtk_waterfall_tools_make_handle_pattern (gdouble            radius,
                                                                        GdkRGBA            inner,
                                                                        GdkRGBA            outer);

/**
 *
 * Вспомогательная функция установки цвета.
 *
 * \param cr контекст cairo;
 * \param rgba цвет, который требуется установить.
 *
 */
HYSCAN_API
void                    hyscan_cairo_set_source_gdk_rgba               (cairo_t           *cr,
                                                                        GdkRGBA           *rgba);

G_END_DECLS

#endif /* __HYSCAN_GTK_WATERFALL_TOOLS_H__ */
