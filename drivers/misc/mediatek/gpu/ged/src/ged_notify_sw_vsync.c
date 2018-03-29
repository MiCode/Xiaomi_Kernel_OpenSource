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
#include <asm/atomic.h>

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include <asm/div64.h>

#include <mt-plat/mtk_gpu_utility.h>
#include "ged_notify_sw_vsync.h"
#include "ged_log.h"
#include "ged_base.h"
#include "ged_monitor_3D_fence.h"

#define GED_DVFS_TIMER_TIMEOUT 25000000

#ifndef ENABLE_TIMER_BACKUP
#undef GED_DVFS_TIMER_TIMEOUT
#define GED_DVFS_TIMER_TIMEOUT 70000000
#endif


static struct hrtimer g_HT_hwvsync_emu;

#include "ged_dvfs.h"

extern void (*mtk_gpu_sodi_entry_fp)(void);
extern void (*mtk_gpu_sodi_exit_fp)(void);


static struct workqueue_struct* g_psNotifyWorkQueue = NULL;

static struct mutex gsVsyncStampLock;


typedef struct GED_NOTIFY_SW_SYNC_TAG
{
	struct work_struct	sWork;
	unsigned long t;
	long phase;
	unsigned long ul3DFenceDoneTime;
} GED_NOTIFY_SW_SYNC;

extern GED_LOG_BUF_HANDLE ghLogBuf_DVFS;
int (*ged_sw_vsync_event_fp)(bool bMode) = NULL;
EXPORT_SYMBOL(ged_sw_vsync_event_fp);
static struct mutex gsVsyncModeLock;
static int ged_sw_vsync_event(bool bMode)
{
	static bool bCurMode = false;
	int ret;
	
	ret = 0;
	mutex_lock(&gsVsyncModeLock);
	
	if(bCurMode!=bMode)
	{
		bCurMode = bMode;
		if(ged_sw_vsync_event_fp)
		{
			ret = ged_sw_vsync_event_fp(bMode);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] ALL mode change to %d ",bCurMode);
		}
		else
		{
			
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] LOCAL mode change to %d ",bCurMode);
		}		
		
		if(bCurMode)
			ret = 1;
	}	
	
	mutex_unlock(&gsVsyncModeLock);
	
	return ret;
}
static unsigned long long sw_vsync_ts;
static void ged_notify_sw_sync_work_handle(struct work_struct *psWork)
{
	GED_NOTIFY_SW_SYNC* psNotify = GED_CONTAINER_OF(psWork, GED_NOTIFY_SW_SYNC, sWork);
	unsigned long long temp;
	temp = 0;
	if (psNotify)
	{
		ged_sw_vsync_event(false); // if this callback is queued, send mode off to real driver
#ifdef ENABLE_TIMER_BACKUP		
		temp = ged_get_time();
		
		if(temp-sw_vsync_ts>GED_DVFS_TIMER_TIMEOUT)
		{
			do_div(temp,1000);
			psNotify->t = temp;
		ged_dvfs_run(psNotify->t, psNotify->phase, psNotify->ul3DFenceDoneTime);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer kicked	(ts=%llu) ", temp);
		}
		else
		{
 			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer kick giveup	(ts=%llu) ", temp);
		}
#endif		
		ged_free(psNotify, sizeof(GED_NOTIFY_SW_SYNC));
	}
}

#define GED_VSYNC_MISS_QUANTUM_NS 16666666



#ifdef ENABLE_COMMON_DVFS	
static unsigned long long hw_vsync_ts;
#endif
static unsigned long long g_ns_gpu_on_ts=0;

static bool g_timer_on = false;
static unsigned long long g_timer_on_ts=0;

static bool g_bGPUClock = false;





/*
 * void timer_switch(bool bTock)
 * only set the staus, not really operating on real timer 
 */
