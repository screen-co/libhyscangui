/* hyscan-gtk-layer.h
 *
 * Copyright 2017 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_GTK_LAYER_H__
#define __HYSCAN_GTK_LAYER_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>
#include <hyscan-gtk-layer-container.h>
#include <hyscan-gtk-layer-common.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER            (hyscan_gtk_layer_get_type ())
#define HYSCAN_GTK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayer))
#define HYSCAN_IS_GTK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER))
#define HYSCAN_GTK_LAYER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayerInterface))

typedef struct _HyScanGtkLayerInterface HyScanGtkLayerInterface;
typedef struct _HyScanGtkLayerHandle HyScanGtkLayerHandle;
typedef enum _HyScanGtkLayerSelMode HyScanGtkLayerSelMode;

/**
 * HyScanGtkLayerSelMode:
 * @HYSCAN_GTK_LAYER_SEL_MODE_NOTHING: снять выделение
 * @HYSCAN_GTK_LAYER_SEL_MODE_REPLACE: заменить предыдущее выделение
 * @HYSCAN_GTK_LAYER_SEL_MODE_ADD: добавить к предыдущему выделению
 *
 * Режим выделения элементов слоя
 */
enum _HyScanGtkLayerSelMode
{
  HYSCAN_GTK_LAYER_SEL_MODE_NOTHING,
  HYSCAN_GTK_LAYER_SEL_MODE_REPLACE,
  HYSCAN_GTK_LAYER_SEL_MODE_ADD,
};

/**
 * HyScanGtkLayerHandle:
 * @val_x: координата x хэндла в логической системе координат
 * @val_y: координата y хэндла в логической системе координат
 * @type_name: имя типа хэндла
 * @user_data: пользовательские данные
 *
 * Стуктура с информацией о хэндле слоя. Содержит в себе координаты хэндла.
 *
 * Вспомогательные поля с названием типа хэндла @type_name и пользовательскими данными
 * @user_data предзначены для внутренней обработки слоем.
 * Имя типа позволяет слою идентифицировать, как был создан полученный хэндл,
 * чтобы кооректного его обработать.
 *
 */
struct _HyScanGtkLayerHandle
{
  gdouble        val_x;
  gdouble        val_y;
  const gchar   *type_name;
  gpointer       user_data;
};

/**
 * HyScanGtkLayerInterface:
 * @g_iface: Базовый интерфейс.
 * @get_param: Получает #HyScanParam с настройками оформления слоя.
 * @added: Регистрирует слой в контейнере @container.
 * @removed: Удаляет слой из контейнера @container.
 * @grab_input: Захватывает пользовательский ввод в своём контейнере.
 * @set_visible: Устанавливает, видно ли пользователю слой.
 * @get_visible: Возвращает, видно ли пользователю слой.
 * @get_icon_name: Возвращает имя иконки для слоя (см. #GtkIconTheme для подробностей).
 * @hint_find: Определяет, есть ли на слое всплывающая подсказка для указанной точки.
 * @hint_shown: Устанавливает, была ли показана всплывающаю подсказка, запрошенная последний раз.
 * @handle_create: Создаёт хэндл в указанной точке.
 * @handle_release: Отпускает хэндл, который был ранее завхвачен слоем.
 * @handle_find: Находит, есть ли на слое хэндл в окрестностях указанной точки.
 * @handle_show: Показывает хэндл, который был ранее найден слоем.
 * @handle_grab: Захватывает хэндл, который был ранее найден слоем.
 * @select_mode: Устанавливает режим выделения.
 * @select_area: Устанавливает область выделения.
 */
struct _HyScanGtkLayerInterface
{
  GTypeInterface         g_iface;

  HyScanParam *        (*get_param)        (HyScanGtkLayer          *gtk_layer);

  void                 (*added)            (HyScanGtkLayer          *gtk_layer,
                                            HyScanGtkLayerContainer *container);

  void                 (*removed)          (HyScanGtkLayer          *gtk_layer);

  gboolean             (*grab_input)       (HyScanGtkLayer          *layer);

  void                 (*set_visible)      (HyScanGtkLayer          *layer,
                                            gboolean                 visible);

  gboolean             (*get_visible)      (HyScanGtkLayer          *layer);

  const gchar *        (*get_icon_name)    (HyScanGtkLayer          *layer);

  gchar *              (*hint_find)        (HyScanGtkLayer          *layer,
                                            gdouble                  x,
                                            gdouble                  y,
                                            gdouble                 *distance);

