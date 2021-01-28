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
#ifdef MTK_GED_KPI
#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>
#endif /* MTK_GED_KPI */

#ifdef GED_KPI_MET_DEBUG
#include <mt-plat/met_drv.h>
#endif /* GED_KPI_MET_DEBUG */

#ifdef GED_KPI_DFRC
#include <dfrc.h>
#include <dfrc_drv.h>
#include <ged_frr.h>
#endif /* GED_KPI_DFRC */

#include <linux/sync_file.h>
#include <linux/dma-fence.h>

#ifdef GED_ENABLE_FB_DVFS
#include <ged_notify_sw_vsync.h>
#endif /* GED_ENABLE_FB_DVFS */

int (*ged_kpi_PushAppSelfFcFp_fbt)(int is_game_control_frame_rate, pid_t pid);
EXPORT_SYMBOL(ged_kpi_PushAppSelfFcFp_fbt);

#ifdef MTK_GED_KPI

#define GED_TAG "[GED_KPI]"
#define GED_PR_DEBUG(fmt, args...)\
	pr_debug(GED_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define GED_KPI_MSEC_DIVIDER 1000000
#define GED_KPI_SEC_DIVIDER 1000000000
#define GED_KPI_MAX_FPS 60
/* set default margin to be distinct from FPSGO(0 or 3) */
#define GED_KPI_DEFAULT_FPS_MARGIN 4


#define GED_TIMESTAMP_TYPE_D    0x1
#define GED_TIMESTAMP_TYPE_1    0x2
#define GED_TIMESTAMP_TYPE_2    0x4
#define GED_TIMESTAMP_TYPE_S    0x8
#define GED_TIMESTAMP_TYPE_P    0x10
#define GED_TIMESTAMP_TYPE_H    0x20
#define GED_SET_TARGET_FPS      0x40
#define GED_TIMESTAMP_TYPE      int

#ifdef GED_KPI_DFRC
#define GED_KPI_FRC_DEFAULT_MODE    DFRC_DRV_MODE_DEFAULT
#define GED_KPI_FRC_FRR_MODE        DFRC_DRV_MODE_FRR
#define GED_KPI_FRC_ARR_MODE        DFRC_DRV_MODE_ARR
#define GED_KPI_FRC_SW_VSYNC_MODE   DFRC_DRV_MODE_INTERNAL_SW
#define GED_KPI_FRC_MODE_TYPE       int
#else
/* No frame control is applied */
#define GED_KPI_FRC_DEFAULT_MODE    0
#define GED_KPI_FRC_FRR_MODE        1
#define GED_KPI_FRC_ARR_MODE        2
#define GED_KPI_FRC_SW_VSYNC_MODE   3
#define GED_KPI_FRC_MODE_TYPE       int
#endif /* GED_KPI_DFRC */

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
	int t_cpu_target;
	int t_gpu_target;
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
	int boost_accum_cpu;
	int boost_accum_gpu;
	long long t_cpu_remained_pred;
	unsigned long long t_acquire_period;
	unsigned long long QedBufferDelay;
	union _cpu_gpu_info cpu_gpu_info;
	long long t_cpu;
	long long t_gpu;
	int t_cpu_target;
	int t_gpu_target;
	int target_fps_margin;
	int if_fallback_to_ft;

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
};







#define GED_KPI_TOTAL_ITEMS 64
#define GED_KPI_UID(pid, wnd) (pid | ((unsigned long)wnd))
#define SCREEN_IDLE_PERIOD 500000000

/* static int display_fps = GED_KPI_MAX_FPS; */
static int is_game_control_frame_rate;
static int target_fps_4_main_head = 60;
static long long vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
static GED_LOG_BUF_HANDLE ghLogBuf;
static struct workqueue_struct *g_psWorkQueue;
static GED_HASHTABLE_HANDLE gs_hashtable;
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
static spinlock_t gs_hashtableLock;
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
static struct GED_KPI g_asKPI[GED_KPI_TOTAL_ITEMS];
static int g_i32Pos;
static GED_THREAD_HANDLE ghThread;
static unsigned int gx_dfps; /* variable to fix FPS*/
static unsigned int gx_frc_mode; /* variable to fix FRC mode*/
#ifdef GED_KPI_CPU_BOOST
static unsigned int enable_cpu_boost = 1;
#endif /* GED_KPI_CPU_BOOST */
static unsigned int enable_gpu_boost = 1;
static unsigned int is_GED_KPI_enabled = 1;
static unsigned int ap_self_frc_detection_rate = 20;
#ifdef GED_ENABLE_FB_DVFS
static unsigned int g_force_gpu_dvfs_fallback;
static int g_fb_dvfs_threshold = 80;
module_param(g_fb_dvfs_threshold, int, 0644);
#endif /* GED_ENABLE_FB_DVFS */
module_param(gx_dfps, uint, 0644);
module_param(gx_frc_mode, uint, 0644);
#ifdef GED_KPI_CPU_BOOST
module_param(enable_cpu_boost, uint, 0644);
#endif /* GED_KPI_CPU_BOOST */
module_param(enable_gpu_boost, uint, 0644);
module_param(is_GED_KPI_enabled, uint, 0644);
module_param(ap_self_frc_detection_rate, uint, 0644);
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

static int gx_game_mode;
static int gx_3D_benchmark_on;
#ifdef GED_KPI_CPU_BOOST
static int gx_force_cpu_boost;
static int gx_top_app_pid;
static int enable_game_self_frc_detect;
#endif /* GED_KPI_CPU_BOOST */
static unsigned int gx_fps;
static unsigned int gx_cpu_time_avg;
static unsigned int gx_gpu_time_avg;
static unsigned int gx_response_time_avg;
static unsigned int gx_gpu_remained_time_avg;
static unsigned int gx_cpu_remained_time_avg;
static unsigned int gx_gpu_freq_avg;

