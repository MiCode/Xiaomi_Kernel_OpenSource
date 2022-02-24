/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include "disp_drv_platform.h"
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_ion.h"
#include "ion_drv.h"
#endif
#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#include "m4u_priv.h"
#endif
#include "ddp_m4u.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb.h"
#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_reg.h"
#include "disp_session.h"
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_rdma.h"
#include "ddp_manager.h"
#include "mtkfb_fence.h"
#include "display_recorder.h"
#include "fbconfig_kdebug.h"
#include "ddp_mmp.h"
#include "mtk_sync.h"
#include "ddp_irq.h"
#include "disp_session.h"
#include "disp_helper.h"
#include "ddp_reg.h"
#include "mtk_disp_mgr.h"
#include "ddp_dsi.h"
#include "mtkfb_console.h"
#if defined(CONFIG_MTK_LEGACY)
#include <mach/mtk_gpio.h>
#include <cust_gpio_usage.h>
#else
#include "disp_dts_gpio.h"
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
#include "mt-plat/aee.h"
#endif

#include "ddp_clkmgr.h"
#ifdef MTK_FB_MMDVFS_SUPPORT
//#include "mmdvfs_mgr.h"
#include "mtk_vcorefs_governor.h"
#include "mtk_vcorefs_manager.h"
#endif

#include "disp_lowpower.h"
#include "disp_recovery.h"
/* #include "mt_spm_sodi_cmdq.h" */
#include "ddp_od.h"
#include "layering_rule.h"
#include "disp_rect.h"
#include "disp_arr.h"
#include "disp_partial.h"
#include "ddp_aal.h"
#include "ddp_gamma.h"
#ifdef MTK_FB_MMDVFS_SUPPORT
#include <linux/pm_qos.h>
#endif

#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)
#define TUI_SINGLE_WINDOW_MODE (0)
#define TUI_MULTIPLE_WINDOW_MODE (1)

#define _DEBUG_DITHER_HANG_

#define FRM_UPDATE_SEQ_CACHE_NUM (DISP_INTERNAL_BUFFER_COUNT+1)

static struct disp_internal_buffer_info
	*decouple_buffer_info[DISP_INTERNAL_BUFFER_COUNT];
static struct RDMA_CONFIG_STRUCT decouple_rdma_config;
static struct WDMA_CONFIG_STRUCT decouple_wdma_config;
static struct disp_mem_output_config mem_config;
atomic_t hwc_configing = ATOMIC_INIT(0);
static unsigned int primary_session_id =
	MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
static struct disp_frm_seq_info frm_update_sequence[FRM_UPDATE_SEQ_CACHE_NUM];
static unsigned int frm_update_cnt;
static unsigned int gPresentFenceIndex;
/* 0: normal, 1: lcd only, 2: none of lcd and lcm */
unsigned int gTriggerDispMode;
static unsigned int g_keep;
static unsigned int g_skip;
#if 0 //def CONFIG_TRUSTONIC_TRUSTED_UI
static struct switch_dev disp_switch_data;
#endif

#if 0
/* global variable for idle manager */
static unsigned long long idlemgr_last_kick_time = ~(0ULL);
static int session_mode_before_enter_idle;
static int is_primary_idle;
static struct task_struct *primary_display_idlemgr_task;
static DECLARE_WAIT_QUEUE_HEAD(idlemgr_wait_queue);
#endif

static struct hrtimer cmd_mode_update_timer;
#if 0 /* defined but not used */
static ktime_t cmd_mode_update_timer_period;
#endif
static int is_fake_timer_inited;

static struct task_struct *primary_display_switch_dst_mode_task;
#ifdef CONFIG_MTK_IOMMU_V2
static struct task_struct *present_fence_release_worker_task;
#endif
static struct task_struct *primary_path_aal_task;
static struct task_struct *primary_delay_trigger_task;
static struct task_struct *primary_od_trigger_task;
static struct task_struct *decouple_update_rdma_config_thread;
static struct task_struct *decouple_trigger_thread;
static struct task_struct *init_decouple_buffer_thread;
#ifdef MTK_FB_MMDVFS_SUPPORT
struct pm_qos_request primary_display_qos_request;
struct pm_qos_request primary_display_emi_opp_request;
struct pm_qos_request primary_display_mm_freq_request;
#endif

static int decouple_mirror_update_rdma_config_thread(void *data);
static int decouple_trigger_worker_thread(void *data);

struct task_struct *primary_display_frame_update_task;
wait_queue_head_t primary_display_frame_update_wq;
atomic_t primary_display_frame_update_event = ATOMIC_INIT(0);
enum DISP_PRIMARY_PATH_MODE primary_display_mode = DIRECT_LINK_MODE;
int primary_display_def_dst_mode;
int primary_display_cur_dst_mode;
unsigned long long last_primary_trigger_time;
bool is_switched_dst_mode;
int primary_trigger_cnt;
unsigned int dynamic_fps_changed;
unsigned int arr_fps_enable;
unsigned int arr_fps_backup;
atomic_t decouple_update_rdma_event = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(decouple_update_rdma_wq);
atomic_t decouple_trigger_event = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(decouple_trigger_wq);
wait_queue_head_t primary_display_present_fence_wq;
atomic_t primary_display_pt_fence_update_event = ATOMIC_INIT(0);
static unsigned int _need_lfr_check(void);
struct Layer_draw_info *draw;

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
static int od_need_start;
#endif

/* dvfs */
#ifdef MTK_FB_MMDVFS_SUPPORT
static int dvfs_last_ovl_req = HRT_LEVEL_NUM - 1;
#endif

/* delayed trigger */
static atomic_t delayed_trigger_kick = ATOMIC_INIT(0);
static atomic_t od_trigger_kick = ATOMIC_INIT(0);

/* record take mutex time */
static unsigned long long mutex_time_start;
static unsigned long long mutex_time_end;
static unsigned long long mutex_time_end1;
static long long mutex_time_period;
static long long mutex_time_period1;

unsigned int round_corner_offset_enable;
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
unsigned int lcm_corner_en;
unsigned int full_content;
unsigned int top_mva, bottom_mva;
unsigned int corner_pattern_width, corner_pattern_height;
unsigned int corner_pattern_height_bot;
static int primary_display_get_round_corner_mva(
	unsigned int *tp_mva, unsigned int *bt_mva,
	unsigned int *pitch, unsigned int *height, unsigned int *height_bot);
#endif

/* hold the wakelock to make kernel awake when primary display is on*/
struct wakeup_source pri_wk_lock;

static int smart_ovl_try_switch_mode_nolock(void);

struct display_primary_path_context *_get_context(void)
{
	static int is_context_inited;
	static struct display_primary_path_context g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0,
			sizeof(struct display_primary_path_context));
		is_context_inited = 1;
	}

	return &g_context;
}

void _primary_path_lock(const char *caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&(pgc->lock));
	mutex_time_start = sched_clock();
	pgc->mutex_locker = (char *)caller;
}

void _primary_path_unlock(const char *caller)
{
	pgc->mutex_locker = NULL;

	mutex_time_end = sched_clock();
	mutex_time_period = mutex_time_end - mutex_time_start;
	if (mutex_time_period > 300000000) {
		DISPCHECK("mutex_release_timeout1 <%lld ns>\n",
			mutex_time_period);
		dump_stack();
	}

	disp_sw_mutex_unlock(&(pgc->lock));

	mutex_time_end1 = sched_clock();
	mutex_time_period1 = mutex_time_end1 - mutex_time_start;
	if ((mutex_time_period < 300000000 && mutex_time_period1 > 300000000) ||
	   (mutex_time_period < 300000000 && mutex_time_period1 < 0)) {
		DISPCHECK("mutex_release_timeout2 <%lld ns>,<%lld ns>\n",
			mutex_time_period1, mutex_time_period);
		dump_stack();
	}

	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

static const char *session_mode_spy(unsigned int mode)
{
	switch (mode) {
	case DISP_SESSION_DIRECT_LINK_MODE:
		return "DIRECT_LINK";
	case DISP_SESSION_DIRECT_LINK_MIRROR_MODE:
		return "DIRECT_LINK_MIRROR";
	case DISP_SESSION_DECOUPLE_MODE:
		return "DECOUPLE";
	case DISP_SESSION_DECOUPLE_MIRROR_MODE:
		return "DECOUPLE_MIRROR";
	case DISP_SESSION_RDMA_MODE:
		return "RDMA_MODE";
	default:
		return "UNKNOWN";
	}
}

int primary_display_is_directlink_mode(void)
{
	enum DISP_MODE mode = pgc->session_mode;

	if (mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
	    mode == DISP_SESSION_DIRECT_LINK_MODE)
		return 1;

	return 0;
}

enum DISP_MODE primary_get_sess_mode(void)
{
	return pgc->session_mode;
}

unsigned int primary_get_sess_id(void)
{
	return pgc->session_id;
}

struct disp_lcm_handle *primary_get_lcm(void)
{
	return pgc->plcm;
}

void *primary_get_dpmgr_handle(void)
{
	return pgc->dpmgr_handle;
}

void *primary_get_ovl2mem_handle(void)
{
	return pgc->ovl2mem_path_handle;
}

int primary_display_is_decouple_mode(void)
{
	enum DISP_MODE mode = pgc->session_mode;

	if (mode == DISP_SESSION_DECOUPLE_MODE ||
	    mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;

	return 0;
}

int _is_mirror_mode(enum DISP_MODE mode)
{
	if (mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
	    mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;

	return 0;
}
int primary_display_is_mirror_mode(void)
{
	return _is_mirror_mode(pgc->session_mode);
}

int primary_is_sec(void)
{
	return pgc->is_primary_sec;
}

void _primary_path_switch_dst_lock(void)
{
	mutex_lock(&(pgc->switch_dst_lock));
}

void _primary_path_switch_dst_unlock(void)
{
	mutex_unlock(&(pgc->switch_dst_lock));
}

int primary_display_config_full_roi(struct disp_ddp_path_config *pconfig,
	disp_path_handle disp_handle, struct cmdqRecStruct *cmdq_handle)
{
	struct disp_rect total_dirty_roi = { 0, 0, 0, 0};

	if (!disp_partial_is_support())
		return -1;

	assign_full_lcm_roi(&total_dirty_roi);
	if (!rect_equal(&total_dirty_roi, &pconfig->ovl_partial_roi)) {
		pconfig->ovl_partial_roi = total_dirty_roi;
		dpmgr_path_update_partial_roi(disp_handle,
					total_dirty_roi, cmdq_handle);
		if (disp_helper_get_option(
			DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING)) {
			/* update rdma goden settin*/
			set_rdma_width_height(total_dirty_roi.width,
				total_dirty_roi.height);
			dpmgr_path_ioctl(disp_handle, cmdq_handle,
				DDP_RDMA_GOLDEN_SETTING, pconfig);
		}
		/* update ovl to full layer */
		pconfig->ovl_dirty = 1;
		pconfig->ovl_partial_dirty = 0;
		dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
		pconfig->ovl_layer_scanned = 0;
		pconfig->ovl_partial_dirty = 0;
		pconfig->ovl_dirty = 0;
	}
	return 0;
}

static int _disp_primary_path_switch_dst_mode_thread(void *data)
{
	while (1) {
		msleep(1000);
		/* 500ms not trigger disp */
		if (((sched_clock() - last_primary_trigger_time) / 1000)
			> 500000) {
			/* switch to cmd mode */
			primary_display_switch_dst_mode(0);
			is_switched_dst_mode = true;
		}

		if (kthread_should_stop())
			break;
	}
	return 0;
}

static DECLARE_WAIT_QUEUE_HEAD(display_state_wait_queue);

enum DISP_POWER_STATE primary_get_state(void)
{
	return pgc->state;
}
static enum DISP_POWER_STATE primary_set_state(enum DISP_POWER_STATE new_state)
{
	enum DISP_POWER_STATE old_state = pgc->state;

	pgc->state = new_state;
	DISPINFO("%s %d to %d\n", __func__, old_state, new_state);
	wake_up(&display_state_wait_queue);
	return old_state;
}

/* AOD power mode API */
enum mtkfb_power_mode primary_display_set_power_mode_nolock(
	enum mtkfb_power_mode new_mode)
{
	enum mtkfb_power_mode prev_mode;

	prev_mode = pgc->pm;
	pgc->pm = new_mode;

	return prev_mode;
}

enum mtkfb_power_mode primary_display_set_power_mode(
	enum mtkfb_power_mode new_mode)
{
	enum mtkfb_power_mode prev_mode;

	_primary_path_lock(__func__);
	prev_mode = primary_display_set_power_mode_nolock(new_mode);
	_primary_path_unlock(__func__);

	return prev_mode;
}

enum mtkfb_power_mode primary_display_get_power_mode_nolock(void)
{
	return pgc->pm;
}

enum mtkfb_power_mode primary_display_get_power_mode(void)
{
	enum mtkfb_power_mode mode = MTKFB_POWER_MODE_UNKNOWN;

	_primary_path_lock(__func__);
	mode = primary_display_get_power_mode_nolock();
	_primary_path_unlock(__func__);

	return mode;
}

bool primary_is_aod_supported(void)
{
	if (disp_helper_get_option(DISP_OPT_AOD) &&
		!disp_lcm_is_video_mode(pgc->plcm))
		return 1;

	return 0;
}

/* LCM power state API */
enum lcm_power_state primary_display_set_lcm_power_state_nolock(
	enum lcm_power_state new_state)
{
	enum lcm_power_state prev_state;

	prev_state = pgc->lcm_ps;
	pgc->lcm_ps = new_state;

	return prev_state;
}

enum lcm_power_state primary_display_set_power_state(
	enum lcm_power_state new_state)
{
	enum lcm_power_state prev_state;

	_primary_path_lock(__func__);
	prev_state = primary_display_set_lcm_power_state_nolock(new_state);
	_primary_path_unlock(__func__);

	return prev_state;
}

enum lcm_power_state primary_display_get_lcm_power_state_nolock(void)
{
	return pgc->lcm_ps;
}

enum lcm_power_state primary_display_get_lcm_power_state(void)
{
	enum lcm_power_state ps = LCM_POWER_STATE_UNKNOWN;

	_primary_path_lock(__func__);
	ps = primary_display_get_lcm_power_state_nolock();
	_primary_path_unlock(__func__);

	return ps;
}

enum mtkfb_power_mode primary_display_check_power_mode(void)
{
	if (primary_display_is_sleepd() &&
		(primary_display_get_lcm_power_state() == LCM_OFF))
		return FB_SUSPEND;
	else if (primary_display_is_alive() &&
		(primary_display_get_lcm_power_state() == LCM_ON))
		return FB_RESUME;
	else if (primary_display_is_sleepd() &&
		(primary_display_get_lcm_power_state() == LCM_ON_LOW_POWER))
		return DOZE_SUSPEND;
	else if (primary_display_is_alive() &&
		(primary_display_get_lcm_power_state() == LCM_ON_LOW_POWER))
		return DOZE;

	return MTKFB_POWER_MODE_UNKNOWN;
}

void debug_print_power_mode_check(enum mtkfb_power_mode prev,
	enum mtkfb_power_mode cur)
{
	if (primary_display_check_power_mode() != cur)
		DISPERR(
		"AOD check error: fail to set FB power mode %s to %s(but now %s)\n",
			power_mode_to_string(prev), power_mode_to_string(cur),
			power_mode_to_string(
			primary_display_check_power_mode()));
	else
		DISPCHECK("AOD check: succeed to set FB power mode %s to %s\n",
		  power_mode_to_string(prev),
		  power_mode_to_string(primary_display_check_power_mode()));
}

/* use MAX_SCHEDULE_TIMEOUT to wait for ever
 * NOTES: primary_path_lock should NOT be held when call this func !!!!!!!!
 */
#define __primary_display_wait_state(condition, timeout) \
	wait_event_timeout(display_state_wait_queue, condition, timeout)

long primary_display_wait_state(enum DISP_POWER_STATE state, long timeout)
{
	long ret;

	ret = __primary_display_wait_state(
		primary_get_state() == state, timeout);
	return ret;
}

long primary_display_wait_not_state(enum DISP_POWER_STATE state, long timeout)
{
	long ret;

	ret = __primary_display_wait_state(
		primary_get_state() != state, timeout);
	return ret;
}

int dynamic_debug_msg_print(unsigned int mva, int w, int h, int pitch,
	int bytes_per_pix)
{
#if 0 //def CONFIG_MTK_M4U
	int ret = -1;
	unsigned int layer_size = pitch * h;
	unsigned int real_mva = 0;
	unsigned long kva = 0;
	unsigned int real_size = 0, mapped_size = 0;

	static MFC_HANDLE mfc_handle;

	if (disp_helper_get_option(DISP_OPT_SHOW_VISUAL_DEBUG_INFO)) {
		ret = m4u_query_mva_info(0, mva, layer_size,
					&real_mva, &real_size);
		if (ret < 0) {
			pr_debug("m4u_query_mva_info error\n");
			return -1;
		}
		ret = m4u_mva_map_kernel(real_mva, real_size,
			&kva, &mapped_size);
		if (ret < 0) {
			pr_debug("m4u_mva_map_kernel fail.\n");
			return -1;
		}
		if (layer_size > mapped_size) {
			pr_debug("warning: layer size > mapped size\n");
			goto err1;
		}

		ret = MFC_Open(&mfc_handle,
			       (void *)kva,
			       pitch,
			       h,
			       bytes_per_pix,
			       DAL_COLOR_WHITE,
			       DAL_COLOR_RED);
		if (ret != MFC_STATUS_OK)
			goto err1;
		screen_logger_print(mfc_handle);
		MFC_Close(mfc_handle);
err1:
		m4u_mva_unmap_kernel(real_mva, real_size, kva);
	}
#endif
	return 0;
}

static int primary_show_basic_debug_info(struct disp_frame_cfg_t *cfg)
{
	int i;
	struct fpsEx fps;
	char disp_tmp[20];
	int dst_layer_id = 0;

	dprec_logger_get_result_value(DPREC_LOGGER_RDMA0_TRANSFER_1SECOND,
		&fps);
	snprintf(disp_tmp, sizeof(disp_tmp), ",rdma_fps:%lld.%02lld,",
		fps.fps, fps.fps_low);
	screen_logger_add_message("rdma_fps", MESSAGE_REPLACE, disp_tmp);

	dprec_logger_get_result_value(DPREC_LOGGER_OVL_FRAME_COMPLETE_1SECOND,
		&fps);
	snprintf(disp_tmp, sizeof(disp_tmp), "ovl_fps:%lld.%02lld,",
		fps.fps, fps.fps_low);
	screen_logger_add_message("ovl_fps", MESSAGE_REPLACE, disp_tmp);

	dprec_logger_get_result_value(DPREC_LOGGER_PQ_TRIGGER_1SECOND, &fps);
	snprintf(disp_tmp, sizeof(disp_tmp), "PQ_trigger:%lld.%02lld,",
		fps.fps, fps.fps_low);
	screen_logger_add_message("PQ trigger", MESSAGE_REPLACE, disp_tmp);

	snprintf(disp_tmp, sizeof(disp_tmp),
		primary_display_is_video_mode() ? "vdo," : "cmd,");
	screen_logger_add_message("mode", MESSAGE_REPLACE, disp_tmp);

	for (i = 0; TOTAL_OVL_LAYER_NUM; i++) {
		if (cfg->input_cfg[i].tgt_offset_y == 0 &&
		    cfg->input_cfg[i].layer_enable) {
			dst_layer_id =
				dst_layer_id > cfg->input_cfg[i].layer_id ?
				dst_layer_id : cfg->input_cfg[i].layer_id;
		}
	}

	dynamic_debug_msg_print(
		(unsigned long)cfg->input_cfg[dst_layer_id].src_phy_addr,
		cfg->input_cfg[dst_layer_id].tgt_width,
		cfg->input_cfg[dst_layer_id].tgt_height,
		cfg->input_cfg[dst_layer_id].src_pitch,
		cfg->input_cfg[dst_layer_id].src_fmt & 0x7);
	return 0;
}

/*************************** fps calculate ************************/
#define FPS_ARRAY_SZ	30
struct fps_ctx_t {
	unsigned long long last_trig;
	unsigned int array[FPS_ARRAY_SZ];
	unsigned int total;
	unsigned int wnd_sz;
	unsigned int cur_wnd_sz;
	struct mutex lock;
	int is_inited;
} primary_fps_ctx;

#define INTERVAL_MS	10000	/* 10 sec */
struct fps_ext_ctx_t {
	unsigned long long last_trig;
	unsigned long long total;
	unsigned long long interval;	/* ns */
	unsigned int fps;
	unsigned int stable;
	unsigned int frequency;
	struct mutex lock;
	int is_inited;
} primary_fps_ext_ctx;

#if 0 /* defined but not used */
static struct task_struct *fps_monitor;
static int fps_monitor_thread(void *data);
#endif

static int _fps_ctx_reset(struct fps_ctx_t *fps_ctx, int reserve_num)
{
	int i;

	if (reserve_num >= FPS_ARRAY_SZ) {
		pr_info("%s error to reset, reserve=%d\n",
			__func__, reserve_num);
		WARN_ON(1);
	}
	for (i = reserve_num; i < FPS_ARRAY_SZ; i++)
		fps_ctx->array[i] = 0;

	if (reserve_num < fps_ctx->cur_wnd_sz)
		fps_ctx->cur_wnd_sz = reserve_num;

	/* re-calc total */
	fps_ctx->total = 0;
	for (i = 0; i < fps_ctx->cur_wnd_sz; i++)
		fps_ctx->total += fps_ctx->array[i];

	return 0;
}

static int _fps_ctx_update(struct fps_ctx_t *fps_ctx, unsigned int fps,
	unsigned long long time_ns)
{
	int i;

	fps_ctx->total -= fps_ctx->array[fps_ctx->wnd_sz-1];

	for (i = fps_ctx->wnd_sz - 1; i > 0; i--)
		fps_ctx->array[i] = fps_ctx->array[i-1];

	fps_ctx->array[0] = fps;

	fps_ctx->total += fps_ctx->array[0];
	if (fps_ctx->cur_wnd_sz < fps_ctx->wnd_sz)
		fps_ctx->cur_wnd_sz++;

	fps_ctx->last_trig = time_ns;

	return 0;
}

static int fps_ctx_init(struct fps_ctx_t *fps_ctx, int wnd_sz)
{
	if (fps_ctx->is_inited)
		return 0;

	memset(fps_ctx, 0, sizeof(*fps_ctx));
	mutex_init(&fps_ctx->lock);

	if (wnd_sz > FPS_ARRAY_SZ) {
		pr_info("%s error: wnd_sz = %d\n", __func__, wnd_sz);
		wnd_sz = FPS_ARRAY_SZ;
	}
	fps_ctx->wnd_sz = wnd_sz;

	fps_ctx->is_inited = 1;

	return 0;
}

static unsigned int _fps_ctx_calc_cur_fps(struct fps_ctx_t *fps_ctx,
	unsigned long long cur_ns)
{
	unsigned long long delta;
	unsigned long long fps = 1000000000;

	if (cur_ns > fps_ctx->last_trig) {
		delta = cur_ns - fps_ctx->last_trig;
		do_div(fps, delta);
	}

	if (fps > 120ULL)
		fps = 120ULL;

	return (unsigned int)fps;
}

static unsigned int _fps_ctx_get_avg_fps(struct fps_ctx_t *fps_ctx)
{
	unsigned int avg_fps;

	if (fps_ctx->cur_wnd_sz == 0)
		return 0;
	avg_fps = fps_ctx->total / fps_ctx->cur_wnd_sz;
	return avg_fps;
}

static unsigned int _fps_ctx_get_avg_fps_ext(struct fps_ctx_t *fps_ctx,
	unsigned int abs_fps)
{
	unsigned int avg_fps;

	avg_fps = (fps_ctx->total + abs_fps) / (fps_ctx->cur_wnd_sz + 1);
	return avg_fps;
}

static int fps_ctx_update(struct fps_ctx_t *fps_ctx)
{
	unsigned int abs_fps, avg_fps;
	unsigned long long ns = sched_clock();

	mutex_lock(&fps_ctx->lock);
	abs_fps = _fps_ctx_calc_cur_fps(fps_ctx, ns);

	/* _fps_ctx_check_drastic_change(fps_ctx, abs_fps); */

	avg_fps = _fps_ctx_get_avg_fps(fps_ctx);

	/* if long time no trigger, ex:500ms
	 * abs_fps=1, avg_fps may be 58
	 * we should let avg_fps decline rapidly
	 */

	if (abs_fps < avg_fps / 2 && avg_fps > 10)
		_fps_ctx_reset(fps_ctx, 0);

	_fps_ctx_update(fps_ctx, abs_fps, ns);

	mmprofile_log_ex(ddp_mmp_get_events()->fps_set, MMPROFILE_FLAG_PULSE,
		abs_fps, fps_ctx->cur_wnd_sz);
	mutex_unlock(&fps_ctx->lock);
	return 0;
}

static int fps_ctx_get_fps(struct fps_ctx_t *fps_ctx, unsigned int *fps,
	int *stable)
{
	unsigned long long ns = sched_clock();
	unsigned int abs_fps = 0, avg_fps = 0;

	*stable = 1;
	mutex_lock(&fps_ctx->lock);

	abs_fps = _fps_ctx_calc_cur_fps(fps_ctx, ns);
	avg_fps = _fps_ctx_get_avg_fps(fps_ctx);

	/* if long time no trigger, we should pull fps down */
	if (abs_fps < avg_fps/2 && avg_fps > 10) {
		_fps_ctx_reset(fps_ctx, 0);
		*fps = abs_fps;
		*stable = 0;
		goto done;
	}

	if (fps_ctx->cur_wnd_sz < fps_ctx->wnd_sz/2)
		*stable = 0;

	if (abs_fps < avg_fps)
		*fps = _fps_ctx_get_avg_fps_ext(fps_ctx, abs_fps);
	else
		*fps = avg_fps;

done:
	mmprofile_log_ex(ddp_mmp_get_events()->fps_get, MMPROFILE_FLAG_PULSE,
		*fps, *stable);
	mutex_unlock(&fps_ctx->lock);
	return 0;
}

static int fps_ctx_set_wnd_sz(struct fps_ctx_t *fps_ctx, unsigned int wnd_sz)
{
	int i;

	if (!fps_ctx->is_inited)
		return fps_ctx_init(fps_ctx, wnd_sz);

	if (wnd_sz > FPS_ARRAY_SZ) {
		DISPWARN("error: %s wnd_sz=%d\n", __func__, wnd_sz);
		return -1;
	}
	mutex_lock(&fps_ctx->lock);

	fps_ctx->total = 0;
	fps_ctx->wnd_sz = wnd_sz;

	for (i = 0; i < wnd_sz; i++)
		fps_ctx->total += fps_ctx->array[i];

	mutex_unlock(&fps_ctx->lock);
	return 0;
}

static int fps_ext_ctx_init(struct fps_ext_ctx_t *fps_ctx, unsigned int ms)
{
	if (disp_helper_get_option(DISP_OPT_FPS_EXT) == 0)
		return 0;

	if (fps_ctx == NULL) {
		DISPERR("%s:%d, fps_cxt is null\n", __func__, __LINE__);
		return -1;
	}
	if (ms > INTERVAL_MS) {
		DISPWARN("%s:%d, interval is too big:%d ms, max:%d ms\n",
			__func__, __LINE__, ms, INTERVAL_MS);
		return -1;
	}
	if (ms == 0) {
		DISPWARN("%s:%d, interval is equal to zero\n",
			__func__, __LINE__);
		return -1;
	}

	if (fps_ctx->is_inited)
		return 0;

	memset(fps_ctx, 0, sizeof(*fps_ctx));
	mutex_init(&fps_ctx->lock);
	fps_ctx->interval = (unsigned long long)ms * 1000;	/* ms to us */
	fps_ctx->frequency = INTERVAL_MS / ms;	/* ms to frequency */

	fps_ctx->is_inited = 1;

	return 0;
}

void (*display_time_fps_stablizer)(unsigned long long ts);

static int fps_ext_ctx_update(struct fps_ext_ctx_t *fps_ctx)
{
	unsigned long long ns = sched_clock();
	unsigned long long delta_us;	/* us */
	unsigned long long fps;

	if (disp_helper_get_option(DISP_OPT_FPS_EXT) == 0)
		return 0;

	if (fps_ctx == NULL) {
		DISPERR("%s:%d, fps_cxt is null\n", __func__, __LINE__);
		return -1;
	}
	if (display_time_fps_stablizer)
		display_time_fps_stablizer(ns);

	mutex_lock(&fps_ctx->lock);
	fps_ctx->total++;

	delta_us = ns - fps_ctx->last_trig;
	do_div(delta_us, 1000);
	if (delta_us > fps_ctx->interval) {
		/* calculate fps */
		fps = fps_ctx->interval * fps_ctx->total * fps_ctx->frequency;
		do_div(fps, delta_us);
		do_div(fps, INTERVAL_MS / 1000);
		fps_ctx->fps = (unsigned int)fps;
		fps_ctx->last_trig = ns;

		/* reset frame count */
		fps_ctx->total = 0;
		fps_ctx->stable = 0;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->fps_ext_set,
		MMPROFILE_FLAG_PULSE, fps_ctx->fps, fps_ctx->stable);
	mutex_unlock(&fps_ctx->lock);

	return 0;
}

static int fps_ext_ctx_get_fps(struct fps_ext_ctx_t *fps_ctx,
	unsigned int *fps, int *stable)
{
	if (disp_helper_get_option(DISP_OPT_FPS_EXT) == 0)
		return 0;

	if (fps_ctx == NULL) {
		DISPERR("%s:%d, fps_cxt is null\n", __func__, __LINE__);
		return -1;
	}
	if (fps == NULL) {
		DISPERR("%s:%d, fps is null\n", __func__, __LINE__);
		return -1;
	}
	if (stable == NULL) {
		DISPERR("%s:%d, stable is null\n", __func__, __LINE__);
		return -1;
	}

	mutex_lock(&fps_ctx->lock);
	*fps = fps_ctx->fps;
	*stable = fps_ctx->stable;

	mmprofile_log_ex(ddp_mmp_get_events()->fps_ext_get,
		MMPROFILE_FLAG_PULSE, fps_ctx->fps, fps_ctx->stable);
	mutex_unlock(&fps_ctx->lock);

	return 0;
}

static int fps_ext_ctx_set_interval(struct fps_ext_ctx_t *fps_ctx,
	unsigned int ms)
{
	if (fps_ctx == NULL) {
		DISPERR("%s:%d, fps_cxt is null\n", __func__, __LINE__);
		return -1;
	}

	if (!fps_ctx->is_inited)
		return fps_ext_ctx_init(fps_ctx, ms);

	if (ms > INTERVAL_MS) {
		DISPWARN("%s:%d, interval is too big:%d ms, max:%d ms\n",
			__func__, __LINE__, ms, INTERVAL_MS);
		return -1;
	}
	if (ms == 0) {
		DISPWARN("%s:%d, interval is equal to zero\n",
			__func__, __LINE__);
		return -1;
	}

	mutex_lock(&fps_ctx->lock);

	fps_ctx->total = 0;
	fps_ctx->interval = (unsigned long long)ms * 1000;	/* ms to us */
	fps_ctx->frequency = INTERVAL_MS / ms;	/* ms to frequency */

	mutex_unlock(&fps_ctx->lock);
	return 0;
}

int primary_fps_ctx_set_wnd_sz(unsigned int wnd_sz)
{
	return fps_ctx_set_wnd_sz(&primary_fps_ctx, wnd_sz);
}

int primary_fps_ctx_get_fps(unsigned int *fps, int *stable)
{
	return fps_ctx_get_fps(&primary_fps_ctx, fps, stable);
}

int primary_fps_ext_ctx_set_interval(unsigned int interval)
{
	return fps_ext_ctx_set_interval(&primary_fps_ext_ctx, interval);
}

#if 0 /* defined but not used */
static int fps_monitor_thread(void *data)
{
	int ret = 0;

	msleep(16000);
	while (1) {
		int fps, stable;

		msleep(20);
		_primary_path_lock(__func__);
		fps_ctx_get_fps(&primary_fps_ctx, &fps, &stable);
		_primary_path_unlock(__func__);
	}

	return 0;
}
#endif

/*********************** fps calculate finish ********************************/

/*********************** idle manager ****************************************/
int primary_display_get_debug_state(char *stringbuf, int buf_len)
{
	int len = 0;
	struct LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	struct LCM_DRIVER *lcm_drv = pgc->plcm->drv;

	len += scnprintf(stringbuf + len, buf_len - len,
		"|--------------------------------------------------------------------------------------|\n");
	len += scnprintf(stringbuf + len, buf_len - len,
		"|********Primary Display Path General Information********\n");
	if (mutex_trylock(&(pgc->lock))) {
		mutex_unlock(&(pgc->lock));
		len += scnprintf(stringbuf + len, buf_len - len,
			"|primary path global mutex is free\n");
	} else {
		len += scnprintf(stringbuf + len, buf_len - len,
			"|primary path global mutex is hold by [%s]\n",
			pgc->mutex_locker);
	}

	if (lcm_param && lcm_drv)
		len += scnprintf(stringbuf + len, buf_len - len,
			"|LCM Driver=[%s]\tResolution=%dx%d,Interface:%s, LCM Connected:%s\n",
			lcm_drv->name, lcm_param->width, lcm_param->height,
			(lcm_param->type == LCM_TYPE_DSI) ? "DSI" : "Other",
			islcmconnected ? "Y" : "N");
	len += scnprintf(stringbuf + len, buf_len - len,
		"|State=%s\tlcm_fps=%d\tmax_layer=%d\tmode:%s\tvsync_drop=%d\n",
		pgc->state == DISP_ALIVE ? "Alive" : "Sleep", pgc->lcm_fps,
		pgc->max_layer, session_mode_spy(pgc->session_mode),
		pgc->vsync_drop);
	len += scnprintf(stringbuf + len, buf_len - len,
		"|cmdq_handle_config=%p\tcmdq_handle_trigger=%p\tdpmgr_handle=%p\tovl2mem_path_handle=%p\n",
		pgc->cmdq_handle_config, pgc->cmdq_handle_trigger,
		pgc->dpmgr_handle, pgc->ovl2mem_path_handle);
	len += scnprintf(stringbuf + len, buf_len - len,
		"|Current display driver status=%s + %s\n",
		primary_display_is_video_mode() ? "video mode" : "cmd mode",
		primary_display_cmdq_enabled() ? "CMDQ On" : "CMDQ Off");

	return len;
}

enum DISP_MODULE_ENUM _get_dst_module_by_lcm(struct disp_lcm_handle *plcm)
{
	if (plcm == NULL) {
		DISPERR("plcm is null\n");
		return DISP_MODULE_UNKNOWN;
	}

	if (plcm->params->type == LCM_TYPE_DSI) {
		if (plcm->lcm_if_id == LCM_INTERFACE_DSI0)
			return DISP_MODULE_DSI0;
		else if (plcm->lcm_if_id == LCM_INTERFACE_DSI1)
			return DISP_MODULE_DSI1;
		else if (plcm->lcm_if_id == LCM_INTERFACE_DSI_DUAL)
			return DISP_MODULE_DSIDUAL;
		else
			return DISP_MODULE_DSI0;
	} else if (plcm->params->type == LCM_TYPE_DPI) {
		return DISP_MODULE_DPI;
	}

	DISPERR("can't find primary path dst module\n");
	return DISP_MODULE_UNKNOWN;
}


/****************************************************************
 *trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 1.wait idle:           N         N       Y        Y
 * 2.lcm update:          N         Y       N        Y
 * 3.path start:       idle->Y      Y    idle->Y     Y
 * 4.path trigger:     idle->Y      Y    idle->Y     Y
 * 5.mutex enable:        N         N    idle->Y     Y
 * 6.set cmdq dirty:      N         Y       N        N
 * 7.flush cmdq:          Y         Y       N        N
 ****************************************************************
 */

int _should_wait_path_idle(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 1.wait idle:        N         N        Y        Y
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;
	} else {
		if (primary_display_is_video_mode())
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		else
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
	}
}

int _should_update_lcm(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 2.lcm update:          N         Y       N        Y
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;

		/* lcm_update can't use cmdq now */
		return 0;
	}
	if (primary_display_is_video_mode())
		return 0;

	return 1;
}

int _should_start_path(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 3.path start:	idle->Y      Y    idle->Y     Y
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode()) {
			return 0;
			/* return dpmgr_path_is_idle(pgc->dpmgr_handle); */
		} else {
			return 0;
		}
	} else {
		if (primary_display_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 1;

	}
}

