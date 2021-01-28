// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/atomic.h>

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>

#include <asm/div64.h>

#include <mt-plat/mtk_gpu_utility.h>
#include "ged_notify_sw_vsync.h"
#include "ged_log.h"
#include "ged_base.h"
#include "ged_monitor_3D_fence.h"
#include "ged.h"

#undef CONFIG_MTK_QOS_V1_SUPPORT

#ifdef CONFIG_MTK_QOS_V1_SUPPORT
#include <mtk_gpu_bw.h>
#endif

#ifdef GED_ENABLE_FB_DVFS
#define GED_DVFS_FB_TIMER_TIMEOUT 100000000
#define GED_DVFS_TIMER_TIMEOUT g_fallback_time_out
#else
#define GED_DVFS_TIMER_TIMEOUT 25000000
#endif

#ifndef ENABLE_TIMER_BACKUP
#undef GED_DVFS_TIMER_TIMEOUT
#ifdef GED_ENABLE_FB_DVFS
#define GED_DVFS_FB_TIMER_TIMEOUT 100000000
#define GED_DVFS_TIMER_TIMEOUT g_fallback_time_out
#else
#define GED_DVFS_TIMER_TIMEOUT 25000000
#endif
#endif

#ifdef GED_ENABLE_FB_DVFS
static u64 g_fallback_time_out = GED_DVFS_FB_TIMER_TIMEOUT;
#endif
static struct hrtimer g_HT_hwvsync_emu;

#include "ged_dvfs.h"
#include "ged_global.h"

static struct workqueue_struct *g_psNotifyWorkQueue;
#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
static struct workqueue_struct *g_psDumpFW;
#endif

static struct mutex gsVsyncStampLock;


struct GED_NOTIFY_SW_SYNC {
	struct work_struct	sWork;
	unsigned long t;
	long phase;
	unsigned long ul3DFenceDoneTime;
};

#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
struct GED_DUMP_FW {
	struct work_struct	sWork;
};
#endif

int (*ged_sw_vsync_event_fp)(bool bMode) = NULL;
EXPORT_SYMBOL(ged_sw_vsync_event_fp);
static struct mutex gsVsyncModeLock;
static int ged_sw_vsync_event(bool bMode)
{
	static bool bCurMode;
	int ret;

	ret = 0;
	mutex_lock(&gsVsyncModeLock);

	if (bCurMode != bMode) {
		bCurMode = bMode;
		if (ged_sw_vsync_event_fp) {
			ret = ged_sw_vsync_event_fp(bMode);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] ALL mode change to %d ", bCurMode);
		} else
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] LOCAL mode change to %d ", bCurMode);
		if (bCurMode)
			ret = 1;
	}

	mutex_unlock(&gsVsyncModeLock);
	return ret;
}
static unsigned long long sw_vsync_ts;
static void ged_notify_sw_sync_work_handle(struct work_struct *psWork)
{
	struct GED_NOTIFY_SW_SYNC *psNotify =
		GED_CONTAINER_OF(psWork, struct GED_NOTIFY_SW_SYNC, sWork);
	unsigned long long temp;

	temp = 0;
	if (psNotify) {
		/* if callback is queued, send mode off to real driver */
		ged_sw_vsync_event(false);
#ifdef ENABLE_TIMER_BACKUP
		temp = ged_get_time();

		if (temp-sw_vsync_ts > GED_DVFS_TIMER_TIMEOUT) {
			do_div(temp, 1000);
			psNotify->t = temp;
			ged_dvfs_run(psNotify->t, psNotify->phase,
				psNotify->ul3DFenceDoneTime);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer kicked	(ts=%llu) ", temp);
		} else {
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer kick giveup (ts=%llu)", temp);
		}
#endif
		ged_free(psNotify, sizeof(struct GED_NOTIFY_SW_SYNC));
	}
}

#define GED_VSYNC_MISS_QUANTUM_NS 16666666



#ifdef ENABLE_COMMON_DVFS
static unsigned long long hw_vsync_ts;
#endif


static bool g_timer_on;
static unsigned long long g_timer_on_ts;
static bool g_bGPUClock;

/*
 * void timer_switch(bool bTock)
 * only set the staus, not really operating on real timer
 */
