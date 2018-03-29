/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "debug.h"

#include "disp_drv_log.h"

#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtk_sync.h"
#include "mtkfb.h"

#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_debug.h"
#include "ddp_od.h"
#include "disp_session.h"

#include "m4u.h"
#include "m4u_port.h"
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "mt_smi.h"

#include "ddp_manager.h"
#include "mtkfb_fence.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "fbconfig_kdebug_rome.h"
#include "ddp_mmp.h"
/* for sodi reg addr define */
#ifdef DISP_ENABLE_SODI
#include "mt_spm.h"
#include "mt_spm_idle.h"
#endif
#include "ddp_irq.h"
/*#include "mach/eint.h" */
#include <mt-plat/mt_gpio.h>
#include "disp_session.h"
#include "disp_drv_platform.h"
#include "ion_drv.h"
#include <linux/slab.h>
#include "mtk_ion.h"
#include "mtkfb.h"
/* for dump decouple internal buffer */
#include<linux/fs.h>
#include<linux/uaccess.h>

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

typedef void (*fence_release_callback) (unsigned long data);

unsigned int is_hwc_enabled = 0;
int primary_display_use_cmdq = CMDQ_DISABLE;
int primary_display_use_m4u = 1;
DISP_PRIMARY_PATH_MODE primary_display_mode = DIRECT_LINK_MODE;

static unsigned long dc_vAddr[DISP_INTERNAL_BUFFER_COUNT];
#if defined(MTK_ALPS_BOX_SUPPORT)
#else
static disp_internal_buffer_info *decouple_buffer_info[DISP_INTERNAL_BUFFER_COUNT];
#endif
static RDMA_CONFIG_STRUCT decouple_rdma_config;
static WDMA_CONFIG_STRUCT decouple_wdma_config;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static DISP_BUFFER_TYPE g_wdma_rdma_security = DISP_NORMAL_BUFFER;
/*just for test*/
unsigned int primary_ovl0_handle;
int primary_ovl0_handle_size;
static bool g_is_sec;
#endif
static disp_mem_output_config mem_config;
static unsigned int primary_session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
unsigned long long last_primary_trigger_time = 0xffffffffffffffff;

static void disp_set_sodi(unsigned int enable, void *cmdq_handle);

/* DDP_SCENARIO_ENUM ddp_scenario = DDP_SCENARIO_SUB_RDMA1_DISP; */
#ifdef DISP_SWITCH_DST_MODE
int primary_display_def_dst_mode = 0;
int primary_display_cur_dst_mode = 0;
#endif

static unsigned long dc_vAddr[DISP_INTERNAL_BUFFER_COUNT];
int primary_trigger_cnt = 0;
unsigned int gPresentFenceIndex = 0;

#define PRIMARY_DISPLAY_TRIGGER_CNT (1)

typedef struct {
	DISP_POWER_STATE state;
	unsigned int lcm_fps;
	int max_layer;
	int need_trigger_overlay;
	int need_trigger_ovl1to2;
	int need_trigger_dcMirror_out;
	DISP_PRIMARY_PATH_MODE mode;
	int session_mode;
	unsigned int session_id;
	unsigned int last_vsync_tick;
	unsigned long framebuffer_mva;
	unsigned long framebuffer_va;
	struct mutex lock;
	struct mutex capture_lock;
	struct mutex vsync_lock;
#ifdef DISP_SWITCH_DST_MODE
	struct mutex switch_dst_lock;
#endif
	struct mutex cmd_lock;
	disp_lcm_handle *plcm;
	cmdqRecHandle cmdq_handle_config_esd;

	cmdqRecHandle cmdq_handle_trigger;

	cmdqRecHandle cmdq_handle_config;
	disp_path_handle dpmgr_handle;

	cmdqRecHandle cmdq_handle_ovl1to2_config;
	disp_path_handle ovl2mem_path_handle;

	char *mutex_locker;
	int vsync_drop;
	int trigger_cnt;
	struct mutex dc_lock;
	struct list_head dc_free_list;
	struct list_head dc_reading_list;
	struct list_head dc_writing_list;
	unsigned int dc_buf_id;
	unsigned int dc_buf[DISP_INTERNAL_BUFFER_COUNT];
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	unsigned int dc_sec_buf[DISP_INTERNAL_BUFFER_COUNT];
#endif
	cmdqBackupSlotHandle cur_config_fence;
	cmdqBackupSlotHandle subtractor_when_free;

	cmdqBackupSlotHandle rdma_buff_info;
	cmdqBackupSlotHandle ovl_status_info;

#ifdef DISP_DUMP_EVENT_STATUS
	cmdqBackupSlotHandle event_status;
#endif
} display_primary_path_context;

#define pgc	_get_context()

static display_primary_path_context *_get_context(void)
{
	static int is_context_inited;
	static display_primary_path_context g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0, sizeof(display_primary_path_context));
		is_context_inited = 1;
	}

	return &g_context;
}

static int _is_mirror_mode(DISP_MODE mode)
{
	if (mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE
	    || mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;
	else
		return 0;
}

static int _is_decouple_mode(DISP_MODE mode)
{
	if (mode == DISP_SESSION_DECOUPLE_MODE || mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return 1;
	else
		return 0;
}

static void _primary_path_lock(const char *caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&(pgc->lock));
	pgc->mutex_locker = (char *)caller;
}

static void _primary_path_unlock(const char *caller)
{
	pgc->mutex_locker = NULL;
	disp_sw_mutex_unlock(&(pgc->lock));
	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

struct mutex esd_mode_switch_lock;
static void _primary_path_esd_check_lock(void)
{
	mutex_lock(&esd_mode_switch_lock);
}

static void _primary_path_esd_check_unlock(void)
{
	mutex_unlock(&esd_mode_switch_lock);
}


#ifdef MTK_DISP_IDLE_LP
static atomic_t isDdp_Idle = ATOMIC_INIT(0);
static atomic_t idle_detect_flag = ATOMIC_INIT(0);
static struct mutex idle_lock;
static struct task_struct *primary_display_idle_detect_task;
unsigned int isIdlePowerOff = 0;
unsigned int isDSIOff = 0;

#define DISP_DSI_REG_VFP 0x28
static DECLARE_WAIT_QUEUE_HEAD(idle_detect_wq);
static DEFINE_SPINLOCK(gLockTopClockOff);
static int primary_display_switch_mode_nolock(int sess_mode, unsigned int session, int force);
static void _cmdq_flush_config_handle_mira(void *handle, int blocking);

/*static void _disp_primary_idle_lock()
{
	mutex_lock(&idle_lock);
}

static void _disp_primary_idle_unlock()
{
	mutex_unlock(&idle_lock);
}*/

static int _disp_primary_path_idle_clock_on(unsigned long level)
{
	dpmgr_path_idle_on(pgc->dpmgr_handle, NULL, level);
	return 0;
}

static int _disp_primary_path_idle_clock_off(unsigned long level)
{
	dpmgr_path_idle_off(pgc->dpmgr_handle, NULL, level);
	return 0;
}

static int _disp_primary_path_dsi_clock_on(unsigned long level)
{
	if (!primary_display_is_video_mode()) {
		unsigned long flags;

		spin_lock_irqsave(&gLockTopClockOff, flags);
		isDSIOff = 0;
		dpmgr_path_dsi_on(pgc->dpmgr_handle, NULL, level);
		spin_unlock_irqrestore(&gLockTopClockOff, flags);
	}

	return 0;
}

static int _disp_primary_path_dsi_clock_off(unsigned long level)
{
	if (!primary_display_is_video_mode()) {
		unsigned long flags;

		spin_lock_irqsave(&gLockTopClockOff, flags);
		dpmgr_path_dsi_off(pgc->dpmgr_handle, NULL, level);
		isDSIOff = 1;
		spin_unlock_irqrestore(&gLockTopClockOff, flags);
	}

	return 0;
}

int _disp_primary_path_set_vfp(int enter)
{
	int ret = 0;

	if (primary_display_is_video_mode()) {
		LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
		cmdqRecHandle cmdq_handle_vfp = NULL;

		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle_vfp);
		if (ret != 0) {
			DISPCHECK("fail to create primary cmdq handle for set vfp\n");
			return -1;
		}
		DISPCHECK("primary set vfp, handle=%p\n", cmdq_handle_vfp);
		cmdqRecReset(cmdq_handle_vfp);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_vfp);
		if (enter) {
			if (disp_low_power_adjust_vfp != 0)
				dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_vfp, DDP_DSI_VFP_LP,
						 (unsigned long *)&(disp_low_power_adjust_vfp));
			else
				dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_vfp, DDP_DSI_VFP_LP,
						 (unsigned long *)&(lcm_param->
								    dsi.vertical_vfp_lp));
		} else
			dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_vfp, DDP_DSI_VFP_LP,
					 (unsigned long *)&(lcm_param->dsi.vertical_frontporch));

		MMProfileLogEx(ddp_mmp_get_events()->dal_clean, MMProfileFlagPulse, 0, enter);

		_cmdq_flush_config_handle_mira(cmdq_handle_vfp, 1);
		DISPCHECK("[VFP]cmdq_handle_vfp ret=%d\n", ret);
		cmdqRecDestroy(cmdq_handle_vfp);
		cmdq_handle_vfp = NULL;
	} else {
		DISPCHECK("CMD mode don't set vfp for lows\n");
	}

	return ret;
}

int _disp_primary_path_change_dst_clk(int enter)
{
	int ret = 0;

	if (primary_display_is_video_mode()) {
		LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
		cmdqRecHandle cmdq_change_clk = NULL;

		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_change_clk);
		if (ret != 0) {
			DISPCHECK("fail to create primary cmdq handle for change dst clock\n");
			return -1;
		}
		DISPCHECK("primary change dst clock, handle=%p\n", cmdq_change_clk);
		cmdqRecReset(cmdq_change_clk);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_change_clk);
		if (enter) {
			if (disp_low_power_reduse_clock != 0)
				dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_change_clk,
						 DDP_DSI_CHANGE_CLK,
						 (unsigned long *)&(disp_low_power_reduse_clock));
			else
				dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_change_clk,
						 DDP_DSI_CHANGE_CLK,
						 (unsigned long *)&(lcm_param->dsi.PLL_CLOCK_lp));
		} else
			dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_change_clk, DDP_DSI_CHANGE_CLK,
					 (unsigned long *)&(lcm_param->dsi.PLL_CLOCK));

		_cmdq_flush_config_handle_mira(cmdq_change_clk, 1);
		DISPCHECK("[Change CLK]cmdq_change_clk ret=%d\n", ret);
		cmdqRecDestroy(cmdq_change_clk);
		cmdq_change_clk = NULL;
	} else {
		DISPCHECK("CMD mode don't change dst clock\n");
	}

	return ret;
}

static int _disp_primary_low_power_change_ddp_clock(unsigned int enter)
{
	if (1 == disp_low_power_disable_ddp_clock) {
		static unsigned int disp_low_power_disable_ddp_clock_cnt;

		DISPDBG("MM clock, disp_low_power_disable_ddp_clock enter %d.\n",
			disp_low_power_disable_ddp_clock_cnt++);

		if (primary_display_is_video_mode() == 0) {
			if (1 == enter) {
				if (isIdlePowerOff == 0) {
					/* extern void clk_stat_bug(void); */
					unsigned long flags;

					DDPDBG("off MM clock start.\n");
					spin_lock_irqsave(&gLockTopClockOff, flags);
					_disp_primary_path_idle_clock_off(0);	/* parameter represent level */
					isIdlePowerOff = 1;
					spin_unlock_irqrestore(&gLockTopClockOff, flags);
					DDPDBG("off MM clock end.\n");
				}
			} else {
				if (isIdlePowerOff == 1) {
					unsigned long flags;

					DDPDBG("on MM clock start.\n");
					spin_lock_irqsave(&gLockTopClockOff, flags);
					isIdlePowerOff = 0;
					_disp_primary_path_idle_clock_on(0);	/* parameter represent level */
					spin_unlock_irqrestore(&gLockTopClockOff, flags);
					DDPDBG("on MM clock end.\n");
				}

			}
		} else {
			if (1 == enter) {
				if (isIdlePowerOff == 0) {
					DDPDBG("off ovl wdma clock start.\n");
					if (_is_decouple_mode(pgc->session_mode))
						dpmgr_path_power_off(pgc->ovl2mem_path_handle,
								     CMDQ_DISABLE);
					isIdlePowerOff = 1;
					DDPDBG("off ovl wdma clock end.\n");
				}
			} else {
				if (isIdlePowerOff == 1) {
					DDPDBG("on ovl wdma clock start.\n");
					if (_is_decouple_mode(pgc->session_mode))
						dpmgr_path_power_on(pgc->ovl2mem_path_handle,
								    CMDQ_DISABLE);
					isIdlePowerOff = 0;
					DDPDBG("on ovl wdma clock end.\n");
				}

			}
		}
	}

	return 0;
}


int primary_display_save_power_for_idle(int enter, unsigned int need_primary_lock)
{
	static unsigned int isLowPowerMode;

	if (is_hwc_enabled == 0)
		return 0;

	/* if outer api has add primary lock, do not have to lock again */
	if (need_primary_lock)
		_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPMSG("suspend mode can not enable low power.\n");
		goto end;
	}
	if (enter == 1 && isLowPowerMode == 1) {
		DISPMSG("already in low power mode.\n");
		goto end;
	}
	if (enter == 0 && isLowPowerMode == 0) {
		DISPMSG("already not in low power mode.\n");
		goto end;
	}
	isLowPowerMode = enter;

	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		goto end;
	}

	DDPDBG("low power in, enter=%d.\n", enter);

	if (primary_display_is_video_mode() == 1 && disp_low_power_reduse_fps == 1) {
		/*_disp_primary_path_change_dst_clk(enter);*/
		_disp_primary_path_set_vfp(enter);
	}

	if (primary_display_is_video_mode() == 0)
		_disp_primary_low_power_change_ddp_clock(enter);

	/* no need idle lock ,cause primary lock will be used inside switch_mode */
	if (disp_low_power_remove_ovl == 1) {
		if (primary_display_is_video_mode() == 1) {	/* only for video mode */
			/* DISPMSG("[LP] OVL pgc->session_mode=%d, enter=%d\n", pgc->session_mode, enter); */
			if (enter) {
				if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
				    && !g_is_sec
#endif
				    ) {
					DDPDBG("[LP]switch to decouple.\n");
					primary_display_switch_mode_nolock
					    (DISP_SESSION_DECOUPLE_MODE, pgc->session_id, 1);
					_disp_primary_low_power_change_ddp_clock(enter);
				}
				/* DISPMSG("disp_low_power_remove_ovl 2\n"); */
			} else {
				if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
					DDPDBG("[LP]switch to directlink.\n");
					_disp_primary_low_power_change_ddp_clock(enter);
					primary_display_switch_mode_nolock
					    (DISP_SESSION_DIRECT_LINK_MODE, pgc->session_id, 1);
				}
				/* DISPMSG("disp_low_power_remove_ovl 4\n"); */
			}
		}
	}

end:
	if (need_primary_lock)
		_primary_path_unlock(__func__);

	return 0;
}

void _disp_primary_path_exit_idle(const char *caller, unsigned int need_primary_lock)
{
	/* _disp_primary_idle_lock(); */
	if (atomic_read(&isDdp_Idle) == 1) {
		DDPDBG("[ddp_idle_on]_disp_primary_path_exit_idle (%s) &&&\n", caller);
		primary_display_save_power_for_idle(0, need_primary_lock);
		atomic_set(&isDdp_Idle, 0);
		atomic_set(&idle_detect_flag, 1);
		wake_up(&idle_detect_wq);
	}
	/* _disp_primary_idle_unlock(); */
}

/* used by aal/pwm to exit idle before config register(for "screen idle top clock off" feature) */
void disp_update_trigger_time(void)
{
	last_primary_trigger_time = sched_clock();
}

void disp_exit_idle_ex(const char *caller)
{
	if (primary_display_is_video_mode() == 0) {
		disp_update_trigger_time();
		_disp_primary_path_exit_idle(caller, 0);
	}
}

static int _disp_primary_path_idle_detect_thread(void *data)
{
	int ret = 0;

	while (1) {
		msleep(500);	/* 0.5s trigger once */
		/*DISPMSG("[ddp_idle]_disp_primary_path_idle_detect start\n"); */

		if (gSkipIdleDetect || atomic_read(&isDdp_Idle) == 1) {	/* skip is already in idle mode */
			DDPDBG("[ddp_idle]_disp_primary_path_idle_detect is already Idle\n");
			continue;
		}
		_primary_path_lock(__func__);
		if (pgc->state == DISP_SLEPT) {
			MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagPulse, 1, 0);
			/* DISPCHECK("[ddp_idle]primary display path is slept?? -- skip ddp_idle\n"); */
			_primary_path_unlock(__func__);
			continue;
		}
		_primary_path_unlock(__func__);
		/* _disp_primary_idle_lock(); */
		_primary_path_esd_check_lock();
		_primary_path_lock(__func__);
		if (((sched_clock() - last_primary_trigger_time) / 1000) > 500 * 1000) {

			DDPDBG("[LP] - enter\n");
			atomic_set(&isDdp_Idle, 1);
			primary_display_save_power_for_idle(1, 0);
			_primary_path_unlock(__func__);
			_primary_path_esd_check_unlock();
		} else {
			/*DISPMSG("[ddp_idle]_disp_primary_path_idle_detect check time <500ms\n"); */
			/* _disp_primary_idle_unlock(); */
			_primary_path_unlock(__func__);
			_primary_path_esd_check_unlock();
			continue;
		}
		/* _disp_primary_idle_unlock(); */

		ret =
		    wait_event_interruptible(idle_detect_wq, (atomic_read(&idle_detect_flag) != 0));
		atomic_set(&idle_detect_flag, 0);
		DDPDBG("[ddp_idle]ret=%d\n", ret);
		if (kthread_should_stop())
			break;

		DDPDBG("[LP] end\n");
	}

	return ret;
}

int primary_display_cmdq_set_reg(unsigned int addr, unsigned int val)
{
	int ret = 0;
	cmdqRecHandle handle = NULL;

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);

	cmdqRecWrite(handle, addr & 0x1fffffff, val, ~0);
	cmdqRecFlushAsync(handle);

	cmdqRecDestroy(handle);

	return 0;
}
#endif


#ifdef DISP_SWITCH_DST_MODE
unsigned long long last_primary_trigger_time;
bool is_switched_dst_mode = false;
static struct task_struct *primary_display_switch_dst_mode_task;
static void _primary_path_switch_dst_lock(void)
{
	mutex_lock(&(pgc->switch_dst_lock));
}

static void _primary_path_switch_dst_unlock(void)
{
	mutex_unlock(&(pgc->switch_dst_lock));
}

static int _disp_primary_path_switch_dst_mode_thread(void *data)
{
	int ret = 0;

	while (1) {
		msleep(1000);

		if (((sched_clock() - last_primary_trigger_time) / 1000) > 500000) {	/* 500ms not trigger disp */
			primary_display_switch_dst_mode(0);	/* switch to cmd mode */
			is_switched_dst_mode = true;
		}
		if (kthread_should_stop())
			break;
	}
	return 0;
}
#endif
int primary_display_get_debug_state(char *stringbuf, int buf_len)
{
	int len = 0;
#if HDMI_MAIN_PATH
#else
	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	LCM_DRIVER *lcm_drv = pgc->plcm->drv;
#endif
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|--------------------------------------------------------------------------------------|\n");
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|********Primary Display Path General Information********\n");
	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|Primary Display is %s\n",
		      dpmgr_path_is_idle(pgc->dpmgr_handle) ? "idle" : "busy");

	if (mutex_trylock(&(pgc->lock))) {
		mutex_unlock(&(pgc->lock));
		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "|primary path global mutex is free\n");
	} else {
		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "|primary path global mutex is hold by [%s]\n", pgc->mutex_locker);
	}

#if HDMI_MAIN_PATH
#else
	if (lcm_param && lcm_drv)
		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "|LCM Driver=[%s]\tResolution=%dx%d,Interface:%s\n", lcm_drv->name,
			      lcm_param->width, lcm_param->height,
			      (lcm_param->type == LCM_TYPE_DSI) ? "DSI" : "Other");
