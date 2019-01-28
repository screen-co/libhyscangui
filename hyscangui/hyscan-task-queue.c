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
  gint                         prop_a;
  GFunc           task_func;            /* Функция выполнения задачи. */
  GCompareFunc    cmp_func;             /* Функция сравнения двух задач. */
  GDestroyNotify  task_free_func;       /* Функция удаления задачи. */

  GQueue         *queue;                /* Очередь задач. */
  GMutex          queue_lock;           /* Мутекс для ограничения доступа к редактиронию очереди задач. */
  GThreadPool    *pool;                 /* Пул обработки задач. */
  gpointer        user_data;            /* Пользовательские данные для пула задач. */
};

static void    hyscan_task_queue_set_property             (GObject               *object,
                                                           guint                  prop_id,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void    hyscan_task_queue_object_constructed       (GObject               *object);
static void    hyscan_task_queue_object_finalize          (GObject               *object);
static void    hyscan_task_queue_process                  (gpointer               task,
                                                           HyScanTaskQueue       *queue);

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
  priv->pool = g_thread_pool_new ((GFunc) hyscan_task_queue_process, task_queue,
                                  3, FALSE, NULL);
  priv->queue = g_queue_new ();
}

static void
hyscan_task_queue_object_finalize (GObject *object)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  /* Освобождаем память. */
  g_queue_free (priv->queue);
  g_thread_pool_free (priv->pool, TRUE, FALSE);

  G_OBJECT_CLASS (hyscan_task_queue_parent_class)->finalize (object);
}

/* Обработка задачи из очереди. */
static void
hyscan_task_queue_process (gpointer         task,
                           HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv = queue->priv;

  /* Выполняем задачу. */
  priv->task_func (task, priv->user_data);

  /* Удаляем задачу из очереди. */
  g_mutex_lock (&priv->queue_lock);
  g_queue_remove (priv->queue, task);
  g_message ("Task done. Rest queue length: %d", g_queue_get_length (priv->queue));
  g_mutex_unlock (&priv->queue_lock);

  /* Освобождаем память от задачи. */
  if (priv->task_free_func != NULL)
    priv->task_free_func (task);
}

/**
 * hyscan_task_queue_push:
 * @task_func:
 * @cmp_func:
 *
 * Создает новый объект #HyScanTaskQueue.
 *
 * Returns: Для удаления g_object_unref().
 */
HyScanTaskQueue *
hyscan_task_queue_new (GFunc          task_func,
                       gpointer       user_data,
                       GDestroyNotify free_func,
                       GCompareFunc   cmp_func)
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
  GError *error = NULL;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));

  priv = queue->priv;

  /* Блокируем другим потокам доступ к очереди задач. */
  g_mutex_lock (&priv->queue_lock);

  /* todo: add timeout. */

  /* Если задача ещё нет в очереди, то добавляем её. */
  if (!g_queue_find_custom (priv->queue, task, priv->cmp_func))
    {
      g_queue_push_tail (priv->queue, task);
      g_thread_pool_push (priv->pool, task, &error);
      if (error != NULL)
        g_warning ("HyScanTaskQueue: %s", error->message);
    }

  /* Операции с очередью сделаны — разблокируем доступ. */
  g_mutex_unlock (&priv->queue_lock);

}