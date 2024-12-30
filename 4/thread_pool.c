#define _GNU_SOURCE
#include <sched.h>
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
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   void *result;
};

struct thread_pool {
	pthread_t *threads; // individual_stacks

	/* PUT HERE OTHER MEMBERS */
   int max_threads_count;
   int active_threads;
   struct thread_task *first_task;
   struct thread_task *last_task;
   int tasks_count;
   pthread_mutex_t task_mutex;
};

/* Creates a task which will borrow other tasks from queue */
void *
init_pool_task(void *args)
{
   printf("started init_pool_task function\n");
   struct thread_pool *pool = args;
   pthread_mutex_lock(&pool->task_mutex);
   pool->active_threads++;
   while (pool->tasks_count > 0)
   {
      pool->tasks_count--;
      if (pool->tasks_count > 0 && pool->active_threads + 1 <= pool->max_threads_count)
         pthread_create(&pool->threads[pool->active_threads], NULL, &init_pool_task, args);
      struct thread_task *curr_task = pool->first_task;
      pthread_mutex_lock(&curr_task->mutex);
      curr_task->result = curr_task->function(curr_task->arg);
      printf("executed function in thread\n");
      pthread_cond_signal(&curr_task->cond);
      pthread_mutex_unlock(&curr_task->mutex);
      pool->first_task = pool->first_task->next;
      if (pool->first_task == NULL)
         pool->last_task = NULL;
      // maybe add another mutex in order to remove the following gap so that
      // thread won't monopolize the processing time
      pthread_mutex_unlock(&pool->task_mutex);

      pthread_mutex_lock(&pool->task_mutex);
   }
   pool->active_threads--;
   pthread_mutex_unlock(&pool->task_mutex);
   return 0;
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
      return TPOOL_ERR_INVALID_ARGUMENT;  
   
   *pool = malloc(sizeof(struct thread_pool));
   (*pool)->threads = malloc(sizeof(pthread_t) * max_thread_count);
   (*pool)->active_threads = 0;
   (*pool)->tasks_count = 0;
   (*pool)->max_threads_count = max_thread_count;
   (*pool)->first_task = NULL;
   (*pool)->last_task = NULL;
   pthread_mutex_init(&(*pool)->task_mutex, NULL);
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
   free(pool->first_task);
   free(pool->last_task);
   free(pool->threads);
   free(pool);
   return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
   printf("started thread_pool_push_task function\n");
   pthread_mutex_lock(&pool->task_mutex);
   if (pool->tasks_count == TPOOL_MAX_TASKS)
      return TPOOL_ERR_TOO_MANY_TASKS;
   pthread_mutex_lock(&task->mutex);
   printf("gained task mutex\n");
   task->is_pushed = true;
   pthread_mutex_unlock(&task->mutex);
   if (pool->first_task != NULL)
      pool->last_task->next = task;
   else
      pool->first_task = task;
   pool->last_task = task;
   if (!pool->active_threads)
   {
      pthread_mutex_unlock(&pool->task_mutex);
      printf("before pthread creation\n");
      pthread_create(&pool->threads[pool->active_threads], NULL, &init_pool_task, &pool);
   } else
      pthread_mutex_unlock(&pool->task_mutex);
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
   (*task)->result = NULL;
   pthread_cond_init(&(*task)->cond, NULL);
   pthread_mutex_init(&(*task)->mutex, NULL);
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
   printf("started thread_task_join function\n");
   pthread_mutex_lock(&task->mutex);
   if (!task->is_pushed)
   {
      pthread_mutex_unlock(&task->mutex);
      return TPOOL_ERR_TASK_NOT_PUSHED;
   }
   while (!task->is_finished)
      pthread_cond_wait(&task->cond, &task->mutex);
   *result = task->result;
   pthread_mutex_unlock(&task->mutex);
   printf("unlocked task mutex in task_join\n");
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