int _should_trigger_path(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 4.path trigger:     idle->Y      Y     idle->Y     Y
	 * 5.mutex enable:        N         N     idle->Y     Y
	 */
	/*
	 * this is not a perfect design, we can't decide path
	 * trigger(ovl/rdma/dsi..) separately with mutex enable
	 * but it's lucky because path trigger and mutex enable is the same w/o
	 * cmdq, and it's correct w/ CMDQ(Y+N).
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode()) {
			return 0;
			/* return dpmgr_path_is_idle(pgc->dpmgr_handle); */
		} else {
			return 0;
		}
	} else {
		if (primary_display_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 1;
	}
}

int _should_set_cmdq_dirty(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 6.set cmdq dirty:	   N         Y       N        N
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 1;
	} else {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;
	}
}

int _should_flush_cmdq_config_handle(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 7.flush cmdq:          Y         Y       N        N
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 1;
		else
			return 1;
	} else {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;
	}
}

int _should_reset_cmdq_config_handle(void)
{
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 1;
		else
			return 1;
	} else {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;
	}
}

int _should_insert_wait_frame_done_token(void)
{
	/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	 * 7.flush cmdq:          Y         Y       N        N
	 */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 1;
		else
			return 1;
	} else {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;
	}
}

int _should_trigger_interface(void)
{
	if (pgc->mode == DECOUPLE_MODE)
		return 0;
	else
		return 1;
}

int _should_config_ovl_input(void)
{
	/* should extend this when display path dynamic switch is ready */
	if (pgc->mode == SINGLE_LAYER_MODE ||
		pgc->mode == DEBUG_RDMA1_DSI0_MODE)
		return 0;
	else
		return 1;
}

static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static enum hrtimer_restart _DISP_CmdModeTimer_handler(struct hrtimer *timer)
{
	DISPMSG("fake timer, wake up\n");
	dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
#if 0
	if ((get_current_time_us() - pgc->last_vsync_tick) > 16666) {
		dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
		pgc->last_vsync_tick = get_current_time_us();
	}
#endif
	hrtimer_forward_now(timer, ns_to_ktime(16666666));
	return HRTIMER_RESTART;
}

int _init_vsync_fake_monitor(int fps)
{
	if (is_fake_timer_inited)
		return 0;

	is_fake_timer_inited = 1;

	if (fps == 0)
		fps = 6000;

	hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;
	hrtimer_start(&cmd_mode_update_timer, ns_to_ktime(16666666),
		HRTIMER_MODE_REL);

	return 0;
}

static void change_dsi_vfp(struct cmdqRecStruct *handle, unsigned int fps)
{
	unsigned int VFP_PORTCH = pgc->plcm->params->dsi.vertical_frontporch;
	unsigned int line_num = (pgc->plcm->params->dsi.vertical_frontporch +
		pgc->plcm->params->dsi.vertical_sync_active +
		pgc->plcm->params->dsi.vertical_backporch +
		pgc->plcm->params->dsi.vertical_active_line);

	if (fps < 60)
		VFP_PORTCH = (line_num * 60) / fps - line_num +
				pgc->plcm->params->dsi.vertical_frontporch;

	cmdqRecBackupUpdateSlot(handle, pgc->dsi_vfp_line, 0, VFP_PORTCH);
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;
	unsigned long dsi_vfp_addr[2] = {0, 0};

	if (primary_display_is_video_mode() &&
		disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
		int VFP_PORTCH =
			pgc->plcm->params->dsi.vertical_frontporch;
		unsigned int gsync_fps =
			(pgc->dynamic_fps ? pgc->dynamic_fps : 60);
		int line_num = pgc->plcm->params->dsi.vertical_frontporch +
			pgc->plcm->params->dsi.vertical_sync_active +
			pgc->plcm->params->dsi.vertical_backporch +
			pgc->plcm->params->dsi.vertical_active_line;

		if (gsync_fps < 60)
			VFP_PORTCH = (line_num * 60) / gsync_fps - line_num +
				pgc->plcm->params->dsi.vertical_frontporch;

		cmdqBackupWriteSlot(pgc->dsi_vfp_line, 0, VFP_PORTCH);
	}

	if (pgc->cmdq_handle_trigger == NULL) {
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP,
			&(pgc->cmdq_handle_trigger));
		DISPINFO("primary path trigger thread cmd handle=%p\n",
			pgc->cmdq_handle_trigger);
	}
	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (primary_display_is_video_mode()) {
		ddp_mutex_set_sof_wait(dpmgr_path_get_mutex(pgc->dpmgr_handle),
			pgc->cmdq_handle_trigger, 0);

		cmdqRecWait(pgc->cmdq_handle_trigger,
			CMDQ_EVENT_MUTEX0_STREAM_EOF);

		/* wait and clear rdma0_sof for vfp change */
		cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
			CMDQ_EVENT_DISP_RDMA0_SOF);

		/* for some module(like COLOR) to read hw register
		 * to GPR after frame done
		 */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger, CMDQ_AFTER_STREAM_EOF, 0);

		if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
			unsigned int addr;

			ret = cmdqRecWait(pgc->cmdq_handle_trigger,
				CMDQ_EVENT_DISP_RDMA0_SOF);
			dpmgr_path_ioctl(pgc->dpmgr_handle,
				pgc->cmdq_handle_trigger,
				DDP_DSI_PORCH_ADDR, (void *)(dsi_vfp_addr));
			addr = (unsigned int)(dsi_vfp_addr[0] & 0x0FFFFFFFF);
			if (dsi_vfp_addr[0]) {
				cmdqRecBackupWriteRegisterFromSlot(
					pgc->cmdq_handle_trigger,
					pgc->dsi_vfp_line, 0,
					addr);
			}

			addr = (unsigned int)(dsi_vfp_addr[1] & 0x0FFFFFFFF);
			if ((_get_dst_module_by_lcm(pgc->plcm) ==
				DISP_MODULE_DSIDUAL) &&
				dsi_vfp_addr[1]) {
				cmdqRecBackupWriteRegisterFromSlot(
					pgc->cmdq_handle_trigger,
					pgc->dsi_vfp_line, 0,
					addr);
			}
		}
	} else {
		/* DSI command mode doesn't have mutex_stream_eof,
		 * need use CMDQ token instead
		 */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		if (need_wait_esd_eof()) {
			/* Wait esd config thread done. */
			ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger,
				CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		/* for operations before frame transfer,
		 * such as waiting for DSI TE
		 */
#ifndef CONFIG_FPGA_EARLY_PORTING /* fpga has no TE signal */
		if (islcmconnected)
			dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				pgc->cmdq_handle_trigger, CMDQ_WAIT_LCM_TE, 0);
#endif
		ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_CABC_EOF);
		/*
		 * clear frame done token, now the config thread will not be
		 * allowed to config registers.
		 * remember that config thread's priority is higher than
		 * trigger thread, so all the config queued before will be
		 * applied then STREAM_EOF token be cleared.
		 * this is what CMDQ did as "Merge"
		 */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_STREAM_EOF);

		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		/* clear rdma EOF token before wait */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
			CMDQ_EVENT_DISP_RDMA0_EOF);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger, CMDQ_BEFORE_STREAM_SOF, 0);

		/*
		 * enable mutex, only cmd mode need this
		 * this is what CMDQ did as "Trigger"
		 */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
			CMDQ_ENABLE);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger, CMDQ_AFTER_STREAM_SOF, 1);

		/*
		 * waiting for frame done, because we can't use mutex stream
		 * eof here, so need to let dpmanager help to decide which event
		 * to wait. most time we wait rdma frame done event.
		 */
		/* wait dsi frame done */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
			CMDQ_EVENT_DISP_DSI0_EOF);
		/* wait dsi not busy */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
			CMDQ_EVENT_DSI0_DONE_EVENT);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger,
			CMDQ_WAIT_STREAM_EOF_EVENT, 0);

		/*
		 * dsi is not idle right after rdma frame done,
		 * so we need to polling about 1us for dsi returns to idle
		 * do not polling dsi idle directly which will decrease CMDQ
		 * performance
		 */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger,
			CMDQ_CHECK_IDLE_AFTER_STREAM_EOF, 0);

		/* for some module(like COLOR) to read hw register
		 * to GPR after frame done
		 */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger, CMDQ_AFTER_STREAM_EOF, 0);
		/* reset some modules to enhance robusty */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_trigger,
			CMDQ_RESET_AFTER_STREAM_EOF, 0);

		/* now frame done, config thread is allowed to
		 * config register now
		 */
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_STREAM_EOF);
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger,
			CMDQ_SYNC_TOKEN_CABC_EOF);
		/* RUN forever!!!! */
		if (ret < 0)
			disp_aee_db_print("cmdq build trigger fail, ret=%d\n",
				ret);
	}

	/* dump trigger loop instructions to check
	 * whether dpmgr_path_build_cmdq works correctly
	 */
	DISPINFO("primary display BUILD cmdq trigger loop finished\n");
}

void disp_spm_enter_cg_mode(void)
{
	mmprofile_log_ex(ddp_mmp_get_events()->cg_mode,
		MMPROFILE_FLAG_PULSE, 0, 0);
}

void disp_spm_enter_power_down_mode(void)
{
	mmprofile_log_ex(ddp_mmp_get_events()->power_down_mode,
		MMPROFILE_FLAG_PULSE, 0, 0);
}

void _cmdq_start_trigger_loop(void)
{
	int ret = 0;

	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!primary_display_is_video_mode()) {
		if (need_wait_esd_eof()) {
			/* Need set esd check eof synctoken to
			 * let trigger loop go.
			 */
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		/*
		 * need to set STREAM_EOF for the first time, otherwise we will
		 * stuck in dead loop
		 */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_CABC_EOF);
		cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW);
	}

	DISPINFO("primary display START cmdq trigger loop finished\n");

}

void _cmdq_stop_trigger_loop(void)
{
	int ret = 0;

	/* this should be called only once because
	 * trigger loop will never stop
	 */
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);
	DISPINFO("primary display STOP cmdq trigger loop finished\n");
}

static void _cmdq_set_config_handle_dirty(void)
{
	if (!primary_display_is_video_mode()) {
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(pgc->cmdq_handle_config,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_handle_clear_dirty(struct cmdqRecStruct *cmdq_handle)
{
	if (!primary_display_is_video_mode()) {
		dprec_logger_trigger(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 1);
		cmdqRecClearEventToken(cmdq_handle,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
	}
}

static void _cmdq_set_config_handle_dirty_mira(void *handle)
{
	if (!primary_display_is_video_mode()) {
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(handle, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	dprec_event_op(DPREC_EVENT_CMDQ_RESET);
}

static void _cmdq_flush_config_handle(int blocking, CmdqAsyncFlushCB callback,
	unsigned int userdata)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, blocking,
		(unsigned long)callback);
	if (blocking) {
		cmdqRecFlush(pgc->cmdq_handle_config);
	} else {
		if (callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config,
				callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}

	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, userdata, 0);

	if (dprec_option_enabled())
		cmdqRecDumpCommand(pgc->cmdq_handle_config);
}

static void _cmdq_flush_config_handle_mira(void *handle, int blocking)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);
	if (blocking)
		cmdqRecFlush(handle);
	else
		cmdqRecFlushAsync(handle);

	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);
}

void _cmdq_insert_wait_primary_path_frame_done(void *handle)
{
	if (primary_display_is_video_mode())
		cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
}

void _cmdq_insert_wait_frame_done_token_mira(void *handle)
{
	if (primary_display_is_video_mode()) {
		/*cmdqRecWaitNoClear(handle, CMDQ_EVENT_DISP_RDMA0_EOF);*/
		cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
		ddp_mutex_set_sof_wait(
			dpmgr_path_get_mutex(pgc->dpmgr_handle), handle, 0);
	} else {
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}

	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

static void update_frm_seq_info(unsigned int addr, unsigned int addr_offset,
	unsigned int seq, enum DISP_FRM_SEQ_STATE state)
{
	int i = 0;

	if (state == FRM_CONFIG) {
		frm_update_sequence[frm_update_cnt].state = state;
		frm_update_sequence[frm_update_cnt].mva = addr;
		frm_update_sequence[frm_update_cnt].max_offset = addr_offset;
		if (seq > 0)
			frm_update_sequence[frm_update_cnt].seq = seq;
		mmprofile_log_ex(ddp_mmp_get_events()->primary_seq_config,
			MMPROFILE_FLAG_PULSE, addr, seq);
	} else if (state == FRM_TRIGGER) {
		frm_update_sequence[frm_update_cnt].state = FRM_TRIGGER;
		mmprofile_log_ex(ddp_mmp_get_events()->primary_seq_trigger,
			MMPROFILE_FLAG_PULSE, frm_update_cnt,
			frm_update_sequence[frm_update_cnt].seq);

		dprec_logger_frame_seq_begin(pgc->session_id,
			frm_update_sequence[frm_update_cnt].seq);

		frm_update_cnt++;
		frm_update_cnt %= FRM_UPDATE_SEQ_CACHE_NUM;
	} else if (state == FRM_START) {
		for (i = 0; i < FRM_UPDATE_SEQ_CACHE_NUM; i++) {
			if ((abs(addr - frm_update_sequence[i].mva) >
				frm_update_sequence[i].max_offset) ||
			    (frm_update_sequence[i].state != FRM_TRIGGER))
				continue;

			mmprofile_log_ex(
				ddp_mmp_get_events()->primary_seq_rdma_irq,
				MMPROFILE_FLAG_PULSE,
				frm_update_sequence[i].mva,
				frm_update_sequence[i].seq);
			frm_update_sequence[i].state = FRM_START;
			dprec_logger_frame_seq_end(pgc->session_id,
				frm_update_sequence[i].seq);
			dprec_logger_frame_seq_begin(0,
				frm_update_sequence[i].seq);
		}
	} else if (state == FRM_END) {
		for (i = 0; i < FRM_UPDATE_SEQ_CACHE_NUM; i++) {
			if (frm_update_sequence[i].state != FRM_START)
				continue;

			frm_update_sequence[i].state = FRM_END;
			dprec_logger_frame_seq_end(0,
				frm_update_sequence[i].seq);
			mmprofile_log_ex(
				ddp_mmp_get_events()->primary_seq_release,
				MMPROFILE_FLAG_PULSE,
				frm_update_sequence[i].mva,
				frm_update_sequence[i].seq);
		}
	}
}

static int _config_wdma_output(struct WDMA_CONFIG_STRUCT *wdma_config,
	disp_path_handle disp_handle, struct cmdqRecStruct *cmdq_handle)
{
	struct disp_ddp_path_config *pconfig =
		dpmgr_path_get_last_config(disp_handle);

	pconfig->wdma_config = *wdma_config;
	pconfig->wdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
	return 0;
}

static int _config_rdma_input_data(struct RDMA_CONFIG_STRUCT *rdma_config,
	disp_path_handle disp_handle, struct cmdqRecStruct *cmdq_handle)
{
	struct disp_ddp_path_config *pconfig =
		dpmgr_path_get_last_config(disp_handle);

	pconfig->rdma_config = *rdma_config;
	pconfig->rdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
	return 0;
}

static void directlink_path_add_memory(struct WDMA_CONFIG_STRUCT *p_wdma,
	enum DISP_MODULE_ENUM after_engine)
{
	int ret = 0;
	struct cmdqRecStruct *cmdq_handle = NULL;
	struct cmdqRecStruct *cmdq_wait_handle = NULL;
	struct disp_ddp_path_config *pconfig = NULL;
	int virtual_height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
	int virtual_width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);

	/* create config thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
	if (ret) {
		DISPERR("dl_to_dc capture:Fail to create cmdq handle\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_handle);

	/* create wait thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,
		&cmdq_wait_handle);
	if (ret) {
		DISPERR("dl_to_dc capture:Fail to create cmdq wait handle\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_wait_handle);
#ifdef MTK_FB_MMDVFS_SUPPORT
	primary_display_request_dvfs_perf(0,
		HRT_LEVEL_NUM - 1);
#endif
	/* configure config thread */
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_add_memout(pgc->dpmgr_handle, after_engine,
		cmdq_handle);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	primary_display_config_full_roi(pconfig, pgc->dpmgr_handle,
		cmdq_handle);
	pconfig->wdma_config = *p_wdma;

	if (disp_helper_get_option(DISP_OPT_DECOUPLE_MODE_USE_RGB565)) {
		pconfig->wdma_config.outputFormat = UFMT_RGB565;
		pconfig->wdma_config.dstPitch =
			pconfig->wdma_config.srcWidth * 2;
	}
	pconfig->wdma_config.srcHeight = virtual_height;
	pconfig->wdma_config.srcWidth = virtual_width;
	pconfig->wdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);

	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DISPDBG("dl_to_dc capture:Flush add memout mva(0x%lx)\n",
		p_wdma->dstAddress);

	/* wait wdma0 sof */
	cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
	cmdqRecFlush(cmdq_wait_handle);
	DISPMSG("dl_to_dc capture:Flush wait wdma sof\n");
out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);
}


void disp_enable_emi_force_on(unsigned int enable, void *cmdq_handle)
{
}

#ifdef CONFIG_MTK_IOMMU_V2
static struct ion_client *ion_client;
static struct ion_handle *sec_ion_handle;
#endif
static u32 sec_mva;

static int sec_buf_ion_alloc(int buf_size)
{
#ifdef CONFIG_MTK_IOMMU_V2
	size_t mva_size = 0;
	unsigned long int sec_hnd = 0;
	/* ion_phys_addr_t sec_hnd = 0; */
	unsigned long align = 0; /* 4096 align */
	struct ion_mm_data mm_data;
	ion_phys_addr_t phy_addr = 0;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	ion_client = ion_client_create(g_ion_device, "display_dc_secmem");

	if (!ion_client) {
		DISPERR("create ion client failed!\n");
		return -1;
	}

	sec_ion_handle = ion_alloc(ion_client, buf_size, align,
		ION_HEAP_MULTIMEDIA_SEC_MASK, 0);

	if (IS_ERR_OR_NULL(sec_ion_handle)) {
		DISPERR("Fatal Error, ion_alloc for size %d failed\n",
			buf_size);
		ion_free(ion_client, sec_ion_handle);
		ion_client_destroy(ion_client);
		return -1;
	}

	/* Query (PA/mva addr)/ sec_hnd */
	/* use get_iova replace config_buffer & get_phys*/
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.kernel_handle = sec_ion_handle;
	mm_data.get_phys_param.module_id = DISP_M4U_PORT_DISP_WDMA0;
	mm_data.get_phys_param.security = 1;

	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data) < 0) {
		DISPERR("ion_test_drv: Config buffer failed.\n");
		ion_free(ion_client, sec_ion_handle);
		ion_client_destroy(ion_client);
		return -1;
	}

	/* 2. query sec hnd */
	if (ion_phys(ion_client, sec_ion_handle, &phy_addr,
		(size_t *)&mva_size)) {
		DISPERR("can't query ion buffer physical address!\n");
		ion_free(ion_client, sec_ion_handle);
		ion_client_destroy(ion_client);
		return -1;
	}
	sec_hnd = (unsigned int)phy_addr;

	if (sec_hnd == 0) {
		DISPERR("Fatal Error, get mva failed\n");
		ion_free(ion_client, sec_ion_handle);
		ion_client_destroy(ion_client);
		return -1;
	}
	sec_mva = sec_hnd;
	/* DISPMSG("create ion sec_mva 0x%x\n", sec_mva); */
#endif
	return 0;
}

static int sec_buf_ion_free(void)
{
#ifdef CONFIG_MTK_IOMMU_V2
	ion_free(ion_client, sec_ion_handle);
	ion_client_destroy(ion_client);
#endif
	return 0;
}

static int init_sec_buf(void)
{
	/* int height = primary_display_get_height(); */
	/* int width = primary_display_get_width(); */
	int height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
	int width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
	int Bpp = primary_display_get_bpp() / 8;
	int buffer_size = width * height * Bpp;
	int ret  = 0;

	if (sec_mva)
		return 0;

	ret = sec_buf_ion_alloc(buffer_size);
	if (ret) {
		DISPERR("sec_buf_ion_alloc failed!\n");
		return -1;
	}

	return 0;
}

static int deinit_sec_buf(void)
{
	int ret  = 0;

	if (sec_mva) {
		ret  = sec_buf_ion_free();
		sec_mva = 0;
		/* DISPMSG("deinit_sec_mva 0x%x\n", sec_mva); */
	}

	return 0;
}
static int _DL_switch_to_DC_fast(int block)
{
	int ret = 0;
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;
	struct RDMA_CONFIG_STRUCT rdma_config = decouple_rdma_config;
	struct WDMA_CONFIG_STRUCT wdma_config = decouple_wdma_config;

	struct disp_ddp_path_config *data_config_dl = NULL;
	struct disp_ddp_path_config *data_config_dc = NULL;
	unsigned int mva;
	struct ddp_io_golden_setting_arg gset_arg;

	if ((primary_is_sec() == 1)) {
		init_sec_buf();
		mva = sec_mva;
		wdma_config.security = DISP_SECURE_BUFFER;
	} else {
		mva = pgc->dc_buf[pgc->dc_buf_id];
		wdma_config.security = DISP_NORMAL_BUFFER;
	}

	if (mva == 0) {
		DISPERR("%s, dc buffer does not exist\n", __func__);
		return -1;
	}
	wdma_config.dstAddress = mva;
	/* wdma_config.security = DISP_NORMAL_BUFFER; */

	/* 1.save a temp frame to intermediate buffer */
	directlink_path_add_memory(&wdma_config, DISP_MODULE_OVL0);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 1, 0);

	/* 2.reset primary handle */
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	/* 3.modify interface path handle to new scenario(rdma->dsi) */
	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP0;

	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle,
		new_scenario, 0);

	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario,
		pgc->cmdq_handle_config,
		primary_display_is_video_mode() ?
		DDP_VIDEO_MODE : DDP_CMD_MODE, 0);

	/* 4.config rdma from directlink mode to memory mode */
	rdma_config.address = mva;
	rdma_config.security = wdma_config.security;

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = rdma_config;
	data_config_dl->rdma_config.dst_x = 0;
	data_config_dl->rdma_config.dst_y = 0;
	data_config_dl->rdma_config.dst_h = data_config_dl->dst_h;
	data_config_dl->rdma_config.dst_w = data_config_dl->dst_w;
	data_config_dl->rdma_dirty = 1;

	/* no need ioctl because of rdma_dirty */
	set_is_dc(1);

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl,
		pgc->cmdq_handle_config);

	screen_logger_add_message("sess_mode", MESSAGE_REPLACE,
		(char *)session_mode_spy(DISP_SESSION_DECOUPLE_MODE));
	dynamic_debug_msg_print(mva, rdma_config.width, rdma_config.height,
		rdma_config.pitch, UFMT_GET_Bpp(rdma_config.inputFormat));

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 1;
	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_OVL_GOLDEN_SETTING, &gset_arg);

	/* 5.backup rdma address to slots */
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info,
		0, rdma_config.address);
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info,
		1, rdma_config.pitch);
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info,
		2, rdma_config.inputFormat);

	/* 6.flush to cmdq */
	_cmdq_set_config_handle_dirty();
	_cmdq_flush_config_handle(1, NULL, 0);

	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 2, 0);

	/* Switch to lower gear */
#ifdef MTK_FB_MMDVFS_SUPPORT
	primary_display_request_dvfs_perf(
		0, HRT_LEVEL_LEVEL0);
#endif
	/* ddp_mmp_rdma_layer(&rdma_config, 0, 20, 20); */

	/* 7.reset  cmdq */
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	/* 9. create ovl2mem path handle */
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	pgc->ovl2mem_path_handle =
		dpmgr_create_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT,
		pgc->cmdq_handle_ovl1to2_config);

	if (pgc->ovl2mem_path_handle) {
		DISPDBG("dpmgr create ovl memout path SUCCESS(%p)\n",
			pgc->ovl2mem_path_handle);
	} else {
		DISPERR("dpmgr create path FAIL\n");
		return -1;
	}

	dpmgr_path_set_video_mode(pgc->ovl2mem_path_handle, 0);
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	data_config_dc->dst_w = rdma_config.width;
	data_config_dc->dst_h = rdma_config.height;
	data_config_dc->dst_dirty = 1;

	/* move ovl config info from dl to dc */
	memcpy(data_config_dc->ovl_config, data_config_dl->ovl_config,
	       sizeof(data_config_dl->ovl_config));

	/* when entering idle , the path will switch from DL to DC.
	 *  OVL0_2L->RSZ -> OVL0 wil be configed again, even there
	 * is not be triggered. Howerver, there miss to sync rsz roi information
	 */

	memcpy(&data_config_dc->rsz_enable, &data_config_dl->rsz_enable,
	       sizeof(data_config_dl->rsz_enable));
	memcpy(&data_config_dc->rsz_src_roi, &data_config_dl->rsz_src_roi,
			sizeof(data_config_dl->rsz_src_roi));
	memcpy(&data_config_dc->rsz_dst_roi, &data_config_dl->rsz_dst_roi,
			sizeof(data_config_dl->rsz_dst_roi));

	data_config_dc->ovl_dirty = 1;
	data_config_dc->p_golden_setting_context =
		data_config_dl->p_golden_setting_context;
	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, data_config_dc,
				pgc->cmdq_handle_ovl1to2_config);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->ovl2mem_path_handle);
	gset_arg.is_decouple_mode = 1;
	dpmgr_path_ioctl(pgc->ovl2mem_path_handle,
		pgc->cmdq_handle_ovl1to2_config, DDP_OVL_GOLDEN_SETTING,
		&gset_arg);

	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, block);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	cmdqRecWait(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_EOF);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 3, 0);

	/* 11..enable event for new path */
	if (primary_display_is_video_mode()) {
		if (_need_lfr_check()) {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC,
				DDP_IRQ_DSI0_FRAME_DONE);
		} else {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
		}
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 4, 0);
	deinit_sec_buf();

	return ret;
}

static int DL_switch_to_DC_fast(int sw_only, int block)
{
	int ret = 0;

	if (!sw_only)
		ret = _DL_switch_to_DC_fast(block);
	else
		ret = -1; /* not support yet */

	return ret;
}

static int modify_path_power_off_callback(unsigned long userdata)
{
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;
	int layer;

	old_scenario = userdata >> 16;
	new_scenario = userdata & ((1 << 16) - 1);
	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	/* release output buffer */
	layer = disp_sync_get_output_interface_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);
	return 0;
}

static int _DC_switch_to_DL_fast(int block)
{
	int ret = 0;
	int layer = 0;
	struct disp_ddp_path_config *data_config_dl = NULL;
	struct disp_ddp_path_config *data_config_dc = NULL;
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;
	struct ddp_io_golden_setting_arg gset_arg;

	/* 3.destroy ovl->mem path. */
	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	/* copy ovl config from DC handle to DL handle */
	memcpy(data_config_dl->ovl_config, data_config_dc->ovl_config,
		sizeof(data_config_dl->ovl_config));
	/* before power off, we should wait wdma0_eof first!!! */
	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	/* wait and get_mutex in the last frame configs */

	dpmgr_path_deinit(pgc->ovl2mem_path_handle,
		(unsigned long)(pgc->cmdq_handle_ovl1to2_config));
	dpmgr_destroy_path(pgc->ovl2mem_path_handle,
		pgc->cmdq_handle_ovl1to2_config);
	/* clear sof token for next dl to dc */
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config,
	CMDQ_EVENT_DISP_WDMA0_SOF);

	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle = NULL;

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* switch back to last request gear */
	primary_display_request_dvfs_perf(
		0, dvfs_last_ovl_req);
#endif

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 1, 1);

	/* release output buffer */
	layer = disp_sync_get_output_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	/* 4.modify interface path handle to new scenario(rdma->dsi) */
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_DISP;

	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle,
		new_scenario, 0);

	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario,
		pgc->cmdq_handle_config, primary_display_is_video_mode() ?
		DDP_VIDEO_MODE : DDP_CMD_MODE, 0);

	/* 5.config rdma from memory mode to directlink mode */
	data_config_dl->rdma_config = decouple_rdma_config;
	data_config_dl->rdma_config.address = 0;
	data_config_dl->rdma_config.pitch = 0;
	data_config_dl->rdma_config.security = DISP_NORMAL_BUFFER;
	data_config_dl->rdma_dirty = 1;
	data_config_dl->dst_dirty = 1;
	data_config_dl->ovl_dirty = 1;

	/* no need ioctl because of rdma_dirty */
	set_is_dc(0);

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl,
		pgc->cmdq_handle_config);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 0;
	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_OVL_GOLDEN_SETTING, &gset_arg);

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info,
		0, 0);

	/* cmdqRecDumpCommand(pgc->cmdq_handle_config); */
	_cmdq_set_config_handle_dirty();

	/*
	 * if blocking flush won't cause UX issue,
	 * we should simplify this code: remove callback
	 * else we should move disable_sodi to callback,
	 * and change to nonblocking flush
	 */
	_cmdq_flush_config_handle(block, modify_path_power_off_callback,
		(old_scenario << 16) | new_scenario);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 2, 1);

	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	/* 9.enable event for new path */
	if (primary_display_is_video_mode()) {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 3, 1);

	return ret;
}

static int _DC_switch_to_DL_sw_only(void)
{
	int ret = 0;
	int layer = 0;
	struct disp_ddp_path_config *data_config_dc = NULL;
	struct disp_ddp_path_config *data_config_dl = NULL;
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;

	/* 3.destroy ovl->mem path. */
	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	dpmgr_destroy_path_handle(pgc->ovl2mem_path_handle);

	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle = NULL;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 1, 1);

	/* release output buffer */
	layer = disp_sync_get_output_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	/* 4.modify interface path handle to new scenario(rdma->dsi) */
	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_DISP;
	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle,
		new_scenario, 1);
	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario,
		pgc->cmdq_handle_config,
		primary_display_is_video_mode() ?
		DDP_VIDEO_MODE : DDP_CMD_MODE, 1);
	dpmgr_modify_path_power_off_old_modules(old_scenario,
		new_scenario, 1);

	/* 5.config rdma from memory mode to directlink mode */
	data_config_dl->rdma_config = decouple_rdma_config;
	data_config_dl->rdma_config.address = 0;
	data_config_dl->rdma_config.pitch = 0;
	data_config_dl->rdma_config.security = DISP_NORMAL_BUFFER;
	/* no need ioctl because of rdma_dirty */
	set_is_dc(0);

	/* release output buffer */
	layer = disp_sync_get_output_interface_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	_cmdq_reset_config_handle();
	_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);
	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	/* 9.enable event for new path */
	if (primary_display_is_video_mode()) {
		if (_need_lfr_check()) {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC,
				DDP_IRQ_DSI0_FRAME_DONE);
		} else {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
		}
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	if (!primary_display_is_video_mode())
		_cmdq_build_trigger_loop();

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_PULSE, 3, 1);

	return ret;
}

static int DC_switch_to_DL_fast(int sw_only, int block)
{
	int ret = 0;

	if (!sw_only)
		ret = _DC_switch_to_DL_fast(block);
	else
		ret = _DC_switch_to_DL_sw_only();

	return ret;
}

