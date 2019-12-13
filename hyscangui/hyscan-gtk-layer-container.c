/* hyscan-gtk-layer-container.c
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
 * SECTION: hyscan-gtk-layer-container
 * @Short_description: Контейнер слоёв виджета #GtkCifroArea
 * @Title: HyScanGtkLayerContainer
 * @See_also: #GtkCifroArea, #HyScanGtkLayer
 *
 * Класс представляет из себя контейнер слоёв #HyScanGtkLayer и служит для
 * управление пользовательским вводом в слои внутри контейнера.
 *
 * В основном слои интерактивны, то есть умеют обрабатывать пользовательский ввод.
 * Поскольку пользовательский опыт унифицирован, одному пользовательскому воздействию
 * может быть присвоена реакция в нескольких слоях.
 *
 * ## Хэндлы
 *
 * Хэндл - это некий элемент интерфейса, за который пользователь может "схватиться"
 * и что-то поменять. Например, взять метку и перенести ее на новое место.
 *
 * Хэндлы позволяют сделать взаимодействие пользователя с различными слоями
 * единообразным и дают контейнеру возможность самостоятельно определять, какой
 * слой должен обрабатывать очередное нажатие кнопки мыши.
 *
 * Чтобы слой мог оперировать хэндлами, он должен реализовать несколько функций
 * из интерфейса HyScanGtkLayerInterface:
 * - HyScanGtkLayerInterface.handle_create()
 * - HyScanGtkLayerInterface.handle_release()
 * - HyScanGtkLayerInterface.handle_find()
 * - HyScanGtkLayerInterface.handle_show()
 * - HyScanGtkLayerInterface.handle_click()
 *
 * Функция HyScanGtkLayerInterface.handle_find() жизненно необходима всем желающим
 * реагировать на действия пользователя. Контейнер вызывает эту функцию, когда
 * у него возникает потребность определить, есть ли в указанной точке хэндл, за
 * который может "схватиться" пользователь. Это может быть в следующих случаях.
 *
 * 1. Пользователь переместил курсор мыши. В этом случае контейнер
 *   1.1 опеределяет, в каком из слоёв хэндл лежит ближе всего к курсору,
 *   1.2 просит выбранный слой отобразить хэндл, вызывая handle_show().
 *
 * 2. Пользователь кликнул мышью. В этом случай контейнер
 *   2.1 опеределяет, в каком из слоёв хэндл лежит ближе всего к курсору,
 *   2.2 предлагает выбранному слою схватиться за хэндл, вызывая handle_grab().
 *
 * Таким образом пользователь заранее видит, за какой именно хэндл он ухватится,
 * если сейчас отпустит кнопку мыши.
 *
 * Хэндл отпускается в момент следующего клика мыши вызовом функции  handle_release().
 *
 * Если никто не схватил хэндл, то контейнер вызывает handle_create(). Слой
 * должен проверить, есть ли у него право обработки ввода
 * (#hyscan_gtk_layer_container_get_input_owner) и если есть -- создать хэндл.
 *
 * Чтобы перехватить "button-release-event" до обработки хэндлов необходимо
 * подключиться к нему с помощью функции #g_signal_connect. Чтобы поймать сигнал
 * при отсутствии хэндла -- #g_signal_connect_after (см. схему ниже).
 *
 * ## Блок-схема эмиссии сигнала "button-release-event"
 *
 * Какие функции работы с хэндлами будут вызваны, зависит от того, есть ли
 * в каком-либо слое активный хэндл handle или нет.
 *
 *                                [ HANDLER_RUN_FIRST ]
 *                                          v
 *                                  (handle == NULL)
 *                                         + +
 *                                     yes | | no
 *                     v-------------------+ +----------------------v
 *                handle_find()                              handle_release()
 *                    + +                                          + +
 *              found | |  not found                      released | | not released
 *              +-----+ +----------+                    v----------+ +----------v
 *              |                  |                 handle = NULL;       STOP EMISSION
 *              v                  |                 STOP EMISSION
 *         handle_click()          |
 *             + +                 |
 *     clicked | |                 |
 *        v----+ +                 |
 *  handle = h_found;              |
 *  STOP EMISSION                  v
 *                          handle_create()
 *                                + +
 *                        created | | not created
 *                        +-------+ +---------+
 *                        v                   V
 *                  handle = NULL;    [ HANDLER_RUN_LAST ]
 *                  STOP EMISSION
 *
 *  - подключение к моменту HANDLER_RUN_FIRST через g_signal_connect(),
 *  - подключение к моменту HANDLER_RUN_LAST через g_signal_connect_after().
 *
 * ## Всплывающие подсказки
 *
 * Каждый слой может изображать несколько элементов на поверхности виджета, по которым
 * предлагаются всплывающие подсказки с дополнительной текстовой информацией. Например,
 * можно навести курсор мыши на метку и увидеть название этой метки.
 *
 * Виджет #HyScanGtkLayerContainer координирует показ таких подсказок и
 * гарантирует показ только одной всплывающей подсказки в каждый момент времени.
 * Для этого слои с подсказками должны имплементировать #HyScanGtkLayerInterface.hint_find.
 *
 * ## Регистрация слоёв
 *
 * Регистрация слоёв происходит с помощью функции #hyscan_gtk_layer_container_add.
 * Контейнер сообщает слою о регистрации вызовом функции #hyscan_gtk_layer_added.
 * В реализации #HyScanGtkLayerInterface.#added слой может подключиться к нужным
 * сигналам контейнера.
 *
 * ## Разрешение пользовательского ввода
 *
 * Пользовательский ввод можно разделить на две части: создание новых хэндлов и
 * изменение уже существующих. Чтобы слои могли понимать, кто имеет право обработать ввод,
 * введены следующие ограничения.
 *
 * - changes_allowed - режим просмотра или редактирования
 *
 *   Это первое разграничение, самое глобальное. Оно означает режим просмотра или
 *   режим редактирования. В режиме просмотра слои не имеют права обрабатывать
 *   никакой пользовательский ввод.
 *   - #hyscan_gtk_layer_container_set_changes_allowed - разрешает или запрещает изменения
 *   - #hyscan_gtk_layer_container_get_changes_allowed - позволяет определнить, разрешены ли изменения
 *
 * - handle_grabbed - хэндл уже захвачен
 *
 *   Это второе разграничение. Если хэндл отличен от %NULL, значит, кто-то
 *   уже обрабатывает пользовательский ввод: создает метку, передвигает линейку и т.д.
 *   Это запрещает обработку ввода всем слоям кроме собственно владельца хэндла.
 *   - #hyscan_gtk_layer_container_set_handle_grabbed - захват хэндла
 *   - #hyscan_gtk_layer_container_get_handle_grabbed - возвращает владельца хэндла
 *
 * - input_owner - владелец ввода
 *
 *   Последнее разграничение. Если хэндл никем не захвачен, то владелец ввода может
 *   начать создание нового объекта.
 *   - #hyscan_gtk_layer_container_set_input_owner - захват ввода
 *   - #hyscan_gtk_layer_container_get_input_owner - возвращает владельца ввода
 *
 */