#ifdef GED_KPI_CPU_BOOST
static int boost_accum_cpu;
/* for non-GED_KPI_MAX_FPS-FPS cases */
static long target_t_cpu_remained = 16000000;
/* static long target_t_cpu_remained_min = 8300000; */
/* default 0.5 vsync period */
static int cpu_boost_policy;
static int boost_extra;
static int boost_amp;
static int deboost_reduce;
static int boost_upper_bound = 100;
static void (*ged_kpi_cpu_boost_policy_fp)(struct GED_KPI_HEAD *psHead,
	struct GED_KPI *psKPI);
module_param(target_t_cpu_remained, long, 0644);
module_param(gx_force_cpu_boost, int, 0644);
module_param(gx_top_app_pid, int, 0644);
module_param(cpu_boost_policy, int, 0644);
module_param(boost_extra, int, 0644);
module_param(boost_amp, int, 0644);
module_param(deboost_reduce, int, 0644);
module_param(boost_upper_bound, int, 0644);
module_param(enable_game_self_frc_detect, int, 0644);
#endif /* GED_KPI_CPU_BOOST */
module_param(gx_game_mode, int, 0644);
module_param(gx_3D_benchmark_on, int, 0644);

int (*ged_kpi_push_game_frame_time_fp_fbt)(
	pid_t pid,
	unsigned long long last_TimeStamp,
	unsigned long long curr_TimeStamp,
	unsigned long long *pRunningTime,
	unsigned long long *pSleepTime);
EXPORT_SYMBOL(ged_kpi_push_game_frame_time_fp_fbt);

void (*ged_kpi_cpu_boost_fp_fbt)(
	long long t_cpu_cur,
	long long t_cpu_target,
	unsigned long long t_cpu_slptime,
	unsigned int target_fps);
EXPORT_SYMBOL(ged_kpi_cpu_boost_fp_fbt);

void (*ged_kpi_cpu_boost_check_01)(
	int gx_game_mode,
	int gx_force_cpu_boost,
	int enable_cpu_boost,
	int ismainhead);
EXPORT_SYMBOL(ged_kpi_cpu_boost_check_01);
/* ------------------------------------------------------------------- */
void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID);
EXPORT_SYMBOL(ged_kpi_output_gfx_info2_fp);

#ifdef GED_ENABLE_FB_DVFS
static void ged_kpi_output_gfx_info2(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID)
{
	if (ged_kpi_output_gfx_info2_fp)
		ged_kpi_output_gfx_info2_fp(t_gpu, cur_freq
						, cur_max_freq, ulID);
}
#endif /* GED_ENABLE_FB_DVFS */

void (*ged_kpi_output_gfx_info_fp)(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq);
EXPORT_SYMBOL(ged_kpi_output_gfx_info_fp);

/* ------------------------------------------------------------------- */
static void ged_kpi_output_gfx_info(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq)
{
	if (ged_kpi_output_gfx_info_fp)
		ged_kpi_output_gfx_info_fp(t_gpu, cur_freq, cur_max_freq);
}

/* ------------------------------------------------------------------- */
#ifdef GED_ENABLE_FB_DVFS
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
#endif /* GED_ENABLE_FB_DVFS */
/* ------------------------------------------------------------------- */

