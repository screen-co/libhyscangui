/**
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
 * Сигнал #HyScanGtkLayerContainer::handle жизненно необходим всем желающим
 * реагировать на действия пользователя. Контейнер эмитирует этот сигнал, когда
 * пользователь отпускает кнопку мыши и никто не держит хэндл. В обработчике
 * сигнала можно определить, есть ли под указателем хэндл и вернуть идентификатор
 * класса (указатель на себя) если хэндл найден и %NULL в противном случае.
 *
 * Эмиссия сигнала прекращается, как только кто-то вернул отличное от %NULL значение.
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
 * Подключение к сигналам мыши и клавиатуры должно производиться ТОЛЬКО с помощью
 * g_signal_connect_after!
 *
 * Пользовательский ввод можно разделить на две части: создание новых хэндлов и
 * изменение уже существующих. Чтобы слои могли понимать, кто имеет право обработать ввод,
 * введены следующие ограничения.
 *
 * - editable - режим просмотра или редактирования
 *
 *   Это первое разграничение, самое глобальное. Оно означает режим просмотра или
 *   режим редактирования. В режиме просмотра слои не имеют права обрабатывать
 *   никакой пользовательский ввод.
 *   - hyscan_gtk_layer_container_set_editable() - разрешает или запрещает изменения
 *   - hyscan_gtk_layer_container_get_editable() - позволяет определнить, разрешены ли изменения
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

enum
{
  PROP_O,
};

enum
{
  SIGNAL_HANDLE,
  SIGNAL_LAST
};

struct _HyScanGtkLayerContainerPrivate
{
  GList                       *layers;            /* Упорядоченный список слоёв. */

  /* Хэндл — это интерактивный элемент слоя, который можно "взять" кликом мыши. */
  gconstpointer                hover_handle;      /* Хэндл под указателем мыши, за который можно ухватиться. */
  gconstpointer                howner;            /* Текущий активный хэндл, с которым взаимодействует пользователь. */

  gconstpointer                iowner;            /* Слой, которому разрешён ввод. */
  gboolean                     changes_allowed;   /* Признак, что ввод в принципе разрешён пользователем. */
};

static void         hyscan_gtk_layer_container_object_constructed      (GObject                 *object);
static void         hyscan_gtk_layer_container_object_finalize         (GObject                 *object);
static gboolean     hyscan_gtk_layer_container_apply_handle            (GtkWidget               *widget,
                                                                        GdkEventButton          *event,
                                                                        HyScanGtkLayerContainer *container);
static gboolean     hyscan_gtk_layer_container_resolve_handle          (GtkWidget               *widget,
                                                                        GdkEventButton          *event,
                                                                        HyScanGtkLayerContainer *container);
static gboolean     hyscan_gtk_layer_container_accumulator             (GSignalInvocationHint   *ihint,
                                                                        GValue                  *return_accu,
                                                                        const GValue            *handler_return,
                                                                        gpointer                 data);