#include "hyscan-gtk-layer-container.h"
#include "hyscan-gui-marshallers.h"
#include "hyscan-gtk-layer-param.h"
#include <hyscan-param-controller.h>
#include <hyscan-param-proxy.h>
#include <math.h>

typedef struct
{
  HyScanGtkLayer *layer;
  gchar          *key;
} HyScanGtkLayerContainerInfo;

struct _HyScanGtkLayerContainerPrivate
{
  GList                 *layers;           /* Упорядоченный список слоёв, HyScanGtkLayerContainerInfo. */
  HyScanDataSchema      *schema;           /* Схема параметров контейнера. */
  GdkRGBA                background;       /* Цвет заливки. */
  HyScanParamProxy      *param_proxy;      /* Параметры контейнера и всех его слоев. */
  HyScanGtkLayerParam   *params;           /* Контроллер параметров, которому делигируем HyScanParam. */

  /* Хэндл — это интерактивный элемент слоя, который можно "взять" кликом мыши. */
  gconstpointer          howner;           /* Текущий активный хэндл, с которым взаимодействует пользователь. */
  HyScanGtkLayer        *hshow_layer;      /* Слой, чей хэндл отображается в данный момент. */

  gconstpointer          iowner;           /* Слой, которому разрешён ввод. */
  gboolean               changes_allowed;  /* Признак, что ввод в принципе разрешён пользователем. */

