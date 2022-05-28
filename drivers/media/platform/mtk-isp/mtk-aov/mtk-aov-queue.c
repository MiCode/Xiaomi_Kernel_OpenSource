// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mm.h>
#include <linux/kernel.h>

#include "mtk-aov-queue.h"

#if defined(CONFIG_HAVE_CMPXCHG_DOUBLE)

/*
 * A practical nonblocking queue algorithm using compare-and-swap
 */

#define NODE(val) ((struct queue_node *)&val)

int32_t queue_init(struct queue *queue)
{
	if (queue == NULL)
		return -1;

	memset(&(queue->node[0]), 0x0, sizeof(queue->node));
	atomic_set(&(queue->front), 0);
	atomic_set(&(queue->rear), 0);
	atomic_set(&(queue->size), 0);
	queue->init = true;

	return 0;
}

int32_t queue_push(struct queue *queue, void *data)
{
	struct queue_node old;
	struct queue_node new;
	struct queue_node *orig;
	uint32_t rear;
	uint32_t front;

	while (queue->init) {
		rear = atomic_read(&(queue->rear));
		old  = *(NODE(queue->node[rear % QUEUE_MAX_SIZE]));

		front = atomic_read(&(queue->front));
		if (rear != atomic_read(&(queue->rear)))
			continue;

		if (rear == (atomic_read(&(queue->front)) + QUEUE_MAX_SIZE))  {
			if ((NODE(queue->node[front % QUEUE_MAX_SIZE])->ptr) &&
				(front == atomic_read(&(queue->front))))
				return -1;

			atomic_cmpxchg(&(queue->front), front, front + 1);
			continue;
		}

		if (!old.ptr) {
			new.ptr = (uintptr_t)data;
			new.ref = old.ref + 1;

			orig = NODE(queue->node[rear % QUEUE_MAX_SIZE]);

			if (cmpxchg_double(&orig->ptr,
				&orig->ref, old.ptr, old.ref, new.ptr, new.ref)) {
				atomic_cmpxchg(&(queue->rear), rear, rear + 1);
				return atomic_inc_return(&(queue->size));
			}
		} else if (NODE(queue->node[rear % QUEUE_MAX_SIZE])->ptr) {
			atomic_cmpxchg(&(queue->rear), rear, rear + 1);
		}
	}

	return 0;
}

void *queue_pop(struct queue *queue)
{
	struct queue_node old;
	struct queue_node new;
	struct queue_node *orig;
	uint32_t rear;
	uint32_t front;

	while (queue->init) {
		front = atomic_read(&(queue->front));
		old = *(NODE(queue->node[front % QUEUE_MAX_SIZE]));
		rear = atomic_read(&(queue->rear));

		if (front != atomic_read(&(queue->front)))
			continue;

		if (front == atomic_read(&(queue->rear))) {
			if (!(NODE(queue->node[rear % QUEUE_MAX_SIZE])->ptr) &&
				(rear == atomic_read(&(queue->rear))))
				return NULL;

			atomic_cmpxchg(&(queue->rear), rear, rear + 1);
			continue;
		}

		if (old.ptr) {
			new.ptr = 0;
			new.ref = old.ref + 1;

			orig = NODE(queue->node[front % QUEUE_MAX_SIZE]);
			if (cmpxchg_double(&(orig->ptr),
					&(orig->ref), old.ptr, old.ref, new.ptr, new.ref)) {
				atomic_cmpxchg(&(queue->front), front, front + 1);
				atomic_dec(&(queue->size));
				return (void *)(NODE(old)->ptr);
			}
		} else if (!(NODE(queue->node[front % QUEUE_MAX_SIZE])->ptr)) {
			atomic_cmpxchg(&(queue->front), front, front + 1);
		}
	}

	return NULL;
}

int32_t queue_size(struct queue *queue)
{
	return atomic_read(&(queue->size));
}

bool queue_empty(struct queue *queue)
{
	return (atomic_read(&(queue->size)) <= 0);
}

int32_t queue_deinit(struct queue *queue)
{
	queue->init = false;
	return 0;
}

#else

int32_t queue_init(struct queue *queue)
{
	queue->front = 0;
	queue->rear  = 0;
	queue->size  = 0;
	queue->init  = true;
	spin_lock_init(&queue->lock);

	return 0;
}

int32_t queue_push(struct queue *queue, void *data)
{
	unsigned long flag;

	spin_lock_irqsave(&queue->lock, flag);

	if (queue->init == false) {
		spin_unlock_irqrestore(&queue->lock, flag);
		return -1;
	}

	if (queue->size >= QUEUE_MAX_SIZE) {
		spin_unlock_irqrestore(&queue->lock, flag);
		return -1;

	queue->data[queue->rear] = (uintptr_t)data;
	queue->rear = (queue->rear + 1) % QUEUE_MAX_SIZE;
	queue->size++;

	spin_unlock_irqrestore(&queue->lock, flag);

	return 0;
}

void *queue_pop(struct queue *queue)
{
	unsigned long flag;
	void *data;

	spin_lock_irqsave(&queue->lock, flag);

	if ((queue == NULL) || (false == queue->init)) {
		spin_unlock_irqrestore(&queue->lock, flag);
		return NULL;
	}

	if (queue->size == 0) {
		spin_unlock_irqrestore(&queue->lock, flag);
		return NULL;

	data = (void *)queue->data[queue->front];
	queue->front = (queue->front + 1) % QUEUE_MAX_SIZE;
	queue->size--;

	spin_unlock_irqrestore(&queue->lock, flag);

	return data;
}

bool queue_empty(struct queue *queue)
{
	unsigned long flag;
	bool empty;

	spin_lock_irqsave(&queue->lock, flag);
	empty = (queue->init == false) || (queue->size == 0);
	spin_unlock_irqrestore(&queue->lock, flag);

	return empty;
}

int32_t queue_deinit(struct queue *queue)
{
	queue->init = false;
	return 0;
}

#endif  // CONFIG_HAVE_CMPXCHG_DOUBLE
