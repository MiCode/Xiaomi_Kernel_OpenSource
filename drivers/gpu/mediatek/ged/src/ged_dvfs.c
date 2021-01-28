// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef GED_DVFS_STRESS_TEST
#include<linux/random.h>
#endif

#include <linux/slab.h>
#include <linux/sched.h>

#include <mt-plat/mtk_boot.h>
#ifdef MTK_GPU_DVFS
#include <mtk_gpufreq.h>
#else
#include "ged_gpufreq.h"
#endif

#undef CONFIG_MTK_QOS_V1_SUPPORT
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
#include <mtk_gpu_bw.h>
#endif

#include <mt-plat/mtk_gpu_utility.h>

#include <asm/siginfo.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

#include "disp_session.h"
#include "ged_dvfs.h"
#include "ged_monitor_3D_fence.h"
#include <ged_notify_sw_vsync.h>
#include "ged_log.h"
#include "ged_base.h"
#include "ged_global.h"

#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50

/* Definition of GED_DVFS_SKIP_ROUNDS is to skip DVFS when boost raised
 *  the value stands for counting down rounds of DVFS period
 *  Current using vsync that would be 16ms as period,
 *  below boost at (32, 48] seconds per boost
 */
#define GED_DVFS_SKIP_ROUNDS 3

#ifdef GED_ENABLE_FB_DVFS
spinlock_t gsGpuUtilLock;
#endif
static struct mutex gsDVFSLock;
static struct mutex gsVSyncOffsetLock;

static unsigned int g_iSkipCount;
static int g_dvfs_skip_round;

static unsigned int gpu_power;
static unsigned int gpu_dvfs_enable;
static unsigned int gpu_debug_enable;

enum MTK_GPU_DVFS_TYPE g_CommitType;
unsigned long g_ulCommitFreq;

#ifdef ENABLE_COMMON_DVFS
static unsigned int boost_gpu_enable;
static unsigned int gpu_bottom_freq;
static unsigned int gpu_cust_boost_freq;
static unsigned int gpu_cust_upbound_freq;
#endif

static unsigned int g_ui32PreFreqID;
static unsigned int g_bottom_freq_id;
static unsigned int g_last_def_commit_freq_id;
static unsigned int g_cust_upbound_freq_id;
static unsigned int g_cust_boost_freq_id;
static unsigned int g_computed_freq_id;

unsigned int g_gpu_timer_based_emu;
unsigned int gpu_bw_err_debug;

static unsigned int gpu_pre_loading;
unsigned int gpu_loading;
unsigned int gpu_av_loading;
unsigned int gpu_sub_loading;
unsigned int gpu_block;
unsigned int gpu_idle;
unsigned long g_um_gpu_tar_freq;

spinlock_t g_sSpinLock;
/* calculate loading reset time stamp */
unsigned long g_ulCalResetTS_us;
/* previous calculate loading reset time stamp */
unsigned long g_ulPreCalResetTS_us;
/* last frame half, t0 */
unsigned long g_ulWorkingPeriod_us;

/* record previous DVFS applying time stamp */
unsigned long g_ulPreDVFS_TS_us;

/* calculate loading reset time stamp */
static unsigned long gL_ulCalResetTS_us;
/* previous calculate loading reset time stamp */
static unsigned long gL_ulPreCalResetTS_us;
/* last frame half, t0 */
static unsigned long gL_ulWorkingPeriod_us;

static unsigned int g_loading2_count;
static unsigned int g_loading2_sum;
static DEFINE_SPINLOCK(load_info_lock);

static unsigned long g_policy_tar_freq;
static int g_mode;

static unsigned int g_ui32FreqIDFromPolicy;

struct GED_DVFS_BW_DATA gsBWprofile[MAX_BW_PROFILE];
int g_bw_head;
int g_bw_tail;

unsigned long g_ulvsync_period;
static GED_DVFS_TUNING_MODE g_eTuningMode;

unsigned int g_ui32EventStatus;
unsigned int g_ui32EventDebugStatus;
static int g_VsyncOffsetLevel;

static int g_probe_pid = GED_NO_UM_SERVICE;

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
#define DEFAULT_DVFS_STEP_MODE	0x0000 /* dvfs step =0, enlarge range= 0 */
unsigned int dvfs_step_mode = DEFAULT_DVFS_STEP_MODE;
static int init;
#endif

static unsigned int g_ui32TargetPeriod_us = 16666;
static unsigned int g_ui32BoostValue = 100;

static unsigned int ged_commit_freq;
static unsigned int ged_commit_opp_freq;

#ifdef GED_DVFS_STRESS_TEST
/***************************
 * GPU DVFS stress test
 * 0 : disable GPU DVFS stress test
 * 1 : enable GPU DVFS stress test for OPP Index
 * 2 : enable GPU DVFS stress test for fine-grained OPP
 ****************************/
static unsigned int ged_dvfs_stress_test;
module_param(ged_dvfs_stress_test, uint, 0644);
#endif

void ged_dvfs_last_and_target_cb(int t_gpu_target, int boost_accum_gpu)
{
	g_ui32TargetPeriod_us = t_gpu_target;
	g_ui32BoostValue = boost_accum_gpu;
	/* mtk_gpu_ged_hint(g_ui32TargetPeriod_us, g_ui32BoostValue); */
}

static bool ged_dvfs_policy(
	unsigned int ui32GPULoading, unsigned int *pui32NewFreqID,
	unsigned long t, long phase, unsigned long ul3DFenceDoneTime,
	bool bRefreshed);

static unsigned long ged_gas_query_mode(void);

struct ld_ud_table {
	int freq;
	int up;
	int down;
};
static struct ld_ud_table *loading_ud_table;

#ifdef GED_ENABLE_DVFS_LOADING_MODE
static int gx_dvfs_loading_mode = LOADING_ACTIVE;
struct GpuUtilization_Ex g_Util_Ex;
static int ged_get_dvfs_loading_mode(void);
#endif

#define GED_DVFS_TIMER_BASED_DVFS_MARGIN 30
static int gx_tb_dvfs_margin = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
static int gx_tb_dvfs_margin_cur = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
#define MAX_TB_DVFS_MARGIN               99
#define MIN_TB_DVFS_MARGIN               10
#define MIN_TB_MARGIN_INC_STEP           1
#define CONFIGURE_TIMER_BASED_MODE       0x00000000
#define DYNAMIC_TB_MASK                  0x00000100
#define DYNAMIC_TB_PIPE_TIME_MASK        0x00000200
#define DYNAMIC_TB_PERF_MODE_MASK        0x00000400
#define DYNAMIC_TB_FIX_TARGET_MASK       0x00000800
#define TIMER_BASED_MARGIN_MASK          0x000000ff
static int g_tb_dvfs_margin_value = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
static int g_tb_dvfs_margin_value_min = MIN_TB_DVFS_MARGIN;
static unsigned int g_tb_dvfs_margin_mode = CONFIGURE_TIMER_BASED_MODE;
#else
module_param(gx_tb_dvfs_margin, int, 0644);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */

static void _init_loading_ud_table(void)
{
	int i;
	int num = (int)mt_gpufreq_get_dvfs_table_num();

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	int temp = 0;

	temp = (dvfs_step_mode&0xff00)>>8;
#endif

	if (!loading_ud_table)
		loading_ud_table = ged_alloc(sizeof(struct ld_ud_table) * num);

	for (i = 0; i < num; ++i) {
		loading_ud_table[i].freq = mt_gpufreq_get_freq_by_idx(i);
		loading_ud_table[i].up = (100 - gx_tb_dvfs_margin_cur);
	}

	for (i = 0; i < num - 1; ++i) {
		int a = loading_ud_table[i].freq;
		int b = loading_ud_table[i+1].freq;

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
		loading_ud_table[i].down
			= ((100 - gx_tb_dvfs_margin_cur - temp) * b) / a;
#else
		loading_ud_table[i].down
			= ((100 - gx_tb_dvfs_margin_cur) * b) / a;
#endif
	}

	if (num >= 2)
		loading_ud_table[num-1].down = loading_ud_table[num-2].down;
}

