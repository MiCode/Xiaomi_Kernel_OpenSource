// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/version.h>
#include <ged_kpi.h>
#include <ged_base.h>
#include <ged_hashtable.h>
#include <ged_dvfs.h>
#include <ged_log.h>
#include <ged.h>
#include "ged_thread.h"
/* #include <ged_vsync.h> */

#include <mt-plat/mtk_gpu_utility.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

#ifdef GED_KPI_MET_DEBUG
#include <mt-plat/met_drv.h>
#endif /* GED_KPI_MET_DEBUG */

#include <linux/sync_file.h>
#include <linux/dma-fence.h>

#include <ged_notify_sw_vsync.h>

#ifdef MTK_CPUFREQ
#include "mtk_cpufreq_common_api.h"
#endif /* MTK_CPUFREQ */

#include "ged_global.h"
#include "ged_eb.h"

#if defined(MTK_GPU_BM_2)
#include <ged_gpu_bm.h>
#endif /* MTK_GPU_BM_2 */
#include <ged_dcs.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_drm_arr.h"
#else
#include "disp_arr.h"
#endif

#ifdef MTK_GED_KPI

#define GED_KPI_TAG "[GED_KPI]"
#define GED_PR_DEBUG(fmt, args...)\
	pr_debug(GED_KPI_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define GED_KPI_MSEC_DIVIDER 1000000
#define GED_KPI_SEC_DIVIDER 1000000000
#define GED_KPI_MAX_FPS 60
/* set default margin to be distinct from FPSGO(0 or 3) */
#define GED_KPI_DEFAULT_FPS_MARGIN 4
#define GED_KPI_CPU_MAX_OPP 0
#define GED_KPI_FPS_LIMIT 120

#define GED_TIMESTAMP_TYPE_D    0x1
#define GED_TIMESTAMP_TYPE_1    0x2
#define GED_TIMESTAMP_TYPE_2    0x4
#define GED_TIMESTAMP_TYPE_S    0x8
#define GED_TIMESTAMP_TYPE_P    0x10
#define GED_TIMESTAMP_TYPE_H    0x20
#define GED_SET_TARGET_FPS      0x40
#define GED_TIMESTAMP_TYPE      int

/* No frame control is applied */
#define GED_KPI_FRC_DEFAULT_MODE    0
#define GED_KPI_FRC_FRR_MODE        1
#define GED_KPI_FRC_ARR_MODE        2
#define GED_KPI_FRC_SW_VSYNC_MODE   3
#define GED_KPI_FRC_MODE_TYPE       int

enum {
	g_idle_set_finish,
	g_idle_set_prepare,
	g_idle_fix
};

struct GED_KPI_HEAD {
	int pid;
	int i32Count;
	unsigned long long ullWnd;
	unsigned long long last_TimeStamp1;
	unsigned long long last_TimeStamp2;
	unsigned long long last_TimeStampS;
	unsigned long long last_TimeStampH;
	unsigned long long pre_TimeStamp2;
	long long t_cpu_remained;
	long long t_gpu_remained;
	long long t_cpu_latest;
	long long t_gpu_latest;
	struct list_head sList;
	int i32QedBuffer_length;
	int i32Gpu_uncompleted;
	int i32DebugQedBuffer_length;
	int isSF;
	int isFRR_enabled;
	int isARR_enabled;

	int target_fps;
	int target_fps_margin;
	int eara_fps_margin;

	int t_cpu_target;
	int t_gpu_target;
	int t_cpu_fpsgo;
	int gpu_done_interval;

	GED_KPI_FRC_MODE_TYPE frc_mode;
	int frc_client;
	unsigned long long last_QedBufferDelay;
};

struct GED_CPU_INFO {
	unsigned long cpu_max_freq_LL;
	unsigned long cpu_max_freq_L;
	unsigned long cpu_max_freq_B;
	unsigned long cpu_cur_freq_LL;
	unsigned long cpu_cur_freq_L;
	unsigned long cpu_cur_freq_B;
	unsigned int cpu_cur_avg_load_LL;
	unsigned int cpu_cur_avg_load_L;
	unsigned int cpu_cur_avg_load_B;
};

struct GED_GPU_INFO {
	unsigned long gpu_dvfs;
	/* bit0~bit9: headroom ratio:10-bias */
	/* bit15: is frame base? */
	/* bit16~bit23: dvfs_margin_mode */
	unsigned long tb_dvfs_mode;
	unsigned long tb_dvfs_margin;
	unsigned long t_gpu_real;
	unsigned long limit_upper;
	unsigned long limit_lower;
	unsigned int dvfs_loading_mode;
	unsigned int gpu_util;
	unsigned int gpu_power;
	int gpu_freq_target;
};

union _cpu_gpu_info {
	struct GED_CPU_INFO cpu;
	struct GED_GPU_INFO gpu;
};

struct GED_KPI {
	int pid;
	unsigned long long ullWnd;
	unsigned long i32DeQueueID;
	unsigned long i32QueueID;
	unsigned long  i32AcquireID;
	unsigned long ulMask;
	unsigned long frame_attr;
	unsigned long long ullTimeStampD;
	unsigned long long ullTimeStamp1;
	unsigned long long ullTimeStamp2;
	unsigned long long ullTimeStampP;
	unsigned long long ullTimeStampS;
	unsigned long long ullTimeStampH;
	unsigned int gpu_freq; /* in MHz*/
	unsigned int gpu_freq_max; /* in MHz*/
	unsigned int gpu_loading;
	struct list_head sList;
	long long t_cpu_remained;
	long long t_gpu_remained;
	int i32QedBuffer_length;
	int i32Gpu_uncompleted;
	int i32DebugQedBuffer_length;
	int boost_linear_cpu;
	int boost_real_cpu;
	int boost_accum_gpu;
	long long t_cpu_remained_pred;
	unsigned long long t_acquire_period;
	unsigned long long QedBufferDelay;
	union _cpu_gpu_info cpu_gpu_info;
	long long t_cpu;
	long long t_gpu;
	int t_cpu_target;
	int t_gpu_target;
	int t_cpu_fpsgo;
	int gpu_done_interval;
	int target_fps_margin;
	int eara_fps_margin;
	int isSF;

	unsigned long long t_cpu_slptime;
};

struct GED_TIMESTAMP {
	GED_TIMESTAMP_TYPE eTimeStampType;
	int pid;
	unsigned long long ullWnd;
	unsigned long i32FrameID;
	unsigned long long ullTimeStamp;
	unsigned int i32GPUloading;
	int i32QedBuffer_length;
	int isSF;
	struct work_struct sWork;
	void *fence_addr;
};

struct GED_KPI_GPU_TS {
	int pid;
	u64 ullWdnd;
	unsigned long i32FrameID;
	struct dma_fence_cb sSyncWaiter;
	struct dma_fence *psSyncFence;
} GED_KPI_GPU_TS;

struct GED_KPI_MEOW_DVFS_FREQ_PRED {
	int gpu_freq_cur;
	int gpu_freq_max;
	int gpu_freq_pred;
	int gift_ratio;

	int target_pid;
	int target_fps;
	int target_fps_margin;
	int eara_fps_margin;
	int gpu_time;
};

static struct GED_KPI_MEOW_DVFS_FREQ_PRED *g_psGIFT;

int g_target_fps_default = GED_KPI_MAX_FPS;
int g_target_time_default = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;

#define GED_KPI_TOTAL_ITEMS 256
#define GED_KPI_UID(pid, wnd) (pid | ((unsigned long)wnd))
#define SCREEN_IDLE_PERIOD 500000000

/* static int display_fps = GED_KPI_MAX_FPS; */
static int target_fps_4_main_head = 60;
static long long vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
static GED_LOG_BUF_HANDLE ghLogBuf_KPI;
static struct workqueue_struct *g_psWorkQueue;
static GED_HASHTABLE_HANDLE gs_hashtable;

static spinlock_t gs_hashtableLock;

static struct GED_KPI g_asKPI[GED_KPI_TOTAL_ITEMS];
static int g_i32Pos;
static GED_THREAD_HANDLE ghThread;
// static unsigned int gx_dfps; /* variable to fix FPS*/

static unsigned int enable_gpu_boost = 1;

#if !defined(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT)
/* Disable for bring-up stage unexpected exception */
static unsigned int is_GED_KPI_enabled;
#else
static unsigned int is_GED_KPI_enabled = 1;
#endif

static unsigned int g_force_gpu_dvfs_fallback;
static int g_fb_dvfs_threshold = 80;
static int idle_fw_set_flag;
static int g_is_idle_fw_enable;

module_param(g_fb_dvfs_threshold, int, 0644);

// module_param(gx_dfps, uint, 0644);
module_param(enable_gpu_boost, uint, 0644);
module_param(is_GED_KPI_enabled, uint, 0644);


/* for calculating remained time budgets of CPU and GPU:
 *		time budget: the buffering time that prevents fram drop
 */
struct GED_KPI_HEAD *main_head;
struct GED_KPI_HEAD *prev_main_head;
/* end */

/* for calculating KPI info per second */
static unsigned long long g_pre_TimeStamp1;
static unsigned long long g_pre_TimeStamp2;
static unsigned long long g_pre_TimeStampS;
static unsigned long long g_elapsed_time_per_sec;
static unsigned long long g_cpu_time_accum;
static unsigned long long g_gpu_time_accum;
static unsigned long long g_ResponseTimeAccu;
static unsigned long long g_GRemTimeAccu; /*g_gpu_remained_time_accum*/
static unsigned long long g_CRemTimeAccu; /*g_cpu_remained_time_accum*/
static unsigned long long g_gpu_freq_accum;
static unsigned int g_frame_count;

static unsigned int gx_fps;
static unsigned int gx_cpu_time_avg;
static unsigned int gx_gpu_time_avg;
static unsigned int gx_response_time_avg;
static unsigned int gx_gpu_remained_time_avg;
static unsigned int gx_cpu_remained_time_avg;
static unsigned int gx_gpu_freq_avg;

unsigned int g_eb_workload;
unsigned int g_eb_coef;

int pid_sysui;
int pid_sf;

/* ------------------------------------------------------------------- */
void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID);
EXPORT_SYMBOL(ged_kpi_output_gfx_info2_fp);

