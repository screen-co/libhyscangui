#ifndef __HYSCAN_GLIKO_H__
#define __HYSCAN_GLIKO_H__

#include <gtk/gtk.h>
#include <hyscan-api.h>
#include <hyscan-data-player.h>

G_BEGIN_DECLS


#define HYSCAN_TYPE_GTK_GLIKO             (hyscan_gtk_gliko_get_type ())
#define HYSCAN_GTK_GLIKO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGliko))
#define HYSCAN_IS_GTK_GLIKO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_GLIKO))
#define HYSCAN_GTK_GLIKO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoClass))
#define HYSCAN_IS_GTK_GLIKO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_GLIKO))
#define HYSCAN_GTK_GLIKO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_GLIKO, HyScanGtkGlikoClass))

typedef struct _HyScanGtkGliko HyScanGtkGliko;
typedef struct _HyScanGtkGlikoPrivate HyScanGtkGlikoPrivate;
typedef struct _HyScanGtkGlikoClass HyScanGtkGlikoClass;

GType hyscan_gtk_gliko_get_type(void);
GtkWidget *hyscan_gtk_gliko_new(void);

/**
 * hyscan_gtk_gliko_set_num_azimuthes:
 * @gliko: указатель на объект индикатора кругового обзора
 * @num_azimuthes: количество угловых дискрет
 *
 * Задает разрешающую способность по углу для индикатора кругового обзора,
 * определяется количеством угловых дискрет на полный оборот 360 градусов.
 * Количество угловых дискрет должно являться степенью 2.
 * По умолчанию принимается количество угловых дискрет, равное 1024.
 * Разрешающая способность по углу должна быть задана только один раз,
 * перед подключением к проигрывателю
 * гидроакустических данных.
 */
HYSCAN_API
void            hyscan_gtk_gliko_set_num_azimuthes (HyScanGtkGliko *gliko,
                                                const guint num_azimuthes);

/**
 * hyscan_gtk_gliko_get_num_azimuthes:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Returns: заданное количество угловых дискрет индикатора кругового обзора.
 */