static int DL_switch_to_rdma_mode(struct cmdqRecStruct *handle, int block)
{
	int ret;
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;
	int need_flush = 0;
	struct ddp_io_golden_setting_arg gset_arg;

	if (!handle) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
		if (ret) {
			DISPERR("%s:%d, create cmdq handle fail!ret=%d\n",
				__func__, __LINE__, ret);
			return -1;
		}
		_cmdq_insert_wait_frame_done_token_mira(handle);
		need_flush = 1;
	}

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP0;
	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle,
		new_scenario, 0);
	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, handle,
		primary_display_is_video_mode() ?
		DDP_VIDEO_MODE : DDP_CMD_MODE, 0);
	dpmgr_modify_path_power_off_old_modules(old_scenario, new_scenario, 0);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 1;
	dpmgr_path_ioctl(pgc->dpmgr_handle, handle, DDP_OVL_GOLDEN_SETTING,
		&gset_arg);

	if (need_flush) {
		if (block)
			cmdqRecFlush(handle);
		else
			cmdqRecFlushAsync(handle);
		cmdqRecDestroy(handle);
	}

	return 0;
}

static int rdma_mode_switch_to_DL(struct cmdqRecStruct *handle, int block)
{
	int ret;
	enum DDP_SCENARIO_ENUM old_scenario, new_scenario;
	int need_flush = 0;
	struct disp_ddp_path_config *pconfig;
	struct ddp_io_golden_setting_arg gset_arg;

	if (!handle) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
		if (ret) {
			DISPERR("%s:%d, create cmdq handle fail!ret=%d\n",
				__func__, __LINE__, ret);
			return -1;
		}
		_cmdq_insert_wait_frame_done_token_mira(handle);
		need_flush = 1;
	}

	old_scenario = dpmgr_get_scenario(pgc->dpmgr_handle);
	new_scenario = DDP_SCENARIO_PRIMARY_DISP;
	dpmgr_modify_path_power_on_new_modules(pgc->dpmgr_handle,
		new_scenario, 0);
	dpmgr_modify_path(pgc->dpmgr_handle, new_scenario, handle,
		primary_display_is_video_mode() ?
		DDP_VIDEO_MODE : DDP_CMD_MODE, 0);
	dpmgr_modify_path_power_off_old_modules(old_scenario,
		new_scenario, 0);

	/* set rdma to DL mode */
	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->rdma_config.address = 0;
	pconfig->rdma_config.pitch = 0;
	pconfig->rdma_config.width = pconfig->dst_w;
	pconfig->rdma_config.height = pconfig->dst_h;
	pconfig->rdma_config.security = DISP_NORMAL_BUFFER;
	pconfig->rdma_dirty = 1;
	pconfig->dst_dirty = 1;
	if (need_flush) {
		/* re-config ovl because ovl is new-coming */
		pconfig->ovl_dirty = 1;
	}

	/* no need ioctl because of rdma_dirty */
	set_is_dc(0);

	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, handle);
	/* clear dirty set by this func */
	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 0;
	dpmgr_path_ioctl(pgc->dpmgr_handle, handle, DDP_OVL_GOLDEN_SETTING,
		&gset_arg);

	if (need_flush) {
		if (block)
			cmdqRecFlush(handle);
		else
			cmdqRecFlushAsync(handle);
		cmdqRecDestroy(handle);
	}

	return 0;
}

static struct disp_internal_buffer_info *allocat_decouple_buffer(int size)
{
	struct disp_internal_buffer_info *buf_info = NULL;
#ifdef CONFIG_MTK_IOMMU_V2
	void *buffer_va = NULL;
	struct ion_mm_data mm_data;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	client = ion_client_create(g_ion_device, "disp_decouple");

	buf_info = kzalloc(sizeof(struct disp_internal_buffer_info),
		GFP_KERNEL);
	if (buf_info) {
		handle = ion_alloc(client, size, 0,
			ION_HEAP_MULTIMEDIA_MASK, 0);
		if (IS_ERR(handle)) {
			DISPERR("Fatal Error, ion_alloc for size %d failed\n",
				size);
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		buffer_va = ion_map_kernel(client, handle);
		if (buffer_va == NULL) {
			DISPERR("ion_map_kernrl failed\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		/* use get_iova replace config_buffer & get_phys*/
		mm_data.get_phys_param.module_id = 0; //default value
		mm_data.get_phys_param.kernel_handle = handle;
		mm_data.mm_cmd = ION_MM_GET_IOVA;
		if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA,
			(unsigned long)&mm_data) < 0) {
			DISPERR("ion_test_drv: Config buffer failed.\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		if (mm_data.get_phys_param.phy_addr == 0) {
			DISPERR("Fatal Error, get mva failed\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		buf_info->handle = handle;
		buf_info->mva = (uint32_t)mm_data.get_phys_param.phy_addr;
		buf_info->size = (size_t)mm_data.get_phys_param.len;
		buf_info->va = buffer_va;
	} else {
		DISPERR("Fatal error, kzalloc internal buffer info failed!!\n");
		kfree(buf_info);
		return NULL;
	}
#endif
	return buf_info;
}

static int init_decouple_buffers(void)
{
	int i = 0;
	enum UNIFIED_COLOR_FMT fmt = UFMT_RGB888;
	int height = primary_display_get_height();
	int width = primary_display_get_width();
	int virtual_height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
	int virtual_width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
	int Bpp = UFMT_GET_Bpp(fmt);

	int buffer_size = width * height * Bpp;

	if (disp_helper_get_option(DISP_OPT_GMO_OPTIMIZE)) {
		decouple_buffer_info[0] = allocat_decouple_buffer(buffer_size);
		if (decouple_buffer_info[0])
			pgc->dc_buf[0] = decouple_buffer_info[0]->mva;
		else
			DISPERR("gmo alloc buf fail!\n");

		for (i = 1; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
			decouple_buffer_info[i] = decouple_buffer_info[0];
			pgc->dc_buf[i] = pgc->dc_buf[0];
		}
	} else {
		/* INTERNAL Buf 3 frames */
		for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
			decouple_buffer_info[i] = allocat_decouple_buffer(
								buffer_size);
			if (decouple_buffer_info[i])
				pgc->dc_buf[i] = decouple_buffer_info[i]->mva;
			}
	}

	/* initialize rdma config */
	decouple_rdma_config.height = height;
	decouple_rdma_config.width = width;
	decouple_rdma_config.idx = 0;
	decouple_rdma_config.inputFormat = fmt;
	decouple_rdma_config.pitch = width * Bpp;
	decouple_rdma_config.security = DISP_NORMAL_BUFFER;
	decouple_rdma_config.dst_x = 0;
	decouple_rdma_config.dst_y = 0;
	decouple_rdma_config.dst_w = virtual_width;
	decouple_rdma_config.dst_h = virtual_height;

	/* initialize wdma config */
	decouple_wdma_config.srcHeight = height;
	decouple_wdma_config.srcWidth = width;
	decouple_wdma_config.clipX = 0;
	decouple_wdma_config.clipY = 0;
	decouple_wdma_config.clipHeight = height;
	decouple_wdma_config.clipWidth = width;
	decouple_wdma_config.outputFormat = fmt;
	decouple_wdma_config.useSpecifiedAlpha = 1;
	decouple_wdma_config.alpha = 0xFF;
	decouple_wdma_config.dstPitch = width * Bpp;
	decouple_wdma_config.security = DISP_NORMAL_BUFFER;

	pr_info("%s done\n", __func__);
	return 0;
}

static int _init_decouple_buffers_thread(void *data)
{
	init_decouple_buffers();
	return 0;
}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISPFUNC();
	pgc->mode = DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DISP,
		pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPDBG("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	} else {
		DISPERR("dpmgr create path FAIL\n");
		return -1;
	}
	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	return ret;
}

static int _convert_disp_input_to_ovl(struct OVL_CONFIG_STRUCT *dst,
	struct disp_input_config *src)
{
	int ret = 0;
	int force_disable_alpha = 0;
	enum UNIFIED_COLOR_FMT tmp_fmt;
	unsigned int Bpp = 0;

	if (!src || !dst) {
		disp_aee_print("%s src(0x%p) or dst(0x%p) is null\n",
			__func__, src, dst);
		return -1;
	}

	dst->layer = src->layer_id;
	dst->isDirty = 1;
	dst->buff_idx = src->next_buff_idx;
	dst->layer_en = src->layer_enable;

	/* if layer is disable, we just need to config above params. */
	if (!src->layer_enable)
		return 0;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	/*
	 * display don't support X channel, like XRGB8888
	 * we need to enable const_bld
	 */
	ufmt_disable_X_channel(tmp_fmt, &dst->fmt, &dst->const_bld);
#if 0
	if (tmp_fmt != dst->fmt)
		force_disable_alpha = 1;
#endif
	Bpp = UFMT_GET_Bpp(dst->fmt);

	dst->addr = (unsigned long)(src->src_phy_addr);
	dst->vaddr = (unsigned long)(src->src_base_addr);
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->src_pitch = src->src_pitch * Bpp;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;

	/* dst W/H should <= src W/H */
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;

	dst->keyEn = src->src_use_color_key;
	dst->key = src->src_color_key;

	dst->aen = force_disable_alpha ? 0 : src->alpha_enable;
	dst->sur_aen = force_disable_alpha ? 0 : src->sur_aen;

	dst->alpha = src->alpha;
	dst->src_alpha = src->src_alpha;
	dst->dst_alpha = src->dst_alpha;

	dst->identity = src->identity;
	dst->connected_type = src->connected_type;
	dst->security = src->security;
	dst->yuv_range = src->yuv_range;

	if (src->buffer_source == DISP_BUFFER_ALPHA) {
		/* dim layer, constant alpha */
		dst->source = OVL_LAYER_SOURCE_RESERVED;
		dst->dim_color = src->dim_color;
	} else if (src->buffer_source == DISP_BUFFER_ION ||
		src->buffer_source == DISP_BUFFER_MVA) {
		dst->source = OVL_LAYER_SOURCE_MEM; /* from memory */
	} else {
		DISPWARN("unknown source = %d", src->buffer_source);
		dst->source = OVL_LAYER_SOURCE_MEM;
	}
	dst->ext_sel_layer = src->ext_sel_layer;
	return ret;
}

int _trigger_display_interface(int blocking, void *callback,
	unsigned int userdata)
{
	int ret;

	DISPFUNC();
	if (_should_wait_path_idle()) {
		DISPCHECK("wait frame_done at trigger_display_interface\n");
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		if (ret <= 0)
			primary_display_diagnose(__func__, __LINE__);
	}

	if (_should_update_lcm()) {
		int x = disp_helper_get_option(DISP_OPT_FAKE_LCM_X);
		int y = disp_helper_get_option(DISP_OPT_FAKE_LCM_Y);
		int width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
		int height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);

		disp_lcm_update(pgc->plcm, x, y, width, height, 0);
	}

	if (_should_start_path())
		dpmgr_path_start(pgc->dpmgr_handle,
			primary_display_cmdq_enabled());

	if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1) &&
	    primary_display_is_video_mode() && dynamic_fps_changed) {
		change_dsi_vfp(pgc->cmdq_handle_config, pgc->dynamic_fps);
		dynamic_fps_changed = 0;
	}

	if (_should_trigger_path()) {
#ifndef CONFIG_FPGA_EARLY_PORTING /* fpga has no vsync */
		if (islcmconnected)
			dpmgr_wait_event(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC);
#endif
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
			primary_display_cmdq_enabled());
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	/* clear cmdq dirty in case trigger loop starts here */
	if (_should_set_cmdq_dirty())
		_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);

	if (_should_insert_wait_frame_done_token())
		_cmdq_insert_wait_frame_done_token_mira(
			pgc->cmdq_handle_config);

	if (_need_lfr_check())
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
			pgc->cmdq_handle_config, CMDQ_DSI_LFR_MODE, 0);

	return 0;
}

int _trigger_ovl_to_memory(disp_path_handle disp_handle,
	struct cmdqRecStruct *cmdq_handle,
	CmdqAsyncFlushCB callback, unsigned int data)
{
	int layer = 0, i;
	unsigned int rdma_pitch_sec;

	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
		MMPROFILE_FLAG_START, 0, data);

	/*
	 * If full shadow mode:
	 * enable mutex in dpmgr_path_trigger() will signal sof
	 * If force_commit/bypass mode:
	 * enable_mutex before get/release_mutex to copy configs
	 * from shadow into working
	 */
	dpmgr_path_trigger(disp_handle, cmdq_handle, CMDQ_ENABLE);

	cmdqRecWaitNoClear(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	layer = disp_sync_get_output_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer,
		mem_config.buff_idx);

	layer = disp_sync_get_output_interface_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer,
				mem_config.interface_idx);

	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 0,
		(unsigned int)mem_config.addr);

	/* rdma pitch only use bit[15..0],
	 * we use bit[31:30] to store secure information
	 */
	rdma_pitch_sec = mem_config.pitch | (mem_config.security << 30);
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info,
		1, rdma_pitch_sec);
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info,
		2, (unsigned int)mem_config.fmt);

	/* backup night params here */
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->night_light_params, 0,
		mem_config.m_ccorr_config.mode);
	for (i = 0; i < 16; i++)
		cmdqRecBackupUpdateSlot(cmdq_handle, pgc->night_light_params,
			i + 1, mem_config.m_ccorr_config.color_matrix[i]);

	cmdqRecFlushAsyncCallback(cmdq_handle, callback, data);
	cmdqRecReset(cmdq_handle);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
		MMPROFILE_FLAG_END, 0, data);

	return 0;
}

static unsigned int _need_lfr_check(void)
{
	unsigned int ret = 0;

#ifdef CONFIG_OF
	if ((pgc->plcm->params->dsi.lfr_enable == 1) && (islcmconnected == 1))
		ret = 1;
#else
	if (pgc->plcm->params->dsi.lfr_enable == 1)
		ret = 1;
#endif
	return ret;
}

static int __primary_check_trigger(void)
{
	int ret = 0;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_aalod_trigger,
		MMPROFILE_FLAG_START, 0, 0);

	_primary_path_lock(__func__);
	if (pgc->state != DISP_ALIVE)
		goto out;
	if (primary_display_is_video_mode())
		goto out;

	dprec_logger_trigger(DPREC_LOGGER_PQ_TRIGGER_1SECOND, 0, 0);

	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		static struct cmdqRecStruct *handle;
		struct disp_ddp_path_config *data_config = NULL;

		if (!handle)
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
				&handle);
		cmdqRecReset(handle);

		primary_display_idlemgr_kick((char *)__func__, 0);
		_cmdq_insert_wait_frame_done_token_mira(handle);

		data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		primary_display_config_full_roi(data_config,
			pgc->dpmgr_handle, handle);

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
		if (od_need_start) {
			od_need_start = 0;
			disp_od_start_read(handle);
		}
		disp_od_update_status(handle);
#endif
		_cmdq_set_config_handle_dirty_mira(handle);
		_cmdq_flush_config_handle_mira(handle, 0);
	} else {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		DISPMSG("Force Trigger Display Path\n");
		primary_display_trigger(1, NULL, 0);
	}

	atomic_set(&delayed_trigger_kick, 1);

out:
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_aalod_trigger,
		MMPROFILE_FLAG_END, 0, 0);
	_primary_path_unlock(__func__);
	return 0;
}

static int _disp_primary_path_check_trigger(void *data)
{
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		__primary_check_trigger();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int _disp_primary_path_check_trigger_delay_33ms(void *data)
{
	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);
	dpmgr_enable_event(pgc->dpmgr_handle,
		DISP_PATH_EVENT_DELAYED_TRIGGER_33ms);
	while (1) {
		unsigned int hwc_fps = 0;
		int stable = 0;

		dpmgr_wait_event(pgc->dpmgr_handle,
			DISP_PATH_EVENT_DELAYED_TRIGGER_33ms);
		/* if current fps < 20, trigger immediately,
		 * so we may enter idle state quickly
		 * if fps > 20, delay AAL trigger to align with HWC trigger,
		 * this can save power
		 */
		fps_ctx_get_fps(&primary_fps_ctx, &hwc_fps, &stable);

		if (hwc_fps < 20) {
			__primary_check_trigger();
			continue;
		}
		atomic_set(&delayed_trigger_kick, 0);

		if (disp_helper_get_option(DISP_OPT_DELAYED_TRIGGER))
			usleep_range(32000, 33000);

		if (!atomic_read(&delayed_trigger_kick))
			__primary_check_trigger();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int _disp_primary_path_check_trigger_od(void *data)
{
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_OD_TRIGGER);
	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_OD_TRIGGER);
		atomic_set(&od_trigger_kick, 1);
		if (kthread_should_stop())
			break;
	}

	return 0;
}

unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
	return 0;
}

unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
	return 0;
}

unsigned int cmdqDdpDumpInfo(uint64_t engineFlag, char *pOutBuf,
	unsigned int bufSize)
{
	DISPERR("cmdq timeout:%llu\n", engineFlag);
	primary_display_diagnose(__func__, __LINE__);

	if (primary_display_is_decouple_mode())
		ddp_dump_analysis(DISP_MODULE_OVL0);

	ddp_dump_analysis(DISP_MODULE_WDMA0);

	return 0;
}

unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
	return 0;
}


int primary_display_change_lcm_resolution(unsigned int width,
	unsigned int height)
{
	if (!pgc->plcm)
		return -1;

	DISPMSG("LCM Resolution will be changed, original: %dx%d, now: %dx%d\n",
		pgc->plcm->params->width, pgc->plcm->params->height,
		width, height);
	/*
	 * align with 4 is the minimal check, to ensure we can
	 * boot up into kernel,and could modify dfo setting again
	 * using meta tool otherwise we will have a panic in lk
	 * (root cause unknown).
	 */
	if (width > pgc->plcm->params->width ||
	    height > pgc->plcm->params->height ||
	    width == 0 || height == 0 || width % 4 || height % 4) {
		DISPWARN("Invalid resolution: %dx%d\n", width, height);
		return -1;
	}

	if (primary_display_is_video_mode()) {
		DISPWARN(
		"Warning!!!Video Mode can't support multiple resolution!\n");
		return -1;
	}

	pgc->plcm->params->width = width;
	pgc->plcm->params->height = height;

	return 0;
}

static int _wdma_fence_release_callback(unsigned long userdata)
{
	int fence_idx, layer;

	layer = disp_sync_get_output_timeline_id();

	cmdqBackupReadSlot(pgc->cur_config_fence, layer, &fence_idx);
	mtkfb_release_fence(primary_session_id, layer, fence_idx);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_wdma_fence_release,
			 MMPROFILE_FLAG_PULSE, layer, fence_idx);

	return 0;
}

static int _Interface_fence_release_callback(unsigned long userdata)
{
	int layer = disp_sync_get_output_interface_timeline_id();
	int ret = 0;

#ifdef _DEBUG_DITHER_HANG_
	if (primary_display_is_video_mode()) {
		unsigned int status;

		cmdqBackupReadSlot(pgc->dither_status_info, 0, &status);
		if ((status) != 0x10001) {
			/* dither is not idle !! */
			DISPERR("disp dither status error!! stat=0x%x\n",
				status);
			/* disp_aee_print("dither_stat 0x%x\n", status); */
			mmprofile_log_ex(ddp_mmp_get_events()->primary_error,
					 MMPROFILE_FLAG_PULSE, status, 1);
			primary_display_diagnose(__func__, __LINE__);
			ret = -1;
		}
	}
#endif

	if (userdata > 0) {
		mtkfb_release_fence(primary_session_id, layer, userdata);
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_wdma_fence_release,
			MMPROFILE_FLAG_PULSE, layer, userdata);
	}

	return ret;
}

static void DC_config_nightlight(struct cmdqRecStruct *cmdq_handle)
{
	int i, mode, ccorr_matrix[16], all_zero = 1;

	cmdqBackupReadSlot(pgc->night_light_params, 0, &mode);

	for (i = 0; i < 16; i++)
		cmdqBackupReadSlot(pgc->night_light_params,
			i + 1, &(ccorr_matrix[i]));

	for (i = 0; i <= 15; i += 5) {
		if (ccorr_matrix[i] != 0) {
			all_zero = 0;
			break;
		}
	}
	if (all_zero)
		disp_aee_print("Night light backup param is zero matrix\n");
	else
		disp_ccorr_set_color_matrix(cmdq_handle, ccorr_matrix, mode);
}

static int _decouple_update_rdma_config_nolock(void)
{
	int interface_fence = 0;
	int layer = 0;
	int ret = 0;
	int composed_size =
		disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT) *
		disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH) * 3;

	if (primary_display_is_decouple_mode()) {
		static struct cmdqRecStruct *cmdq_handle;
		unsigned int rdma_pitch_sec;
		struct RDMA_CONFIG_STRUCT tmpConfig = decouple_rdma_config;

		layer = disp_sync_get_output_interface_timeline_id();
		cmdqBackupReadSlot(pgc->cur_config_fence, layer,
			&interface_fence);

		if (primary_get_state() != DISP_ALIVE) {
			/* don't trigger rdma */
			/* release interface fence */
			_Interface_fence_release_callback(
				interface_fence > 1 ? interface_fence - 1 : 0);

			return -1;
		}

		if (dump_output) {
			int i = pgc->dc_buf_id;

			show_layers_draw_wdma(draw);
			if (dump_output_comp) {
				memcpy(composed_buf,
				decouple_buffer_info[i]->va, composed_size);
				complete(&dump_buf_comp);
				dump_output_comp = 0;
			}
		}

		if (cmdq_handle == NULL) {
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
				&cmdq_handle);
			if (ret) {
				DISPERR("fail to create cmdq\n");
				return 0;
			}
		}

		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

		/* update night light params */
		DC_config_nightlight(cmdq_handle);

		cmdqBackupReadSlot(pgc->rdma_buff_info, 0,
			(uint32_t *)(&(tmpConfig.address)));

		/* rdma pitch only use bit[15..0],
		 * we use bit[31:30] to store secure information
		 */
		cmdqBackupReadSlot(pgc->rdma_buff_info, 1, &(rdma_pitch_sec));
		tmpConfig.pitch = rdma_pitch_sec & ~(3<<30);
		tmpConfig.security = rdma_pitch_sec >> 30;

		cmdqBackupReadSlot(pgc->rdma_buff_info, 2,
			&(tmpConfig.inputFormat));

		tmpConfig.height = primary_display_get_height();
		tmpConfig.width = primary_display_get_width();
		tmpConfig.yuv_range = 1; /* BT601 */
#ifdef _DEBUG_DITHER_HANG_
		if (primary_display_is_video_mode()) {
			cmdqRecBackupRegisterToSlot(cmdq_handle,
				pgc->dither_status_info, 0,
				disp_addr_convert(DISP_REG_DITHER_OUT_CNT));
		}
#endif
		_config_rdma_input_data(&tmpConfig, pgc->dpmgr_handle,
			cmdq_handle);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);

		if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1) &&
		    primary_display_is_video_mode() && dynamic_fps_changed) {
			change_dsi_vfp(cmdq_handle, pgc->dynamic_fps);
			dynamic_fps_changed = 0;
		}

		cmdqRecFlushAsyncCallback(cmdq_handle,
			(CmdqAsyncFlushCB)_Interface_fence_release_callback,
			interface_fence > 1 ? interface_fence - 1 : 0);

		dprec_mmp_dump_rdma_layer(&tmpConfig, 0);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_rdma_config,
				 MMPROFILE_FLAG_PULSE, interface_fence,
				 tmpConfig.address);
	}

	return 0;
}

static int decouple_update_rdma_config(void)
{
	int ret;

	_primary_path_lock(__func__);
	ret = _decouple_update_rdma_config_nolock();
	_primary_path_unlock(__func__);
	return ret;
}

static int _ovl_fence_release_callback(unsigned long userdata)
{
	int i = 0;
	int ret = 0;
	int real_hrt_level = 0;
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
	unsigned int in_fps = 60;
	unsigned int out_fps = 60;
	int stable = 0;
#endif

	mmprofile_log_ex(ddp_mmp_get_events()->session_release,
		MMPROFILE_FLAG_START, 1, userdata);

	/* check overlap layer */
	cmdqBackupReadSlot(pgc->subtractor_when_free, 0, &real_hrt_level);
	real_hrt_level >>= 16;

	_primary_path_lock(__func__);

#ifdef MTK_FB_MMDVFS_SUPPORT
	if ((real_hrt_level >= dvfs_last_ovl_req) &&
	    (!primary_display_is_decouple_mode()))
		primary_display_request_dvfs_perf(0,
			dvfs_last_ovl_req);
#endif
	_primary_path_unlock(__func__);

	/* check last ovl status: should be idle when config */
	if (primary_display_is_video_mode() &&
		!primary_display_is_decouple_mode()) {
		unsigned int status;

		cmdqBackupReadSlot(pgc->ovl_status_info, 0, &status);
#ifdef DEBUG_OVL_CONFIG_TIME
		unsigned int time_event = 0;
		unsigned int time_event1 = 0;
		unsigned int time_event2 = 0;

		cmdqBackupReadSlot(pgc->ovl_config_time, 0, &time_event);
		cmdqBackupReadSlot(pgc->ovl_config_time, 1, &time_event1);
		cmdqBackupReadSlot(pgc->ovl_config_time, 2, &time_event2);
		DISPMSG(
			"ovl config time_event %d time_event1 %d time_event2 %d time1_diff  %d  time2_diff %d\n",
			time_event, time_event1, time_event2,
			time_event1 - time_event, time_event2 - time_event1);
#endif
		if (status & 0x1) {
			/* ovl is not idle !! */
			DISPERR("disp ovl status error!! stat=0x%x\n",
			status);
			/* disp_aee_print("ovl_stat 0x%x\n", status); */
			mmprofile_log_ex(ddp_mmp_get_events()->primary_error,
					 MMPROFILE_FLAG_PULSE, status, 0);
			primary_display_diagnose(__func__, __LINE__);
			ret = -1;
		}
	}

	for (i = 0; i < PRIMARY_SESSION_INPUT_LAYER_COUNT; i++) {
		int fence_idx = 0;
		int subtractor = 0;

		if (i == primary_display_get_option("ASSERT_LAYER") &&
			is_DAL_Enabled()) {
			mtkfb_release_layer_fence(primary_session_id, i);
		} else {
			cmdqBackupReadSlot(pgc->cur_config_fence,
				i, &fence_idx);
			cmdqBackupReadSlot(pgc->subtractor_when_free,
				i, &subtractor);
			subtractor &= 0xFFFF;
			mtkfb_release_fence(primary_session_id,
				i, fence_idx - subtractor);
		}
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_ovl_fence_release,
			MMPROFILE_FLAG_PULSE, i, fence_idx - subtractor);
	}

	if (disp_helper_get_option(DISP_OPT_PERFORMANCE_DEBUG)) {
		unsigned int addr = 0;

		addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), 0);
		if ((primary_display_is_decouple_mode() == 0))
			update_frm_seq_info(addr, 0, 2, FRM_START);
	}

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	primary_fps_ctx_get_fps(&in_fps, &stable);
	if (!primary_display_is_video_mode())
		out_fps = in_fps;
	disp_get_ovl_bandwidth(in_fps, out_fps, &bandwidth);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_START,
			!primary_display_is_decouple_mode(), bandwidth);
	pm_qos_update_request(&primary_display_qos_request, bandwidth);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_END,
			!primary_display_is_decouple_mode(), bandwidth);
#endif

	mmprofile_log_ex(ddp_mmp_get_events()->session_release,
		MMPROFILE_FLAG_END, 1, userdata);
	return ret;
}

/* #define UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK */
static int _ovl_wdma_fence_release_callback(unsigned long userdata)
{
	int ret = 0;

	ret = _ovl_fence_release_callback(userdata);
	ret |= _wdma_fence_release_callback(userdata);

#ifdef UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
	ret |= decouple_update_rdma_config();
#endif

	return ret;
}

#ifdef UPDATE_RDMA_CONFIG_USING_CMDQ_CALLBACK
static int decouple_mirror_update_rdma_config_thread(void *data)
{
	return 0;
}
#else
static void decouple_mirror_irq_callback(enum DISP_MODULE_ENUM module,
	unsigned int reg_value)
{
#if (defined(CONFIG_MTK_TEE_GP_SUPPORT) || \
	defined(CONFIG_TRUSTONIC_TEE_SUPPORT)) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	/*
	 * In TEE, we have to protect WDMA registers, so we can't enable
	 * WDMA interrupt here we use ovl frame done interrupt instead
	 */
	if ((module == DISP_MODULE_OVL0) &&
		(primary_display_is_decouple_mode())) {
		if (reg_value & 0x2) { /* OVL0 frame done */
			atomic_set(&decouple_update_rdma_event, 1);
			wake_up_interruptible(&decouple_update_rdma_wq);
		}
	}
#else
	if ((module == DISP_MODULE_WDMA0) &&
		(primary_display_is_decouple_mode())) {
		if (reg_value & 0x1) { /* wdma0 frame done */
			atomic_set(&decouple_update_rdma_event, 1);
			wake_up_interruptible(&decouple_update_rdma_wq);
		}
	}
#endif
}

static int decouple_mirror_update_rdma_config_thread(void *data)
{
	int ret;
	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);

	disp_register_module_irq_callback(DISP_MODULE_WDMA0,
		decouple_mirror_irq_callback);
	disp_register_module_irq_callback(DISP_MODULE_OVL0,
		decouple_mirror_irq_callback);
	while (1) {
		ret = wait_event_interruptible(decouple_update_rdma_wq,
			atomic_read(&decouple_update_rdma_event));
		if (ret == 0) {
			atomic_set(&decouple_update_rdma_event, 0);
			decouple_update_rdma_config();
		} else {
			DISPWARN(
				"wait decouple_update_rdma_event interrupted , ret = %d\n",
				ret);
		}
		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static int _remove_memout_callback(unsigned long userdata)
{
	int ret = 0;
	CmdqInterruptCB orig_callback = (CmdqInterruptCB)userdata;
	struct DDP_MODULE_DRIVER *ddp_module;

	ddp_module = ddp_get_module_driver(DISP_MODULE_WDMA0);
	if (ddp_module->deinit != 0)
		ddp_module->deinit(DISP_MODULE_WDMA0, NULL);

	if (orig_callback)
		ret = orig_callback(0);

	return ret;
}

static int primary_display_remove_output(void *callback,
	unsigned int userdata)
{
	int ret = 0;
	static struct cmdqRecStruct *cmdq_handle;
	static struct cmdqRecStruct *cmdq_wait_handle;

	/* create config thread */
	if (cmdq_handle == NULL)
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);

	if (ret == 0) {
		/* capture thread wait wdma sof */
		if (cmdq_wait_handle == NULL)
			ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,
				&cmdq_wait_handle);

		if (ret == 0) {
			cmdqRecReset(cmdq_wait_handle);
			cmdqRecWait(cmdq_wait_handle,
				CMDQ_EVENT_DISP_WDMA0_SOF);
			cmdqRecFlush(cmdq_wait_handle);
			/* cmdqRecDestroy(cmdq_wait_handle); */
		} else {
			DISPERR("fail to create  wait handle\n");
		}
		cmdqRecReset(cmdq_handle);

		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		/* update output fence */
		cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence,
			disp_sync_get_output_timeline_id(),
			mem_config.buff_idx);

		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);

		cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_SOF);

		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		cmdqRecFlushAsyncCallback(cmdq_handle, _remove_memout_callback,
			(unsigned long)callback);
		pgc->need_trigger_ovl1to2 = 0;
		/* cmdqRecDestroy(cmdq_handle); */
	} else {
		ret = -1;
		DISPERR("fail to remove memout out\n");
	}
	return ret;
}

static void primary_display_frame_update_irq_callback(
	enum DISP_MODULE_ENUM module, unsigned int param)
{
	/* if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) */
	/*     return; */

	if (module == DISP_MODULE_RDMA0) {
		if (param & 0x2) { /* rdma0 frame start 0x20 */
			if (pgc->session_id > 0 &&
				primary_display_is_decouple_mode())
				update_frm_seq_info(ddp_ovl_get_cur_addr(1, 0),
					0, 1, FRM_START);
		}

		if (param & 0x4) { /* rdma0 frame done */
			atomic_set(&primary_display_frame_update_event, 1);
			wake_up_interruptible(&primary_display_frame_update_wq);
		}
	}

	if ((module == DISP_MODULE_OVL0) &&
		(primary_display_is_decouple_mode() == 0)) {
		if (param & 0x2) { /* ovl0 frame done */
			atomic_set(&primary_display_frame_update_event, 1);
			wake_up_interruptible(&primary_display_frame_update_wq);
		}
	}
}

static int primary_display_frame_update_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);
	for (;;) {
		wait_event_interruptible(primary_display_frame_update_wq,
			atomic_read(&primary_display_frame_update_event));
		atomic_set(&primary_display_frame_update_event, 0);

		if (pgc->session_id > 0)
			update_frm_seq_info(0, 0, 0, FRM_END);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

#ifdef CONFIG_MTK_IOMMU_V2 /* FIXME: remove when ION ready */
static int _present_fence_release_worker_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

	while (1) {
		int fence_increment = 0;
		int timeline_id;
		struct disp_sync_info *layer_info;

		wait_event_interruptible(primary_display_present_fence_wq,
			atomic_read(&primary_display_pt_fence_update_event));
		mmprofile_log_ex(ddp_mmp_get_events()->present_fence_release,
			MMPROFILE_FLAG_PULSE, 0, 0);
		atomic_set(&primary_display_pt_fence_update_event, 0);

		if (!islcmconnected && !primary_display_is_video_mode()) {
			DISPCHECK("LCM Not Connected && CMD Mode\n");
			msleep(20);
		} else if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
			dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				DISP_PATH_EVENT_FRAME_START, HZ/10);
		} else {
			dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, HZ/10);
			mmprofile_log_ex(
				ddp_mmp_get_events()->present_fence_release,
				MMPROFILE_FLAG_PULSE, 1, 1);
		}

		timeline_id = disp_sync_get_present_timeline_id();
		layer_info = _get_sync_info(primary_session_id, timeline_id);
		if (layer_info == NULL) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->present_fence_release,
				MMPROFILE_FLAG_PULSE, -1, 0x5a5a5a5a);
			continue;
		}

		_primary_path_lock(__func__);
		fence_increment =
			gPresentFenceIndex - layer_info->timeline->value;
		if (fence_increment > 0) {
			timeline_inc(layer_info->timeline, fence_increment);
			DISPPR_FENCE("R+/%s%d/L%d/id%d\n",
				disp_session_mode_spy(primary_session_id),
				DISP_SESSION_DEV(primary_session_id),
				timeline_id,
				gPresentFenceIndex);
		}
		mmprofile_log_ex(ddp_mmp_get_events()->present_fence_release,
				 MMPROFILE_FLAG_PULSE,
				 gPresentFenceIndex, fence_increment);
		_primary_path_unlock(__func__);

		if (atomic_read(&od_trigger_kick)) {
			atomic_set(&od_trigger_kick, 0);
			__primary_check_trigger();
		}
	}

	return 0;
}
#endif