static void ged_kpi_output_gfx_info2(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID)
{
	if (ged_kpi_output_gfx_info2_fp)
		ged_kpi_output_gfx_info2_fp(t_gpu, cur_freq
						, cur_max_freq, ulID);
}

/* ------------------------------------------------------------------- */
int (*ged_kpi_gpu_dvfs_fp)(int t_gpu, int t_gpu_target, int target_fps_margin,
	unsigned int force_fallback);

static int ged_kpi_gpu_dvfs(int t_gpu, int t_gpu_target,
	int target_fps_margin, unsigned int force_fallback)
{
	if (ged_kpi_gpu_dvfs_fp)
		return ged_kpi_gpu_dvfs_fp(t_gpu, t_gpu_target,
			target_fps_margin, force_fallback);

	return 0;
}
EXPORT_SYMBOL(ged_kpi_gpu_dvfs_fp);

/* ------------------------------------------------------------------- */
void (*ged_kpi_trigger_fb_dvfs_fp)(void);

void ged_kpi_trigger_fb_dvfs(void)
{
	if (ged_kpi_trigger_fb_dvfs_fp)
		ged_kpi_trigger_fb_dvfs_fp();
}
EXPORT_SYMBOL(ged_kpi_trigger_fb_dvfs_fp);
/* ------------------------------------------------------------------- */
int (*ged_kpi_check_if_fallback_mode_fp)(void);

int ged_kpi_check_if_fallback_mode(void)
{
	if (ged_kpi_check_if_fallback_mode_fp)
		return ged_kpi_check_if_fallback_mode_fp();
	else
		return 1;
}
EXPORT_SYMBOL(ged_kpi_check_if_fallback_mode_fp);

/* ------------------------------------------------------------------- */
static inline void ged_kpi_clean_kpi_info(void)
{
	g_frame_count = 0;
	g_elapsed_time_per_sec = 0;
	g_cpu_time_accum = 0;
	g_gpu_time_accum = 0;
	g_ResponseTimeAccu = 0;
	g_GRemTimeAccu = 0;
	g_CRemTimeAccu = 0;
	g_gpu_freq_accum = 0;
}
/* ------------------------------------------------------------------- */

static GED_BOOL ged_kpi_find_main_head_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *)pvoid;

	if (psHead) {
#ifdef GED_KPI_DEBUG
		GED_LOGD("[GED_KPI] psHead->i32Count: %d, isSF: %d\n",
			psHead->i32Count, psHead->isSF);
#endif /* GED_KPI_DEBUG */
		if (psHead->isSF == 0) {
			if (main_head == NULL
				|| psHead->i32Count > main_head->i32Count) {
				if (main_head && psHead) {
#ifdef GED_KPI_DEBUG
					GED_LOGD("[GED_KPI] main_head",
					"changes from",
					"%p to %p\n", main_head, psHead);
#endif /* GED_KPI_DEBUG */
				}
				main_head = psHead;
			}
		}
	}
	return GED_TRUE;
}
/* ------------------------------------------------------------------- */
/* for calculating average per-second performance info */
/* ------------------------------------------------------------------- */
static inline void ged_kpi_calc_kpi_info(u64 ulID, struct GED_KPI *psKPI
	, struct GED_KPI_HEAD *psHead)
{
	ged_hashtable_iterator(gs_hashtable,
		ged_kpi_find_main_head_func, (void *)NULL);
#ifdef GED_KPI_DEBUG
	/* check if there is a main rendering thread */
	/* only SF is excluded from the group of considered candidates */
	if (main_head)
		GED_LOGD("[GED_KPI] main_head =",
			"%p, i32Count= %d, i32QedBuffer_length=%d\n",
			main_head, main_head->i32Count,
			main_head->i32QedBuffer_length);
	else
		GED_LOGD("[GED_KPI] main_head = NULL\n");
#endif /* GED_KPI_DEBUG */

	if (main_head != prev_main_head && main_head == psHead) {
		ged_kpi_clean_kpi_info();
		g_pre_TimeStamp1 = psKPI->ullTimeStamp1;
		g_pre_TimeStamp2 = psKPI->ullTimeStamp2;
		g_pre_TimeStampS = psKPI->ullTimeStampS;

		prev_main_head = main_head;
		return;
	}

	if (psHead == main_head) {
		g_elapsed_time_per_sec +=
			psKPI->ullTimeStampS - g_pre_TimeStampS;
		g_gpu_time_accum += psKPI->t_gpu;
		g_CRemTimeAccu += psKPI->ullTimeStampS - psKPI->ullTimeStamp1;
		g_gpu_freq_accum += psKPI->gpu_freq;
		g_cpu_time_accum += psKPI->ullTimeStamp1 - g_pre_TimeStamp1;
		g_frame_count++;

		g_pre_TimeStamp1 = psKPI->ullTimeStamp1;
		g_pre_TimeStamp2 = psKPI->ullTimeStamp2;
		g_pre_TimeStampS = psKPI->ullTimeStampS;

		if (g_elapsed_time_per_sec >= GED_KPI_SEC_DIVIDER &&
			g_frame_count > 0) {
			unsigned long long g_fps;

			g_fps = g_frame_count;
			g_fps *= GED_KPI_SEC_DIVIDER;
			do_div(g_fps, g_elapsed_time_per_sec);
			gx_fps = g_fps;

			do_div(g_cpu_time_accum, g_frame_count);
			gx_cpu_time_avg = (unsigned int)g_cpu_time_accum;

			do_div(g_gpu_time_accum, g_frame_count);
			gx_gpu_time_avg = (unsigned int)g_gpu_time_accum;

			do_div(g_ResponseTimeAccu, g_frame_count);
			gx_response_time_avg = (unsigned int)g_ResponseTimeAccu;

			do_div(g_GRemTimeAccu, g_frame_count);
			gx_gpu_remained_time_avg = (unsigned int)g_GRemTimeAccu;

			do_div(g_CRemTimeAccu, g_frame_count);
			gx_cpu_remained_time_avg = (unsigned int)g_CRemTimeAccu;

			do_div(g_gpu_freq_accum, g_frame_count);
			gx_gpu_freq_avg = g_gpu_freq_accum;

			ged_kpi_clean_kpi_info();
		}
	}
}
/* ------------------------------------------------------------------- */
#define GED_KPI_IS_SF_SHIFT 0
#define GED_KPI_IS_SF_MASK 0x1
#define GED_KPI_QBUFFER_LEN_SHIFT 1
#define GED_KPI_QBUFFER_LEN_MASK 0x7
#define GED_KPI_DEBUG_QBUFFER_LEN_SHIFT 4
#define GED_KPI_DEBUG_QBUFFER_LEN_MASK 0x7
#define GED_KPI_GPU_UNCOMPLETED_LEN_SHIFT 7
#define GED_KPI_GPU_UNCOMPLETED_LEN_MASK 0x7
#define GED_KPI_FRC_MODE_SHIFT 10
#define GED_KPI_FRC_MODE_MASK 0x7
#define GED_KPI_FRC_CLIENT_SHIFT 13
#define GED_KPI_FRC_CLIENT_MASK 0xF
#define GED_KPI_GPU_FREQ_MAX_INFO_SHIFT 19
#define GED_KPI_GPU_FREQ_MAX_INFO_MASK 0xFFF /* max @ 4096 MHz */
#define GED_KPI_GPU_FREQ_INFO_SHIFT 7
#define GED_KPI_GPU_FREQ_INFO_MASK 0xFFF /* max @ 4096 MHz */
#define GED_KPI_GPU_LOADING_INFO_SHIFT 0
#define GED_KPI_GPU_LOADING_INFO_MASK 0x7F
static void ged_kpi_statistics_and_remove(struct GED_KPI_HEAD *psHead,
	struct GED_KPI *psKPI)
{
	u64 ulID = psKPI->ullWnd;
	unsigned long frame_attr = 0;
	unsigned long gpu_info = 0;

	struct GpuUtilization_Ex util_ex;
	unsigned int dvfs_loading_mode = 0;

	memset(&util_ex, 0, sizeof(util_ex));

	ged_kpi_calc_kpi_info(ulID, psKPI, psHead);
	frame_attr |= ((psHead->isSF & GED_KPI_IS_SF_MASK)
			<< GED_KPI_IS_SF_SHIFT);
	frame_attr |= ((psKPI->i32QedBuffer_length & GED_KPI_QBUFFER_LEN_MASK)
			<< GED_KPI_QBUFFER_LEN_SHIFT);
	frame_attr |= ((psKPI->i32DebugQedBuffer_length
			& GED_KPI_DEBUG_QBUFFER_LEN_MASK)
			<< GED_KPI_DEBUG_QBUFFER_LEN_SHIFT);
	frame_attr |= ((psKPI->i32Gpu_uncompleted
			& GED_KPI_GPU_UNCOMPLETED_LEN_MASK)
			<< GED_KPI_GPU_UNCOMPLETED_LEN_SHIFT);
	frame_attr |= ((psHead->frc_mode & GED_KPI_FRC_MODE_MASK)
			<< GED_KPI_FRC_MODE_SHIFT);
	frame_attr |= ((psHead->frc_client & GED_KPI_FRC_CLIENT_MASK)
			<< GED_KPI_FRC_CLIENT_SHIFT);
	gpu_info |= ((psKPI->gpu_freq & GED_KPI_GPU_FREQ_INFO_MASK)
			<< GED_KPI_GPU_FREQ_INFO_SHIFT);
	gpu_info |= ((psKPI->gpu_loading & GED_KPI_GPU_LOADING_INFO_MASK)
			<< GED_KPI_GPU_LOADING_INFO_SHIFT);
	gpu_info |=
		((psKPI->gpu_freq_max & GED_KPI_GPU_FREQ_MAX_INFO_MASK)
		<< GED_KPI_GPU_FREQ_MAX_INFO_SHIFT);
	psKPI->frame_attr = frame_attr;

	psKPI->cpu_gpu_info.gpu.gpu_power =
		ged_get_power_by_idx(
		ged_get_oppidx_by_freq(psKPI->gpu_freq*1000));

	ged_get_gpu_utli_ex(&util_ex);

	psKPI->cpu_gpu_info.gpu.gpu_util =
		(util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24);

	mtk_get_dvfs_loading_mode(&dvfs_loading_mode);