#ifdef GED_KPI_CPU_BOOST
/* ------------------------------------------------------------------- */
/* detect if app self frc to make frame time 16 ms */
#define GED_KPI_FALLBACK_VOTE_NUM 30
static int if_fallback_to_ft;
static int num_fallback_vote;
static int recorded_fallback_vote;
static void ged_kpi_check_fallback_main_head_reset(void)
{
	num_fallback_vote = 0;
	recorded_fallback_vote = 0;
	if_fallback_to_ft = 0;
}
static inline void ged_kpi_check_if_fallback_is_needed(int boost_value,
	int t_cpu_latest)
{
	if (if_fallback_to_ft == 0) {
		if (boost_value > 30 && t_cpu_latest < 19000000)
			num_fallback_vote++;
		recorded_fallback_vote++;
		if (recorded_fallback_vote == GED_KPI_FALLBACK_VOTE_NUM) {
			if (num_fallback_vote * 100 /
				recorded_fallback_vote > 85)
				if_fallback_to_ft = 1;
			else
				ged_kpi_check_fallback_main_head_reset();
		}
	}
}
/* ------------------------------------------------------------------- */
#define GED_KPI_GAME_SELF_FRC_DETECT_MONITOR_WINDOW_SIZE 3
#define G_K_G_S_FRC_D_M_W_S (GED_KPI_GAME_SELF_FRC_DETECT_MONITOR_WINDOW_SIZE)
static int fps_records[G_K_G_S_FRC_D_M_W_S];
static int cur_fps_idx;
static int fps_recorded_num = 1;
static int reset = 1;
static int afrc_rst_cnt_down;
static int afrc_rst_over_target_cnt;
static void ged_kpi_frc_detection_main_head_reset(int fps)
{
	cur_fps_idx = 0;
	fps_recorded_num = 1;
	target_fps_4_main_head = fps;
	afrc_rst_cnt_down = 0;
	afrc_rst_over_target_cnt = 0;
	is_game_control_frame_rate = 0;
	reset = 1;
}
static void ged_kpi_PushCurFps_and_DetectAppSelfFrc(int fps)
{
	int fps_grp[G_K_G_S_FRC_D_M_W_S];
	int i;

	if (enable_game_self_frc_detect && fps > 18 && fps <= 61) {
		fps_records[cur_fps_idx] = fps;
		if (reset == 0) {
			if (fps > target_fps_4_main_head + 1
				|| afrc_rst_cnt_down == 120) {
				ged_kpi_frc_detection_main_head_reset(60);
#ifdef GED_KPI_DEBUG
				GED_LOGD("[AFRC] reset:",
					"%d, %d, afrc_rst: %d, %d\n",
					fps, target_fps_4_main_head + 1,
					afrc_rst_cnt_down,
					afrc_rst_over_target_cnt);
#endif /* GED_KPI_DEBUG */
			} else if (afrc_rst_over_target_cnt >= 15) {
				int is_fps_grp_aligned = 1;

				for (i = 0; i < G_K_G_S_FRC_D_M_W_S; i++) {
					if (fps_records[i] <= 28)
						fps_grp[i] = 24;
					else if (fps_records[i] <= 31)
						fps_grp[i] = 30;
					else if (fps_records[i] <= 37)
						fps_grp[i] = 36;
					else if (fps_records[i] <= 45)
						fps_grp[i] = 45;
					else if (fps_records[i] <= 49)
						fps_grp[i] = 48;
					else if (fps_records[i] <= 51)
						fps_grp[i] = 50;
					else
						fps_grp[i] = 60;
				}

				for (i = 0; i < G_K_G_S_FRC_D_M_W_S - 1; i++) {
					if (fps_grp[i] != fps_grp[i + 1]) {
						is_fps_grp_aligned = 0;
						break;
					}
				}

				if (is_fps_grp_aligned) {
					target_fps_4_main_head = fps_grp[0];
					is_game_control_frame_rate = 1;
					afrc_rst_over_target_cnt = 0;
#ifdef GED_KPI_DEBUG
					GED_LOGD("[AFRC] retarget to %d",
						fps_grp[0],
						"FPS due to",
						"over target detection\n");
#endif /* GED_KPI_DEBUG */
				}

			} else {
				if (fps <= 28)
					fps = 24;
				else if (fps <= 31)
					fps = 30;
				else if (fps <= 37)
					fps = 36;
				else if (fps <= 45)
					fps = 45;
				else if (fps <= 49)
					fps = 48;
				else if (fps <= 51)
					fps = 50;
				else
					fps = 60;

				if (fps < target_fps_4_main_head)
					afrc_rst_over_target_cnt++;
				else
					afrc_rst_over_target_cnt = 0;
				afrc_rst_cnt_down++;
			}
		} else {
			if (fps_recorded_num == G_K_G_S_FRC_D_M_W_S) {
				for (i = 0; i < G_K_G_S_FRC_D_M_W_S; i++) {
					if (fps_records[i] <= 28)
						fps_grp[i] = 24;
					else if (fps_records[i] <= 31)
						fps_grp[i] = 30;
					else if (fps_records[i] <= 37)
						fps_grp[i] = 36;
					else if (fps_records[i] <= 45)
						fps_grp[i] = 45;
					else if (fps_records[i] <= 49)
						fps_grp[i] = 48;
					else if (fps_records[i] <= 51)
						fps_grp[i] = 50;
					else
						fps_grp[i] = 60;
				}

				reset = 0;
				for (i = 0; i < G_K_G_S_FRC_D_M_W_S - 1; i++) {
					if (fps_grp[i] != fps_grp[i + 1]) {
						reset = 1;
						break;
					}
				}

#ifdef GED_KPI_DEBUG
				GED_LOGD("[AFRC] fps_grp: %d, %d, %d\n",
					fps_grp[0], fps_grp[1], fps_grp[2]);
#endif /* GED_KPI_DEBUG */
				if (reset == 0 && fps_grp[0] < 60) {
					target_fps_4_main_head = fps_grp[0];
					is_game_control_frame_rate = 1;
				} else {
					reset = 1;
				}
			} else {
				fps_recorded_num++;
			}
		}
		cur_fps_idx++;
		cur_fps_idx %= G_K_G_S_FRC_D_M_W_S;
	} else {
		if (target_fps_4_main_head == 60 ||
			enable_game_self_frc_detect == 0)
			is_game_control_frame_rate = 0;
	}
#ifdef GED_KPI_DEBUG
	/* debug AFRC detection */
	GED_LOGD("[AFRC]: cur_fps:",
		"%d, fps_records: %d, %d, %d, target_fps: %d\n",
		fps, fps_records[0],
		fps_records[1], fps_records[2], target_fps_4_main_head);
	/* end of debug AFRC detection */
#endif /* GED_KPI_DEBUG */
}