#endif

	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|OD is %s\n",
		      disp_od_is_enabled() ? "enabled" : "disabled");

	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|State=%s\tlcm_fps=%d\tmax_layer=%d\tmode:%d\tvsync_drop=%d\n",
		      pgc->state == DISP_ALIVE ? "Alive" : "Sleep", pgc->lcm_fps, pgc->max_layer,
		      pgc->mode, pgc->vsync_drop);
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|cmdq_handle_config=0x%p\tcmdq_handle_trigger=0x%p\tdpmgr_handle=0x%p\tovl2mem_path_handle=0x%p\n",
		      pgc->cmdq_handle_config, pgc->cmdq_handle_trigger, pgc->dpmgr_handle,
		      pgc->ovl2mem_path_handle);
	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|Current display driver status=%s + %s\n",
		      primary_display_is_video_mode() ? "video mode" : "cmd mode",
		      primary_display_cmdq_enabled() ? "CMDQ Enabled" : "CMDQ Disabled");

	return len;
}

static DISP_MODULE_ENUM _get_dst_module_by_lcm(disp_lcm_handle *plcm)
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
		return DISP_MODULE_DPI0;
	}
	DISPERR("can't find primary path dst module\n");
	return DISP_MODULE_UNKNOWN;

}

#define AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

/***************************************************************
***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 1.wait idle:           N         N       Y        Y
*** 2.lcm update:          N         Y       N        Y
*** 3.path start:	idle->Y      Y    idle->Y     Y
*** 4.path trigger:     idle->Y      Y    idle->Y     Y
*** 5.mutex enable:        N         N    idle->Y     Y
*** 6.set cmdq dirty:      N         Y       N        N
*** 7.flush cmdq:          Y         Y       N        N
****************************************************************/

int _should_wait_path_idle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	*** 1.wait idle:	          N         N        Y        Y					*/
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
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 2.lcm update:          N         Y       N        Y        **/
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;

	} else {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 1;

	}
}

int _should_start_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 3.path start:	idle->Y      Y    idle->Y     Y        ***/
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;

	} else {
		if (primary_display_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 1;

	}
}

int _should_trigger_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 4.path trigger:     idle->Y      Y    idle->Y     Y
*** 5.mutex enable:        N         N    idle->Y     Y        ***/

	/* this is not a perfect design, we can't decide path trigger(ovl/rdma/dsi..) separately with mutex enable */
	/* but it's lucky because path trigger and mutex enable is the same w/o cmdq, and it's correct w/ CMDQ(Y+N). */
	if (primary_display_cmdq_enabled()) {
		if (primary_display_is_video_mode())
			return 0;
		else
			return 0;

	} else {
		if (primary_display_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 1;

	}
}

int _should_set_cmdq_dirty(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 6.set cmdq dirty:	    N         Y       N        N     ***/
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
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N        ***/
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
			return 1;
		else
			return 0;

	}
}

int _should_insert_wait_frame_done_token(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N      */
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
	if (pgc->mode == SINGLE_LAYER_MODE || pgc->mode == DEBUG_RDMA1_DSI0_MODE)
		return 0;
	else
		return 1;

}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
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
	return HRTIMER_RESTART;
}

int _init_vsync_fake_monitor(int fps)
{
	static struct hrtimer cmd_mode_update_timer;
	static ktime_t cmd_mode_update_timer_period;

	if (fps == 0)
		fps = 60;

	cmd_mode_update_timer_period = ktime_set(0, 1000 / fps * 1000);
	DISPMSG("[MTKFB] vsync timer_period=%d\n", 1000 / fps);
	hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;

	return 0;
}

#if defined(MTK_ALPS_BOX_SUPPORT)
#else
static disp_internal_buffer_info *allocat_decouple_buffer(int size)
{
	disp_internal_buffer_info *buf_info = NULL;
#if defined(MTK_FB_ION_SUPPORT)
	void *buffer_va = NULL;
	unsigned int buffer_mva = 0;
	unsigned int mva_size = 0;
	struct ion_mm_data mm_data;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	client = ion_client_create(g_ion_device, "disp_decouple");
	buf_info = kzalloc(sizeof(disp_internal_buffer_info), GFP_KERNEL);
	if (buf_info) {
		handle = ion_alloc(client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
		if (IS_ERR(handle)) {
			DISPERR("Fatal Error, ion_alloc for size %d failed\n", size);
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
		mm_data.config_buffer_param.kernel_handle = handle;
		mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
		if (ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0) {
			DISPERR("ion_test_drv: Config buffer failed.\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}

		ion_phys(client, handle, (ion_phys_addr_t *) &buffer_mva, (size_t *) &mva_size);
		if (buffer_mva == 0) {
			DISPERR("Fatal Error, get mva failed\n");
			ion_free(client, handle);
			ion_client_destroy(client);
			kfree(buf_info);
			return NULL;
		}
		buf_info->handle = handle;
		buf_info->mva = (unsigned int)buffer_mva;
		buf_info->size = mva_size;
		buf_info->va = buffer_va;
	} else {
		DISPERR("Fatal error, kzalloc internal buffer info failed!!\n");
		kfree(buf_info);
		return NULL;
	}
#endif
	return buf_info;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
KREE_SESSION_HANDLE primary_display_secure_memory_session_handle(void)
{
	static KREE_SESSION_HANDLE disp_secure_memory_session;

	/* TODO: the race condition here is not taken into consideration. */
	if (!disp_secure_memory_session) {
		TZ_RESULT ret;

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &disp_secure_memory_session);
		if (ret != TZ_RESULT_SUCCESS) {
			DISPERR("KREE_CreateSession fail, ret=%d\n", ret);
			return 0;
		}
	}
	DISPDBG("disp_secure_memory_session_handle() session = %x\n",
		(unsigned int)disp_secure_memory_session);

	return disp_secure_memory_session;
}

static KREE_SECUREMEM_HANDLE allocate_decouple_sec_buffer(unsigned int buffer_size)
{
	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle;

	/* allocate secure buffer by tz api */
	ret = KREE_AllocSecurechunkmemWithTag(primary_display_secure_memory_session_handle(),
				       &mem_handle, 0, buffer_size, "primary_disp");
	if (ret != TZ_RESULT_SUCCESS) {
		DISPERR("KREE_AllocSecurechunkmemWithTag fail, ret = %d\n", ret);
		return -1;
	}
	DISPCHECK("KREE_AllocSecurechunkmemWithTag handle = 0x%x\n", mem_handle);

	return mem_handle;
}

static KREE_SECUREMEM_HANDLE free_decouple_sec_buffer(KREE_SECUREMEM_HANDLE mem_handle)
{
	TZ_RESULT ret;

	ret = KREE_UnreferenceSecurechunkmem(primary_display_secure_memory_session_handle(), mem_handle);

	if (ret != TZ_RESULT_SUCCESS)
		DISPERR("KREE_UnreferenceSecurechunkmem fail, ret = %d\n", ret);

	DISPCHECK("KREE_UnreferenceSecurechunkmem handle = 0x%x\n", mem_handle);

	return ret;
}
#endif

static int init_decouple_buffers(void)
{
	int i = 0;
	int height = primary_display_get_height();
	int width = primary_display_get_width();
	int bpp = primary_display_get_bpp();
	int buffer_size = width * height * bpp / 8;

	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		decouple_buffer_info[i] = allocat_decouple_buffer(buffer_size);
		if (decouple_buffer_info[i] != NULL) {
			pgc->dc_buf[i] = decouple_buffer_info[i]->mva;
			dc_vAddr[i] = (unsigned long)decouple_buffer_info[i]->va;
			DISPMSG
			    ("decouple NORMAL buffer : pgc->dc_buf[%d] = 0x%x dc_vAddr[%d] = 0x%lx\n",
			     i, pgc->dc_buf[i], i, dc_vAddr[i]);
		}
	}

	/*initialize rdma config */
	decouple_rdma_config.height = height;
	decouple_rdma_config.width = width;
	decouple_rdma_config.idx = 0;
	decouple_rdma_config.inputFormat = eRGB888;
	decouple_rdma_config.pitch = width * DP_COLOR_BITS_PER_PIXEL(eRGB888) / 8;
	decouple_rdma_config.security = DISP_NORMAL_BUFFER;

	/*initialize wdma config */
	decouple_wdma_config.srcHeight = height;
	decouple_wdma_config.srcWidth = width;
	decouple_wdma_config.clipX = 0;
	decouple_wdma_config.clipY = 0;
	decouple_wdma_config.clipHeight = height;
	decouple_wdma_config.clipWidth = width;
	decouple_wdma_config.outputFormat = eRGB888;
	decouple_wdma_config.useSpecifiedAlpha = 1;
	decouple_wdma_config.alpha = 0xFF;
	decouple_wdma_config.dstPitch = width * DP_COLOR_BITS_PER_PIXEL(eRGB888) / 8;
	decouple_wdma_config.security = DISP_NORMAL_BUFFER;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* allocate secure buffer */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		pgc->dc_sec_buf[i] = 0;
	}
#endif

	return 0;
}
#endif

static int config_display_m4u_port(M4U_PORT_ID id, DISP_MODULE_ENUM module)
{
	int ret;
	M4U_PORT_STRUCT sPort;

	sPort.ePortID = id;
	sPort.Virtuality = primary_display_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret != 0) {
		DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",
			  ddp_get_module_name(module),
			  primary_display_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

	return 0;
}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	DISPFUNC();
	pgc->mode = DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

#if HDMI_MAIN_PATH
	dst_module = DISP_MODULE_DPI0;
#elif MAIN_PATH_DISABLE_LCM
	dst_module = DISP_MODULE_DSI0;
#else
	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
#endif
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	config_display_m4u_port(M4U_PORT_DISP_OVL0, DISP_MODULE_OVL0);
	config_display_m4u_port(M4U_PORT_DISP_RDMA0, DISP_MODULE_RDMA0);
	config_display_m4u_port(M4U_PORT_DISP_WDMA0, DISP_MODULE_WDMA0);

	/* config m4u port used by hdmi just once */
	config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1);
	config_display_m4u_port(M4U_PORT_DISP_RDMA1, DISP_MODULE_RDMA1);
	config_display_m4u_port(M4U_PORT_DISP_WDMA1, DISP_MODULE_WDMA1);

#if HDMI_MAIN_PATH
#else
	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);
#endif

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	return ret;
}

static int _build_path_decouple(void)
{
	return 0;
}

static int _build_path_single_layer(void)
{
	return 0;
}

static int _build_path_debug_rdma1_dsi0(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = DEBUG_RDMA1_DSI0_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	config_display_m4u_port(M4U_PORT_DISP_RDMA1, DISP_MODULE_RDMA1);

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static int _build_path_debug_rdma2_dpi0(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = DEBUG_RDMA2_DPI0_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA2_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	config_display_m4u_port(M4U_PORT_DISP_RDMA2, DISP_MODULE_RDMA2);

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

#ifdef DISP_SUPPORT_CMDQ
static void _cmdq_build_trigger_loop(void)
{

	int ret = 0;

	if (pgc->cmdq_handle_trigger == NULL) {
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
		DDPDBG("primary path trigger thread cmd handle=0x%p\n", pgc->cmdq_handle_trigger);
	}
	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (primary_display_is_video_mode()) {
		disp_set_sodi(0, pgc->cmdq_handle_trigger);
		/* wait and clear stream_done, HW will assert mutex enable automatically in frame done reset. */
		/* todo: should let dpmanager to decide wait which mutex's eof. */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MUTEX0_STREAM_EOF);
		/* for some module(like COLOR) to read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF);

	} else {
		ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CABC_EOF);
		/* DSI command mode doesn't have mutex_stream_eof, need use CMDQ token instead */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		if (_need_wait_esd_eof()) {
			/* Wait esd config thread done. */
			ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		/* ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MDP_DSI0_TE_SOF); */
		/* for operations before frame transfer, such as waiting for DSI TE */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_BEFORE_STREAM_SOF);

		/* cleat frame done token, now the config thread will not allowed to config registers. */
		/* remember that config thread's priority is higher than trigger thread,
		   so all the config queued before will be applied then STREAM_EOF token be cleared */
		/* this is what CMDQ did as "Merge" */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		ret =
		    cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		/* enable mutex, only cmd mode need this */
		/* this is what CMDQ did as "Trigger" */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger, CMDQ_ENABLE);
		/* ret = cmdqRecWrite(pgc->cmdq_handle_trigger,
		   (unsigned int)(DISP_REG_CONFIG_MUTEX_EN(0))&0x1fffffff, 1, ~0); */

		/* SODI is disabled in config thread,
		   so mutex enable/dsi start/CPU wait TE will not be blocked by SODI */
		/* should enable SODI here, */
		if (gDisableSODIForTriggerLoop == 1)
			disp_set_sodi(1, pgc->cmdq_handle_trigger);

		/* waiting for frame done, because we can't use mutex stream eof here,
		   so need to let dpmanager help to decide which event to wait */
		/* most time we wait rdmax frame done event. */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA0_EOF);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_STREAM_EOF_EVENT);

		/* dsi is not idle rightly after rdma frame done,
		   so we need to polling about 1us for dsi returns to idle */
		/* do not polling dsi idle directly which will decrease CMDQ performance */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_CHECK_IDLE_AFTER_STREAM_EOF);

		/* for some module(like COLOR) to read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF);

		/* polling DSI idle */
		/* ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x1401b00c, 0, 0x80000000); */
		/* polling wdma frame done */
		/* ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x140060A0, 1, 0x1); */

		/* now frame done, config thread is allowed to config register now */
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* RUN forever!!!! */
		BUG_ON(ret < 0);
	}

	/* dump trigger loop instructions to check whether dpmgr_path_build_cmdq works correctly */
	DISPCHECK("primary display BUILD cmdq trigger loop finished\n");

}
#endif

static void _cmdq_start_trigger_loop(void)
{
#ifdef DISP_SUPPORT_CMDQ

	int ret = 0;

	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!primary_display_is_video_mode()) {
		if (_need_wait_esd_eof()) {
			/* Need set esd check eof synctoken to let trigger loop go. */
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		/* need to set STREAM_EOF for the first time, otherwise we will stuck in dead loop */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_CABC_EOF);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW);
	} else {
#if 0
		if (dpmgr_path_is_idle(pgc->dpmgr_handle))
			cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);

#endif
	}
	if (_is_decouple_mode(pgc->session_mode))
		cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
#endif

	DISPCHECK("primary display START cmdq trigger loop finished\n");
}

static void _cmdq_stop_trigger_loop(void)
{
#ifdef DISP_SUPPORT_CMDQ

	int ret = 0;

	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);
#endif
	DISPCHECK("primary display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if (!primary_display_is_video_mode()) {
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
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

static void _cmdq_flush_config_handle(int blocking, void *callback, unsigned int userdata)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, blocking,
			   (unsigned int)(unsigned long)callback);
	if (blocking) {
		cmdqRecFlush(pgc->cmdq_handle_config);
	} else {
		if (callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config, callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}
	pgc->trigger_cnt++;

	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, userdata, 0);
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

static void _cmdq_insert_wait_frame_done_token(void)
{
	if (primary_display_is_video_mode())
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_STREAM_EOF);


	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

void _cmdq_insert_wait_frame_done_token_mira(void *handle)
{
	if (primary_display_is_video_mode())
		cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);


	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

static int _convert_disp_input_to_rdma(RDMA_CONFIG_STRUCT *dst, primary_disp_input_config *src)
{
	if (src && dst) {
		dst->inputFormat = src->fmt;
		dst->address = src->addr;
		dst->width = src->src_w;
		dst->height = src->src_h;
		dst->pitch = src->src_pitch;
		dst->buf_offset = 0;

		return 0;
	}
	DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
	return -1;

}

static int _convert_disp_input_to_ovl(OVL_CONFIG_STRUCT *dst, primary_disp_input_config *src)
{
	if (src && dst) {
		dst->layer = src->layer;
		dst->layer_en = src->layer_en;
		dst->fmt = src->fmt;
		dst->addr = src->addr;
		dst->vaddr = src->vaddr;
		dst->src_x = src->src_x;
		dst->src_y = src->src_y;
		dst->src_w = src->src_w;
		dst->src_h = src->src_h;
		dst->src_pitch = src->src_pitch;
		dst->dst_x = src->dst_x;
		dst->dst_y = src->dst_y;
		dst->dst_w = src->dst_w;
		dst->dst_h = src->dst_h;
		dst->keyEn = src->keyEn;
		dst->key = src->key;
		dst->aen = src->aen;
		dst->alpha = src->alpha;
		dst->sur_aen = src->sur_aen;
		dst->src_alpha = src->src_alpha;
		dst->dst_alpha = src->dst_alpha;

		dst->isDirty = src->isDirty;

		dst->buff_idx = src->buff_idx;
		dst->identity = src->identity;
		dst->connected_type = src->connected_type;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		dst->security = src->security;
		/* just for test svp */
		if (dst->layer == 0 && gDebugSvp == 2)
			dst->security = DISP_SECURE_BUFFER;
		g_wdma_rdma_security = dst->security;
#endif
		dst->yuv_range = src->yuv_range;

		return 0;
	}
	DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
	return -1;

}

int _trigger_display_interface(int blocking, void *callback, unsigned int userdata)
{
	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

	if (_should_update_lcm())
		disp_lcm_update(pgc->plcm, 0, 0, pgc->plcm->params->width,
				pgc->plcm->params->height, 0);

	if (_should_start_path())
		dpmgr_path_start(pgc->dpmgr_handle, primary_display_cmdq_enabled());

	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop,
		which should always be NULL for config thread */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, primary_display_cmdq_enabled());
	}
#ifndef DISP_SUPPORT_CMDQ
	if (primary_display_use_cmdq == CMDQ_DISABLE)
		dpmgr_path_flush(pgc->dpmgr_handle, 0);
#endif
	if (_should_set_cmdq_dirty()) {
		_cmdq_set_config_handle_dirty();

		/* disable SODI after set dirty */
		if (primary_display_is_video_mode() == 0 && gDisableSODIForTriggerLoop == 1)
			disp_set_sodi(0, pgc->cmdq_handle_config);
	}
	/* disable SODI by CPU before flush CMDQ by CPU */
	if (primary_display_is_video_mode() == 1) {
	#ifdef DISP_ENABLE_SODI
		spm_enable_sodi(0);
	#endif
	}

	if (gDumpConfigCMD == 1) {
		DISPMSG("primary_display_config, dump before flush:\n");
		cmdqRecDumpCommand(pgc->cmdq_handle_config);
	}

	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	/* enable SODI by CPU after CMDQ finish */
	if (primary_display_is_video_mode() == 1) {
	#ifdef DISP_ENABLE_SODI
		spm_enable_sodi(1);
	#endif
	}

	if (_should_insert_wait_frame_done_token() && (!_is_decouple_mode(pgc->session_mode))) {
		_cmdq_insert_wait_frame_done_token();
	}

	return 0;
}

int _trigger_overlay_engine(void)
{
	/* maybe we need a simple merge mechanism for CPU config. */
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL, primary_display_use_cmdq);
	return 0;
}

#define EEEEEEEEEEEEEEE
/******************************************************************************/
/* ESD CHECK / RECOVERY ---- BEGIN                                            */
/******************************************************************************/

#if HDMI_MAIN_PATH
#else
static struct task_struct *primary_display_esd_check_task;
#endif

static wait_queue_head_t esd_check_task_wq;	/* For Esd Check Task */

static atomic_t esd_check_task_wakeup = ATOMIC_INIT(0);	/* For Esd Check Task */
static wait_queue_head_t esd_ext_te_wq;	/* For Vdo Mode EXT TE Check */
static atomic_t esd_ext_te_event = ATOMIC_INIT(0);	/* For Vdo Mode EXT TE Check */
static atomic_t esd_check_bycmdq = ATOMIC_INIT(0);
static int eint_flag;		/* For DCT Setting */

struct task_struct *decouple_fence_release_task = NULL;
wait_queue_head_t decouple_fence_release_wq;
atomic_t decouple_fence_release_event = ATOMIC_INIT(0);

unsigned int _need_do_esd_check(void)
{
	int ret = 0;
#ifdef CONFIG_OF
	if ((pgc->plcm->params->dsi.esd_check_enable == 1) && (islcmconnected == 1))
		ret = 1;

#else
	if (pgc->plcm->params->dsi.esd_check_enable == 1)
		ret = 1;

#endif
	return ret;
}