	psKPI->cpu_gpu_info.gpu.dvfs_loading_mode = dvfs_loading_mode;

	/* statistics */
	ged_log_buf_print(ghLogBuf_KPI,
		"%d,%llu,%lu,%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%lu,%d,%d,%lld,%d,%lld,%d,%lu,%lu,%lu,%d,%lu,%lu,%lu,%u,%u,%u,%d,%d,%d,%d,%d,%d",
		psHead->pid,
		psHead->ullWnd,
		psKPI->i32QueueID,
		psKPI->i32AcquireID,
		psKPI->frame_attr,
		psKPI->ullTimeStampD, // dequeue
		psKPI->ullTimeStamp1, // queue == acquire
		psKPI->ullTimeStamp2, // gpu_done
		psKPI->ullTimeStampP, // pre-fence
		psKPI->ullTimeStampS, // acquire buffer
		psKPI->ullTimeStampH, // legacy: HW Vsync
		gpu_info,
		psKPI->t_cpu_target,
		psKPI->t_cpu_fpsgo,
		psKPI->t_gpu,
		psKPI->gpu_done_interval,
		vsync_period,
		g_psGIFT->gift_ratio,

		psKPI->cpu_gpu_info.gpu.gpu_dvfs,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_mode,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_margin,
		psKPI->cpu_gpu_info.gpu.gpu_freq_target,
		psKPI->cpu_gpu_info.gpu.t_gpu_real,
		psKPI->cpu_gpu_info.gpu.limit_upper,
		psKPI->cpu_gpu_info.gpu.limit_lower,
		psKPI->cpu_gpu_info.gpu.dvfs_loading_mode,
		psKPI->cpu_gpu_info.gpu.gpu_util,
#ifdef GED_DCS_POLICY
		dcs_get_cur_core_num(),
#else
		psKPI->cpu_gpu_info.gpu.gpu_power,
#endif /* GED_DCS_POLICY */
#ifdef MTK_CPUFREQ
		mt_cpufreq_get_cur_freq(0) / 1000,
		mt_cpufreq_get_freq_by_idx(0, GED_KPI_CPU_MAX_OPP) / 1000,
		mt_cpufreq_get_cur_freq_idx(0),
		mt_cpufreq_get_cur_freq(1) / 1000,
		mt_cpufreq_get_freq_by_idx(1, GED_KPI_CPU_MAX_OPP) / 1000,
		mt_cpufreq_get_cur_freq_idx(1)
#else
		0, 0, 0, 0, 0, 0
#endif/* MTK_CPUFREQ */
		);
}

/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_tag_type_s(u64 ulID, struct GED_KPI_HEAD *psHead
	, struct GED_TIMESTAMP *psTimeStamp)
{
	struct GED_KPI *psKPI = NULL;
	GED_BOOL ret = GED_FALSE;

	if (psHead) {
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &psHead->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);
			if (psKPI && !(psKPI->ulMask & GED_TIMESTAMP_TYPE_S)
				&& psTimeStamp->i32FrameID == psKPI->i32QueueID)
				break;
			else
				psKPI = NULL;
		}
		if (psKPI) {
			psKPI->ulMask |= GED_TIMESTAMP_TYPE_S;
			psKPI->ullTimeStampS =
				psTimeStamp->ullTimeStamp;
			psHead->t_cpu_remained =
				psTimeStamp->ullTimeStamp
				- psKPI->ullTimeStamp1;
			psKPI->t_cpu_remained = vsync_period;
			psKPI->t_cpu_remained -=
				(psKPI->ullTimeStamp1
				- psHead->last_TimeStampS);
			psKPI->t_acquire_period =
				psTimeStamp->ullTimeStamp
				- psHead->last_TimeStampS;
			psHead->last_TimeStampS =
				psTimeStamp->ullTimeStamp;
			psKPI->i32AcquireID = psTimeStamp->i32FrameID;
			ret = GED_TRUE;
			if (psKPI && (psKPI->ulMask & GED_TIMESTAMP_TYPE_2))
				ged_kpi_statistics_and_remove(psHead, psKPI);
		} else {
#ifdef GED_KPI_DEBUG
			GED_LOGD("[GED_KPI][Exception] TYPE_S:",
				"psKPI NULL, frameID: %lu\n",
				psTimeStamp->i32FrameID);
#endif /* GED_KPI_DEBUG */
		}
		return ret;
	} else {
		return GED_FALSE;
	}
}
/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_h_iterator_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *)pvoid;
	struct GED_TIMESTAMP *psTimeStamp = (struct GED_TIMESTAMP *)pvParam;
	struct GED_KPI *psKPI = NULL;
	struct GED_KPI *psKPI_prev = NULL;

	if (psHead) {
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &psHead->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);
			if (psKPI) {
				if (psKPI->ulMask & GED_TIMESTAMP_TYPE_H) {
					if (psKPI_prev
					&& (psKPI_prev->ulMask &
					GED_TIMESTAMP_TYPE_S)
					&& (psKPI_prev->ulMask &
					GED_TIMESTAMP_TYPE_2)) {
						psKPI_prev->ulMask |=
						GED_TIMESTAMP_TYPE_H;
						psKPI_prev->ullTimeStampH =
						psTimeStamp->ullTimeStamp;

				/* Not yet precise due uncertain type_H ts*/
						psHead->t_gpu_remained =
						psTimeStamp->ullTimeStamp
						- psKPI_prev->ullTimeStamp2;
				/* fixed value since vsync period unchange */
						psKPI_prev->t_gpu_remained =
						vsync_period;
						psKPI_prev->t_gpu_remained -=
						(psKPI_prev->ullTimeStamp2
						- psHead->last_TimeStampH);

						psHead->last_TimeStampH =
						psTimeStamp->ullTimeStamp;
					}
					break;
				}
			}
			psKPI_prev = psKPI;
		}
		if (psKPI && psKPI == psKPI_prev) {
			/* (0 == (psKPI->ulMask & GED_TIMESTAMP_TYPE_H) */
			if ((psKPI->ulMask & GED_TIMESTAMP_TYPE_S)
				&& (psKPI->ulMask & GED_TIMESTAMP_TYPE_2)) {
				psKPI->ulMask |= GED_TIMESTAMP_TYPE_H;
				psKPI->ullTimeStampH =
					psTimeStamp->ullTimeStamp;

				/* Not yet precise due uncertain type_H ts*/
				psHead->t_gpu_remained =
					psTimeStamp->ullTimeStamp
					- psKPI->ullTimeStamp2;
				/* fixed value since vsync period unchange */
				psKPI->t_gpu_remained = vsync_period;
				psKPI->t_gpu_remained -=
					(psKPI->ullTimeStamp2
					- psHead->last_TimeStampH);

				psHead->last_TimeStampH =
					psTimeStamp->ullTimeStamp;
			}
		}
	}
	return GED_TRUE;
}
/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_iterator_delete_func(unsigned long ulID,
	void *pvoid, void *pvParam, GED_BOOL *pbDeleted)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *)pvoid;

	if (psHead) {
		ged_free(psHead, sizeof(struct GED_KPI_HEAD));
		*pbDeleted = GED_TRUE;
	}

	return GED_TRUE;
}
static GED_BOOL ged_kpi_update_TargetTimeAndTargetFps(
	struct GED_KPI_HEAD *psHead,
	int target_fps,
	int target_fps_margin,
	int cpu_time,
	int eara_fps_margin,
	GED_KPI_FRC_MODE_TYPE mode,
	int client)
{
	GED_BOOL ret = GED_FALSE;

	if (psHead) {
		switch (mode) {
		case GED_KPI_FRC_DEFAULT_MODE:
			psHead->frc_mode = GED_KPI_FRC_DEFAULT_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
			break;
		default:
			psHead->frc_mode = GED_KPI_FRC_DEFAULT_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
#ifdef GED_KPI_DEBUG
			GED_LOGD("[GED_KPI][Exception]: no invalid",
				"FRC mode is specified, use default mode\n");
#endif /* GED_KPI_DEBUG */
		}

		psHead->target_fps_margin = target_fps_margin;
		psHead->eara_fps_margin = eara_fps_margin;
		psHead->t_cpu_fpsgo = cpu_time;

		if (target_fps > 0 && target_fps <= GED_KPI_FPS_LIMIT) {
			psHead->t_cpu_target = (int)((int)GED_KPI_SEC_DIVIDER/target_fps);
			psHead->target_fps = target_fps;
		} else {
			psHead->t_cpu_target = (int)((int)GED_KPI_SEC_DIVIDER/g_target_fps_default);
			psHead->target_fps = -1;
		}

		psHead->t_gpu_target = psHead->t_cpu_target;
		psHead->frc_client = client;
		ret = GED_TRUE;

#ifdef GED_KPI_DEBUG
		GED_LOGI("[GED_KPI]FPSGO info PID:%d,tfps:%d,fps_margin:%d,eara_diff:%d,t_cpu:%d\n",
			psHead->pid,
			psHead->target_fps,
			psHead->target_fps_margin,
			psHead->eara_fps_margin,
			psHead->t_cpu_fpsgo);
#endif /* GED_KPI_DEBUG */
	}
	return ret;
}
/* ------------------------------------------------------------------- */
struct GED_KPI_MISS_TAG {
	u64 ulID;
	unsigned long i32FrameID;
	GED_TIMESTAMP_TYPE eTimeStampType;
	struct list_head sList;
};

#define GED_KPI_MISS_TAG_COUNT 16
static struct GED_KPI_MISS_TAG *miss_tag_head;
struct GED_KPI_MISS_TAG gs_miss_tag[GED_KPI_MISS_TAG_COUNT];
static int gs_miss_tag_idx;