  gchar                 *hint;             /* Текст всплывающей подсказки. */

  struct
  {
    GtkWidget           *window;           /* Окно всплывающей подсказки. */
    GtkWidget           *label;            /* Текст всплывающей подсказки. */
  } tooltip;                               /* Всплывающая подсказка. */
};

static void         hyscan_gtk_layer_container_object_constructed      (GObject                 *object);
static void         hyscan_gtk_layer_container_object_finalize         (GObject                 *object);
static gboolean     hyscan_gtk_layer_container_handle_create           (HyScanGtkLayerContainer *container,
                                                                        GdkEventButton          *event);
static gboolean     hyscan_gtk_layer_container_handle_click            (HyScanGtkLayerContainer *container,
                                                                        GdkEventButton          *event);
static gboolean     hyscan_gtk_layer_container_handle_release          (HyScanGtkLayerContainer *container,
                                                                        gconstpointer            handle_owner,
                                                                        GdkEventButton          *event);
static void         hyscan_gtk_layer_container_unrealize               (GtkWidget               *widget);
static gboolean     hyscan_gtk_layer_container_motion_notify           (GtkWidget               *widget,
                                                                        GdkEventMotion          *event);
static gboolean     hyscan_gtk_layer_container_button_release          (GtkWidget               *widget,
                                                                        GdkEventButton          *event);
static void         hyscan_gtk_layer_container_draw                    (HyScanGtkLayerContainer *container,
                                                                        cairo_t                 *cairo);
static void         hyscan_gtk_layer_container_set_hint                (HyScanGtkLayerContainer *container,
                                                                        const gchar             *hint,
                                                                        gdouble                  x,
                                                                        gdouble                  y,
                                                                        HyScanGtkLayer          *hint_layer);
static HyScanGtkLayerContainerInfo *
                    hyscan_gtk_layer_container_info_new                (HyScanGtkLayer          *layer,
                                                                        const gchar             *key);
static void         hyscan_gtk_layer_container_info_free               (gpointer                 data);
static gint         hyscan_gtk_layer_container_info_find               (gconstpointer            _a,
                                                                        gconstpointer            _b);
static gboolean     hyscan_gtk_layer_container_leave_notify_event      (GtkWidget               *widget,
                                                                        GdkEventCrossing        *event);
static void         hyscan_gtk_layer_container_tooltip_move            (HyScanGtkLayerContainer *container,
                                                                        gdouble                  x,
                                                                        gdouble                  y);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkLayerContainer, hyscan_gtk_layer_container, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_layer_container_class_init (HyScanGtkLayerContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = hyscan_gtk_layer_container_object_constructed;
  object_class->finalize = hyscan_gtk_layer_container_object_finalize;

  widget_class->motion_notify_event = hyscan_gtk_layer_container_motion_notify;
  widget_class->button_release_event = hyscan_gtk_layer_container_button_release;
  widget_class->unrealize = hyscan_gtk_layer_container_unrealize;
  widget_class->leave_notify_event = hyscan_gtk_layer_container_leave_notify_event;
  widget_class->enter_notify_event = hyscan_gtk_layer_container_leave_notify_event;
}