void timer_switch(bool bTock)
{
	mutex_lock(&gsVsyncStampLock);
	g_timer_on = bTock;
	if(bTock)
	{
		g_timer_on_ts = ged_get_time();
	}
	mutex_unlock(&gsVsyncStampLock);		
}

void timer_switch_locked(bool bTock)
{
	g_timer_on = bTock;
	if(bTock)
	{
		g_timer_on_ts = ged_get_time();
	}
}


static void ged_timer_switch_work_handle(struct work_struct *psWork)
{
	GED_NOTIFY_SW_SYNC* psNotify = GED_CONTAINER_OF(psWork, GED_NOTIFY_SW_SYNC, sWork);
	if (psNotify)
	{
		ged_sw_vsync_event(false);
		timer_switch(false);
		ged_free(psNotify, sizeof(GED_NOTIFY_SW_SYNC));
	}
}

extern unsigned int g_gpu_timer_based_emu;
extern unsigned long g_ulCalResetTS_us; // calculate loading reset time stamp
extern unsigned long g_ulPreCalResetTS_us; // previous calculate loading reset time stamp
extern unsigned long g_ulWorkingPeriod_us; // last frame half, t0

GED_ERROR ged_notify_sw_vsync(GED_VSYNC_TYPE eType, GED_DVFS_UM_QUERY_PACK* psQueryData)
{
#ifdef ENABLE_COMMON_DVFS	

	long long llDiff = 0;
	bool bHWEventKick = false;
	unsigned long long temp;

	unsigned long t;
	long phase = 0;
	unsigned long ul3DFenceDoneTime;

	psQueryData->bFirstBorn = ged_sw_vsync_event(true);
	
	ul3DFenceDoneTime = ged_monitor_3D_fence_done_time(); 
	
	psQueryData-> ul3DFenceDoneTime = ul3DFenceDoneTime;
	/*psQueryData->ulWorkingPeriod_us = g_ulWorkingPeriod_us;
	psQueryData->ulPreCalResetTS_us = g_ulCalResetTS_us; // IMPORTANT*/
	
	temp = ged_get_time();

	
	if(g_gpu_timer_based_emu) 
	{
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Vsync ignored (ts=%llu)", temp);
		return GED_INTENTIONAL_BLOCK;
	}



	/*critical session begin*/
	mutex_lock(&gsVsyncStampLock);

	if(GED_VSYNC_SW_EVENT==eType)
	{
		sw_vsync_ts = temp;
#ifdef ENABLE_TIMER_BACKUP				
		if(hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL)) // timer not start
		{
			hrtimer_try_to_cancel ( &g_HT_hwvsync_emu );
			hrtimer_restart(&g_HT_hwvsync_emu);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer Restart (ts=%llu)", temp);
		}
		else // timer active
		{
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] New Timer Start (ts=%llu)", temp);
			timer_switch_locked(true);
		}				

#endif			///	#ifdef ENABLE_TIMER_BACKUP		
	}
	else
	{
		hw_vsync_ts = temp;

		llDiff = (long long)(hw_vsync_ts - sw_vsync_ts);

		if(llDiff > GED_VSYNC_MISS_QUANTUM_NS)
		{
			bHWEventKick = true;
		}
	}		
