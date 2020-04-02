/* hyscan-gtk-layer.c
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

/**
 * SECTION: hyscan-gtk-layer
 * @Short_description: Слой виджета #GtkCifroArea
 * @Title: HyScanGtkLayer
 * @See_also: #HyScanGtkLayerContainer
 *
 * Слой - это сущность, рисующая какой-то предопределенный тип объектов. Например,
 * сетка, метки, линейка.
 *
 * Интерфейс #HyScanGtkLayer определяет набор методов, которые позволяют
 * упаковывать несколько слоёв в один виджет-контейнер и разграничивать
 * пользовательский ввод между ними:
 *
 * - hyscan_gtk_layer_added() - регистрирует слой в контейнере,
 * - hyscan_gtk_layer_set_visible() - устанавливает видимость слоя,
 * - hyscan_gtk_layer_grab_input() - передаёт право обработки ввода текущему слою,
 * - hyscan_gtk_layer_hint_find() - ищет всплывающую подсказку в указанной точке,
 * - hyscan_gtk_layer_hint_shown() - устанавливает, была ли показана найденная
 *   ранее всплывающая подсказка.
 *
 * Объект слоя подобен GTK-виджетам в том смысле, что предназначен для
 * использования только внутри контейнера #HyScanGtkLayerContainer. Поэтому
 * рекомендуется:
 *
 * 1. унаследовать классы слоев от #GInitiallyUnowned — так на
 *    момент создания объект слоя не будет принадлежать ни к какой части кода,
 * 2. при создании слоя возвращать указатель на #HyScanGtkLayer, чтобы
 *    пользователю класса не надо было приводить объект к типу интерфейса.
 *
 * Следование этим простым правилам позволит использовать следующий код, без
 * необходимости хранить указатель на слой и делать g_object_unref() для
 * удаления объекта:
 *
 * |[<!-- language="C" -->
 * container = create_container ();
 * hyscan_gtk_layer_container_add (container, create_layer());
 * ]|
 *
 *
 * ## Общие замечания
 *
 * Если слой что-то отображает, то рекомендуется иметь хотя бы следующий набор
 * методов для настройки цвета:
 *
 * - new_layer_set_shadow_color - цвет подложки для рисования под элементами и текстом
 * - new_layer_set_main_color - основной цвет для элементов и текста
 * - new_layer_set_frame_color - цвет окантовки вокруг подложки под текстом
 *
 * Однако это всего лишь совет, а не требование.
 *
 * Для конфигурации внешнего вида слоя также можно использовать #HyScanParam с
 * параметрами оформления. Получить объект настроек стиля можно с помощью
 * функции hyscan_gtk_layer_get_param().
 *
 * Подробнее про добавление слоёв описано в классе #HyScanGtkLayerContainer.
 *
 */

#include "hyscan-gtk-layer.h"

G_DEFINE_INTERFACE (HyScanGtkLayer, hyscan_gtk_layer, G_TYPE_OBJECT)

static void
hyscan_gtk_layer_default_init (HyScanGtkLayerInterface *iface)
{
}

/**
 * hyscan_gtk_layer_added:
 * @layer: указатель на слой #HyScanGtkLayer
 * @container: указатель на контейнер #HyScanGtkLayerContainer
 *
 * Регистрирует слой @layer, который был добавлен в контейнер @container.
 */
void
hyscan_gtk_layer_added (HyScanGtkLayer          *layer,
                        HyScanGtkLayerContainer *container)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->added != NULL)
    (*iface->added) (layer, container);
}

/**
 * hyscan_gtk_layer_removed:
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Удаляет слой из контейнера
 */
void
hyscan_gtk_layer_removed (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->removed != NULL)
    (*iface->removed) (layer);
}

/**
 * hyscan_gtk_layer_set_visible:
 * @layer: указатель на слой #HyScanGtkLayer
 * @visible: %TRUE, если слой должен быть видимым
 *
 * Устанавливает видимость слоя.
 */
void
hyscan_gtk_layer_set_visible (HyScanGtkLayer *layer,
                              gboolean visible)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->set_visible != NULL)
    (*iface->set_visible) (layer, visible);
}

/**
 * hyscan_gtk_layer_get_visible:
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Returns: состояние видимости слоя
 */
gboolean
hyscan_gtk_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), TRUE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->get_visible != NULL)
    return (*iface->get_visible) (layer);

  return TRUE;
}

/**
 * hyscan_gtk_layer_get_param:
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Получает объект параметров оформления слоя или NULL, если слой не содержит
 * никаких параметров.
 *
 * Returns: (transfer full) (nullable): объект параметров слоя,
 *   для удаления g_object_unref()
 */
