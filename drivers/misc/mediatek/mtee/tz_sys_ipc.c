/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/types.h>

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include "tz_cross/trustzone.h"
#include "tz_cross/ree_service.h"
#include "trustzone/kree/system.h"

/* Mutex
 */
int KREE_ServMutexCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	u64 *out;

	mutex = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (mutex == NULL)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	mutex_init(mutex);

	out = (u64 *) &param[0];
	*out = (u64)(unsigned long)mutex;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServMutexDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	mutex = (struct mutex *)*in;

	kfree(mutex);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServMutexLock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	mutex = (struct mutex *)*in;

	mutex_lock(mutex);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServMutexUnlock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	mutex = (struct mutex *)*in;

	mutex_unlock(mutex);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServMutexTrylock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	unsigned long *in;
	int *out;
	int ret;

	in = (unsigned long *) &param[0];
	mutex = (struct mutex *)*in;

	ret = mutex_trylock(mutex);

	out = (int *)&param[0];
	*out = ret;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServMutexIslock(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct mutex *mutex;
	unsigned long *in;
	int *out;
	int ret;

	in = (unsigned long *) &param[0];
	mutex = (struct mutex *)*in;

	ret = mutex_is_locked(mutex);

	out = (int *)&param[0];
	*out = ret;

	return TZ_RESULT_SUCCESS;
}


/* Semaphore
 */
int KREE_ServSemaphoreCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	u64 *out;
	int *val;

	val = (int *)&param[0];

	sema = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	if (sema == NULL)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	sema_init(sema, *val);

	out = (u64 *) &param[0];
	*out = (u64)(unsigned long)sema;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	sema = (struct semaphore *)*in;

	kfree(sema);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreDown(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	sema = (struct semaphore *)*in;

	down(sema);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreDownInterruptible(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	unsigned long *in;
	int *out;

	in = (unsigned long *)&param[0];
	sema = (struct semaphore *)*in;
	out = (int *)&param[0];

	*out = down_interruptible(sema);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreDownTimeout(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	u64 *in;
	long jiffies;
	int *out;
	int ret;

	in = (u64 *) &param[0];
	sema = (struct semaphore *)(unsigned long)in[0];
	jiffies = (long)in[1];

	ret = down_timeout(sema, jiffies);

	out = (int *)&param[0];
	*out = ret;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreDowntrylock(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	unsigned long *in;
	int *out;
	int ret;

	in = (unsigned long *) &param[0];
	sema = (struct semaphore *)*in;

	ret = down_trylock(sema);

	out = (int *)&param[0];
	*out = ret;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServSemaphoreUp(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	struct semaphore *sema;
	unsigned long *in;

	in = (unsigned long *) &param[0];
	sema = (struct semaphore *)*in;

	up(sema);

	return TZ_RESULT_SUCCESS;
}

#if 0
/* wait queue
 */
int KREE_ServWaitqCreate(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	wait_queue_head_t *q;
	u32 *out;

	q = kmalloc(sizeof(wait_queue_head_t), GFP_KERNEL);
	if (q == NULL)
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;

	init_waitqueue_head(q);

	out = (u32 *) &param[0];
	*out = (u32) q;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServWaitqDestroy(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	wait_queue_head_t *q;
	u32 *in;

	in = (u32 *) &param[0];
	q = (wait_queue_head_t *) *in;

	kfree(q);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServWaitqWaitevent(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	wait_queue_head_t *q;
	u32 *in;
	u32 condition;

	in = (u32 *) &param[0];
	q = (wait_queue_head_t *) in[0];
	condition = in[1];

	wait_event(*q, condition);

	return TZ_RESULT_SUCCESS;
}

int KREE_ServWaitqWaiteventTimeout(u32 op,
					u8 param[REE_SERVICE_BUFFER_SIZE])
{
	wait_queue_head_t *q;
	u32 *in;
	u32 condition;
	long timeout;
	int *out;
	int ret;

	in = (u32 *) &param[0];
	q = (wait_queue_head_t *) in[0];
	condition = in[1];
	timeout = (long)in[2];

	ret = wait_event_timeout(*q, condition, timeout);

	out = (int *)&param[0];
	*out = ret;

	return TZ_RESULT_SUCCESS;
}

int KREE_ServWaitqWakeup(u32 op, u8 param[REE_SERVICE_BUFFER_SIZE])
{
	wait_queue_head_t *q;
	u32 *in;

	in = (u32 *) &param[0];
	q = (wait_queue_head_t *) in[0];

	wake_up(q);

	return TZ_RESULT_SUCCESS;
}
#endif