static void ged_kpi_record_miss_tag(u64 ulID, int i32FrameID
	, GED_TIMESTAMP_TYPE eTimeStampType)
{
	struct GED_KPI_MISS_TAG *psMiss_tag;

	if (unlikely(miss_tag_head == NULL)) {
		miss_tag_head = (struct GED_KPI_MISS_TAG *)ged_alloc_atomic(
			sizeof(struct GED_KPI_MISS_TAG));
		if (miss_tag_head) {
			int i;

			memset(miss_tag_head, 0,
				sizeof(struct GED_KPI_MISS_TAG));
			memset(gs_miss_tag, 0, sizeof(gs_miss_tag));
			for (i = 0; i < GED_KPI_MISS_TAG_COUNT; i++)
				INIT_LIST_HEAD(&gs_miss_tag[i].sList);
			INIT_LIST_HEAD(&miss_tag_head->sList);
		} else {
			GED_PR_DEBUG("[GED_KPI][Exception]");
			GED_PR_DEBUG(
			"ged_alloc_atomic(sizeof(GED_KPI_MISS_TAG)) failed\n");
			return;
		}
	}
	psMiss_tag = &gs_miss_tag[gs_miss_tag_idx++];
	if (gs_miss_tag_idx == GED_KPI_MISS_TAG_COUNT)
		gs_miss_tag_idx = 0;
	list_del(&psMiss_tag->sList);

	if (unlikely(!psMiss_tag)) {
		GED_PR_DEBUG("GED_KPI][Exception]:");
		GED_PR_DEBUG(
		"ged_alloc_atomic(sizeof(GED_KPI_MISS_TAG)) failed\n");
		return;
	}

	memset(psMiss_tag, 0, sizeof(struct GED_KPI_MISS_TAG));
	INIT_LIST_HEAD(&psMiss_tag->sList);
	psMiss_tag->i32FrameID = i32FrameID;
	psMiss_tag->eTimeStampType = eTimeStampType;
	psMiss_tag->ulID = ulID;
	list_add_tail(&psMiss_tag->sList, &miss_tag_head->sList);
}
static GED_BOOL ged_kpi_find_and_delete_miss_tag(u64 ulID, int i32FrameID
	, GED_TIMESTAMP_TYPE eTimeStampType)
{
	GED_BOOL ret = GED_FALSE;

	if (miss_tag_head) {
		struct GED_KPI_MISS_TAG *psMiss_tag;
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &miss_tag_head->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psMiss_tag =
			list_entry(psListEntry, struct GED_KPI_MISS_TAG, sList);
			if (psMiss_tag
			&& psMiss_tag->ulID == ulID
			&& psMiss_tag->i32FrameID == i32FrameID
			&& psMiss_tag->eTimeStampType == eTimeStampType) {
				list_del(&psMiss_tag->sList);
				INIT_LIST_HEAD(&psMiss_tag->sList);
				ret = GED_TRUE;
				break;
			}
		}
	}

	return ret;
}
/* ------------------------------------------------------------------- */
/* for FB-base/LB-base mode switch */
/* ------------------------------------------------------------------- */
static int ged_kpi_check_fallback_mode(void)
{
	int i, count = 0;

	if (!main_head)
		return 1;

	/* filter systemui by checking isSF = -1*/
	for (i = 0; i < GED_KPI_TOTAL_ITEMS; i++) {
		if (g_asKPI[i].isSF == -1)
			count += 1;
	}
	count += main_head->i32Count;

	if (count * 100 / GED_KPI_TOTAL_ITEMS > g_fb_dvfs_threshold)
		return 0;

	return 1;
}
/* ------------------------------------------------------------------- */
static void ged_kpi_work_cb(struct work_struct *psWork)
{
	struct GED_TIMESTAMP *psTimeStamp =
		GED_CONTAINER_OF(psWork, struct GED_TIMESTAMP, sWork);
	struct GED_KPI_HEAD *psHead;
	struct GED_KPI *psKPI = NULL;
	u64 ulID;
	unsigned long long phead_last1;
	int target_FPS;
	unsigned long ulIRQFlags;
	int eara_fps_margin;

#ifdef GED_KPI_DEBUG
	GED_LOGD("[GED_KPI] ts type = %d, pid = %d, wnd = %llu, frame = %lu\n",
		psTimeStamp->eTimeStampType,
		psTimeStamp->pid,
		psTimeStamp->ullWnd,
		psTimeStamp->i32FrameID);
#endif /* GED_KPI_DEBUG */

	switch (psTimeStamp->eTimeStampType) {

	case GED_TIMESTAMP_TYPE_D:
		psKPI = &g_asKPI[g_i32Pos++];
		if (g_i32Pos >= GED_KPI_TOTAL_ITEMS)
			g_i32Pos = 0;

		/* remove */
		ulID = psKPI->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
						, (unsigned long)ulID);
		if (psHead) {
			psHead->i32Count -= 1;
			list_del(&psKPI->sList);

			if (psHead->i32Count < 1
			&& (psHead->sList.next == &(psHead->sList))) {
				if (psHead == main_head)
					main_head = NULL;
				spin_lock_irqsave(&gs_hashtableLock,
					ulIRQFlags);
				ged_hashtable_remove(gs_hashtable
					, (unsigned long)ulID);
				spin_unlock_irqrestore(&gs_hashtableLock,
					ulIRQFlags);
				ged_free(psHead, sizeof(struct GED_KPI_HEAD));
			}
		} else {
#ifdef GED_KPI_DEBUG
			GED_PR_DEBUG("[GED_KPI][Exception]");
			GED_PR_DEBUG("no hashtable head for ulID: %lu\n", ulID);
#endif /* GED_KPI_DEBUG */
		}

		/* reset data */
		memset(psKPI, 0, sizeof(struct GED_KPI));
		INIT_LIST_HEAD(&psKPI->sList);

		/* add */
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			psHead = (struct GED_KPI_HEAD *)
				ged_alloc_atomic(sizeof(struct GED_KPI_HEAD));
			if (psHead) {
				memset(psHead, 0, sizeof(struct GED_KPI_HEAD));
				psHead->pid = psTimeStamp->pid;
				psHead->ullWnd = psTimeStamp->ullWnd;
				psHead->isSF = psTimeStamp->isSF;
				ged_kpi_update_TargetTimeAndTargetFps(
					psHead,
					g_target_fps_default,
					GED_KPI_DEFAULT_FPS_MARGIN, 0, 0,
					GED_KPI_FRC_DEFAULT_MODE, -1);
				ged_kpi_set_gift_status(0);
				INIT_LIST_HEAD(&psHead->sList);
				spin_lock_irqsave(&gs_hashtableLock,
					ulIRQFlags);
				ged_hashtable_set(gs_hashtable
				, (unsigned long)ulID, (void *)psHead);
				spin_unlock_irqrestore(&gs_hashtableLock,
					ulIRQFlags);
			} else {
				GED_PR_DEBUG(
				"[GED_KPI][Exception] ged_alloc_atomic");
				GED_PR_DEBUG("(sizeof(GED_KPI_HEAD)) failed\n");
				goto work_cb_end;
			}
		}

		memset(psKPI, 0, sizeof(struct GED_KPI));
		psKPI->ulMask |= GED_TIMESTAMP_TYPE_D;
		psKPI->ullTimeStampD = psTimeStamp->ullTimeStamp;
		psKPI->pid = psTimeStamp->pid;
		psKPI->ullWnd = psTimeStamp->ullWnd;
		psKPI->i32DeQueueID = psTimeStamp->i32FrameID;
		psKPI->isSF = psTimeStamp->isSF;
		list_add_tail(&psKPI->sList, &psHead->sList);
		psHead->i32Count += 1;
		break;

	/* queue buffer scope */
	case GED_TIMESTAMP_TYPE_1:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (psHead) {
			struct list_head *psListEntry, *psListEntryTemp;
			struct list_head *psList = &psHead->sList;

			list_for_each_prev_safe(psListEntry,
				psListEntryTemp, psList) {
				psKPI =
				list_entry(psListEntry, struct GED_KPI, sList);

				if (psKPI
					&& (psKPI->i32DeQueueID ==
					psTimeStamp->i32FrameID))
					break;
				psKPI = NULL;
			}
			if (psKPI == NULL) {
				GED_PR_DEBUG("[GED_KPI][Exception]");
				GED_PR_DEBUG(
					"TYPE_1: psKPI NULL, frameID: %lu\n",
					psTimeStamp->i32FrameID);
				goto work_cb_end;
			}
			/* set fw idle time if display Hz change */
			/* powerhal scenario set default 5ms */
			if (idle_fw_set_flag == g_idle_set_prepare) {
				if (!g_is_idle_fw_enable) {
					mtk_set_gpu_idle(5);
					idle_fw_set_flag = g_idle_set_finish;
				} else {
					if (g_target_fps_default <= 60)
						mtk_set_gpu_idle(0);
					else
						mtk_set_gpu_idle(5);
					idle_fw_set_flag = g_idle_set_finish;
				}
			}

			/* new data */
			psKPI->pid = psTimeStamp->pid;
			psKPI->ullWnd = psTimeStamp->ullWnd;
			psKPI->i32QueueID = psTimeStamp->i32FrameID;
			psKPI->ulMask |= GED_TIMESTAMP_TYPE_1;
			psKPI->ullTimeStamp1 = psTimeStamp->ullTimeStamp;
			psKPI->i32QedBuffer_length =
				psTimeStamp->i32QedBuffer_length;
			psHead->i32QedBuffer_length =
				psTimeStamp->i32QedBuffer_length;

			/**********************************/
			psKPI->t_cpu_fpsgo = psHead->t_cpu_fpsgo;
			psKPI->t_cpu_target = psHead->t_cpu_target;
			psKPI->t_gpu_target = psHead->t_gpu_target;
			psKPI->target_fps_margin = psHead->target_fps_margin;
			psHead->i32Gpu_uncompleted++;
			psKPI->i32Gpu_uncompleted = psHead->i32Gpu_uncompleted;
			psHead->i32DebugQedBuffer_length += 1;
			psKPI->i32DebugQedBuffer_length =
				psHead->i32DebugQedBuffer_length;
			/* recording cpu time per frame & boost CPU if needed */
			phead_last1 = psHead->last_TimeStamp1;
			psHead->t_cpu_latest =
				psKPI->ullTimeStamp1 - psHead->last_TimeStamp1;
			psKPI->t_cpu = psHead->t_cpu_latest;
			ged_log_perf_trace_counter("t_cpu", psKPI->t_cpu,
				psTimeStamp->pid, psTimeStamp->i32FrameID
				, ulID);
			psKPI->QedBufferDelay = psHead->last_QedBufferDelay;
			psHead->last_QedBufferDelay = 0;
			psHead->last_TimeStamp1 = psKPI->ullTimeStamp1;

			if (ged_kpi_find_and_delete_miss_tag(ulID,
				psTimeStamp->i32FrameID,
				GED_TIMESTAMP_TYPE_S) == GED_TRUE) {

				psTimeStamp->eTimeStampType =
					GED_TIMESTAMP_TYPE_S;
				ged_kpi_tag_type_s(ulID, psHead, psTimeStamp);
				psHead->i32DebugQedBuffer_length -= 1;
#ifdef GED_KPI_DEBUG
				GED_LOGD("[GED_KPI] timestamp matched, Type_S:",
					"psHead: %p\n", psHead);
#endif /* GED_KPI_DEBUG */

				if ((main_head != NULL) &&
					(main_head->t_cpu_remained >
						(-1)*SCREEN_IDLE_PERIOD)) {
#ifdef GED_KPI_DEBUG
					GED_LOGD("[GED_KPI] t_cpu_remained",
						": %lld, main_head: %p\n",
						main_head->t_cpu_remained,
						main_head);
#endif /* GED_KPI_DEBUG */
					ged_kpi_set_cpu_remained_time(
						main_head->t_cpu_remained,
						main_head->i32QedBuffer_length);
				}
			}

#ifdef GED_KPI_DEBUG
			if (psHead->isSF != psTimeStamp->isSF)
				GED_LOGD("[GED_KPI][Exception] psHead->isSF !=",
					"psTimeStamp->isSF\n");
			if (psHead->pid != psTimeStamp->pid) {
				GED_LOGD("[GED_KPI][Exception] psHead->pid !=",
					"psTimeStamp->pid: (%d, %d)\n",
					psHead->pid, psTimeStamp->pid);
			}
			GED_LOGD("[GED_KPI] TimeStamp1, i32QedBuffer_length:",
				"%d, ts: %llu, psHead: %p\n",
				psTimeStamp->i32QedBuffer_length,
					psTimeStamp->ullTimeStamp, psHead);
#endif /* GED_KPI_DEBUG */
		}
