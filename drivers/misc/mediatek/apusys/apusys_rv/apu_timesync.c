// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched/clock.h>
#include <linux/workqueue.h>

#include "apu.h"
#include "apu_ipi.h"

static struct workqueue_struct *apu_ts_workq;
static u64 timesync_stamp;

static void apu_timesync_work_func(struct work_struct *work)
{
	struct mtk_apu *apu = container_of(work, struct mtk_apu, timesync_work);
	int ret;

	timesync_stamp = sched_clock();
	ret = apu_ipi_send(apu, APU_IPI_TIMESYNC, &timesync_stamp, sizeof(u64),
			   0);
	pr_info("%s %d\n", __func__, __LINE__);
}

static void apu_timesync_handler(void *data, u32 len, void *priv)
{
	struct mtk_apu *apu = (struct mtk_apu *)priv;

	dev_info(apu->dev, "%s timesync request received\n", __func__);
	queue_work(apu_ts_workq, &apu->timesync_work);
}

int apu_timesync_init(struct mtk_apu *apu)
{
	int ret;

	apu_ts_workq = alloc_workqueue("apu_timesync",
				       WQ_UNBOUND | WQ_HIGHPRI, 0);
	if (!apu_ts_workq) {
		dev_info(apu->dev, "%s: failed to allocate wq for timesync\n",
			 __func__);
		return -ENOMEM;
	}

	INIT_WORK(&apu->timesync_work, apu_timesync_work_func);

	ret = apu_ipi_register(apu, APU_IPI_TIMESYNC, apu_timesync_handler,
			       apu);
	if (ret) {
		dev_info(apu->dev, "%s: failed to register IPI\n", __func__);
		destroy_workqueue(apu_ts_workq);
		apu_ts_workq = NULL;
		return ret;
	}

	pr_info("%s %d\n", __func__, __LINE__);
	return 0;
}

void apu_timesync_remove(struct mtk_apu *apu)
{
	apu_ipi_unregister(apu, APU_IPI_TIMESYNC);

	if (apu_ts_workq)
		destroy_workqueue(apu_ts_workq);

	pr_info("%s %d\n", __func__, __LINE__);
}