unsigned int _need_register_eint(void)
{

	int ret = 1;

	/* 1.need do esd check */
	/* 2.dsi vdo mode */
	/* 3.customization_esd_check_enable = 0 */
	if (_need_do_esd_check() == 0)
		ret = 0;
	else if (primary_display_is_video_mode() == 0)
		ret = 0;
	else if (pgc->plcm->params->dsi.customization_esd_check_enable == 1)
		ret = 0;


	return ret;

}

unsigned int _need_wait_esd_eof(void)
{
	int ret = 1;

	/* 1.need do esd check */
	/* 2.customization_esd_check_enable = 1 */
	/* 3.dsi cmd mode */
	if (_need_do_esd_check() == 0)
		ret = 0;
	else if (pgc->plcm->params->dsi.customization_esd_check_enable == 0)
		ret = 0;
	else if (primary_display_is_video_mode())
		ret = 0;


	return ret;
}

/* For Cmd Mode Read LCM Check */
/* Config cmdq_handle_config_esd */
int _esd_check_config_handle_cmd(void)
{
	int ret = 0;		/* 0:success */

	/* 1.reset */
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	/* 2.write first instruction */
	/* cmd mode: wait CMDQ_SYNC_TOKEN_STREAM_EOF(wait trigger thread done) */
	cmdqRecWaitNoClear(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_STREAM_EOF);

	/* 3.clear CMDQ_SYNC_TOKEN_ESD_EOF(trigger thread need wait this sync token) */
	cmdqRecClearEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	/* 4.write instruction(read from lcm) */
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_ESD_CHECK_READ);

	/* 5.set CMDQ_SYNC_TOKE_ESD_EOF(trigger thread can work now) */
	cmdqRecSetEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	/* 6.flush instruction */
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);


	DISPCHECK("[ESD]_esd_check_config_handle_cmd ret=%d\n", ret);

/* done: */
	if (ret)
		ret = 1;
	return ret;
}
void primary_display_esd_cust_bycmdq(int enable)
{
	atomic_set(&esd_check_bycmdq, enable);
}

int primary_display_esd_cust_get(void)
{
	return atomic_read(&esd_check_bycmdq);
}

/* For Vdo Mode Read LCM Check */
/* Config cmdq_handle_config_esd */
int _esd_check_config_handle_vdo(void)
{
	int ret = 0;		/* 0:success , 1:fail */

	/* 1.reset */
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	primary_display_esd_cust_bycmdq(1);
	/* wait stream eof first */
	if (gEnableMutexRisingEdge == 1)
		cmdqRecWait(pgc->cmdq_handle_config_esd, CMDQ_EVENT_MUTEX0_STREAM_EOF);

	cmdqRecWait(pgc->cmdq_handle_config_esd, CMDQ_EVENT_MUTEX0_STREAM_EOF);

	/* disable SODI by CMDQ */
	if (gESDEnableSODI == 1)
		disp_set_sodi(0, pgc->cmdq_handle_config_esd);

	/* 2.stop dsi vdo mode */
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_STOP_VDO_MODE);

	/* 3.write instruction(read from lcm) */
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_ESD_CHECK_READ);

	/* 4.start dsi vdo mode */
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_START_VDO_MODE);

	/* 5. trigger path */
	dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_ENABLE);

	/* enable SODI by  CMDQ */
	if (gESDEnableSODI == 1)
		disp_set_sodi(1, pgc->cmdq_handle_config_esd);

	/* 6.flush instruction */
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);

	DISPCHECK("[ESD]_esd_check_config_handle_vdo ret=%d\n", ret);

/* done: */
	if (ret)
		ret = 1;
	primary_display_esd_cust_bycmdq(0);
	return ret;
}


/* ESD CHECK FUNCTION */
/* return 1: esd check fail */
/* return 0: esd check pass */
int primary_display_esd_check(void)
{
	int ret = 0;
#ifndef DISP_SUPPORT_CMDQ
	return 0;
#endif
	_primary_path_esd_check_lock();

	dprec_logger_start(DPREC_LOGGER_ESD_CHECK, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD check begin\n");

	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagPulse, 1, 0);
		DISPCHECK("[ESD]primary display path is slept?? -- skip esd check\n");
		_primary_path_unlock(__func__);
		goto done;
	}
	_primary_path_unlock(__func__);
#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_dsi_clock_on(0);
#endif
	/* Esd Check : EXT TE */
	if (pgc->plcm->params->dsi.customization_esd_check_enable == 0) {
		MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagStart, 0, 0);
		if (primary_display_is_video_mode()) {
			if (_need_register_eint()) {
				MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse,
					       1, 1);

				if (wait_event_interruptible_timeout
				    (esd_ext_te_wq, atomic_read(&esd_ext_te_event), HZ / 2) > 0) {
					ret = 0;	/* esd check pass */
				} else {
					ret = 1;	/* esd check fail */
				}
				atomic_set(&esd_ext_te_event, 0);
			}
		} else {
			MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse, 0, 1);
			if (dpmgr_wait_event_timeout
			    (pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ / 2) > 0) {
				ret = 0;	/* esd check pass */
			} else {
				ret = 1;	/* esd check fail */
			}
		}
		MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagEnd, 0, ret);
		goto done;
	}
	/* / Esd Check : Read from lcm */
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagStart, 0,
		       primary_display_cmdq_enabled());
	if (primary_display_cmdq_enabled()) {
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
		/* 0.create esd check cmdq */
		cmdqRecCreate(CMDQ_SCENARIO_DISP_ESD_CHECK, &(pgc->cmdq_handle_config_esd));
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,
				      CMDQ_ESD_ALLC_SLOT);
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);
		DISPCHECK("[ESD]ESD config thread=0x%p\n", pgc->cmdq_handle_config_esd);

		/* 1.use cmdq to read from lcm */
		if (primary_display_is_video_mode())
			ret = _esd_check_config_handle_vdo();
		else
			ret = _esd_check_config_handle_cmd();

		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse,
			       primary_display_is_video_mode(), 3);
		if (ret == 1) {	/* cmdq fail */
			if (_need_wait_esd_eof()) {
				/* Need set esd check eof synctoken to let trigger loop go. */
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
			}
			/* do dsi reset */
			dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,
					      CMDQ_DSI_RESET);
			goto destroy_cmdq;
		}

		DISPCHECK("[ESD]ESD config thread done~\n");

		/* 2.check data(*cpu check now) */
		ret =
		    dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,
					  CMDQ_ESD_CHECK_CMP);
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 4);
		if (ret)
			ret = 1;	/* esd check fail */


destroy_cmdq:
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,
				      CMDQ_ESD_FREE_SLOT);
		/* 3.destroy esd config thread */
		cmdqRecDestroy(pgc->cmdq_handle_config_esd);
		pgc->cmdq_handle_config_esd = NULL;

	} else {
		/* / 0: lock path */
		/* / 1: stop path */
		/* / 2: do esd check (!!!) */
		/* / 3: start path */
		/* / 4: unlock path */

		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
		_primary_path_lock(__func__);

		/* / 1: stop path */
		DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK("[ESD]primary display path is busy\n");
			ret =
			    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE,
						     HZ * 1);
			DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[ESD]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]stop dpmgr path[end]\n");

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK("[ESD]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE,
						 HZ * 1);
			DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[ESD]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]reset display path[end]\n");

		/* / 2: do esd check (!!!) */
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);

		if (primary_display_is_video_mode()) {
			/* ret = 0; */
			ret = disp_lcm_esd_check(pgc->plcm);
		} else {
			ret = disp_lcm_esd_check(pgc->plcm);
		}

		/* / 3: start path */
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse,
			       primary_display_is_video_mode(), 3);

		DISPCHECK("[ESD]start dpmgr path[begin]\n");
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[ESD]start dpmgr path[end]\n");

		DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

		_primary_path_unlock(__func__);
	}
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagEnd, 0, ret);

done:
	_primary_path_esd_check_unlock();
#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_dsi_clock_off(0);
#endif
	DISPCHECK("[ESD]ESD check end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagEnd, 0, ret);
	dprec_logger_done(DPREC_LOGGER_ESD_CHECK, 0, 0);
	return ret;

}

#if HDMI_MAIN_PATH
#else
/* For Vdo Mode EXT TE Check */
static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	MMProfileLogEx(ddp_mmp_get_events()->esd_vdo_eint, MMProfileFlagPulse, 0, 0);
	atomic_set(&esd_ext_te_event, 1);
	wake_up_interruptible(&esd_ext_te_wq);
	return IRQ_HANDLED;
}

static int primary_display_esd_check_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87 }; /* RTPM_PRIO_FB_THREAD */
	/* long int ttt = 0; */
	int ret = 0;
	int i = 0;
	int esd_try_cnt = 5;	/* 20; */

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	while (1) {
#if 0
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ);
		if (ret <= 0) {
			DISPERR("wait frame done timeout, reset whole path now\n");
			primary_display_diagnose();
			dprec_logger_trigger(DPREC_LOGGER_ESD_RECOVERY);
			dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		}
#else
		wait_event_interruptible(esd_check_task_wq, atomic_read(&esd_check_task_wakeup));
		msleep(2000);	/* esd check every 2s */
#ifdef DISP_SWITCH_DST_MODE
		_primary_path_switch_dst_lock();
#endif
#if 0
		{
			/* let's do a mutex holder check here */
			unsigned long long period = 0;

			period = dprec_logger_get_current_hold_period(DPREC_LOGGER_PRIMARY_MUTEX);
			if (period > 2000 * 1000 * 1000) {
				DISPERR("primary display mutex is hold by %s for %dns\n",
					pgc->mutex_locker, period);
			}
		}
#endif
		ret = primary_display_esd_check();
		if (ret == 1) {
			DISPCHECK("[ESD]esd check fail, will do esd recovery\n");
			i = esd_try_cnt;
			while (i--) {
				DISPCHECK("[ESD]esd recovery try:%d\n", i);
				primary_display_esd_recovery();
				ret = primary_display_esd_check();
				if (ret == 0) {
					DISPCHECK("[ESD]esd recovery success\n");
					break;
				}
				DISPCHECK
					("[ESD]after esd recovery, esd check still fail\n");
				if (i == 0) {
					DISPCHECK
						("[ESD]after esd recovery %d times, disable esd check\n",
						 esd_try_cnt);
					primary_display_esd_check_enable(0);
				}
			}
		}
#ifdef DISP_SWITCH_DST_MODE
		_primary_path_switch_dst_unlock();
#endif
#endif

		if (kthread_should_stop())
			break;
	}
	return 0;
}
#endif

extern int ddp_dsi_power_on(DISP_MODULE_ENUM module, void *cmdq_handle);

/* ESD RECOVERY */
static struct platform_device *pregulator;
int primary_display_esd_recovery(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;
	LCM_PARAMS *lcm_param = NULL;

	DISPFUNC();
#ifndef DISP_SUPPORT_CMDQ
	return 0;
#endif
	dprec_logger_start(DPREC_LOGGER_ESD_RECOVERY, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD recovery begin\n");
	_primary_path_lock(__func__);

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("[ESD]esd recovery but primary display path is slept??\n");
		goto done;
	}

	DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
	_cmdq_stop_trigger_loop();
	DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("[ESD]primary display path is busy\n");
		ret =
		    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	DISPCHECK("[ESD]stop dpmgr path[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("[ESD]primary display path is busy after stop\n");
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	DISPCHECK("[ESD]reset display path[begin]\n");
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]reset display path[end]\n");

	DISPCHECK("[ESD]lcm force init[begin]\n");
	disp_lcm_init(pregulator, pgc->plcm, 1);
	disp_lcm_suspend(pgc->plcm);
	disp_lcm_resume_power(pgc->plcm);
	ddp_dsi_power_on(32, NULL);
	disp_lcm_resume(pgc->plcm);
	DISPCHECK("[ESD]lcm force init[end]\n");

	DISPCHECK("[ESD]start dpmgr path[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPERR("[ESD]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		/* goto done; */
	}

	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}

	DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

done:
	_primary_path_unlock(__func__);
	DISPCHECK("[ESD]ESD recovery end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagEnd, 0, 0);
	dprec_logger_done(DPREC_LOGGER_ESD_RECOVERY, 0, 0);
	return ret;
}

void primary_display_esd_check_enable(int enable)
{
	if (_need_do_esd_check()) {
		if (_need_register_eint() && eint_flag != 2) {
			DISPCHECK("[ESD]Please check DCT setting about GPIO107/EINT107\n");
			return;
		}

		if (enable) {
			DISPCHECK("[ESD]esd check thread wakeup\n");
			atomic_set(&esd_check_task_wakeup, 1);
			wake_up_interruptible(&esd_check_task_wq);
		} else {
			DISPCHECK("[ESD]esd check thread stop\n");
			atomic_set(&esd_check_task_wakeup, 0);
		}
	}
}
static void primary_display_register_eint(void)
{
	unsigned long node;
	int irq;
	u32 ints[2] = { 0, 0 };

#ifdef GPIO_DSI_TE_PIN
	/* 1.set GPIO107 eint mode */
	mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_GPIO);
	eint_flag++;
#endif

	/* 2.register eint */
	node = (unsigned long)of_find_compatible_node(NULL, NULL, "mediatek, DSI_TE_1-eint");
	if (node) {
		of_property_read_u32_array((struct device_node *)node, "debounce", ints,
					   ARRAY_SIZE(ints));
		/* mt_gpio_set_debounce(ints[0], ints[1]); */

		irq = irq_of_parse_and_map((struct device_node *)node, 0);
		if (request_irq
		    (irq, _esd_check_ext_te_irq_handler, IRQF_TRIGGER_NONE, "DSI_TE_1-eint",
		     NULL)) {
			DISPCHECK("[ESD]EINT IRQ LINE NOT AVAILABLE!!\n");
		} else {
			eint_flag++;
		}
	} else {
		DISPCHECK("[ESD][%s] can't find DSI_TE_1 eint compatible node\n", __func__);
	}
}

/******************************************************************************/
/* ESD CHECK / RECOVERY ---- End                                              */
/******************************************************************************/
#define EEEEEEEEEEEEEEEEEEEEEEEEEE
static struct task_struct *primary_path_aal_task;
unsigned int gDecouplePQWithRDMA = 1;

static int _disp_primary_path_check_trigger(void *data)
{
	int ret = 0;
#if 0				/* ndef CONFIG_MTK_FPGA  //monica porting asek weiqing */
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		DISPMSG("Force Trigger Display Path\n");
		primary_display_trigger(1, NULL, 0);

		if (kthread_should_stop())
			break;
	}

#else
	cmdqRecHandle handle = NULL;
#ifndef DISP_SUPPORT_CMDQ
		return 0;
#endif

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		MMProfileLogEx(ddp_mmp_get_events()->primary_display_aalod_trigger,
			       MMProfileFlagPulse, 0, 0);

		_primary_path_lock(__func__);

		if (pgc->state != DISP_SLEPT) {
#ifdef MTK_DISP_IDLE_LP
			if (gDecouplePQWithRDMA == 0) {
				last_primary_trigger_time = sched_clock();
				_disp_primary_path_exit_idle(__func__, 0);
			}
#endif
			cmdqRecReset(handle);
			_cmdq_insert_wait_frame_done_token_mira(handle);
			_cmdq_set_config_handle_dirty_mira(handle);
			_cmdq_flush_config_handle_mira(handle, 0);
		}

		_primary_path_unlock(__func__);

		if (kthread_should_stop())
			break;
	}

	cmdqRecDestroy(handle);
#endif

	return 0;
}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO

/* need remove */
unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
	/* DISP_LOG_I("cmdqDdpClockOff\n"); */
	return 0;
}

unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
	/* DISP_LOG_I("cmdqDdpClockOff\n"); */
	return 0;
}

unsigned int cmdqDdpDumpInfo(uint64_t engineFlag, char *pOutBuf, unsigned int bufSize)
{
	DDPDUMP("cmdq timeout:%llu\n", engineFlag);
	primary_display_diagnose();
	/* DISP_LOG_I("cmdqDdpDumpInfo\n"); */
	if (primary_display_is_decouple_mode()) {
		ddp_dump_analysis(DISP_MODULE_OVL0);
		ddp_dump_analysis(DISP_MODULE_OVL1);
	}
	return 0;
}

unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
	/* DISP_LOG_I("cmdqDdpResetEng\n"); */
	return 0;
}

unsigned int display_path_idle_cnt = 0;
static void _RDMA0_INTERNAL_IRQ_Handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (!_is_decouple_mode(pgc->session_mode) && param & 0x2) {
		/* RDMA Start */
		display_path_idle_cnt++;
	#ifdef DISP_ENABLE_SODI
		spm_sodi_mempll_pwr_mode(1);
	#endif
	}
	if (param & 0x4) {
		/* RDMA Done */
		display_path_idle_cnt--;
		if (display_path_idle_cnt == 0) {
		#ifdef DISP_ENABLE_SODI
			spm_sodi_mempll_pwr_mode(0);
		#endif
		}
	}
}

static void _WDMA0_INTERNAL_IRQ_Handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (param & 0x1) {
		/* WDMA Done */
		display_path_idle_cnt--;
		if (display_path_idle_cnt == 0) {
		#ifdef DISP_ENABLE_SODI
			spm_sodi_mempll_pwr_mode(0);
		#endif
		}
	}
}

static void _MUTEX_INTERNAL_IRQ_Handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (param & 0x1) {
		/* RDMA-->DSI SOF */
		display_path_idle_cnt++;

#ifdef DISP_ENABLE_SODI
		spm_sodi_mempll_pwr_mode(1);
#endif
	}
	if (param & 0x2) {
		/* OVL->WDMA SOF */
		display_path_idle_cnt++;

#ifdef DISP_ENABLE_SODI
		spm_sodi_mempll_pwr_mode(1);
#endif
	}
}

void primary_display_sodi_rule_init(void)
{
	if (gEnableSODIControl == 0 && primary_display_is_video_mode() == 1) {
	#ifdef DISP_ENABLE_SODI
		spm_enable_sodi(0);
	#endif
		DISPMSG("SODI disabled!\n");
		return;
	}

	DISPMSG("SODI enabled!\n");
#ifdef DISP_ENABLE_SODI
	spm_enable_sodi(1);
#endif
	if (primary_display_is_video_mode()) {
		/* if switch to video mode, should de-register callback */
		disp_unregister_module_irq_callback(DISP_MODULE_RDMA0, _RDMA0_INTERNAL_IRQ_Handler);

#ifdef DISP_ENABLE_SODI
		spm_sodi_mempll_pwr_mode(1);
#endif
	} else {
		disp_register_module_irq_callback(DISP_MODULE_RDMA0, _RDMA0_INTERNAL_IRQ_Handler);
		if (_is_decouple_mode(pgc->session_mode)) {
			disp_register_module_irq_callback(DISP_MODULE_MUTEX,
							  _MUTEX_INTERNAL_IRQ_Handler);
			disp_register_module_irq_callback(DISP_MODULE_WDMA0,
							  _WDMA0_INTERNAL_IRQ_Handler);
		}
	}
}

#if 0
#define DISP_REG_DRAM_SELF_REFRESH_A 0x10004004
#define DISP_REG_DRAM_SELF_REFRESH_B 0x10011004
/*enable/disable DRAM self-refresh by dram register*/
static void disp_sodi_set_dram_self_refresh(unsigned int enable, void *cmdq_handle)
{
	static int is_drma_reg_map;
	static unsigned long reg_va_a, reg_va_b;

	if (is_drma_reg_map == 0) {
		reg_va_a =
		    (unsigned long)ioremap_nocache(DISP_REG_DRAM_SELF_REFRESH_A,
						   sizeof(unsigned long));
		reg_va_b =
		    (unsigned long)ioremap_nocache(DISP_REG_DRAM_SELF_REFRESH_B,
						   sizeof(unsigned long));
		is_drma_reg_map = 1;
	}

	/*iounmap((void *)reg_va); */

	if (gEnableSODIControl == 1) {
		if (cmdq_handle != NULL) {
			if (enable == 1) {
				cmdqRecWrite(cmdq_handle, DISP_REG_DRAM_SELF_REFRESH_A, 1 << 26,
					     1 << 26);
				cmdqRecWrite(cmdq_handle, DISP_REG_DRAM_SELF_REFRESH_B, 1 << 26,
					     1 << 26);
			} else {
				cmdqRecWrite(cmdq_handle, DISP_REG_DRAM_SELF_REFRESH_A, 0, 1 << 26);
				cmdqRecWrite(cmdq_handle, DISP_REG_DRAM_SELF_REFRESH_B, 0, 1 << 26);
			}
		} else {
			if (enable == 1) {
				DISP_CPU_REG_SET_FIELD(1 << 26, reg_va_a, 1);
				DISP_CPU_REG_SET_FIELD(1 << 26, reg_va_b, 1);
			} else {
				DISP_CPU_REG_SET_FIELD(1 << 26, reg_va_a, 0);
				DISP_CPU_REG_SET_FIELD(1 << 26, reg_va_b, 0);
			}
		}
	}
}
#endif