HYSCAN_API
guint           hyscan_gtk_gliko_get_num_azimuthes (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_player:
 * @gliko: указатель на объект индикатора кругового обзора
 * @player: указатель на объект проигрывателя гидроакустических данных
 *
 * Подключает индикатор кругового обзора к проигрывателю
 * гидроакустических данных.
 */
HYSCAN_API
void            hyscan_gtk_gliko_set_player (HyScanGtkGliko *gliko,
                                         HyScanDataPlayer *player);

/**
 * hyscan_gtk_gliko_set_player:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Возвращает указатель на объект проигрывателя гидроакустических данных.
 * Returns: указатель на объект проигрывателя гидроакустических данных
 */
HYSCAN_API
HyScanDataPlayer *hyscan_gtk_gliko_get_player (HyScanGtkGliko *gliko);


HYSCAN_API
void           hyscan_gtk_gliko_set_fade_period (HyScanGtkGliko *gliko,
                                             const gdouble seconds );

HYSCAN_API
gdouble        hyscan_gtk_gliko_get_fade_period (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_fade_koeff:
 * @gliko: указатель на объект индикатора кругового обзора
 * @koef: коэффициент гашения
 *
 * Задает коэффициент гашения kF для обеспечения послесвечениия
 * гидроакустической информации.
 * Яркость отметок гидроакустической информации изменяется
 * при каждой итерации гашения по формуле
 * A[i] = A[i-1] * kF
 * Период итераций гашения задается вызовом hyscan_gtk_gliko_set_fade_period.
 * По умолчанию используется значение kF равное 0,98.
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_fade_koeff (HyScanGtkGliko *gliko,
                                            const gdouble koef );

/**
 * hyscan_gtk_gliko_get_fade_koeff:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Возвращает значение коэффициента гашения.
 * Returns: коэффициент гашения.
 */
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_fade_koeff (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_scale:
 * @gliko: указатель на объект индикатора кругового обзора
 * @scale: коэффициент масштаба изображения
 *
 * Задает масштаб изображения на индикаторе кругового обзора.
 * Коэффициент масштаба равен отношению диаметра круга к
 * высоте области элемента диалогового интерфейса.
 * Величина 1,0 (значение по умолчанию)
 * соответствует случаю, когда диаметр круга,
 * вписан в высоту области элемента диалогового интерфейса.
 * Величина больше 1 соответствует уменьшению изображения кругового обзора.
 * Величина меньше 1 соответствует увеличение изображения кругового обзора
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_scale (HyScanGtkGliko *gliko,
                                       const gdouble scale);

/**
 * hyscan_gtk_gliko_get_scale:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Получение текущего коэффициента масштаба изображения.
 * Returns: коэффициент масштаба изображения
 */
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_scale (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_center:
 * @gliko: указатель на объект индикатора кругового обзора
 * @cx: смещение по оси X (направление оси слева направо)
 * @cy: смещение по оси Y (направление оси снизу вверх)
 *
 * Задает смещение центра круговой развертки относительно
 * центра области элемента диалогового интерфейса.
 * Смещение задается в единицах относительно диаметра круга,
 * в которых размер диаметра равен 1.
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_center (HyScanGtkGliko *gliko,
                                        const gdouble cx,
                                        const gdouble cy);

/**
 * hyscan_gtk_gliko_get_center:
 * @gliko: указатель на объект индикатора кругового обзора
 * @cx: указатель переменную, в которой будет
 * сохранено смещение по оси X
 * @cy: указатель на переменную, в которой будет
 * сохранено смещение по оси Y
 *
 * Получение смещения центра круговой развертки относительно
 * центра области элемента диалогового интерфейса.
 */
HYSCAN_API
void           hyscan_gtk_gliko_get_center (HyScanGtkGliko *gliko,
                                        gdouble *cx,
                                        gdouble *cy);

/**
 * hyscan_gtk_gliko_set_rotation:
 * @gliko: указатель на объект индикатора кругового обзора
 * @alpha: угол поворота, градусы
 *
 * Задает поворот гидроакустического изображения
 * относительно центра круга.
 * Угол поворота задается в градусах,
 * увеличение угла соответствует направлению поворота
 * по часовой стрелке.
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_rotation (HyScanGtkGliko *gliko,
                                          const gdouble alpha);

/**
 * hyscan_gtk_gliko_get_rotation:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Получение текущего угла поворота гидроакустического изображения.
 * Returns: угол поворота, градусы.
 */
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_rotation (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_contrast:
 * @gliko: указатель на объект индикатора кругового обзора
 * @contrast: значение контрастности изображения
 *
 * Регулировка контрастности изображения.
 * Значение контрастности изображения задается в диапазоне
 * [-1;+1].
 * Яркость отметки гидроакустического изображения
 * вычисляется по следущей формуле:
 * I = B + C * A,
 * где A - нормированная амплитуда сигнала (0..255),
 *     С - коэффициент контрастности,
 *     B - постоянная составляющая яркости;
 *     I - результирующая яркость отметки
 * Коэффициент контрастности рассчитывается по формуле
 * C = 1 / (1 - @contrast), при @contrast > 0
 * C = 1 + @contrast, при @contrast <= 0
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_contrast (HyScanGtkGliko *gliko,
                                          const gdouble contrast);

/**
 * hyscan_gtk_gliko_get_contrast:
 * @gliko: указатель на объект индикатора кругового обзора
 *
 * Получение текущей контрастности изображения.
 * Returns: значение контрастности изображения
 */
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_contrast (HyScanGtkGliko *gliko);

/**
 * hyscan_gtk_gliko_set_brightness:
 * @contrast: значение яркости изображения
 *
 * Регулировка яркости изображения.
 * Задает значение
 */
HYSCAN_API
void           hyscan_gtk_gliko_set_brightness (HyScanGtkGliko *gliko,
                                            const gdouble brightness);
HYSCAN_API
gdouble        hyscan_gtk_gliko_get_brightness (HyScanGtkGliko *gliko);



/* прочие функции без описания, пока просто перечень

hyscan_gtk_gliko_set_color_rgb
hyscan_gtk_gliko_get_color_rgb

hyscan_gtk_gliko_set_color_alpha
hyscan_gtk_gliko_get_color_alpha

hyscan_gtk_gliko_set_black_point
hyscan_gtk_gliko_get_black_point

hyscan_gtk_gliko_set_white_point
hyscan_gtk_gliko_get_white_point

hyscan_gtk_gliko_set_gamma_value
hyscan_gtk_gliko_get_gamma_value

*/

HYSCAN_API
HyScanSourceType hyscan_gtk_gliko_get_source (HyScanGtkGliko *gliko,
                                          const gint channel);

G_END_DECLS

#endif