static void
hyscan_gtk_layer_container_init (HyScanGtkLayerContainer *gtk_layer_container)
{
  gtk_layer_container->priv = hyscan_gtk_layer_container_get_instance_private (gtk_layer_container);
}

static void
hyscan_gtk_layer_container_object_constructed (GObject *object)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (object);
  HyScanGtkLayerContainerPrivate *priv = container->priv;

  G_OBJECT_CLASS (hyscan_gtk_layer_container_parent_class)->constructed (object);

  /* Всплывающая подсказка. */
  priv->tooltip.window = gtk_window_new (GTK_WINDOW_POPUP);
  priv->tooltip.label = gtk_label_new (NULL);
  g_object_set (G_OBJECT (priv->tooltip.label), "margin", 6, NULL);
  gtk_container_add (GTK_CONTAINER (priv->tooltip.window), priv->tooltip.label);

  hyscan_gtk_layer_container_set_changes_allowed (container, TRUE);

  /* Параметры оформления. */
  priv->params = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (priv->params, "container");
  hyscan_gtk_layer_param_add_rgba (priv->params, "/background", &priv->background);
  hyscan_gtk_layer_param_set_default (priv->params);
  g_signal_connect_swapped (priv->params, "set", G_CALLBACK (gtk_widget_queue_draw), container);

  g_signal_connect_swapped (container, "visible-draw", G_CALLBACK (hyscan_gtk_layer_container_draw), container);
}

static void
hyscan_gtk_layer_container_object_finalize (GObject *object)
{
  HyScanGtkLayerContainer *gtk_layer_container = HYSCAN_GTK_LAYER_CONTAINER (object);
  HyScanGtkLayerContainerPrivate *priv = gtk_layer_container->priv;

  g_free (priv->hint);
  g_list_free_full (priv->layers, g_object_unref);
  g_object_unref (priv->params);
  g_signal_handlers_disconnect_by_data (object, object);

  G_OBJECT_CLASS (hyscan_gtk_layer_container_parent_class)->finalize (object);
}

/* Пытается создать хэндл, опрашивая все видимые слои. */
static gboolean
hyscan_gtk_layer_container_handle_create (HyScanGtkLayerContainer *container,
                                          GdkEventButton          *event)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;

  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;

      if (!hyscan_gtk_layer_get_visible (info->layer))
        continue;

      if (hyscan_gtk_layer_handle_create (info->layer, event))
        return TRUE;
    }

  return FALSE;
}

/* Пытается захватить хэндл, опрашивая все видимые слои. */
static HyScanGtkLayer *
hyscan_gtk_layer_container_handle_closest (HyScanGtkLayerContainer *container,
                                           gdouble                  point_x,
                                           gdouble                  point_y,
                                           HyScanGtkLayerHandle    *closest_handle)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;
  gdouble closest_distance;
  HyScanGtkLayer *closest_layer = NULL;
  gdouble val_x, val_y;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (container), point_x, point_y, &val_x, &val_y);

  /* Опрашиваем каждый видимый слой - есть ли в нём хэндл. */
  closest_distance = G_MAXDOUBLE;
  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      HyScanGtkLayerHandle handle;
      gdouble distance;

      if (!hyscan_gtk_layer_get_visible (info->layer))
        continue;

      if (!hyscan_gtk_layer_handle_find (info->layer, val_x, val_y, &handle))
        continue;

      distance = hypot (val_x - handle.val_x, val_y - handle.val_y);
      if (distance > closest_distance)
        continue;

      *closest_handle = handle;
      closest_distance = distance;
      closest_layer = info->layer;
    }

  return closest_layer;
}