  void                 (*hint_shown)       (HyScanGtkLayer          *layer,
                                            gboolean                 shown);

  gboolean             (*handle_create)    (HyScanGtkLayer          *layer,
                                            GdkEventButton          *event);

  gboolean             (*handle_release)   (HyScanGtkLayer          *layer,
                                            GdkEventButton          *event,
                                            gconstpointer            howner);

  gboolean             (*handle_find)      (HyScanGtkLayer          *layer,
                                            gdouble                  x,
                                            gdouble                  y,
                                            HyScanGtkLayerHandle    *handle);

  void                 (*handle_show)      (HyScanGtkLayer          *layer,
                                            HyScanGtkLayerHandle    *handle);

  void                 (*handle_click)     (HyScanGtkLayer          *layer,
                                            GdkEventButton          *event,
                                            HyScanGtkLayerHandle    *handle);

  void                 (*handle_drag)      (HyScanGtkLayer          *layer,
                                            GdkEventButton          *event,
                                            HyScanGtkLayerHandle    *handle);

  gint                 (*select_mode)      (HyScanGtkLayer          *layer,
                                            HyScanGtkLayerSelMode    select);

  gint                 (*select_area)      (HyScanGtkLayer          *layer,
                                            gdouble                  from_x,
                                            gdouble                  to_x,
                                            gdouble                  from_y,
                                            gdouble                  to_y);
};

HYSCAN_API
GType         hyscan_gtk_layer_get_type               (void);

HYSCAN_API
void          hyscan_gtk_layer_added                  (HyScanGtkLayer          *layer,
                                                       HyScanGtkLayerContainer *container);

HYSCAN_API
void          hyscan_gtk_layer_removed                (HyScanGtkLayer          *layer);

HYSCAN_API
gboolean      hyscan_gtk_layer_grab_input             (HyScanGtkLayer          *layer);

HYSCAN_API
void          hyscan_gtk_layer_set_visible            (HyScanGtkLayer          *layer,
                                                       gboolean                 visible);

HYSCAN_API
gboolean      hyscan_gtk_layer_get_visible            (HyScanGtkLayer          *layer);

HYSCAN_API
HyScanParam * hyscan_gtk_layer_get_param              (HyScanGtkLayer          *layer);

HYSCAN_API
const gchar * hyscan_gtk_layer_get_icon_name          (HyScanGtkLayer          *layer);

HYSCAN_API
gchar *       hyscan_gtk_layer_hint_find              (HyScanGtkLayer          *layer,
                                                       gdouble                  x,
                                                       gdouble                  y,
                                                       gdouble                 *distance);

HYSCAN_API
void          hyscan_gtk_layer_hint_shown             (HyScanGtkLayer          *layer,
                                                       gboolean                 shown);

HYSCAN_API
gboolean      hyscan_gtk_layer_handle_create          (HyScanGtkLayer          *layer,
                                                       GdkEventButton          *event);
HYSCAN_API
gboolean      hyscan_gtk_layer_handle_release         (HyScanGtkLayer          *layer,
                                                       GdkEventButton          *event,
                                                       gconstpointer            howner);

HYSCAN_API
gboolean      hyscan_gtk_layer_handle_find            (HyScanGtkLayer          *layer,
                                                       gdouble                  x,
                                                       gdouble                  y,
                                                       HyScanGtkLayerHandle    *handle);

HYSCAN_API
void          hyscan_gtk_layer_handle_show            (HyScanGtkLayer          *layer,
                                                       HyScanGtkLayerHandle    *handle);

HYSCAN_API
void          hyscan_gtk_layer_handle_click           (HyScanGtkLayer          *layer,
                                                       GdkEventButton          *event,
                                                       HyScanGtkLayerHandle    *handle);

HYSCAN_API
void          hyscan_gtk_layer_handle_drag            (HyScanGtkLayer          *layer,
                                                       GdkEventButton          *event,
                                                       HyScanGtkLayerHandle    *handle);

HYSCAN_API
gint          hyscan_gtk_layer_select_mode            (HyScanGtkLayer          *layer,
                                                       HyScanGtkLayerSelMode    mode);

HYSCAN_API
gint          hyscan_gtk_layer_select_area            (HyScanGtkLayer          *layer,
                                                       gdouble                  from_x,
                                                       gdouble                  to_x,
                                                       gdouble                  from_y,
                                                       gdouble                  to_y);

G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_H__ */
