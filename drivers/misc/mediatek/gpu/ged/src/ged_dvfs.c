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

#include <linux/slab.h>
#include <linux/sched.h>

#ifdef GED_DVFS_ENABLE
#include <mt-plat/mt_boot.h>
#include <mt_gpufreq.h>
#endif

#include <trace/events/mtk_events.h>
#include <mt-plat/mtk_gpu_utility.h>

#include <asm/siginfo.h>
#include <linux/sched.h>
#include <linux/signal.h>

#include "disp_session.h"
#include "ged_dvfs.h"
#include "ged_monitor_3D_fence.h"
#include "ged_profile_dvfs.h"
#include "ged_log.h"
#include "ged_base.h"

#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50//16//100

/*Definition of GED_DVFS_SKIP_ROUNDS is to skip DVFS when boost raised 
  the value stands for counting down rounds of DVFS period
  Current using vsync that would be 16ms as period, 
  below boost at (32, 48] seconds per boost
#define GED_DVFS_SKIP_ROUNDS 3 */
#define GED_DVFS_SKIP_ROUNDS 3

extern GED_LOG_BUF_HANDLE ghLogBuf_DVFS;
extern GED_LOG_BUF_HANDLE ghLogBuf_ged_srv;

static struct mutex gsDVFSLock;
static struct mutex gsVSyncOffsetLock;

static unsigned int g_iSkipCount=0;
static int g_dvfs_skip_round=0;

static unsigned int gpu_power = 0;
static unsigned int gpu_dvfs_enable;

MTK_GPU_DVFS_TYPE g_CommitType=0;
unsigned long g_ulCommitFreq=0; 


#ifdef GED_DVFS_ENABLE
static unsigned int boost_gpu_enable;
static unsigned int  gpu_bottom_freq;
static unsigned int  gpu_cust_boost_freq;
static unsigned int  gpu_cust_upbound_freq;
static unsigned int  g_ui32PreFreqID;
static unsigned int  g_bottom_freq_id;



static unsigned int  g_cust_upbound_freq_id;

#endif

static unsigned int  g_computed_freq_id = 0;

static unsigned int gpu_debug_enable;


static unsigned int  g_cust_boost_freq_id;

unsigned int g_gpu_timer_based_emu;

static unsigned int gpu_pre_loading = 0;
unsigned int gpu_loading = 0;
unsigned int gpu_av_loading = 0;
unsigned int gpu_sub_loading = 0;
static unsigned int gpu_block = 0;
static unsigned int gpu_idle = 0;
unsigned long g_um_gpu_tar_freq = 0;


spinlock_t	g_sSpinLock;
unsigned long g_ulCalResetTS_us = 0; // calculate loading reset time stamp
unsigned long g_ulPreCalResetTS_us = 0; // previous calculate loading reset time stamp
unsigned long g_ulWorkingPeriod_us = 0; // last frame half, t0

unsigned long g_ulPreDVFS_TS_us = 0; // record previous DVFS applying time stamp


static unsigned long gL_ulCalResetTS_us = 0; // calculate loading reset time stamp
static unsigned long gL_ulPreCalResetTS_us = 0; // previous calculate loading reset time stamp
static unsigned long gL_ulWorkingPeriod_us = 0; // last frame half, t0




static unsigned int  g_ui32FreqIDFromPolicy = 0;

unsigned long g_ulvsync_period;
static GED_DVFS_TUNING_MODE g_eTuningMode = 0;

unsigned int g_ui32EventStatus = 0;
unsigned int g_ui32EventDebugStatus = 0;
static int g_VsyncOffsetLevel = 0;

static int g_probe_pid=GED_NO_UM_SERVICE;

typedef void (*gpufreq_input_boost_notify)(unsigned int );
typedef void (*gpufreq_power_limit_notify)(unsigned int );
extern void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB);
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);
extern void (*mtk_boost_gpu_freq_fp)(void);
extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int);
extern unsigned int (*mtk_get_bottom_gpu_freq_fp)(void);
extern unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void);
extern void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel);
extern unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void);
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
extern unsigned int (*mtk_get_gpu_block_fp)(void);
extern unsigned int (*mtk_get_gpu_idle_fp)(void);
extern void (*mtk_do_gpu_dvfs_fp)(unsigned long t, long phase, unsigned long ul3DFenceDoneTime);
extern void (*mtk_gpu_dvfs_set_mode_fp)(int eMode);

extern unsigned int (*mtk_get_gpu_sub_loading_fp)(void);
extern unsigned long (*mtk_get_vsync_based_target_freq_fp)(void);
extern void (*mtk_get_gpu_dvfs_from_fp)(MTK_GPU_DVFS_TYPE* peType, unsigned long *pulFreq);

extern unsigned long (*mtk_get_gpu_bottom_freq_fp)(void);
extern unsigned long (*mtk_get_gpu_custom_boost_freq_fp)(void);
extern unsigned long (*mtk_get_gpu_custom_upbound_freq_fp)(void);

extern void ged_monitor_3D_fence_set_enable(GED_BOOL bEnable);