#ifdef GED_DVFS_DEBUG		
	if(GED_VSYNC_HW_EVENT==eType)
	{
		GED_LOGE("[5566] HW VSYNC: llDiff= %lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff, hw_vsync_ts, sw_vsync_ts);
	}
	else
	{
		GED_LOGE("[5566] SW VSYNC: llDiff= %lld, hw_vsync_ts=%llu, sw_vsync_ts=%llu\n", llDiff, hw_vsync_ts, sw_vsync_ts);
	}
#endif		///	#ifdef GED_DVFS_DEBUG		


	if(GED_VSYNC_HW_EVENT==eType)
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] HW VSYNC (ts=%llu) ", hw_vsync_ts);
	else
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] SW VSYNC (ts=%llu) ", sw_vsync_ts);

	mutex_unlock(&gsVsyncStampLock);
	/*critical session end*/

	if(GED_VSYNC_SW_EVENT==eType)
	{
		do_div(temp,1000);
		t = (unsigned long)(temp);
		
		if(ul3DFenceDoneTime>t) // for some cases just align vsync to FenceDoneTime
		{
			if(ul3DFenceDoneTime - t < GED_DVFS_DIFF_THRESHOLD) // allow diff
				t = ul3DFenceDoneTime;
		}
		psQueryData->usT = t;
		
		ged_dvfs_run(t, phase, ul3DFenceDoneTime);
		
		
		ged_dvfs_sw_vsync_query_data(psQueryData);
	}		
	else
	{
		if(bHWEventKick)
		{
#ifdef GED_DVFS_DEBUG		
			GED_LOGE("[5566] HW Event: kick!\n");
#endif							/// GED_DVFS_DEBUG	
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] HW VSync: mending kick!");
			ged_dvfs_run(0, 0, 0);
		}
	}

#else
#if 0
	GED_NOTIFY_SW_SYNC* psNotify;
	unsigned long long temp = cpu_clock(smp_processor_id());
	*pt = (unsigned long)(temp / 1000);

	psNotify = (GED_NOTIFY_SW_SYNC*)ged_alloc(sizeof(GED_NOTIFY_SW_SYNC));
	if (!psNotify)
	{
		return GED_ERROR_OOM;
	}

	INIT_WORK(&psNotify->sWork, ged_notify_sw_sync_work_handle);
	psNotify->t = *pt;
	psNotify->phase = phase;
	psNotify->ul3DFenceDoneTime = ged_monitor_3D_fence_done_time();
	queue_work(g_psNotifyWorkQueue, &psNotify->sWork);
#endif /// #ifdef ENABLE_COMMON_DVFS
	unsigned long long temp;
	temp = ged_get_time();
	ged_sw_vsync_event(true);
	
		/*if no timer-backup need to start timer for event notify to real driver*/
#ifndef ENABLE_TIMER_BACKUP				
		if(hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL)) // timer not start
		{
			hrtimer_try_to_cancel ( &g_HT_hwvsync_emu );
			hrtimer_restart(&g_HT_hwvsync_emu);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer Restart (ts=%llu)", temp);
		}
		else // timer active
		{
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] New Timer Start (ts=%llu)", temp);
			timer_switch_locked(true);
		}				

#endif			///	#ifdef ENABLE_TIMER_BACKUP		
	return GED_INTENTIONAL_BLOCK; // not to do further operations
#endif

	return GED_OK;
}

extern unsigned int gpu_loading;
enum hrtimer_restart ged_sw_vsync_check_cb( struct hrtimer *timer )
{
	unsigned long long temp;
	long long llDiff;
	GED_NOTIFY_SW_SYNC* psNotify;

	temp = cpu_clock(smp_processor_id()); // interrupt contex no need to set non-preempt

	llDiff = (long long)(temp - sw_vsync_ts);

	if(llDiff > GED_VSYNC_MISS_QUANTUM_NS)
	{
		psNotify = (GED_NOTIFY_SW_SYNC*)ged_alloc_atomic(sizeof(GED_NOTIFY_SW_SYNC));
		
#ifndef ENABLE_TIMER_BACKUP		
		mtk_get_gpu_loading(&gpu_loading);
#endif
		if(false==g_bGPUClock && 0==gpu_loading && (temp - g_ns_gpu_on_ts> GED_DVFS_TIMER_TIMEOUT) )
			
		{
			if (psNotify)
			{
				INIT_WORK(&psNotify->sWork, ged_timer_switch_work_handle);	
				queue_work(g_psNotifyWorkQueue, &psNotify->sWork);
			}
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer removed	(ts=%llu) ", temp);						
			return HRTIMER_NORESTART;
		}				


		if (psNotify)
		{
			INIT_WORK(&psNotify->sWork, ged_notify_sw_sync_work_handle);
			/*
			psNotify->t = temp;
			do_div(psNotify->t,1000);
			*/
			psNotify->phase = GED_DVFS_TIMER_BACKUP;
			psNotify->ul3DFenceDoneTime = 0;
			queue_work(g_psNotifyWorkQueue, &psNotify->sWork);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer queue to kick	(ts=%llu) ", temp);
			hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			g_timer_on_ts = temp;
		}
	}	
	return HRTIMER_NORESTART;
}


