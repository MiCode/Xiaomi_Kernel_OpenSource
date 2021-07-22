// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/alarmtimer.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include "aoltest_ring_buffer.h"

#define RB_LATEST(prb) ((prb)->write - 1)
#define RB_SIZE(prb) ((prb)->size)
#define RB_MASK(prb) (RB_SIZE(prb) - 1)
#define RB_COUNT(prb) ((prb)->write - (prb)->read)
#define RB_FULL(prb) (RB_COUNT(prb) >= RB_SIZE(prb))
#define RB_EMPTY(prb) ((prb)->write == (prb)->read)

#define RB_INIT(prb, qsize) \
do { \
	(prb)->read = (prb)->write = 0; \
	(prb)->size = (qsize); \
} while (0)

#define RB_PUT(prb, value) \
do { \
	if (!RB_FULL(prb)) { \
		(prb)->queue[(prb)->write & RB_MASK(prb)] = value; \
		++((prb)->write); \
	} \
	else { \
		pr_info("RB FULL!!"); \
	} \
} while (0)

#define RB_GET(prb, value) \
do { \
	if (!RB_EMPTY(prb)) { \
		value = (prb)->queue[(prb)->read & RB_MASK(prb)]; \
		++((prb)->read); \
		if (RB_EMPTY(prb)) { \
			(prb)->read = (prb)->write = 0; \
		} \
	} \
	else { \
		value = NULL; \
		pr_info("RB EMPTY!!"); \
	} \
} while (0)

#define RB_GET_LATEST(prb, value) \
do { \
	if (!RB_EMPTY(prb)) { \
		value = (prb)->queue[RB_LATEST(prb) & RB_MASK(prb)]; \
		if (RB_EMPTY(prb)) { \
			(prb)->read = (prb)->write = 0; \
		} \
	} \
	else { \
		value = NULL; \
	} \
} while (0)


/*********************************************/
/* Global */
/*********************************************/
struct aoltest_rb_data *aoltest_core_rb_pop_free(struct aoltest_core_rb *rb)
{
	unsigned long flags;
	struct aoltest_rb_data *data = NULL;

	spin_lock_irqsave(&rb->freeQ.lock, flags);
	RB_GET(&rb->freeQ, data);
	pr_info("[%s] r=[%d] w=[%d]", __func__, rb->freeQ.read, rb->freeQ.write);
	spin_unlock_irqrestore(&rb->freeQ.lock, flags);

	return data;
}

void aoltest_core_rb_push_free(struct aoltest_core_rb *rb, struct aoltest_rb_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&rb->freeQ.lock, flags);
	if (!RB_FULL(&rb->freeQ))
		RB_PUT(&rb->freeQ, data);
	else
		pr_info("[%s] freeQ RB full ", __func__);

	pr_info("[%s] r=[%d] w=[%d]", __func__, rb->freeQ.read, rb->freeQ.write);
	spin_unlock_irqrestore(&rb->freeQ.lock, flags);
}

void aoltest_core_rb_push_active(struct aoltest_core_rb *rb, struct aoltest_rb_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&rb->activeQ.lock, flags);
	if (!RB_FULL(&rb->activeQ))
		RB_PUT(&rb->activeQ, data);
	else
		pr_info("[%s] freeQ RB full", __func__);

	pr_info("[%s] r=[%d] w=[%d]", __func__, rb->activeQ.read, rb->activeQ.write);
	spin_unlock_irqrestore(&rb->activeQ.lock, flags);
}

struct aoltest_rb_data *aoltest_core_rb_pop_active(struct aoltest_core_rb *rb)
{
	unsigned long flags;
	struct aoltest_rb_data *data = NULL;

	spin_lock_irqsave(&rb->activeQ.lock, flags);
	RB_GET(&rb->activeQ, data);
	pr_info("[%s] r=[%d] w=[%d]", __func__, rb->activeQ.read, rb->activeQ.write);
	spin_unlock_irqrestore(&rb->activeQ.lock, flags);

	return data;
}

int aoltest_core_rb_has_pending_data(struct aoltest_core_rb *rb)
{
	unsigned long flags;
	int cnt;

	spin_lock_irqsave(&rb->activeQ.lock, flags);
	cnt = RB_COUNT(&rb->activeQ);
	spin_unlock_irqrestore(&rb->activeQ.lock, flags);
	return cnt;
}

int aoltest_core_rb_init(struct aoltest_core_rb *rb)
{
	int i;

	spin_lock_init(&rb->freeQ.lock);
	spin_lock_init(&rb->activeQ.lock);
	spin_lock_init(&rb->lock);

	RB_INIT(&rb->freeQ, CORE_OP_SZ);
	RB_INIT(&rb->activeQ, CORE_OP_SZ);

	for (i = 0; i < CORE_OP_SZ; i++) {
		init_completion(&rb->queue[i].comp);
		aoltest_core_rb_push_free(rb, &(rb->queue[i]));
	}

	return 0;
}

int aoltest_core_rb_deinit(struct aoltest_core_rb *rb)
{
	return 0;
}