#endif /* GED_KPI_CPU_BOOST */
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
void ged_kpi_main_head_reset(void)
{
#ifdef GED_KPI_CPU_BOOST
	ged_kpi_frc_detection_main_head_reset(60);
	ged_kpi_check_fallback_main_head_reset();
#endif /* GED_KPI_CPU_BOOST */
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
		ged_kpi_main_head_reset();
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
#ifdef GED_KPI_CPU_BOOST
			ged_kpi_PushCurFps_and_DetectAppSelfFrc((int)gx_fps);
#endif /* GED_KPI_CPU_BOOST */
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

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	struct GpuUtilization_Ex util_ex;
	unsigned int dvfs_loading_mode = 0;

	memset(&util_ex, 0, sizeof(util_ex));
#endif /* GED_ENABLE_DVFS_LOADING_MODE */
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

#ifdef MTK_GPUFREQ_V1
	psKPI->cpu_gpu_info.gpu.gpu_power =
		mt_gpufreq_get_power_by_idx(
		mt_gpufreq_get_opp_idx_by_freq(psKPI->gpu_freq*1000));
#else
	psKPI->cpu_gpu_info.gpu.gpu_power = 0;
#endif /* MTK_GPUFREQ_V1 */

#ifdef GED_ENABLE_DVFS_LOADING_MODE
	ged_get_gpu_utli_ex(&util_ex);

	psKPI->cpu_gpu_info.gpu.gpu_util =
		(util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24);

	mtk_get_dvfs_loading_mode(&dvfs_loading_mode);

	psKPI->cpu_gpu_info.gpu.dvfs_loading_mode = dvfs_loading_mode;
#endif /* GED_ENABLE_DVFS_LOADING_MODE */
	/* statistics */
	ged_log_buf_print(ghLogBuf,
		"%d,%llu,%lu,%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%lu,",
		psHead->pid,
		psHead->ullWnd,
		psKPI->i32QueueID,
		psKPI->i32AcquireID,
		psKPI->frame_attr,
		psKPI->ullTimeStampD,
		psKPI->ullTimeStamp1,
		psKPI->ullTimeStamp2,
		psKPI->ullTimeStampP,
		psKPI->ullTimeStampS,
		psKPI->ullTimeStampH,
		gpu_info,
		"%d,%d,%lld,%d,%lld,%lld,%llu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u,%u",
		psKPI->boost_accum_cpu,
		psKPI->boost_accum_gpu,
		psKPI->t_cpu_remained_pred,
		psKPI->t_cpu_target,
		psKPI->t_gpu,
		vsync_period,
		psKPI->QedBufferDelay,
#ifdef GED_ENABLE_FB_DVFS
		psKPI->cpu_gpu_info.gpu.gpu_dvfs,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_mode,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_margin,
		psKPI->cpu_gpu_info.gpu.t_gpu_real,
		psKPI->cpu_gpu_info.gpu.limit_upper,
		psKPI->cpu_gpu_info.gpu.limit_lower,
		psKPI->cpu_gpu_info.gpu.dvfs_loading_mode,
		psKPI->cpu_gpu_info.gpu.gpu_util,
		psKPI->cpu_gpu_info.gpu.gpu_power
#else
		psKPI->cpu_gpu_info.cpu.cpu_max_freq_LL,
		psKPI->cpu_gpu_info.cpu.cpu_max_freq_L,
		psKPI->cpu_gpu_info.cpu.cpu_max_freq_B,
		psKPI->cpu_gpu_info.cpu.cpu_cur_freq_LL,
		psKPI->cpu_gpu_info.cpu.cpu_cur_freq_L,
		psKPI->cpu_gpu_info.cpu.cpu_cur_freq_B,
		psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_LL,
		psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_L,
		psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_B
#endif /* GED_ENABLE_FB_DVFS */
		);
}
#ifdef GED_KPI_CPU_BOOST
/* ------------------------------------------------------------------- */
static inline void ged_kpi_cpu_boost_policy_0(struct GED_KPI_HEAD *psHead,
	struct GED_KPI *psKPI)
{
	long long t_cpu_cur, t_gpu_cur, t_cpu_target;
	long long t_cpu_rem_cur = 0;

	if (psHead->pid == gx_top_app_pid) {
		t_cpu_cur = psHead->t_cpu_latest;
		t_gpu_cur = psHead->t_gpu_latest;

		if ((long long)psHead->t_cpu_target > t_gpu_cur) {
			t_cpu_target = (long long)psKPI->t_cpu_target;
		} else {
			/* when GPU bound, chase GPU frame time as target */
			t_cpu_target = t_gpu_cur + 2000000; /* 2 ms buffer*/
		}

		if (psHead->target_fps == GED_KPI_MAX_FPS) {
			t_cpu_rem_cur =
			vsync_period * psHead->i32DebugQedBuffer_length;
			t_cpu_rem_cur -=
			(psKPI->ullTimeStamp1 - psHead->last_TimeStampS);
		} else {
		/* FRR mode or (default mode && FPS != GED_KPI_MAX_FPS) */
			t_cpu_rem_cur = psKPI->t_cpu_target;
			t_cpu_rem_cur -=
				(psKPI->ullTimeStamp1 -
				(long long)psHead->last_TimeStampS);
		}
		psKPI->t_cpu_remained_pred = (long long)t_cpu_rem_cur;

		if (ged_kpi_cpu_boost_fp_fbt)
			ged_kpi_cpu_boost_fp_fbt(
				t_cpu_cur,
				t_cpu_target,
				psKPI->t_cpu_slptime,
				psHead->target_fps);

		ged_kpi_check_if_fallback_is_needed(boost_accum_cpu,
			psKPI->t_cpu);

#ifdef GED_KPI_MET_DEBUG
		if (psHead->t_cpu_latest < 100*1000*1000)
			met_tag_oneshot(0, "ged_pframe_t_cpu",
			(long long)((int)psHead->t_cpu_latest + 999999) /
			GED_KPI_MSEC_DIVIDER);
		else
			met_tag_oneshot(0, "ged_pframe_t_cpu", 100);

		if (psHead->t_gpu_latest < 100*1000*1000)
			met_tag_oneshot(0, "ged_pframe_t_gpu",
			(long long)((int)psHead->t_gpu_latest + 999999) /
			GED_KPI_MSEC_DIVIDER);
		else
			met_tag_oneshot(0, "ged_pframe_t_gpu", 100);

		if (psKPI->QedBufferDelay < 100*1000*1000)
			met_tag_oneshot(0, "ged_pframe_QedBufferDelay",
			((int)psKPI->QedBufferDelay + 999999) /
			GED_KPI_MSEC_DIVIDER);
		else
			met_tag_oneshot(0, "ged_pframe_QedBufferDelay", 100);

		if (t_cpu_rem_cur < 100*1000*1000)
			met_tag_oneshot(0, "ged_pframe_t_cpu_remained_pred",
			((int)t_cpu_rem_cur + 999999) / GED_KPI_MSEC_DIVIDER);
		else
			met_tag_oneshot(0, "ged_pframe_t_cpu_remianed_pred",
			100);

		met_tag_oneshot(0, "ged_pframe_t_cpu_target",
			((int)t_cpu_target + 999999) / GED_KPI_MSEC_DIVIDER);
#endif /* GED_KPI_MET_DEBUG */
	}
}
/* ------------------------------------------------------------------- */
static inline void ged_kpi_cpu_boost(struct GED_KPI_HEAD *psHead,
	struct GED_KPI *psKPI)
{
	void (*cpu_boost_policy_fp)(struct GED_KPI_HEAD *psHead,
		struct GED_KPI *psKPI);