bool ged_gpu_power_on_notified = 0;
bool ged_gpu_power_off_notified = 0;
void ged_dvfs_gpu_clock_switch_notify(bool bSwitch)
{

	if(bSwitch)
	{				
		ged_gpu_power_on_notified = true;
		g_ns_gpu_on_ts = ged_get_time();
		g_bGPUClock = true;
		if( g_timer_on )
		{
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] Timer Already Start");
		}
		else
		{
			hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] HW Start Timer");
			timer_switch(true);
		}
	}
	else
	{
		ged_gpu_power_off_notified = true;
		g_bGPUClock = false;
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
	if(g_timer_on)
	{
		ns_cur_time = ged_get_time();
		ns_timer_remains = ns_cur_time - g_timer_on_ts - GED_DVFS_TIMER_TIMEOUT;
		if( ns_timer_remains < GED_TIMER_BACKUP_THRESHOLD ) // sleeped too long, do timber-based DVFS now
		{
			GED_NOTIFY_SW_SYNC* psNotify;
			psNotify = (GED_NOTIFY_SW_SYNC*)ged_alloc_atomic(sizeof(GED_NOTIFY_SW_SYNC));
			if (psNotify)
			{
				INIT_WORK(&psNotify->sWork, ged_notify_sw_sync_work_handle);
				psNotify->t = ns_cur_time;
				psNotify->phase = GED_DVFS_TIMER_BACKUP;
				psNotify->ul3DFenceDoneTime = 0;
				queue_work(g_psNotifyWorkQueue, &psNotify->sWork);								
			}						
			hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		}
		else if( ns_timer_remains > GED_DVFS_TIMER_TIMEOUT) 
		{
			// unknown status, just start timer with default timeout;
			hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(GED_DVFS_TIMER_TIMEOUT), HRTIMER_MODE_REL);
		}
		else // keep counting down the timer with real remianing time
		{
			hrtimer_start(&g_HT_hwvsync_emu, ns_to_ktime(ns_timer_remains), HRTIMER_MODE_REL);						
		}				
	}

}


GED_ERROR ged_notify_sw_vsync_system_init(void)
{
	g_psNotifyWorkQueue = create_workqueue("ged_notify_sw_vsync");

	if (g_psNotifyWorkQueue == NULL)
	{
		return GED_ERROR_OOM;
	}
	mutex_init(&gsVsyncStampLock);
	mutex_init(&gsVsyncModeLock);
	
	hrtimer_init(&g_HT_hwvsync_emu, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_HT_hwvsync_emu.function = ged_sw_vsync_check_cb;		

	mtk_gpu_sodi_entry_fp= ged_sodi_start;
	mtk_gpu_sodi_exit_fp= ged_sodi_stop;

	return GED_OK;
}

void ged_notify_sw_vsync_system_exit(void)
{
	if (g_psNotifyWorkQueue != NULL)
	{
		flush_workqueue(g_psNotifyWorkQueue);

		destroy_workqueue(g_psNotifyWorkQueue);

		g_psNotifyWorkQueue = NULL;
	}
#ifdef ENABLE_COMMON_DVFS			 
	hrtimer_cancel( &g_HT_hwvsync_emu );
#endif	
	mutex_destroy(&gsVsyncModeLock);
	mutex_destroy(&gsVsyncStampLock);	
}