#define DISP_REG_SODI_PA 0x10006b0c
/*enable/disable DRAM self-refresh by spm interface*/
static void disp_set_sodi(unsigned int enable, void *cmdq_handle)
{
/*wrokround for kernel 3.18*/
#ifdef DISP_ENABLE_SODI
	if (gEnableSODIControl == 1) {

		if (cmdq_handle != NULL) {
			if (enable == 1)
				cmdqRecWrite(cmdq_handle, DISP_REG_SODI_PA, 0, 1);
			else
				cmdqRecWrite(cmdq_handle, DISP_REG_SODI_PA, 1, 1);
		} else {
			if (enable == 1)
				DISP_REG_SET(0, (unsigned long)SPM_PCM_SRC_REQ,
					     DISP_REG_GET(SPM_PCM_SRC_REQ) & (~0x1));
			else
				DISP_REG_SET(0, (unsigned long)SPM_PCM_SRC_REQ,
					     DISP_REG_GET(SPM_PCM_SRC_REQ) | 0x1);
		}

	}
#endif
}



int primary_display_change_lcm_resolution(unsigned int width, unsigned int height)
{
	if (pgc->plcm) {
		DISPMSG("LCM Resolution will be changed, original: %dx%d, now: %dx%d\n",
			pgc->plcm->params->width, pgc->plcm->params->height, width, height);
		/* align with 4 is the minimal check, to ensure we can boot up into kernel,
		   and could modify dfo setting again using meta tool */
		/* otherwise we will have a panic in lk(root cause unknown). */
		if (width > pgc->plcm->params->width || height > pgc->plcm->params->height
		    || width == 0 || height == 0 || width % 4 || height % 4) {
			DISPERR("Invalid resolution: %dx%d\n", width, height);
			return -1;
		}

		if (primary_display_is_video_mode()) {
			DISPERR("Warning!!!Video Mode can't support multiple resolution!\n");
			return -1;
		}

		pgc->plcm->params->width = width;
		pgc->plcm->params->height = height;

		return 0;
	} else {
		return -1;
	}
}


/* use callback to release fence, will be removed */
#if 0
static struct task_struct *fence_release_worker_task;

static struct task_struct *if_fence_release_worker_task;

static int _if_fence_release_worker_thread(void *data)
{
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		if (_is_mirror_mode(pgc->session_mode)) {
			int fence_idx, subtractor, layer;

			layer = disp_sync_get_output_interface_timeline_id();

			cmdqBackupReadSlot(pgc->cur_config_fence, layer, &fence_idx);
			cmdqBackupReadSlot(pgc->subtractor_when_free, layer, &subtractor);
			mtkfb_release_fence(session_id, layer, fence_idx - 1);
		}
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static struct task_struct *ovl2mem_fence_release_worker_task;

static int _ovl2mem_fence_release_worker_thread(void *data)
{
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);

	while (1) {
		/* it's not good to use FRAME_COMPLETE here,
		   because when CPU read wdma addr, maybe it's already changed by next request */
		/* but luckly currently we will wait rdma frame done after wdma done(in CMDQ),
		   so it's safe now */
		dpmgr_wait_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);
		if (_is_mirror_mode(pgc->session_mode)) {
			int fence_idx, subtractor, layer;

			layer = disp_sync_get_output_timeline_id();

			cmdqBackupReadSlot(pgc->cur_config_fence, layer, &fence_idx);
			cmdqBackupReadSlot(pgc->subtractor_when_free, layer, &subtractor);
			mtkfb_release_fence(session_id, layer, fence_idx);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int _fence_release_worker_thread(void *data)
{
	/* int ret = 0; */
	int i = 0;
	/* unsigned int addr = 0; */
	int fence_idx = -1;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
	while (1) {
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

		for (i = 0; i < PRIMARY_DISPLAY_SESSION_LAYER_COUNT; i++) {
			/* addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), i); */
			if (i == primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled()) {
				mtkfb_release_layer_fence(session_id, 3);
			} else {
#if 0
/*
				fence_idx = disp_sync_find_fence_idx_by_addr(session_id, i, addr);
				if (fence_idx < 0) {
					if (fence_idx == -1) {
						DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail,"
							"unregistered addr%d\n", i, addr, fence_idx);
					} else if (fence_idx == -2) {

					} else {
						DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail,"
							"reason unknown%d\n", i, addr, fence_idx);
					}
				} else {
					mtkfb_release_fence(session_id, i, fence_idx);
				}
*/
#else
				int subtractor;

				cmdqBackupReadSlot(pgc->cur_config_fence, i, &fence_idx);
				cmdqBackupReadSlot(pgc->subtractor_when_free, i, &subtractor);
				mtkfb_release_fence(session_id, i, fence_idx - subtractor);
#endif
			}
		}

		MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagEnd, 0, 0);

		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static struct task_struct *present_fence_release_worker_task;

static int _present_fence_release_worker_thread(void *data)
{
	int ret = 0;
	struct sched_param param = {.sched_priority = 87 }; /* RTPM_PRIO_FB_THREAD */

	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
#ifndef CONFIG_MTK_SYNC
	return 0;
#endif
	while (1) {
		if (pgc->state == DISP_SLEPT)
			ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
		else
			ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ * 1);
		/*  release present fence in vsync callback */
		if (ret > 0) {
			int fence_increment = 0;
			disp_sync_info *layer_info =
			    _get_sync_info(MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0),
					   disp_sync_get_present_timeline_id());
			if (layer_info == NULL) {
				DISPERR("_get_sync_info fail in present_fence_release thread\n");
				continue;
			}

			_primary_path_lock(__func__);
			fence_increment = gPresentFenceIndex - layer_info->timeline->value;
			if (fence_increment > 0) {
			#ifdef CONFIG_MTK_SYNC
				timeline_inc(layer_info->timeline, fence_increment);
			#endif
				MMProfileLogEx(ddp_mmp_get_events()->present_fence_release,
					       MMProfileFlagPulse, gPresentFenceIndex,
					       fence_increment);
				DDPDBG("RPF/%d/%d\n", gPresentFenceIndex,
				     gPresentFenceIndex - layer_info->timeline->value);
			}
			if (pgc->trigger_cnt > 10)
				DISPMSG("trigger cnt(%d)\n", pgc->trigger_cnt);
			pgc->trigger_cnt = 0;
			_primary_path_unlock(__func__);
			DISP_PRINTF(DDP_FENCE1_LOG, "RPF/%d/%d\n", gPresentFenceIndex,
				     gPresentFenceIndex - layer_info->timeline->value);
		} else if ((0 == ret) && (pgc->state != DISP_SLEPT)) {
			DISPMSG("%s wait vsync timeout\n", __func__);
		}
	}
	return 0;
}

int primary_display_set_frame_buffer_address(unsigned long va, unsigned long mva)
{

	DISPMSG("framebuffer va %lu, mva %lu\n", va, mva);
	pgc->framebuffer_va = va;
	pgc->framebuffer_mva = mva;
/*
    int frame_buffer_size = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) *
		      ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;
    unsigned long dim_layer_va = va + 2*frame_buffer_size;
    dim_layer_mva = mva + 2*frame_buffer_size;
    memset(dim_layer_va, 0, frame_buffer_size);
*/
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

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
#ifdef DISP_SUPPORT_CMDQ
	int i;

	cmdqBackupAllocateSlot(pSlot, count);
	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);
#endif
	return 0;
}

static void primary_display_frame_update_irq_callback(DISP_MODULE_ENUM module, unsigned int param)
{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* In TEE, we have to protect WDMA registers, so we can't enable WDMA interrupt */
	/* here we use ovl frame done interrupt instead */
	if ((module == DISP_MODULE_OVL0) && (_is_decouple_mode(pgc->session_mode) == 1)) {
		/* OVL0 frame done */
		if (param & 0x2) {
			atomic_set(&decouple_fence_release_event, 1);
			wake_up_interruptible(&decouple_fence_release_wq);
		}
	}
#else
	if ((module == DISP_MODULE_WDMA0) && (_is_decouple_mode(pgc->session_mode) == 1)) {
		/* wdma0 frame done */
		if (param & 0x1) {
			atomic_set(&decouple_fence_release_event, 1);
			wake_up_interruptible(&decouple_fence_release_wq);
		}
	}
#endif
}

static int _config_rdma_input_data(RDMA_CONFIG_STRUCT *rdma_config,
				   disp_path_handle disp_handle, cmdqRecHandle cmdq_handle)
{
	disp_ddp_path_config *pconfig = dpmgr_path_get_last_config(disp_handle);

	pconfig->rdma_config = *rdma_config;
	pconfig->rdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);

	return 0;
}

static void _Interface_fence_release_callback(uint32_t userdata)
{
	int layer = disp_sync_get_output_interface_timeline_id();

	if (userdata > 0) {
		mtkfb_release_fence(primary_session_id, layer, userdata);
		MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_fence_release, MMProfileFlagPulse,
			       layer, userdata);
	}
}

static int decouple_fence_release_kthread(void *data)
{
	int interface_fence = 0;
	int layer = 0;
	int ret = 0;

	struct sched_param param = {.sched_priority = 94 }; /* RTPM_PRIO_SCRN_UPDATE */

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(decouple_fence_release_wq,
					 atomic_read(&decouple_fence_release_event));
		atomic_set(&decouple_fence_release_event, 0);

		/* async callback,need to check if it is still decouple */
		_primary_path_lock(__func__);
		if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
			static cmdqRecHandle cmdq_handle;
			unsigned int rdma_pitch_sec;

			if (cmdq_handle == NULL)
				ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
			if (ret == 0) {
				RDMA_CONFIG_STRUCT tmpConfig = decouple_rdma_config;

				cmdqRecReset(cmdq_handle);
				_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
				cmdqBackupReadSlot(pgc->rdma_buff_info, 0,
						   (uint32_t *) &(tmpConfig.address));

				/*rdma pitch only use bit[15..0], we use bit[31:30] to store secure information */
				cmdqBackupReadSlot(pgc->rdma_buff_info, 1, &(rdma_pitch_sec));
				tmpConfig.pitch = rdma_pitch_sec & ~(3 << 30);
				tmpConfig.security = rdma_pitch_sec >> 30;

				_config_rdma_input_data(&tmpConfig, pgc->dpmgr_handle, cmdq_handle);

				layer = disp_sync_get_output_timeline_id();
				cmdqBackupReadSlot(pgc->cur_config_fence, layer, &interface_fence);
				_cmdq_set_config_handle_dirty_mira(cmdq_handle);
				cmdqRecFlushAsyncCallback(cmdq_handle, (CmdqAsyncFlushCB)
							  _Interface_fence_release_callback,
							  interface_fence >
							  1 ? interface_fence - 1 : 0);
				MMProfileLogEx(ddp_mmp_get_events()->primary_rdma_config,
					       MMProfileFlagPulse, interface_fence,
					       decouple_rdma_config.address);

				/* dump rdma input if enabled */
				/* dprec_mmp_dump_rdma_layer(&tmpConfig, 0); */

				/* cmdqRecDestroy(cmdq_handle); */
			} else {
				DISPERR("fail to create cmdq\n");
			}
		}
		_primary_path_unlock(__func__);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

#define xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
int primary_display_init(struct platform_device *dev, char *lcm_name, unsigned int lcm_fps)
{
	DISP_STATUS ret = DISP_STATUS_OK;
#if HDMI_MAIN_PATH
#else
	LCM_PARAMS *lcm_param = NULL;
#endif
	disp_ddp_path_config *data_config = NULL;

	DISPFUNC();
	pregulator = dev;
	dprec_init();
	/* xuecheng, for debug */
	/* dprec_handle_option(0x3); */
	disp_sync_init();
	dpmgr_init();
	init_cmdq_slots(&(pgc->cur_config_fence), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->subtractor_when_free), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->rdma_buff_info), 2, 0);
	init_cmdq_slots(&(pgc->ovl_status_info), 2, 0);
	mutex_init(&(pgc->capture_lock));
	mutex_init(&(pgc->lock));
#ifdef MTK_DISP_IDLE_LP
	mutex_init(&idle_lock);
#endif
#ifdef DISP_SWITCH_DST_MODE
	mutex_init(&(pgc->switch_dst_lock));
#endif
	mutex_init(&esd_mode_switch_lock);

	_primary_path_lock(__func__);

#if HDMI_MAIN_PATH
#else
	pgc->plcm = disp_lcm_probe(lcm_name, LCM_INTERFACE_NOTDEFINED);

	if (pgc->plcm == NULL) {
		DISPCHECK("disp_lcm_probe returns null\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("disp_lcm_probe SUCCESS\n");
	}

#ifndef MTK_FB_DFO_DISABLE
	if ((0 == dfo_query("LCM_FAKE_WIDTH", (unsigned long *)&lcm_fake_width))
	    && (0 == dfo_query("LCM_FAKE_HEIGHT", (unsigned long *)&lcm_fake_height))) {
		DDPDBG("[DFO] LCM_FAKE_WIDTH=%d, LCM_FAKE_HEIGHT=%d\n", lcm_fake_width,
		       lcm_fake_height);
		if (lcm_fake_width && lcm_fake_height) {
			if (DISP_STATUS_OK !=
			    primary_display_change_lcm_resolution(lcm_fake_width,
								  lcm_fake_height)) {
				DDPDBG("[DISP]WARNING!!! Change LCM Resolution FAILED!!!\n");
			}
		}
	}
#endif

	lcm_param = disp_lcm_get_params(pgc->plcm);

	if (lcm_param == NULL) {
		DISPERR("get lcm params FAILED\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	}
#endif
#ifdef DISP_SUPPORT_CMDQ
	ret =
	    cmdqCoreRegisterCB(CMDQ_GROUP_DISP, (CmdqClockOnCB) cmdqDdpClockOn,
			       (CmdqDumpInfoCB) cmdqDdpDumpInfo, (CmdqResetEngCB) cmdqDdpResetEng,
			       (CmdqClockOffCB) cmdqDdpClockOff);
	if (ret) {
		DISPERR("cmdqCoreRegisterCB failed, ret=%d\n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &(pgc->cmdq_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_handle=0x%p\n", pgc->cmdq_handle_config);
	}

#if 0
	/*create ovl2mem path cmdq handle */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT, &(pgc->cmdq_handle_ovl1to2_config));
#else
	/* for direct link mirror mode */
	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &(pgc->cmdq_handle_ovl1to2_config));
#endif

	if (ret != 0) {
		DISPERR("cmdqRecCreate FAIL, ret=%d\n", ret);
		return -1;
	}
#endif
#if defined(MTK_ALPS_BOX_SUPPORT)
	DISPMSG("don't allocate primary decouple normal/secure buffer in box!\n");
#else
	/* will use decouple mode in idle */
	init_decouple_buffers();
#endif

	if (primary_display_mode == DIRECT_LINK_MODE) {
		_build_path_direct_link();
		pgc->session_mode = DISP_SESSION_DIRECT_LINK_MODE;

		DISPCHECK("primary display is DIRECT LINK MODE\n");
	} else if (primary_display_mode == DECOUPLE_MODE) {
		_build_path_decouple();
		pgc->session_mode = DISP_SESSION_DECOUPLE_MODE;

		DISPCHECK("primary display is DECOUPLE MODE\n");
	} else if (primary_display_mode == SINGLE_LAYER_MODE) {
		_build_path_single_layer();

		DISPCHECK("primary display is SINGLE LAYER MODE\n");
	} else if (primary_display_mode == DEBUG_RDMA1_DSI0_MODE) {
		_build_path_debug_rdma1_dsi0();

		DISPCHECK("primary display is DEBUG RDMA1 DSI0 MODE\n");
	} else if (primary_display_mode == DEBUG_RDMA2_DPI0_MODE) {
		_build_path_debug_rdma2_dpi0();

		DISPCHECK("primary display is DEBUG RDMA1 DSI0 MODE\n");
	} else {
		DISPCHECK("primary display mode is WRONG\n");
	}

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());
#ifdef DISP_SUPPORT_CMDQ
	primary_display_use_cmdq = CMDQ_ENABLE;
#else
	primary_display_use_cmdq = CMDQ_DISABLE;
#endif
	if (primary_display_use_cmdq == CMDQ_ENABLE) {
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}

	dpmgr_path_init(pgc->dpmgr_handle, primary_display_use_cmdq);
#ifdef DISP_SUPPORT_CMDQ
	/* need after dpmgr_path_init for dual-dsi command mode panel */
	_cmdq_build_trigger_loop();
	_cmdq_start_trigger_loop();
#endif
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

#if HDMI_MAIN_PATH
	data_config->dst_w = HDMI_DISP_WIDTH;
	data_config->dst_h = HDMI_DISP_HEIGHT;
	data_config->lcm_bpp = 24;
#else
	memcpy(&(data_config->dispif_config), lcm_param, sizeof(LCM_PARAMS));

	data_config->dst_w = lcm_param->width;
	data_config->dst_h = lcm_param->height;
	if (lcm_param->type == LCM_TYPE_DSI) {
		if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	} else if (lcm_param->type == LCM_TYPE_DPI) {
		if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	}
#endif
	data_config->fps = lcm_fps;
	data_config->dst_dirty = 1;

	if (primary_display_use_cmdq == CMDQ_ENABLE) {
		/*Use cmdq to update config at kernel firstly */
		dpmgr_path_config(pgc->dpmgr_handle, data_config, pgc->cmdq_handle_config);
		_cmdq_flush_config_handle(1, NULL, 0);

		/*For next config */
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	} else {
		/*For bringup */
		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		ret = dpmgr_path_start(pgc->dpmgr_handle, 0);
		ret = dpmgr_path_flush(pgc->dpmgr_handle, 0);
	}

#if HDMI_MAIN_PATH
#else
	ret = disp_lcm_init(dev, pgc->plcm, 0);

	primary_display_esd_check_task =
	    kthread_create(primary_display_esd_check_worker_kthread, NULL, "display_esd_check");
	init_waitqueue_head(&esd_ext_te_wq);
	init_waitqueue_head(&esd_check_task_wq);
	if (_need_do_esd_check()) {
		wake_up_process(primary_display_esd_check_task);
		/* primary_display_esd_check_enable(1); */
	}

	if (_need_register_eint())
		primary_display_register_eint();

	if (_need_do_esd_check())
		primary_display_esd_check_enable(1);
#endif

#ifdef DISP_SWITCH_DST_MODE
	primary_display_switch_dst_mode_task =
	    kthread_create(_disp_primary_path_switch_dst_mode_thread, NULL,
			   "display_switch_dst_mode");
	wake_up_process(primary_display_switch_dst_mode_task);
#endif

	primary_path_aal_task =
	    kthread_create(_disp_primary_path_check_trigger, NULL, "display_check_aal");
	wake_up_process(primary_path_aal_task);

	/* use callback to release fence, will be removed */
#if 0
	/* Zaikuo: disable it when HWC not enable to reduce error log */
	fence_release_worker_task =
	    kthread_create(_fence_release_worker_thread, NULL, "fence_worker");
	wake_up_process(fence_release_worker_task);

	if (_is_decouple_mode(pgc->session_mode)) {
		if_fence_release_worker_task =
		    kthread_create(_if_fence_release_worker_thread, NULL, "if_fence_worker");
		wake_up_process(if_fence_release_worker_task);

		ovl2mem_fence_release_worker_task =
		    kthread_create(_ovl2mem_fence_release_worker_thread, NULL,
				   "ovl2mem_fence_worker");
		wake_up_process(ovl2mem_fence_release_worker_task);
	}
