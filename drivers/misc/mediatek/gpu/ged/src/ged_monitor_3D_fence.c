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

#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <linux/sync_file.h>
#include <linux/dma-fence.h>

#include <mt-plat/mtk_gpu_utility.h>
/* This has been removed on kernel-4.19
 * #include <trace/events/gpu.h>
 */
#ifdef MTK_GPU_DVFS
#include <mtk_gpufreq.h>
#else
#include "ged_gpufreq.h"
#endif

#include "ged_monitor_3D_fence.h"

#include "ged_log.h"
#include "ged_base.h"
#include "ged_type.h"
#include "ged_dvfs.h"
#include "ged_global.h"

#include <asm/div64.h>

static atomic_t g_i32Count = ATOMIC_INIT(0);
static unsigned int ged_monitor_3D_fence_debug;
static unsigned int ged_monitor_3D_fence_disable;
static unsigned int ged_monitor_3D_fence_switch;
static unsigned int ged_smart_boost;
static unsigned int ged_monitor_3D_fence_systrace;
static unsigned long g_ul3DFenceDoneTime;

struct GED_MONITOR_3D_FENCE {
	struct dma_fence_cb sSyncWaiter;
	struct work_struct sWork;
	struct dma_fence *psSyncFence;
};

static void ged_sync_cb(struct dma_fence *sFence, struct dma_fence_cb *waiter)
{
	struct GED_MONITOR_3D_FENCE *psMonitor;
	unsigned long long t;

	t = ged_get_time();

	do_div(t, 1000);

	ged_monitor_3D_fence_notify();

	psMonitor = GED_CONTAINER_OF(waiter,
		struct GED_MONITOR_3D_FENCE, sSyncWaiter);

	ged_log_buf_print(ghLogBuf_DVFS,
		"[-] ged_monitor_3D_fence_done (ts=%llu) %p",
		t, psMonitor->psSyncFence);

	schedule_work(&psMonitor->sWork);
}

static void ged_monitor_3D_fence_work_cb(struct work_struct *psWork)
{
	struct GED_MONITOR_3D_FENCE *psMonitor;

#ifdef GED_DEBUG_MONITOR_3D_FENCE
	ged_log_buf_print(ghLogBuf_GED, "ged_monitor_3D_fence_work_cb");
#endif


	if (atomic_sub_return(1, &g_i32Count) < 1) {
		unsigned int uiFreqLevelID;

		if (mtk_get_bottom_gpu_freq(&uiFreqLevelID)) {
			if (uiFreqLevelID > 0 && ged_monitor_3D_fence_switch) {
#ifdef GED_DEBUG_MONITOR_3D_FENCE
				ged_log_buf_print(ghLogBuf_GED,
					"mtk_set_bottom_gpu_freq(0)");
#endif
				mtk_set_bottom_gpu_freq(0);

			}
		}
	}

	if (ged_monitor_3D_fence_debug > 0)
		GED_LOGI("[-]3D fences count = %d\n", atomic_read(&g_i32Count));

	psMonitor = GED_CONTAINER_OF(psWork,
		struct GED_MONITOR_3D_FENCE, sWork);
	dma_fence_put(psMonitor->psSyncFence);
	ged_free(psMonitor, sizeof(struct GED_MONITOR_3D_FENCE));
}

unsigned long ged_monitor_3D_fence_done_time(void)
{
	return g_ul3DFenceDoneTime;
}

GED_ERROR ged_monitor_3D_fence_add(int fence_fd)
{
	int err;
	unsigned long long t;
	struct GED_MONITOR_3D_FENCE *psMonitor;
	struct dma_fence *psDebugAddress;

	if (ged_monitor_3D_fence_disable)
		return GED_OK;

	t = ged_get_time();

	do_div(t, 1000);

	psMonitor =
		(struct GED_MONITOR_3D_FENCE *)
		ged_alloc(sizeof(struct GED_MONITOR_3D_FENCE));

#ifdef GED_DEBUG_MONITOR_3D_FENCE
	ged_log_buf_print(ghLogBuf_GED, "[+]$s", __func__);
#endif

	if (!psMonitor)
		return GED_ERROR_OOM;

	psMonitor->psSyncFence = sync_file_get_fence(fence_fd);

	if (psMonitor->psSyncFence == NULL) {
		ged_free(psMonitor, sizeof(struct GED_MONITOR_3D_FENCE));
		return GED_ERROR_INVALID_PARAMS;
	}

	INIT_WORK(&psMonitor->sWork, ged_monitor_3D_fence_work_cb);

	psDebugAddress = psMonitor->psSyncFence;

	err = dma_fence_add_callback(psMonitor->psSyncFence,
		&psMonitor->sSyncWaiter, ged_sync_cb);

	ged_log_buf_print(ghLogBuf_DVFS,
		"[+] %s (ts=%llu) %p", __func__, t, psDebugAddress);


#ifdef GED_DEBUG_MONITOR_3D_FENCE
	ged_log_buf_print(ghLogBuf_GED,
		"dma_fence_add_callback, err = %d", err);
#endif


	if (err < 0) {
		dma_fence_put(psMonitor->psSyncFence);
		ged_free(psMonitor, sizeof(struct GED_MONITOR_3D_FENCE));
	} else {
		int iCount = atomic_add_return(1, &g_i32Count);

		if (iCount > 1) {
			unsigned int uiFreqLevelID;

			if (mtk_get_bottom_gpu_freq(&uiFreqLevelID)) {
				if (uiFreqLevelID !=
					mt_gpufreq_get_dvfs_table_num() - 1) {

					if (ged_monitor_3D_fence_switch)
						mtk_set_bottom_gpu_freq(
						mt_gpufreq_get_dvfs_table_num()
						- 1);
				}
			}
		}
	}

	if (ged_monitor_3D_fence_debug > 0)
		GED_LOGI("[+]3D fences count = %d\n", atomic_read(&g_i32Count));

#ifdef GED_DEBUG_MONITOR_3D_FENCE
	ged_log_buf_print(ghLogBuf_GED,
		"[-]%s, count = %d", __func__,
		atomic_read(&g_i32Count));
#endif

	return GED_OK;
}

void ged_monitor_3D_fence_set_enable(GED_BOOL bEnable)
{
	if (bEnable != ged_monitor_3D_fence_switch && ged_smart_boost)
		ged_monitor_3D_fence_switch = bEnable;
}

void ged_monitor_3D_fence_notify(void)
{
	unsigned long long t;

	t = ged_get_time();

	do_div(t, 1000);

	g_ul3DFenceDoneTime = (unsigned long)t;
}

int ged_monitor_3D_fence_get_count(void)
{
	return atomic_read(&g_i32Count);
}

module_param(ged_smart_boost, uint, 0644);
module_param(ged_monitor_3D_fence_debug, uint, 0644);
module_param(ged_monitor_3D_fence_disable, uint, 0644);
module_param(ged_monitor_3D_fence_systrace, uint, 0644);
