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
 * Сигнал #HyScanGtkLayerContainer::handle-grab жизненно необходим всем желающим
 * реагировать на действия пользователя. Контейнер эмитирует этот сигнал, когда
 * пользователь отпускает кнопку мыши ("button-release-event") и никто не держит
 * хэндл. В обработчике сигнала можно определить, есть ли под указателем хэндл
 * и вернуть идентификатор класса (указатель на себя) если хэндл найден или
 * %NULL в противном случае.
 *
 * Эмиссия сигнала "handle-grab" прекращается, как только кто-то вернул отличное от
 * %NULL значение. В этом случае также прекращается и эмиссия сигнала "button-release-event".
 *
 * Хэндл отпускается в момент следующего клика мыши. Для этого контейнер эмитирует
 * сигнал "handle-release". В обработчике сигнала можно определить, принадлежит
 * ли текущий хэндл слою и вернуть %TRUE, если слой отпускает хэндл.
 *
 * Если никто не схватил хэндл, то эмиттируется сигнал "handle-create". Слой
 * должен проверить, есть ли у него право обработки ввода
 * (#hyscan_gtk_layer_container_get_input_owner) и если есть -- создать хэндл.
 *
 * Чтобы перехватить "button-release-event" до сигналов "handle-grab", "handle-release",
 * и "handle-create", необходимо подключиться к нему с помощью функции
 * #g_signal_connect. Чтобы поймать сигнал при отсутствии хэндла -- #g_signal_connect_after
 * (см. схему ниже).
 *
 * ## Блок-схема эмиссии сигнала "button-release-event"
 *
 *
 *                                [ HANDLER_RUN_FIRST ]
 *                                          v
 *                                  (handle == NULL)
 *                                         + +
 *                                     yes | | no
 *                     v-------------------+ +----------------------v
 *              emit "handle-grab"                       emit "handle-release"
 *                    + +                                          + +
 *              found | | not found                       released | | not released
 *         v----------+ +----------+                    v----------+ +----------v
 *  handle = h_found;              |                 handle = NULL;       STOP EMISSION
 *  STOP EMISSION                  |                 STOP EMISSION
 *                                 v
 *                        [ HANDLER_RUN_LAST ]
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

#define INI_GROUP_NAME     "container"
#define COLOR_TEXT_DEFAULT "rgb(0,0,0)"
#define COLOR_BG_DEFAULT   "rgba(255, 255, 255, 0.9)"

enum
{
  SIGNAL_HANDLE_CREATE,
  SIGNAL_HANDLE_GRAB,
  SIGNAL_HANDLE_RELEASE,
  SIGNAL_LAST
};

typedef struct
{
  HyScanGtkLayer *layer;
  gchar          *key;
} HyScanGtkLayerContainerInfo;

struct _HyScanGtkLayerContainerPrivate
{
  GList         *layers;           /* Упорядоченный список зарегистрированных слоёв. */

  /* Хэндл — это интерактивный элемент слоя, который можно "взять" кликом мыши. */
  gconstpointer  howner;           /* Текущий активный хэндл, с которым взаимодействует пользователь. */

  gconstpointer  iowner;           /* Слой, которому разрешён ввод. */
  gboolean       changes_allowed;  /* Признак, что ввод в принципе разрешён пользователем. */

  gchar         *hint;             /* Текст всплывающей подсказки. */
  gdouble        hint_x;           /* Координата x положения всплывающей подсказки. */
  gdouble        hint_y;           /* Координата y положения всплывающей подсказки. */
  PangoLayout   *pango_layout;     /* Раскладка шрифта. */
  GdkRGBA        color_text;       /* Цвет текста всплывающей подсказки. */
  GdkRGBA        color_bg;         /* Фоновый цвет всплывающей подсказки. */
};

static void         hyscan_gtk_layer_container_object_constructed      (GObject                 *object);
static void         hyscan_gtk_layer_container_object_finalize         (GObject                 *object);
static gboolean     hyscan_gtk_layer_container_grab_accu               (GSignalInvocationHint   *ihint,
                                                                        GValue                  *return_accu,
                                                                        const GValue            *handler_return,
                                                                        gpointer                 data);
static gboolean     hyscan_gtk_layer_container_handle_create           (HyScanGtkLayerContainer *container,
                                                                        GdkEventButton          *event);
static gboolean     hyscan_gtk_layer_container_handle_grab             (HyScanGtkLayerContainer *container,
                                                                        GdkEventButton          *event);
static gboolean     hyscan_gtk_layer_container_handle_release          (HyScanGtkLayerContainer *container,
                                                                        gconstpointer            handle_owner,
                                                                        GdkEventButton          *event);
static gboolean     hyscan_gtk_layer_container_release_accu            (GSignalInvocationHint   *ihint,
                                                                        GValue                  *return_accu,
                                                                        const GValue            *handler_return,
                                                                        gpointer                 data);
static void         hyscan_gtk_layer_container_unrealize               (GtkWidget               *widget);
static gboolean     hyscan_gtk_layer_container_configure               (GtkWidget               *widget,
                                                                        GdkEventConfigure       *event);
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

static guint hyscan_gtk_layer_container_signals[SIGNAL_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkLayerContainer, hyscan_gtk_layer_container, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_layer_container_class_init (HyScanGtkLayerContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = hyscan_gtk_layer_container_object_constructed;
  object_class->finalize = hyscan_gtk_layer_container_object_finalize;

  widget_class->configure_event = hyscan_gtk_layer_container_configure;
  widget_class->motion_notify_event = hyscan_gtk_layer_container_motion_notify;
  widget_class->button_release_event = hyscan_gtk_layer_container_button_release;
  widget_class->unrealize = hyscan_gtk_layer_container_unrealize;

  /**
   * HyScanGtkLayerContainer::handle-create:
   * @container: объект, получивший сигнал
   * @event: #GdkEventButton оригинального события "button-release-event"
   *
   * Сигнал отправляется при необходимости создания под указателем мыши,
   * когда пользователь отпускает левую кнопку. Отправляется только если в данный
   * момент никакой хэндл не захвачен.
   *
   * Returns: указатель на слой, если слой создал хэндл; %NULL, если хэндл не создан.
   */
  hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_CREATE] =
    g_signal_new ("handle-create", HYSCAN_TYPE_GTK_LAYER_CONTAINER,
                  G_SIGNAL_RUN_LAST,
                  0, hyscan_gtk_layer_container_grab_accu, NULL,
                  hyscan_gui_marshal_POINTER__POINTER,
                  G_TYPE_POINTER,
                  1, G_TYPE_POINTER);

  /**
   * HyScanGtkLayerContainer::handle-grab:
   * @container: объект, получивший сигнал
   * @event: #GdkEventButton оригинального события "button-release-event"
   *
   * Сигнал отправляется при необходимости захвата хэндла под указателем мыши,
   * когда пользователь отпускает левую кнопку. Отправляется только если в данный
   * момент никакой хэндл не захвачен.
   *
   * Returns: указатель на хэндл, если слой берет хэндл; или %NULL, если у слоя
   *          нет хэндла под указателем мыши.
   */
  hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_GRAB] =
    g_signal_new ("handle-grab", HYSCAN_TYPE_GTK_LAYER_CONTAINER,
                  G_SIGNAL_RUN_LAST,
                  0, hyscan_gtk_layer_container_grab_accu, NULL,
                  hyscan_gui_marshal_POINTER__POINTER,
                  G_TYPE_POINTER,
                  1, G_TYPE_POINTER);

  /**
   * HyScanGtkLayerContainer::handle-release:
   * @container: объект, получивший сигнал
   * @event: #GdkEventButton оригинального события "button-release-event"
   * @handle: хэндл, который сейчас захвачен
   *
   * Сигнал отправляется при необходимости отпустить хэндл при очередном клике
   * левой кнопки мыши. Отправляется только если в данный момент хэндл захвачен.
   *
   * Returns: %TRUE, если слой отпускает хэндл; иначе %FALSE.
   */
  hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_RELEASE] =
    g_signal_new ("handle-release", HYSCAN_TYPE_GTK_LAYER_CONTAINER,
                  G_SIGNAL_RUN_LAST,
                  0, hyscan_gtk_layer_container_release_accu, NULL,
                  hyscan_gui_marshal_BOOLEAN__POINTER_POINTER,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_POINTER, G_TYPE_POINTER);
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

  hyscan_gtk_layer_container_set_changes_allowed (container, TRUE);

  gdk_rgba_parse (&priv->color_bg, COLOR_BG_DEFAULT);
  gdk_rgba_parse (&priv->color_text, COLOR_TEXT_DEFAULT);

  g_signal_connect_swapped (container, "area-draw", G_CALLBACK (hyscan_gtk_layer_container_draw), container);
}