unsigned long ged_query_info(GED_INFO eType)
{
	unsigned int gpu_loading;
	unsigned int gpu_block;
	unsigned int gpu_idle;

	gpu_loading = 0;
	gpu_idle = 0;
	gpu_block = 0;

	switch (eType) {
	case GED_LOADING:
		mtk_get_gpu_loading2(&gpu_loading, 1);
		return gpu_loading;
	case GED_IDLE:
		mtk_get_gpu_idle(&gpu_idle);
		return gpu_idle;
	case GED_BLOCKING:
		mtk_get_gpu_block(&gpu_block);
		return gpu_block;
	case GED_PRE_FREQ:
		return mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
	case GED_PRE_FREQ_IDX:
		return g_ui32PreFreqID;
	case GED_CUR_FREQ:
		return mt_gpufreq_get_freq_by_idx(
			mt_gpufreq_get_cur_freq_index());
	case GED_CUR_FREQ_IDX:
		return mt_gpufreq_get_cur_freq_index();
	case GED_MAX_FREQ_IDX:
		return mt_gpufreq_get_dvfs_table_num()-1;
	case GED_MAX_FREQ_IDX_FREQ:
		return mt_gpufreq_get_freq_by_idx(
			mt_gpufreq_get_dvfs_table_num()-1);
	case GED_MIN_FREQ_IDX:
		return 0;
	case GED_MIN_FREQ_IDX_FREQ:
		return mt_gpufreq_get_freq_by_idx(0);
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
		struct disp_session_info info;

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
#ifdef GED_ENABLE_DVFS_LOADING_MODE
void (*ged_dvfs_cal_gpu_utilization_ex_fp)(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle,
	void *Util_Ex) = NULL;
EXPORT_SYMBOL(ged_dvfs_cal_gpu_utilization_ex_fp);
#else
void (*ged_dvfs_cal_gpu_utilization_fp)(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle) = NULL;
EXPORT_SYMBOL(ged_dvfs_cal_gpu_utilization_fp);
#endif
//-----------------------------------------------------------------------------

#ifdef GED_ENABLE_DVFS_LOADING_MODE
bool ged_dvfs_cal_gpu_utilization_ex(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle,
	struct GpuUtilization_Ex *Util_Ex)
#else
bool ged_dvfs_cal_gpu_utilization(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle)
#endif
{
	unsigned long ui32IRQFlags;

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	if (ged_dvfs_cal_gpu_utilization_ex_fp != NULL) {
		ged_dvfs_cal_gpu_utilization_ex_fp(pui32Loading, pui32Block,
			pui32Idle, (void *) Util_Ex);

		memcpy((void *)&g_Util_Ex, (void *)Util_Ex,
			sizeof(struct GpuUtilization_Ex));
#else
	if (ged_dvfs_cal_gpu_utilization_fp != NULL) {
		ged_dvfs_cal_gpu_utilization_fp(pui32Loading,
			pui32Block, pui32Idle);
#endif
		if (pui32Loading) {
			ged_log_perf_trace_counter("gpu_loading",
				(long long)*pui32Loading,
				5566, 0, 0);
			gpu_av_loading = *pui32Loading;
			gpu_sub_loading = *pui32Loading;

			spin_lock_irqsave(&load_info_lock, ui32IRQFlags);
			g_loading2_sum += gpu_av_loading;
			g_loading2_count++;
			spin_unlock_irqrestore(&load_info_lock, ui32IRQFlags);
		}
		return true;
	}

	return false;
}

void ged_dvfs_get_bw_record(unsigned int *pui32MaxBW,
	unsigned int *pui32AvgBW, bool bFB)
{
	static unsigned int ui32LastPredictMaxBW;
	static unsigned int ui32LastPredictAvgBW;
	static unsigned int ui32LastActivePredictAvgBW;
	unsigned int ui32MaxBW = 0;
	unsigned int ui32AvgBW = 0;
	unsigned int ui32ActAvgBW = 0;
	uint64_t ui64MaxBW = 0;
	uint64_t ui64AvgBW = 0;
	int CurMaxInst = 0;
	int idx;


	/* compute BW */
	/* mt_gpufreq_BW_compute();
	 * reserve for experiment
	 */
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
	ui32MaxBW = mt_gpu_bw_get_BW(0);
	ui32AvgBW = 0;/* mt_gpu_bw_get_BW(1); This is reserved for experiment */
#endif
	if (gpu_av_loading)
		ui32ActAvgBW = 100 * ui32AvgBW / gpu_av_loading;


	/*
	 * If Frame-based is applicable, use previous records
	 * (up to MAX_BW_PROFILE-1) as base line BW
	 */
	if (bFB && g_bw_tail >= 0) {
		gsBWprofile[g_bw_tail].ui32MaxBW = ui32MaxBW;
		gsBWprofile[g_bw_tail].ui32AvgBW = ui32AvgBW;

		g_bw_tail = (g_bw_tail + 1) % MAX_BW_PROFILE;


		/* begin to calculate from profiled */

		while ((g_bw_head + CurMaxInst)%MAX_BW_PROFILE != g_bw_tail) {
			idx = g_bw_head + CurMaxInst;
			if (gsBWprofile[(idx % MAX_BW_PROFILE)].ui32MaxBW >
				ui64MaxBW) {
				ui64MaxBW =
				gsBWprofile[(idx % MAX_BW_PROFILE)].ui32MaxBW;
			}

			ui64AvgBW +=
			gsBWprofile[(idx % MAX_BW_PROFILE)].ui32AvgBW;
			CurMaxInst++;
		}

		/* Find Max BW among previous four inst */
		ui32MaxBW = ui64MaxBW;
		/* sampling average as GPU DVFS did (not used) */
		/* ui32AvgBW = ui64AvgBW / cur_max_inst; */

		if ((g_bw_tail + 1)%MAX_BW_PROFILE == g_bw_head) {
			/* queue full, drop head */
			g_bw_head = (g_bw_head + 1) % MAX_BW_PROFILE;
		}

/* Reserved for debug
 * GED_LOGD("@%s: Frame-based: ui64MaxBW:%llu, ui64AvgBW:%llu h/t %u/%u\n",
 * __func__, ui64MaxBW, ui64AvgBW, g_bw_head, g_bw_tail);
 * GED_LOGD("@%s: Frame-based: ui32MaxBW:%u, ui32AvgBW:%u, inst:%d\n",
 * __func__, ui32MaxBW, ui32AvgBW, cur_max_inst);
 */
	} else {
		g_bw_head = 0;
		g_bw_tail = 0;
	}

	if (pui32MaxBW)
		*pui32MaxBW = ui32MaxBW;

	if (pui32AvgBW)
		*pui32AvgBW = ui32AvgBW;

	ui32LastPredictMaxBW = ui32MaxBW;
	ui32LastPredictAvgBW = ui32AvgBW;
	ui32LastActivePredictAvgBW = ui32ActAvgBW;
}

unsigned int gpu_bw_ratio;
/* return BW with MB/sec */
unsigned int ged_dvfs_vcore(unsigned int prev_freq_khz,
	unsigned int cur_freq_khz, bool bFB)
{
	unsigned int prev_freq_mhz;
	unsigned int g_ui32NextAvBW;
	unsigned int g_ui32NextMaxBW;
	unsigned int g_ui32CurAvBW;
	unsigned int g_ui32CurMaxBW;
	unsigned int cur_freq_mhz;

	cur_freq_mhz = cur_freq_khz / 1000; /* to MHz */
	prev_freq_mhz = prev_freq_khz / 1000;

	ged_dvfs_get_bw_record(&g_ui32CurMaxBW, &g_ui32CurAvBW, bFB);

	/* TO-DO: On S_Y_L_V_I_A, this seems no need,
	 * since we could get MB already
	 *  BW = bw_reg * 8 / 1024 / 1024 to get MB
	 *  and since sampling time is 1ms, * 1000 to get per second BW
	 */

	/* g_ui32CurMaxBW = g_ui32CurMaxBW * 1000 >> 17;
	 * g_ui32CurAvBW = g_ui32CurAvBW * 1000 >> 17;
	 * idle exclusion
	 */
	if (gpu_av_loading)
		g_ui32CurAvBW = 100 * g_ui32CurAvBW / gpu_av_loading;

	if (prev_freq_mhz == 0)
		prev_freq_mhz = cur_freq_mhz;

	g_ui32NextMaxBW = g_ui32CurMaxBW * cur_freq_mhz / (prev_freq_mhz);
	g_ui32NextAvBW = g_ui32CurAvBW * cur_freq_mhz / prev_freq_mhz;


	/* Reserved for debug
	 * GED_LOGD("@%s: Freq(%d): %u/%u avgBW: %u/%u maxBW: %u/%u\n"
	 * , __func__, bFB,cur_freq_mhz,
	 * prev_freq_mhz, g_ui32NextAvBW, g_ui32CurAvBW, g_ui32NextMaxBW,
	 * g_ui32CurMaxBW);
	 */
	if (gpu_bw_err_debug)
		GED_LOGD("@%s: Freq(%d): %u/%u maxBW: %u/%u\n", __func__,
			bFB, cur_freq_mhz,
		prev_freq_mhz, g_ui32NextMaxBW, g_ui32CurMaxBW);

	prev_freq_mhz = cur_freq_mhz;

	if (gpu_bw_ratio)
		return (g_ui32NextMaxBW * gpu_bw_ratio) / 100;
	else
		return g_ui32NextAvBW;
}

/*-----------------------------------------------------------------------------
 * void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID)
 * call back function
 * This shall be registered in vendor's GPU driver,
 * since each IP has its own rule
 */
static unsigned long g_ged_dvfs_commit_idx; /* max freq opp idx */
void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID,
	GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited) = NULL;
EXPORT_SYMBOL(ged_dvfs_gpu_freq_commit_fp);

unsigned long ged_dvfs_get_last_commit_idx(void)
{
	return g_ged_dvfs_commit_idx;
}

bool ged_dvfs_gpu_freq_commit(unsigned long ui32NewFreqID,
	unsigned long ui32NewFreq, GED_DVFS_COMMIT_TYPE eCommitType)
{
	int bCommited = false;
	unsigned long ui32CurFreqID;

	ui32CurFreqID = mt_gpufreq_get_cur_freq_index();
	if (eCommitType == GED_DVFS_DEFAULT_COMMIT)
		g_last_def_commit_freq_id = ui32NewFreqID;
	if (ged_dvfs_gpu_freq_commit_fp != NULL) {

		if (ui32NewFreqID > g_bottom_freq_id) {
			ui32NewFreqID = g_bottom_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_SMARTBOOST;
			ged_log_perf_trace_counter("gpu_api_boost",
				(long long)g_bottom_freq_id,
				5566, 0, 0);
		}

		if (ui32NewFreqID > g_cust_boost_freq_id) {
			ui32NewFreqID = g_cust_boost_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_CUSTOMIZATION;
			ged_log_perf_trace_counter("gpu_cust_floor",
				(long long)g_cust_boost_freq_id,
				5566, 0, 0);
		}

		/* up bound */
		if (ui32NewFreqID < g_cust_upbound_freq_id) {
			ui32NewFreqID = g_cust_upbound_freq_id;
			g_CommitType = MTK_GPU_DVFS_TYPE_CUSTOMIZATION;
			ged_log_perf_trace_counter("gpu_cust_ceiling",
				(long long)g_cust_upbound_freq_id,
				5566, 0, 0);
		}

		/* thermal power limit */
		if (ui32NewFreqID < mt_gpufreq_get_thermal_limit_index()) {
			ui32NewFreqID = mt_gpufreq_get_thermal_limit_index();
			g_CommitType = MTK_GPU_DVFS_TYPE_THERMAL;
			ged_log_perf_trace_counter("gpu_limit_idx",
				(long long)
				mt_gpufreq_get_thermal_limit_index(),
				5566, 0, 0);
		}

		g_ulCommitFreq = mt_gpufreq_get_freq_by_idx(ui32NewFreqID);

#ifdef GED_DVFS_STRESS_TEST
		if (ged_dvfs_stress_test == 1) {
			get_random_bytes(&ui32NewFreqID, sizeof(ui32NewFreqID));
			ui32NewFreqID = (ui32NewFreqID) % 16;
		}
#endif

		ged_commit_freq = ui32NewFreq;
		ged_commit_opp_freq = mt_gpufreq_get_freq_by_idx(ui32NewFreqID);

		/* do change */
		if (ui32NewFreqID != ui32CurFreqID) {
			/* call to DVFS module */
			g_ged_dvfs_commit_idx = ui32NewFreqID;
			ged_dvfs_gpu_freq_commit_fp(ui32NewFreqID, eCommitType,
				&bCommited);
			/*
			 * To-Do: refine previous freq contributions,
			 * since it is possible to have multiple freq settings
			 * in previous execution period
			 * Does this fatal for precision?
			 */
			ged_log_buf_print2(ghLogBuf_DVFS, GED_LOG_ATTR_TIME,
		"[GED_K] new freq ID committed: idx=%lu type=%u, g_type=%u",
				ui32NewFreqID, eCommitType, g_CommitType);
			if (bCommited == true) {
				ged_log_buf_print(ghLogBuf_DVFS,
					"[GED_K] committed true");
				g_ui32PreFreqID = ui32CurFreqID;
			}
		}
		ged_log_perf_trace_counter("gpu_freq",
			(long long)(mt_gpufreq_get_cur_freq() / 1000),
			5566, 0, 0);
		ged_log_perf_trace_counter("gpu_freq_max",
			(long long)(mt_gpufreq_get_freq_by_idx(
			mt_gpufreq_get_cur_ceiling_idx()) / 1000),
			5566, 0, 0);
	}

	return bCommited;
}

unsigned long get_ns_period_from_fps(unsigned int ui32Fps)
{
	return 1000000/ui32Fps;
}

void ged_dvfs_set_tuning_mode(GED_DVFS_TUNING_MODE eMode)
{
	g_eTuningMode = eMode;
}

void ged_dvfs_set_tuning_mode_wrap(int eMode)
{
	ged_dvfs_set_tuning_mode((GED_DVFS_TUNING_MODE)eMode);
}

GED_DVFS_TUNING_MODE ged_dvfs_get_tuning_mode(void)
{
	return g_eTuningMode;
}

GED_ERROR ged_dvfs_vsync_offset_event_switch(
	GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent, bool bSwitch)
{
	unsigned int ui32BeforeSwitchInterpret;
	unsigned int ui32BeforeDebugInterpret;
	GED_ERROR ret = GED_OK;

	mutex_lock(&gsVSyncOffsetLock);

	ui32BeforeSwitchInterpret = g_ui32EventStatus;
	ui32BeforeDebugInterpret = g_ui32EventDebugStatus;

	switch (eEvent) {
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
		/* touch boost */
#ifdef ENABLE_COMMON_DVFS
		if (bSwitch == GED_TRUE)
			ged_dvfs_boost_gpu_freq();
#endif

		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_TOUCH) :
			(g_ui32EventStatus &= (~GED_EVENT_TOUCH));
		break;
	case GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_THERMAL) :
			(g_ui32EventStatus &= (~GED_EVENT_THERMAL));
		break;
	case GED_DVFS_VSYNC_OFFSET_WFD_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_WFD) :
			(g_ui32EventStatus &= (~GED_EVENT_WFD));
		break;
	case GED_DVFS_VSYNC_OFFSET_MHL_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_MHL) :
			(g_ui32EventStatus &= (~GED_EVENT_MHL));
		break;
	case GED_DVFS_VSYNC_OFFSET_VR_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VR) :
			(g_ui32EventStatus &= (~GED_EVENT_VR));
		break;
	case GED_DVFS_VSYNC_OFFSET_DHWC_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_DHWC) :
			(g_ui32EventStatus &= (~GED_EVENT_DHWC));
		break;
	case GED_DVFS_VSYNC_OFFSET_GAS_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_GAS) :
			(g_ui32EventStatus &= (~GED_EVENT_GAS));
		ged_monitor_3D_fence_set_enable(!bSwitch);
		ret = ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_LOW_POWER_MODE) :
			(g_ui32EventStatus &= (~GED_EVENT_LOW_POWER_MODE));
		ret = ged_dvfs_probe_signal(GED_LOW_POWER_MODE_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_MHL4K_VID) :
			(g_ui32EventStatus &= (~GED_EVENT_MHL4K_VID));
		ret = ged_dvfs_probe_signal(GED_MHL4K_VID_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VILTE_VID) :
			(g_ui32EventStatus &= (~GED_EVENT_VILTE_VID));
		ret = ged_dvfs_probe_signal(GED_VILTE_VID_SIGNAL_EVENT);
		break;
	case GED_DVFS_BOOST_HOST_EVENT:
		ret = ged_dvfs_probe_signal(GED_SIGNAL_BOOST_HOST_EVENT);
		goto CHECK_OUT;

	case GED_DVFS_VSYNC_OFFSET_LOW_LATENCY_MODE_EVENT:
		(bSwitch) ?
		(g_ui32EventStatus |= GED_EVENT_LOW_LATENCY_MODE) :
		(g_ui32EventStatus &= (~GED_EVENT_LOW_LATENCY_MODE));
		ret = ged_dvfs_probe_signal
		(GED_LOW_LATENCY_MODE_SIGNAL_EVENT);
		break;
	default:
		GED_LOGE("%s: not acceptable event:%u\n", __func__, eEvent);
		ret = GED_ERROR_INVALID_PARAMS;
		goto CHECK_OUT;
	}
	mtk_ged_event_notify(g_ui32EventStatus);

	if ((ui32BeforeSwitchInterpret != g_ui32EventStatus) ||
		(ui32BeforeDebugInterpret != g_ui32EventDebugStatus) ||
		(g_ui32EventDebugStatus & GED_EVENT_NOT_SYNC))
		ret = ged_dvfs_probe_signal(GED_DVFS_VSYNC_OFFSET_SIGNAL_EVENT);