int primary_display_set_frame_buffer_address(unsigned long va,
	unsigned long mva, unsigned long pa)
{
	DISPDBG("framebuffer va 0x%lx, mva 0x%lx, pa 0x%lx\n",
		va, mva, pa);
	pgc->framebuffer_va = va;
	pgc->framebuffer_mva = mva;
	pgc->framebuffer_pa = pa;

	return 0;
}

unsigned long primary_display_get_frame_buffer_mva_address(void)
{
	return pgc->framebuffer_mva;
}

unsigned long primary_display_get_frame_buffer_va_address(void)
{
	return pgc->framebuffer_va;
}

int is_dim_layer(unsigned long mva)
{
	if (mva == get_dim_layer_mva_addr())
		return 1;
	return 0;
}

unsigned long get_dim_layer_mva_addr(void)
{
	static unsigned long dim_layer_mva;

	if (dim_layer_mva == 0) {
		int frame_buffer_size =
			ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) *
			ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;

		dim_layer_mva = pgc->framebuffer_mva +
			(DISP_GetPages() - 1) * frame_buffer_size;
		DISPMSG("init dim layer mva %lu, size %d",
			dim_layer_mva, frame_buffer_size);
	}
	return dim_layer_mva;
}

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count,
	int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);

	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);

	return 0;
}

static int update_primary_intferface_module(void)
{
	/* update interface module, it may be: dsi0/dsi1/dsi_dual */
	enum DISP_MODULE_ENUM interface_module;

	interface_module = _get_dst_module_by_lcm(pgc->plcm);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_DISP, interface_module);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP0,
		interface_module);
	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_RDMA0_DISP, interface_module);

	ddp_set_dst_module(DDP_SCENARIO_PRIMARY_ALL, interface_module);

	return 0;
}

static void replace_fb_addr_to_mva(void)
{
#if (defined CONFIG_MTK_M4U) || (defined CONFIG_MTK_IOMMU_V2)
	struct ddp_fb_info fb_info;
	int i;
#ifdef CONFIG_MTK_IOMMU_V2
	int mode = 0x1;
#else
	int mode = 0x0;
#endif

	fb_info.fb_mva = pgc->framebuffer_mva;
	fb_info.fb_pa = pgc->framebuffer_pa;
	fb_info.fb_size = DISP_GetFBRamSize();
	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_OVL_MVA_REPLACEMENT, &fb_info);
	for (i = 0; i < 15; i++) {
		DISP_REG_SET_FIELD(pgc->cmdq_handle_config, REG_FLD_MMU_EN,
			DISP_REG_SMI_LARB0_NON_SEC_CON + i * 4, mode);
		DISP_REG_SET_FIELD(pgc->cmdq_handle_config, REG_FLD_MMU_EN,
			DISP_REG_SMI_LARB1_NON_SEC_CON + i * 4, mode);
	}
#endif
}

int primary_display_init(char *lcm_name, unsigned int lcm_fps,
	int is_lcm_inited)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;
	struct LCM_PARAMS *lcm_param = NULL;
	int use_cmdq = disp_helper_get_option(DISP_OPT_USE_CMDQ);
	struct disp_ddp_path_config *data_config;
	struct ddp_io_golden_setting_arg gset_arg;
	int i = 0;

	DISPCHECK("primary_display_init begin lcm=%s, inited=%d\n",
		lcm_name, is_lcm_inited);

	dprec_init();
	dpmgr_init();

	init_cmdq_slots(&(pgc->ovl_config_time), 3, 0);
	init_cmdq_slots(&(pgc->cur_config_fence),
		DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->subtractor_when_free),
		DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->rdma_buff_info), 3, 0);
	init_cmdq_slots(&(pgc->ovl_status_info), 4, 0);
	init_cmdq_slots(&(pgc->dither_status_info), 1, 0x10001);
	init_cmdq_slots(&(pgc->dsi_vfp_line), 1, 0);
	init_cmdq_slots(&(pgc->night_light_params), 17, 0);
	init_cmdq_slots(&(pgc->ovl_dummy_info), OVL_NUM, 0);

	/* init night light related variable */
	mem_config.m_ccorr_config.is_dirty = 1;

	mem_config.m_ccorr_config.mode = 1;
	cmdqBackupWriteSlot(pgc->night_light_params, 0, 1);

	for (i = 0; i <= 15; i += 5) {
		mem_config.m_ccorr_config.color_matrix[i] = 1024;
		cmdqBackupWriteSlot(pgc->night_light_params, i + 1, 1024);
	}

	mutex_init(&(pgc->capture_lock));
	mutex_init(&(pgc->lock));
	mutex_init(&(pgc->switch_dst_lock));

	fps_ctx_init(&primary_fps_ctx,
		disp_helper_get_option(DISP_OPT_FPS_CALC_WND));
	if (disp_helper_get_option(DISP_OPT_FPS_EXT))
		fps_ext_ctx_init(&primary_fps_ext_ctx,
			disp_helper_get_option(DISP_OPT_FPS_EXT_INTERVAL));

#ifdef MTK_FB_MMDVFS_SUPPORT
	pm_qos_add_request(&primary_display_qos_request,
		PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&primary_display_emi_opp_request,
		PM_QOS_DDR_OPP, PM_QOS_DDR_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&primary_display_mm_freq_request,
		PM_QOS_DISP_FREQ, PM_QOS_MM_FREQ_DEFAULT_VALUE);
#endif

	_primary_path_lock(__func__);

	/* Part1: LCM */
	pgc->plcm = disp_lcm_probe(lcm_name, LCM_INTERFACE_NOTDEFINED,
		is_lcm_inited);
	if (unlikely(pgc->plcm == NULL)) {
		DISPDBG("disp_lcm_probe returns null\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("disp_lcm_probe SUCCESS\n");
	}

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (unlikely(lcm_param == NULL)) {
		DISPERR("get lcm params FAILED\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	}

	/* update path dst module: for dual dsi */
	update_primary_intferface_module();

	/* Part2: CMDQ */
	if (use_cmdq) {
		ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,
			(CmdqClockOnCB)cmdqDdpClockOn,
			(CmdqDumpInfoCB)cmdqDdpDumpInfo,
			(CmdqResetEngCB)cmdqDdpResetEng,
			(CmdqClockOffCB)cmdqDdpClockOff);
		if (ret) {
			DISPERR("cmdqCoreRegisterCB failed, ret=%d\n", ret);
			ret = DISP_STATUS_ERROR;
			goto done;
		}

		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
			&(pgc->cmdq_handle_config));
		if (ret) {
			DISPDBG("cmdqRecCreate FAIL, ret=%d\n", ret);
			ret = DISP_STATUS_ERROR;
			goto done;
		} else {
			DISPDBG("cmdqRecCreate SUCCESS, g_cmdq_handle=%p\n",
				pgc->cmdq_handle_config);
		}

		/* create ovl2mem path cmdq handle */
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,
			&(pgc->cmdq_handle_ovl1to2_config));
		if (ret) {
			DISPERR("cmdqRecCreate FAIL, ret=%d\n", ret);
			ret = DISP_STATUS_ERROR;
			goto done;
		}
	} else {
		pgc->cmdq_handle_config = NULL;
		pgc->cmdq_handle_ovl1to2_config = NULL;
	}

	/*
	 * Part3: Top sw info(need by Path OP)
	 *  1. build path
	 *  2. config port
	 *  3. set max layer
	 *  4. init decouple buffer
	 *  5. set path cmd/vdo mode
	 *  -- new design:
	 *  6. need init dsi sw context
	 *  7. all module power on
	 *  8. disable mtcmos
	 *  --
	 */
	if (likely(primary_display_mode == DIRECT_LINK_MODE)) {
		_build_path_direct_link();
		pgc->session_mode = DISP_SESSION_DIRECT_LINK_MODE;
		DISPINFO("primary display is DIRECT LINK MODE\n");
	} else {
		DISPINFO("primary display mode is WRONG\n");
	}
	if (use_cmdq && is_lcm_inited) {
		/*
		 * if lcm is not inited (no LK),
		 * the first config should not wait frame done
		 * because there's no frame done for vdo mode
		 */
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token_mira(
			pgc->cmdq_handle_config);
	}

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	lcm_corner_en = primary_display_get_lcm_corner_en();
	full_content = primary_display_get_corner_full_content();
	if (lcm_corner_en)
		primary_display_get_round_corner_mva(&top_mva, &bottom_mva,
			&corner_pattern_width, &corner_pattern_height,
			&corner_pattern_height_bot);
#endif

	primary_display_set_max_layer(PRIMARY_SESSION_INPUT_LAYER_COUNT);

	init_decouple_buffer_thread =
		kthread_run(_init_decouple_buffers_thread,
			NULL, "init_decouple_buffer");
	if (IS_ERR(init_decouple_buffer_thread))
		DISPERR("kthread_run init_decouple_buffer_thread err = %d",
			IS_ERR(init_decouple_buffer_thread));

	dpmgr_path_set_video_mode(pgc->dpmgr_handle,
		primary_display_is_video_mode());
	DISPDBG("%s->dpmgr_path_init\n", __func__);
	dpmgr_path_init(pgc->dpmgr_handle, use_cmdq);

	/*
	 * use fake timer to generate vsync signal for cmd mode w/o
	 * LCM(originally using LCM TE Signal as VSYNC)
	 * so we don't need to modify display driver's behavior.
	 */
	if (disp_helper_get_option(DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT)) {
		/* only for low power measurement */
		DISPWARN("WARNING!!!!!! FORCE NO LCM MODE!!!\n");
		islcmconnected = 0;

		/* no need to change video mode vsync behavior */
		if (!primary_display_is_video_mode()) {
			_init_vsync_fake_monitor(lcm_fps);

			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_UNKNOWN);
		}
	}

	if (use_cmdq) {
		if (primary_display_is_video_mode() &&
			pgc->dynamic_fps == 0)
			pgc->dynamic_fps = 60;

		_cmdq_build_trigger_loop();
		_cmdq_start_trigger_loop();
	}

	DISPINFO("%s->dpmgr_path_config\n", __func__);

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	memcpy(&(data_config->dispif_config), lcm_param,
		sizeof(struct LCM_PARAMS));
	data_config->dst_w = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
	data_config->dst_h = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
	data_config->p_golden_setting_context = get_golden_setting_pgc();

	if (lcm_param->type == LCM_TYPE_DSI) {
		if (lcm_param->dsi.data_format.format ==
			LCM_DSI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if (lcm_param->dsi.data_format.format ==
			LCM_DSI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		else if (lcm_param->dsi.data_format.format ==
			LCM_DSI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	} else if (lcm_param->type == LCM_TYPE_DPI) {
		if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	}

	data_config->fps = lcm_fps;
	data_config->dst_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
		pgc->cmdq_handle_config);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 0;
	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_OVL_GOLDEN_SETTING, &gset_arg);
	if (disp_helper_get_option(DISP_OPT_USE_M4U))
		replace_fb_addr_to_mva();

	if (is_lcm_inited) {
		/* ??? why need */
		/* no need lcm power on,because lk power on lcm */
		/* ret = disp_lcm_init(pgc->plcm, 0);	*/
	} else {
		/* lcm not inited:
		 * 1. fpga no lk(verify done);
		 * 2. evb no lk(need verify)
		 */
		if (use_cmdq) {
			/* make sure dsi configuration done before lcm init */
			_cmdq_flush_config_handle(1, NULL, 0);
			_cmdq_reset_config_handle();
		}

		ret = disp_lcm_init(pgc->plcm, 1);
	}
	if (!ret)
		primary_display_set_lcm_power_state_nolock(LCM_ON);
	DISPCHECK("%s->dpmgr_path_start\n", __func__);
	/* path start must after lcm init for video mode
	 * because dsi_start will set mode
	 */
	dpmgr_path_start(pgc->dpmgr_handle, use_cmdq);

	if (use_cmdq) {
		_cmdq_flush_config_handle(1, NULL, 0);
		_cmdq_reset_config_handle();
	}

	if (disp_helper_get_option(DISP_OPT_USE_M4U))
		config_display_m4u_port();

	if (use_cmdq)
		_cmdq_insert_wait_frame_done_token_mira(
			pgc->cmdq_handle_config);
	if (!is_lcm_inited && primary_display_is_video_mode())
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, 0);

	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(0);

	/* Part5: Top sw init */
	primary_display_check_recovery_init();

	if (disp_helper_get_option(DISP_OPT_SWITCH_DST_MODE)) {
		primary_display_switch_dst_mode_task =
			kthread_create(
				_disp_primary_path_switch_dst_mode_thread,
				NULL, "display_switch_dst_mode");
		wake_up_process(primary_display_switch_dst_mode_task);
	}

	if (decouple_update_rdma_config_thread == NULL) {
		decouple_update_rdma_config_thread =
			kthread_create(
				decouple_mirror_update_rdma_config_thread,
				NULL, "decouple_update_rdma_cfg");
		wake_up_process(decouple_update_rdma_config_thread);
	}

	if (decouple_trigger_thread == NULL) {
		decouple_trigger_thread =
			kthread_create(decouple_trigger_worker_thread,
				NULL, "decouple_trigger");
		wake_up_process(decouple_trigger_thread);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		primary_path_aal_task =
			kthread_create(_disp_primary_path_check_trigger,
				NULL, "display_check_aal");
		wake_up_process(primary_path_aal_task);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		primary_delay_trigger_task =
			kthread_create(
				_disp_primary_path_check_trigger_delay_33ms,
				NULL, "disp_delay_trigger");
		wake_up_process(primary_delay_trigger_task);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		primary_od_trigger_task =
			kthread_create(_disp_primary_path_check_trigger_od,
				NULL, "disp_od_trigger");
		wake_up_process(primary_od_trigger_task);
	}

#ifdef CONFIG_MTK_IOMMU_V2 /* FIXME: remove when ION ready */
	if (disp_helper_get_option(DISP_OPT_PRESENT_FENCE)) {
		init_waitqueue_head(&primary_display_present_fence_wq);
		present_fence_release_worker_task =
			kthread_create(_present_fence_release_worker_thread,
				NULL, "present_fence_worker");
		wake_up_process(present_fence_release_worker_task);
	}
#endif

	if (disp_helper_get_option(DISP_OPT_PERFORMANCE_DEBUG)) {
		if (primary_display_frame_update_task == NULL) {
			init_waitqueue_head(&primary_display_frame_update_wq);
			disp_register_module_irq_callback(DISP_MODULE_RDMA0,
				primary_display_frame_update_irq_callback);
			disp_register_module_irq_callback(DISP_MODULE_OVL0,
				primary_display_frame_update_irq_callback);
			primary_display_frame_update_task =
				kthread_create(
					primary_display_frame_update_kthread,
					NULL, "frame_update_worker");
			wake_up_process(primary_display_frame_update_task);
		}
	}

	if (primary_display_is_video_mode()) {
		if (disp_helper_get_option(DISP_OPT_SWITCH_DST_MODE)) {
			/* video mode */
			primary_display_cur_dst_mode = 1;
			/* default mode is video mode */
			primary_display_def_dst_mode = 1;
		}
		if (_need_lfr_check()) {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC,
				DDP_IRQ_DSI0_FRAME_DONE);
		} else {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
			if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
				dpmgr_map_event_to_irq(pgc->dpmgr_handle,
					DISP_PATH_EVENT_FRAME_START,
					DDP_IRQ_RDMA0_START);
			}
		}
	}

	pgc->lcm_fps = lcm_fps;
	pgc->lcm_refresh_rate = 60;
	/* keep lowpower init after setting lcm_fps */
	primary_display_lowpower_init();

	primary_set_state(DISP_ALIVE);
#if 0 //def CONFIG_TRUSTONIC_TRUSTED_UI
	disp_switch_data.name = "disp";
	disp_switch_data.index = 0;
	disp_switch_data.state = DISP_ALIVE;
	ret = switch_dev_register(&disp_switch_data);
#endif

	DISPCHECK("%s done\n", __func__);

done:
	DISPDBG("init and hold wakelock...\n");
	wakeup_source_init(&pri_wk_lock, "pri_disp_wakelock");
	__pm_stay_awake(&pri_wk_lock);

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		primary_display_diagnose(__func__, __LINE__);

	layering_rule_init();
	_primary_path_unlock(__func__);
	return ret;
}

static void _primary_protect_mode_switch(void)
{
	int try_cnt = 50;

	while ((--try_cnt) && atomic_read(&hwc_configing)) {
		udelay(1000);
		/* DISPCHECK("detecting protect mode switch\n"); */
	}
	if (try_cnt <= 0)
		DISPCHECK("display warning:switch mode when hwc config\n");
}

static int request_lcm_refresh_rate_change(int fps);
int primary_display_set_lcm_refresh_rate(int fps)
{
	int ret = 0;

	DISPCHECK("set lcm fps(%d)\n", fps);
	_primary_protect_mode_switch();

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	/* Switch back to cmd mode */
	if (fps == 60 && primary_display_is_video_mode())
		primary_display_switch_dst_mode(0);
#endif

	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		_primary_path_unlock(__func__);
		DISPCHECK("Sleep State set lcm rate\n");
		return -1;
	}

	/* TODO: Should skip while MHL connected */

	/*
	 * Do not change to 120HZ here due to the last 60HZ frame update
	 * request, which ovl layers has been dispatched by HRT cacualtion with
	 * 60HZ, has not been executed yet.
	 * If the 60HZ frame request executed in 120HZ mode, the HRT may out of
	 * bound.
	 * Switch to 120HZ mode when the first 120 frame update request coming.
	 */
	if (fps == 120)
		ret = request_lcm_refresh_rate_change(fps);
	else
		ret = _display_set_lcm_refresh_rate(fps);
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_get_lcm_refresh_rate(void)
{
	return pgc->lcm_refresh_rate;
}

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
static int od_by_pass;

void primary_display_od_bypass(int bypass)
{
	DISPCHECK("odbyass %d\n", bypass);
	od_by_pass = bypass;
}

static void _display_set_refresh_rate_post_proc(int fps)
{
	if (fps == 60) {
		/* TODO: switch path after adjusting fps */
		od_need_start = 0;
		spm_enable_sodi(1);
	} else if (fps == 120) {
		if (!od_by_pass)
			od_need_start = 1;

		spm_enable_sodi(0);
	}
}
#endif

static int request_lcm_refresh_rate_change(int fps)
{
#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	static struct cmdqRecStruct *cmdq_handle, cmdq_pre_handle;
	int ret;

	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State set lcm rate\n");
		return -EPERM;
	}

	if (primary_display_get_lcm_max_refresh_rate() <= 60) {
		DISPCHECK("not support set lcm rate!!\n");
		return -EPERM;
	}

	if (fps == pgc->lcm_refresh_rate) {
		pgc->request_fps = 0;
		return 0;
	}

	if (cmdq_handle == NULL) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
		if (ret) {
			DISPCHECK(
				"fail to create primary cmdq handle for adjust fps\n");
			return -EINVAL;
		}
	}
	if (cmdq_pre_handle == NULL) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,
			&cmdq_pre_handle);
		if (ret) {
			DISPCHECK(
				"fail to create memout cmdq handle for adjust fps\n");
			cmdqRecDestroy(cmdq_handle);
			cmdq_handle = NULL;
			return -EINVAL;
		}
	}
	primary_display_idlemgr_kick(__func__, 0);

	pgc->request_fps = fps;
	pgc->lcm_refresh_rate = fps;
#endif
	return 0;
}

int _display_set_lcm_refresh_rate(int fps)
{
#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	static struct cmdqRecStruct *cmdq_handle, cmdq_pre_handle;
	disp_path_handle disp_handle;
	struct disp_ddp_path_config *pconfig = NULL;
	int ret = 0;

	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State set lcm rate\n");
		return -EPERM;
	}

	if (primary_display_get_lcm_max_refresh_rate() <= 60) {
		DISPCHECK("not support set lcm rate!!\n");
		return -EPERM;
	}

	if (fps == pgc->lcm_refresh_rate && pgc->request_fps == 0)
		return 0;

	if (fps == 60 && pgc->request_fps == 120) {
		pgc->lcm_refresh_rate = fps;
		pgc->request_fps = 0;
		DISPCHECK("LCM refresh rate is 60fps already\n");
		return 0;
	}

	if (cmdq_handle == NULL) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
		if (ret) {
			DISPCHECK(
				"fail to create primary cmdq handle for adjust fps\n");
			return -EINVAL;
		}
	}
	if (cmdq_pre_handle == NULL) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,
			&cmdq_pre_handle);
		if (ret) {
			DISPCHECK(
				"fail to create memout cmdq handle for adjust fps\n");
			cmdqRecDestroy(cmdq_handle);
			cmdq_handle = NULL;
			return -EINVAL;
		}
	}
	primary_display_idlemgr_kick(__func__, 0);

	/* don't move, switch need this part */
	pgc->lcm_refresh_rate = fps;
	pgc->request_fps = 0;
	DISPCHECK("[refresh_rate]:fps(%d)\n", fps);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_fps,
		MMPROFILE_FLAG_START, fps, 0);

	/* TODO: switch path before adjusting fps*/
	/*if (fps == 120) {*/
		/*Switch path.*/
	/*} */

	cmdqRecReset(cmdq_handle);
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
	ret = cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DSI_TE);
	ret = cmdqRecWait(cmdq_handle, CMDQ_EVENT_DSI_TE);
	/* 1.Change PLL CLOCK parameter and build fps lcm command */
	disp_lcm_adjust_fps(cmdq_handle, pgc->plcm, fps);

	/* 2.Change RDMA golden setting */
	disp_handle = pgc->dpmgr_handle;
	pconfig = dpmgr_path_get_last_config(disp_handle);
	pconfig->p_golden_setting_context->fps = fps;
	pconfig->dispif_config.dsi.PLL_CLOCK =
		pgc->plcm->params->dsi.PLL_CLOCK;
	dpmgr_path_ioctl(primary_get_dpmgr_handle(), cmdq_handle,
		DDP_RDMA_GOLDEN_SETTING, pconfig);
	/* 3.Change DSI clock */
	dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle, DDP_PHY_CLK_CHANGE,
			 &pgc->plcm->params->dsi.PLL_CLOCK);
	/* OD Enable */
	if (!od_by_pass) {
		if (fps == 120)
			disp_od_set_enabled(cmdq_handle, 1);
		else
			disp_od_set_enabled(cmdq_handle, 0);
	}

	if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
		/* need sync, make sure od is config done,
		 * even if od in decouple path
		 */
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
	} else {
		_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	}

	_display_set_refresh_rate_post_proc(fps);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_fps,
		MMPROFILE_FLAG_END, fps, 0);
#endif
	return 0;
}

int primary_display_get_lcm_max_refresh_rate(void)
{
	if (disp_lcm_is_support_adjust_fps(pgc->plcm) != 0)
		return 120;

	return 60;
}

int primary_display_deinit(void)
{
	_primary_path_lock(__func__);

	_cmdq_stop_trigger_loop();
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);
	_primary_path_unlock(__func__);

#ifdef MTK_FB_MMDVFS_SUPPORT
	pm_qos_remove_request(&primary_display_qos_request);
	pm_qos_remove_request(&primary_display_emi_opp_request);
	pm_qos_remove_request(&primary_display_mm_freq_request);
#endif

	return 0;
}

/* register rdma done event */
int primary_display_wait_for_idle(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);

	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_wait_for_dump(void)
{
	return 0;
}

int primary_display_release_fence_fake(void)
{
	unsigned int layer_en = 0;
	unsigned int addr = 0;
	unsigned int fence_idx = -1;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	int i = 0;

	DISPFUNC();

	for (i = 0; i < PRIMARY_SESSION_INPUT_LAYER_COUNT; i++) {
		if (i == primary_display_get_option("ASSERT_LAYER") &&
			is_DAL_Enabled()) {
			mtkfb_release_layer_fence(session_id, 3);
		} else {
			disp_sync_get_cached_layer_info(
				session_id, i, &layer_en,
				(unsigned long *)&addr, &fence_idx);
			if (fence_idx == -1) {
				DISPWARN(
					"find fence for layer %d,addr 0x%08x fail, unregistered\n",
					i, addr);
			} else if (fence_idx < 0) {
				DISPWARN(
					"find fence idx for layer %d,addr 0x%08x fail,unknown\n",
					i, addr);
			} else {
				if (layer_en)
					mtkfb_release_fence(session_id, i,
						fence_idx - 1);
				else
					mtkfb_release_fence(session_id, i,
						fence_idx);
			}
		}
	}

	return 0;
}

int primary_display_wait_for_vsync(void *config)
{
	struct disp_session_vsync_config *c =
		(struct disp_session_vsync_config *)config;
	int ret = 0, has_vsync = 1;
	unsigned long long ts = 0ULL;

	/* kick idle manager here to ensure sodi is disabled
	 * when screen update begin(not 100% ensure)
	 */
	primary_display_idlemgr_kick(__func__, 1);

#ifdef CONFIG_FPGA_EARLY_PORTING
	if (!primary_display_is_video_mode())
		has_vsync = 0; /* fpga has no TE signal */
#endif

	if (!islcmconnected || !has_vsync) {
		DISPCHECK("use fake vsync: lcm_connect=%d, has_vsync=%d\n",
			  islcmconnected, has_vsync);
		msleep(20);
		return 0;
	}

	if (pgc->force_fps_keep_count && pgc->force_fps_skip_count) {
		g_keep++;
		DISPMSG("vsync|keep %d\n", g_keep);
		if (g_keep == pgc->force_fps_keep_count) {
			g_keep = 0;

			while (g_skip != pgc->force_fps_skip_count) {
				g_skip++;
				DISPMSG("vsync|skip %d\n", g_skip);
				ret = dpmgr_wait_event_timeout(
						pgc->dpmgr_handle,
						DISP_PATH_EVENT_IF_VSYNC,
						HZ / 10);
				if (ret == -2) {
					DISPWARN(
						"vsync for primary display path not enabled yet\n");
					return -1;
				} else if (ret == 0) {
					primary_display_release_fence_fake();
				}
			}
			g_skip = 0;
		}
	} else {
		g_keep = 0;
		g_skip = 0;
	}

	ret = dpmgr_wait_event_ts(pgc->dpmgr_handle,
		DISP_PATH_EVENT_IF_VSYNC, &ts);

	if (ret == -2) {
		DISPWARN("vsync for primary display path not enabled yet\n");
		goto out;
	} else if (ret == 0) {
		/* primary_display_release_fence_fake(); */
	}

	if (pgc->vsync_drop) {
		ret = dpmgr_wait_event_ts(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, &ts);
	}

out:
	c->vsync_ts = ts;
	c->vsync_cnt++;
	c->lcm_fps = pgc->lcm_refresh_rate;

	return ret;
}

unsigned int primary_display_get_ticket(void)
{
	return dprec_get_vsync_count();
}

int primary_suspend_release_fence(void)
{
	unsigned int session =
		(unsigned int)((DISP_SESSION_PRIMARY) << 16 | (0));
	unsigned int i = 0;

	for (i = 0; i < DISP_SESSION_TIMELINE_COUNT; i++) {
		DISPDBG("mtkfb_release_layer_fence session=0x%x,layerid=%d\n",
			session, i);
		mtkfb_release_layer_fence(session, i);
	}
	return 0;
}

/* Need full roi when suspend */
int suspend_to_full_roi(void)
{
	int ret = 0;
	struct cmdqRecStruct *handle = NULL;
	struct disp_ddp_path_config *data_config = NULL;

	if (!disp_partial_is_support())
		return -1;

	if (!primary_display_is_directlink_mode())
		return -1;

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	if (ret) {
		DISPERR("%s:%d, create cmdq handle fail!ret=%d\n",
			__func__, __LINE__, ret);
		return -1;
	}
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	primary_display_config_full_roi(data_config,
		pgc->dpmgr_handle, handle);

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);
	return ret;
}

int primary_display_suspend(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;

	DISPCHECK("%s begin\n", __func__);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_START, 0, 0);
	primary_display_idlemgr_kick(__func__, 1);

	if (disp_helper_get_option(DISP_OPT_SWITCH_DST_MODE))
		primary_display_switch_dst_mode(primary_display_def_dst_mode);

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	/* Switch back to cmd mode */
	if (primary_display_is_video_mode())
		primary_display_switch_dst_mode(0);
#endif
	_primary_path_switch_dst_lock();
	disp_sw_mutex_lock(&(pgc->capture_lock));
	_primary_path_lock(__func__);

	while (primary_get_state() == DISP_BLANK) {
		_primary_path_unlock(__func__);
		DISPCHECK("p%s wait tui finish!!\n", __func__);
#if 0 //def CONFIG_TRUSTONIC_TRUSTED_UI
		switch_set_state(&disp_switch_data, DISP_SLEPT);
#endif
		primary_display_wait_state(DISP_ALIVE, MAX_SCHEDULE_TIMEOUT);
		_primary_path_lock(__func__);
		DISPCHECK("%s wait tui done stat=%d\n", __func__,
			primary_get_state());
	}

	if (pgc->state == DISP_SLEPT) {
		DISPWARN("primary display path is already sleep, skip\n");
		goto done;
	}
	primary_display_idlemgr_kick(__func__, 0);

	if (pgc->session_mode == DISP_SESSION_RDMA_MODE) {
		/* switch back to DL mode before suspend */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			pgc->session_id, 0, NULL, 1);
	}

	/* Should be DL(Ext) mode here */
	if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE ||
		pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
		DISPWARN("HWC DOES NOT set_mode DL before suspend!!!\n");
		/* switch back to DL mode before suspend */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			pgc->session_id, 0, NULL, 1);
	}

	/* restore to 60 fps */
	_display_set_lcm_refresh_rate(60);

	/* restore to full roi */
	if (disp_helper_get_option(DISP_OPT_USE_CMDQ))
		suspend_to_full_roi();

	/* need to leave share sram for suspend */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		leave_share_sram(CMDQ_SYNC_RESOURCE_WROT1);

	/* blocking flush before stop trigger loop */
	if (disp_helper_get_option(DISP_OPT_USE_CMDQ))
		_blocking_flush();
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 1);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		int event_ret;

		mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
			MMPROFILE_FLAG_PULSE, 1, 2);
		event_ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

		mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
			MMPROFILE_FLAG_PULSE, 2, 2);
		DISPCHECK(
			"[POWER]primary display path is busy now, wait frame done, event_ret=%d\n",
			event_ret);
		if (event_ret <= 0) {
			DISPERR("wait frame done in suspend timeout\n");
			mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
				MMPROFILE_FLAG_PULSE, 3, 2);
			primary_display_diagnose(__func__, __LINE__);
			ret = -1;
		}
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 2);

	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPCHECK("[POWER]display cmdq trigger loop stop\n");
		_cmdq_stop_trigger_loop();
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 3);

	DISPDBG("[POWER]primary display path stop[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]primary display path stop[end]\n");
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 4);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
			MMPROFILE_FLAG_PULSE, 1, 4);
		DISPERR("[POWER]stop display path failed, still busy\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		ret = -1;
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 5);

	if (primary_display_get_power_mode_nolock() == DOZE_SUSPEND) {
		if (primary_display_get_lcm_power_state_nolock() !=
			LCM_ON_LOW_POWER) {
			if (pgc->plcm->drv->aod)
				disp_lcm_aod(pgc->plcm, 1);

			primary_display_set_lcm_power_state_nolock(
				LCM_ON_LOW_POWER);
		}
	} else if (primary_display_get_power_mode_nolock() == FB_SUSPEND) {
		DISPCHECK("[POWER]lcm suspend[begin]\n");
		disp_lcm_suspend(pgc->plcm);
		DISPCHECK("[POWER]lcm suspend[end]\n");
		mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
			MMPROFILE_FLAG_PULSE, 0, 6);
		DISPINFO("[POWER]primary display path Release Fence[begin]\n");
		primary_suspend_release_fence();
		DISPINFO("[POWER]primary display path Release Fence[end]\n");
		mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
			MMPROFILE_FLAG_PULSE, 0, 7);
		primary_display_set_lcm_power_state_nolock(LCM_OFF);
	}

	/*enter PD mode*/
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		ddp_set_spm_mode(DDP_PD_MODE, NULL);

	DISPDBG("[POWER]dpmanager path power off[begin]\n");
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(1);

#ifdef MTK_FB_MMDVFS_SUPPORT
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_START,
			!primary_display_is_decouple_mode(), 0);
	pm_qos_update_request(&primary_display_qos_request, 0);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_END,
			!primary_display_is_decouple_mode(), 0);
#endif

	DISPCHECK("[POWER]dpmanager path power off[end]\n");
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_PULSE, 0, 8);

	pgc->lcm_refresh_rate = 60;
	/* pgc->state = DISP_SLEPT; */

done:
	primary_set_state(DISP_SLEPT);

	if (primary_display_get_power_mode_nolock() == DOZE_SUSPEND)
		primary_display_esd_check_enable(0);

	DISPDBG("release wakelock...\n");
	__pm_relax(&pri_wk_lock);

	_primary_path_unlock(__func__);
	disp_sw_mutex_unlock(&(pgc->capture_lock));
	_primary_path_switch_dst_unlock();

#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_wdt_kick_Powkey_api("mtkfb_early_suspend",
		WDT_SETBY_Display);
#endif
	primary_trigger_cnt = 0;
	mmprofile_log_ex(ddp_mmp_get_events()->primary_suspend,
		MMPROFILE_FLAG_END, 0, 0);
	DISPCHECK("%s end\n", __func__);
	ddp_clk_check();
	/* set MMDVFS to default, do not prevent it from stepping into ULPM */
#ifdef MTK_FB_MMDVFS_SUPPORT
	primary_display_request_dvfs_perf(0,
		HRT_LEVEL_DEFAULT);
#endif
	return ret;
}

