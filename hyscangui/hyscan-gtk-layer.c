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
 * - hyscan_gtk_layer_grab_input() - передаёт право обработки ввода текущему слою.
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
 * hyscan_gtk_layer_load_key_file:
 * @layer: указатель на слой #HyScanGtkLayer
 * @key_file: указатель на #GKeyFile
 * @group: название группы, относящейся к данному слою
 *
 * Загружает конфигурацию слоя из группы @group в ini-файле.
 *
 * Returns: %TRUE, если слой сконфигурировался; иначе %FALSE.
 */
gboolean
hyscan_gtk_layer_load_key_file (HyScanGtkLayer          *layer,
                                GKeyFile                *key_file,
                                const gchar             *group)
{
  HyScanGtkLayerInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER (layer), TRUE);

  iface = HYSCAN_GTK_LAYER_GET_IFACE (layer);
  if (iface->load_key_file != NULL)
    return (*iface->load_key_file) (layer, key_file, group);

  return FALSE;
}

/**
 * hyscan_gtk_layer_load_key_file_color:
 * @color
 * @key_file
 * @group_name
 * @key
 * @default_spec
 *
 * Парсит цвет из конфигурационного файла.
 */
void
hyscan_gtk_layer_load_key_file_rgba (GdkRGBA     *color,
                                     GKeyFile    *key_file,
                                     const gchar *group_name,
                                     const gchar *key,
                                     const gchar *default_spec)
{
  gchar *color_spec;

  color_spec = g_key_file_get_string (key_file, group_name, key, NULL);
  if (color_spec == NULL || !gdk_rgba_parse (color, color_spec))
    gdk_rgba_parse (color, default_spec);

  g_free (color_spec);
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
 * Захватывает пользовательский ввод в контейнер. Слой, захвативший ввод имеет
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