CHECK_OUT:
	mutex_unlock(&gsVSyncOffsetLock);
	return ret;
}

void ged_dvfs_vsync_offset_level_set(int i32level)
{
	g_VsyncOffsetLevel = i32level;
}

int ged_dvfs_vsync_offset_level_get(void)
{
	return g_VsyncOffsetLevel;
}


GED_ERROR ged_dvfs_um_commit(unsigned long gpu_tar_freq, bool bFallback)
{
#ifdef ENABLE_COMMON_DVFS
	int i32MaxLevel = 0;
	unsigned int ui32NewFreqID;
	int i;
	unsigned long gpu_freq;
	unsigned int sentinalLoading = 0;
	unsigned int ui32CurFreqID;

	unsigned long ui32IRQFlags;

	i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	ui32CurFreqID = mt_gpufreq_get_cur_freq_index();

	if (g_gpu_timer_based_emu)
		return GED_ERROR_INTENTIONAL_BLOCK;

#ifdef GED_DVFS_UM_CAL
	mutex_lock(&gsDVFSLock);

	if (gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us != 0) {
		sentinalLoading =
		((gpu_loading * (gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)) +
		100 * gL_ulWorkingPeriod_us) /
		(gL_ulCalResetTS_us - g_ulPreDVFS_TS_us);
		if (sentinalLoading > 100) {
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",
				gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] gpu_loading: %u g_ulPreCalResetTS_us:%lu",
				gpu_loading, gL_ulPreCalResetTS_us);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] g_ulWorkingPeriod_us: %lu",
				gL_ulWorkingPeriod_us);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] gpu_av_loading: WTF");

			if (gL_ulWorkingPeriod_us == 0)
				sentinalLoading = gpu_loading;
			else
				sentinalLoading = 100;
		}
		gpu_loading = sentinalLoading;
	} else {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] gpu_av_loading: 5566/ %u", gpu_loading);
		gpu_loading = 0;
	}

	gpu_pre_loading = gpu_av_loading;
	gpu_av_loading = gpu_loading;

	spin_lock_irqsave(&load_info_lock, ui32IRQFlags);
	g_loading2_sum += gpu_loading;
	g_loading2_count += 1;
	spin_unlock_irqrestore(&load_info_lock, ui32IRQFlags);

#ifdef GED_SSPM
	mt_gpufreq_set_loading(gpu_av_loading);