/* Обрабатывает клик по хэндлу под указателем мыши. */
static gboolean
hyscan_gtk_layer_container_handle_click (HyScanGtkLayerContainer *container,
                                         GdkEventButton          *event)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  gconstpointer handle_owner = NULL;
  HyScanGtkLayerHandle closest_handle;
  HyScanGtkLayer *layer = NULL;

  layer = hyscan_gtk_layer_container_handle_closest (container, event->x, event->y, &closest_handle);
  if (layer == NULL)
    return FALSE;

  hyscan_gtk_layer_handle_click (layer, event, &closest_handle);
  handle_owner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  /* Прячем хэндл, если такой есть. */
  if (handle_owner != NULL && priv->hshow_layer != NULL)
    hyscan_gtk_layer_handle_show (priv->hshow_layer, NULL);

  return TRUE;
}

/* Пытается отпустить хэндл, опрашивая все слои. */
static gboolean
hyscan_gtk_layer_container_handle_release (HyScanGtkLayerContainer *container,
                                           gconstpointer            handle_owner,
                                           GdkEventButton          *event)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;

  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;

      if (hyscan_gtk_layer_handle_release (info->layer, event, handle_owner))
        {
          hyscan_gtk_layer_container_set_handle_grabbed (container, NULL);
          break;
        }
    }

  return TRUE;
}

/* Обработчик "unrealize-event".
 * Удаляет все слои. */
static void
hyscan_gtk_layer_container_unrealize (GtkWidget *widget)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);

  hyscan_gtk_layer_container_remove_all (container);
}

/* Обработчик "leave-notify-event" и "enter-notify-event". */
static gboolean
hyscan_gtk_layer_container_leave_notify_event (GtkWidget        *widget,
                                               GdkEventCrossing *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  HyScanGtkLayerContainerPrivate *priv = container->priv;

  if (event->type == GDK_LEAVE_NOTIFY)
    gtk_widget_hide (priv->tooltip.window);
  else if (priv->hint != NULL)
    gtk_widget_show_all (priv->tooltip.window);

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_layer_container_hint_show (HyScanGtkLayerContainer *container,
                                      gdouble                  x,
                                      gdouble                  y)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;

  HyScanGtkLayer *hint_layer = NULL;
  gchar *hint = NULL;
  gdouble distance = G_MAXDOUBLE;

  /* Запрашивает у каждого видимого слоя всплывающую подсказку. */
  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      gchar *layer_hint = NULL;
      gdouble layer_distance;

      if (!hyscan_gtk_layer_get_visible (info->layer))
        continue;

      layer_hint = hyscan_gtk_layer_hint_find (info->layer, x, y, &layer_distance);
      if (layer_hint == NULL || layer_distance > distance)
        {
          g_free (layer_hint);
          continue;
        }

      /* Запоминаем текущую подсказку. */
      distance = layer_distance;
      g_free (hint);
      hint = layer_hint;
      hint_layer = info->layer;
    }

  /* Сообщаем слоям, была ли выбрана их подсказка для показа. */
  hyscan_gtk_layer_container_set_hint (container, hint, x, y, hint_layer);

  g_free (hint);
}

static void
hyscan_gtk_layer_container_handle_show (HyScanGtkLayerContainer *container,
                                        gdouble                  point_x,
                                        gdouble                  point_y)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  HyScanGtkLayerHandle closest_handle;
  HyScanGtkLayer *layer;

  layer = hyscan_gtk_layer_container_handle_closest (container, point_x, point_y, &closest_handle);

  /* Если слой, на котором размещен видимый хэндл, изменился, то сообщим ему об этом. */
  if (layer != priv->hshow_layer && priv->hshow_layer != NULL)
    hyscan_gtk_layer_handle_show (priv->hshow_layer, NULL);

  /* Если какой-то слой выбран для отображения хэндла, то сообщаем ему. */
  if (layer != NULL)
    hyscan_gtk_layer_handle_show (layer, &closest_handle);

  priv->hshow_layer = layer;
}

/* Перемещает всплывающую подсказку в нужную точку на виджете. */
static void
hyscan_gtk_layer_container_tooltip_move (HyScanGtkLayerContainer *container,
                                         gdouble                  x,
                                         gdouble                  y)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  gint origin_x, origin_y;

  gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (container)), &origin_x, &origin_y);
  gtk_window_move (GTK_WINDOW (priv->tooltip.window), x + origin_x, y + origin_y + 20);
}