static void
hyscan_gtk_layer_container_object_finalize (GObject *object)
{
  HyScanGtkLayerContainer *gtk_layer_container = HYSCAN_GTK_LAYER_CONTAINER (object);
  HyScanGtkLayerContainerPrivate *priv = gtk_layer_container->priv;

  g_clear_object (&priv->pango_layout);
  g_free (priv->hint);
  g_list_free_full (priv->layers, g_object_unref);
  g_signal_handlers_disconnect_by_data (object, object);

  G_OBJECT_CLASS (hyscan_gtk_layer_container_parent_class)->finalize (object);
}

/* Аккумулятор эмиссии сигнала "handle-release". */
static gboolean
hyscan_gtk_layer_container_release_accu (GSignalInvocationHint *ihint,
                                         GValue                *return_accu,
                                         const GValue          *handler_return,
                                         gpointer               data)
{
  gboolean handle_released;

  handle_released = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, handle_released);

  /* Для остановки эмиссии надо вернуть FALSE.
   * Эмиссия останавливается, если слой сообщил, что он отпустил хэндл. */
  return handle_released ? FALSE : TRUE;
}

/* Аккумулятор эмиссии сигнала "handle-grab". */
static gboolean
hyscan_gtk_layer_container_grab_accu (GSignalInvocationHint *ihint,
                                      GValue                *return_accu,
                                      const GValue          *handler_return,
                                      gpointer               data)
{
  gpointer instance;

  instance = g_value_get_pointer (handler_return);
  g_value_set_pointer (return_accu, instance);

  /* Для остановки эмиссии надо вернуть FALSE.
   * Эмиссия останавливается, если нашелся хэндл. */
  return (instance == NULL);
}