static bool ged_dvfs_policy(
		unsigned int ui32GPULoading, unsigned int* pui32NewFreqID, 
		unsigned long t, long phase, unsigned long ul3DFenceDoneTime, bool bRefreshed);
unsigned long ged_gas_query_mode(void);

unsigned long ged_query_info( GED_INFO eType)
{
	unsigned int gpu_loading;
	unsigned int gpu_block;
	unsigned int gpu_idle;
	switch(eType)
	{
		case GED_LOADING:
			mtk_get_gpu_loading(&gpu_loading);
			return gpu_loading;
		case GED_IDLE:
			mtk_get_gpu_idle(&gpu_idle);
			return gpu_idle;
		case GED_BLOCKING:
			mtk_get_gpu_block(&gpu_block);
			return gpu_block;
#ifdef GED_DVFS_ENABLE
		case GED_PRE_FREQ:
			return mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
		case GED_PRE_FREQ_IDX:
			return g_ui32PreFreqID;
		case GED_CUR_FREQ:
			return mt_gpufreq_get_freq_by_idx(mt_gpufreq_get_cur_freq_index());
		case GED_CUR_FREQ_IDX:
			return mt_gpufreq_get_cur_freq_index();
		case GED_MAX_FREQ_IDX:
			return mt_gpufreq_get_dvfs_table_num()-1;
		case GED_MAX_FREQ_IDX_FREQ:   
			return mt_gpufreq_get_freq_by_idx(mt_gpufreq_get_dvfs_table_num()-1);
		case GED_MIN_FREQ_IDX:
			return 0;
		case GED_MIN_FREQ_IDX_FREQ:
			return mt_gpufreq_get_freq_by_idx(0);
#endif
		case GED_EVENT_GAS_MODE:
			return ged_gas_query_mode();
		case GED_3D_FENCE_DONE_TIME:
			return ged_monitor_3D_fence_done_time();
		case GED_VSYNC_OFFSET:
			return ged_dvfs_vsync_offset_level_get();
		case GED_EVENT_STATUS:
			return g_ui32EventStatus;
		case GED_EVENT_DEBUG_STATUS:
			return g_ui32EventDebugStatus;
		case GED_SRV_SUICIDE:
			ged_dvfs_probe_signal(GED_SRV_SUICIDE_EVENT);
			return g_probe_pid;
		case GED_PRE_HALF_PERIOD:
			return g_ulWorkingPeriod_us;
		case GED_LATEST_START:
			return g_ulPreCalResetTS_us;
		case GED_FPS: {
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757) || \
    defined(CONFIG_ARCH_MT6797)
			disp_session_info info;
			memset(&info, 0, sizeof(info));
			info.session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
			disp_mgr_get_session_info(&info);
			return info.updateFPS;
#else
			return 0;
#endif
		}
		default:
			return 0;
	}
}
EXPORT_SYMBOL(ged_query_info);

//-----------------------------------------------------------------------------
void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle) = NULL;
EXPORT_SYMBOL(ged_dvfs_cal_gpu_utilization_fp);
//-----------------------------------------------------------------------------

bool ged_dvfs_cal_gpu_utilization(unsigned int* pui32Loading , unsigned int* pui32Block,unsigned int* pui32Idle)
{

	if (NULL != ged_dvfs_cal_gpu_utilization_fp)
	{
		ged_dvfs_cal_gpu_utilization_fp(pui32Loading, pui32Block, pui32Idle);        
		gpu_sub_loading = *pui32Loading;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID)
// call back function
// This shall be registered in vendor's GPU driver,
// since each IP has its own rule
void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType, int* pbCommited) = NULL;
EXPORT_SYMBOL(ged_dvfs_gpu_freq_commit_fp);
//-----------------------------------------------------------------------------




bool ged_dvfs_gpu_freq_commit(unsigned long ui32NewFreqID, GED_DVFS_COMMIT_TYPE eCommitType)
{
	int bCommited=false;
#ifdef GED_DVFS_ENABLE    
	unsigned long ui32CurFreqID;
	ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
	if (NULL != ged_dvfs_gpu_freq_commit_fp)
	{

		if (ui32NewFreqID > g_bottom_freq_id)
		{
			ui32NewFreqID = g_bottom_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_SMARTBOOST;
		}
		if (ui32NewFreqID > g_cust_boost_freq_id)
		{
			ui32NewFreqID = g_cust_boost_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_CUSTOMIZATION;
		}

		// up bound
		if (ui32NewFreqID < g_cust_upbound_freq_id)
		{
			ui32NewFreqID = g_cust_upbound_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_CUSTOMIZATION;
		}

		// thermal power limit
		if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index())
		{
			ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();
			g_CommitType = MTK_GPU_DVFS_TYPE_THERMAL;
		}

		g_ulCommitFreq = mt_gpufreq_get_freq_by_idx(ui32NewFreqID);
		
		// do change
		if (ui32NewFreqID != ui32CurFreqID)
		{
			// call to DVFS module
			
			ged_dvfs_gpu_freq_commit_fp(ui32NewFreqID, eCommitType, &bCommited);
			/* 
			 * To-Do: refine previous freq contributions, 
			 * since it is possible to have multiple freq settings in previous execution period
			 * Does this fatal for precision?
			 */
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] new freq ID commited: idx=%lu type=%u",ui32NewFreqID, eCommitType);
			if(true==bCommited)
			{
				ged_log_trace_counter("Freq-idx",ui32NewFreqID);
				ged_log_trace_counter("commit-type",eCommitType);
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] commited true");
				g_ui32PreFreqID = ui32CurFreqID;            
			}
		}	
	}