#endif

	g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;

    /* Magic to kill ged_srv */
	if (gpu_tar_freq & 0x1)
		ged_dvfs_probe_signal(GED_SRV_SUICIDE_EVENT);

	if (bFallback == true) /* fallback mode, gpu_tar_freq is freq idx */
		ged_dvfs_policy(gpu_loading, &ui32NewFreqID, 0, 0, 0, true);
	else {
#ifdef GED_DVFS_STRESS_TEST
		if (ged_dvfs_stress_test == 2)
			gpu_tar_freq  = get_random_int() %
				(ged_query_info(GED_MIN_FREQ_IDX_FREQ) -
				ged_query_info(GED_MAX_FREQ_IDX_FREQ) + 1) +
				ged_query_info(GED_MAX_FREQ_IDX_FREQ);
#endif

		/* Search suitable frequency level */
		g_CommitType = MTK_GPU_DVFS_TYPE_VSYNCBASED;
		g_um_gpu_tar_freq = gpu_tar_freq;
		g_policy_tar_freq = g_um_gpu_tar_freq;
		g_mode = 1;

		ui32NewFreqID = i32MaxLevel;
		for (i = 0; i <= i32MaxLevel; i++) {
			gpu_freq = mt_gpufreq_get_freq_by_idx(i);

			if (gpu_tar_freq > gpu_freq) {
				if (i == 0)
					ui32NewFreqID = 0;
				else
					ui32NewFreqID = i-1;
				break;
			}
		}
	}

	ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] rdy to commit (%u)", ui32NewFreqID);

	g_computed_freq_id = ui32NewFreqID;
	if (bFallback == true)
		ged_dvfs_gpu_freq_commit(ui32NewFreqID,
				mt_gpufreq_get_freq_by_idx(ui32NewFreqID),
				GED_DVFS_DEFAULT_COMMIT);
	else
		ged_dvfs_gpu_freq_commit(ui32NewFreqID, gpu_tar_freq,
				GED_DVFS_DEFAULT_COMMIT);


	mutex_unlock(&gsDVFSLock);
#endif /* GED_DVFS_UM_CAL */
#else
	gpu_pre_loading = 0;
#endif /* ENABLE_COMMON_DVFS */

	return GED_OK;
}

#ifdef GED_ENABLE_FB_DVFS
#define DEFAULT_DVFS_MARGIN 100 /* 10% margin */
#define FIXED_FPS_MARGIN 3 /* Fixed FPS margin: 3fps */

int gx_fb_dvfs_margin = DEFAULT_DVFS_MARGIN;/* 10-bias */

#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN
#define MAX_DVFS_MARGIN 990 /* 99 % margin */
#define MIN_DVFS_MARGIN 10 /* 1% margin */

/* dynamic margin mode for FPSGo control fps margin */
#define DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN 0x10

/* dynamic margin mode for fixed fps margin */
#define DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN 0x11

/* dynamic margin mode, margin low bound 1% */
#define DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN 0x12

/* dynamic margin mode, extend-config step and low bound */
#define DYNAMIC_MARGIN_MODE_EXTEND 0x13

/* dynamic margin mode, performance */
#define DYNAMIC_MARGIN_MODE_PERF 0x14

/* configure margin mode */
#define CONFIGURE_MARGIN_MODE 0x00

/* variable margin mode OPP Iidx */
#define VARIABLE_MARGIN_MODE_OPP_INDEX 0x01

#define MIN_MARGIN_INC_STEP 1 /* 1% headroom */

static int dvfs_margin_value = DEFAULT_DVFS_MARGIN/10;
unsigned int dvfs_margin_mode = CONFIGURE_MARGIN_MODE;

static int dvfs_min_margin_inc_step = MIN_MARGIN_INC_STEP;
static int dvfs_margin_low_bound = 1; /* 1% headroom */
#endif

module_param(gx_fb_dvfs_margin, int, 0644);
#define GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM 4
#define GED_FB_DVFS_FERQ_DROP_RATIO_LIMIT 70
static int is_fb_dvfs_triggered;
static int is_fallback_mode_triggered;

static int ged_dvfs_is_fallback_mode_triggered(void)
{
	return is_fallback_mode_triggered;
}

static void ged_dvfs_trigger_fb_dvfs(void)
{
	is_fb_dvfs_triggered = 1;
}
/*
 *	t_gpu, t_gpu_target in ms * 10
 */
static int ged_dvfs_fb_gpu_dvfs(int t_gpu, int t_gpu_target,
	int target_fps_margin, unsigned int force_fallback)
{
	int i, i32MaxLevel, gpu_freq_tar, ui32NewFreqID = 0;
	int ret_freq = -1;
	static int gpu_freq_pre = -1;
	static int num_pre_frames;
	static int cur_frame_idx;
	static int pre_frame_idx;
	static int busy_cycle[GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM];
	int gpu_busy_cycle = 0;
	int busy_cycle_cur;
	unsigned long ui32IRQFlags;
	static int force_fallback_pre;

#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN
	static int margin_low_bound;
#endif

	if (gpu_dvfs_enable == 0) {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][FB_DVFS] skip %s due to gpu_dvfs_enable=%u",
			__func__, gpu_dvfs_enable);
		gpu_freq_pre = ret_freq = mt_gpufreq_get_cur_freq();
		goto FB_RET;
	}

	if (force_fallback_pre != force_fallback) {
		force_fallback_pre = force_fallback;
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
		if (force_fallback == 1) {
			int i32NewFreqID =
			(int) mt_gpufreq_get_cur_freq_index();

			if (dvfs_step_mode == 0)
				i32NewFreqID = 0;
			else
				i32NewFreqID -= (dvfs_step_mode&0xff);

			if (i32NewFreqID < 0)
				i32NewFreqID = 0;

			ged_dvfs_gpu_freq_commit((unsigned long)i32NewFreqID
			, mt_gpufreq_get_freq_by_idx((unsigned long)
			i32NewFreqID)
			, GED_DVFS_DEFAULT_COMMIT);
		}
#else
		if (force_fallback == 1)
			ged_dvfs_gpu_freq_commit(0
				, mt_gpufreq_get_freq_by_idx(0)
				, GED_DVFS_DEFAULT_COMMIT);
#endif

	}
	if (force_fallback) {
		gpu_freq_pre = ret_freq = mt_gpufreq_get_cur_freq();
		goto FB_RET;
	}

	t_gpu /= 100000;
	t_gpu_target /= 100000;

	spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);
	if (is_fallback_mode_triggered)
		is_fallback_mode_triggered = 0;
	spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);

	if (t_gpu <= 0) {
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K][FB_DVFS] skip DVFS due to t_gpu <= 0, t_gpu: %d"
			, t_gpu);
		gpu_freq_pre = ret_freq = mt_gpufreq_get_cur_freq();
		goto FB_RET;
	}
	ged_cancel_backup_timer();


#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN

	/* configure margin mode */
	if (dvfs_margin_mode == CONFIGURE_MARGIN_MODE)
		gx_fb_dvfs_margin = dvfs_margin_value*10; /* 10-bias */

	if (dvfs_margin_mode & 0x10) {
		/* dvfs_margin_mode == */
		/* DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN or */
		/* DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN) or */
		/* DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN or */
		/* DYNAMIC_MARGIN_MODE_EXTEND or */
		/* DYNAMIC_MARGIN_MODE_PERF */

		int t_gpu_target_hd;/* apply headroom target */

		if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_PERF)
			t_gpu_target_hd = t_gpu_target *
				(1000 - gx_fb_dvfs_margin) / 1000;
		else
			t_gpu_target_hd = t_gpu_target;

		if (t_gpu > t_gpu_target_hd) { /* must set to max. margin */
			int temp;

			temp = (gx_fb_dvfs_margin*(t_gpu-t_gpu_target_hd))
				/t_gpu_target_hd;

			if (temp < dvfs_min_margin_inc_step*10)
				temp = dvfs_min_margin_inc_step*10;

			gx_fb_dvfs_margin += temp;

			if (gx_fb_dvfs_margin > (dvfs_margin_value*10))
				gx_fb_dvfs_margin = dvfs_margin_value*10;
		} else {
			if (dvfs_margin_mode
				== DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN)
				margin_low_bound = MIN_DVFS_MARGIN;
			else {
				int target_time_low_bound;

				if (dvfs_margin_mode ==
					DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN)
					target_fps_margin = FIXED_FPS_MARGIN;

				if (target_fps_margin == 0)
					margin_low_bound = MIN_DVFS_MARGIN;
				else {
					target_time_low_bound =
					10000/((10000/t_gpu_target) +
					target_fps_margin);

					margin_low_bound =
					1000 *
					(t_gpu_target - target_time_low_bound)
					/ t_gpu_target;
				}

				if (margin_low_bound > dvfs_margin_value*10)
					margin_low_bound =
					dvfs_margin_value*10;

				if (margin_low_bound < dvfs_margin_low_bound*10)
					margin_low_bound =
					dvfs_margin_low_bound*10;
			}

		gx_fb_dvfs_margin -=
		((gx_fb_dvfs_margin*(t_gpu_target_hd-t_gpu))/t_gpu_target_hd);

		if (gx_fb_dvfs_margin < margin_low_bound)
			gx_fb_dvfs_margin = margin_low_bound;
		}
	}
#endif

	t_gpu_target = t_gpu_target * (1000 - gx_fb_dvfs_margin) / 1000;
	i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	gpu_freq_pre = mt_gpufreq_get_cur_freq() >> 10;

	busy_cycle_cur = t_gpu * gpu_freq_pre;
	busy_cycle[cur_frame_idx] = busy_cycle_cur;
	if (num_pre_frames != GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM - 1) {
		gpu_busy_cycle = busy_cycle[cur_frame_idx];
		num_pre_frames++;
	} else {
		for (i = 0; i < GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM; i++)
			gpu_busy_cycle += busy_cycle[i];
		gpu_busy_cycle /= GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM;
		gpu_busy_cycle = (gpu_busy_cycle > busy_cycle_cur) ?
			gpu_busy_cycle : busy_cycle_cur;
	}
	gpu_freq_tar = (gpu_busy_cycle / t_gpu_target);
	if (gpu_freq_tar * 100
		< GED_FB_DVFS_FERQ_DROP_RATIO_LIMIT * gpu_freq_pre) {
		gpu_freq_tar = gpu_freq_pre;
		gpu_freq_tar *= GED_FB_DVFS_FERQ_DROP_RATIO_LIMIT;
		gpu_freq_tar /= 100;
	}
	gpu_freq_tar = gpu_freq_tar << 10;
	pre_frame_idx = cur_frame_idx;
	cur_frame_idx = (cur_frame_idx + 1) %
		GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM;

	ui32NewFreqID = i32MaxLevel;
	for (i = 0; i <= i32MaxLevel; i++) {
		int gpu_freq;

		gpu_freq = mt_gpufreq_get_freq_by_idx(i);

		if (gpu_freq_tar > gpu_freq) {
			if (i == 0)
				ui32NewFreqID = 0;
			else
				ui32NewFreqID = i-1;
			break;
		}
	}