#ifdef GED_KPI_DEBUG
		else {
			GED_PR_DEBUG("[GED_KPI][Exception]");
			GED_PR_DEBUG("no hashtable head for ulID: %lu\n", ulID);
		}
#endif /* GED_KPI_DEBUG */
		break;

	/* GPU done scope */
	case GED_TIMESTAMP_TYPE_2:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (psHead) {
			struct list_head *psListEntry, *psListEntryTemp;
			struct list_head *psList = &psHead->sList;
			static unsigned long long last_3D_done, cur_3D_done;
			int time_spent;
			static int gpu_freq_pre;

			list_for_each_prev_safe(psListEntry, psListEntryTemp,
			psList) {
				psKPI =
				list_entry(psListEntry, struct GED_KPI, sList);
				if (psKPI
				&& ((psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0)
				&& (psKPI->i32QueueID ==
				psTimeStamp->i32FrameID))
					break;
				psKPI = NULL;
			}

			if (psKPI) {
				psKPI->ulMask |= GED_TIMESTAMP_TYPE_2;
				psKPI->ullTimeStamp2 =
					psTimeStamp->ullTimeStamp;
				psKPI->cpu_gpu_info.gpu.tb_dvfs_mode =
					ged_dvfs_get_tb_dvfs_margin_mode();
				psKPI->cpu_gpu_info.gpu.tb_dvfs_margin =
					ged_dvfs_get_tb_dvfs_margin_cur();

				/* calculate gpu_pipe time */
				if (psKPI->ullTimeStamp1 >
					psHead->last_TimeStamp2
					&& psKPI->ullTimeStamp1 >
					psKPI->ullTimeStampP)
					psHead->t_gpu_latest =
						psKPI->ullTimeStamp2
						- psKPI->ullTimeStamp1;
				else if (psKPI->ullTimeStamp1 >
					psHead->last_TimeStamp2)
					psHead->t_gpu_latest =
						psKPI->ullTimeStamp2
						- psKPI->ullTimeStampP;
				else if (psHead->last_TimeStamp2 >
					psKPI->ullTimeStampP)
					psHead->t_gpu_latest =
					psKPI->ullTimeStamp2
					- psHead->last_TimeStamp2;
				else
					psHead->t_gpu_latest =
					psKPI->ullTimeStamp2
					- psKPI->ullTimeStampP;

				/* gpu info to KPI TAG*/
				psKPI->t_gpu = psHead->t_gpu_latest;

				psKPI->gpu_freq =
					ged_get_cur_freq() / 1000;
				psKPI->gpu_freq_max =
					ged_get_freq_by_idx(
					ged_get_cur_limit_idx_ceil()) / 1000;

				psHead->pre_TimeStamp2 =
					psHead->last_TimeStamp2;
				psHead->last_TimeStamp2 =
					psTimeStamp->ullTimeStamp;
				psHead->i32Gpu_uncompleted--;
				psKPI->gpu_loading = psTimeStamp->i32GPUloading;
				if (psKPI->gpu_loading == 0)
					mtk_get_gpu_loading(
						&psKPI->gpu_loading);

				psKPI->cpu_gpu_info.gpu.t_gpu_real =
					((unsigned long long)
					(psHead->last_TimeStamp2
					- psHead->pre_TimeStamp2))
					* psKPI->gpu_loading / 100U;

				psKPI->cpu_gpu_info.gpu.limit_upper =
					ged_get_cur_limiter_ceil();
				psKPI->cpu_gpu_info.gpu.limit_lower =
					ged_get_cur_limiter_floor();

				cur_3D_done = psKPI->ullTimeStamp2;
				if (psTimeStamp->i32GPUloading) {
					/* not fallback mode */

					/* choose which loading to calc. t_gpu */
					struct GpuUtilization_Ex util_ex;
					unsigned int mode = 0;

					memset(&util_ex, 0, sizeof(util_ex));

					mtk_get_dvfs_loading_mode(&mode);
					ged_get_gpu_utli_ex(&util_ex);

					/* calculate gpu time using
					 * choosed gpu loading
					 */
					if (mode == LOADING_MAX_3DTA_COM)
						psTimeStamp->i32GPUloading =
						MAX(util_ex.util_3d,
							util_ex.util_ta) +
						util_ex.util_compute;
					else if (mode == LOADING_MAX_3DTA)
						psTimeStamp->i32GPUloading =
						MAX(util_ex.util_3d,
						util_ex.util_ta);

					/* hint JS0, JS1 info to EAT */
					ged_log_perf_trace_counter("gpu_ta_loading",
					util_ex.util_ta, psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);
					ged_log_perf_trace_counter("gpu_3d_loading",
					util_ex.util_3d, psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);

					/* hint GiFT ratio to EAT */
					ged_log_perf_trace_counter(
					"is_gift_on", g_psGIFT->gift_ratio,
					psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);

					time_spent = psKPI->cpu_gpu_info.gpu.t_gpu_real;

					psKPI->gpu_done_interval = cur_3D_done - last_3D_done;

					psKPI->t_gpu =
						psHead->t_gpu_latest =
						time_spent;

					if (ged_is_fdvfs_support() &&
						psTimeStamp->pid != pid_sf &&
						psTimeStamp->pid != pid_sysui) {
						g_eb_coef = mtk_gpueb_dvfs_set_feedback_info(
							psKPI->gpu_done_interval, util_ex,
							ged_kpi_get_cur_fps());
						ged_log_perf_trace_counter("eb_coef",
							(long long)g_eb_coef, 5566, 0, 0);
					}
				} else {
					psKPI->t_gpu
						= time_spent
						= psHead->t_gpu_latest;
				}
				/* Detect if there are multi renderers by */
				/* checking if there is struct GED_KPI info
				 * resource monopoly
				 */
				g_force_gpu_dvfs_fallback = ged_kpi_check_fallback_mode();

			/* dvfs_margin_mode == */
			/* DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN or */
			/* DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN) or */
			/* DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN */
			/* bit0~bit9: headroom ratio:10-bias */
			/* bit15: is frame base? */
			/* bit16~bit23: dvfs_margin_mode */

			psKPI->cpu_gpu_info.gpu.gpu_dvfs |=
			(((unsigned long) ged_get_dvfs_margin()) & 0x3FF);
			psKPI->cpu_gpu_info.gpu.gpu_dvfs |=
			((((unsigned long) ged_get_dvfs_margin_mode()) & 0xFF)
				<< 16);

			if (psTimeStamp->pid != pid_sf && psTimeStamp->pid != pid_sysui) {
				if (main_head == psHead && !g_force_gpu_dvfs_fallback) {
					psKPI->cpu_gpu_info.gpu.gpu_dvfs |= (0x8000);
					if (ged_is_fdvfs_support())
						mtk_gpueb_dvfs_set_frame_base_dvfs(1);
				} else {
					if (ged_is_fdvfs_support())
						mtk_gpueb_dvfs_set_frame_base_dvfs(0);
				}
			}
			if (main_head == psHead) {
				if (psHead->target_fps == -1) {
					psKPI->t_gpu_target = g_target_time_default;
					ged_log_perf_trace_counter("target_fps_fb",
						g_target_fps_default, 5566, 0, 0);
				} else {
					ged_log_perf_trace_counter("target_fps_fb",
						psHead->target_fps, 5566, 0, 0);
				}
			}

			if (main_head == psHead)
				gpu_freq_pre = ged_kpi_gpu_dvfs(
					time_spent, psKPI->t_gpu_target
					, psKPI->target_fps_margin
					, g_force_gpu_dvfs_fallback);
			else if (g_force_gpu_dvfs_fallback)
				gpu_freq_pre = ged_kpi_gpu_dvfs(
					time_spent, psKPI->t_gpu_target
					, psKPI->target_fps_margin
					, 1); /* fallback mode */
			else
				/* t_gpu is not accurate, so hint -1 */
				gpu_freq_pre = ged_kpi_gpu_dvfs(
					-1, psKPI->t_gpu_target
					, psKPI->target_fps_margin
					, 0); /* do nothing */

			psKPI->cpu_gpu_info.gpu.gpu_freq_target
			= gpu_freq_pre;
			last_3D_done = cur_3D_done;

			g_psGIFT->gpu_freq_cur = psKPI->gpu_freq * 1000;
			g_psGIFT->gpu_freq_max = psKPI->gpu_freq_max * 1000;
			g_psGIFT->gpu_freq_pred = gpu_freq_pre;
			if (main_head == psHead &&
				psHead->pid == g_psGIFT->target_pid) {
				g_psGIFT->target_fps = psHead->target_fps;
				g_psGIFT->target_fps_margin = psKPI->target_fps_margin;
				g_psGIFT->eara_fps_margin = psHead->eara_fps_margin;
				g_psGIFT->gpu_time = time_spent;
			} else {
				g_psGIFT->target_fps = -1;
				g_psGIFT->target_fps_margin = 0;
				g_psGIFT->eara_fps_margin = 0;
				g_psGIFT->gpu_time = -1;
			}

			if (!g_force_gpu_dvfs_fallback)
				ged_set_backup_timer_timeout(0);
			else
				ged_set_backup_timer_timeout(
					psKPI->t_gpu_target << 1);

			ged_log_perf_trace_counter("t_gpu",
				psKPI->t_gpu, psTimeStamp->pid,
				psTimeStamp->i32FrameID, ulID);
			if (main_head == psHead)
				ged_log_perf_trace_counter("t_gpu",
					psKPI->t_gpu, 5566, 0, 0);

			if (psHead->last_TimeStamp1
				!= psKPI->ullTimeStamp1) {
				psHead->last_QedBufferDelay =
					psTimeStamp->ullTimeStamp
					- psHead->last_TimeStamp1;
			}

			if (g_force_gpu_dvfs_fallback) {
				/* hint FPSGO do not use t_gpu */
				ged_kpi_output_gfx_info2(-1
					, psKPI->gpu_freq * 1000
					, psKPI->gpu_freq_max * 1000
					, ulID);
			} else {
				ged_kpi_output_gfx_info2(
					psHead->t_gpu_latest
					, psKPI->gpu_freq * 1000
					, psKPI->gpu_freq_max * 1000
					, ulID);
			}

			if (psKPI &&
				(psKPI->ulMask & GED_TIMESTAMP_TYPE_S))
				ged_kpi_statistics_and_remove(psHead
					, psKPI);
			} else {
				GED_PR_DEBUG(
		"[GED_KPI][Exception] TYPE_2: psKPI NULL, frameID: %lu\n",
				psTimeStamp->i32FrameID);
			}
		} else {
#ifdef GED_KPI_DEBUG
			GED_PR_DEBUG(
		"[GED_KPI][Exception] no hashtable head for ulID: %lu\n",
			ulID);
#endif /* GED_KPI_DEBUG */
		}
		break;

	/* Prefence scope */
	case GED_TIMESTAMP_TYPE_P:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		// if (gx_dfps <= GED_KPI_MAX_FPS && gx_dfps >= 10)
		// 	ged_kpi_set_target_FPS(ulID, gx_dfps);
		// else
		// 	gx_dfps = 0;

		if (psHead) {
			struct list_head *psListEntry, *psListEntryTemp;
			struct list_head *psList = &psHead->sList;

			list_for_each_prev_safe(psListEntry,
				psListEntryTemp, psList) {
				psKPI =
				list_entry(psListEntry, struct GED_KPI, sList);
				if (psKPI
				&& (
				(psKPI->ulMask & GED_TIMESTAMP_TYPE_P) == 0)
				&& (
				(psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0)
				&& (
				psKPI->i32QueueID == psTimeStamp->i32FrameID))
					break;
				psKPI = NULL;

				// check for abnormal list
				if (psListEntry->prev == NULL || psListEntry->next == NULL)
					GED_LOGI(
				"[GED_KPI] PID: 0x%x, Wnd:0x%llx, count: %d. prev/next: %p/%p",
						psHead->pid, psHead->ullWnd, psHead->i32Count,
						psListEntry->prev, psListEntry->next);
			}

			if (psKPI) {
				long long pre_fence_delay;

				pre_fence_delay =
				psTimeStamp->ullTimeStamp -
					psKPI->ullTimeStamp1;
				ged_log_perf_trace_counter("t_pre_fence_delay",
				pre_fence_delay, psTimeStamp->pid,
				psTimeStamp->i32FrameID, ulID);
				psKPI->ulMask |= GED_TIMESTAMP_TYPE_P;
				psKPI->ullTimeStampP =
					psTimeStamp->ullTimeStamp;
			} else {
				ged_log_perf_trace_counter("t_pre_fence_delay",
				0, psTimeStamp->pid, psTimeStamp->i32FrameID
				, ulID);
#ifdef GED_KPI_DEBUG
				GED_LOGD(
		"[GED_KPI][Exception] TYPE_P: psKPI NULL, frameID: %lu\n",
				psTimeStamp->i32FrameID);
#endif /* GED_KPI_DEBUG */
			}
		} else {
#ifdef GED_KPI_DEBUG
			GED_LOGD(
			"[GED_KPI][Exception] no hashtable head for ulID: %lu\n",
			ulID);
#endif /* GED_KPI_DEBUG */
		}
		break;

	/* acquire buffer scope (deprecated) */
	case GED_TIMESTAMP_TYPE_S:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

#ifdef GED_KPI_DEBUG
		if (!psHead) {
			GED_PR_DEBUG(
			"[GED_KPI][Exception] TYPE_S:",
			"no hashtable head for ulID: %lu\n"
			, ulID);
		}
#endif /* GED_KPI_DEBUG */

		if (ged_kpi_tag_type_s(ulID, psHead, psTimeStamp) != GED_TRUE) {
#ifdef GED_KPI_DEBUG
			GED_LOGD("[GED_KPI] TYPE_S timestamp miss, ",
				"ulID: %lu\n", ulID);
#endif /* GED_KPI_DEBUG */
			ged_kpi_record_miss_tag(ulID, psTimeStamp->i32FrameID,
				GED_TIMESTAMP_TYPE_S);
		} else {
			psHead->i32DebugQedBuffer_length -= 1;
#ifdef GED_KPI_DEBUG
			GED_LOGD(
			"[GED_KPI] timestamp matched, Type_S: psHead: %p\n",
			psHead);
#endif /* GED_KPI_DEBUG */
		}

		if ((main_head != NULL) &&
			(main_head->t_cpu_remained > (-1)*SCREEN_IDLE_PERIOD)) {
#ifdef GED_KPI_DEBUG
			GED_LOGD(
			"[GED_KPI] t_cpu_remained: %lld, main_head: %p\n",
			main_head->t_cpu_remained, main_head);
#endif /* GED_KPI_DEBUG */
			ged_kpi_set_cpu_remained_time(main_head->t_cpu_remained,
				main_head->i32QedBuffer_length);
		}
		break;

	/* HW vsync scope (deprecated)*/
	case GED_TIMESTAMP_TYPE_H:
		ged_hashtable_iterator(gs_hashtable, ged_kpi_h_iterator_func,
			(void *)psTimeStamp);

#ifdef GED_KPI_DEBUG
		{
			long long t_cpu_remained, t_gpu_remained;
			long t_cpu_target, t_gpu_target;

			ged_kpi_get_latest_perf_state(&t_cpu_remained,
				&t_gpu_remained, &t_cpu_target, &t_gpu_target);
			GED_LOGD(
			"[GED_KPI] t_cpu: %ld, %lld, t_gpu: %ld, %lld\n",
			t_cpu_target,
			t_cpu_remained,
			t_gpu_target,
			t_gpu_remained);
		}
#endif /* GED_KPI_DEBUG */
		break;

	case GED_SET_TARGET_FPS:

		target_FPS = psTimeStamp->i32FrameID;

		ged_log_perf_trace_counter("target_fps_fpsgo",
				(target_FPS&0x000000ff), 5566, 0, 0);

		ulID = psTimeStamp->ullWnd;
		eara_fps_margin = psTimeStamp->i32QedBuffer_length;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);
		if (psHead) {
			ged_kpi_update_TargetTimeAndTargetFps(psHead,
				target_FPS&0x000000ff,
				(target_FPS&0x00000700) >> 8,
				(target_FPS&0xfffff100) >> 11,
				eara_fps_margin,
				GED_KPI_FRC_DEFAULT_MODE, -1);
		}
#ifdef GED_KPI_DEBUG
		else
			GED_LOGD("@%s: no such renderer for BQ_ID: %llu\n",
				__func__, ulID);
#endif /* GED_KPI_DEBUG */
		break;
	default:
		break;
	}