int primary_display_get_lcm_index(void)
{
	int index = 0;

	DISPFUNC();

	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	index = pgc->plcm->index;
	DISPDBG("lcm index = %d\n", index);
	return index;
}

static int check_switch_lcm_mode_for_debug(void)
{
	static enum LCM_DSI_MODE_CON vdo_mode_type;
	struct LCM_PARAMS *lcm_param_cv = NULL;

	if (lcm_mode_status == 0)
		return 0;

	lcm_param_cv = disp_lcm_get_params(pgc->plcm);
	DISPCHECK("lcm_mode_status=%d, lcm_param_cv->dsi.mode %d\n",
		  lcm_mode_status, lcm_param_cv->dsi.mode);
	if (lcm_param_cv->dsi.mode != CMD_MODE)
		vdo_mode_type = lcm_param_cv->dsi.mode;
	if (lcm_mode_status == 1) {
		lcm_dsi_mode = CMD_MODE;
	} else if (lcm_mode_status == 2) {
		if (vdo_mode_type)
			lcm_dsi_mode = vdo_mode_type;
		else
			lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	} else {
		lcm_dsi_mode = lcm_param_cv->dsi.mode;
	}

	lcm_param_cv->dsi.mode = lcm_dsi_mode;
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, lcm_mode_status);
	lcm_mode_status = 0;
	set_esd_check_mode(GPIO_EINT_MODE);
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		primary_display_sodi_rule_init();
	DISPDBG("lcm_mode_status=%d, lcm_dsi_mode=%d\n",
		lcm_mode_status, lcm_dsi_mode);

	return 1;
}

int primary_display_resume(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;
	struct ddp_io_golden_setting_arg gset_arg;
	int i, skip_update = 0;
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
	unsigned int in_fps = 60;
	unsigned int out_fps = 60;
#endif

	DISPCHECK("primary_display_resume begin\n");
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_START, 0, 0);

	_primary_path_lock(__func__);
	if (pgc->state == DISP_ALIVE) {
		DISPCHECK("primary display path is already resume, skip\n");
		goto done;
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 1);

	if (is_ipoh_bootup) {
		DISPCHECK(
			"[primary display path] leave %s -- IPOH\n", __func__);
		DISPCHECK("ESD check start[begin]\n");
		primary_display_esd_check_enable(1);
		DISPCHECK("ESD check start[end]\n");
		is_ipoh_bootup = false;
		DISPDBG("[POWER]start cmdq[begin]--IPOH\n");
		if (disp_helper_get_option(DISP_OPT_USE_CMDQ))
			_cmdq_start_trigger_loop();

		enable_idlemgr(1);
		DISPDBG("[POWER]start cmdq[end]--IPOH\n");
		/* pgc->state = DISP_ALIVE; */
		goto done;
	}

	if (disp_helper_get_option(DISP_OPT_CV_BYSUSPEND)) {
		int dsi_force_config = 0;

		dsi_force_config |= check_switch_lcm_mode_for_debug();
		if (dsi_force_config)
			DSI_ForceConfig(1);
	}

	DISPDBG("dpmanager path power on[begin]\n");
	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	/*Exit PD mode*/
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		ddp_set_spm_mode(DDP_CG_MODE, NULL);

	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(0);

	DISPCHECK("dpmanager path power on[end]\n");

	DISPINFO("dpmanager path reset[begin]\n");
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPINFO("dpmanager path reset[end]\n");

	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 2);

	DISPDBG("[POWER]dpmanager re-init[begin]\n");

	{
		struct LCM_PARAMS *lcm_param;
		struct disp_ddp_path_config *data_config;

		/*
		 * disconnect primary path first
		 * because MMsys config register may not power off
		 * during early suspend. BUT session mode may change
		 * in primary_display_switch_mode()
		 */
		ddp_disconnect_path(DDP_SCENARIO_PRIMARY_ALL, NULL);
		ddp_disconnect_path(DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP0,
			NULL);
		DISPCHECK("cmd/video mode=%d\n",
			primary_display_is_video_mode());
		dpmgr_path_set_video_mode(pgc->dpmgr_handle,
			primary_display_is_video_mode());

		dpmgr_path_connect(pgc->dpmgr_handle, CMDQ_DISABLE);
		if (primary_display_is_decouple_mode()) {
			if (pgc->ovl2mem_path_handle)
				dpmgr_path_connect(pgc->ovl2mem_path_handle,
					CMDQ_DISABLE);
			else
				DISPERR(
				"in decouple_mode but no ovl2mem_path_handle\n");
		}

		mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
			MMPROFILE_FLAG_PULSE, 1, 2);
		lcm_param = disp_lcm_get_params(pgc->plcm);

		data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		memcpy(&(data_config->dispif_config), lcm_param,
			sizeof(struct LCM_PARAMS));

		data_config->dst_w =
			disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
		data_config->dst_h =
			disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
		if (lcm_param->type == LCM_TYPE_DSI) {
			if (lcm_param->dsi.data_format.format ==
				LCM_DSI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if (lcm_param->dsi.data_format.format ==
				LCM_DSI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			else if (lcm_param->dsi.data_format.format ==
				LCM_DSI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		} else if (lcm_param->type == LCM_TYPE_DPI) {
			if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		}

		data_config->fps = pgc->lcm_fps;
		if (disp_partial_is_support()) {
			data_config->ovl_partial_roi.x = 0;
			data_config->ovl_partial_roi.y = 0;
			data_config->ovl_partial_roi.width =
				primary_display_get_width();
			data_config->ovl_partial_roi.height =
				primary_display_get_height();
			if (disp_helper_get_option(
				DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING)) {
				/* update rdma goden settin */
				set_rdma_width_height(
					data_config->ovl_partial_roi.width,
					data_config->ovl_partial_roi.height);
			}
		}
		data_config->dst_dirty = 1;

		/*
		 * disable all ovl layers to show black screen
		 * note that if WFD is connected, we may miss the black setting
		 * before the last suspend
		 */
		for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
			if (is_DAL_Enabled() &&
			    data_config->ovl_config[i].layer ==
			    primary_display_get_option("ASSERT_LAYER"))
				continue;

			data_config->ovl_config[i].layer_en = 0;
		}
		data_config->ovl_dirty = 1;
		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
			MMPROFILE_FLAG_PULSE, 2, 2);
		data_config->dst_dirty = 0;

		memset(&gset_arg, 0, sizeof(gset_arg));
		gset_arg.dst_mod_type =
			dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
		gset_arg.is_decouple_mode = primary_display_is_decouple_mode();
		dpmgr_path_ioctl(pgc->dpmgr_handle, NULL,
			DDP_OVL_GOLDEN_SETTING, &gset_arg);

	}
	DISPCHECK("[POWER]dpmanager re-init[end]\n");
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 3);

	if (primary_display_get_power_mode_nolock() == DOZE) {
		if (primary_display_get_lcm_power_state_nolock() !=
			LCM_ON_LOW_POWER) {
			if (pgc->plcm->drv->aod)
				disp_lcm_aod(pgc->plcm, 1);
			else
				disp_lcm_resume(pgc->plcm);
			primary_display_set_lcm_power_state_nolock(
				LCM_ON_LOW_POWER);
		}
	} else if (primary_display_get_power_mode_nolock() == FB_RESUME) {
		if (primary_display_get_lcm_power_state_nolock() != LCM_ON) {
			DISPCHECK("[POWER]lcm resume[begin]\n");
			if (primary_display_get_lcm_power_state_nolock() !=
				LCM_ON_LOW_POWER) {
				disp_lcm_resume(pgc->plcm);
			} else {
				disp_lcm_aod(pgc->plcm, 0);
				skip_update = 1;
			}
			DISPCHECK("[POWER]lcm resume[end]\n");
			primary_display_set_lcm_power_state_nolock(LCM_ON);
		}
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 4);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
			MMPROFILE_FLAG_PULSE, 1, 4);
		DISPERR(
			"[POWER]Fatal error, we didn't start display path but it's already busy\n");
		ret = -1;
		/* goto done; */
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 5);
	DISPINFO("[POWER]dpmgr path start[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (primary_display_is_decouple_mode())
		dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_DISABLE);

	DISPINFO("[POWER]dpmgr path start[end]\n");

	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 6);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
			MMPROFILE_FLAG_PULSE, 1, 6);
		DISPERR(
			"[POWER]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		/* goto done; */
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 7);
	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPINFO("[POWER]build cmdq trigger loop[begin]\n");
		_cmdq_build_trigger_loop();
		DISPINFO("[POWER]build cmdq trigger loop[end]\n");
	}
	if (primary_display_is_video_mode()) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
			MMPROFILE_FLAG_PULSE, 1, 7);
		/*
		 * for video mode, we need to force trigger here
		 * for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when
		 * trigger loop starts
		 */
		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		/* for next frame */
		if (_should_insert_wait_frame_done_token())
			_cmdq_insert_wait_frame_done_token_mira(
				pgc->cmdq_handle_config);

		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
		dpmgr_enable_event(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC);

		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);

		if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_FRAME_START,
				DDP_IRQ_RDMA0_START);
			dpmgr_enable_event(pgc->dpmgr_handle,
				DISP_PATH_EVENT_FRAME_START);
		}

	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 8);

	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPDBG("[POWER]start cmdq[begin]\n");
		_cmdq_start_trigger_loop();
		DISPDBG("[POWER]start cmdq[end]\n");
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 9);

	/* primary_display_diagnose(__func__, __LINE__); */
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 10);

	if (!primary_display_is_video_mode()) {
		DISPINFO("[POWER]triggger cmdq[begin]\n");
		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		if (_should_insert_wait_frame_done_token())
			_cmdq_insert_wait_frame_done_token_mira(
				pgc->cmdq_handle_config);

		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_EXT_TE);
		dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

		if (primary_display_get_power_mode_nolock() == FB_RESUME &&
			!skip_update) {
			/* refresh black picture of ovl bg */
			_trigger_display_interface(1, NULL, 0);
			DISPINFO("[POWER]triggger cmdq[end]\n");
			/* wait for one frame for pms workarround!!!! */
			mdelay(16);
		}
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_PULSE, 0, 11);

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	disp_get_ovl_bandwidth(in_fps, out_fps, &bandwidth);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_START,
			!primary_display_is_decouple_mode(), bandwidth);
	pm_qos_update_request(&primary_display_qos_request, bandwidth);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_END,
			!primary_display_is_decouple_mode(), bandwidth);
#endif

	/*
	 * (in suspend) when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend
	 */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);

	/*
	 * reinit fake timer for debug, we can enable option then press power
	 * key to enable this feature.
	 * use fake timer to generate vsync signal for cmd mode w/o
	 * LCM(originally using LCM TE Signal as VSYNC) so we don't need to
	 * modify display driver's behavior.
	 */
	if (disp_helper_get_option(DISP_OPT_NO_LCM_FOR_LOW_POWER_MEASUREMENT)) {
		/* only for low power measurement */
		DISPWARN("WARNING!!!!!! FORCE NO LCM MODE!!!\n");
		islcmconnected = 0;

		/* no need to change video mode vsync behavior */
		if (!primary_display_is_video_mode()) {
			_init_vsync_fake_monitor(pgc->lcm_fps);

			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_UNKNOWN);
		}
	}

done:
	primary_set_state(DISP_ALIVE);
#if 0 //def CONFIG_TRUSTONIC_TRUSTED_UI
	switch_set_state(&disp_switch_data, DISP_ALIVE);
#endif

	/* need enter share sram for resume */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		enter_share_sram(CMDQ_SYNC_RESOURCE_WROT1);
	if (disp_helper_get_option(DISP_OPT_CV_BYSUSPEND))
		DSI_ForceConfig(0);

	if (primary_display_get_power_mode_nolock() == DOZE)
		primary_display_esd_check_enable(1);

	DISPDBG("hold the wakelock...\n");
	__pm_stay_awake(&pri_wk_lock);

	_primary_path_unlock(__func__);
	DISPMSG("skip_update:%d\n", skip_update);

#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_wdt_kick_Powkey_api("mtkfb_late_resume",
		WDT_SETBY_Display);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->primary_resume,
		MMPROFILE_FLAG_END, 0, 0);
	ddp_clk_check();
	return ret;
}

int primary_display_ipoh_restore(void)
{
	DISPMSG("primary_display_ipoh_restore In\n");
	DISPDBG("ESD check stop[begin]\n");
	enable_idlemgr(0);
	primary_display_esd_check_enable(0);
	DISPCHECK("ESD check stop[end]\n");
	if (pgc->cmdq_handle_trigger) {
		void *pTask =
			pgc->cmdq_handle_trigger->running_task;

		if (pTask) {
			DISPCHECK(
				"[Primary_display]display cmdq trigger loop stop[begin]\n");
			_cmdq_stop_trigger_loop();
			DISPCHECK(
				"[Primary_display]display cmdq trigger loop stop[end]\n");
			ddp_mutex_set_sof_wait(
				dpmgr_path_get_mutex(pgc->dpmgr_handle),
				NULL, 0);
		}
	}
	DISPMSG("primary_display_ipoh_restore Out\n");
	return 0;
}

int primary_display_start(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPERR(
			"Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_stop(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPERR("stop display path failed, still busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

void primary_display_update_present_fence(unsigned int fence_idx)
{
	gPresentFenceIndex = fence_idx;
	mmprofile_log_ex(ddp_mmp_get_events()->present_fence_set,
		MMPROFILE_FLAG_PULSE, fence_idx, 1);
	atomic_set(&primary_display_pt_fence_update_event, 1);
	if (disp_helper_get_option(DISP_OPT_PRESENT_FENCE))
		wake_up_interruptible(&primary_display_present_fence_wq);
}

/* the function will trigger ovl->wdma */
static int trigger_decouple_mirror(void)
{
	if (pgc->need_trigger_dcMirror_out == 0) {
		;
	} else {
		pgc->need_trigger_dcMirror_out = 0;

		/*
		 * check if still in decouple mirror mode:
		 *   DC->DL switch may happen before vsync, it may free
		 *   ovl2mem_handle!
		 */
		if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
			_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
		       pgc->cmdq_handle_ovl1to2_config,
		       (CmdqAsyncFlushCB)_ovl_wdma_fence_release_callback,
		       DISP_SESSION_DECOUPLE_MIRROR_MODE);
			dprec_logger_trigger(DPREC_LOGGER_PRIMARY_TRIGGER,
				0xffffffff, 0);
		} else {
			dprec_logger_trigger(DPREC_LOGGER_PRIMARY_TRIGGER,
				0xffffffff, 0xaaaaaaaa);
		}
	}
	return 0;
}

static int primary_display_trigger_nolock(int blocking, void *callback,
	int need_merge)
{
	int ret = 0;

	DISPFUNC();

	last_primary_trigger_time = sched_clock();
	if (is_switched_dst_mode) {
		/* switch to vdo mode if trigger disp */
		primary_display_switch_dst_mode(1);
		is_switched_dst_mode = false;
	}

	if (gTriggerDispMode > 0) {
		primary_display_release_fence_fake();
		return ret;
	}

	primary_trigger_cnt++;

	if (pgc->state == DISP_SLEPT) {
		DISPWARN("%s, skip because primary dipslay is slept\n",
			__func__);
		goto done;
	}
	primary_display_idlemgr_kick(__func__, 0);

	dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE ||
	    pgc->session_mode == DISP_SESSION_RDMA_MODE) {
		_trigger_display_interface(blocking,
			_ovl_fence_release_callback,
			DISP_SESSION_DIRECT_LINK_MODE);
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		_trigger_display_interface(0, _ovl_fence_release_callback,
			DISP_SESSION_DIRECT_LINK_MIRROR_MODE);

		if (pgc->need_trigger_ovl1to2 == 0) {
			DISPWARN(
				"There is no output config when directlink mirror!!\n");
		} else {
			primary_display_remove_output(
				_wdma_fence_release_callback,
				DISP_SESSION_DIRECT_LINK_MIRROR_MODE);
			pgc->need_trigger_ovl1to2 = 0;
		}
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
			pgc->cmdq_handle_ovl1to2_config,
			(CmdqAsyncFlushCB)_ovl_fence_release_callback,
			DISP_SESSION_DECOUPLE_MODE);
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		if (need_merge == 0 || primary_is_sec()) {
			trigger_decouple_mirror();
		} else {
			/*
			 * because only one of MM/UI thread will set output
			 * we have to merge it up, then trigger at next vsync
			 * see decouple_trigger_worker_thread below
			 */
			atomic_set(&decouple_trigger_event, 1);
			wake_up(&decouple_trigger_wq);
		}
	}

	dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	smart_ovl_try_switch_mode_nolock();

	atomic_set(&delayed_trigger_kick, 1);

done:
#ifdef CONFIG_MTK_AEE_FEATURE
	if ((primary_trigger_cnt > 1) && aee_kernel_Powerkey_is_press()) {
		aee_kernel_wdt_kick_Powkey_api("primary_display_trigger",
			WDT_SETBY_Display);
		primary_trigger_cnt = 0;
	}
#endif
	if (pgc->session_id > 0)
		update_frm_seq_info(0, 0, 0, FRM_TRIGGER);

	return ret;
}

int primary_display_trigger(int blocking, void *callback, int need_merge)
{
	int ret;

	_primary_path_lock(__func__);
	ret = primary_display_trigger_nolock(blocking, callback,
		need_merge);

	atomic_set(&hwc_configing, 0);

	_primary_path_unlock(__func__);
	return ret;
}

/*
 * the function will trigger dc and wake up dc thread for merge status;
 * decouple_trigger thread->trigger mirror->decouple_update_rdma_config_thread
 */
static int decouple_trigger_worker_thread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_event(decouple_trigger_wq,
			atomic_read(&decouple_trigger_event));
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

		_primary_path_lock(__func__);
		trigger_decouple_mirror();
		atomic_set(&decouple_trigger_event, 0);
		wake_up(&decouple_trigger_wq);
		_primary_path_unlock(__func__);
		if (kthread_should_stop()) {
			DISPWARN("error: stop %s as demond\n",
				__func__);
			break;
		}
	}
	return 0;
}

static int config_wdma_output(disp_path_handle disp_handle,
	struct cmdqRecStruct *cmdq_handle,
	struct disp_output_config *output)
{
	struct disp_ddp_path_config *pconfig = NULL;

	ASSERT(output != NULL);

	pconfig = dpmgr_path_get_last_config(disp_handle);
	pconfig->wdma_config.dstAddress = (unsigned long)output->pa;

	pconfig->wdma_config.srcHeight = primary_display_get_height();
	pconfig->wdma_config.srcWidth = primary_display_get_width();

	pconfig->wdma_config.clipX = output->x;
	pconfig->wdma_config.clipY = output->y;
	pconfig->wdma_config.clipHeight = output->height;
	pconfig->wdma_config.clipWidth = output->width;
	pconfig->wdma_config.outputFormat =
		disp_fmt_to_unified_fmt(output->fmt);
	pconfig->wdma_config.useSpecifiedAlpha = 1;
	pconfig->wdma_config.alpha = 0xFF;
	pconfig->wdma_config.dstPitch =
		output->pitch * UFMT_GET_Bpp(pconfig->wdma_config.outputFormat);
	pconfig->wdma_config.security = output->security;
	pconfig->wdma_dirty = 1;

	return dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
}

static int _convert_disp_output_to_memout(struct disp_output_config *src,
					  struct disp_mem_output_config *dst)
{
	dst->fmt = disp_fmt_to_unified_fmt(src->fmt);

	dst->vaddr = (unsigned long)src->va;
	dst->security = src->security;
	dst->w = src->width;
	dst->h = src->height;

	dst->addr = (unsigned long)src->pa;

	dst->buff_idx = src->buff_idx;
	dst->interface_idx = src->interface_idx;

	dst->x = src->x;
	dst->y = src->y;
	dst->pitch = src->pitch * UFMT_GET_Bpp(dst->fmt);
	return 0;
}

static int primary_frame_cfg_output(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	disp_path_handle disp_handle;
	struct cmdqRecStruct *cmdq_handle = NULL;

	if (pgc->state == DISP_SLEPT) {
		DISPWARN("mem out is already slept or mode wrong(%d)\n",
			pgc->session_mode);
		goto done;
	}

	if (!primary_display_is_mirror_mode()) {
		DISPWARN("should not config output if not mirror mode!!\n");
		goto done;
	}

	if (primary_display_is_decouple_mode()) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	} else {
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	if (primary_display_is_decouple_mode()) {
		pgc->need_trigger_dcMirror_out = 1;
	} else {
		/* direct link mirror mode should add memout first */
		dpmgr_path_add_memout(pgc->dpmgr_handle,
			DISP_MODULE_OVL0, cmdq_handle);
		pgc->need_trigger_ovl1to2 = 1;
	}

	ret = config_wdma_output(disp_handle, cmdq_handle, &cfg->output_cfg);

	if ((pgc->session_id > 0) && primary_display_is_decouple_mode())
		update_frm_seq_info((unsigned long)(cfg->output_cfg.pa), 0,
			mtkfb_query_frm_seq_by_addr(pgc->session_id, 0, 0),
			FRM_CONFIG);

	_convert_disp_output_to_memout(&cfg->output_cfg, &mem_config);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_wdma_config,
		MMPROFILE_FLAG_PULSE, cfg->output_cfg.buff_idx,
		(unsigned long)(cfg->output_cfg.pa));

done:
	return ret;
}

enum SVP_STATE {
	SVP_NOMAL = 0,
	SVP_IN_POINT,
	SVP_SEC,
	SVP_2_NOMAL,
	SVP_EXIT_POINT
};

static enum SVP_STATE svp_state = SVP_NOMAL;
static int svp_sum;

#ifndef OPT_BACKUP_NUM
	#define OPT_BACKUP_NUM 3
#endif

static enum DISP_HELPER_OPT opt_backup_name[OPT_BACKUP_NUM] = {
	DISP_OPT_SMART_OVL,
	DISP_OPT_IDLEMGR_SWTCH_DECOUPLE,
	DISP_OPT_BYPASS_OVL
};

static int opt_backup_value[OPT_BACKUP_NUM];
static unsigned int idlemgr_flag_backup;

static int svp_inited;

/* svp_inited: make sure the opt_backup &
 * restore_opt functions be used in pairs.
 */
static int disp_enter_svp(enum SVP_STATE state)
{
	int i;

	if (state != SVP_IN_POINT)
		return 0;

	if (svp_inited == 0) {
		for (i = 0; i < OPT_BACKUP_NUM; i++) {
			opt_backup_value[i] =
				disp_helper_get_option(opt_backup_name[i]);
			disp_helper_set_option(opt_backup_name[i], 0);
		}
		idlemgr_flag_backup = set_idlemgr(0, 0);
		svp_inited = 1;
	}

	if (primary_display_is_decouple_mode() &&
		(!primary_display_is_mirror_mode())) {
		/* switch to DL */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			pgc->session_id, 0, NULL, 0);
	}

	return 0;
}

static int disp_leave_svp(enum SVP_STATE state)
{
	int i;

	if (state != SVP_EXIT_POINT || svp_inited != 1)
		return 0;

	for (i = 0; i < OPT_BACKUP_NUM; i++)
		disp_helper_set_option(opt_backup_name[i], opt_backup_value[i]);

	set_idlemgr(idlemgr_flag_backup, 0);
	svp_inited = 0;

	return 0;
}

static int setup_disp_sec(struct disp_ddp_path_config *data_config,
	struct cmdqRecStruct *cmdq_handle, int is_locked)
{
	int i, has_sec_layer = 0;

	for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
		if (data_config->ovl_config[i].layer_en &&
		    (data_config->ovl_config[i].security == DISP_SECURE_BUFFER))
			has_sec_layer = 1;
	}

	if (has_sec_layer != primary_is_sec()) {
		mmprofile_log_ex(ddp_mmp_get_events()->sec,
			MMPROFILE_FLAG_PULSE, has_sec_layer, 0);
		/* sec/nonsec switch */

		cmdqRecReset(cmdq_handle);
		if (primary_display_is_decouple_mode())
			cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
		else
			_cmdq_insert_wait_frame_done_token_mira(
				pgc->cmdq_handle_config);

		mmprofile_log_ex(ddp_mmp_get_events()->sec,
			MMPROFILE_FLAG_PULSE, has_sec_layer, 1);

		if (has_sec_layer) {
			/* switch nonsec --> sec */
			svp_state = SVP_IN_POINT;
			disp_enter_svp(svp_state);
			mmprofile_log_ex(ddp_mmp_get_events()->sec,
				MMPROFILE_FLAG_START, 0, 0);
		} else {
			/* switch sec --> nonsec */
			svp_state = SVP_2_NOMAL;
			svp_sum = 0;
			mmprofile_log_ex(ddp_mmp_get_events()->sec,
				MMPROFILE_FLAG_END, 0, 0);
		}
	}

	if ((has_sec_layer == primary_is_sec()) &&
		(primary_is_sec() == 0)) {
		if (svp_state == SVP_2_NOMAL) {
			svp_sum++;
			if (svp_sum > 2) {
				svp_sum = 0;
				svp_state = SVP_EXIT_POINT;
				disp_leave_svp(svp_state);
			}
		} else {
			svp_state = SVP_NOMAL;
		}
	}

	if ((has_sec_layer == primary_is_sec()) &&
		(primary_is_sec() == 1)) /* IN SVP now!*/
		svp_state = SVP_SEC;

	pgc->is_primary_sec = has_sec_layer;
	return 0;
}

static int can_bypass_ovl(struct disp_ddp_path_config *data_config,
	int *bypass_layer_id)
{
	int total_layer = 0;
	int i;
	unsigned int w, h;
	struct OVL_CONFIG_STRUCT *oc;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	/* rdma doesn't support rotation */
	return 0;
#endif

	if (!disp_helper_get_option(DISP_OPT_BYPASS_OVL))
		return 0;

	for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
		if (data_config->ovl_config[i].layer_en) {
			total_layer++;
			*bypass_layer_id = i;
		}
	}

	if (total_layer != 1)
		return 0;

	oc = &data_config->ovl_config[*bypass_layer_id];
	if (oc->src_w != oc->dst_w || oc->src_h != oc->dst_h)
		return 0;

	/* rdma cannot process dim layer */
	if (data_config->ovl_config[*bypass_layer_id].source !=
		OVL_LAYER_SOURCE_MEM)
		return 0;

	/* now we have only 1 layer */
	/* check background (bg).
	 * If top_bg=0, and left bg!=0, we should not use rdma mode.
	 */
	if (data_config->ovl_config[*bypass_layer_id].dst_y == 0 &&
	    data_config->ovl_config[*bypass_layer_id].dst_x != 0)
		return 0;

	/*
	 * we need to check layer size, because rdma has
	 * output_valid_thres setting
	 * if (size < output_valid_thres) RDMA will hang !!
	 */
	h = data_config->ovl_config[*bypass_layer_id].dst_h;
	w = data_config->ovl_config[*bypass_layer_id].dst_w;

	/* RDMA must use even width */
	if (w & 0x1)
		return 0;
	if (w * h <= 512 * 16 / 2)
		return 0;

	return 1;
}

static int evaluate_bandwidth_save(struct disp_ddp_path_config *cfg,
	int *ori, int *act)
{
	int i = 0;
	int pixel = 0;
	int partial_pixel = 0;
	int save = 0;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		int layer_pixel = 0;
		struct disp_rect layer_roi = {0, 0, 0, 0};
		struct disp_rect layer_partial_roi = {0, 0, 0, 0};
		struct OVL_CONFIG_STRUCT *layer = &cfg->ovl_config[i];

		if (!layer->layer_en)
			continue;

		layer_pixel = layer->dst_w * layer->dst_h;
		pixel += layer_pixel;
		if (cfg->ovl_partial_dirty) {
			layer_roi.x = layer->dst_x;
			layer_roi.y = layer->dst_y;
			layer_roi.width = layer->dst_w;
			layer_roi.height = layer->dst_h;
			if (rect_intersect(&layer_roi, &cfg->ovl_partial_roi,
				&layer_partial_roi))
				partial_pixel +=
					layer_partial_roi.width *
					layer_partial_roi.height;

		} else {
			partial_pixel += layer_pixel;
		}
	}

	if (pixel)
		save = (pixel - partial_pixel) * 100 / pixel;

	*ori = pixel;
	*act = partial_pixel;

	DISPDBG("frame partial save:%d, %d, %%%d\n",
		pixel, partial_pixel, save);
	return 0;
}

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
static int primary_display_get_round_corner_mva(
			unsigned int *tp_mva, unsigned int *bt_mva,
			unsigned int *pitch, unsigned int *height,
			unsigned int *height_bot)
{
	unsigned char ret = -1;
	unsigned char argb4444_bpp = 2;
	unsigned int corner_size = 0;
	unsigned int bt_size = 0;
	unsigned int dal_buf_size = DAL_GetLayerSize();
	unsigned int frame_buf_size = DISP_GetFBRamSize();
	unsigned int vram_buf_size = mtkfb_get_fb_size();
	unsigned long frame_buf_mva =
			primary_display_get_frame_buffer_mva_address();

	if (vram_buf_size > dal_buf_size + frame_buf_size) {
		*height =
			primary_display_get_corner_pattern_height();
		*height_bot =
			primary_display_get_corner_pattern_height_bot();
		*height =
			primary_display_get_corner_pattern_height();
		*pitch =
			primary_display_get_width();
		corner_size = (*pitch) * (*height) * argb4444_bpp;
		bt_size = (*pitch) * (*height_bot) * argb4444_bpp;

		*tp_mva =
			frame_buf_mva + vram_buf_size -
			corner_size - bt_size;
		*bt_mva =
			frame_buf_mva + vram_buf_size -
			corner_size;

		ret = 0;
	} else {
		DISPERR("vram_buf may not contain corner size!\n");
	}
	return 0;
}

void add_round_corner_layers(
	struct disp_ddp_path_config *cfg, unsigned int w, unsigned int h,
	unsigned int h_bot, unsigned int pitch,
	unsigned long mva_top, unsigned long mva_bot,
	enum DISP_MODULE_ENUM module, unsigned int layer,
	unsigned int phy_layer, unsigned int enable)
{
	struct OVL_CONFIG_STRUCT *input_phy, *input_ext;
	unsigned int offset =
			round_corner_offset_enable ? 100 : 0;

	input_phy = &(cfg->ovl_config[layer]);
	input_phy->ovl_index = module;
	input_phy->layer = layer;
	input_phy->isDirty = 1;
	input_phy->buff_idx = -1;
	input_phy->layer_en = enable;
	input_phy->fmt = UFMT_RGBA4444;
	input_phy->addr = (unsigned long)mva_top;
	input_phy->const_bld = 0;
	input_phy->src_x = 0;
	input_phy->src_y = 0;
	input_phy->src_w = w;
	input_phy->src_h = h;
	input_phy->src_pitch = pitch*2;
	input_phy->dst_x = 0;
	input_phy->dst_y = offset;
	input_phy->dst_w = w;
	input_phy->dst_h = h;
	input_phy->aen = 1;
	input_phy->sur_aen = 0;
	input_phy->alpha = 255;
	input_phy->keyEn = 0;
	input_phy->key = 0;
	input_phy->src_alpha = 0;
	input_phy->dst_alpha = 0;
	input_phy->source = OVL_LAYER_SOURCE_MEM;
	input_phy->security = 0;
	input_phy->yuv_range = 0;
	input_phy->ext_layer = -1;
	input_phy->ext_sel_layer = -1;
	input_phy->phy_layer = phy_layer;

	input_ext = &(cfg->ovl_config[layer + 1]);
	input_ext->ovl_index = module;
	input_ext->layer = layer + 1;
	input_ext->isDirty = 1;
	input_ext->buff_idx = -1;
	input_ext->layer_en = enable;
	input_ext->fmt = UFMT_RGBA4444;
	input_ext->addr = (unsigned long)mva_bot;
	input_ext->const_bld = 0;
	input_ext->src_x = 0;
	input_ext->src_y = 0;
	input_ext->src_w = w;
	input_ext->src_h = h;
	input_ext->src_pitch = pitch*2;
	input_ext->dst_x = 0;
	input_ext->dst_y =
		primary_display_get_height() - h - offset;
	input_ext->dst_w = w;
	input_ext->dst_h = h;
	input_ext->aen = 1;
	input_ext->sur_aen = 0;
	input_ext->alpha = 255;
	input_ext->keyEn = 0;
	input_ext->key = 0;
	input_ext->src_alpha = 0;
	input_ext->dst_alpha = 0;
	input_ext->source = OVL_LAYER_SOURCE_MEM;
	input_ext->security = 0;
	input_ext->yuv_range = 0;
	input_ext->ext_layer = 2;
	input_ext->ext_sel_layer = phy_layer;
	input_ext->phy_layer = phy_layer;

	if (full_content) {
		input_ext->src_h = h_bot;
		input_ext->dst_y =
			primary_display_get_height() - h_bot - offset;
		input_ext->dst_h = h_bot;
	}
}
#endif

static bool disp_rsz_frame_has_rsz_layer(struct disp_frame_cfg_t *cfg)
{
	int i = 0;
	bool rsz = false;
	int path = 0;

	for (i = 0; i < cfg->input_layer_num; i++) {
		struct disp_input_config *input_cfg = &cfg->input_cfg[i];

		if (!input_cfg->layer_enable)
			continue;

		if (input_cfg->src_width != input_cfg->tgt_width ||
		    input_cfg->src_height != input_cfg->tgt_height) {
			rsz = true;
			break;
		}
	}

	path = HRT_GET_PATH_ID(HRT_GET_PATH_SCENARIO(cfg->overlap_layer_num));
	if ((path != 2 && path != 3 && path != 4) && (rsz == true)) {
		struct disp_input_config *c = &cfg->input_cfg[i];

		DISPERR("not RPO but L%d(%u,%u,%ux%u)->(%u,%u,%ux%u)\n",
			i, c->src_offset_x, c->src_offset_y, c->src_width,
			c->src_height, c->tgt_offset_x, c->tgt_offset_y,
			c->tgt_width, c->tgt_height);
	}

	return rsz;
}