/* Пытается создать хэндл, эмитируя сигнал "handle-create". */
static gboolean
hyscan_gtk_layer_container_handle_create (HyScanGtkLayerContainer *container,
                                          GdkEventButton          *event)
{
  gconstpointer handle_owner = NULL;

  g_signal_emit (container, hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_CREATE], 0, event, &handle_owner);
  hyscan_gtk_layer_container_set_handle_grabbed (container, handle_owner);

  return handle_owner != NULL;
}

/* Пытается захватить хэндл, эмитируя сигнал "handle-grab". */
static gboolean
hyscan_gtk_layer_container_handle_grab (HyScanGtkLayerContainer *container,
                                        GdkEventButton          *event)
{
  gconstpointer handle_owner = NULL;

  g_signal_emit (container, hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_GRAB], 0, event, &handle_owner);
  hyscan_gtk_layer_container_set_handle_grabbed (container, handle_owner);

  return handle_owner != NULL;
}

/* Пытается отпустить хэндл, эмитируя сигнал "handle-release". */
static gboolean
hyscan_gtk_layer_container_handle_release (HyScanGtkLayerContainer *container,
                                           gconstpointer            handle_owner,
                                           GdkEventButton          *event)
{
  gboolean released;

  g_signal_emit (container, hyscan_gtk_layer_container_signals[SIGNAL_HANDLE_RELEASE], 0,
                 event, handle_owner, &released);

  if (released)
    hyscan_gtk_layer_container_set_handle_grabbed (container, NULL);

  return GDK_EVENT_STOP;
}

/* Обработчик "unrealize-event".
 * Удаляет все слои. */
static void
hyscan_gtk_layer_container_unrealize (GtkWidget *widget)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);

  hyscan_gtk_layer_container_remove_all (container);
}

/* Обработчик "configure-event". */
static gboolean
hyscan_gtk_layer_container_configure (GtkWidget         *widget,
                                      GdkEventConfigure *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  HyScanGtkLayerContainerPrivate *priv = container->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (widget, NULL);

  return GTK_WIDGET_CLASS (hyscan_gtk_layer_container_parent_class)->configure_event (widget, event);
}

