/* hyscan-task-queue.h
 *
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

#ifndef __HYSCAN_TASK_QUEUE_H__
#define __HYSCAN_TASK_QUEUE_H__

#include <hyscan-api.h>
#include <gio/gio.h>

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

/**
 * HyScanTaskQueueFunc:
 * @task: указатель на объект задачи
 * @user_data: пользовательские данные
 * @cancellable: объект для отмены задачи
 *
 * Функция обработки задачи.
 */
typedef void (*HyScanTaskQueueFunc) (GObject      *task,
                                     gpointer      user_data,
                                     GCancellable *cancellable);

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
HyScanTaskQueue *      hyscan_task_queue_new              (HyScanTaskQueueFunc   task_func,
                                                           gpointer              user_data,
                                                           GCompareFunc          cmp_func);

HYSCAN_API
void                   hyscan_task_queue_push             (HyScanTaskQueue      *queue,
                                                           GObject              *task);

HYSCAN_API
void                   hyscan_task_queue_push_end         (HyScanTaskQueue      *queue);

HYSCAN_API
void                   hyscan_task_queue_shutdown         (HyScanTaskQueue      *queue);

G_END_DECLS

#endif /* __HYSCAN_TASK_QUEUE_H__ */
