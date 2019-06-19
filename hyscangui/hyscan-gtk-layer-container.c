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
 * Чтобы перехватить "button-release-event" до сигналов "handle-grab" и "handle-release",
 * необходимо подключиться к нему с помощью функции g_signal_connect(). Чтобы поймать
 * сигнал при отсутствии хэндла - g_signal_connect_after() (см. схему ниже).
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
 * Регистрация слоёв происходит с помощью функции hyscan_gtk_layer_container_add().
 * Контейнер сообщает слою о регистрации вызовом функции hyscan_gtk_layer_added().
 * В реализации #HyScanGtkLayerInterface.added() слой может подключиться к нужным
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
 *   - hyscan_gtk_layer_container_set_changes_allowed() - разрешает или запрещает изменения
 *   - hyscan_gtk_layer_container_get_changes_allowed() - позволяет определнить, разрешены ли изменения
 *
 * - handle_grabbed - хэндл уже захвачен
 *
 *   Это второе разграничение. Если хэндл отличен от %NULL, значит, кто-то
 *   уже обрабатывает пользовательский ввод: создает метку, передвигает линейку и т.д.
 *   Это запрещает обработку ввода всем слоям кроме собственно владельца хэндла.
 *   - hyscan_gtk_layer_container_set_handle_grabbed() - захват хэндла
 *   - hyscan_gtk_layer_container_get_handle_grabbed() - возвращает владельца хэндла
 *
 * - input_owner - владелец ввода
 *
 *   Последнее разграничение. Если хэндл никем не захвачен, то владелец ввода может
 *   начать создание нового объекта.
 *   - hyscan_gtk_layer_container_set_input_owner() - захват ввода
 *   - hyscan_gtk_layer_container_get_input_owner() - возвращает владельца ввода
 *
 */

#include "hyscan-gtk-layer-container.h"
#include "hyscan-gui-marshallers.h"

#define INI_GROUP_NAME     "container"
#define COLOR_TEXT_DEFAULT "rgb(0,0,0)"
#define COLOR_BG_DEFAULT   "rgba(255, 255, 255, 0.9)"

enum
{
  PROP_O,
};

enum
{
  SIGNAL_HANDLE,
  SIGNAL_HANDLE_RELEASE,
  SIGNAL_LAST
};

struct _HyScanGtkLayerContainerPrivate
{
  GList                       *layers;            /* Упорядоченный список зарегистрированных слоёв. */
  GHashTable                  *layers_table;      /* Таблица для хранения ИД слоёв. */

  /* Хэндл — это интерактивный элемент слоя, который можно "взять" кликом мыши. */
  gconstpointer                howner;            /* Текущий активный хэндл, с которым взаимодействует пользователь. */

  gconstpointer                iowner;            /* Слой, которому разрешён ввод. */
  gboolean                     changes_allowed;   /* Признак, что ввод в принципе разрешён пользователем. */

  gchar                        *hint;             /* Текст всплывающей подсказки. */
  gdouble                       hint_x;           /* Координата x положения всплывающей подсказки. */
  gdouble                       hint_y;           /* Координата y положения всплывающей подсказки. */
  PangoLayout                  *pango_layout;     /* Раскладка шрифта. */
  GdkRGBA                       color_text;       /* Цвет текста всплывающей подсказки. */
  GdkRGBA                       color_bg;         /* Фоновый цвет всплывающей подсказки. */
};

static void         hyscan_gtk_layer_container_object_constructed      (GObject                 *object);
static void         hyscan_gtk_layer_container_object_finalize         (GObject                 *object);
static gboolean     hyscan_gtk_layer_container_configure               (GtkWidget               *widget,
                                                                        GdkEventConfigure       *event);
static gboolean     hyscan_gtk_layer_container_motion_notify           (GtkWidget               *widget,
                                                                        GdkEventMotion          *event);
static void         hyscan_gtk_layer_container_draw                    (HyScanGtkLayerContainer *container,
                                                                        cairo_t                 *cairo);
static gboolean     hyscan_gtk_layer_container_button_release          (GtkWidget               *widget,
                                                                        GdkEventButton          *event);
static void         hyscan_gtk_layer_container_unrealize               (GtkWidget               *widget);
static gboolean     hyscan_gtk_layer_container_grab_accu               (GSignalInvocationHint   *ihint,
                                                                        GValue                  *return_accu,
                                                                        const GValue            *handler_return,
                                                                        gpointer                 data);
static gboolean     hyscan_gtk_layer_container_release_accu            (GSignalInvocationHint   *ihint,
                                                                        GValue                  *return_accu,
                                                                        const GValue            *handler_return,
                                                                        gpointer                 data);


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
  hyscan_gtk_layer_container_signals[SIGNAL_HANDLE] =
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

  priv->layers_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
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
  g_hash_table_destroy (priv->layers_table);
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