/* Обработчик "button-release-event". */
static gboolean
hyscan_gtk_layer_container_motion_notify (GtkWidget      *widget,
                                          GdkEventMotion *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *link;

  HyScanGtkLayer *hint_layer = NULL;
  gchar *hint = NULL;
  gdouble distance = G_MAXDOUBLE;

  /* Если пользователь схватил хэндл, то не выводим подсказки. */
  if (hyscan_gtk_layer_container_get_handle_grabbed (container) != NULL)
    return GDK_EVENT_PROPAGATE;

  /* Запрашивает у каждого видимого слоя всплывающую подсказку. */
  for (link = priv->layers; link != NULL; link = link->next)
    {
      // HyScanGtkLayer *layer = link->data;
      HyScanGtkLayerContainerInfo * info = link->data;
      gchar *layer_hint = NULL;
      gdouble layer_distance;

      if (!hyscan_gtk_layer_get_visible (info->layer))
        continue;

      layer_hint = hyscan_gtk_layer_hint_find (info->layer, event->x, event->y, &layer_distance);
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
  hyscan_gtk_layer_container_set_hint (container, hint, event->x, event->y, hint_layer);

  g_free (hint);

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик "button-release-event". */
static gboolean
hyscan_gtk_layer_container_button_release (GtkWidget       *widget,
                                           GdkEventButton  *event)
{
  HyScanGtkLayerContainer *container = HYSCAN_GTK_LAYER_CONTAINER (widget);
  gconstpointer handle_owner = NULL;

  /* Обрабатываем только нажатия левой клавишей мыши. */
  if (event->button != GDK_BUTTON_PRIMARY)
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

  if (hyscan_gtk_layer_container_handle_grab (container, event))
    return GDK_EVENT_STOP;

  if (hyscan_gtk_layer_container_handle_create (container, event))
    return GDK_EVENT_STOP;

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик сигнала "visible-draw". Рисует всплывающую подсказку. */
static void
hyscan_gtk_layer_container_draw (HyScanGtkLayerContainer *container,
                                 cairo_t                 *cairo)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  gint text_width, text_height;
  gdouble x, y;
  gdouble text_margin;
  gdouble box_margin;

  if (priv->hint == NULL)
    return;

  pango_layout_set_text (priv->pango_layout, priv->hint, -1);
  pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
  text_height /= PANGO_SCALE;
  text_width /= PANGO_SCALE;

  text_margin = 0.1 * text_height;
  box_margin = 0.3 * text_height;

  x = priv->hint_x;
  y = priv->hint_y + (text_height + text_margin + box_margin);
  gdk_cairo_set_source_rgba (cairo, &priv->color_bg);
  cairo_rectangle (cairo, x - text_margin, y - text_margin,
                   text_width + 2.0 * text_margin, text_height + 2.0 * text_margin);
  cairo_fill (cairo);

  cairo_move_to (cairo, x, y);
  gdk_cairo_set_source_rgba (cairo, &priv->color_text);
  pango_cairo_show_layout (cairo, priv->pango_layout);
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

  if (hint != NULL || priv->hint != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (container));

  g_free (priv->hint);
  priv->hint = g_strdup (hint);
  priv->hint_x = x;
  priv->hint_y = y;

  for (link = priv->layers; link != NULL; link = link->next)
    {
      HyScanGtkLayerContainerInfo *info = link->data;
      hyscan_gtk_layer_hint_shown (info->layer, info->layer == hint_layer);
    }
}

static HyScanGtkLayerContainerInfo *
hyscan_gtk_layer_container_info_new (HyScanGtkLayer *layer,
                                     const gchar    *key)
{
  HyScanGtkLayerContainerInfo * info = g_slice_new0 (HyScanGtkLayerContainerInfo);

  info->layer = g_object_ref_sink (layer);
  info->key = g_strdup (key);

  return info;
}

static void
hyscan_gtk_layer_container_info_free (gpointer data)
{
  HyScanGtkLayerContainerInfo * info = data;

  g_object_unref (info->layer);
  g_free (info->key);

  g_slice_free (HyScanGtkLayerContainerInfo, info);
}

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
 * Добавляет в контейнер новый слой @layer. Если слой с таким идентификатором уже
 * существует, то ключ @key теперь будет связан с новым слоем @layer.
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
 * hyscan_gtk_layer_container_load_key_file:
 * @container: указатель на #HyScanGtkLayerContainer
 * @key_file: указатель на #GKeyFile
 *
 * Загружает конфигурацию слоев из файла @key_file. Каждому слою передается
 * конфигурация из группы с именем, соответствующем ключу слоя, назначенном в
 * hyscan_gtk_layer_container_add().
 *
 * Внутри функция вызывает hyscan_gtk_layer_load_key_file() на каждом слое.
 *
 */
void
hyscan_gtk_layer_container_load_key_file (HyScanGtkLayerContainer *container,
                                          GKeyFile                *key_file)
{
  HyScanGtkLayerContainerPrivate *priv;
  GList *link;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  priv = container->priv;

  /* Загружаем конфигурацию контейнера. */
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_text, key_file, INI_GROUP_NAME, "text-color", COLOR_TEXT_DEFAULT);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_bg,   key_file, INI_GROUP_NAME, "bg-color",   COLOR_BG_DEFAULT);

  /* Загружаем конфигурацию каждого слоя. */
  for (link = priv->layers; link != NULL; link = link->next)
    {
       HyScanGtkLayerContainerInfo *info = link->data;
       hyscan_gtk_layer_load_key_file (info->layer, key_file, info->key);
    }
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