#endif    
	return bCommited;
}


unsigned long get_ns_period_from_fps(unsigned int ui32Fps)
{
	return 1000000/ui32Fps;
}


void ged_dvfs_set_tuning_mode(GED_DVFS_TUNING_MODE eMode)
{
	g_eTuningMode=eMode;    

}

void ged_dvfs_set_tuning_mode_wrap(int eMode)
{
	ged_dvfs_set_tuning_mode( (GED_DVFS_TUNING_MODE) eMode) ;
}



GED_DVFS_TUNING_MODE ged_dvfs_get_tuning_mode()
{
	return g_eTuningMode;
}

//g_i32EvenStatus



GED_ERROR ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent, bool bSwitch)
{
	unsigned int  ui32BeforeSwitchInterpret;
	unsigned int  ui32BeforeDebugInterpret;
	GED_ERROR ret = GED_OK;
	mutex_lock(&gsVSyncOffsetLock);

	ui32BeforeSwitchInterpret = g_ui32EventStatus;
	ui32BeforeDebugInterpret = g_ui32EventDebugStatus;
	
	switch(eEvent)
	{
		case GED_DVFS_VSYNC_OFFSET_FORCE_ON:
			g_ui32EventDebugStatus |= GED_EVENT_FORCE_ON;
			g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_OFF);
			break;
		case GED_DVFS_VSYNC_OFFSET_FORCE_OFF:
			g_ui32EventDebugStatus |= GED_EVENT_FORCE_OFF;
			g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_ON);
			break;
		case GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT:
			g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_ON);
			g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_OFF);
			break;
		case GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT:
			if(GED_TRUE==bSwitch) // touch boost
			{
				ged_dvfs_boost_gpu_freq(); 
			}
			(bSwitch)? (g_ui32EventStatus|=GED_EVENT_TOUCH): (g_ui32EventStatus&= (~GED_EVENT_TOUCH));            
			break;
		case GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT:
			(bSwitch)? (g_ui32EventStatus|=GED_EVENT_THERMAL): (g_ui32EventStatus&= (~GED_EVENT_THERMAL));
			break;
		case GED_DVFS_VSYNC_OFFSET_WFD_EVENT:
			(bSwitch)? (g_ui32EventStatus|=GED_EVENT_WFD): (g_ui32EventStatus&= (~GED_EVENT_WFD));
			break;
		case GED_DVFS_VSYNC_OFFSET_MHL_EVENT:
			(bSwitch)? (g_ui32EventStatus|=GED_EVENT_MHL): (g_ui32EventStatus&= (~GED_EVENT_MHL));
			break;
		case GED_DVFS_VSYNC_OFFSET_VR_EVENT:
			(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VR) : (g_ui32EventStatus &= (~GED_EVENT_VR));
			break;
		case GED_DVFS_VSYNC_OFFSET_GAS_EVENT:
			(bSwitch)? (g_ui32EventStatus|=GED_EVENT_GAS): (g_ui32EventStatus&= (~GED_EVENT_GAS));
			ged_monitor_3D_fence_set_enable(!bSwitch);
			ret = ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
			break;
		case GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT:
			(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_LOW_POWER_MODE) : (g_ui32EventStatus &= (~GED_EVENT_LOW_POWER_MODE));
			ret = ged_dvfs_probe_signal(GED_LOW_POWER_MODE_SIGNAL_EVENT);
			break;
		case GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT:
			(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_MHL4K_VID) : (g_ui32EventStatus &= (~GED_EVENT_MHL4K_VID));
			ret = ged_dvfs_probe_signal(GED_MHL4K_VID_SIGNAL_EVENT);
			break;
		case GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT:
			(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VILTE_VID) : (g_ui32EventStatus &= (~GED_EVENT_VILTE_VID));
			ret = ged_dvfs_probe_signal(GED_VILTE_VID_SIGNAL_EVENT);
			break;
		case GED_DVFS_BOOST_HOST_EVENT:
			ret = ged_dvfs_probe_signal(GED_SIGNAL_BOOST_HOST_EVENT);
			goto CHECK_OUT;
		default:
			GED_LOGE("%s: not acceptable event:%u \n", __func__,  eEvent); 
			ret = GED_ERROR_INVALID_PARAMS;
			goto CHECK_OUT;
	}

	if(ui32BeforeSwitchInterpret != g_ui32EventStatus || ui32BeforeDebugInterpret != g_ui32EventDebugStatus 
			|| g_ui32EventDebugStatus&GED_EVENT_NOT_SYNC)
	{
		ged_log_trace_counter("vsync-offset event",g_ui32EventStatus);
		ged_log_trace_counter("vsync-offset debug",g_ui32EventDebugStatus);
		ret = ged_dvfs_probe_signal(GED_DVFS_VSYNC_OFFSET_SIGNAL_EVENT);
	}