HyScanParam *
hyscan_gtk_layer_get_param (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), NULL);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->get_param != NULL)
    return (*iface->get_param) (layer);

  return NULL;
}

/**
 * hyscan_gtk_layer_get_icon_name:
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Returns: название иконки слоя из темы оформления
 */
const gchar *
hyscan_gtk_layer_get_icon_name (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), NULL);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->get_icon_name != NULL)
    return (*iface->get_icon_name) (layer);

  return "image-missing";
}

/**
 * hyscan_gtk_layer_grab_input:
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Захватывает пользовательский ввод в контейнер. Слой, захвативший ввод, имеет
 * право обрабатывать действия пользователя (за исключением работы с хэндлами).
 * Подробнее про разграничение ввода в документации класса #HyScanGtkLayerContainer.
 *
 * Returns: %TRUE, если слой действительно захватил ввод. Иначе %FALSE
 */
gboolean
hyscan_gtk_layer_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), FALSE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->grab_input != NULL)
    return (*iface->grab_input) (layer);

  return FALSE;
}

/**
 * hyscan_gtk_layer_hint_find:
 * @layer: указатель на слой #HyScanGtkLayer
 * @x: координата x курсора мыши
 * @y: координата y курсора мыши
 * @distance: (out): расстояние до точки с подсказкой
 *
 * Ищет всплывающающую подсказку в окрестности точки (@x, @y). Если подсказка
 * найдена, возвращает её текст и расстояние @distnance до неё.
 *
 * Returns: текст всплывающей подсказки или %NULL. Для удаления g_free().
 */
gchar *
hyscan_gtk_layer_hint_find (HyScanGtkLayer *layer,
                            gdouble         x,
                            gdouble         y,
                            gdouble        *distance)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), FALSE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->hint_find == NULL)
    return NULL;

  return (*iface->hint_find) (layer, x, y, distance);
}

/**
 * hyscan_gtk_layer_hint_shown:
 * @layer: указатель на слой #HyScanGtkLayer
 * @shown: признак того, что подсказка показана
 *
 * Сообщает слою, была ли показана его всплывающая подсказка, запрошенная предыдущим
 * вызовом hyscan_gtk_layer_hint_find().
 *
 * Если подсказка показана, то слою следует выделить элемент, который соответствует
 * этой подсказке.
 */
void
hyscan_gtk_layer_hint_shown (HyScanGtkLayer *layer,
                             gboolean        shown)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->hint_shown != NULL)
    iface->hint_shown (layer, shown);
}

/**
 * hyscan_gtk_layer_handle_create:
 * @layer: указатель на слой #HyScanGtkLayer
 * @event: событие #GdkEventButton, которое привело к созданию хэндла
 *
 * Создаёт хэндл на слое @layer по событию @event нажатия на кнопку мыши.
 * Слой может как захватить созданный им хэндл, так и не захватывать.
 *
 * Returns: если хэндл был создан, возвращает %TRUE; иначе %FALSE
 */
gboolean
hyscan_gtk_layer_handle_create (HyScanGtkLayer *layer,
                                GdkEventButton *event)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), FALSE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_create != NULL)
    return iface->handle_create (layer, event);

  return FALSE;
}

/**
 * hyscan_gtk_layer_handle_find:
 * @layer: указатель на слой #HyScanGtkLayer
 * @x: координата x точки в логической системе координат
 * @y: координата y точки в логической системе координат
 * @handle: (out): указатель на хэндл #HyScanGtkLayerHandle
 *
 * Находит, есть ли на слое @layer какой-либо хэндл в окрестности точки с
 * координатами (@x, @y). Если хэндл найден, то информация о нём записывается
 * в структуру @handle.
 *
 * Найденный хэндл затем может быть передан в функции
 * hyscan_gtk_layer_handle_show() и hyscan_gtk_layer_handle_click().
 *
 * Returns: %TRUE, если хэндл был найден; иначе %FALSE.
 */
gboolean
hyscan_gtk_layer_handle_find (HyScanGtkLayer       *layer,
                              gdouble               x,
                              gdouble               y,
                              HyScanGtkLayerHandle *handle)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), FALSE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_find != NULL)
    return iface->handle_find (layer, x, y, handle);

  return FALSE;
}

