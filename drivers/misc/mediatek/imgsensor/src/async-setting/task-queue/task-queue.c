#include "task-queue.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>


#define DEFAULT_QUEUE_SIZE 128

static int do_task(void *data)
{
	task_queue_t *tq = (task_queue_t *)data;
	task_t* t = NULL;

	while(!kthread_should_stop()) {
		wait_event_interruptible(tq->wq, !queue_is_empty(tq) || kthread_should_stop());

		while (1) {
			t = queue_pop_head(tq);
			if (t) {
				if (t->run) {
					mutex_lock(&tq->queue_mutex);
					t->run(t->param);
					mutex_unlock(&tq->queue_mutex);
				}
			} else {
				break;
			}
		}
	}

	pr_info("exit task-queue thread!\n");
	return 0;
}

task_queue_t* queue_create(char *name)
{
	task_queue_t *tq = kmalloc(sizeof(task_queue_t), GFP_KERNEL);
	if (!tq) {
		pr_err("Failed to kmalloc task_queue_t : %s\n", name);
		return NULL;
	}

	tq->tasks = kmalloc(DEFAULT_QUEUE_SIZE * sizeof(task_t), GFP_KERNEL);
	if (!tq->tasks) {
		pr_err("Failed to kmalloc task_t : %s\n", name);
		kfree(tq);
		return NULL;
	}

	tq->capcity = DEFAULT_QUEUE_SIZE;
	tq->head    = tq->tail = tq->size = 0;
	mutex_init(&tq->mutex);
	mutex_init(&tq->queue_mutex);
	init_waitqueue_head(&tq->wq);

	tq->task_thread = kthread_run(do_task, tq, "%s-task-queue", name);
	if (!tq->task_thread) {
		pr_err("Failed to create kthread : %s-task-queue\n", name);
		kfree(tq->tasks);
		kfree(tq);
		return NULL;
	}

	return tq;
}

void queue_free(task_queue_t* tq)
{
	if (tq)	{
		if (tq->task_thread) {
			kthread_stop(tq->task_thread);
		}

		if (tq->tasks) {
			kfree(tq->tasks);
		}

		kfree(tq);
	}
}

bool queue_is_full(task_queue_t* tq)
{
	return tq->size == tq->capcity;
}

bool queue_is_empty(task_queue_t* tq)
{
	return tq->size == 0;
}

bool queue_push_tail(task_queue_t* tq, task_t* t)
{
	bool ret = false;

	if (!tq || !t) {
		return false;
	}

	mutex_lock(&tq->mutex);
	if (!queue_is_full(tq)) {
		tq->tasks[tq->tail].run = t->run;
		tq->tasks[tq->tail].param = t->param;

		tq->tail = (tq->tail + 1) % (tq->capcity);
		tq->size++;
		ret = true;
	}
	mutex_unlock(&tq->mutex);

	wake_up_interruptible(&tq->wq);

	return ret;
}

task_t* queue_pop_head(task_queue_t* tq)
{
	task_t* t = NULL;

	if (!tq) {
		return NULL;
	}

	mutex_lock(&tq->mutex);
	if (!queue_is_empty(tq)) {
		t = &(tq->tasks[tq->head]);
		tq->head = (tq->head + 1) % (tq->capcity);
		tq->size--;
	}
	mutex_unlock(&tq->mutex);

	return t;
}

void queue_lock(task_queue_t* tq)
{
	if (!tq) {
		return;
	}

	mutex_lock(&tq->queue_mutex);
}

void queue_unlock(task_queue_t* tq)
{
	if (!tq) {
		return;
	}

	mutex_unlock(&tq->queue_mutex);
}