#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN
	if (dvfs_margin_mode == VARIABLE_MARGIN_MODE_OPP_INDEX)
		gx_fb_dvfs_margin = (ui32NewFreqID / 3)*10;
#endif

	gpu_freq_pre = gpu_freq_pre << 10;
#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN
	ged_log_buf_print(ghLogBuf_DVFS,
"[GED_K][FB_DVFS]t_gpu:%d,t_gpu_tar:%d,gpu_freq_tar:%d,gpu_freq_pre:%d",
	t_gpu, t_gpu_target, gpu_freq_tar, gpu_freq_pre);

	ged_log_buf_print(ghLogBuf_DVFS,
"[GED_K][FB_DVFS]mode:%x,h:%d,margin:%d,l:%d,fps:+%d,l_b:%d,step:%d",
	dvfs_margin_mode, dvfs_margin_value, gx_fb_dvfs_margin,
	margin_low_bound, target_fps_margin, dvfs_margin_low_bound,
	dvfs_min_margin_inc_step);
#else
	ged_log_buf_print(ghLogBuf_DVFS,
"[GED_K][FB_DVFS] FB DVFS mode, t_gpu: %d, t_gpu_target: %d, gpu_freq_tar: %d, gpu_freq_pre: %d"
		, t_gpu, t_gpu_target, gpu_freq_tar, gpu_freq_pre);
#endif
	g_CommitType = MTK_GPU_DVFS_TYPE_VSYNCBASED;
	ged_dvfs_gpu_freq_commit((unsigned long)ui32NewFreqID,
		gpu_freq_tar, GED_DVFS_DEFAULT_COMMIT);

	ret_freq = gpu_freq_tar;
FB_RET:
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
	mt_gpu_bw_qos_vcore(ged_dvfs_vcore(gpu_freq_pre,
		mt_gpufreq_get_cur_freq(), true));
#endif
	is_fb_dvfs_triggered = 0;
	return ret_freq;
}
#endif

static int _loading_avg(int ui32loading)
{
	static int data[4];
	static int idx;
	static int sum;

	int cidx = ++idx % ARRAY_SIZE(data);

	sum += ui32loading - data[cidx];
	data[cidx] = ui32loading;

	return sum / ARRAY_SIZE(data);
}

static bool ged_dvfs_policy(
		unsigned int ui32GPULoading, unsigned int *pui32NewFreqID,
		unsigned long t, long phase, unsigned long ul3DFenceDoneTime,
		bool bRefreshed)
{
	int i32MaxLevel = (int)(mt_gpufreq_get_dvfs_table_num() - 1);
	unsigned int ui32GPUFreq = mt_gpufreq_get_cur_freq_index();
	unsigned int sentinalLoading = 0;
	unsigned int ui32GPULoading_avg;

	int i32NewFreqID = (int)ui32GPUFreq;

	unsigned long ui32IRQFlags;

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	int loading_mode;
#endif
	g_um_gpu_tar_freq = 0;
	if (bRefreshed == false) {
		if (gL_ulCalResetTS_us - g_ulPreDVFS_TS_us != 0) {
			sentinalLoading = ((gpu_loading *
				(gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)) +
				100 * gL_ulWorkingPeriod_us) /
				(gL_ulCalResetTS_us - g_ulPreDVFS_TS_us);

			if (sentinalLoading > 100) {
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",
					gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] gpu_loading: %u g_ulPreCalResetTS_us:%lu",
					gpu_loading, gL_ulPreCalResetTS_us);
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] g_ulWorkingPeriod_us: %lu",
					gL_ulWorkingPeriod_us);
				ged_log_buf_print(ghLogBuf_DVFS,
						"[GED_K1] gpu_av_loading: WTF");

				if (gL_ulWorkingPeriod_us == 0)
					sentinalLoading = gpu_loading;
				else
					sentinalLoading = 100;
			}
			gpu_loading = sentinalLoading;
		} else {
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K1] gpu_av_loading: 5566 / %u", gpu_loading);
			gpu_loading = 0;
		}

		g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;

		gpu_pre_loading = gpu_av_loading;
		ui32GPULoading = gpu_loading;
		gpu_av_loading = gpu_loading;

		spin_lock_irqsave(&load_info_lock, ui32IRQFlags);
		g_loading2_sum += gpu_loading;
		g_loading2_count += 1;
		spin_unlock_irqrestore(&load_info_lock, ui32IRQFlags);

#ifdef GED_SSPM
		mt_gpufreq_set_loading(gpu_av_loading);
#endif
	}

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	loading_mode = ged_get_dvfs_loading_mode();

	if (loading_mode == LOADING_MAX_3DTA_COM) {
		ui32GPULoading =
		 MAX(g_Util_Ex.util_3d, g_Util_Ex.util_ta) +
		g_Util_Ex.util_compute;
	} else if (loading_mode == LOADING_MAX_3DTA) {
		ui32GPULoading =
		 MAX(g_Util_Ex.util_3d, g_Util_Ex.util_ta);
	}
#endif
	ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] timer: loading %u", ui32GPULoading);

	/* conventional timer-based policy */
	if (g_gpu_timer_based_emu) {
		if (ui32GPULoading >= 99)
			i32NewFreqID = 0;
		else if (ui32GPULoading <= 1)
			i32NewFreqID = i32MaxLevel;
		else if (ui32GPULoading >= 85)
			i32NewFreqID -= 2;
		else if (ui32GPULoading <= 30)
			i32NewFreqID += 2;
		else if (ui32GPULoading >= 70)
			i32NewFreqID -= 1;
		else if (ui32GPULoading <= 50)
			i32NewFreqID += 1;

		if (i32NewFreqID < ui32GPUFreq) {
			if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
				i32NewFreqID -= 1;
		} else if (i32NewFreqID > ui32GPUFreq) {
			if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
				i32NewFreqID += 1;
		}

		g_CommitType = MTK_GPU_DVFS_TYPE_TIMERBASED;
	} else {
		/* vsync-based fallback mode */
#ifndef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
		static int init;
#endif

#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	int t_gpu_real = -1;
	int t_gpu_pipe = -1;
	int t_gpu = -1;
	int t_gpu_target = -1;
	int temp = 0;
	unsigned long long ullWnd = 0;

	if (g_tb_dvfs_margin_mode & DYNAMIC_TB_MASK) {
		if (ged_kpi_timer_based_pick_riskyBQ(
			&t_gpu_real, &t_gpu_pipe,
			&t_gpu_target, &ullWnd) == GED_OK) {
			t_gpu =
				(g_tb_dvfs_margin_mode
				& DYNAMIC_TB_PIPE_TIME_MASK) ?
				t_gpu_pipe : t_gpu_real;
			t_gpu_target =
				(g_tb_dvfs_margin_mode
				& DYNAMIC_TB_FIX_TARGET_MASK) ?
				33333 : t_gpu_target;
			t_gpu_target =
				(g_tb_dvfs_margin_mode
				& DYNAMIC_TB_PERF_MODE_MASK) ?
				(t_gpu_target
				* (100 - gx_tb_dvfs_margin) / 100)
				: t_gpu_target;
			if (t_gpu > t_gpu_target) {
				temp = gx_tb_dvfs_margin
					* (t_gpu - t_gpu_target)
					/ t_gpu_target;

				if (temp < MIN_TB_MARGIN_INC_STEP)
					temp = MIN_TB_MARGIN_INC_STEP;

				gx_tb_dvfs_margin += temp;

				if (gx_tb_dvfs_margin > g_tb_dvfs_margin_value)
					gx_tb_dvfs_margin =
					g_tb_dvfs_margin_value;
			} else {
				gx_tb_dvfs_margin -=
					gx_tb_dvfs_margin
					* (t_gpu_target - t_gpu)
					/ t_gpu_target;

				if (gx_tb_dvfs_margin <
					g_tb_dvfs_margin_value_min)
					gx_tb_dvfs_margin =
						g_tb_dvfs_margin_value_min;
			}
		}
	} else {
		gx_tb_dvfs_margin = g_tb_dvfs_margin_value;
	}
#endif

		if (init == 0) {
			init = 1;
			gx_tb_dvfs_margin_cur
				= gx_tb_dvfs_margin;
			_init_loading_ud_table();
		}

		if (gx_tb_dvfs_margin != gx_tb_dvfs_margin_cur
				&& gx_tb_dvfs_margin < 100
				&& gx_tb_dvfs_margin >= 0) {
			gx_tb_dvfs_margin_cur
				= gx_tb_dvfs_margin;
			_init_loading_ud_table();
		}

#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][LB_DVFS] mode:0x%x, u_b:%d, l_b:%d, margin:%d, gpu_real:%d, gpu_pipe:%d, t_gpu:%d, target:%d, BQ:%llu",
			g_tb_dvfs_margin_mode,
			g_tb_dvfs_margin_value,
			g_tb_dvfs_margin_value_min,
			gx_tb_dvfs_margin_cur,
			t_gpu_real,
			t_gpu_pipe,
			t_gpu,
			t_gpu_target,
			ullWnd);