/**
 * hyscan_gtk_layer_handle_show:
 * @layer: указатель на слой #HyScanGtkLayer
 * @handle: (nullable): указатель на хэндл #HyScanGtkLayerHandle
 *
 * Показывает хэндл @handle, который был найден слоем при последнем вызове функции
 * hyscan_gtk_layer_find().
 * Если в качестве хэндла был передан %NULL, то слой не показывает никакой хэндл.
 */
void
hyscan_gtk_layer_handle_show (HyScanGtkLayer       *layer,
                              HyScanGtkLayerHandle *handle)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_show != NULL)
    iface->handle_show (layer, handle);
}

/**
 * hyscan_gtk_layer_handle_click:
 * @layer: указатель на слой #HyScanGtkLayer
 * @event: событие #GdkEventButton, которое привело к захвату хэндла
 * @handle: указатель на хэндл #HyScanGtkLayerHandle
 *
 * Обрабатывает клик по хэндлу @handle, который был найден слоем при последнем вызове функции
 * hyscan_gtk_layer_handle_find().
 *
 * В результате клика слой может захватить хэндл или не захватывать его.
 *
 * Для того, чтобы отпустить хэндл, необходимо вызывать функцию
 * hyscan_gtk_layer_handle_release().
 */
void
hyscan_gtk_layer_handle_click (HyScanGtkLayer       *layer,
                               GdkEventButton       *event,
                               HyScanGtkLayerHandle *handle)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_click != NULL)
    iface->handle_click (layer, event, handle);
}


/**
 * hyscan_gtk_layer_handle_release:
 * @layer: указатель на слой #HyScanGtkLayer
 * @event: событие #GdkEventButton, которое привело к отпусканию хэндла
 * @howner: признак того, что подсказка показана
 *
 * Отпускает хэндл, принадлежащий владельцу @howner.
 *
 * Returns: %TRUE, если хэндл был отпущен. Иначе %FALSE.
 */
gboolean
hyscan_gtk_layer_handle_release (HyScanGtkLayer *layer,
                                 GdkEventButton *event,
                                 gconstpointer   howner)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), FALSE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_release != NULL)
    return iface->handle_release (layer, event, howner);

  return FALSE;
}

/**
 * hyscan_gtk_layer_handle_drag:
 * @layer: указатель на слой #HyScanGtkLayer
 * @event: событие #GdkEventButton, которое привело к захвату хэндла
 * @handle: указатель на хэндл #HyScanGtkLayerHandle
 *
 * Обрабатывает захват хэндла для перемещения. В качестве параметра @handle
 * передаётся значение, полученное при последнем  вызове функции hyscan_gtk_layer_handle_find().
 *
 * В результате вызова функции слой может захватить хэндл или не захватывать его.
 *
 * Для того, чтобы отпустить хэндл, необходимо вызывать функцию
 * hyscan_gtk_layer_handle_release().
 */
void
hyscan_gtk_layer_handle_drag (HyScanGtkLayer       *layer,
                               GdkEventButton       *event,
                               HyScanGtkLayerHandle *handle)
{
  HyScanGtkLayerInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->handle_drag != NULL)
    iface->handle_drag (layer, event, handle);
}

/**
 * hyscan_gtk_layer_select_items:
 * @layer: указатель на слой #HyScanGtkLayer
 * @from_x: минимальное значение по X области выделения
 * @to_x: максимальное значение по X области выделения
 * @from_y: минимальное значение по Y области выделения
 * @to_y: максимальное значение по Y области выделения
 *
 * Изменяет область текущего выделения на слое. Если элементы слоя хотя бы частично
 * располагаются внутри области выделения, они считаются выбранными.
 *
 * Выбранные элементы определяются областью выделения и текущим режимом, установленным
 * функцией hyscan_gtk_layer_select_mode().
 *
 * Returns: общее количество выделенных элементов на слое
 */
gint
hyscan_gtk_layer_select_area (HyScanGtkLayer       *layer,
                              gdouble               from_x,
                              gdouble               to_x,
                              gdouble               from_y,
                              gdouble               to_y)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), 0);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->select_area != NULL)
    iface->select_area (layer, from_x, to_x, from_y, to_y);

  return 0;
}

/**
 * hyscan_gtk_layer_select_mode:
 * @layer: указатель на слой #HyScanGtkLayer
 * @mode: режим выделения

 * Устанавливает режим выделения элементов на слое.
 *
 * Returns: общее количество выделенных элементов на слое
 */
gint
hyscan_gtk_layer_select_mode (HyScanGtkLayer          *layer,
                              HyScanGtkLayerSelMode    mode)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), 0);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->select_mode != NULL)
    iface->select_mode (layer, mode);

  return 0;
}