#endif

	present_fence_release_worker_task =
	    kthread_create(_present_fence_release_worker_thread, NULL, "present_fence_worker");
	wake_up_process(present_fence_release_worker_task);

	if (decouple_fence_release_task == NULL) {
		disp_register_module_irq_callback(DISP_MODULE_OVL0,
						  primary_display_frame_update_irq_callback);
		disp_register_module_irq_callback(DISP_MODULE_WDMA0,
						  primary_display_frame_update_irq_callback);
		init_waitqueue_head(&decouple_fence_release_wq);
		decouple_fence_release_task = kthread_create(decouple_fence_release_kthread,
							     NULL, "decouple_fence_release_worker");
		wake_up_process(decouple_fence_release_task);
	}

	/* this will be set to always enable cmdq later */
	if (primary_display_is_video_mode()) {
#ifdef DISP_SWITCH_DST_MODE
		primary_display_cur_dst_mode = 1;	/* video mode */
		primary_display_def_dst_mode = 1;	/* default mode is video mode */
#endif
#if HDMI_MAIN_PATH
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_START);
#else
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_DONE);
#endif
	} else {

	}

	pgc->lcm_fps = lcm_fps;
	if (lcm_fps > 6000)
		pgc->max_layer = 4;
	else
		pgc->max_layer = 4;

	pgc->state = DISP_ALIVE;

	primary_display_sodi_rule_init();

#ifdef MTK_DISP_IDLE_LP
	init_waitqueue_head(&idle_detect_wq);
	primary_display_idle_detect_task =
	    kthread_create(_disp_primary_path_idle_detect_thread, NULL, "display_idle_detect");
	disp_update_trigger_time();
	wake_up_process(primary_display_idle_detect_task);
#endif

#if defined(MTK_ALPS_BOX_SUPPORT)
#else
	/* if clock default is on, need enable/disable to close */
	dpmgr_path_spare_module_clock_off();
#endif

done:
	_primary_path_unlock(__func__);
	DISPCHECK("primary_display_init end\n");

	return ret;
}

int primary_display_deinit(void)
{
	_primary_path_lock(__func__);

	_cmdq_stop_trigger_loop();
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);

	_primary_path_unlock(__func__);

	return 0;
}

/* register rdma done event */
int primary_display_wait_for_idle(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);

/* done: */
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_wait_for_dump(void)
{
	return 0;
}

int primary_display_wait_for_vsync(void *config)
{
	disp_session_vsync_config *c = (disp_session_vsync_config *) config;
	int ret = 0;

#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_dsi_clock_on(0);
#endif

	ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	if (ret == -2)
		DISPCHECK("vsync for primary display path not enabled yet\n");


	if (pgc->vsync_drop)
		ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

	/* DISPMSG("vsync signaled\n"); */
	c->vsync_ts = ktime_to_ns(ktime_get());
	c->vsync_cnt++;

#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_dsi_clock_off(0);
#endif

	return ret;
}

unsigned int primary_display_get_ticket(void)
{
	return dprec_get_vsync_count();
}

int primary_suspend_release_fence(void)
{
	unsigned int session = (unsigned int)((DISP_SESSION_PRIMARY) << 16 | (0));
	unsigned int i = 0;

	for (i = 0; i < HW_OVERLAY_COUNT; i++) {
		DISPMSG("mtkfb_release_layer_fence  session=0x%x,layerid=%d\n", session, i);
		mtkfb_release_layer_fence(session, i);
	}
	return 0;
}

int primary_display_suspend(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;
	int event_ret;

	DISPFUNC();

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	primary_display_switch_dst_mode(primary_display_def_dst_mode);
#endif
	disp_sw_mutex_lock(&(pgc->capture_lock));
	_primary_path_esd_check_lock();
	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("primary display path is already sleep, skip\n");
		goto done;
	}
#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_exit_idle(__func__, 0);
#endif

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 1);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 1, 2);
		event_ret =
		    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 2, 2);
		DISPCHECK
		    ("[POWER]primary display path is busy now, wait frame done, event_ret=%d\n",
		     event_ret);
		if (event_ret <= 0) {
			DISPERR("wait frame done in suspend timeout\n");
			MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 3,
				       2);
			primary_display_diagnose();
			ret = -1;
		}
	}

	if (_is_decouple_mode(pgc->session_mode)) {
		if (dpmgr_path_is_busy(pgc->ovl2mem_path_handle)) {
			dpmgr_wait_event_timeout(pgc->ovl2mem_path_handle,
						 DISP_PATH_EVENT_FRAME_COMPLETE, HZ);
		}
		/* xuecheng, BAD WROKAROUND for decouple mode */
		msleep(30);
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 2);

	DISPCHECK("[POWER]display cmdq trigger loop stop[begin]\n");
	_cmdq_stop_trigger_loop();
	DISPCHECK("[POWER]display cmdq trigger loop stop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]primary display path stop[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]primary display path stop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 4);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 1, 4);
		DISPERR("[POWER]stop display path failed, still busy\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		ret = -1;
		/* even path is busy(stop fail), we still need to continue power off other module/devices */
		/* goto done; */
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 5);

#if HDMI_MAIN_PATH
#else
	DISPCHECK("[POWER]lcm suspend[begin]\n");
	disp_lcm_suspend(pgc->plcm);
	DISPCHECK("[POWER]lcm suspend[end]\n");
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 6);
	DISPCHECK("[POWER]primary display path Release Fence[begin]\n");
	primary_suspend_release_fence();
	DISPCHECK("[POWER]primary display path Release Fence[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 7);

	DISPCHECK("[POWER]dpmanager path power off[begin]\n");
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]dpmanager path power off[end]\n");

	if (_is_decouple_mode(pgc->session_mode))
		dpmgr_path_power_off(pgc->ovl2mem_path_handle, CMDQ_DISABLE);


	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 8);

	pgc->state = DISP_SLEPT;
done:
	_primary_path_unlock(__func__);
	_primary_path_esd_check_unlock();
	disp_sw_mutex_unlock(&(pgc->capture_lock));
#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	aee_kernel_wdt_kick_Powkey_api("mtkfb_early_suspend", WDT_SETBY_Display);
#endif
	primary_trigger_cnt = 0;
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagEnd, 0, 0);
	return ret;
}

int primary_display_resume(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;
#if HDMI_MAIN_PATH
#else
	LCM_PARAMS *lcm_param;
#endif
	disp_ddp_path_config *data_config;

	DISPFUNC();
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagStart, 0, 0);

	_primary_path_lock(__func__);
	if (pgc->state == DISP_ALIVE) {
		DISPCHECK("primary display path is already resume, skip\n");
		goto done;
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 1);

	disp_lcm_resume_power(pgc->plcm);

	DISPCHECK("dpmanager path power on[begin]\n");
	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("dpmanager path power on[end]\n");

	if (_is_decouple_mode(pgc->session_mode))
		dpmgr_path_power_on(pgc->ovl2mem_path_handle, CMDQ_DISABLE);


	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 2);
	if (is_ipoh_bootup) {
		DISPCHECK("[primary display path] leave primary_display_resume -- IPOH\n");
		is_ipoh_bootup = false;
		DISPCHECK("[POWER]start cmdq[begin]--IPOH\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[POWER]start cmdq[end]--IPOH\n");
		pgc->state = DISP_ALIVE;
		goto done;
	}
	DISPCHECK("[POWER]dpmanager re-init[begin]\n");

	{
		dpmgr_path_connect(pgc->dpmgr_handle, CMDQ_DISABLE);
		if (_is_decouple_mode(pgc->session_mode))
			dpmgr_path_connect(pgc->ovl2mem_path_handle, CMDQ_DISABLE);

		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 2);
#if HDMI_MAIN_PATH
#else
		lcm_param = disp_lcm_get_params(pgc->plcm);
#endif

		data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

#if HDMI_MAIN_PATH
		data_config->dst_w = HDMI_DISP_WIDTH;
		data_config->dst_h = HDMI_DISP_HEIGHT;
		data_config->lcm_bpp = 24;
#else
		data_config->dst_w = lcm_param->width;
		data_config->dst_h = lcm_param->height;
		if (lcm_param->type == LCM_TYPE_DSI) {
			if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		} else if (lcm_param->type == LCM_TYPE_DPI) {
			if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
				data_config->lcm_bpp = 24;
			else if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
				data_config->lcm_bpp = 16;
			if (lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
				data_config->lcm_bpp = 18;
		}
#endif

		data_config->fps = pgc->lcm_fps;
		data_config->dst_dirty = 1;

		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 2, 2);
		if (_is_decouple_mode(pgc->session_mode)) {
			data_config = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);

			data_config->fps = pgc->lcm_fps;
			data_config->dst_dirty = 1;
			data_config->wdma_dirty = 1;

			ret = dpmgr_path_config(pgc->ovl2mem_path_handle, data_config, NULL);
			MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 2,
				       2);
		}
		data_config->dst_dirty = 0;
	}
	DISPCHECK("[POWER]dpmanager re-init[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]lcm resume[begin]\n");
	disp_lcm_resume(pgc->plcm);
	DISPCHECK("[POWER]lcm resume[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 4);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 4);
		DISPERR("[POWER]Fatal error, we didn't start display path but it's already busy\n");
		ret = -1;
		/* goto done; */
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 5);
	DISPCHECK("[POWER]dpmgr path start[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (_is_decouple_mode(pgc->session_mode))
		dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_DISABLE);

	DISPCHECK("[POWER]dpmgr path start[end]\n");

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 6);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 6);
		DISPERR
		    ("[POWER]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		/* goto done; */
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 7);
	if (primary_display_is_video_mode()) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 7);
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		/* insert a wait token to make sure first config after resume will config to HW when HW idle */
		_cmdq_insert_wait_frame_done_token();
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 8);

	DISPCHECK("[POWER]start cmdq[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[POWER]start cmdq[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 9);

#if 0
	DISPCHECK("[POWER]wakeup aal/od trigger process[begin]\n");
	wake_up_process(primary_path_aal_task);
	DISPCHECK("[POWER]wakeup aal/od trigger process[end]\n");
#endif
	pgc->state = DISP_ALIVE;

done:
	_primary_path_unlock(__func__);
	last_primary_trigger_time = sched_clock();
	/* primary_display_diagnose(); */
#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	aee_kernel_wdt_kick_Powkey_api("mtkfb_late_resume", WDT_SETBY_Display);
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagEnd, 0, 0);
	return 0;
}

int primary_display_ipoh_restore(void)
{
	DISPMSG("primary_display_ipoh_restore In\n");
	if (NULL != pgc->cmdq_handle_trigger) {
		struct TaskStruct *pTask = pgc->cmdq_handle_trigger->pRunningTask;

		if (NULL != pTask) {
			DISPCHECK("[Primary_display]display cmdq trigger loop stop[begin]\n");
			_cmdq_stop_trigger_loop();
			DISPCHECK("[Primary_display]display cmdq trigger loop stop[end]\n");
		}
	}
	DISPMSG("primary_display_ipoh_restore Out\n");
	return 0;
}

int primary_display_start(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_stop(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);


	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("stop display path failed, still busy\n");
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
	DISP_PRINTF(DDP_FENCE1_LOG, "primary_display_update_present_fence %d\n",
				gPresentFenceIndex);
}

static int _ovl_fence_release_callback(uint32_t userdata)
{
	int i = 0;
	unsigned int addr = 0;
	int ret = 0;

	MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagStart, 1, userdata);

	for (i = 0; i < PRIMARY_DISPLAY_SESSION_LAYER_COUNT; i++) {
		int fence_idx = 0;
		int subtractor = 0;

		if (i == primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled()) {
			mtkfb_release_layer_fence(primary_session_id, i);
		} else {
			cmdqBackupReadSlot(pgc->cur_config_fence, i, &fence_idx);
			cmdqBackupReadSlot(pgc->subtractor_when_free, i, &subtractor);
			mtkfb_release_fence(primary_session_id, i, fence_idx - subtractor);
		}
		MMProfileLogEx(ddp_mmp_get_events()->primary_ovl_fence_release, MMProfileFlagPulse,
			       i, fence_idx - subtractor);
	}

	if (primary_display_is_video_mode() == 1
	    && primary_display_is_decouple_mode() == 0 && gEnableOVLStatusCheck == 1) {
		unsigned int ovl_status[2];

		cmdqBackupReadSlot(pgc->ovl_status_info, 0, &ovl_status[0]);
		cmdqBackupReadSlot(pgc->ovl_status_info, 1, &ovl_status[1]);

		if ((ovl_status[0] & 0x1) != 0) {
			/* ovl is not idle !! */
			DISPERR
			    ("disp ovl status(0x%x)(0x%x) error!config maybe not finish during blanking\n",
			     ovl_status[0], ovl_status[1]);
		#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "DDP", "gce late");
		#endif
			ret = -1;
		}
	}

	addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), 0);

	/* async callback,need to check if it is still decouple */
	_primary_path_lock(__func__);
	if (_is_decouple_mode(pgc->session_mode) && userdata == DISP_SESSION_DECOUPLE_MODE) {
		static cmdqRecHandle cmdq_handle;

		if (cmdq_handle == NULL)
			ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);

		if (ret == 0) {
			cmdqRecReset(cmdq_handle);
			_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
			cmdqBackupReadSlot(pgc->rdma_buff_info, 0, &addr);
			decouple_rdma_config.address = addr;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
			decouple_rdma_config.security = g_wdma_rdma_security;
#else
			decouple_rdma_config.security = DISP_NORMAL_BUFFER;
#endif
			_config_rdma_input_data(&decouple_rdma_config, pgc->dpmgr_handle,
						cmdq_handle);
			_cmdq_set_config_handle_dirty_mira(cmdq_handle);
			cmdqRecFlushAsyncCallback(cmdq_handle, NULL, 0);
			MMProfileLogEx(ddp_mmp_get_events()->primary_rdma_config,
				       MMProfileFlagPulse, 0, decouple_rdma_config.address);
			/* cmdqRecDestroy(cmdq_handle); */
		} else {
			/* ret = -1; */
			DISPERR("fail to create cmdq\n");
		}
	}
	_primary_path_unlock(__func__);

	return ret;
}

static void _wdma_fence_release_callback(uint32_t userdata)
{
	int fence_idx = 0;
	int layer = 0;

	layer = disp_sync_get_output_timeline_id();
	cmdqBackupReadSlot(pgc->cur_config_fence, layer, &fence_idx);
	mtkfb_release_fence(primary_session_id, layer, fence_idx);
	MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_fence_release, MMProfileFlagPulse, layer,
		       fence_idx);
}

static int _trigger_ovl_to_memory(disp_path_handle disp_handle,
				  cmdqRecHandle cmdq_handle,
				  fence_release_callback callback, unsigned int data)
{
	dpmgr_path_trigger(disp_handle, cmdq_handle, CMDQ_ENABLE);
	cmdqRecWaitNoClear(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 0, mem_config.addr);

	cmdqRecFlushAsyncCallback(cmdq_handle, (CmdqAsyncFlushCB) callback, data);
	cmdqRecReset(cmdq_handle);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
	MMProfileLogEx(ddp_mmp_get_events()->ovl_trigger, MMProfileFlagPulse, 0, data);

	return 0;
}

static int _trigger_ovl_to_memory_mirror(disp_path_handle disp_handle,
					 cmdqRecHandle cmdq_handle,
					 fence_release_callback callback, unsigned int data)
{
	int layer = 0;
	unsigned int rdma_pitch_sec;

	dpmgr_path_trigger(disp_handle, cmdq_handle, CMDQ_ENABLE);

	cmdqRecWaitNoClear(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);

	layer = disp_sync_get_output_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer, mem_config.buff_idx);

	layer = disp_sync_get_output_interface_timeline_id();
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence, layer,
				mem_config.interface_idx);

	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 0, (unsigned int)mem_config.addr);

	/* rdma pitch only use bit[15..0], we use bit[31:30] to store secure information */
	rdma_pitch_sec = mem_config.pitch | (mem_config.security << 30);
	cmdqRecBackupUpdateSlot(cmdq_handle, pgc->rdma_buff_info, 1, rdma_pitch_sec);

	cmdqRecFlushAsyncCallback(cmdq_handle, (CmdqAsyncFlushCB) callback, data);
	cmdqRecReset(cmdq_handle);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_EOF);
	MMProfileLogEx(ddp_mmp_get_events()->ovl_trigger, MMProfileFlagPulse, 0, data);

	return 0;
}

static int primary_display_remove_output(void *callback, unsigned int userdata)
{
	int ret = 0;
	static cmdqRecHandle cmdq_handle;
	static cmdqRecHandle cmdq_wait_handle;

	/* create config thread */
	if (cmdq_handle == NULL)
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);

	if (ret == 0) {
		/* capture thread wait wdma sof */
		ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_MIRROR_MODE, &cmdq_wait_handle);
		if (ret == 0) {
			cmdqRecReset(cmdq_wait_handle);
			cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
			cmdqRecFlush(cmdq_wait_handle);
			/* cmdqRecDestroy(cmdq_wait_handle); */
		} else {
			DISPERR("fail to create  wait handle\n");
		}
		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

		/* update output fence */
		cmdqRecBackupUpdateSlot(cmdq_handle, pgc->cur_config_fence,
					disp_sync_get_output_timeline_id(), mem_config.buff_idx);

		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);

		cmdqRecClearEventToken(cmdq_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		cmdqRecFlushAsyncCallback(cmdq_handle, callback, 0);
		pgc->need_trigger_ovl1to2 = 0;
		/* cmdqRecDestroy(cmdq_handle); */
	} else {
		ret = -1;
		DISPERR("fail to remove memout out\n");
	}
	return ret;
}

static unsigned int _ovl_wdma_fence_release_callback(uint32_t userdata)
{
	_ovl_fence_release_callback(userdata);
	_wdma_fence_release_callback(userdata);

	return 0;
}

int primary_display_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;

	last_primary_trigger_time = sched_clock();
#ifdef DISP_SWITCH_DST_MODE
	if (is_switched_dst_mode) {
		primary_display_switch_dst_mode(1);	/* swith to vdo mode if trigger disp */
		is_switched_dst_mode = false;
	}
#endif

	primary_trigger_cnt++;

	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	if (blocking)
		DISPMSG("%s, change blocking to non blocking trigger\n", __func__);

#ifdef MTK_DISP_IDLE_LP
	_disp_primary_path_exit_idle(__func__, 0);
#endif
	dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		_trigger_display_interface(blocking, _ovl_fence_release_callback,
					   DISP_SESSION_DIRECT_LINK_MODE);
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		_trigger_display_interface(0, _ovl_fence_release_callback,
					   DISP_SESSION_DIRECT_LINK_MIRROR_MODE);
		if (pgc->need_trigger_ovl1to2 == 0) {
			DISPPR_ERROR("There is no output config when directlink mirror!!\n");
		} else {
			primary_display_remove_output(_wdma_fence_release_callback,
						      DISP_SESSION_DIRECT_LINK_MIRROR_MODE);
			pgc->need_trigger_ovl1to2 = 0;
		}
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE) {
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config,
				       (fence_release_callback) _ovl_fence_release_callback,
				       DISP_SESSION_DECOUPLE_MODE);
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		if (pgc->need_trigger_dcMirror_out == 0) {
			DISPPR_ERROR("There is no output config when decouple mirror!!\n");
		} else {
			pgc->need_trigger_dcMirror_out = 0;
			_trigger_ovl_to_memory_mirror(pgc->ovl2mem_path_handle,
						      pgc->cmdq_handle_ovl1to2_config,
						      (fence_release_callback)
						      _ovl_wdma_fence_release_callback,
						      DISP_SESSION_DECOUPLE_MIRROR_MODE);
		}
	}

	dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

done:
	_primary_path_unlock(__func__);

	/* FIXME: find aee_kernel_Powerkey_is_press definitation */
#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	if ((primary_trigger_cnt > PRIMARY_DISPLAY_TRIGGER_CNT) && aee_kernel_Powerkey_is_press()) {
		aee_kernel_wdt_kick_Powkey_api("primary_display_trigger", WDT_SETBY_Display);
		primary_trigger_cnt = 0;
	}
#endif

	return ret;
}