CHECK_OUT:    
	mutex_unlock(&gsVSyncOffsetLock);
	return ret;
}

void ged_dvfs_vsync_offset_level_set(int i32level)
{
	g_VsyncOffsetLevel = i32level;
	ged_log_trace_counter("vsync-offset",g_VsyncOffsetLevel);
}

int ged_dvfs_vsync_offset_level_get()
{
	return g_VsyncOffsetLevel;
}


GED_ERROR ged_dvfs_um_commit( unsigned long gpu_tar_freq, bool bFallback)
{
#ifdef ENABLE_COMMON_DVFS    
	int i32MaxLevel = 0;
	unsigned int  ui32NewFreqID;
	int i ;
	unsigned long gpu_freq ;

	unsigned int sentinalLoading=0;


#ifdef GED_DVFS_ENABLE
	unsigned int ui32CurFreqID;
	i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
#endif  
	if(g_gpu_timer_based_emu)
	{
		return GED_INTENTIONAL_BLOCK;
	}

#ifdef GED_DVFS_UM_CAL
	mutex_lock(&gsDVFSLock);

	if(gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us !=0)
	{
		sentinalLoading = (( gpu_loading * (gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)  ) + 100*gL_ulWorkingPeriod_us ) / (gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us); 
		if(sentinalLoading > 100)
		{
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] gpu_loading: %u g_ulPreCalResetTS_us:%lu",gpu_loading, gL_ulPreCalResetTS_us);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] g_ulWorkingPeriod_us: %lu",gL_ulWorkingPeriod_us);
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] gpu_av_loading: WTF");
			if(gL_ulWorkingPeriod_us==0)
					sentinalLoading = gpu_loading;
				else
					sentinalLoading = 100;
		}
		gpu_loading = sentinalLoading;		
	}
	else
	{
		ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] gpu_av_loading: 5566/ %u",gpu_loading);
		gpu_loading =0 ;
	}
	
	
	gpu_pre_loading = gpu_av_loading;
	gpu_av_loading = gpu_loading;


	g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;        

	if(gpu_tar_freq&0x1) // Magic to kill ged_srv
	{
		ged_dvfs_probe_signal(GED_SRV_SUICIDE_EVENT);
	}

	if(bFallback==true) // in the fallback mode, gpu_tar_freq taking as freq index
	{
		ged_dvfs_policy(gpu_loading, &ui32NewFreqID, 0, 0, 0, true);  
	}
	else
	{
		// Search suitable frequency level
		
		g_CommitType = MTK_GPU_DVFS_TYPE_VSYNCBASED;
		
		g_um_gpu_tar_freq = gpu_tar_freq;
		ui32NewFreqID = i32MaxLevel;
		for (i = 0; i <= i32MaxLevel; i++)
		{
#ifdef GED_DVFS_ENABLE
			gpu_freq = mt_gpufreq_get_freq_by_idx(i);
#endif
			if (gpu_tar_freq > gpu_freq)
			{
				if(i==0)
					ui32NewFreqID = 0;
				else
					ui32NewFreqID = i-1;
				break;
			}
		}
	}    


	ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] rdy to commit (%u)",ui32NewFreqID);

	g_computed_freq_id = ui32NewFreqID;
	ged_dvfs_gpu_freq_commit(ui32NewFreqID, GED_DVFS_DEFAULT_COMMIT);

	mutex_unlock(&gsDVFSLock);            
#endif
#else
	gpu_pre_loading = 0;
#endif

	return GED_OK;
}