#endif

		ui32GPULoading_avg = _loading_avg(ui32GPULoading);
		if (ui32GPULoading >= 110 - gx_tb_dvfs_margin_cur) {
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
			if (dvfs_step_mode == 0)
				i32NewFreqID = 0;
			else
				i32NewFreqID -= (dvfs_step_mode&0xff);

			if (i32NewFreqID < 0)
				i32NewFreqID = 0;
#else
			i32NewFreqID = 0;
#endif
		} else if (ui32GPULoading_avg >=
			loading_ud_table[ui32GPUFreq].up) {
			i32NewFreqID -= 1;
		} else if (ui32GPULoading_avg <=
			loading_ud_table[ui32GPUFreq].down) {
			i32NewFreqID += 1;
		}
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
		ged_log_buf_print(ghLogBuf_DVFS,
	"[GED_K1] rdy gpu_av_loading:%u, %d(%d)-up:%d,%d, new: %d, step: 0x%x",
				ui32GPULoading,
				ui32GPUFreq,
				loading_ud_table[ui32GPUFreq].freq,
				loading_ud_table[ui32GPUFreq].up,
				loading_ud_table[ui32GPUFreq].down,
				i32NewFreqID,
				dvfs_step_mode);
#else
		ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] rdy gpu_av_loading: %u, %d(%d)-up:%d,%d, new: %d",
				ui32GPULoading,
				ui32GPUFreq,
				loading_ud_table[ui32GPUFreq].freq,
				loading_ud_table[ui32GPUFreq].up,
				loading_ud_table[ui32GPUFreq].down,
				i32NewFreqID);
#endif
		g_CommitType = MTK_GPU_DVFS_TYPE_FALLBACK;
	}

	if (i32NewFreqID > i32MaxLevel)
		i32NewFreqID = i32MaxLevel;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	*pui32NewFreqID = (unsigned int)i32NewFreqID;
	g_policy_tar_freq = mt_gpufreq_get_freq_by_idx(i32NewFreqID);
	g_mode = 2;

#ifdef CONFIG_MTK_QOS_V1_SUPPORT
	return GED_TRUE;
#else
	return ((*pui32NewFreqID != ui32GPUFreq) || ged_log_perf_trace_enable)
		? GED_TRUE : GED_FALSE;
#endif
}


#ifdef ENABLE_COMMON_DVFS
static void ged_dvfs_freq_input_boostCB(unsigned int ui32BoostFreqID)
{
	if (g_iSkipCount > 0)
		return;

	if (boost_gpu_enable == 0)
		return;

	mutex_lock(&gsDVFSLock);

	if (ui32BoostFreqID < mt_gpufreq_get_cur_freq_index()) {
		if (ged_dvfs_gpu_freq_commit(ui32BoostFreqID,
				mt_gpufreq_get_freq_by_idx(ui32BoostFreqID),
				GED_DVFS_INPUT_BOOST_COMMIT))
			g_dvfs_skip_round = GED_DVFS_SKIP_ROUNDS;
	}

	mutex_unlock(&gsDVFSLock);
}

static void ged_dvfs_freq_thermal_limitCB(unsigned int ui32LimitFreqID)
{
	if (g_iSkipCount > 0)
		return;

	if (ui32LimitFreqID == 0) /* thermal event disable */
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT, GED_FALSE);
	else
		ged_dvfs_vsync_offset_event_switch(
			GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT, GED_TRUE);

	mutex_lock(&gsDVFSLock);

	if (ui32LimitFreqID > mt_gpufreq_get_cur_freq_index()) {
		if (ged_dvfs_gpu_freq_commit(ui32LimitFreqID,
				mt_gpufreq_get_freq_by_idx(ui32LimitFreqID),
				GED_DVFS_SET_LIMIT_COMMIT))
			g_dvfs_skip_round = 0;
	}

	mutex_unlock(&gsDVFSLock);
}

void ged_dvfs_boost_gpu_freq(void)
{
	if (gpu_debug_enable)
		GED_LOGD("@%s", __func__);

	ged_dvfs_freq_input_boostCB(0);
}

static void ged_dvfs_set_bottom_gpu_freq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;
	static unsigned int s_bottom_freq_id;

	if (gpu_debug_enable)
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	s_bottom_freq_id = ui32MaxLevel - ui32FreqLevel;
	gpu_bottom_freq = mt_gpufreq_get_freq_by_idx(s_bottom_freq_id);
	if (g_bottom_freq_id < s_bottom_freq_id) {
		g_bottom_freq_id = s_bottom_freq_id;
		if (s_bottom_freq_id < g_last_def_commit_freq_id)
			ged_dvfs_gpu_freq_commit(s_bottom_freq_id,
			gpu_bottom_freq,
			GED_DVFS_SET_BOTTOM_COMMIT);
		else
			ged_dvfs_gpu_freq_commit(g_last_def_commit_freq_id,
			mt_gpufreq_get_freq_by_idx(g_last_def_commit_freq_id),
			GED_DVFS_SET_BOTTOM_COMMIT);
	} else {
	/* if current id is larger, ie lower freq, reflect immedately */
		g_bottom_freq_id = s_bottom_freq_id;
		if (s_bottom_freq_id < mt_gpufreq_get_cur_freq_index())
			ged_dvfs_gpu_freq_commit(s_bottom_freq_id,
			gpu_bottom_freq,
			GED_DVFS_SET_BOTTOM_COMMIT);
	}
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
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_boost_freq_id = ui32FreqLevel;
	gpu_cust_boost_freq = mt_gpufreq_get_freq_by_idx(g_cust_boost_freq_id);

	if (g_cust_boost_freq_id < mt_gpufreq_get_cur_freq_index()) {
		ged_dvfs_gpu_freq_commit(g_cust_boost_freq_id,
			gpu_cust_boost_freq, GED_DVFS_CUSTOM_BOOST_COMMIT);
	}

	mutex_unlock(&gsDVFSLock);
}

static void ged_dvfs_custom_ceiling_gpu_freq(unsigned int ui32FreqLevel)
{
	unsigned int ui32MaxLevel;

	if (gpu_debug_enable)
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;
	if (ui32MaxLevel < ui32FreqLevel)
		ui32FreqLevel = ui32MaxLevel;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_upbound_freq_id = ui32FreqLevel;

	gpu_cust_upbound_freq =
		mt_gpufreq_get_freq_by_idx(g_cust_upbound_freq_id);

	if (g_cust_upbound_freq_id > mt_gpufreq_get_cur_freq_index())
		ged_dvfs_gpu_freq_commit(g_cust_upbound_freq_id,
		gpu_cust_upbound_freq, GED_DVFS_CUSTOM_CEIL_COMMIT);

	mutex_unlock(&gsDVFSLock);
}

static unsigned int ged_dvfs_get_bottom_gpu_freq(void)
{
	unsigned int ui32MaxLevel = mt_gpufreq_get_dvfs_table_num() - 1;

	return ui32MaxLevel - g_bottom_freq_id;
}

static unsigned int ged_dvfs_get_custom_ceiling_gpu_freq(void)
{
	return g_cust_upbound_freq_id;
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

unsigned int ged_dvfs_get_custom_boost_gpu_freq(void)
{
	return g_cust_boost_freq_id;
}

#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
static void ged_dvfs_margin_value(int i32MarginValue)
{
	/* -1:  default: configure margin mode */
	/* -2:  variable margin mode by opp index */
	/* 0~100: configure margin mode */
	/* 101~199:  dynamic margin mode - CONFIG_FPS_MARGIN */
	/* 201~299:  dynamic margin mode - FIXED_FPS_MARGIN */
	/* 301~399:  dynamic margin mode - NO_FPS_MARGIN */
	/* 401~499:  dynamic margin mode - EXTEND MODE */
	/* 501~599:  dynamic margin mode - PERF MODE */

	/****************************************************/
	/* bit0~bit9: dvfs_margin_value setting             */
	/*                                                  */
	/* EXTEND MODE (401~499):                           */
	/* PERF MODE (501~599):                             */
	/* bit10~bit15: min. inc step (0~63 step)           */
	/* bit16~bit22: (0~100%)dynamic headroom low bound  */
	/****************************************************/

	int i32MarginValue_ori;

	mutex_lock(&gsDVFSLock);

	dvfs_min_margin_inc_step = MIN_MARGIN_INC_STEP;
	dvfs_margin_low_bound = MIN_DVFS_MARGIN/10;

	if (i32MarginValue == -1) {
		dvfs_margin_mode = CONFIGURE_MARGIN_MODE;
		dvfs_margin_value = DEFAULT_DVFS_MARGIN/10;
		mutex_unlock(&gsDVFSLock);
		return;
	} else if (i32MarginValue == -2) {
		dvfs_margin_mode = VARIABLE_MARGIN_MODE_OPP_INDEX;
		mutex_unlock(&gsDVFSLock);
		return;
	}

	i32MarginValue_ori = i32MarginValue;
	i32MarginValue = i32MarginValue & 0x3ff;

	if ((i32MarginValue >= 0) && (i32MarginValue <= 100))
		dvfs_margin_mode = CONFIGURE_MARGIN_MODE;
	else if ((i32MarginValue > 100) && (i32MarginValue < 200)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 100;
	} else if ((i32MarginValue > 200) && (i32MarginValue < 300)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 200;
	} else if ((i32MarginValue > 300) && (i32MarginValue < 400)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 300;
	} else if ((i32MarginValue > 400) && (i32MarginValue < 500)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_EXTEND;
		i32MarginValue = i32MarginValue - 400;
		dvfs_min_margin_inc_step = (i32MarginValue_ori & 0xfc00) >> 10;
		dvfs_margin_low_bound = (i32MarginValue_ori & 0x7f0000) >> 16;
	} else if ((i32MarginValue > 500) && (i32MarginValue < 600)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_PERF;
		i32MarginValue = i32MarginValue - 500;
		dvfs_min_margin_inc_step = (i32MarginValue_ori & 0xfc00) >> 10;
		dvfs_margin_low_bound = (i32MarginValue_ori & 0x7f0000) >> 16;
	}

	if (i32MarginValue > (MAX_DVFS_MARGIN/10)) /* 0~ MAX_DVFS_MARGIN % */
		dvfs_margin_value = (MAX_DVFS_MARGIN/10);
	else
		dvfs_margin_value = i32MarginValue;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_dvfs_margin_value(void)
{
	int ret = 0;

	if (dvfs_margin_mode == CONFIGURE_MARGIN_MODE)
		ret = dvfs_margin_value;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN)
		ret = dvfs_margin_value + 100;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN)
		ret = dvfs_margin_value + 200;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN)
		ret = dvfs_margin_value + 300;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_EXTEND)
		ret = dvfs_margin_value + 400;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_PERF)
		ret = dvfs_margin_value + 500;
	else if (dvfs_margin_mode == VARIABLE_MARGIN_MODE_OPP_INDEX)
		ret = -2;

	return ret;
}

