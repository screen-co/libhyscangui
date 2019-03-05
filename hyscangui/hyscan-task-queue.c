#include "hyscan-task-queue.h"

enum
{
  PROP_O,
  PROP_TASK_FUNC,
  PROP_USER_DATA,
  PROP_TASK_FREE_FUNC,
  PROP_CMP_FUNC
};

struct _HyScanTaskQueuePrivate
{
  HyScanTaskQueueFunc task_func;            /* Функция выполнения задачи. */
  GCompareFunc        cmp_func;             /* Функция сравнения двух задач. */
  GDestroyNotify      task_free_func;       /* Функция удаления задачи. */
  guint               max_concurrent;       /* Максимальное количество задач, выполняемых одновременно. */

  /* Состояние очереди задач. */
  GQueue             *queue;                /* Очередь задач, ожидающих обработки. */
  GList              *processing_tasks;     /* Список задач, находящихся в обработке. */
  guint               processing_count;     /* Длина списка processing_tasks. */
  GMutex              queue_lock;           /* Мутекс для ограничения доступа к редактиронию переменных состояния очереди. */

  GThreadPool        *pool;                 /* Пул обработки задач. */
  gpointer            user_data;            /* Пользовательские данные для пула задач. */
  gboolean            shutdown;
  GCancellable       *cancellable;          /* Объект для отмены текущих задач. */
};

static void    hyscan_task_queue_set_property             (GObject               *object,
                                                           guint                  prop_id,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void    hyscan_task_queue_object_constructed       (GObject               *object);
static void    hyscan_task_queue_object_finalize          (GObject               *object);
static void    hyscan_task_queue_process                  (gpointer               task,
                                                           HyScanTaskQueue       *queue);
static void    hyscan_task_queue_try_next                 (HyScanTaskQueue       *queue);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTaskQueue, hyscan_task_queue, G_TYPE_OBJECT)

