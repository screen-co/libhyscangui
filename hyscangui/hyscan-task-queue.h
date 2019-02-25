#ifndef __HYSCAN_TASK_QUEUE_H__
#define __HYSCAN_TASK_QUEUE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TASK_QUEUE             (hyscan_task_queue_get_type ())
#define HYSCAN_TASK_QUEUE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TASK_QUEUE, HyScanTaskQueue))
#define HYSCAN_IS_TASK_QUEUE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TASK_QUEUE))
#define HYSCAN_TASK_QUEUE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TASK_QUEUE, HyScanTaskQueueClass))
#define HYSCAN_IS_TASK_QUEUE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TASK_QUEUE))
#define HYSCAN_TASK_QUEUE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TASK_QUEUE, HyScanTaskQueueClass))

typedef struct _HyScanTaskQueue HyScanTaskQueue;
typedef struct _HyScanTaskQueuePrivate HyScanTaskQueuePrivate;
typedef struct _HyScanTaskQueueClass HyScanTaskQueueClass;

struct _HyScanTaskQueue
{
  GObject parent_instance;

  HyScanTaskQueuePrivate *priv;
};

struct _HyScanTaskQueueClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_task_queue_get_type         (void);

HYSCAN_API
HyScanTaskQueue *      hyscan_task_queue_new              (GFunc             task_func,
                                                           gpointer          user_data,
                                                           GDestroyNotify    free_func,
                                                           GCompareFunc      cmp_func);

HYSCAN_API
void                   hyscan_task_queue_push             (HyScanTaskQueue  *queue,
                                                           gpointer          task);

HYSCAN_API
void                   hyscan_task_queue_clear            (HyScanTaskQueue  *queue);

G_END_DECLS

#endif /* __HYSCAN_TASK_QUEUE_H__ */