static int primary_display_ovl2mem_callback(unsigned int userdata)
{
	/* int i = 0; */
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0);
	int fence_idx = userdata;

	disp_ddp_path_config *data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	if (data_config) {
		WDMA_CONFIG_STRUCT wdma_layer;

		wdma_layer.dstAddress = mtkfb_query_buf_mva(session_id, 4, fence_idx);
		wdma_layer.outputFormat = data_config->wdma_config.outputFormat;
		wdma_layer.srcWidth = primary_display_get_width();
		wdma_layer.srcHeight = primary_display_get_height();
		wdma_layer.dstPitch = data_config->wdma_config.dstPitch;
		dprec_mmp_dump_wdma_layer(&wdma_layer, 0);
	}

	if (fence_idx > 0)
		mtkfb_release_fence(session_id, EXTERNAL_DISPLAY_SESSION_LAYER_COUNT, fence_idx);

	DISPMSG("mem_out release fence idx:0x%x\n", fence_idx);

	return 0;
}

int primary_display_mem_out_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;
	/* DISPFUNC(); */
	if (pgc->state == DISP_SLEPT || !_is_mirror_mode(pgc->session_mode)) {
		DISPMSG("mem out trigger is already slept or mode wrong(%d)\n",
			pgc->session_mode);
		return 0;
	}
	/* /dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0); */

	/* if(blocking) */
	{
		_primary_path_lock(__func__);
	}

	if (pgc->need_trigger_ovl1to2 == 0)
		goto done;


	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_DONE,
					 HZ * 1);


	if (_should_trigger_path())
		;

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);


	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0);

	if (_should_reset_cmdq_config_handle())
		cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	cmdqRecWait(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_SOF);

	_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);
	dpmgr_path_remove_memout(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config);

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);


	if (_should_flush_cmdq_config_handle())
		/* /_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0); */
		cmdqRecFlushAsyncCallback(pgc->cmdq_handle_ovl1to2_config,
					  (CmdqAsyncFlushCB) primary_display_ovl2mem_callback,
					  userdata);

	if (_should_reset_cmdq_config_handle())
		cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	/* /_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config); */

done:

	pgc->need_trigger_ovl1to2 = 0;

	_primary_path_unlock(__func__);

	/* /dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0); */

	return ret;
}

static int config_wdma_output(disp_path_handle disp_handle,
			      cmdqRecHandle cmdq_handle, disp_mem_output_config *output)
{
	disp_ddp_path_config *pconfig = NULL;

	ASSERT(output != NULL);
	pconfig = dpmgr_path_get_last_config(disp_handle);
	pconfig->wdma_config.dstAddress = output->addr;
	pconfig->wdma_config.srcHeight = output->h;
	pconfig->wdma_config.srcWidth = output->w;
	pconfig->wdma_config.clipX = output->x;
	pconfig->wdma_config.clipY = output->y;
	pconfig->wdma_config.clipHeight = output->h;
	pconfig->wdma_config.clipWidth = output->w;
	pconfig->wdma_config.outputFormat = output->fmt;
	pconfig->wdma_config.useSpecifiedAlpha = 1;
	pconfig->wdma_config.alpha = 0xFF;
	pconfig->wdma_config.dstPitch = output->pitch;
	pconfig->wdma_config.security = output->security;
	pconfig->wdma_dirty = 1;

	return dpmgr_path_config(disp_handle, pconfig, cmdq_handle);
}

int primary_display_config_output(disp_mem_output_config *output)
{
	int ret = 0;
	disp_path_handle disp_handle;
	cmdqRecHandle cmdq_handle = NULL;

	DISPFUNC();
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPMSG("mem out is already slept or mode wrong(%d)\n", pgc->session_mode);
		goto done;
	}

	if (!_is_mirror_mode(pgc->session_mode)) {
		DISPERR("should not config output if not mirror mode!!\n");
		goto done;
	}

	if (_is_decouple_mode(pgc->session_mode)) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	} else {
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	if (_is_decouple_mode(pgc->session_mode)) {
		pgc->need_trigger_dcMirror_out = 1;
	} else {
		/* direct link mirror mode should add memout first */
		dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);
		pgc->need_trigger_ovl1to2 = 1;
	}

	ret = config_wdma_output(disp_handle, cmdq_handle, output);

	mem_config = *output;
	MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_config,
		       MMProfileFlagPulse, output->buff_idx, (unsigned int)output->addr);

done:
	_primary_path_unlock(__func__);
	/* dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, output->src_x, output->src_y); */

	return ret;
}

static int _config_ovl_input(primary_disp_input_config *input,
			     disp_session_input_config *session_input,
			     disp_path_handle disp_handle, cmdqRecHandle cmdq_handle)
{
	int ret = 0;
	int i = 0;
	int layer = 0;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	bool sec = false;
#endif
	disp_path_handle *handle = disp_handle;
	disp_ddp_path_config *data_config = dpmgr_path_get_last_config(handle);

	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			if (input[i].layer_en) {
				if (input[i].vaddr) {
					_debug_pattern(0x00000000, input[i].vaddr, input[i].dst_w,
						       input[i].dst_h, input[i].src_pitch,
						       0x00000000, input[i].layer,
						       input[i].buff_idx);
				} else {
					_debug_pattern(input[i].addr, 0x00000000, input[i].dst_w,
						       input[i].dst_h, input[i].src_pitch,
						       0x00000000, input[i].layer,
						       input[i].buff_idx);
				}
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
				if (input[i].security == DISP_SECURE_BUFFER)
					sec = true;
#endif
			}
			/* DISPMSG("[primary], i:%d, layer:%d, layer_en:%d, dirty:%d\n",
			   i, input[i].layer, input[i].layer_en, input[i].dirty); */
			if (input[i].dirty) {
				dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG,
						   input[i].layer | (input[i].layer_en << 16),
						   input[i].addr);
				dprec_mmp_dump_ovl_layer(&(data_config->ovl_config[input[i].layer]),
							 input[i].layer, 1);
				ret =
				    _convert_disp_input_to_ovl(&
							       (data_config->ovl_config
								[input[i].layer]), &input[i]);
				dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input[i].src_x,
						  input[i].src_y);
			}
			data_config->ovl_dirty = 1;

		}
	}
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	g_is_sec = sec;
#endif
	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

	ret = dpmgr_path_config(handle, data_config,
				primary_display_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* write fence_id/enable to DRAM using cmdq
	 * it will be used when release fence (put these after config registers done) */
	for (i = 0; i < session_input->config_layer_num; i++) {
		unsigned int last_fence, cur_fence;
		disp_input_config *input_cfg = &session_input->config[i];

		layer = input_cfg->layer_id;

		cmdqBackupReadSlot(pgc->cur_config_fence, layer, &last_fence);
		cur_fence = input_cfg->next_buff_idx;

		if (cur_fence != -1 && cur_fence > last_fence)
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->cur_config_fence,
						layer, cur_fence);

		/* for dim_layer/disable_layer/no_fence_layer, just release all fences configured */
		/* for other layers, release current_fence-1 */
		if (input_cfg->buffer_source == DISP_BUFFER_ALPHA
		    || input_cfg->layer_enable == 0 || cur_fence == -1)
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->subtractor_when_free,
						layer, 0);
		else
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->subtractor_when_free,
						layer, 1);
	}

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;

	return 0;
}

static int _config_rdma_input(primary_disp_input_config *input,
			      disp_session_input_config *session_input, disp_path_handle *handle)
{
	int ret;
	disp_ddp_path_config *data_config = NULL;

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

	ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
	data_config->rdma_dirty = 1;

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);

	ret = dpmgr_path_config(handle, data_config,
				primary_display_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	return ret;
}

static int _config_wdma_output(WDMA_CONFIG_STRUCT *wdma_config,
			       disp_path_handle disp_handle, cmdqRecHandle cmdq_handle)
{
	disp_ddp_path_config *pconfig = dpmgr_path_get_last_config(disp_handle);

	pconfig->wdma_config = *wdma_config;
	pconfig->wdma_dirty = 1;
	dpmgr_path_config(disp_handle, pconfig, cmdq_handle);

	return 0;
}

int primary_display_config_input_multiple(primary_disp_input_config *input,
					  disp_session_input_config *session_input)
{
	int ret = 0;
	unsigned int wdma_mva = 0;
	disp_path_handle disp_handle;
	cmdqRecHandle cmdq_handle;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	int i = 0;
	int height = primary_display_get_height();
	int width = primary_display_get_width();
	int bpp = 24;   /*internal picture is RGB888*/
	int buffer_size = width * height * bpp / 8;
#endif

	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}
#ifdef MTK_DISP_IDLE_LP
	/* call this in trigger is enough, do not have to call this in config */
	_disp_primary_path_exit_idle(__func__, 0);
#endif

	if (_is_decouple_mode(pgc->session_mode)) {
		disp_handle = pgc->ovl2mem_path_handle;
		cmdq_handle = pgc->cmdq_handle_ovl1to2_config;
	} else {
		disp_handle = pgc->dpmgr_handle;
		cmdq_handle = pgc->cmdq_handle_config;
	}

	if (_should_config_ovl_input())
		_config_ovl_input(input, session_input, disp_handle, cmdq_handle);
	else
		_config_rdma_input(input, session_input, disp_handle);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	if (g_is_sec && (pgc->dc_sec_buf[0] == 0)) {
		for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++)
			pgc->dc_sec_buf[i] = allocate_decouple_sec_buffer(buffer_size);
		primary_ovl0_handle = pgc->dc_sec_buf[0];
		primary_ovl0_handle_size = buffer_size;
	} else if (!g_is_sec && (pgc->dc_sec_buf[0] != 0)) {
		for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
			pgc->dc_sec_buf[i] = free_decouple_sec_buffer(pgc->dc_sec_buf[i]);
			pgc->dc_sec_buf[i] = 0;
		}
		primary_ovl0_handle = 0;
		primary_ovl0_handle_size = 0;
	}
#endif

	/* decouple mode, config internal buffer */
	if (_is_decouple_mode(pgc->session_mode) && !_is_mirror_mode(pgc->session_mode)) {
		pgc->dc_buf_id++;
		pgc->dc_buf_id %= DISP_INTERNAL_BUFFER_COUNT;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		decouple_wdma_config.security = g_wdma_rdma_security;
		if (decouple_wdma_config.security == DISP_SECURE_BUFFER)
			wdma_mva = pgc->dc_sec_buf[pgc->dc_buf_id];
		else
			wdma_mva = pgc->dc_buf[pgc->dc_buf_id];
#else
		wdma_mva = pgc->dc_buf[pgc->dc_buf_id];
#endif
		decouple_wdma_config.dstAddress = wdma_mva;
		_config_wdma_output(&decouple_wdma_config, pgc->ovl2mem_path_handle,
				    pgc->cmdq_handle_ovl1to2_config);
		mem_config.addr = wdma_mva;
		MMProfileLogEx(ddp_mmp_get_events()->primary_wdma_config, MMProfileFlagPulse,
			       pgc->dc_buf_id, wdma_mva);
	}


	/* backup ovl status for debug */
	if (primary_display_is_video_mode() && !primary_display_is_decouple_mode()) {
		cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_status_info, 1,
					    disp_addr_convert(DISP_REG_OVL_ADDCON_DBG));
		cmdqRecBackupRegisterToSlot(cmdq_handle, pgc->ovl_status_info, 0,
					    disp_addr_convert(DISP_REG_OVL_STA));
	}

done:
	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_config_input(primary_disp_input_config *input)
{
	int ret = 0;
	disp_ddp_path_config *data_config;
	disp_path_handle *handle;

	_primary_path_lock(__func__);

	if (_is_decouple_mode(pgc->session_mode))
		handle = pgc->ovl2mem_path_handle;
	else
		handle = pgc->dpmgr_handle;

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

	if (pgc->state == DISP_SLEPT) {
		if (isAEEEnabled && input->layer == primary_display_get_option("ASSERT_LAYER")) {
			/* hope we can use only 1 input struct for input config, just set layer number */
			if (_should_config_ovl_input()) {
				ret =
				    _convert_disp_input_to_ovl(&
							       (data_config->ovl_config
								[input->layer]), input);
				data_config->ovl_dirty = 1;
			} else {
				ret =
				    _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
				data_config->rdma_dirty = 1;
			}
			DISPCHECK("%s save temp asset layer ,because primary dipslay is sleep\n",
				  __func__);
		}
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, input->layer | (input->layer_en << 16),
			   input->addr);

	if (input->layer_en) {
		if (input->vaddr) {
			_debug_pattern(0x00000000, input->vaddr, input->dst_w, input->dst_h,
				       input->src_pitch, 0x00000000, input->layer, input->buff_idx);
		} else {
			_debug_pattern(input->addr, 0x00000000, input->dst_w, input->dst_h,
				       input->src_pitch, 0x00000000, input->layer, input->buff_idx);
		}
	}
	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
		data_config->ovl_dirty = 1;
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(handle, DISP_PATH_EVENT_FRAME_DONE, HZ * 1);


	ret =
	    dpmgr_path_config(handle, data_config,
			      primary_display_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;

	dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input->src_x, input->src_y);

done:

	_primary_path_unlock(__func__);
	return ret;
}


int rdma2_config_input(RDMA_CONFIG_STRUCT *config)
{
	int ret = 0;
	/* int i = 0; */
	/* int layer = 0; */

	disp_ddp_path_config *data_config;

	_primary_path_lock(__func__);

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 1;
	data_config->wdma_dirty = 0;

	memcpy(&data_config->rdma_config, config, sizeof(RDMA_CONFIG_STRUCT));

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
				primary_display_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	_primary_path_unlock(__func__);

	return ret;
}


static int Panel_Master_primary_display_config_dsi(const char *name, uint32_t config_value)
{
	int ret = 0;
	disp_ddp_path_config *data_config;
	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	/* modify below for config dsi */
	if (!strcmp(name, "PM_CLK")) {
		DISPMSG("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		data_config->dispif_config.dsi.PLL_CLOCK = config_value;
	} else if (!strcmp(name, "PM_SSC")) {
		data_config->dispif_config.dsi.ssc_range = config_value;
	}
	DISPMSG("Pmaster_config_dsi: will Run path_config()\n");
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);

	return ret;
}

int primary_display_user_cmd(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	cmdqRecHandle handle = NULL;
	int cmdqsize = 0;

	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagStart,
		       (unsigned long)handle, 0);

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);
	cmdqsize = cmdqRecGetInstructionCount(handle);

	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		cmdqRecDestroy(handle);
		handle = NULL;
	}
	_primary_path_unlock(__func__);

#ifdef MTK_DISP_IDLE_LP
	/* will write register in dpmgr_path_user_cmd, need to exit idle */
	if (gDecouplePQWithRDMA == 0) {
		last_primary_trigger_time = sched_clock();
		_disp_primary_path_exit_idle(__func__, 1);
	}
#endif
	ret = dpmgr_path_user_cmd(pgc->dpmgr_handle, cmd, arg, handle);

	if (handle) {
		if (cmdqRecGetInstructionCount(handle) > cmdqsize) {
			_primary_path_lock(__func__);
			if (pgc->state == DISP_ALIVE) {
				_cmdq_set_config_handle_dirty_mira(handle);
				/* use non-blocking flush here to avoid primary path is locked for too long */
				_cmdq_flush_config_handle_mira(handle, 0);
			}
			_primary_path_unlock(__func__);
		}

		cmdqRecDestroy(handle);
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagEnd,
		       (unsigned long)handle, cmdqsize);

	return ret;
}

static void directlink_path_add_memory(WDMA_CONFIG_STRUCT *p_wdma)
{
	int ret = 0;
	cmdqRecHandle cmdq_handle = NULL;
	cmdqRecHandle cmdq_wait_handle = NULL;
	disp_ddp_path_config *pconfig = NULL;

	/*create config thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
	if (ret != 0) {
		DISPCHECK("dl_to_dc capture:Fail to create cmdq handle\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_handle);

	/*create wait thread */
	ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &cmdq_wait_handle);
	if (ret != 0) {
		DISPCHECK("dl_to_dc capture:Fail to create cmdq wait handle\n");
		ret = -1;
		goto out;
	}
	cmdqRecReset(cmdq_wait_handle);

	/*configure  config thread */
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->wdma_config = *p_wdma;
	pconfig->wdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);

	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DDPDBG("dl_to_dc capture:Flush add memout mva(0x%lx)\n", p_wdma->dstAddress);

	/*wait wdma0 sof */
	cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
	cmdqRecFlush(cmdq_wait_handle);
	DDPDBG("dl_to_dc capture:Flush wait wdma sof\n");
#if 0
	cmdqRecReset(cmdq_handle);
	_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

	dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);
	_cmdq_set_config_handle_dirty_mira(cmdq_handle);
	/* flush remove memory to cmdq */
	_cmdq_flush_config_handle_mira(cmdq_handle, 0);
	DISPMSG("dl_to_dc capture: Flush remove memout\n");

	dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
#endif
out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);

}

static int _DL_switch_to_DC_fast(void)
{
	int ret = 0;

	RDMA_CONFIG_STRUCT rdma_config = decouple_rdma_config;
	WDMA_CONFIG_STRUCT wdma_config = decouple_wdma_config;

	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	unsigned int mva = pgc->dc_buf[pgc->dc_buf_id];

	wdma_config.dstAddress = mva;
	wdma_config.security = DISP_NORMAL_BUFFER;

	/* disable SODI by CPU */
#ifdef DISP_ENABLE_SODI
	spm_enable_sodi(0);
#endif
	/* 1.save a temp frame to intermediate buffer */
	directlink_path_add_memory(&wdma_config);

	/* 2.reset primary handle */
	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token();

	/* 3.modify interface path handle to new scenario(rdma->dsi) */
	/* DDP_SCENARIO_PRIMARY_RDMA0_DISP will off/on OD clock
	   that causes screen blinking when OD_CORE_EN=1. */
	dpmgr_modify_path(pgc->dpmgr_handle, DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP,
			  pgc->cmdq_handle_config,
			  primary_display_is_video_mode() ? DDP_VIDEO_MODE : DDP_CMD_MODE);
	/* 4.config rdma from directlink mode to memory mode */
	rdma_config.address = mva;
	rdma_config.security = DISP_NORMAL_BUFFER;
	rdma_config.pitch =
	    primary_display_get_width() * DP_COLOR_BITS_PER_PIXEL(rdma_config.inputFormat) >> 3;

	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	data_config_dl->rdma_config = rdma_config;
	data_config_dl->rdma_dirty = 1;
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	/* 5. backup rdma address to slots */
	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, mva);

	/* 6 .flush to cmdq */
	_cmdq_set_config_handle_dirty();
	_cmdq_flush_config_handle(1, NULL, 0);

	/* ddp_mmp_rdma_layer(&rdma_config, 0,  20, 20); */

	/* 7.reset  cmdq */
	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token();

	/* 9. create ovl2mem path handle */
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle =
	    dpmgr_create_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, pgc->cmdq_handle_ovl1to2_config);
	if (pgc->ovl2mem_path_handle) {
		DDPDBG("dpmgr create ovl memout path SUCCESS(%p)\n", pgc->ovl2mem_path_handle);
	} else {
		DDPERR("dpmgr create path FAIL\n");
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

	ret =
	    dpmgr_path_config(pgc->ovl2mem_path_handle, data_config_dc,
			      pgc->cmdq_handle_ovl1to2_config);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);

	/* use blocking flush to make sure all config is done. */
	/* cmdqRecDumpCommand(pgc->cmdq_handle_ovl1to2_config); */
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_EOF);
	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

	/* 11..enable event for new path */
/* dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE); */
/* dpmgr_map_event_to_irq(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START, DDP_IRQ_WDMA0_FRAME_COMPLETE); */
/* dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START); */

	if (primary_display_is_video_mode()) {
#if HDMI_MAIN_PATH
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_START);
#else
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_DONE);
#endif
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	/* dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE); */
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

/*out:*/
	/* enable SODI after switch */
#ifdef DISP_ENABLE_SODI
	spm_enable_sodi(1);
#endif
	return ret;
}