void timer_switch(bool bTock)
{
	mutex_lock(&gsVsyncStampLock);
	g_timer_on = bTock;
	if (bTock)
		g_timer_on_ts = ged_get_time();
	mutex_unlock(&gsVsyncStampLock);
}

void timer_switch_locked(bool bTock)
{
	g_timer_on = bTock;
	if (bTock)
		g_timer_on_ts = ged_get_time();
}

static void ged_timer_switch_work_handle(struct work_struct *psWork)
{
	struct GED_NOTIFY_SW_SYNC *psNotify =
		GED_CONTAINER_OF(psWork, struct GED_NOTIFY_SW_SYNC, sWork);
	if (psNotify) {
		ged_sw_vsync_event(false);
		timer_switch(false);
		ged_free(psNotify, sizeof(struct GED_NOTIFY_SW_SYNC));
	}
}

#ifdef GED_ENABLE_FB_DVFS
void ged_set_backup_timer_timeout(u64 time_out)
{
	if (time_out != 0)
		g_fallback_time_out = time_out;
	else
		g_fallback_time_out = GED_DVFS_FB_TIMER_TIMEOUT;
}
void ged_cancel_backup_timer(void)
{
	unsigned long long temp;

	temp = ged_get_time();
#ifdef ENABLE_TIMER_BACKUP
	if (hrtimer_try_to_cancel(&g_HT_hwvsync_emu)) {
		/* Timer is either queued or in cb
		 * cancel it to ensure it is not bother any way
		 */
		hrtimer_cancel(&g_HT_hwvsync_emu);
		hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] Timer Restart (ts=%llu)", temp);
	} else {
		/*
		 * Timer is not existed
		 */
		hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] New Timer Start (ts=%llu)", temp);
		timer_switch_locked(true);
	}
#endif /*	#ifdef ENABLE_TIMER_BACKUP	*/
}
#endif

GED_ERROR ged_notify_sw_vsync(GED_VSYNC_TYPE eType,
	struct GED_DVFS_UM_QUERY_PACK *psQueryData)
{
	ged_notification(GED_NOTIFICATION_TYPE_SW_VSYNC);

	{
#ifdef ENABLE_COMMON_DVFS

#ifndef GED_ENABLE_FB_DVFS
	long phase = 0;
	unsigned long t;
	bool bHWEventKick = false;
	long long llDiff = 0;
#endif

	unsigned long long temp;
	unsigned long ul3DFenceDoneTime;


	psQueryData->bFirstBorn = ged_sw_vsync_event(true);

	ul3DFenceDoneTime = ged_monitor_3D_fence_done_time();

	psQueryData->ul3DFenceDoneTime = ul3DFenceDoneTime;

	hw_vsync_ts = temp = ged_get_time();


	if (g_gpu_timer_based_emu) {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] Vsync ignored (ts=%llu)", temp);
#ifndef GED_ENABLE_FB_DVFS
		return GED_ERROR_INTENTIONAL_BLOCK;
#endif
	}

#ifdef GED_ENABLE_FB_DVFS
	return GED_ERROR_INTENTIONAL_BLOCK;
#else

	/*critical session begin*/
	mutex_lock(&gsVsyncStampLock);

	if (eType == GED_VSYNC_SW_EVENT) {
		sw_vsync_ts = temp;
#ifdef ENABLE_TIMER_BACKUP
		if (hrtimer_try_to_cancel(&g_HT_hwvsync_emu)) {
			/* Timer is either queued or in cb
			 * cancel it to ensure it is not bother any way
			 */
			hrtimer_cancel(&g_HT_hwvsync_emu);
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer Restart (ts=%llu)", temp);
		} else {
			/*
			 * Timer is not existed
			 */
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] New Timer Start (ts=%llu)", temp);
			timer_switch_locked(true);
		}

#endif // #ifdef ENABLE_TIMER_BACKUP
	} else {
		hw_vsync_ts = temp;

		llDiff = (long long)(hw_vsync_ts - sw_vsync_ts);

		if (llDiff > GED_VSYNC_MISS_QUANTUM_NS)
			bHWEventKick = true;
	}
#ifdef GED_DVFS_DEBUG
	if (eType == GED_VSYNC_HW_EVENT)
		GED_LOGD("HW VSYNC: llDiff=",
		"%lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff,
		hw_vsync_ts, sw_vsync_ts);
	else
		GED_LOGD("SW VSYNC: llDiff=",
		"%lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff,
		hw_vsync_ts, sw_vsync_ts);