/* Обработчик "motion-notify-event". */
static gboolean
hyscan_gtk_layer_container_motion_notify (GtkWidget      *widget,
                                          GdkEventMotion *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  gconstpointer handle_owner;

  /* Если пользователь схватил хэндл, то не выводим подсказки. */
  handle_owner = hyscan_gtk_layer_container_get_handle_grabbed (container);
  if (handle_owner != NULL)
    return GDK_EVENT_PROPAGATE;

  hyscan_gtk_layer_container_handle_show (container, event->x, event->y);
  hyscan_gtk_layer_container_hint_show (container, event->x, event->y);

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик "button-release-event". */
static gboolean
hyscan_gtk_layer_container_button_release (GtkWidget       *widget,
                                           GdkEventButton  *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  gconstpointer handle_owner;

  hyscan_gtk_layer_container_tooltip_move (container, event->x, event->y);

  /* Обрабатываем только нажатия левой и правой клавишами мыши. */
  if (event->button != GDK_BUTTON_PRIMARY && event->button != GDK_BUTTON_SECONDARY)
    return GDK_EVENT_PROPAGATE;

  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return GDK_EVENT_PROPAGATE;

  /* Нам нужно выяснить, кто имеет право отреагировать на это воздействие.
   * Возможны следующие ситуации:
   * - handle_owner != NULL - значит, этот слой уже обрабатывает взаимодействия,
   *   спрашиваем, не хочет ли слой отпустить хэндл.
   * - handle_owner == NULL - значит, никто не обрабатывает взаимодействие,
   *   спрашиваем, не хочет ли какой-то слой схватить хэндл.
   * - если никто не хочет, то предлагаем создать хэндл.
   * - если никто не создал, позволяем событию случаться дальше.
   */
  handle_owner = hyscan_gtk_layer_container_get_handle_grabbed (container);
  if (handle_owner != NULL)
    return hyscan_gtk_layer_container_handle_release (container, handle_owner, event);

  if (hyscan_gtk_layer_container_handle_click (container, event))
    return GDK_EVENT_STOP;

  if (hyscan_gtk_layer_container_handle_create (container, event))
    return GDK_EVENT_STOP;

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик сигнала "visible-draw". Рисует заливку. */
static void
hyscan_gtk_layer_container_draw (HyScanGtkLayerContainer *container,
                                 cairo_t                 *cairo)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;

  gdk_cairo_set_source_rgba (cairo, &priv->background);
  cairo_paint (cairo);
}

/* Устанавливает всплывающую подсказку. */
static void
hyscan_gtk_layer_container_set_hint (HyScanGtkLayerContainer *container,
                                     const gchar             *hint,
                                     gdouble                  x,
                                     gdouble                  y,
                                     HyScanGtkLayer          *hint_layer)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;

  if (hint == NULL && priv->hint != NULL)
    gtk_widget_hide (priv->tooltip.window);
  else if (hint != NULL && priv->hint == NULL)
    gtk_widget_show_all (priv->tooltip.window);

  if (hint != NULL)
    {
      hyscan_gtk_layer_container_tooltip_move (container, x, y);
      gtk_label_set_text (GTK_LABEL (priv->tooltip.label), hint);
      /* Уменьшаем окно до размеров текста. */
      gtk_window_resize (GTK_WINDOW (priv->tooltip.window), 1, 1);
    }

  g_free (priv->hint);
  priv->hint = g_strdup (hint);

  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      hyscan_gtk_layer_hint_shown (info->layer, info->layer == hint_layer);
    }
}

/* Функция создает новую HyScanGtkLayerContainerInfo. */
static HyScanGtkLayerContainerInfo *
hyscan_gtk_layer_container_info_new (HyScanGtkLayer *layer,
                                     const gchar    *key)
{
  HyScanGtkLayerContainerInfo * info = g_slice_new0 (HyScanGtkLayerContainerInfo);

  info->layer = g_object_ref_sink (layer);
  info->key = g_strdup (key);

  return info;
}