work_cb_end:
	ged_free(psTimeStamp, sizeof(struct GED_TIMESTAMP));
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_push_timestamp(
	GED_TIMESTAMP_TYPE eTimeStampType,
	u64 ullTimeStamp,
	int pid,
	unsigned long long ullWnd,
	int i32FrameID,
	int QedBuffer_length,
	int isSF,
	void *fence_addr)
{
	static atomic_t event_QedBuffer_cnt, event_3d_fence_cnt, event_hw_vsync;
	unsigned long ui32IRQFlags;

	if (g_psWorkQueue && is_GED_KPI_enabled) {
		struct GED_TIMESTAMP *psTimeStamp =
			(struct GED_TIMESTAMP *)ged_alloc_atomic(
			sizeof(struct GED_TIMESTAMP));
		unsigned int pui32Block, pui32Idle;

		if (!psTimeStamp) {
			GED_PR_DEBUG("[GED_KPI]: GED_ERROR_OOM in %s\n",
				__func__);
			return GED_ERROR_OOM;
		}

		if (eTimeStampType == GED_TIMESTAMP_TYPE_2) {
			spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);

			if (!ged_kpi_check_if_fallback_mode()
				&& !g_force_gpu_dvfs_fallback && pid != pid_sysui
				&& pid != pid_sf) {
				struct GpuUtilization_Ex util_ex;
				ged_kpi_trigger_fb_dvfs();
				ged_dvfs_cal_gpu_utilization_ex(
					&(psTimeStamp->i32GPUloading),
					&pui32Block, &pui32Idle, &util_ex);
			} else {
				psTimeStamp->i32GPUloading = 0;
			}
			spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
		}

		psTimeStamp->eTimeStampType = eTimeStampType;
		psTimeStamp->ullTimeStamp = ullTimeStamp;
		psTimeStamp->pid = pid;
		psTimeStamp->ullWnd = ullWnd;
		psTimeStamp->i32FrameID = i32FrameID;
		psTimeStamp->i32QedBuffer_length = QedBuffer_length;
		psTimeStamp->isSF = isSF;
		psTimeStamp->fence_addr = fence_addr;
		INIT_WORK(&psTimeStamp->sWork, ged_kpi_work_cb);
		queue_work(g_psWorkQueue, &psTimeStamp->sWork);
		switch (eTimeStampType) {
		case GED_TIMESTAMP_TYPE_D:
			break;
		case GED_TIMESTAMP_TYPE_1:
			ged_log_trace_counter("GED_KPI_QedBuffer_CNT",
				atomic_inc_return(&event_QedBuffer_cnt));
			ged_log_trace_counter("GED_KPI_3D_fence_CNT",
				atomic_inc_return(&event_3d_fence_cnt));
			break;
		case GED_TIMESTAMP_TYPE_2:
			ged_log_trace_counter("GED_KPI_3D_fence_CNT",
				atomic_dec_return(&event_3d_fence_cnt));
			break;
		case GED_TIMESTAMP_TYPE_P:
			break;
		case GED_TIMESTAMP_TYPE_S:
			ged_log_trace_counter("GED_KPI_QedBuffer_CNT",
				atomic_dec_return(&event_QedBuffer_cnt));
			break;
		case GED_TIMESTAMP_TYPE_H:
			ged_log_trace_counter("GED_KPI_HW_Vsync",
				atomic_read(&event_hw_vsync));
			atomic_set(&event_hw_vsync,
				(atomic_inc_return(&event_hw_vsync)%2));
			break;
		case GED_SET_TARGET_FPS:
			break;
		}
	}