	cpu_boost_policy_fp = NULL;

	switch (cpu_boost_policy) {
	case 0:
		cpu_boost_policy_fp = ged_kpi_cpu_boost_policy_0;
		break;
	default:
		break;
	}

	if (ged_kpi_cpu_boost_policy_fp != cpu_boost_policy_fp) {
		ged_kpi_cpu_boost_policy_fp = cpu_boost_policy_fp;
#ifdef GED_KPI_DEBUG
		GED_LOGD("[GED_KPI] use cpu_boost_policy %d\n",
			cpu_boost_policy);
#endif /* GED_KPI_DEBUG */
	}

	if (ged_kpi_cpu_boost_policy_fp != NULL)
		ged_kpi_cpu_boost_policy_fp(psHead, psKPI);
#ifdef GED_KPI_DEBUG
	else
		GED_LOGD("[GED_KPI] no such cpu_boost_policy %d\n",
			cpu_boost_policy);
#endif /* GED_KPI_DEBUG */
}

#endif /* GED_KPI_CPU_BOOST */
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
		case GED_KPI_FRC_FRR_MODE:
			psHead->frc_mode = GED_KPI_FRC_FRR_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
			break;
		case GED_KPI_FRC_ARR_MODE:
			psHead->frc_mode = GED_KPI_FRC_ARR_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / target_fps;
			break;
		case GED_KPI_FRC_SW_VSYNC_MODE:
			psHead->frc_mode = GED_KPI_FRC_SW_VSYNC_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / target_fps;
			break;
		default:
			psHead->frc_mode = GED_KPI_FRC_DEFAULT_MODE;
			vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
#ifdef GED_KPI_DEBUG
			GED_LOGD("[GED_KPI][Exception]: no invalid",
				"FRC mode is specified, use default mode\n");
