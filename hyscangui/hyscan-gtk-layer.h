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

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_LAYER            (hyscan_gtk_layer_get_type ())
#define HYSCAN_GTK_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayer))
#define HYSCAN_IS_GTK_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_LAYER))
#define HYSCAN_GTK_LAYER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_LAYER, HyScanGtkLayerInterface))

typedef struct _HyScanGtkLayer HyScanGtkLayer;
typedef struct _HyScanGtkLayerInterface HyScanGtkLayerInterface;
typedef struct _HyScanGtkLayerContainer HyScanGtkLayerContainer;

/**
 * HyScanGtkLayerInterface:
 * @g_iface: Базовый интерфейс.
 * @load_key_file: Загружает конфигурацию слоя из указанной группы в #GKeyFile.
 * @added: Регистрирует слой в контейнере @container.
 * @removed: Удаляет слой из контейнера @container.
 * @grab_input: Захватывает пользовательский ввод в своём контейнере.
 * @set_visible: Устанавливает, видно ли пользователю слой.
 * @get_visible: Возвращает, видно ли пользователю слой.
 * @get_icon_name: Возвращает имя иконки для слоя (см. #GtkIconTheme для подробностей).
 * @hint_find: Определяет, есть ли на слое всплывающая подсказка для указанной точки.
 * @hint_shown: Устанавливает, была ли показана всплывающаю подсказка, запрошенная последний раз.
 */
struct _HyScanGtkLayerInterface
{
  GTypeInterface       g_iface;

  gboolean           (*load_key_file)    (HyScanGtkLayer          *gtk_layer,
                                          GKeyFile                *key_file,
                                          const gchar             *group);
  void               (*added)            (HyScanGtkLayer          *gtk_layer,
                                          HyScanGtkLayerContainer *container);
  void               (*removed)          (HyScanGtkLayer          *gtk_layer);
  gboolean           (*grab_input)       (HyScanGtkLayer          *layer);
  void               (*set_visible)      (HyScanGtkLayer          *layer,
                                          gboolean                 visible);
  gboolean           (*get_visible)      (HyScanGtkLayer          *layer);
  const gchar *      (*get_icon_name)    (HyScanGtkLayer          *layer);
  gchar *            (*hint_find)        (HyScanGtkLayer          *layer,
                                          gdouble                  x,
                                          gdouble                  y,
                                          gdouble                 *distance);
  void               (*hint_shown)       (HyScanGtkLayer          *layer,
                                          gboolean                 shown);
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
gboolean      hyscan_gtk_layer_load_key_file          (HyScanGtkLayer          *layer,
                                                       GKeyFile                *key_file,
                                                       const gchar             *group);

HYSCAN_API
void          hyscan_gtk_layer_load_key_file_rgba     (GdkRGBA                 *color,
                                                       GKeyFile                *key_file,
                                                       const gchar             *group_name,
                                                       const gchar             *key,
                                                       const gchar             *default_spec);

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

G_END_DECLS

#endif /* __HYSCAN_GTK_LAYER_H__ */
