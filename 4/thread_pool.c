#include "thread_pool.h"
#include <pthread.h>

#include <stdlib.h>
// temp
#include <stdio.h>

struct thread_task {
	thread_task_f function;
	void *arg;

	/* PUT HERE OTHER MEMBERS */
   struct thread_task *next;
   struct thread_task *prev;
   bool is_pushed;
   bool is_running;
   bool is_finished;
};

struct thread_pool {
	pthread_t *threads; // individual_stacks

	/* PUT HERE OTHER MEMBERS */
   int max_threads_count;
   int active_threads;
   struct thread_task *first_task;
   struct thread_task *last_task;
   int tasks_count;
};

/* Creates task which will borrow tasks from queue */
// int
// wrapper_task(struct thread_pool *pool)
// {
//    return 0;
// }

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
      return TPOOL_ERR_INVALID_ARGUMENT;  
   *pool = malloc(sizeof(struct thread_pool*));
   (*pool)->threads = malloc(sizeof(pthread_t) * max_thread_count);
   (*pool)->active_threads = 0;
   (*pool)->tasks_count = 0;
   (*pool)->max_threads_count = max_thread_count;
   (*pool)->first_task = NULL;
   (*pool)->last_task = NULL;
   return 0; 
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
   return pool->active_threads;
}

int
thread_pool_delete(struct thread_pool *pool)
{
   if (pool->tasks_count)
      return TPOOL_ERR_HAS_TASKS;
   // free(pool->threads);
   free(pool);
   return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
   if (pool->tasks_count == TPOOL_MAX_TASKS)
      return TPOOL_ERR_TOO_MANY_TASKS;
   task->is_pushed = true;
   if (pool->first_task != NULL)
      pool->last_task->next = task;
   else
      pool->first_task = task;
   pool->last_task = task;
   return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = malloc(sizeof(struct thread_task*));
   (*task)->function = function;
   (*task)->arg = arg;
   (*task)->next = NULL;
   (*task)->is_pushed = false;
   (*task)->is_running = false;
   (*task)->is_finished = false;
   return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return task->is_finished;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return task->is_running;
}

int
thread_task_join(struct thread_task *task, void **result)
{
   if (!task->is_pushed)
      return TPOOL_ERR_TASK_NOT_PUSHED;
   // pthread_cond_t
   (void)result;
   return 0;
}

#if NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	if (task->is_pushed)
      return TPOOL_ERR_TASK_IN_POOL;
   free(task);
   return 0;
}

#if NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
