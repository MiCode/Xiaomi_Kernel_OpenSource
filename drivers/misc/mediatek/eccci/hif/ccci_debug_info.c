// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/sched/clock.h>

#include "ccci_debug.h"
#include "ccci_debug_info.h"
#include "ccci_hif_ccif.h"

#define TAG "deb"

#define RECV_DATA_SIZE 5000



struct recv_time {
	u64 recv_time;
	u8 qno;

};

struct queue_recv_info {
	u64 first_time;
	u64 last_time;
	int count;

};

struct total_recv_info {
	int ring_Read;
	int ring_Write;
	int repeat_count;
	u64 pre_time;
	struct recv_time times[RECV_DATA_SIZE];

};

static struct total_recv_info s_total_info;
static spinlock_t s_recv_data_info_lock;


static inline void calc_irq_info_per_q(
		struct queue_recv_info *q_inofs, int s, int e)
{
	int qno;

	while (s <= e) {
		qno = s_total_info.times[s].qno;
		q_inofs[qno].count++;

		if (!q_inofs[qno].first_time) {
			q_inofs[qno].first_time =
					s_total_info.times[s].recv_time;
			q_inofs[qno].last_time =
					s_total_info.times[s].recv_time;
			s++;
			continue;
		}

		if (s_total_info.times[s].recv_time <
				q_inofs[qno].first_time)
			q_inofs[qno].first_time =
					s_total_info.times[s].recv_time;
		else if (s_total_info.times[s].recv_time >
				q_inofs[qno].last_time)
			q_inofs[qno].last_time =
					s_total_info.times[s].recv_time;

		s++;
	}
}

static inline void ccif_debug_print_irq_info(void)
{
	struct queue_recv_info q_inofs[CCIF_CH_NUM] = {0};
	int i;

	calc_irq_info_per_q(&q_inofs[0], 0,	RECV_DATA_SIZE - 1);

	for (i = 0; i < CCIF_CH_NUM; i++)
		if (q_inofs[i].count)
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] qno=%d; cou=%d; ft=%lld; lt=%lld\n",
				__func__, i, q_inofs[i].count,
				q_inofs[i].first_time, q_inofs[i].last_time);

	/* clear irq info, re-start save irq info */
	s_total_info.ring_Read = 0;
	s_total_info.ring_Write = 0;
	s_total_info.repeat_count = 0;
	s_total_info.pre_time = 0;
}

void ccif_debug_save_irq(u8 qno, u64 cur_time)
{
	unsigned long flags;
	int last_write;

	spin_lock_irqsave(&s_recv_data_info_lock, flags);

	if (s_total_info.pre_time == cur_time)
		s_total_info.repeat_count++;
	else
		s_total_info.pre_time = cur_time;

	s_total_info.times[s_total_info.ring_Write].qno = qno;
	s_total_info.times[s_total_info.ring_Write].recv_time = cur_time;

	last_write = s_total_info.ring_Write;

	if ((s_total_info.ring_Write + 1) < RECV_DATA_SIZE)
		s_total_info.ring_Write++;
	else
		s_total_info.ring_Write = 0;

	if (s_total_info.ring_Read == s_total_info.ring_Write) {
		/* ring buffer is full */
		if ((s_total_info.ring_Read + 1) < RECV_DATA_SIZE)
			s_total_info.ring_Read++;
		else
			s_total_info.ring_Read = 0;

		if (s_total_info.times[last_write].recv_time -
			s_total_info.times[s_total_info.ring_Write].recv_time
				<= 1000000000) {/* spend time <= 1s ? */

			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] error: occur irq burst(c:%d; s:%d; r:%d; w:%d; rt:%lld; wt:%lld).\n",
				__func__, s_total_info.repeat_count,
				RECV_DATA_SIZE,
				s_total_info.ring_Write, last_write,
				s_total_info.times[
					s_total_info.ring_Write].recv_time,
				s_total_info.times[last_write].recv_time);

			ccif_debug_print_irq_info();
		}
	}

	spin_unlock_irqrestore(&s_recv_data_info_lock, flags);
}

void ccif_debug_info_init(void)
{
	spin_lock_init(&s_recv_data_info_lock);
	memset(&s_total_info, 0, sizeof(struct total_recv_info));
}