static bool ged_dvfs_policy(
		unsigned int ui32GPULoading, unsigned int* pui32NewFreqID, 
		unsigned long t, long phase, unsigned long ul3DFenceDoneTime, bool bRefreshed)
{
#ifdef GED_DVFS_ENABLE    
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	unsigned int ui32GPUFreq = mt_gpufreq_get_cur_freq_index();
	unsigned int sentinalLoading = 0;

	int i32NewFreqID = (int)ui32GPUFreq;
	g_um_gpu_tar_freq = 0;
	if(false==bRefreshed)
	{
		
	
		if(gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us !=0)
		{
			sentinalLoading = (( gpu_loading * (gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)  ) + 100*gL_ulWorkingPeriod_us ) / (gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us); 
			
			if(sentinalLoading >100)
			{
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K1] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K1] gpu_loading: %u g_ulPreCalResetTS_us:%lu",gpu_loading, gL_ulPreCalResetTS_us);
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K1] g_ulWorkingPeriod_us: %lu",gL_ulWorkingPeriod_us);
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K1] gpu_av_loading: WTF");
				
				if(gL_ulWorkingPeriod_us==0)
					sentinalLoading = gpu_loading;
		else
					sentinalLoading = 100;
			}
			gpu_loading = sentinalLoading;
		}
		else
		{
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K1] gpu_av_loading: 5566 / %u",gpu_loading);
			gpu_loading = 0;
		}
		
		g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;        

		gpu_pre_loading = gpu_av_loading;
		ui32GPULoading = gpu_loading;
		gpu_av_loading = gpu_loading;
		
	}

	
	if(g_gpu_timer_based_emu) // conventional timer-based policy
	{
	if (ui32GPULoading >= 99)
	{
		i32NewFreqID = 0;
	}
	else if (ui32GPULoading <= 1)
	{
		i32NewFreqID = i32MaxLevel;
	}
	else if (ui32GPULoading >= 85)
	{
		i32NewFreqID -= 2;
	}
	else if (ui32GPULoading <= 30)
	{
		i32NewFreqID += 2;
	}
	else if (ui32GPULoading >= 70)
	{
		i32NewFreqID -= 1;
	}
	else if (ui32GPULoading <= 50)
	{
		i32NewFreqID += 1;
	}

	if (i32NewFreqID < ui32GPUFreq)
	{
		if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
		{
			i32NewFreqID -= 1;
		}
	}
	else if (i32NewFreqID > ui32GPUFreq)
	{
		if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
		{
			i32NewFreqID += 1;
		}
	}
		
		g_CommitType = MTK_GPU_DVFS_TYPE_TIMERBASED; 
	}
	else if(GED_DVFS_TIMER_BACKUP==phase) // easy to boost in offscreen cases
	{
		if (ui32GPULoading >= 50)
		{
			i32NewFreqID -= 1;
		}
		else if (ui32GPULoading <= 30)
	{
			i32NewFreqID += 1;
		}
		
		g_CommitType = MTK_GPU_DVFS_TYPE_TIMERBASED; 
	}
	else // vsync-based fallback mode
	{
			
		if (ui32GPULoading >= 70)
		{
			i32NewFreqID -= 1;
		}
		else if (ui32GPULoading <= 50)
		{
			i32NewFreqID += 1;
		}
		
		g_CommitType = MTK_GPU_DVFS_TYPE_FALLBACK; 
	}


	if (i32NewFreqID > i32MaxLevel)
	{
		i32NewFreqID = i32MaxLevel;
	}
	else if (i32NewFreqID < 0)
	{
		i32NewFreqID = 0;
	}

	*pui32NewFreqID = (unsigned int)i32NewFreqID;


	return *pui32NewFreqID != ui32GPUFreq ? GED_TRUE : GED_FALSE;
#else
	return GED_FALSE;
#endif    
}


static void ged_dvfs_freq_input_boostCB(unsigned int ui32BoostFreqID)
{
#ifdef GED_DVFS_ENABLE
	if (0 < g_iSkipCount)
	{
		return;
	}

	if (boost_gpu_enable == 0)
	{
		return;
	}

	mutex_lock(&gsDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index())
	{
		if (ged_dvfs_gpu_freq_commit(ui32BoostFreqID,GED_DVFS_INPUT_BOOST_COMMIT ))
		{
			g_dvfs_skip_round = GED_DVFS_SKIP_ROUNDS; // of course this must be fixed
		}
	}

	mutex_unlock(&gsDVFSLock);
#endif    
}

#ifdef GED_DVFS_ENABLE
static void ged_dvfs_freq_thermal_limitCB(unsigned int ui32LimitFreqID)
{

	if (0 < g_iSkipCount)
	{
		return;
	}

	if(ui32LimitFreqID == 0) // thermal event disable
		ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT , GED_FALSE);
	else
		ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT , GED_TRUE);

	mutex_lock(&gsDVFSLock);

	if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index())
	{
		if (ged_dvfs_gpu_freq_commit(ui32LimitFreqID, GED_DVFS_SET_LIMIT_COMMIT))
		{
			g_dvfs_skip_round = 0;//GED_DVFS_SKIP_ROUNDS; // of course this must be fixed
		}
	}

	mutex_unlock(&gsDVFSLock);

}
#endif      

void ged_dvfs_boost_gpu_freq(void)
{
	if (gpu_debug_enable)
	{
		GED_LOGE("%s", __func__);
	}
	ged_dvfs_freq_input_boostCB(0);
}

#ifdef GED_DVFS_ENABLE
static void ged_dvfs_set_bottom_gpu_freq(unsigned int ui32FreqLevel)
{


	unsigned int ui32MaxLevel;
	if (gpu_debug_enable)
	{
		GED_LOGE("%s: freq = %d", __func__,ui32FreqLevel);
	}

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
	{
		ui32FreqLevel = ui32MaxLevel;
	}

	mutex_lock(&gsDVFSLock);

	// 0 => The highest frequency
	// table_num - 1 => The lowest frequency
	g_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;

	gpu_bottom_freq = mt_gpufreq_get_freq_by_idx(g_bottom_freq_id);

	//if current id is larger, ie lower freq, we need to reflect immedately
	if(g_bottom_freq_id < mt_gpufreq_get_cur_freq_index()) 
		ged_dvfs_gpu_freq_commit(g_bottom_freq_id, GED_DVFS_SET_BOTTOM_COMMIT);

	mutex_unlock(&gsDVFSLock);

}