static int _DC_switch_to_DL_fast(void)
{
	int ret = 0;
	int layer = 0;
	disp_ddp_path_config *data_config_dl = NULL;
	disp_ddp_path_config *data_config_dc = NULL;
	OVL_CONFIG_STRUCT ovl_config[OVL_LAYER_NUM];

	/* 1. disable SODI */
#ifdef DISP_ENABLE_SODI
	spm_enable_sodi(0);
#endif
	/* 2.enable ovl/wdma clock */

	/* 3.destroy ovl->mem path. */
	data_config_dc = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	/*save ovl info */;
	memcpy(ovl_config, data_config_dc->ovl_config, sizeof(ovl_config));

	dpmgr_path_deinit(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
	dpmgr_destroy_path(pgc->ovl2mem_path_handle);
	/*clear sof token for next dl to dc */
	cmdqRecClearEventToken(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_SOF);

	_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 1);
	cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
	pgc->ovl2mem_path_handle = NULL;

	/* release output buffer */
	layer = disp_sync_get_output_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);


	/* 4.modify interface path handle to new scenario(rdma->dsi) */
	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token();

	dpmgr_modify_path(pgc->dpmgr_handle, DDP_SCENARIO_PRIMARY_DISP, pgc->cmdq_handle_config,
			  primary_display_is_video_mode() ? DDP_VIDEO_MODE : DDP_CMD_MODE);

	/* 5.config rdma from memory mode to directlink mode */
	data_config_dl = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config_dl->rdma_config = decouple_rdma_config;
	data_config_dl->rdma_config.address = 0;
	data_config_dl->rdma_config.pitch = 0;
	data_config_dl->rdma_config.security = DISP_NORMAL_BUFFER;
	data_config_dl->rdma_dirty = 1;
	memcpy(data_config_dl->ovl_config, ovl_config, sizeof(ovl_config));
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config_dl, pgc->cmdq_handle_config);

	/* use blocking flush to make sure all config is done, then stop/start trigger loop */

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config, pgc->rdma_buff_info, 0, 0);

	/* cmdqRecDumpCommand(pgc->cmdq_handle_config); */
	_cmdq_set_config_handle_dirty();
	_cmdq_flush_config_handle(1, NULL, 0);

	/* release output buffer */
	layer = disp_sync_get_output_interface_timeline_id();
	mtkfb_release_layer_fence(primary_session_id, layer);

	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token();

	/* 9.enable event for new path */
	if (primary_display_is_video_mode()) {
#if HDMI_MAIN_PATH
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_START);
#else
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_DONE);
#endif
	}
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

/*out:*/
	/* 1. enable SODI */
#ifdef DISP_ENABLE_SODI
	spm_enable_sodi(1);
#endif
	return ret;
}

const char *session_mode_spy(unsigned int mode)
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
	default:
		return "UNKNOWN";
	}
}

int primary_display_switch_mode(int sess_mode, unsigned int session, int force)
{
	int ret = 0;

	DDPDBG("primary_display_switch_mode switch mode from %d(%s) to %d(%s)\n",
		pgc->session_mode, session_mode_spy(pgc->session_mode), sess_mode,
		session_mode_spy(sess_mode));

	_primary_path_lock(__func__);

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagStart,
		       pgc->session_mode, sess_mode);

	if (pgc->session_mode == sess_mode)
		goto done;

	if (pgc->state == DISP_SLEPT) {
		DISPMSG("primary display switch from %s to %s in suspend state!!!\n",
			session_mode_spy(pgc->session_mode), session_mode_spy(sess_mode));
		pgc->session_mode = sess_mode;
		goto done;
	}
	DDPDBG("primary_display_switch_mode from DISP_IOCTL_SET_SESSION_MODE\n");
	DDPDBG("primary display will switch from %s to %s\n", session_mode_spy(pgc->session_mode),
	       session_mode_spy(sess_mode));

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
	    && sess_mode == DISP_SESSION_DECOUPLE_MODE) {
		/* dl to dc */
		_DL_switch_to_DC_fast();
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* dc to dl */
#ifdef MTK_DISP_IDLE_LP
		if (primary_display_is_video_mode() == 1)
			_disp_primary_low_power_change_ddp_clock(0);
#endif
		_DC_switch_to_DL_fast();
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		/* dl to dl mirror */
		/* cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &pgc->cmdq_handle_dl_mirror); */
		/* cmdqRecReset(pgc->cmdq_handle_dl_mirror); */
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/*dl mirror to dl */
		/* cmdqRecDestroy(pgc->cmdq_handle_dl_mirror); */
		/* pgc->cmdq_handle_dl_mirror = NULL; */
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
		   && sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		/* dl to dc mirror  mirror */
		_DL_switch_to_DC_fast();
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/*dc mirror  to dl */
		_DC_switch_to_DL_fast();
		pgc->session_mode = sess_mode;
		DISPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else {
		DISPERR("invalid mode switch from %s to %s\n", session_mode_spy(pgc->session_mode),
			session_mode_spy(sess_mode));
	}
done:
	_primary_path_unlock(__func__);
	pgc->session_id = session;

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagEnd,
		       pgc->session_mode, sess_mode);

	return ret;
}

#ifdef MTK_DISP_IDLE_LP
static int primary_display_switch_mode_nolock(int sess_mode, unsigned int session, int force)
{
	int ret = 0;

	DISPCHECK("primary_display_switch_mode_nolock sess_mode %d, session 0x%x\n", sess_mode,
		  session);

	/* if(!force && _is_decouple_mode(pgc->session_mode)) */
	/* return 0; */

	/* _primary_path_lock(__func__); */

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagStart,
		       pgc->session_mode, sess_mode);

	if (pgc->session_mode == sess_mode)
		goto done;

	DDPDBG("primary display will switch from %s to %s\n", session_mode_spy(pgc->session_mode),
	       session_mode_spy(sess_mode));

	if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
	    && sess_mode == DISP_SESSION_DECOUPLE_MODE) {
		/* signal frame start event to switch logic in fence_release_worker_thread */
		/* dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START); */
		_DL_switch_to_DC_fast();
		pgc->session_mode = sess_mode;
		DDPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* signal frame start event to switch logic in fence_release_worker_thread */
		/* dpmgr_signal_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START); */
		_DC_switch_to_DL_fast();
		pgc->session_mode = sess_mode;
		DDPMSG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE) {
		/*need delay switch to output */
		pgc->session_mode = sess_mode;
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		/* xxx */
		pgc->session_mode = sess_mode;
	} else if (pgc->session_mode == DISP_SESSION_DIRECT_LINK_MODE
		   && sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		/*need delay switch to output */
		_DL_switch_to_DC_fast();
		pgc->session_mode = sess_mode;
		DDPDBG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
		/* pgc->session_delay_mode = sess_mode; */
	} else if (pgc->session_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE
		   && sess_mode == DISP_SESSION_DIRECT_LINK_MODE) {
		_DC_switch_to_DL_fast();
		pgc->session_mode = sess_mode;
		DDPDBG("primary display is %s mode now\n", session_mode_spy(pgc->session_mode));
		MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagPulse,
			       pgc->session_mode, sess_mode);
		/* primary_display_diagnose(); */
	} else {
		DISPERR("invalid mode switch from %s to %s\n", session_mode_spy(pgc->session_mode),
			session_mode_spy(sess_mode));
	}
done:
	/* _primary_path_unlock(__func__); */
	DDPDBG("leave primary_display_switch_mode_nolock\n");
	pgc->session_id = session;

	MMProfileLogEx(ddp_mmp_get_events()->primary_switch_mode, MMProfileFlagEnd,
		       pgc->session_mode, sess_mode);

	return ret;
}
#endif

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
#if HDMI_MAIN_PATH
	return HDMI_DISP_WIDTH;
#else
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->width;

	DISPERR("lcm_params is null!\n");
	return 0;

#endif
}

int primary_display_get_height(void)
{
#if HDMI_MAIN_PATH
	return HDMI_DISP_HEIGHT;
#else
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return 0;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params->height;

	DISPERR("lcm_params is null!\n");
	return 0;

#endif
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

void primary_display_set_max_layer(int maxlayer)
{
	pgc->max_layer = maxlayer;
}

int primary_display_get_info(void *info)
{
#if 1
	/* DISPFUNC(); */
	disp_session_info *dispif_info = (disp_session_info *) info;

#if HDMI_MAIN_PATH
#else
	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);

	if (lcm_param == NULL) {
		DISPCHECK("lcm_param is null\n");
		return -1;
	}
#endif

	memset((void *)dispif_info, 0, sizeof(disp_session_info));

	/* TODO: modify later */
	if (is_DAL_Enabled() && pgc->max_layer == 4)
		dispif_info->maxLayerNum = pgc->max_layer - 1;
	else
		dispif_info->maxLayerNum = pgc->max_layer;

#if HDMI_MAIN_PATH
	dispif_info->displayType = DISP_IF_TYPE_DPI;
	dispif_info->displayMode = DISP_IF_MODE_VIDEO;
	dispif_info->isHwVsyncAvailable = 1;
#elif MAIN_PATH_DISABLE_LCM
	dispif_info->displayType = DISP_IF_TYPE_DSI0;
	dispif_info->displayMode = DISP_IF_MODE_VIDEO;
	dispif_info->isHwVsyncAvailable = 1;
#else
	switch (lcm_param->type) {
	case LCM_TYPE_DBI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DBI;
			dispif_info->displayMode = DISP_IF_MODE_COMMAND;
			dispif_info->isHwVsyncAvailable = 1;
			/* DISPMSG("DISP Info: DBI, CMD Mode, HW Vsync enable\n"); */
			break;
		}
	case LCM_TYPE_DPI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DPI;
			dispif_info->displayMode = DISP_IF_MODE_VIDEO;
			dispif_info->isHwVsyncAvailable = 1;
			/* DISPMSG("DISP Info: DPI, VDO Mode, HW Vsync enable\n"); */
			break;
		}
	case LCM_TYPE_DSI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DSI0;
			if (lcm_param->dsi.mode == CMD_MODE) {
				dispif_info->displayMode = DISP_IF_MODE_COMMAND;
				dispif_info->isHwVsyncAvailable = 1;
				/* DISPMSG("DISP Info: DSI, CMD Mode, HW Vsync enable\n"); */
			} else {
				dispif_info->displayMode = DISP_IF_MODE_VIDEO;
				dispif_info->isHwVsyncAvailable = 1;
				/* DISPMSG("DISP Info: DSI, VDO Mode, HW Vsync enable\n"); */
			}

			break;
		}
	default:
		break;
	}
#endif


	dispif_info->displayFormat = DISP_IF_FORMAT_RGB888;

	dispif_info->displayWidth = primary_display_get_width();
	dispif_info->displayHeight = primary_display_get_height();

	if (2560 == dispif_info->displayWidth)
		dispif_info->displayWidth = 2048;
	if (1600 == dispif_info->displayHeight)
		dispif_info->displayHeight = 1536;
	dispif_info->vsyncFPS = pgc->lcm_fps;

	if (dispif_info->displayWidth * dispif_info->displayHeight <= 240 * 432)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (dispif_info->displayWidth * dispif_info->displayHeight <= 320 * 480)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (dispif_info->displayWidth * dispif_info->displayHeight <= 480 * 854)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;


	dispif_info->isConnected = 1;

#ifdef ROME_TODO
#error
	{
		LCM_PARAMS lcm_params_temp;

		memset((void *)&lcm_params_temp, 0, sizeof(lcm_params_temp));
		if (lcm_drv) {
			lcm_drv->get_params(&lcm_params_temp);
			dispif_info->lcmOriginalWidth = lcm_params_temp.width;
			dispif_info->lcmOriginalHeight = lcm_params_temp.height;
			DISPMSG("DISP Info: LCM Panel Original Resolution(For DFO Only): %d x %d\n",
				dispif_info->lcmOriginalWidth, dispif_info->lcmOriginalHeight);
		} else {
			DISPMSG("DISP Info: Fatal Error!!, lcm_drv is null\n");
		}
	}
#endif

#endif
	return 0;
}

int primary_display_get_pages(void)
{
	return 3;
}


int primary_display_is_video_mode(void)
{
	/* TODO: we should store the video/cmd mode in runtime, because ROME will support cmd/vdo dynamic switch */
	return disp_lcm_is_video_mode(pgc->plcm);
}

int primary_display_is_decouple_mode(void)
{
	return _is_decouple_mode(pgc->session_mode);
}

int primary_display_diagnose(void)
{
	int ret = 0;

	dpmgr_check_status(pgc->dpmgr_handle);
	if (primary_display_mode == DECOUPLE_MODE)
		dpmgr_check_status(pgc->ovl2mem_path_handle);
	primary_display_check_path(NULL, 0);

	return ret;
}

CMDQ_SWITCH primary_display_cmdq_enabled(void)
{
	return primary_display_use_cmdq;
}

int primary_display_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq)
{
	_primary_path_lock(__func__);

	primary_display_use_cmdq = use_cmdq;
	DISPCHECK("display driver use %s to config register now\n",
		  (use_cmdq == CMDQ_ENABLE) ? "CMDQ" : "CPU");

	_primary_path_unlock(__func__);
	return primary_display_use_cmdq;
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

unsigned int primary_display_get_fps(void)
{
	unsigned int fps = 0;

	_primary_path_lock(__func__);
	fps = pgc->lcm_fps;
	_primary_path_unlock(__func__);

	return fps;
}

int primary_display_force_set_vsync_fps(unsigned int fps)
{
	int ret = 0;

	DISPMSG("force set fps to %d\n", fps);
	_primary_path_lock(__func__);

	if (fps == pgc->lcm_fps) {
		pgc->vsync_drop = 0;
		ret = 0;
	} else if (fps == 30) {
		pgc->vsync_drop = 1;
		ret = 0;
	} else {
		ret = -1;
	}

	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_enable_path_cg(int enable)
{
	int ret = 0;

	DISPMSG("%s primary display's path cg\n", enable ? "enable" : "disable");
	_primary_path_lock(__func__);

	if (enable) {

		clk_disable(ddp_clk_map[MM_CLK_DSI0_ENGINE]);
		clk_unprepare(ddp_clk_map[MM_CLK_DSI0_ENGINE]);
		clk_disable(ddp_clk_map[MM_CLK_DSI0_DIGITAL]);
		clk_unprepare(ddp_clk_map[MM_CLK_DSI0_DIGITAL]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_RDMA0]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_RDMA0]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_OVL0]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_OVL0]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_COLOR0]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_COLOR0]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_OD]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_OD]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_UFOE]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_UFOE]);
		clk_disable(ddp_clk_map[MM_CLK_DISP_AAL]);
		clk_unprepare(ddp_clk_map[MM_CLK_DISP_AAL]);
		/* clk_disable(ddp_clk_map[MM_CLK_DISP_PWM026M]);
		   clk_unprepare(ddp_clk_map[MM_CLK_DISP_PWM026M]); */
		/* clk_disable(ddp_clk_map[MM_CLK_DISP_PWM0MM]);
		   clk_unprepare(ddp_clk_map[MM_CLK_DISP_PWM0MM]); */

		clk_disable(ddp_clk_map[MM_CLK_MUTEX_32K]);
		clk_unprepare(ddp_clk_map[MM_CLK_MUTEX_32K]);
		mtk_smi_larb_clock_off(4, true);
		mtk_smi_larb_clock_off(0, true);
	} else {
		ret += clk_prepare(ddp_clk_map[MM_CLK_MUTEX_32K]);
		ret += clk_enable(ddp_clk_map[MM_CLK_MUTEX_32K]);
		mtk_smi_larb_clock_on(0, true);
		mtk_smi_larb_clock_on(4, true);

		ret += clk_prepare(ddp_clk_map[MM_CLK_DSI0_ENGINE]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DSI0_ENGINE]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DSI0_DIGITAL]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DSI0_DIGITAL]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_RDMA0]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_RDMA0]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_OVL0]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_OVL0]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_COLOR0]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_COLOR0]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_OD]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_OD]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_UFOE]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_UFOE]);
		ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_AAL]);
		ret += clk_enable(ddp_clk_map[MM_CLK_DISP_AAL]);
		/*              ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_PWM026M]);
		   ret += clk_enable(ddp_clk_map[MM_CLK_DISP_PWM026M]); */
		/*              ret += clk_prepare(ddp_clk_map[MM_CLK_DISP_PWM0M]);
		   ret += clk_enable(ddp_clk_map[MM_CLK_DISP_PWM0M]); */
	}

	_primary_path_unlock(__func__);

	return ret;
}

int _set_backlight_by_cmdq(unsigned int level)
{
	int ret = 0;
	cmdqRecHandle cmdq_handle_backlight = NULL;

	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 1);
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle_backlight);
	DISPCHECK("primary backlight, handle=0x%p\n", cmdq_handle_backlight);
	if (ret != 0) {
		DISPCHECK("fail to create primary cmdq handle for backlight\n");
		return -1;
	}

	if (primary_display_is_video_mode()) {
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 2);
		cmdqRecReset(cmdq_handle_backlight);
		dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT,
				 (unsigned long *)&level);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n", ret);
	} else {
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 3);
		cmdqRecReset(cmdq_handle_backlight);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_backlight);
		cmdqRecClearEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT,
				 (unsigned long *)&level);
		cmdqRecSetEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 4);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 6);
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n", ret);
	}
	cmdqRecDestroy(cmdq_handle_backlight);
	cmdq_handle_backlight = NULL;
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 5);
	return ret;
}

int _set_backlight_by_cpu(unsigned int level)
{
	int ret = 0;

	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 1);
	if (primary_display_is_video_mode()) {
		disp_lcm_set_backlight(pgc->plcm, level);
	} else {
		DISPCHECK("[BL]display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPCHECK("[BL]display cmdq trigger loop stop[end]\n");

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK("[BL]primary display path is busy\n");
			ret =
			    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE,
						     HZ * 1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[BL]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]stop dpmgr path[end]\n");
		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			DISPCHECK("[BL]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE,
						 HZ * 1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}
		DISPCHECK("[BL]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]reset display path[end]\n");

		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 2);

		disp_lcm_set_backlight(pgc->plcm, level);

		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 3);

		DISPCHECK("[BL]start dpmgr path[begin]\n");
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]start dpmgr path[end]\n");

		DISPCHECK("[BL]start cmdq trigger loop[begin]\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[BL]start cmdq trigger loop[end]\n");
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
	return ret;
}

int primary_display_setbacklight(unsigned int level)
{
	int ret = 0;

	DISPFUNC();
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State set backlight invald\n");
	} else {
		if (primary_display_cmdq_enabled()) {
			if (primary_display_is_video_mode()) {
				MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl,
					       MMProfileFlagPulse, 0, 7);
				disp_lcm_set_backlight(pgc->plcm, level);
			} else {
				_set_backlight_by_cmdq(level);
			}
		} else {
			_set_backlight_by_cpu(level);
		}
	}
	_primary_path_unlock(__func__);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagEnd, 0, 0);
	return ret;
}

#define LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL

/***********************/
/*****Legacy DISP API*****/
/***********************/
uint32_t DISP_GetScreenWidth(void)
{
	return primary_display_get_width();
}

uint32_t DISP_GetScreenHeight(void)
{
	return primary_display_get_height();
}

uint32_t DISP_GetActiveHeight(void)
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


uint32_t DISP_GetActiveWidth(void)
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

LCM_PARAMS *DISP_GetLcmPara(void)
{
	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if (pgc->plcm->params)
		return pgc->plcm->params;

	return NULL;
}

LCM_DRIVER *DISP_GetLcmDrv(void)
{

	if (pgc->plcm == NULL) {
		DISPERR("lcm handle is null\n");
		return NULL;
	}

	if (pgc->plcm->drv)
		return pgc->plcm->drv;

	return NULL;
}