#endif		///	#ifdef GED_DVFS_DEBUG


	if (eType == GED_VSYNC_HW_EVENT)
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] HW VSYNC (ts=%llu) ", hw_vsync_ts);
	else
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] SW VSYNC (ts=%llu) ", sw_vsync_ts);

	mutex_unlock(&gsVsyncStampLock);
	/*critical session end*/

	if (eType == GED_VSYNC_SW_EVENT) {
		do_div(temp, 1000);
		t = (unsigned long)(temp);

		// for some cases just align vsync to FenceDoneTime
		if (ul3DFenceDoneTime > t) {
			if (ul3DFenceDoneTime - t < GED_DVFS_DIFF_THRESHOLD)
				t = ul3DFenceDoneTime;
		}
		psQueryData->usT = t;
		ged_dvfs_run(t, phase, ul3DFenceDoneTime);
		ged_dvfs_sw_vsync_query_data(psQueryData);
	} else {
		if (bHWEventKick) {
#ifdef GED_DVFS_DEBUG
			GED_LOGD("HW Event: kick!\n");
#endif							/// GED_DVFS_DEBUG
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] HW VSync: mending kick!");
			ged_dvfs_run(0, 0, 0);
		}
	}
#endif
#else
	unsigned long long temp;

	temp = ged_get_time();
	ged_sw_vsync_event(true);
	return GED_ERROR_INTENTIONAL_BLOCK;
#endif

	return GED_OK;
	}
}

extern unsigned int gpu_loading;
enum hrtimer_restart ged_sw_vsync_check_cb(struct hrtimer *timer)
{
	unsigned long long temp;
	long long llDiff;
	struct GED_NOTIFY_SW_SYNC *psNotify;

	temp = cpu_clock(smp_processor_id());

	llDiff = (long long)(temp - sw_vsync_ts);

	if (llDiff > GED_VSYNC_MISS_QUANTUM_NS) {
		psNotify = (struct GED_NOTIFY_SW_SYNC *)
			ged_alloc_atomic(sizeof(struct GED_NOTIFY_SW_SYNC));

#ifndef ENABLE_TIMER_BACKUP
		ged_dvfs_cal_gpu_utilization(&gpu_av_loading,
			&gpu_block, &gpu_idle);
		gpu_loading = gpu_av_loading;
#endif
		if (false == g_bGPUClock && 0 == gpu_loading
			&& (temp - g_ns_gpu_on_ts > GED_DVFS_TIMER_TIMEOUT)) {
			if (psNotify) {
				INIT_WORK(&psNotify->sWork,
					ged_timer_switch_work_handle);
				queue_work(g_psNotifyWorkQueue,
					&psNotify->sWork);
			}
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer removed	(ts=%llu) ", temp);
			return HRTIMER_NORESTART;
		}

		if (psNotify) {
			INIT_WORK(&psNotify->sWork,
				ged_notify_sw_sync_work_handle);
			psNotify->phase = GED_DVFS_TIMER_BACKUP;
			psNotify->ul3DFenceDoneTime = 0;
			queue_work(g_psNotifyWorkQueue, &psNotify->sWork);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer queue to kick (ts=%llu)", temp);
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			g_timer_on_ts = temp;
		}
	}
	return HRTIMER_NORESTART;
}


bool ged_gpu_power_on_notified;
bool ged_gpu_power_off_notified;
void ged_dvfs_gpu_clock_switch_notify(bool bSwitch)
{

	if (bSwitch) {
		ged_gpu_power_on_notified = true;
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
		mt_gpu_bw_toggle(1);
#endif
		g_ns_gpu_on_ts = ged_get_time();
		g_bGPUClock = true;
		if (g_timer_on) {
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] Timer Already Start");
		} else {
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] HW Start Timer");
			timer_switch(true);
		}
	} else {
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
		mt_gpu_bw_toggle(0);
#endif
		ged_gpu_power_off_notified = true;
		g_bGPUClock = false;
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Buck-off");
	}
}
EXPORT_SYMBOL(ged_dvfs_gpu_clock_switch_notify);

#define GED_TIMER_BACKUP_THRESHOLD 3000

/*
 *	SODI implementation need to cancel timer physically.
 *	but timer status is logically unchanged	*
 */