#endif /* GED_KPI_DEBUG */
		}

		if (is_game_control_frame_rate && (psHead == main_head)
			&& (psHead->frc_mode == GED_KPI_FRC_DEFAULT_MODE)
			&& (gx_3D_benchmark_on == 0))
			target_fps = target_fps_4_main_head;
		psHead->target_fps = target_fps;
		psHead->target_fps_margin = target_fps_margin;
		psHead->t_cpu_target = GED_KPI_SEC_DIVIDER/target_fps;
		psHead->t_gpu_target = psHead->t_cpu_target;
		psHead->frc_client = client;
		ret = GED_TRUE;
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
		GED_PR_DEBUG("[GED_KPI][Exception]:");
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
static void ged_kpi_work_cb(struct work_struct *psWork)
{
	struct GED_TIMESTAMP *psTimeStamp =
		GED_CONTAINER_OF(psWork, struct GED_TIMESTAMP, sWork);
	struct GED_KPI_HEAD *psHead;
	struct GED_KPI *psKPI = NULL;
	u64 ulID;
	unsigned long long phead_last1;
	int target_FPS;
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	unsigned long ulIRQFlags;
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */

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
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				spin_lock_irqsave(&gs_hashtableLock,
					ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
				ged_hashtable_remove(gs_hashtable
					, (unsigned long)ulID);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				spin_unlock_irqrestore(&gs_hashtableLock,
					ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
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
					GED_KPI_MAX_FPS,
					GED_KPI_DEFAULT_FPS_MARGIN,
					GED_KPI_FRC_DEFAULT_MODE, -1);
				INIT_LIST_HEAD(&psHead->sList);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				spin_lock_irqsave(&gs_hashtableLock,
					ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
				ged_hashtable_set(gs_hashtable
				, (unsigned long)ulID, (void *)psHead);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				spin_unlock_irqrestore(&gs_hashtableLock,
					ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
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
		list_add_tail(&psKPI->sList, &psHead->sList);
		psHead->i32Count += 1;
		break;

	case GED_TIMESTAMP_TYPE_1:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (psHead) {
#ifdef GED_KPI_DFRC
			int d_target_fps, mode, client;
#endif /* GED_KPI_DFRC */
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

			/* section to query fps from FPS policy */
#ifdef GED_KPI_DFRC
			if (dfrc_get_frr_config(psKPI->pid, 0,
				&d_target_fps, &mode, &client) == 0) {

				if (d_target_fps != 0)
					ged_kpi_update_TargetTimeAndTargetFps(
						psHead, d_target_fps,
						GED_KPI_DEFAULT_FPS_MARGIN,
						mode, client);
#ifdef GED_KPI_DEBUG
				GED_LOGD(
					"[GED_KPI] psHead: %p, fps: %d, mode: %d, client: %d\n",
					psHead, d_target_fps, mode, client);
#endif /* GED_KPI_DEBUG */
			}
#endif /* GED_KPI_DFRC */

			/**********************************/
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

#ifndef GED_ENABLE_FB_DVFS
#ifdef GED_KPI_CPU_INFO
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_LL =
			arch_scale_get_max_freq(0);
			psKPI->cpu_gpu_info.cpu.cpu_cur_freq_LL =
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_LL
			* cpufreq_scale_freq_capacity(NULL, 0) / 1024;
			psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_LL =
			(sched_get_cpu_load(0) + sched_get_cpu_load(1) +
			sched_get_cpu_load(2) + sched_get_cpu_load(3)) / 4;
#ifndef GED_KPI_CPU_SINGLE_CLUSTER
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_L =
			arch_scale_get_max_freq(4);
			psKPI->cpu_gpu_info.cpu.cpu_cur_freq_L =
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_L
			* cpufreq_scale_freq_capacity(NULL, 4) / 1024;
			psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_L =
			(sched_get_cpu_load(4) + sched_get_cpu_load(5) +
			sched_get_cpu_load(6) + sched_get_cpu_load(7)) / 4;
#ifdef GED_KPI_CPU_TRI_CLUSTER
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_B =
			arch_scale_get_max_freq(8);
			psKPI->cpu_gpu_info.cpu.cpu_cur_freq_B =
			psKPI->cpu_gpu_info.cpu.cpu_max_freq_B
			* cpufreq_scale_freq_capacity(NULL, 8) / 1024;
			psKPI->cpu_gpu_info.cpu.cpu_cur_avg_load_B =
			(sched_get_cpu_load(8)
			+ sched_get_cpu_load(9)) / 2;
#endif /* ifdef GED_KPI_CPU_TRI_CLUSTER */
#endif /* ifndef GED_KPI_CPU_SINGLE_CLUSTER */
#endif /* ifdef GED_KPI_CPU_INFO */
#endif /* ifndef GED_ENABLE_FB_DVFS */
#ifdef GED_KPI_CPU_BOOST
			if (ged_kpi_cpu_boost_check_01)
				ged_kpi_cpu_boost_check_01(
					gx_game_mode,
					gx_force_cpu_boost,
					enable_cpu_boost,
					psHead->pid == gx_top_app_pid);

			if ((gx_game_mode == 1 || gx_force_cpu_boost == 1)
				&& enable_cpu_boost == 1) {

				if (ged_kpi_push_game_frame_time_fp_fbt
					&& psHead->pid == gx_top_app_pid) {
					unsigned long long vRunningTime
						= psHead->t_cpu_latest;
					unsigned long long vSleepTime = 0;

					ged_kpi_push_game_frame_time_fp_fbt(
						psHead->pid,
						phead_last1,
						psKPI->ullTimeStamp1,
						&vRunningTime,
						&vSleepTime);
					psHead->t_cpu_latest = vRunningTime;
					psKPI->t_cpu = psHead->t_cpu_latest;
					psKPI->t_cpu_slptime = vSleepTime;
				}

				if (ged_kpi_PushAppSelfFcFp_fbt
					&& (psHead->pid == gx_top_app_pid)
					&& (gx_game_mode == 1))
					ged_kpi_PushAppSelfFcFp_fbt(
						1, psHead->pid);

				/* is_EAS_boost_off = 0; */
				ged_kpi_cpu_boost(psHead, psKPI);
			}
#endif /* GED_KPI_CPU_BOOST */
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

				if ((main_head != NULL && gx_game_mode == 1) &&
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
	case GED_TIMESTAMP_TYPE_2:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);


		if (psHead) {
			struct list_head *psListEntry, *psListEntryTemp;
			struct list_head *psList = &psHead->sList;
#ifdef GED_ENABLE_FB_DVFS
			static unsigned long long last_3D_done, cur_3D_done;
			int time_spent;
			static int gpu_freq_pre;
#endif /* GED_ENABLE_FB_DVFS */

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
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				psKPI->cpu_gpu_info.gpu.tb_dvfs_mode =
					ged_dvfs_get_tb_dvfs_margin_mode();
				psKPI->cpu_gpu_info.gpu.tb_dvfs_margin =
					ged_dvfs_get_tb_dvfs_margin_cur();
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
				/* calculate gpu time */
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

				psKPI->t_gpu = psHead->t_gpu_latest;
				psKPI->gpu_freq =
					mt_gpufreq_get_cur_freq() / 1000;
				psKPI->gpu_freq_max =
					mt_gpufreq_get_freq_by_idx(
					mt_gpufreq_get_cur_ceiling_idx())
					/ 1000;
				ged_log_perf_trace_counter("gpu_freq_max",
					(long long)psKPI->gpu_freq_max,
					psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);
				ged_log_perf_trace_counter("gpu_freq",
					(long long)psKPI->gpu_freq,
					psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);
				psHead->pre_TimeStamp2 =
					psHead->last_TimeStamp2;
				psHead->last_TimeStamp2 =
					psTimeStamp->ullTimeStamp;
				psHead->i32Gpu_uncompleted--;
				psKPI->gpu_loading = psTimeStamp->i32GPUloading;
				if (psKPI->gpu_loading == 0)
					mtk_get_gpu_loading(
						&psKPI->gpu_loading);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
				psKPI->cpu_gpu_info.gpu.t_gpu_real =
					((unsigned int)
					(psHead->last_TimeStamp2
					- psHead->pre_TimeStamp2))
					* psKPI->gpu_loading / 100U;
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
				ged_log_perf_trace_counter("gpu_loading",
					(long long)psKPI->gpu_loading,
					psTimeStamp->pid
					, psTimeStamp->i32FrameID, ulID);
#ifdef MTK_GPUFREQ_V1
				psKPI->cpu_gpu_info.gpu.limit_upper =
					mt_gpufreq_get_limit_user(1);
				psKPI->cpu_gpu_info.gpu.limit_lower =
					mt_gpufreq_get_limit_user(0);
#else
				psKPI->cpu_gpu_info.gpu.limit_upper = 0;
				psKPI->cpu_gpu_info.gpu.limit_lower = 0;
#endif /* MTK_GPUFREQ_V1 */
#ifdef GED_ENABLE_FB_DVFS
				cur_3D_done = psKPI->ullTimeStamp2;
				if (psTimeStamp->i32GPUloading) {
					/* not fallback mode */
#ifdef GED_ENABLE_DVFS_LOADING_MODE
					struct GpuUtilization_Ex util_ex;
					unsigned int mode = 0;

					memset(&util_ex, 0, sizeof(util_ex));

					mtk_get_dvfs_loading_mode(&mode);
					ged_get_gpu_utli_ex(&util_ex);

					if (mode == LOADING_MAX_3DTA_COM)
						psTimeStamp->i32GPUloading =
						MAX(util_ex.util_3d,
							util_ex.util_ta) +
						util_ex.util_compute;
					else if (mode == LOADING_MAX_3DTA)
						psTimeStamp->i32GPUloading =
						MAX(util_ex.util_3d,
						util_ex.util_ta);
#endif /* GED_ENABLE_DVFS_LOADING_MODE */
					time_spent =
					(int)(cur_3D_done - last_3D_done)
					/ 100 * psTimeStamp->i32GPUloading;

					if (time_spent > psKPI->t_gpu)
						psKPI->t_gpu =
							psHead->t_gpu_latest =
							time_spent;
					else
						time_spent = psKPI->t_gpu;
				} else {
					psKPI->t_gpu
						= time_spent
						= psHead->t_gpu_latest;
				}
				/* Detect if there are multi renderers by */
				/* checking if there is struct GED_KPI info
				 * resource monopoly
				 */
				if (main_head && main_head->i32Count * 100
					/ GED_KPI_TOTAL_ITEMS
					> g_fb_dvfs_threshold)
					g_force_gpu_dvfs_fallback = 0;
				else
					g_force_gpu_dvfs_fallback = 1;

#ifdef GED_ENABLE_DYNAMIC_DVFS_MARGIN
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

			if (!g_force_gpu_dvfs_fallback)
				psKPI->cpu_gpu_info.gpu.gpu_dvfs |= (0x8000);
#endif /* GED_ENABLE_DYNAMIC_DVFS_MARGIN */
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
				last_3D_done = cur_3D_done;

				if (!g_force_gpu_dvfs_fallback)
					ged_set_backup_timer_timeout(0);
				else
					ged_set_backup_timer_timeout(
						psKPI->t_gpu_target << 1);
#endif /* GED_ENABLE_FB_DVFS */
				ged_log_perf_trace_counter("t_gpu",
					psKPI->t_gpu, psTimeStamp->pid,
					psTimeStamp->i32FrameID, ulID);
				if (psHead->last_TimeStamp1
					!= psKPI->ullTimeStamp1) {
					psHead->last_QedBufferDelay =
						psTimeStamp->ullTimeStamp
						- psHead->last_TimeStamp1;
				}
				ged_kpi_output_gfx_info(psHead->t_gpu_latest
					, psKPI->gpu_freq * 1000
					, psKPI->gpu_freq_max * 1000);
#ifdef GED_ENABLE_FB_DVFS
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
#endif /* GED_ENABLE_FB_DVFS */
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
	case GED_TIMESTAMP_TYPE_P:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (gx_dfps <= GED_KPI_MAX_FPS && gx_dfps >= 10)
			ged_kpi_set_target_FPS(ulID, gx_dfps);
		else
			gx_dfps = 0;


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

		if ((main_head != NULL && gx_game_mode == 1) &&
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
		ulID = psTimeStamp->ullWnd;

		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);
		if (psHead) {
			ged_kpi_update_TargetTimeAndTargetFps(psHead,
				(target_FPS&0x0fff), ((target_FPS&0xf000)>>12),
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
	static int event_QedBuffer_cnt, event_3d_fence_cnt, event_hw_vsync;
#ifdef GED_ENABLE_FB_DVFS
	unsigned long ui32IRQFlags;
#endif /* GED_ENABLE_FB_DVFS */

	if (g_psWorkQueue && is_GED_KPI_enabled) {
		struct GED_TIMESTAMP *psTimeStamp =
			(struct GED_TIMESTAMP *)ged_alloc_atomic(
			sizeof(struct GED_TIMESTAMP));
#ifdef GED_ENABLE_FB_DVFS
		unsigned int pui32Block, pui32Idle;
#endif /* GED_ENABLE_FB_DVFS */

		if (!psTimeStamp) {
			GED_PR_DEBUG("[GED_KPI]: GED_ERROR_OOM in %s\n",
				__func__);
			return GED_ERROR_OOM;
		}

		if (eTimeStampType == GED_TIMESTAMP_TYPE_2) {
#ifdef GED_ENABLE_FB_DVFS
			spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);
			if (!ged_kpi_check_if_fallback_mode()
				&& !g_force_gpu_dvfs_fallback) {
#ifdef GED_ENABLE_DVFS_LOADING_MODE
				struct GpuUtilization_Ex util_ex;
#endif /* GED_ENABLE_DVFS_LOADING_MODE */
				ged_kpi_trigger_fb_dvfs();
#ifdef GED_ENABLE_DVFS_LOADING_MODE
				ged_dvfs_cal_gpu_utilization_ex(
					&(psTimeStamp->i32GPUloading),
					&pui32Block, &pui32Idle, &util_ex);
#else
				ged_dvfs_cal_gpu_utilization(
					&(psTimeStamp->i32GPUloading),
					&pui32Block, &pui32Idle);
#endif /* GED_ENABLE_DVFS_LOADING_MODE */
			} else {
				psTimeStamp->i32GPUloading = 0;
			}
			spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
#else
			psTimeStamp->i32GPUloading = 0;
#endif /* GED_ENABLE_FB_DVFS */
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
			event_QedBuffer_cnt++;
			ged_log_trace_counter("GED_KPI_QedBuffer_CNT",
				event_QedBuffer_cnt);
			event_3d_fence_cnt++;
			ged_log_trace_counter("GED_KPI_3D_fence_CNT",
				event_3d_fence_cnt);
			break;
		case GED_TIMESTAMP_TYPE_2:
			event_3d_fence_cnt--;
			ged_log_trace_counter("GED_KPI_3D_fence_CNT",
				event_3d_fence_cnt);
			break;
		case GED_TIMESTAMP_TYPE_P:
			break;
		case GED_TIMESTAMP_TYPE_S:
			event_QedBuffer_cnt--;
			ged_log_trace_counter("GED_KPI_QedBuffer_CNT",
				event_QedBuffer_cnt);
			break;
		case GED_TIMESTAMP_TYPE_H:
			ged_log_trace_counter("GED_KPI_HW_Vsync",
				event_hw_vsync);
			event_hw_vsync++;
			event_hw_vsync %= 2;
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

	ged_kpi_time2(psMonitor->pid, psMonitor->ullWdnd,
		psMonitor->i32FrameID);

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
	struct GED_KPI_GPU_TS *psMonitor;
	struct dma_fence *psSyncFence;

	psSyncFence = sync_file_get_fence(fence_fd);

	psMonitor =
	(struct GED_KPI_GPU_TS *)ged_alloc(sizeof(struct GED_KPI_GPU_TS));

	if (!psMonitor) {
		pr_info("[GED_KPI]: GED_ERROR_OOM in %s\n",
			__func__);
		return GED_ERROR_OOM;
	}

	ged_kpi_timeD(pid, ullWdnd, i32FrameID, isSF);

	psMonitor->psSyncFence = psSyncFence;
	psMonitor->pid = pid;
	psMonitor->ullWdnd = ullWdnd;
	psMonitor->i32FrameID = i32FrameID;

	if (psMonitor->psSyncFence == NULL) {
		ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
		ret = ged_kpi_timeP(pid, ullWdnd, i32FrameID);
	} else {
		ret = dma_fence_add_callback(psMonitor->psSyncFence,
			&psMonitor->sSyncWaiter, ged_kpi_pre_fence_sync_cb);

		if (ret < 0) {
			dma_fence_put(psMonitor->psSyncFence);
			ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
			ret = ged_kpi_timeP(pid, ullWdnd, i32FrameID);
		}
	}
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
GED_ERROR ged_kpi_system_init(void)
{
#ifdef MTK_GED_KPI

	ghLogBuf = ged_log_buf_alloc(GED_KPI_MAX_FPS * 10,
		220 * GED_KPI_MAX_FPS * 10,
		GED_LOG_BUF_TYPE_RINGBUFFER, NULL, "KPI");

	g_psWorkQueue =
		alloc_ordered_workqueue("ged_kpi",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	if (g_psWorkQueue) {
		int i;

		memset(g_asKPI, 0, sizeof(g_asKPI));
		for (i = 0; i < GED_KPI_TOTAL_ITEMS; i++)
			g_asKPI[i].ullWnd = 0x0 - 1;
		gs_hashtable = ged_hashtable_create(10);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
		spin_lock_init(&gs_hashtableLock);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
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
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
	ged_hashtable_iterator_delete(gs_hashtable,
		ged_kpi_iterator_delete_func, NULL);
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
	destroy_workqueue(g_psWorkQueue);
	ged_thread_destroy(ghThread);
	ged_log_buf_free(ghLogBuf);
	ghLogBuf = 0;
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
void (*ged_kpi_set_game_hint_value_fp)(int is_game_mode);
EXPORT_SYMBOL(ged_kpi_set_game_hint_value_fp);
void (*ged_kpi_set_game_hint_value_fp_2)(int is_game_mode);
EXPORT_SYMBOL(ged_kpi_set_game_hint_value_fp_2);

void (*ged_kpi_set_game_hint_value_fp_fbt)(int is_game_mode);
EXPORT_SYMBOL(ged_kpi_set_game_hint_value_fp_fbt);
void (*ged_kpi_set_game_hint_value_fp_cmmgr)(int is_game_mode);
EXPORT_SYMBOL(ged_kpi_set_game_hint_value_fp_cmmgr);


bool ged_kpi_set_game_hint_value(int is_game_mode)
{
	bool ret = false;

	if (ged_kpi_set_game_hint_value_fp != NULL) {
		ged_kpi_set_game_hint_value_fp(is_game_mode);
		ret = true;
	}
	if (ged_kpi_set_game_hint_value_fp_2 != NULL) {
		ged_kpi_set_game_hint_value_fp_2(is_game_mode);
		ret = true;
	}

	if (ged_kpi_set_game_hint_value_fp_fbt) {
		ged_kpi_set_game_hint_value_fp_fbt(is_game_mode);
		ret = true;
	}
	if (ged_kpi_set_game_hint_value_fp_cmmgr) {
		ged_kpi_set_game_hint_value_fp_cmmgr(is_game_mode);
		ret = true;
	}
	if (ged_kpi_PushAppSelfFcFp_fbt && is_game_mode == 0) {
		ged_kpi_PushAppSelfFcFp_fbt(0, -1);
		ret = true;
	}

	return ret;
}
/* ------------------------------------------------------------------- */
void ged_kpi_set_game_hint(int mode)
{
#ifdef MTK_GED_KPI
	if (mode == 1 || mode == 0) {
		gx_game_mode = mode;
		ged_kpi_set_game_hint_value(mode);
	}
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void ged_kpi_set_target_FPS(u64 ulID, int target_FPS)
{
#ifdef MTK_GED_KPI
	ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1,
				ulID, target_FPS, -1, -1, NULL);
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_set_target_FPS);
/* ------------------------------------------------------------------- */
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
		int target_FPS_margin)
{
#ifdef MTK_GED_KPI
		ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1,
			ulID, (target_FPS | (target_FPS_margin<<12)),
			-1, -1, NULL);
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_set_target_FPS_margin);
/* ------------------------------------------------------------------- */
#ifdef GED_ENABLE_TIMER_BASED_DVFS_MARGIN
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
		if ((psHead->target_fps == GED_KPI_MAX_FPS)
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
#endif /* GED_ENABLE_TIMER_BASED_DVFS_MARGIN */
