#ifndef __TASK_QUEUE_H__
#define __TASK_QUEUE_H__

#include "adaptor-subdrv.h"
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>

typedef struct task
{
	void (*run)(void *);
	void* param;
}task_t;

typedef struct task_queue
{
	int head;
	int tail;
	int size;
	int capcity;
	task_t*  tasks;
	struct mutex mutex;
	struct mutex queue_mutex;
	wait_queue_head_t wq;
	struct task_struct * task_thread;
}task_queue_t;

task_queue_t *queue_create(char *name);

bool queue_is_full(task_queue_t* tq);

bool queue_is_empty(task_queue_t* tq);

bool queue_push_tail(task_queue_t* tq, task_t* t);

task_t* queue_pop_head(task_queue_t* tq);

void queue_free(task_queue_t* tq);

void queue_lock(task_queue_t* tq);

void queue_unlock(task_queue_t* tq);

#endif