/* Пытается отпустить хэндл, эмитируя сигнал "handle-release". */
static gboolean
hyscan_gtk_layer_container_release_handle (HyScanGtkLayerContainer *container,
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

/* Устанавливает всплывающую подсказку. */
static void
hyscan_gtk_layer_container_set_hint (HyScanGtkLayerContainer *container,
                                     const gchar             *hint,
                                     gdouble                  x,
                                     gdouble                  y,
                                     HyScanGtkLayer          *hint_layer)
{
  HyScanGtkLayerContainerPrivate *priv = container->priv;
  GList *layer_l;

  if (hint != NULL || priv->hint != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (container));

  g_free (priv->hint);
  priv->hint = g_strdup (hint);
  priv->hint_x = x;
  priv->hint_y = y;

  for (layer_l = priv->layers; layer_l != NULL; layer_l = layer_l->next)
    {
      HyScanGtkLayer *layer = layer_l->data;

      hyscan_gtk_layer_hint_shown (layer, layer == hint_layer);
    }
}

/* Пытается захватить хэндл, эмитируя сигнал "handle-grab". */
static gboolean
hyscan_gtk_layer_container_grab_handle (HyScanGtkLayerContainer *container,
                                        GdkEventButton          *event)
{
  gconstpointer handle_owner = NULL;

  g_signal_emit (container, hyscan_gtk_layer_container_signals[SIGNAL_HANDLE], 0, event, &handle_owner);
  hyscan_gtk_layer_container_set_handle_grabbed (container, handle_owner);

  return handle_owner != NULL ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
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
  GList *layer_l;

  HyScanGtkLayer *hint_layer = NULL;
  gchar *hint = NULL;
  gdouble distance = G_MAXDOUBLE;

  /* Если пользователь схватил хэндл, то не выводим подсказки. */
  if (hyscan_gtk_layer_container_get_handle_grabbed (container) != NULL)
    return GDK_EVENT_PROPAGATE;

  /* Запрашивает у каждого видимого слоя всплывающую подсказку. */
  for (layer_l = priv->layers; layer_l != NULL; layer_l = layer_l->next)
    {
      HyScanGtkLayer *layer = layer_l->data;
      gchar *layer_hint = NULL;
      gdouble layer_distance;

      if (!hyscan_gtk_layer_get_visible (layer))
        continue;

      layer_hint = hyscan_gtk_layer_hint_find (layer, event->x, event->y, &layer_distance);
      if (layer_hint == NULL || layer_distance > distance)
        {
          g_free (layer_hint);
          continue;
        }

      /* Запоминаем текущую подсказку. */
      distance = layer_distance;
      g_free (hint);
      hint = layer_hint;
      hint_layer = layer;
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
   */
  handle_owner = hyscan_gtk_layer_container_get_handle_grabbed (container);
  if (handle_owner != NULL)
    return hyscan_gtk_layer_container_release_handle (container, handle_owner, event);
  else
    return hyscan_gtk_layer_container_grab_handle (container, event);

}

/* Коллбэк для g_hash_table_foreach_remove.
 * Просит удалить из таблицы все слои remove_layer. */
static gboolean
hyscan_gtk_layer_hash_table_remove (gpointer key,
                                    gpointer layer,
                                    gpointer remove_layer)
{
  return layer == remove_layer;
}

/* Удаляет слой из контейнера.
 * Предварительно слой должен быть удалён из списка priv->layers. */
static inline void
hyscan_gtk_layer_container_layer_removed (HyScanGtkLayerContainerPrivate *priv,
                                          HyScanGtkLayer                 *layer)
{
  g_hash_table_foreach_remove (priv->layers_table, hyscan_gtk_layer_hash_table_remove, layer);
  hyscan_gtk_layer_removed (layer);
  g_object_unref (layer);
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

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  priv = container->priv;
  priv->layers = g_list_append (priv->layers, g_object_ref_sink (layer));
  if (key != NULL)
    g_hash_table_insert (priv->layers_table, g_strdup (key), layer);

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
                                   HyScanGtkLayer          *layer)
{
  HyScanGtkLayerContainerPrivate *priv;
  GList *found;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  priv = container->priv;

  /* Ищем слой в контейнере. */
  found = g_list_find (priv->layers, layer);
  if (found == NULL)
    return;

  /* Удаляем слой. */
  priv->layers = g_list_delete_link (priv->layers, found);
  hyscan_gtk_layer_container_layer_removed (priv, layer);
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
  GList *layer_l;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  priv = container->priv;

  layer_l = priv->layers;
  while (layer_l != NULL)
    {
      GList *next_l = layer_l->next;

      hyscan_gtk_layer_container_remove (container, layer_l->data);
      layer_l = next_l;
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
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  return g_hash_table_lookup (container->priv->layers_table, key);
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
  GHashTableIter iter;
  gchar *key;
  HyScanGtkLayer *layer;
  
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  priv = container->priv;

  /* Загружаем конфигурацию контейнера. */
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_text, key_file, INI_GROUP_NAME, "text-color", COLOR_TEXT_DEFAULT);
  hyscan_gtk_layer_load_key_file_rgba (&priv->color_bg,   key_file, INI_GROUP_NAME, "bg-color",   COLOR_BG_DEFAULT);

  /* Загружаем конфигурацию каждого слоя. */
  g_hash_table_iter_init (&iter, priv->layers_table);
  while (g_hash_table_iter_next (&iter, (gpointer) &key, (gpointer) &layer))
    hyscan_gtk_layer_load_key_file (layer, key_file, key);
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
