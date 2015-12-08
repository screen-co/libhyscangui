/**
 * \file hyscan-gtk-area.h
 *
 * \brief Заголовочный файл виджета рабочей области
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanGtkArea HyScanGtkArea - виджет рабочей области
 *
 * Виджет представляет собой контейнер с пятью рабочими областями: центральной, левой,
 * правой, верхней и нижней. В каждую рабочую область можно разместить один дочерний виджет.
 * Для размещения нескольких виджетов необходимо использовать Gtk контейнер, например GtkBox или GtkGrid.
 * Размещение дочерних виджетов осуществляется с помощью функций: #hyscan_gtk_area_set_central,
 * #hyscan_gtk_area_set_left, #hyscan_gtk_area_set_right, #hyscan_gtk_area_set_top и #hyscan_gtk_area_set_bottom.
 *
 * Рабочие области вокруг центральной могут быть скрыты пользователем через графический интерфейс,
 * нажатием на соответствующие элементы управления (стрелки) или программно, с помощью функций:
 * #hyscan_gtk_area_set_left_visible, #hyscan_gtk_area_set_right_visible, #hyscan_gtk_area_set_top_visible,
 * #hyscan_gtk_area_set_bottom_visible и #hyscan_gtk_area_set_all_visible.
 *
 * Определить отображаются рабочие области или нет можно с помощью функций: #hyscan_gtk_area_is_left_visible,
 * #hyscan_gtk_area_is_right_visible, #hyscan_gtk_area_is_top_visible и #hyscan_gtk_area_is_bottom_visible.
 *
 * Виджет создаётся функцией: #hyscan_gtk_area_new.
 *
*/

#ifndef __HYSCAN_GTK_AREA_H__
#define __HYSCAN_GTK_AREA_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_AREA             (hyscan_gtk_area_get_type ())
#define HYSCAN_GTK_AREA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_AREA, HyScanGtkArea))
#define HYSCAN_IS_GTK_AREA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_AREA))
#define HYSCAN_GTK_AREA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_AREA, HyScanGtkAreaClass))
#define HYSCAN_IS_GTK_AREA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_AREA))
#define HYSCAN_GTK_AREA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_AREA, HyScanGtkAreaClass))

typedef struct _HyScanGtkArea HyScanGtkArea;
typedef struct _HyScanGtkAreaClass HyScanGtkAreaClass;

struct _HyScanGtkAreaClass
{
  GtkGridClass parent_class;
};

GType hyscan_gtk_area_get_type (void);

/*!
 *
 * Функция создаёт новый виджет \link HyScanGtkArea \endlink.
 *
 * \return Указатель на новый виджет \link HyScanGtkArea \endlink.
 *
*/
GtkWidget     *hyscan_gtk_area_new                     (void);

/*!
 *
 * Функция задаёт дочерний виджет центральной рабочей области.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param child дочерний виджет.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_central             (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

/*!
 *
 * Функция задаёт дочерний виджет рабочей области слева.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param child дочерний виджет.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_left                (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

/*!
 *
 * Функция задаёт дочерний виджет рабочей области справа.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param child дочерний виджет.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_right               (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

/*!
 *
 * Функция задаёт дочерний виджет рабочей области сверху.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param child дочерний виджет.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_top                 (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

/*!
 *
 * Функция задаёт дочерний виджет рабочей области снизу.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param child дочерний виджет.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_bottom              (HyScanGtkArea         *area,
                                                        GtkWidget             *child);

/*!
 *
 * Функция устанавливает видимость рабочей области слева.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param visible показывать или нет рабочую область слева.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_left_visible        (HyScanGtkArea         *area,
                                                        gboolean               visible);

/*!
 *
 * Функция возвращает состояние видимости рабочей области слева.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink.
 *
 * \return Состояние видимости рабочей области слева.
 *
*/
gboolean       hyscan_gtk_area_is_left_visible         (HyScanGtkArea         *area);

/*!
 *
 * Функция устанавливает видимость рабочей области справа.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param visible показывать или нет рабочую область справа.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_right_visible       (HyScanGtkArea         *area,
                                                        gboolean               visible);

/*!
 *
 * Функция возвращает состояние видимости рабочей области справа.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink.
 *
 * \return Состояние видимости рабочей области справа.
 *
*/
gboolean       hyscan_gtk_area_is_right_visible        (HyScanGtkArea         *area);

/*!
 *
 * Функция устанавливает видимость рабочей области сверху.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param visible показывать или нет рабочую область сверху.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_top_visible         (HyScanGtkArea         *area,
                                                        gboolean               visible);

/*!
 *
 * Функция возвращает состояние видимости рабочей области сверху.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink.
 *
 * \return Состояние видимости рабочей области сверху.
 *
*/
gboolean       hyscan_gtk_area_is_top_visible          (HyScanGtkArea         *area);

/*!
 *
 * Функция устанавливает видимость рабочей области снизу.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param visible показывать или нет рабочую область снизу.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_bottom_visible      (HyScanGtkArea         *area,
                                                        gboolean               visible);

/*!
 *
 * Функция возвращает состояние видимости рабочей области снизу.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink.
 *
 * \return Состояние видимости рабочей области снизу.
 *
*/
gboolean       hyscan_gtk_area_is_bottom_visible       (HyScanGtkArea         *area);

/*!
 *
 * Функция устанавливает видимость всех рабочих областей, кроме центральной.
 *
 * \param area указатель на виджет \link HyScanGtkArea \endlink;
 * \param visible показывать или нет рабочие области.
 *
 * \return Нет.
 *
*/
void           hyscan_gtk_area_set_all_visible         (HyScanGtkArea         *area,
                                                        gboolean               visible);

G_END_DECLS

#endif /* __HYSCAN_GTK_AREA_H__ */