static unsigned int ged_dvfs_get_gpu_freq_level_count(void)
{

	return mt_gpufreq_get_dvfs_table_num();

}


static void ged_dvfs_custom_boost_gpu_freq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
	{
		GED_LOGE("%s: freq = %d", __func__ ,ui32FreqLevel);
	}

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
	{
		ui32FreqLevel = ui32MaxLevel;
	}

	mutex_lock(&gsDVFSLock);

	// 0 => The highest frequency
	// table_num - 1 => The lowest frequency
	g_cust_boost_freq_id = ui32MaxLevel - ui32FreqLevel;

	gpu_cust_boost_freq = mt_gpufreq_get_freq_by_idx(g_cust_boost_freq_id);


	if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index())
	{
		ged_dvfs_gpu_freq_commit(g_cust_boost_freq_id, GED_DVFS_CUSTOM_BOOST_COMMIT);
	}

	mutex_unlock(&gsDVFSLock);

}


static void ged_dvfs_custom_ceiling_gpu_freq(unsigned int ui32FreqLevel)
{

	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
	{
		GED_LOGE("%s: freq = %d", __func__,ui32FreqLevel);
	}

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
	{
		ui32FreqLevel = ui32MaxLevel;
	}

	mutex_lock(&gsDVFSLock);

	// 0 => The highest frequency
	// table_num - 1 => The lowest frequency
	g_cust_upbound_freq_id = ui32MaxLevel - ui32FreqLevel;

	gpu_cust_upbound_freq = mt_gpufreq_get_freq_by_idx(g_cust_upbound_freq_id);


	if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
	{
		ged_dvfs_gpu_freq_commit(g_cust_upbound_freq_id, GED_DVFS_CUSTOM_CEIL_COMMIT);
	}

	mutex_unlock(&gsDVFSLock);

}
#endif        

unsigned int ged_dvfs_get_custom_boost_gpu_freq(void)
{
#ifdef GED_DVFS_ENABLE
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
#else    
	unsigned int ui32MaxLevel = 0;
#endif           
	return ui32MaxLevel - g_cust_boost_freq_id;
}



void ged_dvfs_save_loading_page(void) // Need spinlocked
{
	gL_ulCalResetTS_us = g_ulCalResetTS_us;
	gL_ulPreCalResetTS_us = g_ulPreCalResetTS_us;
	gL_ulWorkingPeriod_us = g_ulWorkingPeriod_us;
	
	g_ulWorkingPeriod_us = 0; // set as zero for next time
}


void ged_dvfs_cal_gpu_utilization_force()
{
	unsigned long ui32IRQFlags;
	unsigned int loading;
	unsigned int block;
	unsigned int idle;
	unsigned long long t;
	unsigned long ulwork;


	t = ged_get_time();

	do_div(t,1000);

	ged_dvfs_cal_gpu_utilization(&loading, &block, &idle);

	
	spin_lock_irqsave(&g_sSpinLock, ui32IRQFlags);

	ulwork = (( t - g_ulCalResetTS_us ) * loading );
	do_div(ulwork, 100);

	g_ulWorkingPeriod_us += ulwork;

	g_ulPreCalResetTS_us = g_ulCalResetTS_us;
	g_ulCalResetTS_us = t;
	
	spin_unlock_irqrestore(&g_sSpinLock, ui32IRQFlags);
}

void ged_dvfs_run(unsigned long t, long phase, unsigned long ul3DFenceDoneTime)
{
	bool bError;	
	unsigned long ui32IRQFlags;
	//ged_profile_dvfs_record_SW_vsync(t, phase, ul3DFenceDoneTime);
	mutex_lock(&gsDVFSLock);

	//gpu_pre_loading = gpu_loading;

	if (0 == gpu_dvfs_enable)
	{
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block= 0;
		gpu_idle = 0;
		goto EXIT_ged_dvfs_run;         
	}

	// SKIP for keeping boost freq
	if(g_dvfs_skip_round>0)
	{
		g_dvfs_skip_round--;
		//goto EXIT_ged_dvfs_run;			
	}		

	if (g_iSkipCount > 0)
	{
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block= 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;
	}
	else
	{
		bError=ged_dvfs_cal_gpu_utilization(&gpu_loading, &gpu_block, &gpu_idle);
		spin_lock_irqsave(&g_sSpinLock,ui32IRQFlags);
		g_ulPreCalResetTS_us = g_ulCalResetTS_us;
		g_ulCalResetTS_us = t;
		
		ged_dvfs_save_loading_page();
		
		spin_unlock_irqrestore(&g_sSpinLock,ui32IRQFlags);

#ifdef GED_DVFS_UM_CAL        
		if(GED_DVFS_TIMER_BACKUP==phase) // timer-backup DVFS use only
#endif             
		{

			if (ged_dvfs_policy(gpu_loading, &g_ui32FreqIDFromPolicy, t, phase, ul3DFenceDoneTime, false))
			{
				g_computed_freq_id = g_ui32FreqIDFromPolicy;
				ged_dvfs_gpu_freq_commit(g_ui32FreqIDFromPolicy, GED_DVFS_DEFAULT_COMMIT);
			}

		}

	}

	if(gpu_debug_enable)
	{
#ifdef GED_DVFS_ENABLE	
		GED_LOGE("%s:gpu_loading=%d %d, g_iSkipCount=%d",__func__, gpu_loading, mt_gpufreq_get_cur_freq_index(), g_iSkipCount);
#endif        
	}

EXIT_ged_dvfs_run:
	mutex_unlock(&gsDVFSLock);
}