int ged_get_dvfs_margin(void)
{
	return gx_fb_dvfs_margin;
}

unsigned int ged_get_dvfs_margin_mode(void)
{
	return dvfs_margin_mode;
}

#endif

#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
static void ged_loading_base_dvfs_step(int i32StepValue)
{
	/* -1:  default */
	/* bit0~bit7: dvfs step */
	/* bit8~bit15: enlarge range  */

	mutex_lock(&gsDVFSLock);

	if (i32StepValue != ((dvfs_step_mode&0xff00)>>8))
		init = 0;

	dvfs_step_mode = i32StepValue;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_loading_base_dvfs_step(void)
{
	return dvfs_step_mode;
}

#endif

#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
static void ged_timer_base_dvfs_margin(int i32MarginValue)
{
	/*
	 * value < 0: default, GED_DVFS_TIMER_BASED_DVFS_MARGIN
	 * bit   7~0: margin value
	 * bit     8: dynamic timer based dvfs margin
	 * bit     9: use gpu pipe time for dynamic timer based dvfs margin
	 * bit    10: use performance mode for dynamic timer based dvfs margin
	 * bit    11: fix target FPS to 30 for dynamic timer based dvfs margin
	 * bit 23~16: min margin value
	 */
	unsigned int mode = CONFIGURE_TIMER_BASED_MODE;
	int value = i32MarginValue & TIMER_BASED_MARGIN_MASK;
	int value_min = (i32MarginValue >> 16) & TIMER_BASED_MARGIN_MASK;

	if (i32MarginValue < 0)
		value = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
	else {
		mode = (i32MarginValue & DYNAMIC_TB_MASK) ?
				(mode | DYNAMIC_TB_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_PIPE_TIME_MASK) ?
				(mode | DYNAMIC_TB_PIPE_TIME_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_PERF_MODE_MASK) ?
				(mode | DYNAMIC_TB_PERF_MODE_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_FIX_TARGET_MASK) ?
				(mode | DYNAMIC_TB_FIX_TARGET_MASK) : mode;
	}

	mutex_lock(&gsDVFSLock);

	g_tb_dvfs_margin_mode = mode;

	if (value > MAX_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value = MAX_TB_DVFS_MARGIN;
	else if (value < MIN_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value = MIN_TB_DVFS_MARGIN;
	else
		g_tb_dvfs_margin_value = value;

	if (value_min > MAX_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value_min = MAX_TB_DVFS_MARGIN;
	else if (value_min < MIN_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value_min = MIN_TB_DVFS_MARGIN;
	else
		g_tb_dvfs_margin_value_min = value_min;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_timer_base_dvfs_margin(void)
{
	return g_tb_dvfs_margin_value;
}

int ged_dvfs_get_tb_dvfs_margin_cur(void)
{
	return gx_tb_dvfs_margin_cur;
}

unsigned int ged_dvfs_get_tb_dvfs_margin_mode(void)
{
	return g_tb_dvfs_margin_mode;
}
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
#ifdef GED_ENABLE_DVFS_LOADING_MODE
static void ged_dvfs_loading_mode(int i32MarginValue)
{
	/* -1: default: */
	/* 0: default (Active)/(Active+Idle)*/
	/* 1: max(TA,3D)+COMPUTE/(Active+Idle) */
	/* 2: max(TA,3D)/(Active+Idle) */

	mutex_lock(&gsDVFSLock);

	if (i32MarginValue == -1)
		gx_dvfs_loading_mode = LOADING_ACTIVE;
	else if ((i32MarginValue >= 0) && (i32MarginValue < 100))
		gx_dvfs_loading_mode = i32MarginValue;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_dvfs_loading_mode(void)
{
	return gx_dvfs_loading_mode;
}

void ged_get_gpu_utli_ex(struct GpuUtilization_Ex *util_ex)
{
	memcpy((void *)util_ex, (void *)&g_Util_Ex,
		sizeof(struct GpuUtilization_Ex));
}
#endif

/* Need spinlocked */
void ged_dvfs_save_loading_page(void)
{
	gL_ulCalResetTS_us = g_ulCalResetTS_us;
	gL_ulPreCalResetTS_us = g_ulPreCalResetTS_us;
	gL_ulWorkingPeriod_us = g_ulWorkingPeriod_us;

    /* set as zero for next time */
	g_ulWorkingPeriod_us = 0;
}

void ged_dvfs_run(unsigned long t, long phase, unsigned long ul3DFenceDoneTime)
{
	unsigned long ui32IRQFlags;
	unsigned int gpu_freq_pre;

	mutex_lock(&gsDVFSLock);

	if (gpu_dvfs_enable == 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][LB_DVFS] skip %s due to gpu_dvfs_enable=%u",
			__func__, gpu_dvfs_enable);
		goto EXIT_ged_dvfs_run;
	}

	/* SKIP for keeping boost freq */
	if (g_dvfs_skip_round > 0)
		g_dvfs_skip_round--;

	if (g_iSkipCount > 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;
	} else {
#ifdef GED_ENABLE_FB_DVFS
		spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);
		if (is_fb_dvfs_triggered) {
			spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
			goto EXIT_ged_dvfs_run;
		}

		is_fallback_mode_triggered = 1;
#ifdef GED_ENABLE_DVFS_LOADING_MODE
		ged_dvfs_cal_gpu_utilization_ex(&gpu_loading,
			&gpu_block, &gpu_idle, &g_Util_Ex);
#else
		ged_dvfs_cal_gpu_utilization(&gpu_loading,
			&gpu_block, &gpu_idle);
#endif
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][FB_DVFS] fallback mode");
		spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
#endif /* GED_ENABLE_FB_DVFS */

		spin_lock_irqsave(&g_sSpinLock, ui32IRQFlags);
		g_ulPreCalResetTS_us = g_ulCalResetTS_us;
		g_ulCalResetTS_us = t;

		ged_dvfs_save_loading_page();

		spin_unlock_irqrestore(&g_sSpinLock, ui32IRQFlags);

#ifdef GED_DVFS_UM_CAL
		if (phase == GED_DVFS_TIMER_BACKUP)
#endif
		{
			/* timer-backup DVFS use only */
			if (ged_dvfs_policy(gpu_loading,
				&g_ui32FreqIDFromPolicy, t, phase,
				ul3DFenceDoneTime, false)) {
				gpu_freq_pre = mt_gpufreq_get_cur_freq();
				g_computed_freq_id = g_ui32FreqIDFromPolicy;
				ged_dvfs_gpu_freq_commit(g_ui32FreqIDFromPolicy,
						mt_gpufreq_get_freq_by_idx(
						g_ui32FreqIDFromPolicy),
						GED_DVFS_DEFAULT_COMMIT);
#ifdef CONFIG_MTK_QOS_V1_SUPPORT
				mt_gpu_bw_qos_vcore(ged_dvfs_vcore(gpu_freq_pre,
					mt_gpufreq_get_cur_freq(), false));
#endif
			}
		}
	}

	if (gpu_debug_enable)
		GED_LOGD("@%s: gpu_loading=%d %d, g_iSkipCount=%d", __func__,
			gpu_loading, mt_gpufreq_get_cur_freq_index(),
			g_iSkipCount);

EXIT_ged_dvfs_run:
	mutex_unlock(&gsDVFSLock);
}

void ged_dvfs_sw_vsync_query_data(struct GED_DVFS_UM_QUERY_PACK *psQueryData)
{
	psQueryData->ui32GPULoading = gpu_loading;
	psQueryData->ui32GPUFreqID =  mt_gpufreq_get_cur_freq_index();
	psQueryData->gpu_cur_freq =
		mt_gpufreq_get_freq_by_idx(psQueryData->ui32GPUFreqID);
	psQueryData->gpu_pre_freq = mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
	psQueryData->nsOffset = ged_dvfs_vsync_offset_level_get();

	psQueryData->ulWorkingPeriod_us = gL_ulWorkingPeriod_us;
	psQueryData->ulPreCalResetTS_us = gL_ulPreCalResetTS_us;

	psQueryData->ui32TargetPeriod_us = g_ui32TargetPeriod_us;
	psQueryData->ui32BoostValue = g_ui32BoostValue;
}

void ged_dvfs_track_latest_record(enum MTK_GPU_DVFS_TYPE *peType,
	unsigned long *pulFreq)
{
	*peType = g_CommitType;
	*pulFreq = g_ulCommitFreq;
}

unsigned long ged_dvfs_get_gpu_commit_freq(void)
{
	return ged_commit_freq;
}

unsigned long ged_dvfs_get_gpu_commit_opp_freq(void)
{
	return ged_commit_opp_freq;
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

unsigned int ged_dvfs_get_gpu_loading2(int reset)
{
	int loading = 0;
	unsigned long ui32IRQFlags;

	spin_lock_irqsave(&load_info_lock, ui32IRQFlags);

	if (g_loading2_count > 0)
		loading = g_loading2_sum / g_loading2_count;

	if (reset) {
		g_loading2_sum = 0;
		g_loading2_count = 0;
	}

	spin_unlock_irqrestore(&load_info_lock, ui32IRQFlags);

	return loading;
}

unsigned int ged_dvfs_get_gpu_blocking(void)
{
	return gpu_block;
}

unsigned int ged_dvfs_get_gpu_idle(void)
{
	return 100 - gpu_av_loading;
}

void ged_dvfs_get_gpu_cur_freq(struct GED_DVFS_FREQ_DATA *psData)
{
	psData->ui32Idx = mt_gpufreq_get_cur_freq_index();
	psData->ulFreq = mt_gpufreq_get_freq_by_idx(psData->ui32Idx);
}

void ged_dvfs_get_gpu_pre_freq(struct GED_DVFS_FREQ_DATA *psData)
{
	psData->ui32Idx = g_ui32PreFreqID;
	psData->ulFreq = mt_gpufreq_get_freq_by_idx(g_ui32PreFreqID);
}

void ged_get_gpu_dvfs_cal_freq(unsigned long *p_policy_tar_freq, int *pmode)
{
	*p_policy_tar_freq = g_policy_tar_freq;
	*pmode = g_mode;
}

GED_ERROR ged_dvfs_probe_signal(int signo)
{
	int cache_pid = GED_NO_UM_SERVICE;
	struct task_struct *t = NULL;
	struct siginfo info;

	info.si_signo = signo;
	info.si_code = SI_QUEUE;
	info.si_int = 1234;

	if (cache_pid != g_probe_pid) {
		cache_pid = g_probe_pid;
		if (g_probe_pid == GED_NO_UM_SERVICE)
			t = NULL;
		else {
			rcu_read_lock();
			t = pid_task(find_vpid(g_probe_pid), PIDTYPE_PID);
			rcu_read_unlock();
		}
	}

	if (t != NULL) {
		send_sig_info(signo, &info, t);
		return GED_OK;
	} else {
		g_probe_pid = GED_NO_UM_SERVICE;
		return GED_ERROR_INVALID_PARAMS;
	}
}

void set_target_fps(int i32FPS)
{
	g_ulvsync_period = get_ns_period_from_fps(i32FPS);
}

unsigned long ged_gas_query_mode(void)
{
	unsigned int ret;

	ret  = (g_ui32EventStatus & GED_EVENT_GAS)
			? GAS_CATEGORY_GAME
			: GAS_CATEGORY_OTHERS;
	return ret;
}

GED_ERROR ged_dvfs_probe(int pid)
{
	if (pid == GED_VSYNC_OFFSET_NOT_SYNC) {
		g_ui32EventDebugStatus |= GED_EVENT_NOT_SYNC;
		return GED_OK;
	}

	if (pid == GED_VSYNC_OFFSET_SYNC)	{
		g_ui32EventDebugStatus &= (~GED_EVENT_NOT_SYNC);
		return GED_OK;
	}

	g_probe_pid = pid;

	/* clear bits among start */
	if (g_probe_pid != GED_NO_UM_SERVICE) {
		g_ui32EventStatus &= (~GED_EVENT_TOUCH);
		g_ui32EventStatus &= (~GED_EVENT_WFD);
		g_ui32EventStatus &= (~GED_EVENT_GAS);

		g_ui32EventDebugStatus = 0;
	}

	return GED_OK;
}

GED_ERROR ged_dvfs_system_init(void)
{
	mutex_init(&gsDVFSLock);
	mutex_init(&gsVSyncOffsetLock);
#ifdef GED_ENABLE_FB_DVFS
	spin_lock_init(&gsGpuUtilLock);
#endif

	/* initial as locked, signal when vsync_sw_notify */
#ifdef ENABLE_COMMON_DVFS
	gpu_dvfs_enable = 1;

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

	g_ulvsync_period = get_ns_period_from_fps(60);

#ifdef GED_ENABLE_FB_DVFS
	ged_kpi_gpu_dvfs_fp = ged_dvfs_fb_gpu_dvfs;
	ged_kpi_trigger_fb_dvfs_fp = ged_dvfs_trigger_fb_dvfs;
	ged_kpi_check_if_fallback_mode_fp = ged_dvfs_is_fallback_mode_triggered;
#endif

	g_dvfs_skip_round = 0;

	g_bottom_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_bottom_freq = mt_gpufreq_get_freq_by_idx(g_bottom_freq_id);

	g_cust_boost_freq_id = mt_gpufreq_get_dvfs_table_num() - 1;
	gpu_cust_boost_freq = mt_gpufreq_get_freq_by_idx(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;
	gpu_cust_upbound_freq =
		mt_gpufreq_get_freq_by_idx(g_cust_upbound_freq_id);

	g_policy_tar_freq = 0;
	g_mode = 0;

	ged_commit_freq = 0;
	ged_commit_opp_freq = 0;

#ifdef GED_DVFS_STRESS_TEST
	ged_dvfs_stress_test = 0;
#endif

#ifdef ENABLE_TIMER_BACKUP
	g_gpu_timer_based_emu = 0;
#else
	g_gpu_timer_based_emu = 1;
#endif

#ifdef CONFIG_MTK_QOS_V1_SUPPORT
	/* default as %100 */
	gpu_bw_ratio = 100;
#endif

	/* GPU HAL fp mount */
	mt_gpufreq_power_limit_notify_registerCB(ged_dvfs_freq_thermal_limitCB);
#ifdef ENABLE_COMMON_DVFS
	mtk_boost_gpu_freq_fp = ged_dvfs_boost_gpu_freq;
#endif
	mtk_set_bottom_gpu_freq_fp = ged_dvfs_set_bottom_gpu_freq;
	mtk_get_bottom_gpu_freq_fp = ged_dvfs_get_bottom_gpu_freq;
	mtk_custom_get_gpu_freq_level_count_fp =
		ged_dvfs_get_gpu_freq_level_count;
	mtk_custom_boost_gpu_freq_fp = ged_dvfs_custom_boost_gpu_freq;
	mtk_custom_upbound_gpu_freq_fp = ged_dvfs_custom_ceiling_gpu_freq;
	mtk_get_custom_boost_gpu_freq_fp = ged_dvfs_get_custom_boost_gpu_freq;
	mtk_get_custom_upbound_gpu_freq_fp =
		ged_dvfs_get_custom_ceiling_gpu_freq;
	mtk_get_gpu_loading_fp = ged_dvfs_get_gpu_loading;
	mtk_get_gpu_loading2_fp = ged_dvfs_get_gpu_loading2;
	mtk_get_gpu_block_fp = ged_dvfs_get_gpu_blocking;
	mtk_get_gpu_idle_fp = ged_dvfs_get_gpu_idle;
	mtk_do_gpu_dvfs_fp = ged_dvfs_run;
	mtk_gpu_dvfs_set_mode_fp = ged_dvfs_set_tuning_mode_wrap;

	mtk_get_gpu_sub_loading_fp = ged_dvfs_get_sub_gpu_loading;
	mtk_get_vsync_based_target_freq_fp = ged_dvfs_get_gpu_tar_freq;
	mtk_GetGpuDVFSfromFp = ged_dvfs_track_latest_record;

	mtk_get_gpu_bottom_freq_fp = ged_get_gpu_bottom_freq;
	mtk_get_gpu_custom_boost_freq_fp = ged_get_gpu_custom_boost_freq;
	mtk_get_gpu_custom_upbound_freq_fp = ged_get_gpu_custom_upbound_freq;

	ged_kpi_set_gpu_dvfs_hint_fp = ged_dvfs_last_and_target_cb;

#if (defined(GED_ENABLE_FB_DVFS) && defined(GED_ENABLE_DYNAMIC_DVFS_MARGIN))
	mtk_dvfs_margin_value_fp = ged_dvfs_margin_value;
	mtk_get_dvfs_margin_value_fp = ged_get_dvfs_margin_value;
#endif
#ifdef GED_CONFIGURE_LOADING_BASE_DVFS_STEP
	mtk_loading_base_dvfs_step_fp = ged_loading_base_dvfs_step;
	mtk_get_loading_base_dvfs_step_fp = ged_get_loading_base_dvfs_step;
#endif
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	mtk_timer_base_dvfs_margin_fp =	ged_timer_base_dvfs_margin;
	mtk_get_timer_base_dvfs_margin_fp = ged_get_timer_base_dvfs_margin;
#endif
#ifdef GED_ENABLE_DVFS_LOADING_MODE
	mtk_dvfs_loading_mode_fp = ged_dvfs_loading_mode;
	mtk_get_dvfs_loading_mode_fp = ged_get_dvfs_loading_mode;
#endif

	/* CAP query */
	mtk_get_gpu_dvfs_cal_freq_fp = ged_get_gpu_dvfs_cal_freq;

	spin_lock_init(&g_sSpinLock);
#else
	gpu_dvfs_enable = 0;
#endif /* ENABLE_COMMON_DVFS */

	return GED_OK;
}

void ged_dvfs_system_exit(void)
{
	mutex_destroy(&gsDVFSLock);
	mutex_destroy(&gsVSyncOffsetLock);
}

#ifdef CONFIG_MTK_QOS_V1_SUPPORT
module_param(gpu_bw_ratio, uint, 0644);
#endif

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
module_param(gpu_bw_err_debug, uint, 0644);
#endif