/*
 * enter sodi state is trivial, just cancel timer
 */
void ged_sodi_start(void)
{
	hrtimer_try_to_cancel(&g_HT_hwvsync_emu);
}

/*
 * exit sodi state should aware sands of time is still running
 */
void ged_sodi_stop(void)
{
	unsigned long long ns_cur_time;
	unsigned long long ns_timer_remains;

	if (g_timer_on) {
		ns_cur_time = ged_get_time();
		ns_timer_remains = ns_cur_time
			- g_timer_on_ts - GED_DVFS_TIMER_TIMEOUT;
		// slept too long, do timber-based DVFS now
		if (ns_timer_remains < GED_TIMER_BACKUP_THRESHOLD) {
			struct GED_NOTIFY_SW_SYNC *psNotify;

			psNotify = (struct GED_NOTIFY_SW_SYNC *)
				ged_alloc_atomic(
				sizeof(struct GED_NOTIFY_SW_SYNC));
			if (psNotify) {
				INIT_WORK(&psNotify->sWork,
				ged_notify_sw_sync_work_handle);
				psNotify->t = ns_cur_time;
				psNotify->phase = GED_DVFS_TIMER_BACKUP;
				psNotify->ul3DFenceDoneTime = 0;
				queue_work(g_psNotifyWorkQueue,
				&psNotify->sWork);
			}
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		} else if (ns_timer_remains > GED_DVFS_TIMER_TIMEOUT)  {
			// unknown status, just start timer with default timeout
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		} else {
			// keep counting down the timer with real remianing time
			hrtimer_start(&g_HT_hwvsync_emu,
			ns_to_ktime(ns_timer_remains), HRTIMER_MODE_REL);
		}
	}
}

#ifdef CONFIG_MTK_GPU_SUPPORT /* Only enable when GPU isn't kerenl module */
#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
static void ged_dump_fw_handle(struct work_struct *psWork)
{
	struct GED_DUMP_FW *psNotify
		= GED_CONTAINER_OF(psWork, struct GED_DUMP_FW, sWork);

	if (psNotify) {
		MTKFWDump();
		ged_free(psNotify, sizeof(struct GED_DUMP_FW));
	}
}

void ged_dump_fw(void)
{
	struct GED_DUMP_FW *psNotify;

	psNotify = (struct GED_DUMP_FW *)
		ged_alloc_atomic(sizeof(struct GED_DUMP_FW));

	if (psNotify) {
		INIT_WORK(&psNotify->sWork, ged_dump_fw_handle);
		queue_work(g_psDumpFW, &psNotify->sWork);
	}
}
EXPORT_SYMBOL(ged_dump_fw);
#endif
#endif /* CONFIG_MTK_GPU_SUPPORT */

GED_ERROR ged_notify_sw_vsync_system_init(void)
{
	g_psNotifyWorkQueue = create_workqueue("ged_notify_sw_vsync");

	if (g_psNotifyWorkQueue == NULL)
		return GED_ERROR_OOM;

#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
	g_psDumpFW = NULL;
	g_psDumpFW = create_workqueue("ged_dump_fw_log");

	if (g_psDumpFW == NULL)
		return GED_ERROR_OOM;
#endif

	mutex_init(&gsVsyncStampLock);
	mutex_init(&gsVsyncModeLock);

	hrtimer_init(&g_HT_hwvsync_emu, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_hwvsync_emu.function = ged_sw_vsync_check_cb;

	mtk_gpu_sodi_entry_fp = ged_sodi_start;
	mtk_gpu_sodi_exit_fp = ged_sodi_stop;

	return GED_OK;
}

void ged_notify_sw_vsync_system_exit(void)
{
	if (g_psNotifyWorkQueue != NULL) {
		flush_workqueue(g_psNotifyWorkQueue);
		destroy_workqueue(g_psNotifyWorkQueue);
		g_psNotifyWorkQueue = NULL;
	}

#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
	if (g_psDumpFW != NULL) {
		flush_workqueue(g_psDumpFW);
		destroy_workqueue(g_psDumpFW);
		g_psDumpFW = NULL;
	}
#endif
#ifdef ENABLE_COMMON_DVFS
	hrtimer_cancel(&g_HT_hwvsync_emu);
#endif
	mutex_destroy(&gsVsyncModeLock);
	mutex_destroy(&gsVsyncStampLock);
}