/* Функция освобождает HyScanGtkLayerContainerInfo. */
static void
hyscan_gtk_layer_container_info_free (gpointer data)
{
  HyScanGtkLayerContainerInfo * info = data;

  g_object_unref (info->layer);
  g_free (info->key);

  g_slice_free (HyScanGtkLayerContainerInfo, info);
}

/* Функция поиска по ключу. */
static gint
hyscan_gtk_layer_container_info_find (gconstpointer _a,
                                      gconstpointer _b)
{
  const HyScanGtkLayerContainerInfo * a = _a;
  const gchar * b = _b;

  return g_strcmp0 (a->key, b);
}

/**
 * hyscan_gtk_layer_container_add:
 * @container: указатель на #HyScanGtkLayerContainer
 * @key: уникальный идентификатор слоя
 * @layer: слой #HyScanGtkLayer
 *
 * Добавляет в контейнер новый слой @layer. Если слой с таким идентификатором
 * уже существует, то старый слой будет удален.
 */
void
hyscan_gtk_layer_container_add (HyScanGtkLayerContainer *container,
                                HyScanGtkLayer          *layer,
                                const gchar             *key)
{
  HyScanGtkLayerContainerPrivate *priv;
  HyScanGtkLayerContainerInfo *info;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  /* Удаляем старый слой. */
  hyscan_gtk_layer_container_remove (container, key);

  info = hyscan_gtk_layer_container_info_new (layer, key);
  priv = container->priv;
  priv->layers = g_list_append (priv->layers, info);

  hyscan_gtk_layer_added (layer, container);
}

/**
 * hyscan_gtk_layer_container_remove:
 * @container: указатель на контейнер
 * @layer: указатель на слой #HyScanGtkLayer
 *
 * Удаляет слой @layer из контейнера.
 */
void
hyscan_gtk_layer_container_remove (HyScanGtkLayerContainer *container,
                                   const gchar             *key)
{
  HyScanGtkLayerContainerPrivate *priv;
  HyScanGtkLayerContainerInfo *info;
  GList *link;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  priv = container->priv;

  /* Ищем слой в контейнере. */
  link = g_list_find_custom (priv->layers, key, hyscan_gtk_layer_container_info_find);
  if (link == NULL)
    return;

  /* Удаляем слой. */
  priv->layers = g_list_remove_link (priv->layers, link);
  info = link->data;
  hyscan_gtk_layer_removed (info->layer);
  g_list_free_full (link, hyscan_gtk_layer_container_info_free);
}

/**
 * hyscan_gtk_layer_container_remove_all:
 * @container: указатель на #HyScanGtkLayerContainer
 *
 * Удаляет все слои из контейнера.
 */
void
hyscan_gtk_layer_container_remove_all (HyScanGtkLayerContainer *container)
{
  HyScanGtkLayerContainerPrivate *priv;
  GList *link;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  priv = container->priv;

  for (link = priv->layers; link != NULL; link = priv->layers)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      hyscan_gtk_layer_container_remove (container, info->key);
    }
}

/**
 * hyscan_gtk_layer_container_get_param:
 * @container: указатель на #HyScanGtkLayerContainer
 *
 * Формирует прокси параметров оформления контейнера и вложенных слоев. Параметры слоя
 * размещаются в ветке /container, каждого из слоев - в ветке соответствующей названию слоя,
 * указанного в функции hyscan_gtk_layer_container_add().
 *
 * Схема полученного объекта #HyScanParam может быть изменена при добавлении или
 * удалении слоев из контейнера.
 *
 * Returns: (transfer full): новый объек HyScanParam. Для удаления g_object_unref().
 */
