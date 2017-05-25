/**
 * \file hyscan-gtk-waterfall-last.h
 *
 * \brief Создание виджета водопад.
 *
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanGtkWaterfallLast HyScanGtkWaterfallLast - виджет "водопад".
 *
 * Данный файл призван упростить жизнь разработчику. Он объявляет
 * макрос hyscan_gtk_waterfall_new. Таким образом, разработчик может
 * просто включить этот файл и использовать функцию вида
 * \code
 * GtkWidget *waterfall = hyscan_gtk_waterfall_new ();
 * \endcode
 *
 * Будет создана наиболее актуальная работоспособная версия водопада.
 * В данный момент это
 * \code
 *     HyScanGtkWaterfall       // Базовый класс
 *     HyScanGtkWaterfallDrawer // Отрисовка тайлов
 * --> HyScanGtkWaterfallGrid   // Координатная сетка
 *     HyScanGtkWaterfallDepth  // Информация о глубине
 *     HyScanGtkWaterfallMarks  // Метки
 * \endcode
 *
 */
#ifndef __HYSCAN_GTK_WATERFALL_LAST_H__
#define __HYSCAN_GTK_WATERFALL_LAST_H__

#include <hyscan-gtk-waterfall-depth.h>
#define hyscan_gtk_waterfall_new (hyscan_gtk_waterfall_depth_new)

#endif /* __HYSCAN_GTK_WATERFALL_LAST_H__ */