static void rsz_in_out_roi(struct disp_frame_cfg_t *cfg,
				struct disp_ddp_path_config *data_config)
{
	int i = 0;
	struct disp_rect dst_layer_roi = {0, 0, 0, 0};
	struct disp_rect dst_total_roi = {0, 0, 0, 0};
	struct disp_rect src_layer_roi = {0, 0, 0, 0};
	struct disp_rect src_total_roi = {0, 0, 0, 0};
	struct disp_input_config *input_cfg = NULL;

	data_config->rsz_enable = FALSE;

	for (i = 0; i < cfg->input_layer_num; i++) {

		input_cfg = &cfg->input_cfg[i];

		if (input_cfg->layer_enable) {

			if (i == 0 &&
				input_cfg->buffer_source == DISP_BUFFER_ALPHA)
				continue;

			if (input_cfg->src_width < input_cfg->tgt_width ||
				input_cfg->src_height < input_cfg->tgt_height) {
				rect_make(&src_layer_roi,
					(input_cfg->tgt_offset_x
					* input_cfg->src_width)
					/ input_cfg->tgt_width,
					(input_cfg->tgt_offset_y
					* input_cfg->src_height)
					/ input_cfg->tgt_height,
					input_cfg->src_width,
					input_cfg->src_height);
				rect_make(&dst_layer_roi,
					input_cfg->tgt_offset_x,
					input_cfg->tgt_offset_y,
					input_cfg->tgt_width,
					input_cfg->tgt_height);
				rect_join(&src_layer_roi,
					&src_total_roi, &src_total_roi);
				rect_join(&dst_layer_roi,
					&dst_total_roi, &dst_total_roi);
				data_config->rsz_enable = TRUE;
			} else
				break;
		}
	}

	data_config->rsz_src_roi = src_total_roi;
	data_config->rsz_dst_roi = dst_total_roi;

	DISPINFO("RPO] rsz_src(x,y,w,h)=(%d,%d,%d,%d)\n",
		data_config->rsz_src_roi.x,
		data_config->rsz_src_roi.y,
		data_config->rsz_src_roi.width,
		data_config->rsz_src_roi.height);
	DISPINFO("[RPO] rsz_dst(x,y,w,h)=(%d,%d,%d,%d)\n",
		data_config->rsz_dst_roi.x,
		data_config->rsz_dst_roi.y,
		data_config->rsz_dst_roi.width,
		data_config->rsz_dst_roi.height);
}

static void _ovl_sbch_invalid_config(struct cmdqRecStruct *cmdq_handle)
{
	int i = 0, j = 0;
	CMDQ_VARIABLE sbch_invalid_status;
	CMDQ_VARIABLE result;
	CMDQ_VARIABLE shift;

	cmdq_op_init_variable(&sbch_invalid_status);
	cmdq_op_init_variable(&result);
	cmdq_op_init_variable(&shift);

	for (j = 0; j < OVL_NUM; j++) {
		unsigned long ovl_base = ovl_base_addr(j);

		if (ovl_base == 0)
			continue;

		/* Read SBCH invalid status */
		cmdq_op_read_reg(cmdq_handle,
				disp_addr_convert(DISP_REG_OVL_SBCH_CON
					+ ovl_base),
				&sbch_invalid_status, ~0x0);

		/* If SBCH layer status is invalid, disable sbch_tran_en */

		/* SBCH phy invalid status judgement */
		for (i = 0; i < OVL_MODULE_MAX_PHY_LAYER; i++) {
			cmdq_op_assign(cmdq_handle, &shift, (1 << (16 + i)));

			cmdq_op_and(cmdq_handle, &result,
						sbch_invalid_status, shift);

			cmdq_op_if(cmdq_handle, result, CMDQ_NOT_EQUAL, 0);

			cmdq_op_write_reg(cmdq_handle,
				disp_addr_convert(DISP_REG_OVL_SBCH
					+ ovl_base),
				0, (1 << (16 + (i * 4))));

			cmdq_op_end_if(cmdq_handle);

		}

		/* SBCH ext invalid status judgement */
		for (i = 0; i < OVL_MODULE_MAX_EXT_LAYER; i++) {
			cmdq_op_assign(cmdq_handle, &shift, (1 << (20 + i)));

			cmdq_op_and(cmdq_handle, &result,
						sbch_invalid_status, shift);

			cmdq_op_if(cmdq_handle, result, CMDQ_NOT_EQUAL, 0);

			cmdq_op_write_reg(cmdq_handle,
				disp_addr_convert(DISP_REG_OVL_SBCH_EXT
					+ ovl_base),
				0, (1 << (16 + (i * 4))));

			cmdq_op_end_if(cmdq_handle);
		}
	}
}

static int _config_ovl_input(struct disp_frame_cfg_t *cfg,
			     disp_path_handle disp_handle,
			     struct cmdqRecStruct *cmdq_handle)
{
	int ret = 0, i = 0, layer = 0, aee_layer = 0;
	struct disp_ddp_path_config *data_config = NULL;
	int max_layer_id_configed = 0;
	int bypass = 0, bypass_layer_id = 0;
	int hrt_level;
	struct disp_rect total_dirty_roi = {0, 0, 0, 0};
	static long long total_ori;
	static long long total_partial;

	int j, l_num;
#ifdef DEBUG_OVL_CONFIG_TIME
	cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_config_time,
		0, 0x10008028);
#endif
	/*=== create new data_config for ovl input ===*/
	data_config = dpmgr_path_get_last_config(disp_handle);

	disp_layer_info_statistic(data_config, cfg);

	if (disp_partial_is_support()) {
		if (primary_display_is_directlink_mode())
			disp_partial_compute_ovl_roi(cfg, data_config,
			&total_dirty_roi);
		else
			assign_full_lcm_roi(&total_dirty_roi);
	}

	if (disp_rsz_frame_has_rsz_layer(cfg))
		assign_full_lcm_roi(&total_dirty_roi);

	rsz_in_out_roi(cfg, data_config);

	for (i = 0; i < cfg->input_layer_num; i++) {
		struct disp_input_config *input_cfg = &cfg->input_cfg[i];
		struct OVL_CONFIG_STRUCT *ovl_cfg;

		layer = input_cfg->layer_id;
		ovl_cfg = &(data_config->ovl_config[layer]);
		if (cfg->setter != SESSION_USER_AEE) {
			aee_layer = primary_display_get_option("ASSERT_LAYER");
			if (is_DAL_Enabled() &&	layer == aee_layer) {
				DISPMSG("skip AEE layer %d\n", layer);
				continue;
			}
		} else {
			DISPMSG("set AEE layer %d\n", layer);
		}
		_convert_disp_input_to_ovl(ovl_cfg, input_cfg);

		dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG,
			((ovl_cfg->src_x & 0xFFFF) << 16) |
			(ovl_cfg->src_y & 0xFFFF),
			((ovl_cfg->src_w & 0xFFFF) << 16) |
			(ovl_cfg->src_h & 0xFFFF));
		mmprofile_log_ex(ddp_mmp_get_events()->primary_config,
			MMPROFILE_FLAG_PULSE,
			((ovl_cfg->layer & 0xF) << 28) |
			((ovl_cfg->layer_en & 0xF) << 24) |
			((ovl_cfg->ext_layer & 0xF) << 20) |
			((ovl_cfg->ext_sel_layer & 0xF) << 16) |
			((ovl_cfg->yuv_range & 0xF) << 12) |
			((UFMT_GET_RGB(ovl_cfg->fmt) & 0xF) << 8) |
			(UFMT_GET_ID(ovl_cfg->fmt) & 0xFF), ovl_cfg->addr);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG,
			((ovl_cfg->dst_x & 0xFFFF) << 16) |
			(ovl_cfg->dst_y & 0xFFFF),
			((ovl_cfg->dst_w & 0xFFFF) << 16) |
			(ovl_cfg->dst_h & 0xFFFF));

		dprec_mmp_dump_ovl_layer(ovl_cfg, layer, 1);

		if ((ovl_cfg->layer == 0) &&
			(!primary_display_is_decouple_mode()))
			update_frm_seq_info(ovl_cfg->addr,
			    ovl_cfg->src_x * 4 +
			    ovl_cfg->src_y * ovl_cfg->src_pitch,
			    mtkfb_query_frm_seq_by_addr(pgc->session_id, 0, 0),
			    FRM_CONFIG);

		if (max_layer_id_configed < layer)
			max_layer_id_configed = layer;

		data_config->ovl_layer_dirty |= (1 << i);
	}

	hrt_level = HRT_GET_DVFS_LEVEL(cfg->overlap_layer_num);
	data_config->overlap_layer_num = hrt_level;

#if 0
#ifdef MTK_FB_MMDVFS_SUPPORT
	/* TODO: Set Vcore Level here. */
	if (hrt_level > HRT_LEVEL_NUM - 1)
		DISPCHECK("overlayed layer num is %d > %d\n",
			hrt_level, HRT_LEVEL_NUM - 1);
	if ((hrt_level - dvfs_last_ovl_req) > 0 &&
		(!primary_display_is_decouple_mode()))
		primary_display_request_dvfs_perf(
			0, hrt_level);

	dvfs_last_ovl_req = hrt_level;
#endif

	if (disp_helper_get_option(DISP_OPT_SHOW_VISUAL_DEBUG_INFO)) {
		char msg[10];

		snprintf(msg, sizeof(msg), "HRT=%d,", hrt_level);
		screen_logger_add_message("HRT", MESSAGE_REPLACE, msg);
	}
#endif

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(disp_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

	if (cmdq_handle) {
		int sess_mode = pgc->session_mode;

		setup_disp_sec(data_config, cmdq_handle, 1);

		if (sess_mode != pgc->session_mode) {
			/* the switch mode case:
			 * 1)enter svp: DC->DL; 2)leave svp:
			 * restore sess_mode, DL->DC
			 */
			DISPMSG(
				"setup_disp_sec() has switch mode, then update the handle.\n");

			if (primary_display_is_decouple_mode()) {
				disp_handle = pgc->ovl2mem_path_handle;
				cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
			} else {
				disp_handle = pgc->dpmgr_handle;
				cmdq_handle = pgc->cmdq_handle_config;
			}
			data_config = dpmgr_path_get_last_config(disp_handle);
			data_config->overlap_layer_num = hrt_level;
		}
	}

	/* check bypass ovl */
	if (cfg->setter != SESSION_USER_INVALID) {
		bypass = can_bypass_ovl(data_config, &bypass_layer_id);
		if (bypass) {
			/* switch to rdma mode */
			if (pgc->session_mode ==
				DISP_SESSION_DIRECT_LINK_MODE) {
				assign_full_lcm_roi(&total_dirty_roi);
				primary_display_config_full_roi(data_config,
					disp_handle, cmdq_handle);
				do_primary_display_switch_mode(
					DISP_SESSION_RDMA_MODE,
					pgc->session_id, 0, cmdq_handle, 0);
			}
		} else {
			if (pgc->session_mode == DISP_SESSION_RDMA_MODE) {
				do_primary_display_switch_mode(
					DISP_SESSION_DIRECT_LINK_MODE,
					pgc->session_id, 0, cmdq_handle, 0);
				assign_full_lcm_roi(&total_dirty_roi);
			}
		}
	}

	/* Disable sbch when user isn't HWC */
	if (cfg->setter != SESSION_USER_HWC)
		data_config->sbch_enable = 0;
	else
		data_config->sbch_enable = 1;

	if (pgc->session_mode != DISP_SESSION_RDMA_MODE) {
		data_config->ovl_dirty = 1;
	} else {
		ret = ddp_convert_ovl_input_to_rdma(&data_config->rdma_config,
			&data_config->ovl_config[bypass_layer_id],
			data_config->dst_w, data_config->dst_h);
		data_config->rdma_dirty = 1;

		/* no need ioctl because of rdma_dirty */
		set_is_dc(1);

		dynamic_debug_msg_print(data_config->rdma_config.address,
			data_config->rdma_config.width,
			data_config->rdma_config.height,
			data_config->rdma_config.pitch,
			UFMT_GET_Bpp(data_config->rdma_config.inputFormat));
	}

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* display hrt request*/
	if (primary_display_is_decouple_mode() &&
		primary_display_is_mirror_mode()) {
		/* if DCMirror mode(WFD/ScreenRecord), */
		/* forced set dvfs to opp0 */
		primary_display_request_dvfs_perf(0,
			HRT_LEVEL_LEVEL3);
		dvfs_last_ovl_req = HRT_LEVEL_LEVEL3;
	} else if (hrt_level > HRT_LEVEL_LEVEL2 &&
		primary_display_is_directlink_mode()) {
		primary_display_request_dvfs_perf(0,
			HRT_LEVEL_LEVEL3);
		dvfs_last_ovl_req = HRT_LEVEL_LEVEL3;
	} else if (hrt_level > HRT_LEVEL_LEVEL1 &&
		primary_display_is_directlink_mode()) {
		primary_display_request_dvfs_perf(0,
			HRT_LEVEL_LEVEL2);
		dvfs_last_ovl_req = HRT_LEVEL_LEVEL2;
	} else if (hrt_level > HRT_LEVEL_LEVEL0) {
		dvfs_last_ovl_req = HRT_LEVEL_LEVEL1;
	} else {
		dvfs_last_ovl_req = HRT_LEVEL_LEVEL0;
	}

	if (disp_helper_get_option(DISP_OPT_SHOW_VISUAL_DEBUG_INFO)) {
		char msg[10];

		snprintf(msg, sizeof(msg), "HRT=%d,", hrt_level);
		screen_logger_add_message("HRT", MESSAGE_REPLACE, msg);
	}
#endif

	if (disp_helper_get_option(
			DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK)) {
		if (bypass) {
			if (set_one_layer(1))
				;
		} else {
			if (set_one_layer(0))
				;
		}
	}

	if (disp_partial_is_support() &&
		primary_display_is_directlink_mode()) {
		if (!ddp_debug_force_roi())
			disp_patial_lcm_validate_roi(pgc->plcm,
				&total_dirty_roi);
		if (is_equal_full_lcm(&total_dirty_roi))
			aal_request_partial_support(0);
		else
			aal_request_partial_support(1);

		if ((!aal_is_partial_support() ||
			PanelMaster_is_enable() == 1) &&
			!ddp_debug_force_roi())
			assign_full_lcm_roi(&total_dirty_roi);

		DISPDBG("frame partial roi(%d,%d,%d,%d)\n",
			total_dirty_roi.x, total_dirty_roi.y,
			total_dirty_roi.width, total_dirty_roi.height);

		if (!rect_equal(&total_dirty_roi,
			&data_config->ovl_partial_roi)) {
			/* update roi to lcm */
			disp_partial_update_roi_to_lcm(disp_handle,
				total_dirty_roi, cmdq_handle);
			data_config->ovl_partial_roi = total_dirty_roi;
			if (disp_helper_get_option(
				DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING)) {
				/* update rdma goden settin */
				set_rdma_width_height(total_dirty_roi.width,
					total_dirty_roi.height);
				dpmgr_path_ioctl(disp_handle, cmdq_handle,
					DDP_RDMA_GOLDEN_SETTING, data_config);
			}
		}
		if (is_equal_full_lcm(&total_dirty_roi))
			data_config->ovl_partial_dirty = 0;
		else
			data_config->ovl_partial_dirty = 1;

		if (ddp_debug_partial_statistic()) {
			int frame_ori = 0;
			int frame_partial = 0;
			long long save = 0;

			evaluate_bandwidth_save(data_config, &frame_ori,
				&frame_partial);
			total_ori += frame_ori;
			total_partial += frame_partial;

			if (total_ori) {
				unsigned long long tmp =
					(total_ori - total_partial) * 100;

				do_div(tmp, total_ori);
				save = tmp;
			}

			DISPDBG("total partial save:%%%lld\n", save);
		} else {
			total_ori = 0;
			total_partial = 0;
		}
	}

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	/* Workaroud!!!!!!*/
	/* disable sw rc, when decouple mirror */
	if (primary_display_is_mirror_mode()) {
		if (lcm_corner_en)
			add_round_corner_layers(data_config,
					primary_display_get_width(),
					corner_pattern_height,
					corner_pattern_height_bot,
					corner_pattern_width,
					top_mva, bottom_mva,
					DISP_MODULE_OVL0,
					TOTAL_OVL_LAYER_NUM, 3, 0);
	} else {
		if (lcm_corner_en) {
			if (disp_helper_get_option(DISP_OPT_ROUND_CORNER))
				add_round_corner_layers(data_config,
					primary_display_get_width(),
					corner_pattern_height,
					corner_pattern_height_bot,
					corner_pattern_width,
					top_mva, bottom_mva,
					DISP_MODULE_OVL0,
					TOTAL_OVL_LAYER_NUM, 3, 1);
			else
				add_round_corner_layers(data_config,
					primary_display_get_width(),
					corner_pattern_height,
					corner_pattern_height_bot,
					corner_pattern_width,
					top_mva, bottom_mva,
					DISP_MODULE_OVL0,
					TOTAL_OVL_LAYER_NUM, 3, 0);
		}
	}
#endif

	if (dump_output) {
		kfree(draw);
		draw = NULL;

		draw = kzalloc(sizeof(*draw), GFP_KERNEL);
		for (i = 0; i < TOTAL_REAL_OVL_LAYER_NUM; i++) {
			struct disp_input_config *input_cfg =
				&cfg->input_cfg[i];
			struct OVL_CONFIG_STRUCT *ovl_cfg;

			if (input_cfg->layer_enable) {
				layer = input_cfg->layer_id;
				ovl_cfg = &(data_config->ovl_config[layer]);
				_convert_disp_input_to_ovl(ovl_cfg, input_cfg);
			} else
				ovl_cfg = &(data_config->ovl_config[i]);

			if (ovl_cfg->layer_en == 1) {
				l_num = draw->layer_num++;
				draw->dx[l_num] = ovl_cfg->dst_x;
				draw->dy[l_num] = ovl_cfg->dst_y;
				draw->top_l_x[l_num] = ovl_cfg->dst_x;
				draw->top_l_y[l_num] = ovl_cfg->dst_y;
				draw->bot_r_x[l_num] =
					ovl_cfg->dst_x + ovl_cfg->dst_w - 1;
				draw->bot_r_y[l_num] =
					ovl_cfg->dst_y + ovl_cfg->dst_h - 1;
				draw->frame_width[l_num] = 6;
				if ((draw->dy[l_num] >
					primary_display_get_height() - 4*16))
					draw->dy[l_num] =
					primary_display_get_height() - 4*16 - 1;

				for (j = 0; j < draw->layer_num - 1; j++) {
					if ((draw->dx[l_num] == draw->dx[j]) &&
					(draw->dy[l_num] == draw->dy[j])) {
						draw->dx[l_num] =
							draw->dx[j] + 4*8;
					}
					if (((ovl_cfg->dst_x <=
						draw->top_l_x[j] +
						draw->frame_width[j])
							&&
						(ovl_cfg->dst_x >=
						draw->top_l_x[j]))
						||
						((ovl_cfg->dst_y <=
						draw->top_l_x[j] +
						draw->frame_width[j])
							&&
						(ovl_cfg->dst_y >=
						draw->top_l_x[j]))
						||
						((ovl_cfg->dst_x +
						ovl_cfg->dst_w >=
						draw->bot_r_x[j] -
						draw->frame_width[j])
							&&
						(ovl_cfg->dst_x +
						ovl_cfg->dst_w <=
						draw->bot_r_x[j]))
						||
						((ovl_cfg->dst_y +
						ovl_cfg->dst_h >=
						draw->bot_r_y[j] -
						draw->frame_width[j])
							&&
						(ovl_cfg->dst_y +
						ovl_cfg->dst_h <=
						draw->bot_r_y[j])))
						draw->frame_width[l_num] += 3;
					if (draw->top_l_y[l_num]
						+ draw->frame_width[l_num]
						> draw->bot_r_y[l_num])
						draw->frame_width[l_num] =
							draw->bot_r_y[l_num]
							- draw->top_l_y[l_num];
				}
			}
		}
	}

	ret = dpmgr_path_config(disp_handle, data_config, cmdq_handle);

#ifdef DEBUG_OVL_CONFIG_TIME
	cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_config_time,
		1, 0x10008028);
#endif
	if (!cmdq_handle)
		goto done;

	/*
	 * write fence_id/enable to DRAM using cmdq
	 * it will be used when release fence
	 * (put these after config registers done)
	 */
	for (i = 0; i < cfg->input_layer_num; i++) {
		unsigned int last_fence, cur_fence, sub;
		struct disp_input_config *input_cfg = &cfg->input_cfg[i];

		layer = input_cfg->layer_id;

		cmdqBackupReadSlot(pgc->cur_config_fence, layer, &last_fence);
		cur_fence = input_cfg->next_buff_idx;

		if (cur_fence != -1 && cur_fence > last_fence)
			cmdqRecBackupUpdateSlot(cmdq_handle,
				pgc->cur_config_fence, layer, cur_fence);

		/*
		 * for dim_layer/disable_layer/no_fence_layer, just release
		 * all fences configured.
		 * for other layers, release current_fence-1
		 */
		if (input_cfg->buffer_source == DISP_BUFFER_ALPHA ||
		    input_cfg->layer_enable == 0 || cur_fence == -1)
			sub = 0;
		else
			sub = 1;

		/* store overlap layer to layer0's
		 * subtractor_when_free: bit[31:16]
		 */
		if (layer == 0)
			sub |= hrt_level << 16;
		cmdqRecBackupUpdateSlot(cmdq_handle,
			pgc->subtractor_when_free, layer, sub);
	}

	/* SBCH invalid status judge and handle */
	if (disp_helper_get_option(DISP_OPT_OVL_SBCH) &&
			(data_config->sbch_enable == 1))
		_ovl_sbch_invalid_config(cmdq_handle);

	/* GCE read ovl register about full transparent layer */
	for (i = 0; i < OVL_NUM; i++) {
		if (data_config->read_dum_reg[i]) {
			unsigned long ovl_base = ovl_base_addr(i);

			data_config->read_dum_reg[i] = 0;

			/* full transparent layer */
			cmdqRecBackupRegisterToSlot(cmdq_handle,
				pgc->ovl_dummy_info, i,
				disp_addr_convert
				(DISP_REG_OVL_DUMMY_REG + ovl_base));
		}
	}

	/* check last ovl status: should be idle when config */
	if (primary_display_is_video_mode() &&
		!primary_display_is_decouple_mode()) {
		/* last OVL is DISP_MODULE_OVL0_2L */
		unsigned long ovl_base = ovl_base_addr(DISP_MODULE_OVL0_2L);

		cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_status_info,
			0, disp_addr_convert(DISP_REG_OVL_STA + ovl_base));
	}

done:
#ifdef DEBUG_OVL_CONFIG_TIME
	cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_config_time,
		2, 0x10008028);
#endif
	return ret;
}

/* notes: primary lock should be held when call this func */
static int primary_frame_cfg_input(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	unsigned int wdma_mva = 0;
	disp_path_handle disp_handle;
	struct cmdqRecStruct *cmdq_handle;
	struct disp_ccorr_config m_ccorr_config = cfg->ccorr_config;

	if (gTriggerDispMode > 0)
		return 0;

	if (disp_helper_get_option(DISP_OPT_IDLE_MGR))
		primary_display_idlemgr_kick(__func__, 0);

	if (primary_display_is_decouple_mode()) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	} else {
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	if (pgc->state == DISP_SLEPT) {
		DISPWARN("%s, skip because primary dipslay is slept\n",
			__func__);

		if (is_DAL_Enabled() &&
			cfg->setter == SESSION_USER_AEE &&
		    (cfg->input_cfg[0].layer_id ==
		    primary_display_get_option("ASSERT_LAYER"))) {
			struct disp_ddp_path_config *data_config =
				dpmgr_path_get_last_config(disp_handle);
			int layer = cfg->input_cfg[0].layer_id;

			ret = _convert_disp_input_to_ovl(
				&(data_config->ovl_config[layer]),
				&cfg->input_cfg[0]);
		}

		goto done;
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		fps_ctx_update(&primary_fps_ctx);

	if (disp_helper_get_option(DISP_OPT_FPS_EXT))
		fps_ext_ctx_update(&primary_fps_ext_ctx);

	if (disp_helper_get_option(DISP_OPT_SHOW_VISUAL_DEBUG_INFO))
		primary_show_basic_debug_info(cfg);

	_config_ovl_input(cfg, disp_handle, cmdq_handle);

	/* handle night light in DL, DC separately */
	if (m_ccorr_config.is_dirty) {
		int i = 0, all_zero = 1;

		for (i = 0; i <= 15; i += 5) {
			if (m_ccorr_config.color_matrix[i] != 0) {
				all_zero = 0;
				break;
			}
		}
		if (all_zero)
			disp_aee_print("HWC set zero matrix\n");
		else if (!primary_display_is_decouple_mode()) {
			disp_ccorr_set_color_matrix(cmdq_handle,
				m_ccorr_config.color_matrix,
				m_ccorr_config.mode);

			/* backup night params here */
			cmdqRecBackupUpdateSlot(cmdq_handle,
				pgc->night_light_params, 0,
				mem_config.m_ccorr_config.mode);
			for (i = 0; i < 16; i++)
				cmdqRecBackupUpdateSlot(cmdq_handle,
				pgc->night_light_params, i + 1,
				mem_config.m_ccorr_config.color_matrix[i]);
		} else
			mem_config.m_ccorr_config = m_ccorr_config;
	}

	if (primary_display_is_decouple_mode() &&
		!primary_display_is_mirror_mode()) {
		pgc->dc_buf_id++;
		pgc->dc_buf_id %= DISP_INTERNAL_BUFFER_COUNT;
		wdma_mva = pgc->dc_buf[pgc->dc_buf_id];
		if (wdma_mva == 0) {
			DISPWARN("%s, dc buffer does not exist\n", __func__);
			ret = -1;
			goto done;
		}

		decouple_wdma_config.dstAddress = wdma_mva;
		_config_wdma_output(&decouple_wdma_config,
			pgc->ovl2mem_path_handle,
			pgc->cmdq_handle_ovl1to2_config);
		mem_config.addr = wdma_mva;
		mem_config.buff_idx = -1;
		mem_config.interface_idx = -1;
		mem_config.security = DISP_NORMAL_BUFFER;
		mem_config.pitch = decouple_wdma_config.dstPitch;
		mem_config.fmt = decouple_wdma_config.outputFormat;
		mmprofile_log_ex(ddp_mmp_get_events()->primary_wdma_config,
			MMPROFILE_FLAG_PULSE, pgc->dc_buf_id, wdma_mva);
		show_layers_va = decouple_buffer_info[pgc->dc_buf_id]->va;
		DAL_CHECK_MFC_RET(MFC_Open(&show_mfc_handle,
			decouple_buffer_info[pgc->dc_buf_id]->va,
			DISP_GetScreenWidth(), DISP_GetScreenHeight(),
			3, color_wdma[0], DAL_COLOR_OPAQUE));
		DAL_CHECK_MFC_RET(MFC_SetScale(show_mfc_handle, 4));
	}
done:
	return ret;
}

int primary_display_config_input_multiple(
	struct disp_session_input_config *session_input)
{
	int ret = 0;
	struct disp_frame_cfg_t *frame_cfg;

	if (sizeof(session_input->config) != sizeof(frame_cfg->input_cfg)) {
		DISPWARN(
			"session input config size not equal frame config size\n");
		return -1;
	}
	frame_cfg = kzalloc(sizeof(struct disp_frame_cfg_t), GFP_KERNEL);
	if (frame_cfg == NULL)
		return -ENOMEM;

	frame_cfg->session_id = primary_session_id;
	frame_cfg->setter = session_input->setter;
	frame_cfg->input_layer_num = session_input->config_layer_num;
	frame_cfg->overlap_layer_num = HRT_LEVEL_LEVEL2;
	frame_cfg->ccorr_config = session_input->ccorr_config;

	memcpy(frame_cfg->input_cfg, session_input->config,
		sizeof(frame_cfg->input_cfg));

	if (disp_validate_ioctl_params(frame_cfg)) {
		ret = -EINVAL;
		goto out;
	}
	_primary_path_lock(__func__);

	atomic_set(&hwc_configing, 1);

	ret = primary_frame_cfg_input(frame_cfg);

	_primary_path_unlock(__func__);

out:
	kfree(frame_cfg);
	return ret;
}

int primary_display_frame_cfg(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	struct disp_session_sync_info *session_info =
		disp_get_session_sync_info_for_debug(cfg->session_id);
	struct dprec_logger_event *input_event, *output_event, *trigger_event;

	if (session_info) {
		input_event = &session_info->event_setinput;
		output_event = &session_info->event_setoutput;
		trigger_event = &session_info->event_trigger;
	} else {
		input_event = output_event = trigger_event = NULL;
	}

	_primary_path_lock(__func__);

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
	if (pgc->request_fps && HRT_FPS(cfg->overlap_layer_num) == 120)
		_display_set_lcm_refresh_rate(pgc->request_fps);
#endif

	/* set input */
	dprec_start(input_event, cfg->present_fence_idx, cfg->input_layer_num);
	primary_frame_cfg_input(cfg);
	dprec_done(input_event, cfg->overlap_layer_num, 0);

	if (cfg->output_en) {
		dprec_start(output_event, cfg->present_fence_idx,
			cfg->output_cfg.buff_idx);
		primary_frame_cfg_output(cfg);
		dprec_done(output_event, 0, 0);
	}

	if (trigger_event) {
		/* to debug UI thread or MM thread */
		unsigned int proc_name =
			(current->comm[0] << 24) | (current->comm[1] << 16) |
			(current->comm[2] << 8) | (current->comm[3] << 0);

		dprec_start(trigger_event, cfg->present_fence_idx, proc_name);
	}

	primary_display_trigger_nolock(0, NULL, 0);

	if (cfg->present_fence_idx != (unsigned int)-1)
		primary_display_update_present_fence(cfg->present_fence_idx);

	dprec_done(trigger_event, 0, 0);

	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_user_cmd(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct cmdqRecStruct *handle = NULL;
	int cmdqsize = 0;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_cmd,
		MMPROFILE_FLAG_START, _IOC_NR(cmd), 0);

	if (cmd == DISP_IOCTL_AAL_GET_HIST || cmd == DISP_IOCTL_CCORR_GET_IRQ) {
		_primary_path_lock(__func__);

		if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
				&handle);
			cmdqRecReset(handle);
			_cmdq_insert_wait_frame_done_token_mira(handle);
			cmdqsize = cmdqRecGetInstructionCount(handle);
		}

		if (pgc->state == DISP_SLEPT && handle) {
			cmdqRecDestroy(handle);
			handle = NULL;
		}
		_primary_path_unlock(__func__);

		/* only cmd mode & with disable mmsys clk will kick */
		if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS) &&
			!primary_display_is_video_mode())
			primary_display_idlemgr_kick(__func__, 1);

		ret = dpmgr_path_user_cmd(pgc->dpmgr_handle, cmd, arg, handle);

		if (handle) {
			if (cmdqRecGetInstructionCount(handle) > cmdqsize) {
				_primary_path_lock(__func__);
				if (pgc->state == DISP_ALIVE) {
					/* use non-blocking flush here to avoid
					 * primary path is locked for too long
					 */
					_cmdq_flush_config_handle_mira(
						handle, 0);
				}
				_primary_path_unlock(__func__);
			}
			cmdqsize = cmdqRecGetInstructionCount(handle);
			cmdqRecDestroy(handle);
		}
	} else {
		_primary_path_switch_dst_lock();
		_primary_path_lock(__func__);

		if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,
				&handle);
			cmdqRecReset(handle);
			_cmdq_insert_wait_frame_done_token_mira(handle);
			cmdqsize = cmdqRecGetInstructionCount(handle);
		}

		if (pgc->state == DISP_SLEPT && handle) {
			cmdqRecDestroy(handle);
			handle = NULL;
			goto user_cmd_unlock;
		}
		/* only cmd mode & with disable mmsys clk will kick */
		if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS) &&
			!primary_display_is_video_mode())
			primary_display_idlemgr_kick(__func__, 0);

		ret = dpmgr_path_user_cmd(pgc->dpmgr_handle, cmd, arg, handle);

		if (handle) {
			if (cmdqRecGetInstructionCount(handle) > cmdqsize &&
				pgc->state == DISP_ALIVE) {
				/* use non-blocking flush here to avoid
				 * primary path is locked for too long
				 */
				_cmdq_flush_config_handle_mira(handle, 0);
			}
			cmdqsize = cmdqRecGetInstructionCount(handle);
			cmdqRecDestroy(handle);
		}

user_cmd_unlock:
		_primary_path_unlock(__func__);
		_primary_path_switch_dst_unlock();

	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_cmd,
		MMPROFILE_FLAG_END, cmdqsize, 0);

	return ret;
}

int do_primary_display_switch_mode(int sess_mode, unsigned int session,
	int need_lock, struct cmdqRecStruct *handle, int block)
{
	int ret = 0, sw_only = 0;

	if (need_lock)
		_primary_path_lock(__func__);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
		MMPROFILE_FLAG_START, pgc->session_mode, sess_mode);

	if (pgc->session_mode == sess_mode)
		goto done;


	if (pgc->state == DISP_SLEPT) {
		DISPWARN(
			"primary display switch from %s to %s in suspend state!!!\n",
			session_mode_spy(pgc->session_mode),
			session_mode_spy(sess_mode));
		sw_only = 1;
	}

	DISPMSG("primary display will switch from %s to %s\n",
		session_mode_spy(pgc->session_mode),
		session_mode_spy(sess_mode));

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE &&
	    sess_mode == DISP_SESSION_DECOUPLE_MODE) {
		/* dl to dc */
		ret = DL_switch_to_DC_fast(sw_only, block);
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE &&
		   sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* dc to dl */
		ret = DC_switch_to_DL_fast(sw_only, block);
		/* primary_display_diagnose(__func__, __LINE__); */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE &&
		   sess_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		/* dl to dl mirror */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE &&
		   sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* dl mirror to dl */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE &&
		   sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		/* dl to dc mirror  mirror */
		ret = DL_switch_to_DC_fast(sw_only, block);
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE &&
		   sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* dc mirror  to dl */
		ret = DC_switch_to_DL_fast(sw_only, block);
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE &&
		   sess_mode == DISP_SESSION_DECOUPLE_MODE){
		/* do nothing */
		/* just switch mode */
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE &&
		   sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE){
		/* do nothing */
		/* just switch mode */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE &&
		   sess_mode == DISP_SESSION_RDMA_MODE) {
		ret = DL_switch_to_rdma_mode(handle, block);
	} else if (pgc->session_mode == DISP_SESSION_RDMA_MODE &&
		   sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		ret = rdma_mode_switch_to_DL(handle, block);
	} else if (pgc->session_mode == DISP_SESSION_RDMA_MODE &&
		   sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		/* switch to DL mode first */
		ret = rdma_mode_switch_to_DL(NULL, 0);
		if (ret)
			goto done;
		mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
			MMPROFILE_FLAG_PULSE, pgc->session_mode, 0);
		ret = DL_switch_to_DC_fast(0, 0);
	} else {
		DISPWARN("invalid mode switch from %s to %s\n",
			session_mode_spy(pgc->session_mode),
			session_mode_spy(sess_mode));
	}