HyScanParam *
hyscan_gtk_layer_container_get_param (HyScanGtkLayerContainer *container)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;
  HyScanParamProxy *param_proxy;

  param_proxy = hyscan_param_proxy_new ();
  hyscan_param_proxy_add (param_proxy, "/container", HYSCAN_PARAM (priv->params), "/");
  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      HyScanParam *layer_param;
      gchar *prefix;

      if (info->key == NULL)
        continue;

      layer_param = hyscan_gtk_layer_get_param (info->layer);
      if (layer_param == NULL)
        continue;

      prefix = g_strdup_printf ("/%s", info->key);
      hyscan_param_proxy_add (param_proxy, prefix, layer_param, "/");
      g_free (prefix);

      g_object_unref (layer_param);
    }

  hyscan_param_proxy_bind (param_proxy);

  return HYSCAN_PARAM (param_proxy);
}

/**
 * hyscan_gtk_layer_container_lookup:
 * @container: указатель на #HyScanGtkLayerContainer
 * @key: ключ для поиска
 *
 * Ищет в контейнере слой, соответствующий ключу @key.
 *
 * Returns: (transfer none): указатель на найденный слой или %NULL, если слой не найден.
 */
HyScanGtkLayer *
hyscan_gtk_layer_container_lookup (HyScanGtkLayerContainer *container,
                                   const gchar             *key)
{
  GList *link;
  HyScanGtkLayerContainerInfo *info;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  link = g_list_find_custom (container->priv->layers, key,
                             hyscan_gtk_layer_container_info_find);
  if (link == NULL)
    return NULL;

  info = link->data;
  return info->layer;
}

/**
 * hyscan_gtk_layer_container_set_changes_allowed:
 * @container: указатель на #HyScanGtkLayerContainer
 * @changes_allowed: %TRUE, если редактирование разрешено; иначе %FALSE
 *
 * Устанавливает, разрешено ли редактирование слоёв или нет.
 */
void
hyscan_gtk_layer_container_set_changes_allowed (HyScanGtkLayerContainer *container,
                                                gboolean                 changes_allowed)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  container->priv->changes_allowed = changes_allowed;
}

/**
 * hyscan_gtk_layer_container_get_changes_allowed:
 * @container: указатель на #HyScanGtkLayerContainer
 *
 * Returns: %TRUE, если редактирование слоёв разрешено
 */
gboolean
hyscan_gtk_layer_container_get_changes_allowed (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), FALSE);

  return container->priv->changes_allowed;
}

/**
 * hyscan_gtk_layer_container_set_input_owner:
 * @container: указатель на #HyScanGtkLayerContainer
 * @instance: новый владелц ввода или %NULL
 */
void
hyscan_gtk_layer_container_set_input_owner (HyScanGtkLayerContainer *container,
                                            gconstpointer            instance)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  container->priv->iowner = instance;
}

/**
 * hyscan_gtk_layer_container_get_input_owner:
 * @container: указатель на #HyScanGtkLayerContainer
 *
 * Returns: текущий владелец ввода
 */
gconstpointer
hyscan_gtk_layer_container_get_input_owner (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  return container->priv->iowner;
}

/**
 * hyscan_gtk_layer_container_set_handle_grabbed:
 * @container: указатель на #HyScanGtkLayerContainer
 * @instance: новый владеле хэндла или %NULL
 *
 * Устанавливает нового владельца хэндла.
 */
void
hyscan_gtk_layer_container_set_handle_grabbed (HyScanGtkLayerContainer *container,
                                               gconstpointer            instance)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  if (instance != NULL)
    hyscan_gtk_layer_container_set_hint (container, NULL, 0, 0, NULL);

  container->priv->howner = instance;
}

/**
 * hyscan_gtk_layer_container_get_handle_grabbed:
 * @container: указатель на #HyScanGtkLayerContainer
 *
 * Returns: текущий владелец хэндла или %NULL
 */
gconstpointer
hyscan_gtk_layer_container_get_handle_grabbed (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  return container->priv->howner;
}