static guint hyscan_gtk_layer_container_signals[SIGNAL_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkLayerContainer, hyscan_gtk_layer_container, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_layer_container_class_init (HyScanGtkLayerContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_layer_container_object_constructed;
  object_class->finalize = hyscan_gtk_layer_container_object_finalize;

  hyscan_gtk_layer_container_signals[SIGNAL_HANDLE] =
    g_signal_new ("handle", HYSCAN_TYPE_GTK_LAYER_CONTAINER,
                  G_SIGNAL_RUN_LAST,
                  0, hyscan_gtk_layer_container_accumulator, NULL,
                  hyscan_gui_marshal_POINTER__POINTER,
                  G_TYPE_POINTER,
                  1, G_TYPE_POINTER);
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
  GtkWidget *widget = GTK_WIDGET (object);
  HyScanGtkLayerContainerPrivate *priv = container->priv;

  G_OBJECT_CLASS (hyscan_gtk_layer_container_parent_class)->constructed (object);

  priv->changes_allowed = TRUE;
  g_signal_connect (widget, "motion-notify-event",
                    G_CALLBACK (hyscan_gtk_layer_container_resolve_handle), container);
  g_signal_connect (widget, "button-release-event",
                    G_CALLBACK (hyscan_gtk_layer_container_apply_handle), container);
}

static void
hyscan_gtk_layer_container_object_finalize (GObject *object)
{
  HyScanGtkLayerContainer *gtk_layer_container = HYSCAN_GTK_LAYER_CONTAINER (object);
  HyScanGtkLayerContainerPrivate *priv = gtk_layer_container->priv;

  g_list_free_full (priv->layers, g_object_unref);
  g_signal_handlers_disconnect_by_data (object, object);

  G_OBJECT_CLASS (hyscan_gtk_layer_container_parent_class)->finalize (object);
}

/* Аккумулятор эмиссии сигнала "handle". */
static gboolean
hyscan_gtk_layer_container_accumulator (GSignalInvocationHint *ihint,
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

/* Устанавливает хэндл по нажатию мыши. */
static gboolean
hyscan_gtk_layer_container_apply_handle (GtkWidget               *widget,
                                         GdkEventButton          *event,
                                         HyScanGtkLayerContainer *container)
{
  gconstpointer handle = NULL;

  /* Обрабатываем только нажатия левой клавишей мыши. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  /* Нам нужно выяснить, кто имеет право отреагировать на это воздействие.
   * Возможны следующие ситуации:
   * - handle_owner != NULL - значит, этот слой уже обрабатывает взаимодействия,
   *   нельзя ему мешать
   * - handle_owner == NULL - значит, никто не обрабатывает взаимодействие.
   */
  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return GDK_EVENT_PROPAGATE;

  handle = hyscan_gtk_layer_container_get_handle_grabbed (container);
  if (handle != NULL)
    return GDK_EVENT_PROPAGATE;

  hyscan_gtk_layer_container_set_handle_grabbed (container, container->priv->hover_handle);
  container->priv->hover_handle = NULL;

  return GDK_EVENT_PROPAGATE;
}

/* Определяет доступный хэндл под указателем мыши. */
static gboolean
hyscan_gtk_layer_container_resolve_handle (GtkWidget               *widget,
                                           GdkEventButton          *event,
                                           HyScanGtkLayerContainer *container)
{
  gconstpointer handle_owner = NULL;

 /* Нам нужно выяснить, кто имеет право отреагировать на это воздействие.
  * Возможны следующие ситуации:
  * - handle_owner != NULL - значит, этот слой уже обрабатывает взаимодействия,
  *   нельзя ему мешать
  * - handle_owner == NULL - значит, никто не обрабатывает взаимодействие.
  */
  handle_owner = hyscan_gtk_layer_container_get_handle_grabbed (container);
  if (handle_owner != NULL)
    return GDK_EVENT_PROPAGATE;

  g_signal_emit (container, hyscan_gtk_layer_container_signals[SIGNAL_HANDLE], 0, event, &handle_owner);
  container->priv->hover_handle = handle_owner;
  g_message ("Hover howner %p", handle_owner);

  return GDK_EVENT_PROPAGATE;
}

/**
 * hyscan_gtk_layer_container_add:
 * @container:
 * @layer:
 *
 * Добавляет в контейнер новый слой.
 */
void
hyscan_gtk_layer_container_add (HyScanGtkLayerContainer *container,
                                HyScanGtkLayer          *layer)
{
  HyScanGtkLayerContainerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));
  g_return_if_fail (HYSCAN_IS_GTK_LAYER (layer));

  priv = container->priv;
  priv->layers = g_list_append (priv->layers, g_object_ref (layer));
  hyscan_gtk_layer_added (layer, container);
}

/**
 * hyscan_gtk_layer_container_get_input_owner:
 * @container:
 * Returns:
 */
gconstpointer
hyscan_gtk_layer_container_get_input_owner (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  return container->priv->iowner;
}

/**
 * hyscan_gtk_layer_container_set_input_owner:
 * @container:
 * @instance:
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
 * @container:
 * @instance:
 */
void
hyscan_gtk_layer_container_set_handle_grabbed (HyScanGtkLayerContainer *container,
                                               gconstpointer            instance)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  container->priv->howner = instance;
}

/**
 * hyscan_gtk_layer_container_get_handle_grabbed:
 * @container
 * Returns:
 */
gconstpointer
hyscan_gtk_layer_container_get_handle_grabbed (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), NULL);

  return container->priv->howner;
}

/**
 * hyscan_gtk_layer_container_get_changes_allowed:
 * @container:
 * Returns:
 */
gboolean
hyscan_gtk_layer_container_get_changes_allowed (HyScanGtkLayerContainer *container)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container), FALSE);

  return container->priv->changes_allowed;
}

/**
 * hyscan_gtk_layer_container_set_changes_allowed:
 * @container:
 * @changes_allowed:
 */
void
hyscan_gtk_layer_container_set_changes_allowed (HyScanGtkLayerContainer *container,
                                                gboolean                 changes_allowed)
{
  g_return_if_fail (HYSCAN_IS_GTK_LAYER_CONTAINER (container));

  container->priv->changes_allowed = changes_allowed;
}