static void
hyscan_task_queue_class_init (HyScanTaskQueueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_task_queue_set_property;

  object_class->constructed = hyscan_task_queue_object_constructed;
  object_class->finalize = hyscan_task_queue_object_finalize;

  g_object_class_install_property (object_class, PROP_TASK_FUNC,
                                   g_param_spec_pointer ("task-func", "Task function", "GFunc",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_USER_DATA,
                                   g_param_spec_pointer ("task-data", "User data", "",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CMP_FUNC,
                                   g_param_spec_pointer ("cmp-func", "Compare function", "GCompareFunc",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_TASK_FREE_FUNC,
                                   g_param_spec_pointer ("free-func", "Task free function", "GDestroyNotify",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_task_queue_init (HyScanTaskQueue *task_queue)
{
  task_queue->priv = hyscan_task_queue_get_instance_private (task_queue);
}

static void
hyscan_task_queue_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  switch (prop_id)
    {
    case PROP_TASK_FUNC:
      priv->task_func = g_value_get_pointer (value);
      break;

    case PROP_USER_DATA:
      priv->user_data = g_value_get_pointer (value);
      break;

    case PROP_CMP_FUNC:
      priv->cmp_func = g_value_get_pointer (value);
      break;

    case PROP_TASK_FREE_FUNC:
      priv->task_free_func = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_task_queue_object_constructed (GObject *object)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  G_OBJECT_CLASS (hyscan_task_queue_parent_class)->constructed (object);

  /* todo: сколько надо потоков? */
  priv->max_concurrent = 3;
  priv->pool = g_thread_pool_new ((GFunc) hyscan_task_queue_process, task_queue,
                                  priv->max_concurrent, FALSE, NULL);
  priv->queue = g_queue_new ();
  priv->cancellable = g_cancellable_new ();

  g_mutex_init (&priv->queue_lock);
}

static void
hyscan_task_queue_object_finalize (GObject *object)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  if (!g_atomic_int_get (&priv->shutdown))
    g_warning ("HyScanTaskQueue: hyscan_task_queue_shutdown() must be called before finalize");

  /* Блокируем очередь и освобождаем память. */
  g_mutex_lock (&priv->queue_lock);

  if (priv->task_free_func != NULL)
    g_queue_free_full (priv->queue, priv->task_free_func);
  else
    g_queue_free (priv->queue);

  g_mutex_unlock (&priv->queue_lock);
  g_mutex_clear (&priv->queue_lock);

  /* Придётся подождать, пока не завершится выполнение уже отправленных задач. */
  g_thread_pool_free (priv->pool, FALSE, TRUE);
  g_object_unref (priv->cancellable);

  G_OBJECT_CLASS (hyscan_task_queue_parent_class)->finalize (object);
}

/* Обработка задачи из очереди. */
static void
hyscan_task_queue_process (gpointer         task,
                           HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv = queue->priv;

  /* Выполняем задачу. */
  priv->task_func (task, priv->user_data, priv->cancellable);

  /* Удаляем задачу из очереди. */
  g_mutex_lock (&priv->queue_lock);
  priv->processing_tasks = g_list_remove (priv->processing_tasks, task);
  g_atomic_int_add (&priv->processing_count, -1);
  g_mutex_unlock (&priv->queue_lock);

  /* Освобождаем память от задачи. */
  if (priv->task_free_func != NULL)
    priv->task_free_func (task);

  /* Пробуем обработать следующую задачу. */
  hyscan_task_queue_try_next (queue);
}

/* Пробует отправить на обработку следующую задачу. */
static void
hyscan_task_queue_try_next (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv = queue->priv;
  GError *error = NULL;
  gpointer task;

  /* Объект находится в стадии завершения, больше не добавляем задачи. */
  if (g_atomic_int_get (&priv->shutdown))
    return;

  /* Не обрабатываем одновременно более n задач. */
  if (g_atomic_int_get (&priv->processing_count) > (gint) priv->max_concurrent)
    return;

  /* Блокируем другим потокам доступ к очереди задач. */
  g_mutex_lock (&priv->queue_lock);

  /* Добавляем задачу task из начала очереди в обработку. */
  task = g_queue_pop_head (priv->queue);
  if (task != NULL)
    {
      g_thread_pool_push (priv->pool, task, &error);
      priv->processing_tasks = g_list_append (priv->processing_tasks, task);
      g_atomic_int_inc (&priv->processing_count);
      if (error != NULL)
        {
          g_warning ("HyScanTaskQueue: %s", error->message);
          g_clear_error (&error);
        }
    }

  /* Операции с очередью сделаны — разблокируем доступ. */
  g_mutex_unlock (&priv->queue_lock);
}

/**
 * hyscan_task_queue_push:
 * @task_func:
 * @cmp_func:
 *
 * Создает новый объект #HyScanTaskQueue.
 *
 * Returns: Перед удалением необходимо завершить работу очереди с помощью
 *    hyscan_task_queue_shutdown() и затем удалить через g_object_unref().
 */
HyScanTaskQueue *
hyscan_task_queue_new (HyScanTaskQueueFunc task_func,
                       gpointer            user_data,
                       GDestroyNotify      free_func,
                       GCompareFunc        cmp_func)
{
  return g_object_new (HYSCAN_TYPE_TASK_QUEUE,
                       "task-func", task_func,
                       "task-data", user_data,
                       "free-func", free_func,
                       "cmp-func", cmp_func,
                       NULL);
}

/**
 * hyscan_task_queue_push:
 * \param queue
 * \param task
 *
 * Добавляет задачу в очередь.
 */
void
hyscan_task_queue_push (HyScanTaskQueue *queue,
                        gpointer         task)
{
  HyScanTaskQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));

  priv = queue->priv;

  /* Блокируем другим потокам доступ к очереди задач. */
  g_mutex_lock (&priv->queue_lock);

  /* Проверяем, есть ли уже такая задача в очереди или в обработке. */
  if (!g_queue_find_custom (priv->queue, task, priv->cmp_func) &&
      !g_list_find_custom (priv->processing_tasks, task, priv->cmp_func))

    /* Если задачи ещё нет в очереди, то добавляем её. */
    {
      g_queue_push_tail (priv->queue, task);
    }
  else

    /* Если такая же задача уже есть, то удаляем её. */
    {
      if (priv->task_free_func)
        priv->task_free_func (task);
    }

  /* Операции с очередью сделаны — разблокируем доступ. */
  g_mutex_unlock (&priv->queue_lock);

  hyscan_task_queue_try_next (queue);
}

/**
 * hyscan_task_queue_clear:
 * @queue
 *
 * Очищает очередь задач. Те задачи, которые уже поступили в обработку, будут
 * всё равно обработаны.
 */
void
hyscan_task_queue_clear (HyScanTaskQueue  *queue)
{
  HyScanTaskQueuePrivate *priv;
  gpointer task;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));

  priv = queue->priv;

  /* Блокируем другим потокам доступ к очереди задач. */
  g_mutex_lock (&priv->queue_lock);

  /* Удаляем все задачи. */
  while ((task = g_queue_pop_tail (priv->queue)) != NULL)
    {
      if (priv->task_free_func)
        priv->task_free_func (task);
    }

  /* Операции с очередью сделаны — разблокируем доступ. */
  g_mutex_unlock (&priv->queue_lock);
}

/**
 * hyscan_task_queue_shutdown:
 * @queue
 *
 * Останавливает выполнение задач в очереди. Должна быть вызвана перед
 * удаление объекта g_object_unref().
 */
void
hyscan_task_queue_shutdown (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));
  priv = queue->priv;

  /* Ставим флаг о завершении работы. */
  g_atomic_int_set (&priv->shutdown, TRUE);

  /* Отменяем все задачи. */
  g_cancellable_cancel (priv->cancellable);
}