#ifdef GED_DVFS_ENABLE	
static unsigned int ged_dvfs_get_bottom_gpu_freq(void)
{

	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;


	return ui32MaxLevel - g_bottom_freq_id;
}


static unsigned int ged_dvfs_get_custom_ceiling_gpu_freq(void)
{

	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return ui32MaxLevel - g_cust_upbound_freq_id;
}

void ged_dvfs_sw_vsync_query_data(GED_DVFS_UM_QUERY_PACK* psQueryData)
{
	psQueryData->ui32GPULoading = gpu_loading;
	psQueryData->ui32GPUFreqID =  mt_gpufreq_get_cur_freq_index();
	psQueryData->gpu_cur_freq = mt_gpufreq_get_freq_by_idx(psQueryData->ui32GPUFreqID) ;
	psQueryData->gpu_pre_freq = mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
	psQueryData->nsOffset = ged_dvfs_vsync_offset_level_get();

	psQueryData->ulWorkingPeriod_us = gL_ulWorkingPeriod_us;
	psQueryData->ulPreCalResetTS_us = gL_ulPreCalResetTS_us;


}

static unsigned long ged_get_gpu_bottom_freq(void)
{
	return mt_gpufreq_get_freq_by_idx(g_bottom_freq_id);
}

static unsigned long ged_get_gpu_custom_boost_freq(void)
{
	return mt_gpufreq_get_freq_by_idx(g_cust_boost_freq_id);
}

static unsigned long ged_get_gpu_custom_upbound_freq(void)
{
	return mt_gpufreq_get_freq_by_idx(g_cust_upbound_freq_id);
}

#endif

void ged_dvfs_track_latest_record( MTK_GPU_DVFS_TYPE* peType, unsigned long *pulFreq)
{
	*peType = g_CommitType;
	*pulFreq = g_ulCommitFreq;
}

unsigned long ged_dvfs_get_gpu_tar_freq(void)
{
	return g_um_gpu_tar_freq;
}

unsigned int ged_dvfs_get_sub_gpu_loading(void)
{
	return gpu_sub_loading;
}


unsigned int ged_dvfs_get_gpu_loading(void)
{
	return gpu_av_loading;
}

unsigned int ged_dvfs_get_gpu_blocking(void)
{
	return gpu_block;
}


unsigned int ged_dvfs_get_gpu_idle(void)
{
	return 100 - gpu_av_loading;
}

void ged_dvfs_get_gpu_cur_freq(GED_DVFS_FREQ_DATA* psData)
{
#ifdef GED_DVFS_ENABLE    
	psData->ui32Idx = mt_gpufreq_get_cur_freq_index();    
	psData->ulFreq = mt_gpufreq_get_freq_by_idx(psData->ui32Idx);
#endif    
}

void ged_dvfs_get_gpu_pre_freq(GED_DVFS_FREQ_DATA* psData)
{
#ifdef GED_DVFS_ENABLE    
	psData->ui32Idx = g_ui32PreFreqID;
	psData->ulFreq = mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
#endif    
}





GED_ERROR ged_dvfs_probe_signal(int signo)
{
	int cache_pid=GED_NO_UM_SERVICE;
	struct task_struct *t=NULL;
	struct siginfo info;


	info.si_signo = signo;
	info.si_code = SI_QUEUE;
	info.si_int = 1234; 

	if(cache_pid!=g_probe_pid)
	{
		cache_pid = g_probe_pid;
		if(g_probe_pid==GED_NO_UM_SERVICE)
			t = NULL;
		else
			t = pid_task(find_vpid(g_probe_pid), PIDTYPE_PID); 
	}

	if(t!=NULL)
	{
		send_sig_info(signo, &info, t);
		ged_log_buf_print(ghLogBuf_ged_srv, "[GED_K] send signo %d to ged_srv [%d]",signo, g_probe_pid);
		return GED_OK;
	}
	else
	{
		g_probe_pid = GED_NO_UM_SERVICE;
		ged_log_buf_print(ghLogBuf_ged_srv, "[GED_K] ged_srv not running");
		return GED_ERROR_INVALID_PARAMS;
	}


}

void set_target_fps(int i32FPS)
{
	g_ulvsync_period = get_ns_period_from_fps(i32FPS);

}


unsigned long ged_gas_query_mode()
{
	if (g_ui32EventStatus & GED_EVENT_GAS)
		return GAS_CATEGORY_OTHERS;
	else
		return GAS_CATEGORY_GAME;
}