int primary_display_capture_framebuffer_ovl(unsigned long pbuf, unsigned int format)
{
	/* unsigned int i = 0; */
	int ret = 0;
	cmdqRecHandle cmdq_handle = NULL;
	cmdqRecHandle cmdq_wait_handle = NULL;
	disp_ddp_path_config *pconfig = NULL;
	m4u_client_t *m4uClient = NULL;
	unsigned int mva = 0;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	/* bpp is either 32 or 16, can not be other value */
	unsigned int pixel_byte = primary_display_get_bpp() / 8;
	unsigned int buffer_size = h_yres * w_xres * pixel_byte;
	/*DISPMSG("primary capture: begin\n"); */
	DISPERR("primary capture: pbuf = 0x%lx, buffer_size=%d\n", pbuf, buffer_size);

	disp_sw_mutex_lock(&(pgc->capture_lock));

	if (primary_display_is_sleepd() || !primary_display_cmdq_enabled()) {
		memset((void *)pbuf, 0, buffer_size);
		DISPMSG("primary capture: Fail black End\n");
		goto out;
	}

	if (_is_decouple_mode(pgc->session_mode) || _is_mirror_mode(pgc->session_mode)) {
		/* primary_display_capture_framebuffer_decouple(pbuf, format); */
		memset((void *)pbuf, 0, buffer_size);
		DISPMSG("primary capture: Fail black for decouple & mirror mode End\n");
		goto out;
	}

	m4uClient = m4u_create_client();
	if (m4uClient == NULL) {
		DISPCHECK("primary capture:Fail to alloc  m4uClient\n");
		ret = -1;
		goto out;
	}

	ret =
	    m4u_alloc_mva(m4uClient, M4U_PORT_DISP_WDMA0, pbuf, NULL, buffer_size,
			  M4U_PROT_READ | M4U_PROT_WRITE, 0, &mva);
	if (ret != 0) {
		DISPCHECK("primary capture:Fail to allocate mva\n");
		ret = -1;
		goto out;
	}

	ret =
	    m4u_cache_sync(m4uClient, M4U_PORT_DISP_WDMA0, pbuf, buffer_size, mva,
			   M4U_CACHE_FLUSH_BY_RANGE);
	if (ret != 0) {
		DISPCHECK("primary capture:Fail to cach sync\n");
		ret = -1;
		goto out;
	}

	if (primary_display_cmdq_enabled()) {
		/*create config thread */
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &cmdq_handle);
		if (ret != 0) {
			DISPCHECK
			    ("primary capture:Fail to create primary cmdq handle for capture\n");
			ret = -1;
			goto out;
		}
		cmdqRecReset(cmdq_handle);

		/*create wait thread */
		ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &cmdq_wait_handle);
		if (ret != 0) {
			DISPCHECK
			    ("primary capture:Fail to create primary cmdq wait handle for capture\n");
			ret = -1;
			goto out;
		}
		cmdqRecReset(cmdq_wait_handle);

		/*configure  config thread */
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		dpmgr_path_memout_clock(pgc->dpmgr_handle, 1);

		_primary_path_lock(__func__);
		pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		pconfig->wdma_dirty = 1;
		pconfig->wdma_config.dstAddress = mva;
		pconfig->wdma_config.srcHeight = h_yres;
		pconfig->wdma_config.srcWidth = w_xres;
		pconfig->wdma_config.clipX = 0;
		pconfig->wdma_config.clipY = 0;
		pconfig->wdma_config.clipHeight = h_yres;
		pconfig->wdma_config.clipWidth = w_xres;
		pconfig->wdma_config.outputFormat = format;
		pconfig->wdma_config.useSpecifiedAlpha = 1;
		pconfig->wdma_config.alpha = 0xFF;
		pconfig->wdma_config.dstPitch = w_xres * DP_COLOR_BITS_PER_PIXEL(format) / 8;
		dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);
		ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
		pconfig->wdma_dirty = 0;
		_primary_path_unlock(__func__);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
		DISPMSG("primary capture:Flush add memout mva(0x%x)\n", mva);

		/*wait wdma0 sof */
		cmdqRecWait(cmdq_wait_handle, CMDQ_EVENT_DISP_WDMA0_SOF);
		cmdqRecFlush(cmdq_wait_handle);
		DISPMSG("primary capture:Flush wait wdma sof\n");

		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		_primary_path_lock(__func__);
		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);
		_primary_path_unlock(__func__);

		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		/* flush remove memory to cmdq */
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
		DISPMSG("primary capture: Flush remove memout\n");

		dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
	}

out:
	cmdqRecDestroy(cmdq_handle);
	cmdqRecDestroy(cmdq_wait_handle);
	if (mva > 0)
		m4u_dealloc_mva(m4uClient, M4U_PORT_DISP_WDMA0, mva);

	if (m4uClient != 0)
		m4u_destroy_client(m4uClient);

	disp_sw_mutex_unlock(&(pgc->capture_lock));
	DISPMSG("primary capture: end\n");

	return ret;
}

int primary_display_capture_framebuffer(unsigned int pbuf)
{
#if 1
	unsigned int fb_layer_id = primary_display_get_option("FB_LAYER");
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_bpp = primary_display_get_bpp() / 8;	/* bpp is either 32 or 16, can not be other value */
	unsigned int w_fb = ALIGN_TO(w_xres, MTK_FB_ALIGNMENT);
	unsigned int fbsize = w_fb * h_yres * pixel_bpp;	/* frame buffer size */
	unsigned int fbaddress =
	    dpmgr_path_get_last_config(pgc->dpmgr_handle)->ovl_config[fb_layer_id].addr;
	/* unsigned int mem_off_x = 0; */
	/* unsigned int mem_off_y = 0; */
	unsigned int fbv = 0;
	unsigned int i = 0;
	unsigned long ttt = 0;

	DISPMSG("w_res=%d, h_yres=%d, pixel_bpp=%d, w_fb=%d, fbsize=%d, fbaddress=0x%08x\n", w_xres,
		h_yres, pixel_bpp, w_fb, fbsize, fbaddress);
	fbv = (unsigned int)(unsigned long)ioremap(fbaddress, fbsize);
	DISPMSG
	    ("w_xres = %d, h_yres = %d, w_fb = %d, pixel_bpp = %d, fbsize = %d, fbaddress = 0x%08x\n",
	     w_xres, h_yres, w_fb, pixel_bpp, fbsize, fbaddress);
	if (!fbv) {
		DISPMSG
		    ("[FB Driver], Unable to allocate memory for frame buffer: address=0x%08x, size=0x%08x\n",
		     fbaddress, fbsize);
		return -1;
	}

	ttt = get_current_time_us();
	for (i = 0; i < h_yres; i++) {
		/* DISPMSG("i=%d, dst=0x%08x,src=%08x\n", i, (pbuf + i * w_xres * pixel_bpp),
		(fbv + i * w_fb * pixel_bpp)); */
		memcpy((void *)(unsigned long)(pbuf + i * w_xres * pixel_bpp),
		       (void *)(unsigned long)(fbv + i * w_fb * pixel_bpp), w_xres * pixel_bpp);
	}
	DISPMSG("capture framebuffer cost %ldus\n", get_current_time_us() - ttt);
	iounmap((void *)(unsigned long)fbv);
#endif
	return -1;
}


#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
uint32_t DISP_GetPanelBPP(void)
{
#if 0
	PANEL_COLOR_FORMAT fmt;

	disp_drv_init_context();

	if (disp_if_drv->get_panel_color_format == NULL)
		return DISP_STATUS_NOT_IMPLEMENTED;


	fmt = disp_if_drv->get_panel_color_format();
	switch (fmt) {
	case PANEL_COLOR_FORMAT_RGB332:
		return 8;
	case PANEL_COLOR_FORMAT_RGB444:
		return 12;
	case PANEL_COLOR_FORMAT_RGB565:
		return 16;
	case PANEL_COLOR_FORMAT_RGB666:
		return 18;
	case PANEL_COLOR_FORMAT_RGB888:
		return 24;
	default:
		return 0;
	}
#else
	return 0;
#endif
}

static uint32_t disp_fb_bpp = 32;
static uint32_t disp_fb_pages = 3;

uint32_t DISP_GetScreenBpp(void)
{
	return disp_fb_bpp;
}

uint32_t DISP_GetPages(void)
{
	return disp_fb_pages;
}

uint32_t DISP_GetFBRamSize(void)
{
	return ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) *
	    ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) *
	    ((DISP_GetScreenBpp() + 7) >> 3) * DISP_GetPages();
}


uint32_t DISP_GetVRamSize(void)
{
#if 0
	/* Use a local static variable to cache the calculated vram size */
	/*  */
	static uint32_t vramSize;

	if (0 == vramSize) {
		disp_drv_init_context();

		/* /get framebuffer size */
		vramSize = DISP_GetFBRamSize();

		/* /get DXI working buffer size */
		vramSize += disp_if_drv->get_working_buffer_size();

		/* get assertion layer buffer size */
		vramSize += DAL_GetLayerSize();

		/* Align vramSize to 1MB */
		/*  */
		vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);

		DISP_LOG("DISP_GetVRamSize: %u bytes\n", vramSize);
	}

	return vramSize;
#else
	return 0;
#endif
}

uint32_t DISP_GetVRamSizeBoot(char *cmdline)
{
#ifdef CONFIG_OF
	_parse_tag_videolfb();
	if (vramsize == 0)
		vramsize = 0x3000000;
	DISPCHECK("[DT]display vram size = 0x%08x|%d\n", vramsize, vramsize);
	return vramsize;
#else
	char *p = NULL;
	uint32_t vramSize = 0;

	DISPMSG("%s, cmdline=%s\n", __func__, cmdline);
	p = strstr(cmdline, "vram=");
	if (p == NULL) {
		vramSize = 0x3000000;
		DISPERR("[FB driver]can not get vram size from lk\n");
	} else {
		p += 5;
		vramSize = kstrtol(p, NULL, 10);
		if (0 == vramSize)
			vramSize = 0x3000000;
	}

	DISPCHECK("display vram size = 0x%08x|%d\n", vramSize, vramSize);
	return vramSize;
#endif
}


struct sg_table table;

int disp_hal_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end, unsigned long *va,
				  unsigned long *mva)
{
	int ret = 0;
	*va = (unsigned long)ioremap_nocache(pa_start, pa_end - pa_start + 1);
	DISPDBG("disphal_allocate_fb, pa=%pa, va=0x%lx\n", &pa_start, *va);

/* if (_get_init_setting("M4U")) */
	/* xuecheng, m4u not enabled now */
	if (1) {
		m4u_client_t *client;

		struct sg_table *sg_table = &table;

		sg_alloc_table(sg_table, 1, GFP_KERNEL);

		sg_dma_address(sg_table->sgl) = pa_start;
		sg_dma_len(sg_table->sgl) = (pa_end - pa_start + 1);
		client = m4u_create_client();
		if (IS_ERR_OR_NULL(client))
			DISPMSG("create client fail!\n");

		*mva = pa_start & 0xffffffffULL;
		ret =
		    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, 0, sg_table, (pa_end - pa_start + 1),
				  M4U_PROT_READ | M4U_PROT_WRITE, M4U_FLAGS_FIX_MVA,
				  (unsigned int *)mva);
		/* m4u_alloc_mva(M4U_PORT_DISP_OVL0, pa_start, (pa_end - pa_start + 1), 0, 0, mva); */
		if (ret)
			DISPMSG("m4u_alloc_mva returns fail: %d\n", ret);

		DISPDBG("[DISPHAL] FB MVA is 0x%lx PA is %pa\n", *mva, &pa_start);

	} else {
		*mva = pa_start & 0xffffffffULL;
	}

	return 0;
}

int primary_display_remap_irq_event_map(void)
{
	return 0;
}

unsigned int primary_display_get_option(const char *option)
{
	if (!strcmp(option, "FB_LAYER"))
		return 0;
	if (!strcmp(option, "ASSERT_LAYER"))
		return 3;
	if (!strcmp(option, "M4U_ENABLE"))
		return 1;
	ASSERT(0);
	return 0;
}

int primary_display_get_debug_info(char *buf)
{
	/* resolution */
	/* cmd/video mode */
	/* display path */
	/* dsi data rate/lane number/state */
	/* primary path trigger count */
	/* frame done count */
	/* suspend/resume count */
	/* current fps 10s/5s/1s */
	/* error count and message */
	/* current state of each module on the path */
	return 0;
}

#include "ddp_reg.h"

#define IS_READY(x)	((x)?"READY\t":"Not READY")
#define IS_VALID(x)	((x)?"VALID\t":"Not VALID")

#define READY_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8) & (1 << x)))
#define VALID_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0) & (1 << x)))

#define READY_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bc) & (1 << x)))
#define VALID_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4) & (1 << x)))

int primary_display_check_path(char *stringbuf, int buf_len)
{
	int len = 0;

	if (stringbuf) {
		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "|--------------------------------------------------------------------------------------|\n");

		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",
			      DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),
			      DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),
			      DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),
			      DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "OVL0\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "OVL0_MOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "COLOR0_SEL:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "COLOR0:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "COLOR0_SOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "AAL_SEL:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "AAL0:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "OD:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "OD_MOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "RDMA0:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "RDMA0_SOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "PATH0_SEL:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "PATH0_SOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "UFOE:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "UFOE_MOUT:\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "DSI0_SEL:\t\t%s\t%s\n",
			      IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)),
			      IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "DSI0:\t\t\t%s\t%s\n",
			      IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)),
			      IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	} else {
		DISPMSG
		    ("|--------------------------------------------------------------------------------------|\n");

		DISPMSG("READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",
			DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),
			DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),
			DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),
			DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4));
		DISPMSG("OVL0\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		DISPMSG("OVL0_MOUT:\t\t%s\t%s\n",
			IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		DISPMSG("COLOR0_SEL:\t\t%s\t%s\n",
			IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		DISPMSG("COLOR0:\t\t\t%s\t%s\n",
			IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		DISPMSG("COLOR0_SOUT:\t\t%s\t%s\n",
			IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		DISPMSG("AAL_SEL:\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		DISPMSG("AAL0:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		DISPMSG("OD:\t\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		DISPMSG("OD_MOUT:\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		DISPMSG("RDMA0:\t\t\t%s\t%s\n", IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		DISPMSG("RDMA0_SOUT:\t\t%s\t%s\n",
			IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		DISPMSG("PATH0_SEL:\t\t%s\t%s\n",
			IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		DISPMSG("PATH0_SOUT:\t\t%s\t%s\n",
			IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		DISPMSG("UFOE:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		DISPMSG("UFOE_MOUT:\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		DISPMSG("DSI0_SEL:\t\t%s\t%s\n",
			IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)),
			IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		DISPMSG("DSI0:\t\t\t%s\t%s\n", IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)),
			IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	}

	return len;
}

int primary_display_lcm_ATA(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);
	if (pgc->state == 0) {
		DISPCHECK("ATA_LCM, primary display path is already sleep, skip\n");
		goto done;
	}

	DISPCHECK("[ATA_LCM]primary display path stop[begin]\n");
	if (primary_display_is_video_mode())
		dpmgr_path_ioctl(pgc->dpmgr_handle, NULL, DDP_STOP_VIDEO_MODE, NULL);

	DISPCHECK("[ATA_LCM]primary display path stop[end]\n");
	ret = disp_lcm_ATA(pgc->plcm);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (primary_display_is_video_mode()) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
done:
	_primary_path_unlock(__func__);
	return ret;
}

int fbconfig_get_esd_check_test(uint32_t dsi_id, uint32_t cmd, uint8_t *buffer, uint32_t num)
{
	int ret = 0;
	/* extern int fbconfig_get_esd_check(DSI_INDEX dsi_id, uint32_t cmd, UINT8 *buffer, uint32_t num); */
	_primary_path_lock(__func__);

	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("[ESD]primary display path is slept?? -- skip esd check\n");
		_primary_path_unlock(__func__);
		goto done;
	}
	/* / 1: stop path */
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
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	_primary_path_unlock(__func__);

done:
	return ret;
}

int Panel_Master_dsi_config_entry(const char *name, void *config_value)
{
	int ret = 0;
	int force_trigger_path = 0;
	uint32_t *config_dsi = (uint32_t *) config_value;
	LCM_PARAMS *lcm_param = NULL;
	LCM_DRIVER *pLcm_drv = DISP_GetLcmDrv();
	int esd_check_backup = atomic_read(&esd_check_task_wakeup);

	DISPFUNC();
	if (!strcmp(name, "DRIVER_IC_RESET") || !strcmp(name, "PM_DDIC_CONFIG")) {
		primary_display_esd_check_enable(0);
		msleep(2500);
	}
	_primary_path_lock(__func__);

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (pgc->state == DISP_SLEPT) {
		DISPERR("[Pmaster]Panel_Master: primary display path is slept??\n");
		goto done;
	}
	/* / Esd Check : Read from lcm */
	/* / the following code is to */
	/* / 0: lock path */
	/* / 1: stop path */
	/* / 2: do esd check (!!!) */
	/* / 3: start path */
	/* / 4: unlock path */
	/* / 1: stop path */
	_cmdq_stop_trigger_loop();

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);


	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
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
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		force_trigger_path = 0;
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[Pmaster]start cmdq trigger loop\n");
done:
	_primary_path_unlock(__func__);

	if (force_trigger_path) {	/* command mode only */
		primary_display_trigger(0, NULL, 0);
		DISPCHECK("[Pmaster]force trigger display path\r\n");
	}
	atomic_set(&esd_check_task_wakeup, esd_check_backup);

	return ret;
}

/*
mode: 0, switch to cmd mode; 1, switch to vdo mode
*/
int primary_display_switch_dst_mode(int mode)
{
	DISP_STATUS ret = DISP_STATUS_ERROR;
#ifdef DISP_SWITCH_DST_MODE
	void *lcm_cmd = NULL;

	DISPFUNC();
	_primary_path_switch_dst_lock();
	disp_sw_mutex_lock(&(pgc->capture_lock));
	if (pgc->plcm->params->type != LCM_TYPE_DSI) {
		DISPMSG("[primary_display_switch_dst_mode] Error, only support DSI IF\n");
		goto done;
	}
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK
		    ("[primary_display_switch_dst_mode], primary display path is already sleep, skip\n");
		goto done;
	}

	if (mode == primary_display_cur_dst_mode) {
		DISPCHECK
		    ("[primary_display_switch_dst_mode]not need switch,cur_mode:%d, switch_mode:%d\n",
		     primary_display_cur_dst_mode, mode);
		goto done;
	}
	lcm_cmd = disp_lcm_switch_mode(pgc->plcm, mode);
	if (lcm_cmd == NULL) {
		DISPCHECK("[primary_display_switch_dst_mode]get lcm cmd fail\n",
			  primary_display_cur_dst_mode, mode);
		goto done;
	} else {
		int temp_mode = 0;

		if (0 !=
		    dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
				     DDP_SWITCH_LCM_MODE, lcm_cmd)) {
			DISPMSG("switch lcm mode fail, return directly\n");
			goto done;
		}
		_primary_path_lock(__func__);
		temp_mode = (int)(pgc->plcm->params->dsi.mode);
		pgc->plcm->params->dsi.mode = pgc->plcm->params->dsi.switch_mode;
		pgc->plcm->params->dsi.switch_mode = temp_mode;
		dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());
		if (0 !=
		    dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
				     DDP_SWITCH_DSI_MODE, lcm_cmd)) {
			DISPMSG("switch dsi mode fail, return directly\n");
			_primary_path_unlock(__func__);
			goto done;
		}
	}
	primary_display_sodi_rule_init();
	#ifdef DISP_SUPPORT_CMDQ
	_cmdq_stop_trigger_loop();
	_cmdq_build_trigger_loop();
	_cmdq_start_trigger_loop();
	_cmdq_reset_config_handle();	/* must do this */
	_cmdq_insert_wait_frame_done_token();
	#endif
	primary_display_cur_dst_mode = mode;

	if (primary_display_is_video_mode()) {
#if HDMI_MAIN_PATH
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_START);
#else
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA0_DONE);
#endif
	} else {
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_DSI0_EXT_TE);
	}
	_primary_path_unlock(__func__);
	ret = DISP_STATUS_OK;
done:
/* dprec_handle_option(0x0); */
	disp_sw_mutex_unlock(&(pgc->capture_lock));
	_primary_path_switch_dst_unlock();
#else
	DISPMSG("[ERROR: primary_display_switch_dst_mode]this function not enable in disp driver\n");
#endif
	return ret;
}

int primary_display_set_panel_param(unsigned int param)
{
	int ret = DISP_STATUS_OK;

	DISPFUNC();
	MMProfileLogEx(ddp_mmp_get_events()->dsi_wrlcm, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	_primary_path_lock(__func__);
	if (pgc->state == DISP_SLEPT) {
		DISPCHECK("Sleep State set display parameter invald\n");
	} else {
		if (primary_display_cmdq_enabled()) {
			if (primary_display_is_video_mode()) {
				MMProfileLogEx(ddp_mmp_get_events()->dsi_wrlcm,
					       MMProfileFlagPulse, 0, 7);
				disp_lcm_set_param(pgc->plcm, param);
			} else {
				DISPCHECK("NOT video mode\n");
				/* _set_backlight_by_cmdq(param); */
			}
		} else {
			DISPCHECK("display cmdq NOT enabled\n");
			/* _set_backlight_by_cpu(level); */
		}
	}
	_primary_path_unlock(__func__);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	MMProfileLogEx(ddp_mmp_get_events()->dsi_wrlcm, MMProfileFlagEnd, 0, 0);

	return ret;
}