#ifdef GED_KPI_DEBUG
	else {
		GED_LOGD("[GED_KPI][Exception]: g_psWorkQueue: ",
			"NULL or GED KPI is disabled\n");
		return GED_ERROR_FAIL;
	}
#endif /* GED_KPI_DEBUG */
	return GED_OK;
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeD(int pid, u64 ullWdnd, int i32FrameID, int isSF)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_D, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, isSF, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_time1(int pid, u64 ullWdnd, int i32FrameID
		, int QedBuffer_length, void *fence_addr)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_1, ged_get_time(), pid,
		ullWdnd, i32FrameID, QedBuffer_length, 0, fence_addr);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_time2(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_2, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeP(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_P, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeS(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_S, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
static
void ged_kpi_pre_fence_sync_cb(struct dma_fence *sFence,
	struct dma_fence_cb *waiter)
{
	struct GED_KPI_GPU_TS *psMonitor;

	psMonitor =
	GED_CONTAINER_OF(waiter, struct GED_KPI_GPU_TS, sSyncWaiter);

	ged_kpi_timeP(psMonitor->pid, psMonitor->ullWdnd,
		psMonitor->i32FrameID);

	dma_fence_put(psMonitor->psSyncFence);
	ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
}
/* ------------------------------------------------------------------- */
static
void ged_kpi_gpu_3d_fence_sync_cb(struct dma_fence *sFence,
	struct dma_fence_cb *waiter)
{
	struct GED_KPI_GPU_TS *psMonitor;

	psMonitor =
	GED_CONTAINER_OF(waiter, struct GED_KPI_GPU_TS, sSyncWaiter);

#if defined(MTK_GPU_BM_2)
	mtk_bandwidth_update_info(psMonitor->pid,
		qos_inc_frame_nr(),
		qos_get_frame_nr());
#endif /* MTK_GPU_BM_2 */

	ged_kpi_time2(psMonitor->pid, psMonitor->ullWdnd,
		psMonitor->i32FrameID);

	// Hint frame boundary
	if (g_ged_gpueb_support &&
		(!ged_kpi_check_if_fallback_mode() && !g_force_gpu_dvfs_fallback)
			&& psMonitor->pid != pid_sf && psMonitor->pid != pid_sysui)
		g_eb_workload = mtk_gpueb_dvfs_set_frame_done();

	dma_fence_put(psMonitor->psSyncFence);
	ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
}
#endif /* MTK_GED_KPI */
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_acquire_buffer_ts(int pid, u64 ullWdnd, int i32FrameID)
{
#ifdef MTK_GED_KPI
	GED_ERROR ret;

	ret = ged_kpi_timeS(pid, ullWdnd, i32FrameID);
	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_enabled(void)
{
#ifdef MTK_GED_KPI
	return is_GED_KPI_enabled;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_dequeue_buffer_ts(int pid, u64 ullWdnd, int i32FrameID,
					int fence_fd, int isSF)
{
#ifdef MTK_GED_KPI
	int ret;

#if defined(MTK_GPU_BM_2)
	mtk_bandwidth_check_SF(pid, isSF);
#endif /* MTK_GPU_BM_2 */

	/* For kernel 5.4, pre_fence_sync_cb will cause deadlock
	 * due to refcount = 0 after calling dma_fence_put().
	 * We remove this fence callback usage since linux community
	 * claimed the clients need to main fence_fd lifecyle
	 * themselves, while shouldn't be implemented in our kernel module
	 * Regarding the feature, "ged_kpi_timeP" refer to the feature
	 * "pre_fence_delay" and could be replaced by
	 * "wait for HWC release" in systrace provides by AOSP
	 */
	/* psSyncFence = sync_file_get_fence(fence_fd); */

	ged_kpi_timeD(pid, ullWdnd, i32FrameID, isSF);
	ret = ged_kpi_timeP(pid, ullWdnd, i32FrameID);

	if (isSF == -1 && pid != pid_sysui)
		pid_sysui = pid;

	if (isSF == 1 && pid != pid_sf)
		pid_sf = pid;

	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_queue_buffer_ts(int pid, u64 ullWdnd, int i32FrameID,
					int fence_fd, int QedBuffer_length)
{
#ifdef MTK_GED_KPI
	int ret;
	struct GED_KPI_GPU_TS *psMonitor;
	struct dma_fence *psSyncFence;

	psSyncFence = sync_file_get_fence(fence_fd);

	ret = ged_kpi_time1(pid, ullWdnd, i32FrameID, QedBuffer_length,
		(void *)psSyncFence);

	if (ret != GED_OK)
		return ret;

	psMonitor =
	(struct GED_KPI_GPU_TS *)ged_alloc(sizeof(struct GED_KPI_GPU_TS));

	if (!psMonitor) {
		pr_info("[GED_KPI]: GED_ERROR_OOM in %s\n",
			__func__);
		return GED_ERROR_OOM;
	}

	psMonitor->psSyncFence = psSyncFence;
	psMonitor->pid = pid;
	psMonitor->ullWdnd = ullWdnd;
	psMonitor->i32FrameID = i32FrameID;

	if (psMonitor->psSyncFence == NULL) {
		ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
		ret = ged_kpi_time2(pid, ullWdnd, i32FrameID);
	} else {

		ret = dma_fence_add_callback(psMonitor->psSyncFence
			, &psMonitor->sSyncWaiter
			, ged_kpi_gpu_3d_fence_sync_cb);

		if (ret < 0) {
			dma_fence_put(psMonitor->psSyncFence);
			ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
			ret = ged_kpi_time2(pid, ullWdnd, i32FrameID);
		}
	}
	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_sw_vsync(void)
{
#ifdef MTK_GED_KPI
	return GED_OK; /* disabled function*/
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_hw_vsync(void)
{
#ifdef MTK_GED_KPI
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_H,
		ged_get_time(), 0, 0, 0, 0, 0, NULL);
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void ged_kpi_get_latest_perf_state(long long *t_cpu_remained,
						long long *t_gpu_remained,
						long *t_cpu_target,
						long *t_gpu_target)
{
#ifdef MTK_GED_KPI
	if (t_cpu_remained != NULL && main_head != NULL &&
		!(main_head->t_cpu_remained < (-1)*SCREEN_IDLE_PERIOD))
		*t_cpu_remained = main_head->t_cpu_remained;

	if (t_gpu_remained != NULL && main_head != NULL &&
		!(main_head->t_gpu_remained < (-1)*SCREEN_IDLE_PERIOD))
		*t_gpu_remained = main_head->t_gpu_remained;

	if (t_cpu_target != NULL && main_head != NULL)
		*t_cpu_target = main_head->t_cpu_target;
	if (t_gpu_target != NULL && main_head != NULL)
		*t_gpu_target = main_head->t_gpu_target;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
int ged_kpi_get_uncompleted_count(void)
{
#ifdef MTK_GED_KPI
	/* return gx_i32UncompletedCount; */
	return 0; /* disabled function */
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_fps(void)
{
#ifdef MTK_GED_KPI
	return gx_fps;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_cpu_time(void)
{
#ifdef MTK_GED_KPI
	return gx_cpu_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_time(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_response_time(void)
{
#ifdef MTK_GED_KPI
	return gx_response_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_remained_time(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_remained_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_cpu_remained_time(void)
{
#ifdef MTK_GED_KPI
	return gx_cpu_remained_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_freq(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_freq_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_fw_idle(void)
{
	return g_is_idle_fw_enable;
}
/* ------------------------------------------------------------------- */
void ged_dfrc_fps_limit_cb(unsigned int target_fps)
{
	g_target_fps_default =
		(target_fps > 0 && target_fps <= GED_KPI_FPS_LIMIT) ?
		target_fps : g_target_fps_default;
	g_target_time_default = GED_KPI_SEC_DIVIDER / g_target_fps_default;
#ifdef GED_KPI_DEBUG
	GED_LOGI("[GED_KPI] dfrc_fps:%d, dfrc_time %u\n",
		g_target_fps_default, g_target_time_default);
#endif /* GED_KPI_DEBUG */

	idle_fw_set_flag = g_idle_set_prepare;

}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_system_init(void)
{
#ifdef MTK_GED_KPI
#ifndef GED_BUFFER_LOG_DISABLE
	ghLogBuf_KPI = ged_log_buf_alloc(GED_KPI_MAX_FPS * 10,
		220 * GED_KPI_MAX_FPS * 10,
		GED_LOG_BUF_TYPE_RINGBUFFER, NULL, "KPI");
#else
	ghLogBuf_KPI = 0;
#endif /* GED_BUFFER_LOG_DISABLE */
	is_GED_KPI_enabled = ged_gpufreq_bringup() ? 0 : 1;
	g_eb_workload = 0;

	g_psGIFT = (struct GED_KPI_MEOW_DVFS_FREQ_PRED *)
		ged_alloc_atomic(sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED));
	if (unlikely(!g_psGIFT)) {
		GED_PR_DEBUG("[GED_KPI][Exception]:");
		GED_PR_DEBUG(
		"ged_alloc_atomic(sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED)) failed\n");
		return GED_ERROR_FAIL;
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	drm_register_fps_chg_callback(ged_dfrc_fps_limit_cb);
#elif IS_ENABLED(CONFIG_MTK_HIGH_FRAME_RATE)
	disp_register_fps_chg_callback(ged_dfrc_fps_limit_cb);
#endif

	g_psWorkQueue =
		alloc_ordered_workqueue("ged_kpi",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	if (g_psWorkQueue) {
		int i;

		memset(g_asKPI, 0, sizeof(g_asKPI));
		for (i = 0; i < GED_KPI_TOTAL_ITEMS; i++)
			g_asKPI[i].ullWnd = 0x0 - 1;
		gs_hashtable = ged_hashtable_create(10);
		spin_lock_init(&gs_hashtableLock);

		return gs_hashtable ? GED_OK : GED_ERROR_FAIL;
	}
	return GED_ERROR_FAIL;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void ged_kpi_system_exit(void)
{
#ifdef MTK_GED_KPI
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	ged_hashtable_iterator_delete(gs_hashtable,
		ged_kpi_iterator_delete_func, NULL);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
	destroy_workqueue(g_psWorkQueue);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	drm_unregister_fps_chg_callback(ged_dfrc_fps_limit_cb);
#elif IS_ENABLED(CONFIG_MTK_HIGH_FRAME_RATE)
	disp_unregister_fps_chg_callback(ged_dfrc_fps_limit_cb);
#endif
	ged_thread_destroy(ghThread);
#ifndef GED_BUFFER_LOG_DISABLE
	ged_log_buf_free(ghLogBuf_KPI);
	ghLogBuf_KPI = 0;
#endif /* GED_BUFFER_LOG_DISABLE */
	ged_free(g_psGIFT, sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED));
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void (*ged_kpi_set_gpu_dvfs_hint_fp)(int t_gpu_target, int boost_accum_gpu);
EXPORT_SYMBOL(ged_kpi_set_gpu_dvfs_hint_fp);

bool ged_kpi_set_gpu_dvfs_hint(int t_gpu_target, int boost_accum_gpu)
{
	if (ged_kpi_set_gpu_dvfs_hint_fp != NULL) {
		ged_kpi_set_gpu_dvfs_hint_fp(t_gpu_target, boost_accum_gpu);
		return true;
	}
	return false;
}
/* ------------------------------------------------------------------- */
void (*ged_kpi_set_cpu_remained_time_fp)(long long t_cpu_remained,
	int QedBuffer_length);
EXPORT_SYMBOL(ged_kpi_set_cpu_remained_time_fp);

bool ged_kpi_set_cpu_remained_time(long long t_cpu_remained,
	int QedBuffer_length)
{
	if (ged_kpi_set_cpu_remained_time_fp != NULL) {
		ged_kpi_set_cpu_remained_time_fp(t_cpu_remained,
			QedBuffer_length);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------- */
// void ged_kpi_set_target_FPS(u64 ulID, int target_FPS)
// {
// #ifdef MTK_GED_KPI
// 	ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1,
// 				ulID, target_FPS, -1, -1, NULL);
// #endif /* MTK_GED_KPI */
// }
// EXPORT_SYMBOL(ged_kpi_set_target_FPS);
/* ------------------------------------------------------------------- */
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
		int target_FPS_margin, int eara_fps_margin, int cpu_time)
{
#ifdef MTK_GED_KPI
		ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1, ulID,
			(target_FPS | (target_FPS_margin << 8)
			| ((cpu_time/1000) << 11)), eara_fps_margin, -1, NULL);
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_set_target_FPS_margin);
/* ------------------------------------------------------------------- */
void ged_kpi_set_fw_idle(unsigned int time)
{
	g_is_idle_fw_enable = time;
	idle_fw_set_flag = g_idle_set_prepare;
}
EXPORT_SYMBOL(ged_kpi_set_fw_idle);
/* ------------------------------------------------------------------- */

static GED_BOOL ged_kpi_find_riskyBQ_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead =
		(struct GED_KPI_HEAD *)pvoid;
	struct GED_KPI_HEAD *psRiskyBQ =
		(struct GED_KPI_HEAD *)pvParam;

	if (psRiskyBQ && psHead
			&& psHead->t_gpu_latest > 0
			&& psHead->t_gpu_target > 0) {
		int t_gpu_latest;
		int t_gpu_target;
		int risk;
		int maxRisk;

		/* FPSGO skip this BQ, we should skip */
		if ((psHead->target_fps == g_target_fps_default)
			&& (psHead->target_fps_margin
			== GED_KPI_DEFAULT_FPS_MARGIN))
			return GED_TRUE;

		t_gpu_latest = ((int)psHead->t_gpu_latest) / 1000; // ns -> ms
		t_gpu_target = psHead->t_gpu_target / 1000;
		risk = t_gpu_latest * 100 / t_gpu_target;
		t_gpu_latest = ((int)psRiskyBQ->t_gpu_latest) / 1000;
		t_gpu_target = psRiskyBQ->t_gpu_target / 1000;
		maxRisk = (t_gpu_target > 0) ?
			(t_gpu_latest * 100 / t_gpu_target) : 0;

		if (risk > maxRisk)
			*psRiskyBQ = *psHead;
	}
	return GED_TRUE;
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_timer_based_pick_riskyBQ(int *pT_gpu_real, int *pT_gpu_pipe,
	int *pT_gpu_target, unsigned long long *pullWnd)
{
	GED_ERROR ret = GED_ERROR_FAIL;
	struct GED_KPI_HEAD sRiskyBQ = {0};
	unsigned int deltaTime = 0;
	unsigned int loading = 0;
	int i;
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	ged_hashtable_iterator(gs_hashtable,
		ged_kpi_find_riskyBQ_func, (void *)&sRiskyBQ);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);

	if (sRiskyBQ.ullWnd == 0
			|| sRiskyBQ.last_TimeStamp2 == 0
			|| sRiskyBQ.pre_TimeStamp2 == 0
			|| sRiskyBQ.t_gpu_latest <= 0
			|| sRiskyBQ.t_gpu_target <= 0)
		return ret;

	deltaTime = ((unsigned int)
		(sRiskyBQ.last_TimeStamp2 - sRiskyBQ.pre_TimeStamp2))
		/ 1000U; // ns -> ms

	for (i = 0; i < GED_KPI_TOTAL_ITEMS; ++i) {
		if (g_asKPI[i].ullTimeStamp2 == sRiskyBQ.last_TimeStamp2
			&& g_asKPI[i].ullWnd == sRiskyBQ.ullWnd) {
			loading = g_asKPI[i].gpu_loading;
			break;
		}
	}
	if (loading == 0)
		mtk_get_gpu_loading(&loading);

	*pT_gpu_real = deltaTime * loading / 100U;
	*pT_gpu_pipe = ((int)sRiskyBQ.t_gpu_latest) / 1000; // ns -> ms
	*pT_gpu_target = sRiskyBQ.t_gpu_target / 1000;
	*pullWnd = sRiskyBQ.ullWnd;
	ret = GED_OK;

	return ret;
}
EXPORT_SYMBOL(ged_kpi_timer_based_pick_riskyBQ);

/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_query_dvfs_freq_pred(int *gpu_freq_cur
	, int *gpu_freq_max, int *gpu_freq_pred)
{
#ifdef MTK_GED_KPI
	if (gpu_freq_cur == NULL
			|| gpu_freq_max == NULL
			|| gpu_freq_pred == NULL)
		return GED_ERROR_FAIL;

	*gpu_freq_cur = g_psGIFT->gpu_freq_cur;
	*gpu_freq_max = g_psGIFT->gpu_freq_max;
	*gpu_freq_pred = g_psGIFT->gpu_freq_pred;

	return GED_OK;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_query_dvfs_freq_pred);

/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_query_gpu_dvfs_info(
	struct GED_BRIDGE_OUT_QUERY_GPU_DVFS_INFO *out)
{
#ifdef MTK_GED_KPI
	if (out == NULL)
		return GED_ERROR_FAIL;

	out->gpu_freq_cur = g_psGIFT->gpu_freq_cur;
	out->gpu_freq_max = g_psGIFT->gpu_freq_max;
	out->gpu_freq_dvfs_pred = g_psGIFT->gpu_freq_pred;
	out->target_fps = g_psGIFT->target_fps;
	out->target_fps_margin = g_psGIFT->target_fps_margin;
	out->eara_fps_margin = g_psGIFT->eara_fps_margin;
	out->gpu_time = (g_psGIFT->gpu_time == -1) ? -1 : g_psGIFT->gpu_time / 1000;

#ifdef GED_KPI_DEBUG
	GED_LOGI("[GED_KPI] pid:%d,freq_c:%d,freq_max:%d,freq_pre:%d,tfps:%d",
		g_psGIFT->target_pid,
		out->gpu_freq_cur,
		out->gpu_freq_max,
		out->gpu_freq_dvfs_pred,
		out->target_fps);
	GED_LOGI("[GED_KPI] tfps_mar:%d,eara_diff:%d t_gpu:%d,gift_ratio:%d\n",
		out->target_fps_margin,
		out->eara_fps_margin,
		out->gpu_time,
		g_psGIFT->gift_ratio);
#endif /* GED_KPI_DEBUG */

	return GED_OK;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_query_gpu_dvfs_info);

/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_set_gift_status(int ratio)
{
#ifdef MTK_GED_KPI
	if (ratio > 0)
		g_psGIFT->gift_ratio = ratio;
	else
		g_psGIFT->gift_ratio = 0;

	return GED_OK;
#endif /* MTK_GED_KPI */
	return GED_OK;
}
EXPORT_SYMBOL(ged_kpi_set_gift_status);
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_set_gift_target_pid(int pid)
{
#ifdef MTK_GED_KPI
	if (pid != g_psGIFT->target_pid)
		g_psGIFT->target_pid = pid;
	return GED_OK;
#endif /* MTK_GED_KPI */
	return GED_OK;
}
EXPORT_SYMBOL(ged_kpi_set_gift_target_pid);