done:
	if (ret)
		goto err;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_mode[pgc->session_mode],
			 MMPROFILE_FLAG_END, pgc->session_mode, sess_mode);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_mode[sess_mode],
			 MMPROFILE_FLAG_START, pgc->session_mode, sess_mode);

	pgc->session_mode = sess_mode;
	DISPMSG("primary display is %s mode now\n",
		session_mode_spy(pgc->session_mode));
	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
			 MMPROFILE_FLAG_PULSE, pgc->session_mode, sess_mode);

	screen_logger_add_message("sess_mode", MESSAGE_REPLACE,
		(char *)session_mode_spy(sess_mode));
err:
	mmprofile_log_ex(ddp_mmp_get_events()->primary_switch_mode,
			 MMPROFILE_FLAG_END, pgc->session_mode, sess_mode);
	if (ret)
		DISPERR("primary display switch mode fail\n");
	if (need_lock)
		_primary_path_unlock(__func__);
	pgc->session_id = session;
	return ret;
}

int primary_display_switch_mode(int sess_mode, unsigned int session, int force)
{
	int ret = 0;

	_primary_path_lock(__func__);
	primary_display_idlemgr_kick(__func__, 0);

	/* HWC only needs to control mirror or
	 * not mirror it doesn't need to control DL/DC
	 */
	if (!force &&
		primary_display_is_mirror_mode() == _is_mirror_mode(sess_mode))
		goto done;

	if (pgc->session_mode == sess_mode)
		goto done;

	while (primary_get_state() == DISP_BLANK) {
		_primary_path_unlock(__func__);
		DISPMSG("%s wait for leave TUI\n", __func__);
		primary_display_wait_not_state(DISP_BLANK,
			MAX_SCHEDULE_TIMEOUT);
		_primary_path_lock(__func__);
	}

	ret = do_primary_display_switch_mode(sess_mode, session, 0, NULL, 0);

done:
	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_switch_mode_blocked(int sess_mode,
	unsigned int session, int force)
{
	int ret = 0;

	_primary_path_lock(__func__);
	primary_display_idlemgr_kick(__func__, 0);

	/* HWC only needs to control mirror or
	 * not mirror it doesn't need to control DL/DC
	 */
	if (!force &&
		primary_display_is_mirror_mode() == _is_mirror_mode(sess_mode))
		goto done;


	if (pgc->session_mode == sess_mode)
		goto done;

	while (primary_get_state() == DISP_BLANK) {
		_primary_path_unlock(__func__);
		DISPMSG("%s wait for leave TUI\n", __func__);
		primary_display_wait_not_state(DISP_BLANK,
			MAX_SCHEDULE_TIMEOUT);
		_primary_path_lock(__func__);
	}

	ret = do_primary_display_switch_mode(sess_mode, session,
		0, NULL, 1);

done:
	_primary_path_unlock(__func__);

	return ret;
}

static int smart_ovl_try_switch_mode_nolock(void)
{
	unsigned int hwc_fps, lcm_fps;
	unsigned long long ovl_sz, rdma_sz;
	disp_path_handle disp_handle = NULL;
	struct disp_ddp_path_config *data_config = NULL;
	int i, stable;
	unsigned long long DL_bw, DC_bw, bw_th;

	if (!disp_helper_get_option(DISP_OPT_SMART_OVL))
		return 0;

	if (!primary_display_is_video_mode())
		return 0;

	if (pgc->session_mode != DISP_SESSION_DIRECT_LINK_MODE &&
	    pgc->session_mode != DISP_SESSION_DECOUPLE_MODE)
		return 0;

	if (pgc->state != DISP_ALIVE)
		return 0;

	lcm_fps = pgc->lcm_fps / 100;
	fps_ctx_get_fps(&primary_fps_ctx, &hwc_fps, &stable);

	/* we switch DL->DC only when fps is stable ! */
	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		if (!stable)
			return 0;
	}

	if (hwc_fps > lcm_fps)
		hwc_fps = lcm_fps;

	if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE)
		disp_handle = pgc->ovl2mem_path_handle;
	else
		disp_handle = pgc->dpmgr_handle;

	data_config = dpmgr_path_get_last_config(disp_handle);

	/* calc wdma/rdma data size */
	rdma_sz = (unsigned long long)data_config->dst_h *
					data_config->dst_w * 3;

	/* calc ovl data size */
	ovl_sz = 0;
	for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++) {
		struct OVL_CONFIG_STRUCT *ovl_cfg =
			&(data_config->ovl_config[i]);

		if (ovl_cfg->layer_en) {
			unsigned int Bpp = UFMT_GET_Bpp(ovl_cfg->fmt);

			ovl_sz += (unsigned long long)ovl_cfg->dst_w *
						ovl_cfg->dst_h * Bpp;
		}
	}

	/* switch criteria is:  (DL) V.S. (DC):
	 * (ovl*lcm_fps) V.S. (ovl*hwc_fps + wdma*hwc_fps + rdma*lcm_fps)
	 */
	DL_bw = ovl_sz * lcm_fps;
	DC_bw = (ovl_sz + rdma_sz) * hwc_fps + rdma_sz * lcm_fps;

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		bw_th = DL_bw * 4;
		do_div(bw_th, 5);
		if (DC_bw < bw_th) {
			/* switch to DC */
			do_primary_display_switch_mode(
				DISP_SESSION_DECOUPLE_MODE,
				pgc->session_id, 0, NULL, 0);
		}
	} else {
		bw_th = DC_bw * 4;
		do_div(bw_th, 5);
		if (DL_bw < bw_th) {
			/* switch to DL */
			do_primary_display_switch_mode(
				DISP_SESSION_DIRECT_LINK_MODE,
				pgc->session_id, 0, NULL, 0);
		}
	}

	return 0;
}

int primary_display_is_alive(void)
{
	unsigned int temp = 0;

	/* DISPFUNC(); */
	_primary_path_lock(__func__);

	if (pgc->state == DISP_ALIVE)
		temp = 1;

	_primary_path_unlock(__func__);

	return temp;
}

int primary_display_is_sleepd(void)
{
	unsigned int temp = 0;

	/* DISPFUNC(); */
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT)
		temp = 1;

	_primary_path_unlock(__func__);

	return temp;
}

int primary_display_get_width(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->width;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_height(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->height;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_virtual_width(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->virtual_width;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_virtual_height(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->virtual_height;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_original_width(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->lcm_original_width;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_original_height(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->lcm_original_height;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_bpp(void)
{
	return 32;
}

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
int primary_display_get_corner_full_content(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->full_content;

	DISPERR("lcm_params is null!\n");
	return 0;
}
int primary_display_get_lcm_corner_en(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->round_corner_en;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_corner_pattern_width(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->corner_pattern_width;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_corner_pattern_height(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->corner_pattern_height;

	DISPERR("lcm_params is null!\n");
	return 0;
}

int primary_display_get_corner_pattern_height_bot(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->corner_pattern_height_bot;

	DISPERR("lcm_params is null!\n");
	return 0;
}
#endif

void primary_display_set_max_layer(int maxlayer)
{
	pgc->max_layer = maxlayer;
}

int primary_display_get_max_layer(void)
{
	return pgc->max_layer;
}

int primary_display_get_info(struct disp_session_info *info)
{
	struct disp_session_info *dispif_info =
		(struct disp_session_info *)info;
	struct LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);

	if (lcm_param == NULL) {
		DISPERR("lcm_param is null\n");
		return -1;
	}

	memset((void *)dispif_info, 0, sizeof(struct disp_session_info));

	if (is_DAL_Enabled())
		dispif_info->maxLayerNum = pgc->max_layer - 1;
	else
		dispif_info->maxLayerNum = pgc->max_layer;

	dispif_info->const_layer_num = 0;

	switch (lcm_param->type) {
	case LCM_TYPE_DBI:
	{
		dispif_info->displayType = DISP_IF_TYPE_DBI;
		dispif_info->displayMode = DISP_IF_MODE_COMMAND;
		dispif_info->isHwVsyncAvailable = 1;
		break;
	}
	case LCM_TYPE_DPI:
	{
		dispif_info->displayType = DISP_IF_TYPE_DPI;
		dispif_info->displayMode = DISP_IF_MODE_VIDEO;
		dispif_info->isHwVsyncAvailable = 1;
		break;
	}
	case LCM_TYPE_DSI:
	{
		dispif_info->displayType = DISP_IF_TYPE_DSI0;
		if (lcm_param->dsi.mode == CMD_MODE) {
			dispif_info->displayMode = DISP_IF_MODE_COMMAND;
			dispif_info->isHwVsyncAvailable = 1;
		} else {
			dispif_info->displayMode = DISP_IF_MODE_VIDEO;
			dispif_info->isHwVsyncAvailable = 1;
		}

		break;
	}
	default:
		break;
	}


	dispif_info->displayFormat = DISP_IF_FORMAT_RGB888;

	dispif_info->displayWidth = primary_display_get_width();
	dispif_info->displayHeight = primary_display_get_height();

	dispif_info->physicalWidth = DISP_GetActiveWidth();
	dispif_info->physicalHeight = DISP_GetActiveHeight();
	dispif_info->physicalWidthUm = DISP_GetActiveWidthUm();
	dispif_info->physicalHeightUm = DISP_GetActiveHeightUm();
	dispif_info->density = DISP_GetDensity();

	dispif_info->vsyncFPS = pgc->lcm_fps;

	dispif_info->isConnected = 1;

	if (disp_helper_get_option(DISP_OPT_FPS_EXT))
		fps_ext_ctx_get_fps(&primary_fps_ext_ctx,
			&dispif_info->updateFPS,
			&dispif_info->is_updateFPS_stable);
	else
		fps_ctx_get_fps(&primary_fps_ctx, &dispif_info->updateFPS,
			&dispif_info->is_updateFPS_stable);

	dispif_info->updateFPS *= 100;

	return 0;
}

int primary_display_get_pages(void)
{
	return 3;
}

int primary_display_is_video_mode(void)
{
	/*
	 * TODO: we should store the video/cmd mode in runtime because ROME
	 * will support cmd/vdo dynamic switch
	 */
	return disp_lcm_is_video_mode(pgc->plcm);
}

int primary_display_diagnose(const char *func, int line)
{
	int ret = 0;

	DISPMSG("==== %s of func:%s, line:%d===>\n", __func__, func, line);
	/* confirm mm clk*/
	/* check_mm0_clk_sts(); */
	dpmgr_check_status(pgc->dpmgr_handle);

	if (primary_display_is_decouple_mode()) {
		/* to prevent race condition with DC->DL switch */
		dpmgr_check_status_by_scenario(DDP_SCENARIO_PRIMARY_OVL_MEMOUT);
	}
	DISPMSG("==== %s ===<\n", __func__);
	return ret;
}

enum CMDQ_SWITCH primary_display_cmdq_enabled(void)
{
	return disp_helper_get_option(DISP_OPT_USE_CMDQ);
}

int primary_display_manual_lock(void)
{
	_primary_path_lock(__func__);

	return 0;
}

int primary_display_manual_unlock(void)
{
	_primary_path_unlock(__func__);

	return 0;
}

void primary_display_reset(void)
{
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
}

unsigned int primary_display_get_fps_nolock(void)
{
	return pgc->lcm_fps;
}

unsigned int primary_display_get_fps(void)
{
	unsigned int fps = 0;

	_primary_path_lock(__func__);
	fps = pgc->lcm_fps;
	_primary_path_unlock(__func__);

	return fps;
}

int primary_display_force_set_fps(unsigned int keep, unsigned int skip)
{
	int ret = 0;

	DISPMSG("force set fps to keep %d, skip %d\n", keep, skip);
	_primary_path_lock(__func__);

	pgc->force_fps_keep_count = keep;
	pgc->force_fps_skip_count = skip;

	g_keep = 0;
	g_skip = 0;
	_primary_path_unlock(__func__);

	return ret;
}

unsigned int primary_display_force_get_vsync_fps(void)
{
	if (primary_display_is_idle()) {
		DISPMSG(
			"primary_display_force_get_vsync_fps=50, currently it is idle\n");
		if (pgc->plcm->params->min_refresh_rate != 0)
			return pgc->plcm->params->min_refresh_rate;
	} else if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
		DISPMSG(
			"primary_display_force_get_vsync_fps=%d, not idle and support ARR\n",
			pgc->dynamic_fps);
		return pgc->dynamic_fps;
	}

	DISPMSG(
		"primary_display_force_get_vsync_fps=%d, not idle and not support ARR\n",
		60);
	return 60;
}

int primary_display_force_set_vsync_fps(unsigned int fps, unsigned int scenario)
{
	int ret = 0;

	if (!primary_display_is_video_mode()) {
		DISPWARN(
			"currently display is not video mode. fps changing fail\n");
		return -1;
	}

	DISPMSG(
		"%s scenario=%d, fps=%d\n", __func__,
		scenario, fps);

	/* scenario is APP, and support ARR change fps */
	if ((scenario == 0) && disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
		_primary_path_lock(__func__);
		/* record dynamic fps */
		pgc->dynamic_fps = fps;

		/* backup ARR fps */
		arr_fps_backup = fps;

		/* mark arr fps enable or disable */
		if ((fps < primary_display_get_min_refresh_rate()) &&
			(fps > primary_display_get_max_refresh_rate())) {
			DISPMSG("mark ARR fps enable\n");
			arr_fps_enable = 1;
		} else if (fps == 60) {
			DISPMSG("mark ARR fps disable\n");
			arr_fps_enable = 0;
		}

		/* if need to change ARR fps?
		 * idle status, no need to change ARR fps,
		 * ecause idle fps is lowest.
		 * after leave idle, dynamic_fps_changed will be
		 * set 1 to backup ARR fps.
		 */
		if (primary_display_is_idle()) {
			DISPMSG("it is idle, won't change dynamic fps\n");
			dynamic_fps_changed = 0;
		} else {
			DISPMSG("it is not idle, will change dynamic fps=%d\n",
				pgc->dynamic_fps);
			dynamic_fps_changed = 1;
		}
		_primary_path_unlock(__func__);
	}

	/* scenario is "enter idle", change fps to be input idle fps */
	if (scenario == 1) {
		/* record dynamic fps */
		pgc->dynamic_fps = fps;
		dynamic_fps_changed = 1;
		DISPMSG(
			"enter idle, change dynamic fps to be %d of idle status\n",
			pgc->dynamic_fps);
	}

	/* scenario is "leave idle", change fps to be ARR fps */
	if (scenario == 2) {
		if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1) &&
			(arr_fps_enable == 1)) {
			/* support ARR, leave idle will change to
			 * ARR_fps_backup
			 */
			pgc->dynamic_fps = arr_fps_backup;
			DISPMSG(
				"leave idle and support ARR, change dynamic fps=%d\n",
				pgc->dynamic_fps);
		} else {
			/* not support ARR, leave idle will change fps to
			 * be 60
			 */
			pgc->dynamic_fps = fps;
			DISPMSG(
				"leave idle and does not support ARR, change dynamic fps=%d\n",
				pgc->dynamic_fps);
		}
		dynamic_fps_changed = 1;
	}

	DISPMSG("%s done\n", __func__);

	return ret;
}

int primary_display_vsync_switch(int method)
{
	int ret = 0;

	if (method == 0) {
		pr_debug("Vsync map RDMA %d\n", method);
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	} else if (method == 1) {
		pr_debug("Vsync map DSI TE %d\n", method);
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_EXT_TE);
	} else if (method == 2) {
		pr_debug("Vsync map DSI FRAME DONE %d\n", method);
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_FRAME_DONE);
	}

	return ret;
}

int _set_backlight_by_cmdq(unsigned int level)
{
	int ret = 0;
	struct cmdqRecStruct *cmdq_handle_backlight = NULL;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_PULSE, 1, 1);
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle_backlight);
	DISPDBG("primary backlight, handle=%p\n", cmdq_handle_backlight);
	if (ret) {
		DISPWARN("fail to create primary cmdq handle for backlight\n");
		return -1;
	}

	if (primary_display_is_video_mode()) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
				MMPROFILE_FLAG_PULSE, 1, 2);
		cmdqRecReset(cmdq_handle_backlight);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_backlight);
		disp_lcm_set_backlight(pgc->plcm, cmdq_handle_backlight, level);
		/*Async flush by cmdq*/
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 0);
		DISPMSG("[BL]%s ret=%d\n", __func__, ret);
	} else {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 1, 3);
		cmdqRecReset(cmdq_handle_backlight);
		cmdqRecWait(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		_cmdq_handle_clear_dirty(cmdq_handle_backlight);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_backlight);
		disp_lcm_set_backlight(pgc->plcm, cmdq_handle_backlight, level);
		cmdqRecSetEventToken(cmdq_handle_backlight,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		cmdqRecSetEventToken(cmdq_handle_backlight,
			CMDQ_SYNC_TOKEN_CABC_EOF);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 1, 4);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 1, 6);
		DISPMSG("[BL]%s ret=%d\n", __func__, ret);
	}
	cmdqRecDestroy(cmdq_handle_backlight);
	cmdq_handle_backlight = NULL;
	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_PULSE, 1, 5);

	return ret;
}

int _set_backlight_by_cpu(unsigned int level)
{
	int ret = 0;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		DISPMSG("%s skip due to stage %s\n",
			__func__, disp_helper_stage_spy());
		return 0;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_PULSE, 0, 1);
	if (primary_display_is_video_mode()) {
		disp_lcm_set_backlight(pgc->plcm, NULL, level);
	} else {
		DISPCHECK("[BL]display cmdq trigger loop stop[begin]\n");
		if (primary_display_cmdq_enabled())
			_cmdq_stop_trigger_loop();

		DISPCHECK("[BL]display cmdq trigger loop stop[end]\n");

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK("[BL]primary display path is busy\n");
			ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[BL]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]stop dpmgr path[end]\n");
		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK(
				"[BL]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}
		DISPCHECK("[BL]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]reset display path[end]\n");

		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 0, 2);

		disp_lcm_set_backlight(pgc->plcm, NULL, level);

		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 0, 3);

		DISPCHECK("[BL]start dpmgr path[begin]\n");
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]start dpmgr path[end]\n");

		if (primary_display_cmdq_enabled()) {
			DISPCHECK("[BL]start cmdq trigger loop[begin]\n");
			_cmdq_start_trigger_loop();
		}
		DISPCHECK("[BL]start cmdq trigger loop[end]\n");
	}
	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_PULSE, 0, 7);
	return ret;
}

int primary_display_setbacklight(unsigned int level)
{
	int ret = 0;
	static unsigned int last_level;
	bool aal_is_support = disp_aal_is_support();

	DISPFUNC();
	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		DISPMSG("%s skip due to stage %s\n", __func__,
			disp_helper_stage_spy());
		return 0;
	}

	if (last_level == level)
		return 0;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_START, 0, 0);

	if (aal_is_support == false) {
		_primary_path_switch_dst_lock();

		_primary_path_lock(__func__);
	}

	if (pgc->state == DISP_SLEPT) {
		DISPWARN("Sleep State set backlight invalid\n");
	} else {
		primary_display_idlemgr_kick(__func__, 0);
		if (primary_display_cmdq_enabled()) {
			if (primary_display_is_video_mode()) {
				mmprofile_log_ex(
					ddp_mmp_get_events()->primary_set_bl,
					MMPROFILE_FLAG_PULSE, 0, 7);
				_set_backlight_by_cmdq(level);
			} else {
				_set_backlight_by_cmdq(level);
			}
			atomic_set(&delayed_trigger_kick, 1);
		} else {
			_set_backlight_by_cpu(level);
		}
		last_level = level;
	}

	if (aal_is_support == false) {
		_primary_path_unlock(__func__);

		_primary_path_switch_dst_unlock();
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
		MMPROFILE_FLAG_END, 0, 0);
	return ret;
}

int _set_lcm_cmd_by_cmdq(unsigned int *lcm_cmd, unsigned int *lcm_count,
	unsigned int *lcm_value)
{
	int ret = 0;
	struct cmdqRecStruct *cmdq_handle_lcm_cmd = NULL;

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
		MMPROFILE_FLAG_PULSE, 1, 1);
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle_lcm_cmd);
	DISPDBG("primary set lcm cmd, handle=%p\n", cmdq_handle_lcm_cmd);
	if (ret) {
		DISPWARN("fail to create primary cmdq handle for setlcmcmd\n");
		return -1;
	}

	if (primary_display_is_video_mode()) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
			MMPROFILE_FLAG_PULSE, 1, 2);
		cmdqRecReset(cmdq_handle_lcm_cmd);
		disp_lcm_set_lcm_cmd(pgc->plcm, cmdq_handle_lcm_cmd, lcm_cmd,
			lcm_count, lcm_value);
		_cmdq_flush_config_handle_mira(cmdq_handle_lcm_cmd, 1);
		DISPCHECK("[CMD]_set_lcm_cmd_by_cmdq ret=%d\n", ret);
	} else {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_bl,
			MMPROFILE_FLAG_PULSE, 1, 3);
		cmdqRecReset(cmdq_handle_lcm_cmd);
		_cmdq_handle_clear_dirty(cmdq_handle_lcm_cmd);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_lcm_cmd);

		disp_lcm_set_lcm_cmd(pgc->plcm, cmdq_handle_lcm_cmd, lcm_cmd,
			lcm_count, lcm_value);
		cmdqRecSetEventToken(cmdq_handle_lcm_cmd,
			CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
			MMPROFILE_FLAG_PULSE, 1, 4);
		_cmdq_flush_config_handle_mira(cmdq_handle_lcm_cmd, 1);
		mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
			MMPROFILE_FLAG_PULSE, 1, 6);
		DISPCHECK("[CMD]_set_lcm_cmd_by_cmdq ret=%d\n", ret);
	}
	cmdqRecDestroy(cmdq_handle_lcm_cmd);
	cmdq_handle_lcm_cmd = NULL;
	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
		MMPROFILE_FLAG_PULSE, 1, 5);
	return ret;
}

int primary_display_setlcm_cmd(unsigned int *lcm_cmd, unsigned int *lcm_count,
	unsigned int *lcm_value)
{
	int ret = 0;

	DISPFUNC();
	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		DISPMSG("%s skip due to stage %s\n",
			__func__, disp_helper_stage_spy());
		return 0;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
		MMPROFILE_FLAG_START, 0, 0);

	_primary_path_switch_dst_lock();
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State set backlight invalid\n");
	} else {
		if (primary_display_cmdq_enabled()) {
			if (primary_display_is_video_mode()) {
				mmprofile_log_ex(
					ddp_mmp_get_events()->primary_set_cmd,
					MMPROFILE_FLAG_PULSE, 0, 7);
				_set_lcm_cmd_by_cmdq(lcm_cmd, lcm_count,
					lcm_value);
			} else {
				_set_lcm_cmd_by_cmdq(lcm_cmd, lcm_count,
					lcm_value);
			}
		} else {
			/* cpu */
		}
	}

	_primary_path_unlock(__func__);
	_primary_path_switch_dst_lock();

	mmprofile_log_ex(ddp_mmp_get_events()->primary_set_cmd,
		MMPROFILE_FLAG_END, 0, 0);

	return ret;
}

int primary_display_mipi_clk_change(unsigned int clk_value)
{
	struct cmdqRecStruct *cmdq_handle = NULL;

	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State clk change invalid\n");
		return 0;
	}

	_primary_path_lock(__func__);

	if (!primary_display_is_video_mode()) {
		DISPCHECK("clk change CMD Mode return\n");
		return 0;
	}

	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
	cmdqRecReset(cmdq_handle);

	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
	pgc->plcm->params->dsi.PLL_CLOCK = clk_value;

	dpmgr_path_build_cmdq(pgc->dpmgr_handle, cmdq_handle,
			      CMDQ_STOP_VDO_MODE, 0);

	dpmgr_path_ioctl(primary_get_dpmgr_handle(), cmdq_handle,
			 DDP_PHY_CLK_CHANGE, &clk_value);

	dpmgr_path_build_cmdq(pgc->dpmgr_handle, cmdq_handle,
			      CMDQ_START_VDO_MODE, 0);

	cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DISP_RDMA0_EOF);

	dpmgr_path_trigger(pgc->dpmgr_handle, cmdq_handle, CMDQ_ENABLE);
	ddp_mutex_set_sof_wait(dpmgr_path_get_mutex(pgc->dpmgr_handle),
			       pgc->cmdq_handle_config_esd, 0);
	_cmdq_flush_config_handle_mira(cmdq_handle, 1);

	cmdqRecDestroy(cmdq_handle);
	cmdq_handle = NULL;

	DISPCHECK("%s return\n", __func__);

	_primary_path_unlock(__func__);

	return 0;
}

/********************** Legacy DISP API ****************************/
UINT32 DISP_GetScreenWidth(void)
{
	return primary_display_get_width();
}

UINT32 DISP_GetScreenHeight(void)
{
	return primary_display_get_height();
}

UINT32 DISP_GetActiveHeight(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->physical_height;

	DISPERR("lcm_params is null!\n");
	return 0;
}

UINT32 DISP_GetActiveWidth(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->physical_width;

	DISPERR("lcm_params is null!\n");
	return 0;
}

uint32_t DISP_GetActiveHeightUm(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->physical_height_um;

	DISPERR("lcm_params is null!\n");
	return 0;
}

uint32_t DISP_GetActiveWidthUm(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->physical_width_um;

	DISPERR("lcm_params is null!\n");
	return 0;
}

uint32_t DISP_GetDensity(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->density;

	DISPERR("lcm_params is null!\n");
	return 0;
}

struct LCM_PARAMS *DISP_GetLcmPara(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params;

	return NULL;
}

struct LCM_DRIVER *DISP_GetLcmDrv(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if (pgc->plcm->drv)
		return pgc->plcm->drv;

	return NULL;
}

#if defined(CONFIG_MTK_M4U) || \
	defined(CONFIG_MTK_IOMMU_V2)
static int _screen_cap_by_cmdq(unsigned int mva, enum UNIFIED_COLOR_FMT ufmt,
			       enum DISP_MODULE_ENUM after_eng)
{
	int ret = 0;
	struct cmdqRecStruct *cmdq_handle = NULL;
	struct cmdqRecStruct *cmdq_wait_handle = NULL;
	struct disp_ddp_path_config *pconfig = NULL;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();

	/* create config thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
	if (ret) {
		DISPCHECK(
			"primary capture:Fail to create primary cmdq handle for capture\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_handle);

	/* create wait thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,
		&cmdq_wait_handle);
	if (ret) {
		DISPCHECK(
			"primary capture:Fail to create primary cmdq wait handle for capture\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_wait_handle);

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 1);

	_cmdq_handle_clear_dirty(cmdq_handle);
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	_primary_path_lock(__func__);

	primary_display_idlemgr_kick(__func__, 0);
	dpmgr_path_add_memout(pgc->dpmgr_handle, after_eng, cmdq_handle);
	cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->wdma_dirty = 1;
	pconfig->ovl_dirty = 1;
	pconfig->dst_dirty = 1;
	pconfig->rdma_dirty = 1;
	pconfig->wdma_config.dstAddress = mva;
	pconfig->wdma_config.srcHeight = h_yres;
	pconfig->wdma_config.srcWidth = w_xres;
	pconfig->wdma_config.clipX = 0;
	pconfig->wdma_config.clipY = 0;
	pconfig->wdma_config.clipHeight = h_yres;
	pconfig->wdma_config.clipWidth = w_xres;
	pconfig->wdma_config.outputFormat = ufmt;
	pconfig->wdma_config.useSpecifiedAlpha = 1;
	pconfig->wdma_config.alpha = 0xFF;
	pconfig->wdma_config.dstPitch = w_xres * UFMT_GET_bpp(ufmt) / 8;
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
	pconfig->wdma_dirty = 0;

	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DISPMSG("primary capture:Flush add memout mva(0x%x)\n", mva);
	/* wait wdma0 sof */
	cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
	cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
	cmdqRecFlush(cmdq_wait_handle);
	DISPMSG("primary capture:Flush wait wdma sof\n");
	cmdqRecReset(cmdq_handle);
	_cmdq_handle_clear_dirty(cmdq_handle);
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);

	cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	/* flush remove memory to cmdq */
	cmdqRecFlushAsyncCallback(cmdq_handle, _remove_memout_callback, 0);
	DISPMSG("primary capture: Flush remove memout\n");

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
	_primary_path_unlock(__func__);

out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);
	return 0;
}

static int _screen_cap_by_cpu(unsigned int mva, enum UNIFIED_COLOR_FMT ufmt,
	enum DISP_MODULE_ENUM after_eng)
{
	int ret = 0;
	struct disp_ddp_path_config *pconfig = NULL;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 1);

	if (_should_wait_path_idle()) {
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		if (ret <= 0)
			primary_display_diagnose(__func__, __LINE__);
	}

	_primary_path_lock(__func__);
	primary_display_idlemgr_kick(__func__, 1);

	dpmgr_path_add_memout(pgc->dpmgr_handle, after_eng, NULL);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->wdma_dirty = 1;
	pconfig->wdma_config.dstAddress = mva;
	pconfig->wdma_config.srcHeight = h_yres;
	pconfig->wdma_config.srcWidth = w_xres;
	pconfig->wdma_config.clipX = 0;
	pconfig->wdma_config.clipY = 0;
	pconfig->wdma_config.clipHeight = h_yres;
	pconfig->wdma_config.clipWidth = w_xres;
	pconfig->wdma_config.outputFormat = ufmt;
	pconfig->wdma_config.useSpecifiedAlpha = 1;
	pconfig->wdma_config.alpha = 0xFF;
	pconfig->wdma_config.dstPitch = w_xres * UFMT_GET_bpp(ufmt) / 8;
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, NULL);
	pconfig->wdma_dirty = 0;

	_trigger_display_interface(1, NULL, 0);
	msleep(20);
	if (_should_wait_path_idle()) {
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		if (ret <= 0)
			primary_display_diagnose(__func__, __LINE__);
	}

	dpmgr_path_remove_memout(pgc->dpmgr_handle, NULL);

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
	_primary_path_unlock(__func__);
	return 0;
}
#endif

#ifdef CONFIG_MTK_IOMMU_V2
int primary_display_capture_framebuffer_ovl(unsigned long pbuf,
	enum UNIFIED_COLOR_FMT ufmt)
{
	int ret = 0;
	struct ion_client *ion_display_client = NULL;
	struct ion_handle *ion_display_handle = NULL;
	unsigned long mva = 0;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_byte = primary_display_get_bpp() / 8;
	int buffer_size = h_yres * w_xres * pixel_byte;
	enum DISP_MODULE_ENUM after_eng = DISP_MODULE_OVL0;
	int tmp;

	DISPMSG("primary capture: begin\n");

	disp_sw_mutex_lock(&(pgc->capture_lock));

	if (primary_display_is_sleepd()) {
		memset((void *)pbuf, 0, buffer_size);
		DISPMSG("primary capture: Fail black End\n");
		goto out;
	}

	ion_display_client = disp_ion_create("disp_cap_ovl");
	if (ion_display_client == NULL) {
		DISPMSG("primary capture:Fail to create ion\n");
		ret = -1;
		goto out;
	}

	ion_display_handle = disp_ion_alloc(ion_display_client,
					    ION_HEAP_MULTIMEDIA_MAP_MVA_MASK,
					    pbuf, buffer_size);
	if (!ion_display_handle) {
		DISPMSG("primary capture:Fail to allocate buffer\n");
		ret = -1;
		goto out;
	}

	disp_ion_get_mva(ion_display_client, ion_display_handle,
		&mva, 0, DISP_M4U_PORT_DISP_WDMA0);
	disp_ion_cache_flush(ion_display_client, ion_display_handle,
		ION_CACHE_INVALID_BY_RANGE);

	tmp = disp_helper_get_option(DISP_OPT_SCREEN_CAP_FROM_DITHER);
	if (tmp == 0)
		after_eng = DISP_MODULE_OVL0;

	if (primary_display_cmdq_enabled())
		_screen_cap_by_cmdq((unsigned int)mva, ufmt, after_eng);
	else
		_screen_cap_by_cpu((unsigned int)mva, ufmt, after_eng);

	disp_ion_cache_flush(ion_display_client, ion_display_handle,
		ION_CACHE_INVALID_BY_RANGE);

out:
	if (ion_display_client)
		disp_ion_free_handle(ion_display_client, ion_display_handle);

	if (ion_display_client)
		disp_ion_destroy(ion_display_client);

	disp_sw_mutex_unlock(&(pgc->capture_lock));
	DISPMSG("primary capture: end\n");
	return ret;
}

#else

int primary_display_capture_framebuffer_ovl(unsigned long pbuf,
	enum UNIFIED_COLOR_FMT ufmt)
{
	int ret = 0;
#ifdef CONFIG_MTK_M4U
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_byte = primary_display_get_bpp() / 8;
	int buffer_size = h_yres * w_xres * pixel_byte;
	enum DISP_MODULE_ENUM after_eng = DISP_MODULE_OVL0;
	int tmp;
	struct m4u_client_t *m4uClient = NULL;
	unsigned int mva = 0;
#endif
	DISPMSG("primary capture: begin\n");