GED_ERROR ged_dvfs_probe(int pid)
{
	// lock here, wait vsync to relief
	//wait_for_completion(&gsVSyncOffsetLock);

	if(GED_VSYNC_OFFSET_NOT_SYNC ==pid)
	{
		g_ui32EventDebugStatus |= GED_EVENT_NOT_SYNC;
		return GED_OK;
	}

	if(GED_VSYNC_OFFSET_SYNC ==pid)
	{
		g_ui32EventDebugStatus &= (~GED_EVENT_NOT_SYNC);
		return GED_OK;
	}

	g_probe_pid = pid;

	/* clear bits among start */
	if(g_probe_pid!=GED_NO_UM_SERVICE)
	{
		g_ui32EventStatus &= (~GED_EVENT_TOUCH);
		g_ui32EventStatus &= (~GED_EVENT_WFD);
		g_ui32EventStatus &= (~GED_EVENT_GAS);

		g_ui32EventDebugStatus = 0;
	}

	ged_log_buf_print(ghLogBuf_ged_srv, "[GED_K] ged_srv pid: %d",g_probe_pid);

	return GED_OK;
}

GED_ERROR ged_dvfs_system_init()
{
	mutex_init(&gsDVFSLock);
	mutex_init(&gsVSyncOffsetLock);

	// initial as locked, signal when vsync_sw_notify    

	/* Removed. In ged timer-based dvfs device boot stage, 3D ged will set freq to highest level
	   with GED_DVFS_SET_BOTTOM_COMMIT, but common dvfs not work by g_iSkipCount!=0 */
	/* g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS; */

	g_ulvsync_period = get_ns_period_from_fps(60);


#ifdef GED_DVFS_ENABLE
	gpu_dvfs_enable = 1;
#else
	gpu_dvfs_enable = 0;
#endif	

	g_dvfs_skip_round = 0;

#ifdef GED_DVFS_ENABLE	
	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1; 
	gpu_bottom_freq = mt_gpufreq_get_freq_by_idx(g_bottom_freq_id);

	g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_cust_boost_freq = mt_gpufreq_get_freq_by_idx(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;
	gpu_cust_upbound_freq = mt_gpufreq_get_freq_by_idx(g_cust_upbound_freq_id);

#ifdef ENABLE_TIMER_BACKUP
	g_gpu_timer_based_emu=0;
#else	
	g_gpu_timer_based_emu=1;
#endif

	// GPU HAL fp mount	
	//mt_gpufreq_input_boost_notify_registerCB(ged_dvfs_freq_input_boostCB); // MTKFreqInputBoostCB
	mt_gpufreq_power_limit_notify_registerCB(ged_dvfs_freq_thermal_limitCB); // MTKFreqPowerLimitCB
	mtk_boost_gpu_freq_fp = ged_dvfs_boost_gpu_freq;
	mtk_set_bottom_gpu_freq_fp = ged_dvfs_set_bottom_gpu_freq;
	mtk_get_bottom_gpu_freq_fp = ged_dvfs_get_bottom_gpu_freq;
	mtk_custom_get_gpu_freq_level_count_fp = ged_dvfs_get_gpu_freq_level_count;
	mtk_custom_boost_gpu_freq_fp = ged_dvfs_custom_boost_gpu_freq;
	mtk_custom_upbound_gpu_freq_fp = ged_dvfs_custom_ceiling_gpu_freq;
	mtk_get_custom_boost_gpu_freq_fp = ged_dvfs_get_custom_boost_gpu_freq;
	mtk_get_custom_upbound_gpu_freq_fp = ged_dvfs_get_custom_ceiling_gpu_freq;
	mtk_get_gpu_loading_fp = ged_dvfs_get_gpu_loading;
	mtk_get_gpu_block_fp = ged_dvfs_get_gpu_blocking;
	mtk_get_gpu_idle_fp = ged_dvfs_get_gpu_idle;
	mtk_do_gpu_dvfs_fp = ged_dvfs_run;
	mtk_gpu_dvfs_set_mode_fp = ged_dvfs_set_tuning_mode_wrap;
	
	mtk_get_gpu_sub_loading_fp = ged_dvfs_get_sub_gpu_loading;
	mtk_get_vsync_based_target_freq_fp = ged_dvfs_get_gpu_tar_freq;
	mtk_get_gpu_dvfs_from_fp = ged_dvfs_track_latest_record;
	
	mtk_get_gpu_bottom_freq_fp = ged_get_gpu_bottom_freq;
	mtk_get_gpu_custom_boost_freq_fp = ged_get_gpu_custom_boost_freq;
	mtk_get_gpu_custom_upbound_freq_fp = ged_get_gpu_custom_upbound_freq;
	
	spin_lock_init(&g_sSpinLock); 

#endif	

	return GED_OK;
}

void ged_dvfs_system_exit()
{
	mutex_destroy(&gsDVFSLock);
	mutex_destroy(&gsVSyncOffsetLock);

}

#ifdef ENABLE_COMMON_DVFS	
module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(g_gpu_timer_based_emu, uint, 0644);
#endif

