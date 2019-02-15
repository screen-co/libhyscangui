#include <hyscan-task-queue.h>

static gchar *test_user_data = "user data";
static guint  created_tasks = 0;

void
task_func (gpointer task,
           gpointer user_data)
{
  g_assert_true (test_user_data == user_data);
  g_message ("Processing task %s", task);
  // g_usleep (G_USEC_PER_SEC / 1000);
}

void
del_task (gpointer task)
{
  g_atomic_int_add (&created_tasks, -1);

  g_free (task);
}

gpointer
new_task (gint i)
{
  g_atomic_int_add (&created_tasks, 1);

  return g_strdup_printf ("task %d", i);
}

int
main (int    argc,
      char **argv)
{
  HyScanTaskQueue *queue;
  gint i;
  gint n = 1000;

  queue = hyscan_task_queue_new (task_func, test_user_data, del_task, (GCompareFunc) g_strcmp0);

  for (i = 0; i < n; ++i)
    hyscan_task_queue_push (queue, new_task (i));

  g_usleep (G_USEC_PER_SEC);
  g_object_unref (queue);

  g_assert_cmpint (g_atomic_int_get (&created_tasks), ==, 0);

  return 0;
}