	disp_sw_mutex_lock(&(pgc->capture_lock));

#ifdef CONFIG_MTK_M4U
	if (primary_display_is_sleepd()) {
		memset((void *)pbuf, 0, buffer_size);
		DISPMSG("primary capture: Fail black End\n");
		goto out;
	}

	m4uClient = m4u_create_client();
	if (m4uClient == NULL) {
		DISPMSG("primary capture:Fail to alloc  m4uClient\n");
		ret = -1;
		goto out;
	}

	ret = m4u_alloc_mva(m4uClient, DISP_M4U_PORT_DISP_WDMA0, pbuf, NULL,
		buffer_size, M4U_PROT_READ | M4U_PROT_WRITE, 0, &mva);
	if (ret) {
		DISPMSG("primary capture:Fail to allocate mva\n");
		ret = -1;
		goto out;
	}

	ret = m4u_cache_sync(m4uClient, DISP_M4U_PORT_DISP_WDMA0, pbuf,
			     buffer_size, mva, M4U_CACHE_FLUSH_ALL);
	if (ret) {
		DISPMSG("primary capture:Fail to cach sync\n");
		ret = -1;
		goto out;
	}

	tmp = disp_helper_get_option(DISP_OPT_SCREEN_CAP_FROM_DITHER);
	if (tmp == 0)
		after_eng = DISP_MODULE_OVL0;

	if (primary_display_cmdq_enabled())
		_screen_cap_by_cmdq(mva, ufmt, after_eng);
	else
		_screen_cap_by_cpu(mva, ufmt, after_eng);

	ret = m4u_cache_sync(m4uClient, DISP_M4U_PORT_DISP_WDMA0, pbuf,
			     buffer_size, mva, M4U_CACHE_INVALID_BY_RANGE);

out:
	if (mva > 0)
		m4u_dealloc_mva(m4uClient, DISP_M4U_PORT_DISP_WDMA0, mva);

	if (m4uClient != 0)
		m4u_destroy_client(m4uClient);
#endif
	disp_sw_mutex_unlock(&(pgc->capture_lock));
	DISPMSG("primary capture: end\n");
	return ret;
}
#endif

int primary_display_capture_framebuffer(unsigned long pbuf)
{
	unsigned int fb_layer_id = primary_display_get_option("FB_LAYER");
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	/* bpp is either 32 or 16, can not be other value */
	unsigned int pixel_bpp = primary_display_get_bpp() / 8;
	unsigned int w_fb = ALIGN_TO(w_xres, MTK_FB_ALIGNMENT);
	/* frame buffer size */
	unsigned int fbsize = w_fb * h_yres * pixel_bpp;
	struct disp_ddp_path_config *path_config =
		dpmgr_path_get_last_config(pgc->dpmgr_handle);
	unsigned long fbaddress = path_config->ovl_config[fb_layer_id].addr;
	void *fbv = 0;
	unsigned int i;
	unsigned long ttt;

	DISPMSG(
		"w_res=%d, h_yres=%d, pixel_bpp=%d, w_fb=%d, fbsize=%d, fbaddress=0x%lx\n",
		w_xres, h_yres, pixel_bpp, w_fb, fbsize, fbaddress);
	fbv = ioremap(fbaddress, fbsize);
	DISPMSG(
		"w_xres = %d, h_yres = %d, w_fb = %d, pixel_bpp = %d, fbsize = %d, fbaddress = 0x%08lx\n",
		w_xres, h_yres, w_fb, pixel_bpp, fbsize, fbaddress);
	if (!fbv) {
		DISPMSG(
			"[FB Driver], Unable to allocate memory for frame buffer: address=0x%lx, size=0x%08x\n",
			fbaddress, fbsize);
		return -1;
	}

	ttt = get_current_time_us();
	for (i = 0; i < h_yres; i++) {
		memcpy((void *)(pbuf + i * w_xres * pixel_bpp),
		       (void *)(fbv + i * w_fb * pixel_bpp),
		       w_xres * pixel_bpp);
	}
	DISPMSG("capture framebuffer cost %ld us\n",
		get_current_time_us() - ttt);
	iounmap((void *)fbv);
	return -1;
}

static UINT32 disp_fb_bpp = 32;
static UINT32 disp_fb_pages = 3;

UINT32 DISP_GetScreenBpp(void)
{
	return disp_fb_bpp;
}

UINT32 DISP_GetPages(void)
{
	return disp_fb_pages;
}

UINT32 DISP_GetFBRamSize(void)
{
	return ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) *
		ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) *
		((DISP_GetScreenBpp() + 7) >> 3) * DISP_GetPages();
}

UINT32 DISP_GetVRamSize(void)
{
#if 0
	/* Use a local static variable to cache the calculated vram size */
	static UINT32 vramSize;

	if (vramSize == 0) {
		disp_drv_init_context();

		/* get framebuffer size */
		vramSize = DISP_GetFBRamSize();

		/* get DXI working buffer size */
		vramSize += disp_if_drv->get_working_buffer_size();

		/* get assertion layer buffer size */
		vramSize += DAL_GetLayerSize();

		/* Align vramSize to 1MB */
		vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);

		DISP_LOG("%s: %u bytes\n", __func__, vramSize);
	}

	return vramSize;
#endif

	return 0;
}

UINT32 DISP_GetVRamSizeBoot(char *cmdline)
{
	unsigned int vramsize;

	vramsize = mtkfb_get_fb_size();
	if (!vramsize)
		pr_info("get fb size fail, size=0x%08x|%d\n",
			vramsize, vramsize);
	DISPCHECK("[DT]display vram size = 0x%08x|%d\n",
		vramsize, vramsize);
	return vramsize;
}

unsigned int primary_display_get_option(const char *option)
{
	if (!strcmp(option, "FB_LAYER"))
		return 0;
	if (!strcmp(option, "ASSERT_LAYER"))
		return PRIMARY_SESSION_INPUT_LAYER_COUNT - 1;
	if (!strcmp(option, "M4U_ENABLE"))
		return disp_helper_get_option(DISP_OPT_USE_M4U);

	/* ASSERT(0); */
	pr_info("%s, invalid option\n", __func__);
	return -1;
}

int primary_display_lcm_ATA(void)
{
	enum DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_switch_dst_lock();
	primary_display_esd_check_enable(0);
	_primary_path_lock(__func__);
	disp_irq_esd_cust_bycmdq(0);
	if (pgc->state == 0) {
		DISPCHECK(
			"ATA_LCM, primary display path is already sleep, skip\n");
		goto done;
	}

	DISPCHECK("dxs [ATA_LCM]primary display path stop[begin]\n");
	if (primary_display_is_video_mode())
		dpmgr_path_ioctl(pgc->dpmgr_handle, NULL,
			DDP_STOP_VIDEO_MODE, NULL);

	DISPCHECK("[ATA_LCM]primary display path stop[end]\n");
	ret = disp_lcm_ATA(pgc->plcm);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/*
		 * for video mode, we need to force trigger here
		 * for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when
		 * trigger loop start
		 */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}

done:
	disp_irq_esd_cust_bycmdq(1);
	_primary_path_unlock(__func__);
	primary_display_esd_check_enable(1);
	_primary_path_switch_dst_unlock();
	return ret;
}

static int Panel_Master_primary_display_config_dsi(const char *name,
	UINT32 config_value)
{
	int ret = 0;
	struct disp_ddp_path_config *data_config;

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	/* modify below for config dsi */
	if (!strcmp(name, "PM_CLK")) {
		pr_debug("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		data_config->dispif_config.dsi.PLL_CLOCK = config_value;
	} else if (!strcmp(name, "PM_SSC")) {
		data_config->dispif_config.dsi.ssc_range = config_value;
	}
	pr_debug("Pmaster_config_dsi: will Run path_config()\n");
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);

	return ret;
}

int fbconfig_get_esd_check_test(UINT32 dsi_id, UINT32 cmd,
	UINT8 *buffer, UINT32 num)
{
	int ret = 0;

	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK(
			"[ESD]primary display path is slept?? -- skip esd check\n");
		_primary_path_unlock(__func__);
		goto done;
	}
	mmprofile_log_ex(ddp_mmp_get_events()->esd_check_t,
		MMPROFILE_FLAG_START, cmd, -1);

	primary_display_idlemgr_kick(__func__, 0);

	_blocking_flush();

	primary_display_esd_check_enable(0);
	disp_irq_esd_cust_bycmdq(0);
	/* 1: stop path */
	_cmdq_stop_trigger_loop();
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	ret = fbconfig_get_esd_check(dsi_id, cmd, buffer, num);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if (primary_display_is_video_mode()) {
		/*
		 * for video mode, we need to force trigger here
		 * for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when
		 * trigger loop start
		 */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);

	}
	_cmdq_start_trigger_loop();
	/*
	 * when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend
	 */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	disp_irq_esd_cust_bycmdq(1);
	primary_display_esd_check_enable(1);

	mmprofile_log_ex(ddp_mmp_get_events()->esd_check_t, MMPROFILE_FLAG_END,
		buffer[0], -1);
	_primary_path_unlock(__func__);

done:
	return ret;
}

int Panel_Master_dsi_config_entry(const char *name, void *config_value)
{
	int ret = 0;
	int force_trigger_path = 0;
	UINT32 *config_dsi = (UINT32 *)config_value;
	struct LCM_PARAMS *lcm_param = NULL;
	struct LCM_DRIVER *pLcm_drv = DISP_GetLcmDrv();

	DISPFUNC();
	if (!strcmp(name, "DRIVER_IC_RESET") ||
		!strcmp(name, "PM_DDIC_CONFIG")) {
		primary_display_esd_check_enable(0);
		msleep(2500);
	}

	_primary_path_lock(__func__);

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK(
			"[Pmaster]Panel_Master: primary display path is slept??\n");
		goto done;
	}

	/*
	 * Esd Check : Read from lcm
	 * the following code is to
	 * 0: lock path
	 * 1: stop path
	 * 2: do esd check (!!!)
	 * 3: start path
	 * 4: unlock path
	 * 1: stop path
	 */
	 primary_display_idlemgr_kick(__func__, 0);
	_blocking_flush();
	/* blocking flush before stop trigger loop */
	_cmdq_stop_trigger_loop();

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		int event_ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		if (event_ret <= 0) {
			DISPCHECK("wait frame done in suspend timeout\n");
			primary_display_diagnose(__func__, __LINE__);
			ret = -1;
		}
	}
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	/*after dsi_stop, we should enable the DSI_TE/CMD_DONE.*/
	dsi_basic_irq_enable(DISP_MODULE_DSI0, NULL);
	if ((!strcmp(name, "PM_CLK")) || (!strcmp(name, "PM_SSC")))
		Panel_Master_primary_display_config_dsi(name, *config_dsi);
	else if (!strcmp(name, "PM_DDIC_CONFIG")) {
		Panel_Master_DDIC_config();
		force_trigger_path = 1;
	} else if (!strcmp(name, "DRIVER_IC_RESET")) {
		if (pLcm_drv && pLcm_drv->init_power)
			pLcm_drv->init_power();
		if (pLcm_drv)
			pLcm_drv->init();
		else
			ret = -1;
		force_trigger_path = 1;
	}
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/*
		 * for video mode, we need to force trigger here
		 * for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when
		 * trigger loop start
		 */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);

		force_trigger_path = 0;
	}
	_cmdq_start_trigger_loop();

	/*
	 * when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend
	 */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);

	DISPCHECK("[Pmaster]start cmdq trigger loop\n");

done:
	_primary_path_unlock(__func__);

	if (force_trigger_path) {
		primary_display_trigger(0, NULL, 0);
		DISPCHECK("[Pmaster]force trigger display path\r\n");
	}

	if (!strcmp(name, "DRIVER_IC_RESET") || !strcmp(name, "PM_DDIC_CONFIG"))
		primary_display_esd_check_enable(1);

	return ret;
}

/* mode: 0, switch to cmd mode; 1, switch to vdo mode */
int primary_display_switch_dst_mode(int mode)
{
	enum DISP_STATUS ret = DISP_STATUS_ERROR;
	disp_path_handle disp_handle = NULL;
	struct disp_ddp_path_config *pconfig = NULL;
	void *lcm_cmd = NULL;
	int temp_mode = 0;

	if (!disp_helper_get_option(DISP_OPT_CV_BYSUSPEND))
		return 0;

	DISPFUNC();
	_primary_path_switch_dst_lock();
	disp_sw_mutex_lock(&(pgc->capture_lock));
	_primary_path_lock(__func__);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
		MMPROFILE_FLAG_START, primary_display_cur_dst_mode, mode);
	DISPMSG("[C2V]cur_mode:%d, dst_mode:%d\n",
		primary_display_cur_dst_mode, mode);

	if (pgc->plcm->params->type != LCM_TYPE_DSI) {
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_display_switch_dst_mode,
			MMPROFILE_FLAG_PULSE, 5, pgc->plcm->params->type);
		DISPCHECK("[C2V]dst mode switch only support DSI IF\n");
		goto done;
	}
	if (pgc->state == DISP_SLEPT) {
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_display_switch_dst_mode,
			MMPROFILE_FLAG_PULSE, 6, pgc->state);
		DISPCHECK(
			"[%s] primary display path is already sleep, skip\n",
			__func__);
		goto done;
	}

	if (pgc->lcm_refresh_rate != 120) {
		DISPCHECK(
			"Only support 120HZ CV switch. But lcm_refresh_rate is:%d\n",
			pgc->lcm_refresh_rate);
		goto done;
	}

	if (mode == primary_display_cur_dst_mode) {
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_display_switch_dst_mode,
			MMPROFILE_FLAG_PULSE, 7, mode);
		DISPCHECK(
			"[%s]not need switch,cur_mode:%d, switch_mode:%d\n",
			__func__,
			primary_display_cur_dst_mode, mode);
		goto done;
	}

	/* get c2v switch lcm cmd */
	lcm_cmd = disp_lcm_switch_mode(pgc->plcm, mode);
	if (lcm_cmd == NULL) {
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_display_switch_dst_mode,
			MMPROFILE_FLAG_PULSE, 8, mode);
		DISPCHECK(
			"[%s]get lcm cmd fail primary_display_cur_dst_mode=%d mode=%d\n",
			__func__, primary_display_cur_dst_mode, mode);
		goto done;
	}

	/* When switch to vdo mode, go back to DL mode
	 * if display path change to DC mode by SMART OVL
	 */
	if (disp_helper_get_option(DISP_OPT_SMART_OVL) &&
		!primary_display_is_video_mode()) {
		/* switch to the mode before idle */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			primary_get_sess_id(), 0, NULL, 0);

		set_is_dc(0);
	}

#ifndef CONFIG_FPGA_EARLY_PORTING
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		ddp_set_spm_mode(DDP_CG_MODE, NULL);
#endif

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
			 MMPROFILE_FLAG_PULSE, 4, 0);
	_cmdq_reset_config_handle();

	/* 1. modify lcm mode - sw */
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
			 MMPROFILE_FLAG_PULSE, 4, 1);
	temp_mode = (int)(pgc->plcm->params->dsi.mode);
	pgc->plcm->params->dsi.mode = pgc->plcm->params->dsi.switch_mode;
	pgc->plcm->params->dsi.switch_mode = temp_mode;
	dpmgr_path_set_video_mode(pgc->dpmgr_handle,
		primary_display_is_video_mode());

	/* 2.Change PLL CLOCK parameter and build fps lcm command */
	disp_lcm_adjust_fps(pgc->cmdq_handle_config, pgc->plcm,
		pgc->lcm_refresh_rate);
	disp_handle = pgc->dpmgr_handle;
	pconfig = dpmgr_path_get_last_config(disp_handle);
	pconfig->dispif_config.dsi.PLL_CLOCK = pgc->plcm->params->dsi.PLL_CLOCK;

	/* 3.Change PLL LCM clock parameter */
	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_UPDATE_PLL_CLK_ONLY, &pgc->plcm->params->dsi.PLL_CLOCK);

	/* 4.Switch mode and change DSI clock */
	if (dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
		DDP_SWITCH_DSI_MODE, lcm_cmd) != 0) {
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_display_switch_dst_mode,
			MMPROFILE_FLAG_PULSE, 9, 0);
		ret = -1;
	}

	/* 5. rebuild trigger loop */
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
			 MMPROFILE_FLAG_PULSE, 4, 2);
	_cmdq_build_trigger_loop();
	_cmdq_stop_trigger_loop();
	_cmdq_start_trigger_loop();
	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_config);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
			 MMPROFILE_FLAG_PULSE, 4, 3);
	primary_display_cur_dst_mode = mode;
	DISPMSG("primary_display_cur_dst_mode %d\n",
		primary_display_cur_dst_mode);
	if (primary_display_is_video_mode())
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	else
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_EXT_TE);

	ret = DISP_STATUS_OK;
done:
	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_switch_dst_mode,
		MMPROFILE_FLAG_END, primary_display_cur_dst_mode, mode);
	_primary_path_unlock(__func__);
	disp_sw_mutex_unlock(&(pgc->capture_lock));
	_primary_path_switch_dst_unlock();
	return ret;
}

/*****************************************************************************
 * Below code is for Efuse test in Android Load.
 * include TE, ROI and Resolution.
 *****************************************************************************/
/* extern void DSI_ForceConfig(int forceconfig);	*/
/* extern int DSI_set_roi(int x,int y);			*/
/* extern int DSI_check_roi(void);			*/

static int width_array[] = {2560, 1440, 1920, 1280, 1200, 800, 960, 640};
static int heigh_array[] = {1440, 2560, 1200, 800, 1920, 1280, 640, 960};
static int array_id[] = {6,   2,   7,   4,   3,    0,   5,   1};
struct LCM_PARAMS *lcm_param2;
struct disp_ddp_path_config data_config2;

int primary_display_te_test(void)
{
	int ret = 0;
	int try_cnt = 3;
	int time_interval = 0;
	int time_interval_max = 0;
	long long time_te = 0;
	long long time_framedone = 0;

	DISPMSG("display_test te begin\n");
	if (primary_display_is_video_mode()) {
		DISPMSG("Video Mode No TE\n");
		return ret;
	}

	while (try_cnt >= 0) {
		try_cnt--;
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, HZ*1);
		time_te = sched_clock();

		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		time_framedone = sched_clock();
		time_interval = (int) (time_framedone - time_te);
		time_interval = time_interval / 1000;
		if (time_interval > time_interval_max)
			time_interval_max = time_interval;
	}

	if (time_interval_max > 20000)
		ret = 0;
	else
		ret = -1;

	if (ret >= 0)
		DISPMSG("[display_test_result]==>Force On TE Open!(%d)\n",
			time_interval_max);
	else
		DISPMSG("[display_test_result]==>Force On TE Closed!(%d)\n",
			time_interval_max);

	DISPMSG("display_test te end\n");
	return ret;
}

int primary_display_roi_test(int x, int y)
{
	int ret = 0;

	DISPMSG("display_test roi begin\n");
	DISPMSG("display_test roi set roi %d, %d\n", x, y);
	DSI_set_roi(x, y);
	msleep(50);

	dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	msleep(50);

	DISPMSG("display_test DSI_check_roi\n");
	ret = DSI_check_roi();
	msleep(20);
	if (ret == 0)
		DISPMSG("[display_test_result]==>DSI_ROI limit!\n");
	else
		DISPMSG("[display_test_result]==>DSI_ROI Normal!\n");

	DISPMSG("display_test set roi %d, %d\n", 0, 0);

	DSI_set_roi(0, 0);

	msleep(20);
	DISPCHECK("display_test end\n");
	return ret;
}

int primary_display_resolution_test(void)
{
	int ret = 0;
	int i = 0;
	unsigned int w_backup = 0;
	unsigned int h_backup = 0;
	int dst_width = 0;
	int dst_heigh = 0;
	enum LCM_DSI_MODE_CON dsi_mode_backup =
		primary_display_is_video_mode();

	memset((void *)&data_config2, 0, sizeof(data_config2));
	lcm_param2 = NULL;
	memcpy((void *)&data_config2,
		(void *)dpmgr_path_get_last_config(pgc->dpmgr_handle),
		sizeof(struct disp_ddp_path_config));
	w_backup = data_config2.dst_w;
	h_backup = data_config2.dst_h;
	DISPCHECK(
		"[display_test resolution]w_backup %d h_backup %d dsi_mode_backup %d\n",
		  w_backup, h_backup, dsi_mode_backup);
	/* for dsi config */
	DSI_ForceConfig(1);
	for (i = 0; i < sizeof(width_array)/sizeof(int); i++) {
		dst_width = width_array[i];
		dst_heigh = heigh_array[i];
		DISPCHECK("[display_test resolution] width %d, heigh %d\n",
			dst_width, dst_heigh);
		lcm_param2 = disp_lcm_get_params(pgc->plcm);
		lcm_param2->dsi.mode = CMD_MODE;
		lcm_param2->dsi.horizontal_active_pixel = dst_width;
		lcm_param2->dsi.vertical_active_line = dst_heigh;

		data_config2.dispif_config.dsi.mode = CMD_MODE;
		data_config2.dispif_config.dsi.horizontal_active_pixel =
			dst_width;
		data_config2.dispif_config.dsi.vertical_active_line =
			dst_heigh;

		data_config2.dst_w = dst_width;
		data_config2.dst_h = dst_heigh;

		data_config2.ovl_config[0].layer    = 0;
		data_config2.ovl_config[0].layer_en = 0;
		data_config2.ovl_config[1].layer    = 1;
		data_config2.ovl_config[1].layer_en = 0;
		data_config2.ovl_config[2].layer    = 2;
		data_config2.ovl_config[2].layer_en = 0;
		data_config2.ovl_config[3].layer    = 3;
		data_config2.ovl_config[3].layer_en = 0;

		data_config2.dst_dirty = 1;
		data_config2.ovl_dirty = 1;

		dpmgr_path_set_video_mode(pgc->dpmgr_handle,
			primary_display_is_video_mode());

		dpmgr_path_config(pgc->dpmgr_handle, &data_config2,
			NULL);
		data_config2.dst_dirty = 0;
		data_config2.ovl_dirty = 0;

		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

		if (dpmgr_path_is_busy(pgc->dpmgr_handle))
			DISPERR(
				"[display_test]==>Fatal error, we didn't trigger display path but it's already busy\n");

		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);

		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		if (ret > 0 && !dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			if (i == 0)
				DISPCHECK(
					"[display_test resolution] display_result 0x%x unlimited!\n",
					array_id[i]);
			else if (i == 1)
				DISPCHECK(
					"[display_test resolution] display_result 0x%x unlimited (W<H)\n",
					array_id[i]);
			else
				DISPCHECK(
					"[display_test resolution] display_result 0x%x(%d x %d)\n",
					array_id[i], dst_width, dst_heigh);
			break;
		}
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	}
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	lcm_param2 = disp_lcm_get_params(pgc->plcm);
	lcm_param2->dsi.mode = dsi_mode_backup;
	lcm_param2->dsi.vertical_active_line = h_backup;
	lcm_param2->dsi.horizontal_active_pixel = w_backup;
	data_config2.dispif_config.dsi.vertical_active_line = h_backup;
	data_config2.dispif_config.dsi.horizontal_active_pixel = w_backup;
	data_config2.dispif_config.dsi.mode = dsi_mode_backup;
	data_config2.dst_w = w_backup;
	data_config2.dst_h = h_backup;
	data_config2.dst_dirty = 1;
	dpmgr_path_set_video_mode(pgc->dpmgr_handle,
		primary_display_is_video_mode());
	dpmgr_path_connect(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_config(pgc->dpmgr_handle, &data_config2, NULL);
	data_config2.dst_dirty = 0;
	DSI_ForceConfig(0);
	return ret;
}

int primary_display_check_test(void)
{
	int ret = 0;
	int esd_backup = 0;

	DISPCHECK("[display_test]Display test[Start]\n");
	_primary_path_lock(__func__);
	/* disable esd check */
	if (1) {
		esd_backup = 1;
		primary_display_esd_check_enable(0);
		msleep(2000);
		DISPCHECK("[display_test]Disable esd check end\n");
	}

	/* if suspend => return */
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK(
			"[display_test_result]======================================\n");
		DISPCHECK(
			"[display_test_result]==>Test Fail : primary display path is slept\n");
		DISPCHECK(
			"[display_test_result]======================================\n");
		goto done;
	}

	/* stop trigger loop */
	DISPCHECK("[display_test]Stop trigger loop[begin]\n");
	_cmdq_stop_trigger_loop();
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("[display_test]==>primary display path is busy\n");
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		if (ret <= 0)
			dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

		DISPCHECK("[display_test]==>wait frame done ret:%d\n", ret);
	}
	DISPCHECK("[display_test]Stop trigger loop[end]\n");

	/* test force te */
	/* primary_display_te_test(); */

	/* test roi */
	/* primary_display_roi_test(30, 30); */

	/* test resolution test */
	primary_display_resolution_test();

	DISPCHECK("[display_test]start dpmgr path[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPERR(
			"[display_test]==>Fatal error, we didn't trigger display path but it's already busy\n");

	DISPCHECK("[display_test]start dpmgr path[end]\n");

	DISPCHECK("[display_test]Start trigger loop[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[display_test]Start trigger loop[end]\n");

done:
	/* restore esd */
	if (esd_backup == 1) {
		primary_display_esd_check_enable(1);
		DISPCHECK("[display_test]Restore esd check\n");
	}
	/* unlock path */
	_primary_path_unlock(__func__);
	DISPCHECK("[display_test]Display test[End]\n");
	return ret;
}

struct OPT_BACKUP tui_opt_backup[4] = {
	{DISP_OPT_SHARE_SRAM, 0},
	{DISP_OPT_IDLEMGR_SWTCH_DECOUPLE, 0},
	{DISP_OPT_SMART_OVL, 0},
	{DISP_OPT_BYPASS_OVL, 0}
};

void stop_smart_ovl_nolock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tui_opt_backup); i++) {
		tui_opt_backup[i].value =
			disp_helper_get_option(tui_opt_backup[i].option);
		disp_helper_set_option(tui_opt_backup[i].option, 0);
	}

	for (i = 0; i < ARRAY_SIZE(tui_opt_backup); i++) {
		if ((tui_opt_backup[i].option == DISP_OPT_SHARE_SRAM) &&
			(tui_opt_backup[i].value == 1))
			leave_share_sram(CMDQ_SYNC_RESOURCE_WROT0);
	}
	/* primary_display_esd_check_enable(0);*/
}

void restart_smart_ovl_nolock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tui_opt_backup); i++)
		disp_helper_set_option(tui_opt_backup[i].option,
			tui_opt_backup[i].value);

	for (i = 0; i < ARRAY_SIZE(tui_opt_backup); i++) {
		if ((tui_opt_backup[i].option == DISP_OPT_SHARE_SRAM) &&
			(tui_opt_backup[i].value == 1))
			enter_share_sram(CMDQ_SYNC_RESOURCE_WROT0);
	}
}

static enum DISP_POWER_STATE tui_power_stat_backup;
static int tui_session_mode_backup;
static struct DDP_MODULE_DRIVER *ddp_module_backup;

/*
 * Now the normal display vsync is DDP_IRQ_RDMA0_DONE in vdo mode, but when
 * enter TUI, we must protect the rdma0, then, should switch it to the
 * DDP_IRQ_DSI0_FRAME_DONE.
 */
int display_vsync_switch_to_dsi(unsigned int flg)
{
	if (!primary_display_is_video_mode())
		return 0;

	if (!flg) {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
		dsi_enable_irq(DISP_MODULE_DSI0, NULL, 0);

	} else {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_FRAME_DONE);
		dsi_enable_irq(DISP_MODULE_DSI0, NULL, 1);
	}

	return 0;
}

int display_enter_tui(void)
{
	/* msleep(500); */
	DISPMSG("TDDP: %s\n", __func__);

	mmprofile_log_ex(ddp_mmp_get_events()->tui,
		MMPROFILE_FLAG_START, 0, 0);

	_primary_path_lock(__func__);

	if (primary_get_state() != DISP_ALIVE) {
		DISPERR("Can't enter tui: current_stat=%d is not alive\n",
			primary_get_state());
		goto err0;
	}

	tui_power_stat_backup = primary_set_state(DISP_BLANK);

	primary_display_idlemgr_kick(__func__, 0);

	if (primary_display_is_mirror_mode()) {
		DISPERR("Can't enter tui: current_mode=%s\n",
			session_mode_spy(pgc->session_mode));
		goto err1;
	}

	stop_smart_ovl_nolock();

	tui_session_mode_backup = pgc->session_mode;

	if (tui_session_mode_backup == DISP_SESSION_RDMA_MODE) {
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			pgc->session_id, 0, NULL, 0);
		tui_session_mode_backup = DISP_SESSION_DIRECT_LINK_MODE;
	}

	if (disp_helper_get_option(DISP_OPT_TUI_MODE)
			== TUI_SINGLE_WINDOW_MODE) {
		do_primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE,
			pgc->session_id, 0, NULL, 0);
	} else if (disp_helper_get_option(DISP_OPT_TUI_MODE)
			== TUI_MULTIPLE_WINDOW_MODE) {
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			pgc->session_id, 0, NULL, 0);
		ddp_module_backup = ddp_get_module_driver(DISP_MODULE_OVL0_2L);
		ddp_set_module_driver(DISP_MODULE_OVL0_2L, 0);
		DISPMSG("[cc]%s:set module driver(OVL0_2L):%p\n",
			__func__, ddp_get_module_driver(DISP_MODULE_OVL0_2L));
	} else {
		DISPERR("Unsupport TUI mode: %d\n",
			disp_helper_get_option(DISP_OPT_TUI_MODE));
	}

	display_vsync_switch_to_dsi(1);
	mmprofile_log_ex(ddp_mmp_get_events()->tui,
		MMPROFILE_FLAG_PULSE, 0, 1);

	_primary_path_unlock(__func__);
	return 0;

err1:
	primary_set_state(tui_power_stat_backup);

err0:
	mmprofile_log_ex(ddp_mmp_get_events()->tui,
		MMPROFILE_FLAG_END, 0, 0);
	_primary_path_unlock(__func__);

	return -1;
}

int display_exit_tui(void)
{
	pr_info("[TUI-HAL] display_exit_tui() start\n");
	mmprofile_log_ex(ddp_mmp_get_events()->tui,
		MMPROFILE_FLAG_PULSE, 1, 1);

	_primary_path_lock(__func__);
	primary_set_state(tui_power_stat_backup);

	/* trigger rdma to display last normal buffer */
	_decouple_update_rdma_config_nolock();
	/* workaround: wait until this frame triggered to lcm */
	/* msleep(32); */
	do_primary_display_switch_mode(tui_session_mode_backup,
		pgc->session_id, 0, NULL, 0);
	if (disp_helper_get_option(DISP_OPT_TUI_MODE)
		== TUI_MULTIPLE_WINDOW_MODE)
		ddp_set_module_driver(DISP_MODULE_OVL0_2L, ddp_module_backup);

	/* DISP_REG_SET(NULL, DISP_REG_RDMA_INT_ENABLE, 0xffffffff); */

	restart_smart_ovl_nolock();
	display_vsync_switch_to_dsi(0);
	_primary_path_unlock(__func__);

	mmprofile_log_ex(ddp_mmp_get_events()->tui, MMPROFILE_FLAG_END, 0, 0);
	DISPMSG("TDDP: %s\n", __func__);
	pr_info("[TUI-HAL] %s() done\n", __func__);
	return 0;
}

void ddp_irq_callback(enum DISP_MODULE_ENUM module, unsigned int reg_value)
{
	/* set config dirty, it will keep trigger loop busy refresh */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
}

static int self_refresh_idlemgr_status_backup;
static int primary_display_enter_self_refresh(void)
{
	_primary_path_lock(__func__);

	mmprofile_log_ex(ddp_mmp_get_events()->self_refresh,
		MMPROFILE_FLAG_START, 0, 0);

	if (primary_display_is_mirror_mode()) {
		/* we only accept non-mirror mode */
		disp_aee_print("enter self-refresh mode fail\n");
		goto out;
	}

	/* disable idle manager */
	self_refresh_idlemgr_status_backup = set_idlemgr(0, 0);

	/* stop smart ovl / bypass ovl */
	stop_smart_ovl_nolock();
	/* switch to directlink mode */
	do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
		pgc->session_id, 0, NULL, 1);

	if (!primary_display_is_video_mode())
		disp_register_irq_callback(ddp_irq_callback);

	pgc->primary_display_scenario = DISP_SCENARIO_SELF_REFRESH;
out:
	_primary_path_unlock(__func__);
	return 0;
}

static int primary_display_exit_self_refresh(void)
{
	_primary_path_lock(__func__);

	if (primary_display_is_mirror_mode()) {
		/* we only accept non-mirror mode */
		disp_aee_print("enter self-refresh mode fail\n");
		goto out;
	}

	/* disable idle manager */
	set_idlemgr(self_refresh_idlemgr_status_backup, 0);

	/* restart smart ovl */
	restart_smart_ovl_nolock();

	if (!primary_display_is_video_mode())
		disp_unregister_irq_callback(ddp_irq_callback);

	pgc->primary_display_scenario = DISP_SCENARIO_NORMAL;
out:
	_primary_path_unlock(__func__);
	mmprofile_log_ex(ddp_mmp_get_events()->self_refresh,
		MMPROFILE_FLAG_END, 0, 0);
	return 0;
}

int primary_display_set_scenario(int scenario)
{
	int ret = 0;

	if (scenario != DISP_SCENARIO_NORMAL &&
	    pgc->primary_display_scenario != DISP_SCENARIO_NORMAL) {
		/* every scenario should start from NORMAL !! */
		pr_info("%s set scenario %d fail ! current scenario is %d\n",
			__func__, scenario, pgc->primary_display_scenario);
		return -EINVAL;
	}

	if (scenario == DISP_SCENARIO_SELF_REFRESH)
		ret = primary_display_enter_self_refresh();

	if (scenario == DISP_SCENARIO_NORMAL) {
		if (pgc->primary_display_scenario == DISP_SCENARIO_SELF_REFRESH)
			ret = primary_display_exit_self_refresh();
	}

	return ret;
}
