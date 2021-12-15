/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/workqueue.h>
#include <mt-plat/aee.h>

#include "mtk_gpu_log.h"
#include "mtk_gpufreq_core.h"

static struct workqueue_struct *g_aee_workqueue;
static struct work_struct g_aee_work;

static int aee_dumping;

static void aee_Handle(struct work_struct *_psWork)
{
	GPULOG2("trigger aee, call aee_kernel_exception");

	aee_kernel_exception("gpulog", "aee dump gpulog");

	aee_dumping = 0;
}

void mtk_gpu_log_trigger_aee(const char *msg)
{
	static int count;

	if (g_aee_workqueue && count < 5 && aee_dumping == 0) {
		count += 1;
		aee_dumping = 1;

		GPULOG2("trigger aee [%s], count: %d", msg, count);

		//queue_work(g_aee_workqueue, &g_aee_work);
	} else {
		GPULOG2("skip aee [%s], count: %d, aee_dumping: %d", msg, count, aee_dumping);
	}
}

void mtk_gpu_log_init(void)
{
	int ret;

	//g_aee_workqueue = alloc_ordered_workqueue("gpu_aee_wq", WQ_FREEZABLE | WQ_MEM_RECLAIM);
	//INIT_WORK(&g_aee_work, aee_Handle);

	/* init log hnd */
	//ret = ged_log_buf_get_early("fence_trace", &_mtk_gpu_log_hnd);
}

