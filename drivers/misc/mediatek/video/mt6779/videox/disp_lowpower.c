// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
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
#include <linux/math64.h>
#include "disp_drv_platform.h"	/* must be at the top-most */
/* #include "mtk_boot_common.h" */
#ifdef MTK_FB_SPM_SUPPORT
#include "mtk_idle.h"
#endif
#ifdef MTK_FB_MMDVFS_SUPPORT
#include "mt-plat/mtk_smi.h"
#include "mtk_smi.h"
#include "disp_pm_qos.h"
#include "mmdvfs_pmqos.h"
#endif
#if defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif

#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "disp_session.h"
#include "primary_display.h"

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
#include "external_display.h"
#endif

#include "disp_helper.h"
#if defined(CONFIG_MTK_CMDQ)
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#endif
#include "ddp_manager.h"
#include "disp_lcm.h"
#include "ddp_clkmgr.h"
#include "disp_drv_log.h"
#include "disp_lowpower.h"
#include "disp_arr.h"
#include "disp_rect.h"
#include "layering_rule.h"
#include "ddp_reg.h"
/* #include "mtk_dramc.h" */
#include "disp_partial.h"
#include "mtk_disp_mgr.h"
#include "disp_assert_layer.h"

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)

#define idlemgr_pgc		__get_idlemgr_context()
#define golden_setting_pgc	__get_golden_setting_context()

#define KICK_DUMP_MAX_LENGTH (1024 * 16 * 4)

#define MAX_IDLE_RSZ_RATIO 250

static unsigned char kick_string_buffer_analysize[KICK_DUMP_MAX_LENGTH];
static unsigned int kick_buf_length;
static atomic_t idlemgr_task_active = ATOMIC_INIT(1);
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
static atomic_t ext_idlemgr_task_wakeup = ATOMIC_INIT(1);
#endif
#ifdef MTK_FB_MMDVFS_SUPPORT
/* dvfs */
static atomic_t dvfs_ovl_req_status = ATOMIC_INIT(HRT_LEVEL_LEVEL0);
#endif
static int register_share_sram;
int hrt_bw_privilege;
static DECLARE_WAIT_QUEUE_HEAD(hrt_cond_wait_queue);
atomic_t last_hrt_idx = ATOMIC_INIT(0);
atomic_t hrt_cond_sig = ATOMIC_INIT(0);

/* check interval of entering screen idle */

atomic_t idle_need_repaint = ATOMIC_INIT(0);

/* Local API */
static int _primary_path_idlemgr_monitor_thread(void *data);
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
static int _external_path_idlemgr_monitor_thread(void *data);
#endif

static struct disp_idlemgr_context *__get_idlemgr_context(void)
{
	static int is_inited;
	static struct disp_idlemgr_context idlemgr_ctx;

	if (is_inited)
		return &idlemgr_ctx;

	init_waitqueue_head(&idlemgr_ctx.idlemgr_wait_queue);
	idlemgr_ctx.session_mode_before_enter_idle = DISP_INVALID_SESSION_MODE;
	idlemgr_ctx.is_primary_idle = 0;
	idlemgr_ctx.enterulps = 0;
	idlemgr_ctx.idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx.cur_lp_cust_mode = 0;
	idlemgr_ctx.primary_display_idlemgr_task =
			kthread_create(_primary_path_idlemgr_monitor_thread,
				       NULL, "disp_idlemgr");
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	init_waitqueue_head(&(idlemgr_ctx.ext_idlemgr_wait_queue));
	idlemgr_ctx.is_external_idle = 0;
	idlemgr_ctx.ext_idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx.external_display_idlemgr_task =
			kthread_create(_external_path_idlemgr_monitor_thread,
				       NULL, "ext_disp_idlemgr");
#endif
	is_inited = 1;

	return &idlemgr_ctx;
}

static int primary_display_idlemgr_init(void)
{
	wake_up_process(idlemgr_pgc->primary_display_idlemgr_task);
	return 0;
}

static struct golden_setting_context *__get_golden_setting_context(void)
{
	static int is_inited;
	static struct golden_setting_context gs_ctx;

	if (is_inited)
		goto done;

	/* default setting */
	gs_ctx.is_one_layer = 0;
	gs_ctx.fps = 60;
	gs_ctx.is_dc = 0;
	gs_ctx.is_display_idle = 0;
	gs_ctx.is_wrot_sram = 0;
	gs_ctx.mmsys_clk = MMSYS_CLK_LOW;

	/* primary_display */
	gs_ctx.dst_width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
	gs_ctx.dst_height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
	gs_ctx.rdma_width = gs_ctx.dst_width;
	gs_ctx.rdma_height = gs_ctx.dst_height;
	if (gs_ctx.dst_width == 1080 && gs_ctx.dst_height == 1920)
		gs_ctx.hrt_magicnum = 4;
	else if (gs_ctx.dst_width == 1440 && gs_ctx.dst_height == 2560)
		gs_ctx.hrt_magicnum = 4;

	/* set hrtnum max */
	gs_ctx.hrt_num = gs_ctx.hrt_magicnum + 1;

	/* fifo mode : 0/1/2 */
	if (gs_ctx.is_display_idle)
		gs_ctx.fifo_mode = 0;
	else if (gs_ctx.hrt_num > gs_ctx.hrt_magicnum)
		gs_ctx.fifo_mode = 2;
	else
		gs_ctx.fifo_mode = 1;

	/* ext_display */
	gs_ctx.ext_dst_width = gs_ctx.dst_width;
	gs_ctx.ext_dst_height = gs_ctx.dst_height;
	gs_ctx.ext_hrt_magicnum = gs_ctx.hrt_magicnum;
	gs_ctx.ext_hrt_num = gs_ctx.hrt_num;

	is_inited = 1;

done:
	return &gs_ctx;
}

static void set_is_display_idle(unsigned int is_displayidle)
{
	if (golden_setting_pgc->is_display_idle == is_displayidle)
		return;

	golden_setting_pgc->is_display_idle = is_displayidle;

	if (is_displayidle)
		golden_setting_pgc->fifo_mode = 0;
	else if (golden_setting_pgc->hrt_num <=
		 golden_setting_pgc->hrt_magicnum)
		golden_setting_pgc->fifo_mode = 1;
	else
		golden_setting_pgc->fifo_mode = 2;
}

static void set_share_sram(unsigned int is_share_sram)
{
	if (golden_setting_pgc->is_wrot_sram != is_share_sram)
		golden_setting_pgc->is_wrot_sram = is_share_sram;

	if (is_share_sram)
		mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
				 MMPROFILE_FLAG_START, 0, 0);
	else
		mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
				 MMPROFILE_FLAG_END, 0, 0);
}

static unsigned int use_wrot_sram(void)
{
	return golden_setting_pgc->is_wrot_sram;
}

static void set_mmsys_clk(unsigned int clk)
{
	if (golden_setting_pgc->mmsys_clk != clk)
		golden_setting_pgc->mmsys_clk = clk;
}

static unsigned int get_mmsys_clk(void)
{
	return golden_setting_pgc->mmsys_clk;
}

static int primary_display_set_idle_stat(int is_idle)
{
	int old_stat = idlemgr_pgc->is_primary_idle;

	idlemgr_pgc->is_primary_idle = is_idle;
	return old_stat;
}

/* Need blocking for stopping trigger loop */
int _blocking_flush(void)
{
	int ret = 0;
	struct cmdqRecStruct *handle = NULL;

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	if (ret) {
		DISP_PR_ERR("%s:%d, create cmdq handle fail! ret=%d\n",
			    __func__, __LINE__, ret);
		return -1;
	}
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);
	if (primary_display_is_video_mode()) {
		struct cmdqRecStruct *handle_vfp = NULL;

		ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_VFP_CHANGE,
			&handle_vfp);
		if (ret) {
			DISP_PR_INFO("%s:%d, create cmdq handle fail!ret=%d\n",
				__func__, __LINE__, ret);
			return -1;
		}
		cmdqRecReset(handle_vfp);
		_cmdq_insert_wait_frame_done_token_mira(handle_vfp);
		cmdqRecFlush(handle_vfp);

		cmdqRecDestroy(handle_vfp);
	}

	return ret;
}

static int primary_display_dsi_vfp_change(int state)
{
	int ret = 0;
	struct cmdqRecStruct *qhandle = NULL;
	struct LCM_PARAMS *params;

	cmdqRecCreate(CMDQ_SCENARIO_DISP_VFP_CHANGE, &qhandle);
	cmdqRecReset(qhandle);

	/* make sure token RDMA_SOF is clear */
	cmdqRecClearEventToken(qhandle, CMDQ_EVENT_DISP_RDMA0_SOF);

	/*
	 * for chips later than M17,VFP can be set at anytime
	 * So don't need to wait-SOF here
	 */
	/* cmdqRecWaitNoClear(qhandle, CMDQ_EVENT_DISP_RDMA0_SOF); */

	params = primary_get_lcm()->params;
	if (state == 1) {
		/* need calculate fps by VDO mode params */
		/* set_fps(55); */
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), qhandle,
				DDP_DSI_PORCH_CHANGE,
				&params->dsi.vertical_frontporch_for_low_power);
	} else if (state == 0) {
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), qhandle,
				 DDP_DSI_PORCH_CHANGE,
				 &params->dsi.vertical_frontporch);
	}
	cmdqRecFlushAsync(qhandle);
	cmdqRecDestroy(qhandle);
	return ret;
}

static void _idle_set_golden_setting(void)
{
	struct cmdqRecStruct *qhandle = NULL;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	/* no need lock */
	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &qhandle);
	cmdqRecReset(qhandle);

	/* 2.wait mutex0_stream_eof: only used for video mode */
	cmdqRecWaitNoClear(qhandle, CMDQ_EVENT_MUTEX0_STREAM_EOF);

	/* 3.golden setting */
	dpmgr_path_ioctl(phandle, qhandle, DDP_RDMA_GOLDEN_SETTING, pconfig);

	/* 4.flush */
	cmdqRecFlushAsync(qhandle);
	cmdqRecDestroy(qhandle);
}

/* share WROT SRAM for VDO mode, increase enter SODI ratio */
static void _acquire_wrot_resource_nolock(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct cmdqRecStruct *qhandle = NULL;
	int32_t acquireResult;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	DISPINFO("[LP]%s\n", __func__);

	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	if (use_wrot_sram())
		return;

	if (is_mipi_enterulps())
		return;

	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &qhandle);
	cmdqRecReset(qhandle);

	/* 2.wait EOF */
	_cmdq_insert_wait_frame_done_token_mira(qhandle);

	/* 3.try to share WROT SRAM */
	acquireResult = cmdqRecAcquireResource(qhandle, resourceEvent);

	if (acquireResult < 0) {
		/* acquire resource fail */
		DISPINFO("acquire resource fail\n");
		cmdqRecDestroy(qhandle);
		return;
	}

	/* acquire resource success */
	DISPDBG("share SRAM success\n");
	/* cmdqRecClearEventToken(qhandle, resourceEvent); //???cmdq do it */

	/* set RDMA golden setting parameters*/
	set_share_sram(1);

	/* add instr for modification RDMA fifo regs */
	/* dpmgr_handle can cover both DC & DL */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(phandle, qhandle, DDP_RDMA_GOLDEN_SETTING,
				 pconfig);

	cmdqRecFlushAsync(qhandle);
	cmdqRecDestroy(qhandle);
}

static int32_t _acquire_wrot_resource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 239, 0);
	primary_display_manual_lock();

	if (!register_share_sram) {
		primary_display_manual_unlock();
		return 0;
	}

	_acquire_wrot_resource_nolock(resourceEvent);
	primary_display_manual_unlock();
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 239, 1);

	return 0;
}

static void _release_wrot_resource_nolock(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct cmdqRecStruct *qhandle = NULL;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;
	unsigned int rdma0_shadow_mode = 0;

	DISPINFO("[LP]%s\n", __func__);

	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	if (use_wrot_sram() == 0)
		return;

	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &qhandle);
	cmdqRecReset(qhandle);

	/* 2.wait EOF */
	_cmdq_insert_wait_frame_done_token_mira(qhandle);

	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER) &&
	    disp_helper_get_option(DISP_OPT_SHADOW_MODE) != 2) {
		/*
		 * In CMD mode, after release WROT resource, primary display
		 * maybe enter idle mode, so the RDMA0 register only be written
		 * into shadow register. It will bring about RDMA0 and WROT1 use
		 * SRAM in the same time. Fix this bug by bypass shadow
		 * register.
		 */
		/*
		 * RDMA0 backup shadow mode and change to shadow
		 * register bypass mode
		 */
		rdma0_shadow_mode = DISP_REG_GET(DISP_REG_RDMA_SHADOW_UPDATE);
		DISP_REG_SET(qhandle, DISP_REG_RDMA_SHADOW_UPDATE,
			     (0x1 << 1) | (0x0 << 2));

		/* 3.disable RDMA0 share SRAM */
		DISP_REG_SET(qhandle, DISP_REG_RDMA_SRAM_SEL, 0);

		/* RDMA0 recover shadow mode */
		DISP_REG_SET(qhandle, DISP_REG_RDMA_SHADOW_UPDATE,
			     rdma0_shadow_mode);
	}

	/* set RDMA golden setting parameters */
	set_share_sram(0);

	/* 4.add instr for modification RDMA fifo regs */
	/* RDMA: dpmgr_handle can cover both DC & DL */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(phandle, qhandle, DDP_RDMA_GOLDEN_SETTING,
				 pconfig);

	/* 5.release share SRAM resourceEvent */
	cmdqRecReleaseResource(qhandle, resourceEvent);

	cmdqRecFlushAsync(qhandle);
	cmdqRecDestroy(qhandle);
}

static int32_t _release_wrot_resource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 239, 2);
	/* need lock */
	primary_display_manual_lock();

	if (!register_share_sram) {
		primary_display_manual_unlock();
		return 0;
	}

	_release_wrot_resource_nolock(resourceEvent);
	primary_display_manual_unlock();
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 239, 3);

	return 0;
}

static int __switch_mmsys_clk(int mmsys_clk_old, int mmsys_clk_new)
{
	int ret = 0;
	struct cmdqRecStruct *qhandle = NULL;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	DISPDBG("[LP]%s\n", __func__);

	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	if (mmsys_clk_new == get_mmsys_clk())
		return ret;

	if (primary_get_state() != DISP_ALIVE || is_mipi_enterulps()) {
		DISP_PR_ERR("[LP]%s: when display suspend old=%d & new=%d\n",
			    __func__, mmsys_clk_old, mmsys_clk_new);
		return ret;
	}

	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &qhandle);
	cmdqRecReset(qhandle);

	if (mmsys_clk_old == MMSYS_CLK_HIGH && mmsys_clk_new == MMSYS_CLK_LOW) {
		/* 2.wait SOF */
		_cmdq_insert_wait_frame_done_token_mira(qhandle);
		/* set RDMA golden setting parameters */
		set_mmsys_clk(MMSYS_CLK_LOW);
		/* need_disable_pll = MM_VENCPLL; */
	} else if (mmsys_clk_old == MMSYS_CLK_LOW &&
		   mmsys_clk_new == MMSYS_CLK_HIGH) {
		/* 2.wait SOF */
		_cmdq_insert_wait_frame_done_token_mira(qhandle);
		/* set RDMA golden setting parameters */
		set_mmsys_clk(MMSYS_CLK_HIGH);
		/* need_disable_pll = SYSPLL2_D2; */
	} else {
		goto cmdq_d;
	}

	/* 4.add instr for modification RDMA fifo regs */
	/* RDMA: dpmgr_handle can cover both DC & DL */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(phandle, qhandle, DDP_RDMA_GOLDEN_SETTING,
				 pconfig);

	cmdqRecFlush(qhandle);

cmdq_d:
	cmdqRecDestroy(qhandle);

	return get_mmsys_clk();
}

int primary_display_switch_mmsys_clk(int mmsys_clk_old, int mmsys_clk_new)
{
	/* need lock */
	DISPDBG("[LP]%s\n", __func__);
	primary_display_manual_lock();
	__switch_mmsys_clk(mmsys_clk_old, mmsys_clk_new);
	primary_display_manual_unlock();

	return 0;
}

static void _primary_display_disable_mmsys_clk(void)
{
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	if (primary_get_sess_mode() == DISP_SESSION_RDMA_MODE) {
		/* switch back to DL mode before suspend */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
					       primary_get_sess_id(), 0,
					       NULL, 1);
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE &&
	    primary_get_sess_mode() != DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return;
#else
	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE)
		return;
#endif
	/* blocking flush before stopping trigger loop */
	_blocking_flush();
	/* no need lock now */
	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPINFO("[LP]1.display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPINFO("[LP]1.display cmdq trigger loop stop[end]\n");
	}

	pconfig = dpmgr_path_get_last_config(phandle);
	if (disp_partial_is_support())
		primary_display_config_full_roi(pconfig, phandle, NULL);

	DISPINFO("[LP]2.primary display path stop[begin]\n");
	dpmgr_path_stop(phandle, CMDQ_DISABLE);
	DISPINFO("[LP]2.primary display path stop[end]\n");

	if (dpmgr_path_is_busy(phandle)) {
		DISP_PR_ERR("[LP]2.stop display path failed, still busy\n");
		dpmgr_path_reset(phandle, CMDQ_DISABLE);
		/* even path is busy(stop fail), we still need to continue
		 * power off other module/devices
		 */
	}

	/* can not release fence here */
	DISPINFO("[LP]3.dpmanager path power off[begin]\n");
	dpmgr_path_power_off_bypass_pwm(phandle, CMDQ_DISABLE);

	if (primary_display_is_decouple_mode()) {
		DISPCHECK("[LP]3.1.power off ovl2men path[begin]\n");
		if (primary_get_ovl2mem_handle())
			dpmgr_path_power_off(primary_get_ovl2mem_handle(),
					     CMDQ_DISABLE);
		else
			DISP_PR_ERR("DC but ovl2mem_path_handle is null\n");

		DISPINFO("[LP]3.1.power off ovl2men path[end]\n");
	}
	DISPDBG("[LP]3.dpmanager path power off[end]\n");
	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(1);
}

static void _primary_display_enable_mmsys_clk(void)
{
	disp_path_handle phandle = primary_get_dpmgr_handle();
	disp_path_handle ovl2mem_phandle = primary_get_ovl2mem_handle();
	struct disp_ddp_path_config *pconfig = NULL;
	struct ddp_io_golden_setting_arg gset_arg;

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE &&
	    primary_get_sess_mode() != DISP_SESSION_DECOUPLE_MIRROR_MODE)
		return;
#else
	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE)
		return;
#endif

	/* do something */
	DISPINFO("[LP]1.dpmanager path power on[begin]\n");
	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = dpmgr_path_get_dst_module_type(phandle);
	if (primary_display_is_decouple_mode()) {
		if (ovl2mem_phandle == NULL) {
			DISP_PR_ERR("DC but ovl2mem_path_handle is null\n");
			return;
		}

		gset_arg.is_decouple_mode = 1;
		DISPINFO("[LP]1.1.power on ovl2men path[begin]\n");
		dpmgr_path_power_on(ovl2mem_phandle, CMDQ_DISABLE);
		DISPINFO("[LP]1.1.power on ovl2men path[end]\n");
	}

	dpmgr_path_power_on_bypass_pwm(phandle, CMDQ_DISABLE);
	DISPINFO("[LP]1.dpmanager path power on[end]\n");
	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(0);

	DISPDBG("[LP]2.dpmanager path config[begin]\n");
	/*
	 * disconnect primary path first
	 * because MMsys config register may not power off during early suspend
	 * BUT session mode may change in primary_display_switch_mode()
	 */
	ddp_disconnect_path(DDP_SCENARIO_PRIMARY_ALL, NULL);
	ddp_disconnect_path(DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP, NULL);

	dpmgr_path_connect(phandle, CMDQ_DISABLE);
	if (primary_display_is_decouple_mode())
		dpmgr_path_connect(ovl2mem_phandle, CMDQ_DISABLE);

	pconfig = dpmgr_path_get_last_config(phandle);
	pconfig->dst_dirty = 1;
	pconfig->ovl_dirty = 1;
	pconfig->rdma_dirty = 1;
	dpmgr_path_config(phandle, pconfig, NULL);

	if (primary_display_is_decouple_mode()) {
		pconfig = dpmgr_path_get_last_config(phandle);
		pconfig->rdma_dirty = 1;
		dpmgr_path_config(phandle, pconfig, NULL);

		pconfig = dpmgr_path_get_last_config(ovl2mem_phandle);
		pconfig->dst_dirty = 1;
		dpmgr_path_config(ovl2mem_phandle, pconfig, NULL);
		dpmgr_path_ioctl(ovl2mem_phandle, NULL, DDP_OVL_GOLDEN_SETTING,
				 &gset_arg);
	} else {
		dpmgr_path_ioctl(phandle, NULL, DDP_OVL_GOLDEN_SETTING,
				 &gset_arg);
	}
	DISPDBG("[LP]2.dpmgr path config[end]\n");

	DISPDBG("[LP]3.dpmgr path start[begin]\n");
	dpmgr_path_start(phandle, CMDQ_DISABLE);

	if (primary_display_is_decouple_mode())
		dpmgr_path_start(ovl2mem_phandle, CMDQ_DISABLE);
	DISPINFO("[LP]3.dpmgr path start[end]\n");

	if (dpmgr_path_is_busy(phandle))
		DISP_PR_ERR("[LP]3.didn't trigger display but already busy\n");

	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPDBG("[LP]4.start cmdq[begin]\n");
		_cmdq_start_trigger_loop();
		DISPINFO("[LP]4.start cmdq[end]\n");
	}

	/*
	 * (in suspend) when we stop trigger loop
	 * if no other thread is running, cmdq may disable its clock
	 * all cmdq event will be cleared after suspend
	 */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
}

/* share WROT SRAM end */
static void _vdo_mode_enter_idle(void)
{
	struct LCM_PARAMS *params;
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
	unsigned int out_fps = 60;
	unsigned int in_fps = 0;
#endif

	DISPDBG("[LP]%s\n", __func__);

	/* backup for DL <-> DC */
	idlemgr_pgc->session_mode_before_enter_idle = primary_get_sess_mode();

	/* DL -> DC */
	if (!primary_is_sec() &&
	    primary_get_sess_mode() == DISP_SESSION_DIRECT_LINK_MODE &&
	    (disp_helper_get_option(DISP_OPT_IDLEMGR_SWTCH_DECOUPLE) ||
	     disp_helper_get_option(DISP_OPT_SMART_OVL))) {
		/* switch to decouple mode */
		if (disp_helper_get_option(DISP_OPT_IDLEMGR_BY_REPAINT)) {
			if (atomic_read(&real_input_layer) > 1) {
				atomic_set(&idle_need_repaint, 1);
				trigger_repaint(REFRESH_FOR_IDLE);
			}
		} else {
			do_primary_display_switch_mode(
				DISP_SESSION_DECOUPLE_MODE,
				primary_get_sess_id(), 0, NULL, 0);
			set_is_dc(1);
		}
	}

	/* disable IRQ & increase VFP */
	if (!primary_is_sec()) {
		if (disp_helper_get_option(
					DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
			/*
			 * disable routine IRQ before switch to decouple mode,
			 * otherwise we need to disable two paths
			 */
			dpmgr_path_enable_irq(primary_get_dpmgr_handle(), NULL,
					      DDP_IRQ_LEVEL_ERROR);
		}

		if (get_lp_cust_mode() > LP_CUST_DISABLE &&
		    get_lp_cust_mode() < PERFORMANC_MODE + 1) {
			switch (get_lp_cust_mode()) {
			case LOW_POWER_MODE: /* 50 */
			case JUST_MAKE_MODE: /* 55 */
				primary_display_dsi_vfp_change(1);
				idlemgr_pgc->cur_lp_cust_mode = 1;
				break;
			case PERFORMANC_MODE: /* 60 */
				primary_display_dsi_vfp_change(0);
				idlemgr_pgc->cur_lp_cust_mode = 0;
				break;
			}
		} else {
			params = primary_get_lcm()->params;
			if (get_backup_vfp() !=
			    params->dsi.vertical_frontporch_for_low_power)
				params->dsi.vertical_frontporch_for_low_power =
							get_backup_vfp();

			if (params->dsi.vertical_frontporch_for_low_power) {
				primary_display_dsi_vfp_change(1);
				idlemgr_pgc->cur_lp_cust_mode = 1;
			}
		}
	}

	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		enter_share_sram(CMDQ_SYNC_RESOURCE_WROT1);

	/* set golden setting, merge fps/dc */
	set_is_display_idle(1);
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		_idle_set_golden_setting();

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	if (primary_display_is_directlink_mode())
		in_fps = 60;
	disp_pm_qos_set_ovl_bw(in_fps, out_fps, &bandwidth);
	disp_pm_qos_update_bw(bandwidth);
#endif
}

static void _vdo_mode_leave_idle(void)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
	unsigned int in_fps = 60;
	unsigned int out_fps = 60;
#endif

	DISPDBG("[LP]%s\n", __func__);

	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		leave_share_sram(CMDQ_SYNC_RESOURCE_WROT1);

	/* set golden setting */
	set_is_display_idle(0);
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		_idle_set_golden_setting();

	/* enable IRQ & restore VFP */
	if (!primary_is_sec()) {
		if (idlemgr_pgc->cur_lp_cust_mode) {
			primary_display_dsi_vfp_change(0);
			idlemgr_pgc->cur_lp_cust_mode = 0;
			if (disp_helper_get_option(
					DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
				_idle_set_golden_setting();
		}
		if (disp_helper_get_option(
					DISP_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
			/*
			 * enable routine IRQ after switch to directlink mode,
			 * otherwise we need to disable two paths
			 */
			dpmgr_path_enable_irq(primary_get_dpmgr_handle(), NULL,
					      DDP_IRQ_LEVEL_ALL);
		}
	}

	/* DC -> DL */
	if (disp_helper_get_option(DISP_OPT_IDLEMGR_SWTCH_DECOUPLE) &&
	    !disp_helper_get_option(DISP_OPT_SMART_OVL)) {
		/* switch to the mode before idle */
		do_primary_display_switch_mode(
				idlemgr_pgc->session_mode_before_enter_idle,
				primary_get_sess_id(), 0, NULL, 0);

		set_is_dc(0);
		if (disp_helper_get_option(
					DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
			_idle_set_golden_setting();
	}

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	disp_pm_qos_set_ovl_bw(in_fps, out_fps, &bandwidth);
	disp_pm_qos_update_bw(bandwidth);
#endif
}

static void _cmd_mode_enter_idle(void)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
#endif

	DISPDBG("[LP]%s\n", __func__);

	/* need to leave share SRAM for disable mmsys clk */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		; /* leave_share_sram(CMDQ_SYNC_RESOURCE_WROT0); */

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	disp_pm_qos_set_default_bw(&bandwidth);
	disp_pm_qos_update_bw(bandwidth);
	if (disp_helper_get_option(DISP_OPT_HRT_MODE) == 1)
		prim_disp_request_hrt_bw(HRT_BW_UNREQ,
			DDP_SCENARIO_PRIMARY_DISP, __func__);
#endif

	/* please keep last */
	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
		_primary_display_disable_mmsys_clk();
}

static void _cmd_mode_leave_idle(void)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;
	unsigned int in_fps = 60;
	unsigned int out_fps = 60;
	int stable = 0;
	enum DDP_SCENARIO_ENUM scen = 0;
	int overlap_num = 0;

	DISPDBG("[LP]%s\n", __func__);

	if (disp_helper_get_option(DISP_OPT_HRT_MODE) == 1) {
		scen = (primary_display_is_decouple_mode()) ?
		    DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP :
		    DDP_SCENARIO_PRIMARY_DISP;
		overlap_num = (primary_display_is_decouple_mode()) ? 2 :
		    primary_display_get_dvfs_last_req();

		prim_disp_request_hrt_bw(overlap_num, scen, __func__);
	}
#endif

	DISPDBG("[LP]%s\n", __func__);

	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
		_primary_display_enable_mmsys_clk();

#ifdef MTK_FB_MMDVFS_SUPPORT
	if (disp_helper_get_option(DISP_OPT_HRT_MODE) == 1)
		prim_disp_request_hrt_bw(overlap_num, scen, __func__);
#endif

	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		; /* enter_share_sram(CMDQ_SYNC_RESOURCE_WROT0); */

#ifdef MTK_FB_MMDVFS_SUPPORT
	/* update bandwidth */
	primary_fps_ctx_get_fps(&in_fps, &stable);
	out_fps = in_fps;
	disp_pm_qos_set_ovl_bw(in_fps, out_fps, &bandwidth);
	disp_pm_qos_update_bw(bandwidth);
#endif
}

void primary_display_idlemgr_enter_idle_nolock(void)
{
	if (primary_display_is_video_mode())
		_vdo_mode_enter_idle();
	else
		_cmd_mode_enter_idle();
}

void primary_display_idlemgr_leave_idle_nolock(void)
{
	if (primary_display_is_video_mode())
		_vdo_mode_leave_idle();
	else
		_cmd_mode_leave_idle();
}

unsigned long long disp_lp_set_idle_check_interval(
	unsigned long long new_interval)
{
	unsigned long long old_interval = idle_check_interval;

	idle_check_interval = new_interval;
	return old_interval;
}

static int get_rsz_ratio(void)
{
	struct disp_ddp_path_config *config =
		dpmgr_path_get_last_config(primary_get_dpmgr_handle());
	struct RSZ_CONFIG_STRUCT *rsz_config = &config->rsz_config;
	int ratio_w, ratio_h;

	if (rsz_config->frm_in_w == 0 || rsz_config->frm_in_h == 0
		|| rsz_config->frm_out_w == 0 || rsz_config->frm_out_h == 0)
		return 100;

	ratio_w = rsz_config->frm_out_w * 100 / rsz_config->frm_in_w;
	ratio_h = rsz_config->frm_out_h * 100 / rsz_config->frm_in_h;

	return (ratio_w >= ratio_h) ? ratio_w : ratio_h;
}

static bool check_dim_layer(void)
{
	struct disp_ddp_path_config *config =
		dpmgr_path_get_last_config(primary_get_dpmgr_handle());
	struct OVL_CONFIG_STRUCT *cfg;
	int i = 0;
	bool exist = 0;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		cfg = &config->ovl_config[i];
		if (cfg->layer_en &&
			cfg->source == OVL_LAYER_SOURCE_RESERVED) {
			exist = 1;
			break;
		}
	}

	return exist;
}

static int has_yuv_layer(void)
{
	struct disp_ddp_path_config *config =
		dpmgr_path_get_last_config(primary_get_dpmgr_handle());
	struct OVL_CONFIG_STRUCT *ovl_cfg;
	enum UNIFIED_COLOR_FMT fmt;
	int i = 0;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		ovl_cfg = &config->ovl_config[i];
		if (ovl_cfg->layer_en == 0)
			continue;

		fmt = ovl_cfg->fmt;
		if (fmt == UFMT_UYVY || fmt == UFMT_VYUY ||
		    fmt == UFMT_YUYV || fmt == UFMT_YVYU)
			return 1;
	}

	return 0;
}

/* small video would shake due to low resolution */
static int rsz_skip_idle(void)
{
	int ratio = get_rsz_ratio();

	if (ratio >= MAX_IDLE_RSZ_RATIO) {
		DISPINFO("%s ratio:%d\n", __func__, ratio);
		return 1;
	}

	if (has_yuv_layer()) {
		DISPINFO("%s ratio:%d, has yuv\n", __func__, ratio);
		return 1;
	}

	return 0;
}

static int _primary_path_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	long long t_to_check = 0;
	unsigned long long t_idle;

	msleep(16000);
	while (1) {
		ret = wait_event_interruptible(idlemgr_pgc->idlemgr_wait_queue,
					atomic_read(&idlemgr_task_active));

		t_idle = local_clock() - idlemgr_pgc->idlemgr_last_kick_time;
		t_to_check = idle_check_interval * 1000 * 1000 - t_idle;
		do_div(t_to_check, 1000000);

		mmprofile_log_ex(ddp_mmp_get_events()->idle_monitor,
				 MMPROFILE_FLAG_PULSE, idle_check_interval,
				 t_to_check);

		/* error handling */
		t_to_check = min(t_to_check, 1000LL);
		/* when starting up before the first time kick */
		if (idlemgr_pgc->idlemgr_last_kick_time == 0)
			msleep_interruptible(idle_check_interval);
		else if (t_to_check > 0)
			msleep_interruptible(t_to_check);

		primary_display_manual_lock();

		if (primary_get_state() != DISP_ALIVE) {
			primary_display_manual_unlock();
			primary_display_wait_state(DISP_ALIVE,
						   MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if (primary_display_is_idle()) {
			primary_display_manual_unlock();
			continue;
		}

		/* Do not enter idle when we needs calculate FPS */
		if (atomic_read(&lcm_fps_ctx.skip_update) == 0) {
			DISPMSG("skip idle due to fps calculation\n");
			primary_display_manual_unlock();
			continue;
		}

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
		if (primary_display_get_lcm_refresh_rate() == 120) {
			primary_display_manual_unlock();
			continue;
		}
#endif

		if (primary_display_is_video_mode() &&
			rsz_skip_idle()) {
			primary_display_manual_unlock();
			continue;
		}

		if (primary_display_is_video_mode() && check_dim_layer()) {
			primary_display_manual_unlock();
			continue;
		}

		t_idle = local_clock() - idlemgr_pgc->idlemgr_last_kick_time;
		if (t_idle < idle_check_interval * 1000 * 1000) {
			/* kicked in idle_check_interval msec, it's not idle */
			primary_display_manual_unlock();
			continue;
		}
		/* double check if dynamic switch on/off */
		if (atomic_read(&idlemgr_task_active)) {
			mmprofile_log_ex(ddp_mmp_get_events()->idlemgr,
					 MMPROFILE_FLAG_START, 0, 0);
			DISPINFO("[LP]primary enter idle state\n");

			/* enter idle state */
			primary_display_idlemgr_enter_idle_nolock();
			primary_display_set_idle_stat(1);
		}

		primary_display_manual_unlock();

		wait_event_interruptible(idlemgr_pgc->idlemgr_wait_queue,
					 !primary_display_is_idle());

		if (kthread_should_stop())
			break;
	}

	return 0;
}

void kick_logger_dump(char *string)
{
	if (kick_buf_length + strlen(string) >= KICK_DUMP_MAX_LENGTH)
		kick_logger_dump_reset();

	kick_buf_length += scnprintf(kick_string_buffer_analysize +
				     kick_buf_length, KICK_DUMP_MAX_LENGTH -
				     kick_buf_length, string);
}

void kick_logger_dump_reset(void)
{
	kick_buf_length = 0;
	memset(kick_string_buffer_analysize, 0,
	       sizeof(kick_string_buffer_analysize));
}

char *get_kick_dump(void)
{
	return kick_string_buffer_analysize;
}

unsigned int get_kick_dump_size(void)
{
	return kick_buf_length;
}

/* API */
/**************************************************************************/
struct golden_setting_context *get_golden_setting_pgc(void)
{
	return golden_setting_pgc;
}

/* for met - begin */
unsigned int is_mipi_enterulps(void)
{
	return idlemgr_pgc->enterulps;
}

unsigned int get_mipi_clk(void)
{
	if (disp_helper_get_option(DISP_OPT_MET_LOG)) {
		if (is_mipi_enterulps())
			return 0;
		else
			return dsi_phy_get_clk(DISP_MODULE_NUM);
	} else {
		return 0;
	}
}
/* for met - end */

void primary_display_sodi_rule_init(void)
{
}

void hrt_bw_sync_idx(unsigned int cur_idx)
{
	atomic_set(&last_hrt_idx, cur_idx);
	atomic_set(&hrt_cond_sig, 1);
	wake_up(&hrt_cond_wait_queue);
}

int hrt_bw_cond_state(void)
{
	return hrt_bw_privilege;
}

int hrt_bw_set_state(int sta)
{
	hrt_bw_privilege = sta;

	return 0;
}

bool pri_disp_leave_privilege(bool need_lock)
{
	if (need_lock)
		primary_display_manual_lock();

	if (hrt_bw_privilege != 1) {
		if (need_lock)
			primary_display_manual_unlock();
		return 0;
	}

	DISPMSG("%s, switch from DC to DL\n", __func__);
	do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
			primary_get_sess_id(), 0, NULL, 0);
	hrt_bw_privilege = 0;

	set_is_dc(0);

	/* enable idlemgr */
	DISPCHECK("[disp_lowpower]enable idlemgr\n");
	atomic_set(&idlemgr_task_active, 1);
	wake_up_interruptible(&(idlemgr_pgc->idlemgr_wait_queue));

	if (need_lock)
		primary_display_manual_unlock();

	return 1;
}

static int hrt_bw_cond_change_cb(struct notifier_block *nb,
		unsigned long value, void *v)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	int ret, i;
	unsigned int hrt_idx;

	primary_display_manual_lock();

	switch (value) {
	case BW_THROTTLE_START: /* CAM on */
		DISPMSG("DISP BW Throttle start\n");

		if (get_has_hrt_bw() == 0) {
			DISPMSG("DISP has no HRT BW... return throttle\n");
			break;
		}

		/* switch to decouple mode */
		if (disp_mgr_has_mem_session() ||
				layering_get_valid_hrt() >= 400) {
			/* enable HRT throttle */
			DISPINFO("Cam trigger repain\n");
			hrt_idx = layering_rule_get_hrt_idx();
			hrt_idx++;
			trigger_repaint(REFRESH_FOR_IDLE);
			primary_display_manual_unlock();
			for (i = 0 ; i < 5 ; ++i) {
				ret = wait_event_timeout(hrt_cond_wait_queue,
					atomic_read(&hrt_cond_sig), 1 * HZ);
				if (ret == 0)
					DISPINFO("wait repaint timeout %d\n",
					 i);
				atomic_set(&hrt_cond_sig, 0);
				if (atomic_read(&last_hrt_idx) >= hrt_idx)
					break;
			}
			primary_display_manual_lock();
			break;
		}

		/* disable idlemgr during such scenario */
		atomic_set(&idlemgr_task_active, 0);
		primary_display_idlemgr_kick((char *)__func__, 0);

		do_primary_display_switch_mode(DISP_SESSION_DECOUPLE_MODE,
				primary_get_sess_id(), 0, NULL, 0);
		hrt_bw_privilege = 1;

		set_is_dc(1);
		break;
	case BW_THROTTLE_END: /* CAM off */
		DISPMSG("DISP BW Throttle end\n");

		pri_disp_leave_privilege(0);

		break;
	default:
		break;
	}

	primary_display_manual_unlock();
#endif

	return 0;
}

#ifdef MTK_FB_MMDVFS_SUPPORT
static struct notifier_block pmqos_hrt_notifier = {
	.notifier_call = hrt_bw_cond_change_cb,
};
#endif

void hrt_bw_debug(unsigned int v)
{
	hrt_bw_cond_change_cb(NULL, v, NULL);
}

int primary_display_lowpower_init(void)
{
	struct LCM_PARAMS *params;

	params = primary_get_lcm()->params;
	backup_vfp_for_lp_cust(params->dsi.vertical_frontporch_for_low_power);

	/* init idlemgr */
	if (disp_helper_get_option(DISP_OPT_IDLE_MGR)
		/* get_boot_mode() == NORMAL_BOOT */
		)
		primary_display_idlemgr_init();

	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		primary_display_sodi_rule_init();

	/* CMD mode always enables share SRAM */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		; /* enter_share_sram(CMDQ_SYNC_RESOURCE_WROT0); */

#ifdef MTK_FB_MMDVFS_SUPPORT
	/****             LockProve issue               ****
	 *  register			called
	 *				rwsem(notifier head)
	 *  pgc->lock
	 *  rwsem(notifier head)
	 *				pgc->lock
	 *-------------------------------------------------
	 * Reason: For this case, lockdep tool use lock sequence to
	 *     detect lock flow, so, even deadlock won't
	 *     happens, and actually it is,
	 *     we still need to resolve it by unlock and lock.
	 * Flow: __func__(primary_display_lowpower_init) is called in
	 *     primary_display_init, and before call __func__, already
	 *     called _primary_path_lock(), so, here need unlock tempoarily.
	 */
	_primary_path_unlock(__func__);
	mm_hrt_add_bw_throttle_notifier(&pmqos_hrt_notifier);
	_primary_path_lock(__func__);

#endif

	return 0;
}

int primary_display_is_idle(void)
{
	return idlemgr_pgc->is_primary_idle;
}

void primary_display_idlemgr_kick(const char *source, int need_lock)
{
	char log[128] = "";

	mmprofile_log_ex(ddp_mmp_get_events()->idlemgr, MMPROFILE_FLAG_PULSE,
			 1, 0);

	scnprintf(log, sizeof(log), "[kick]%s kick at %lld\n",
		  source, sched_clock());
	kick_logger_dump(log);

	/*
	 * get primary lock to protect idlemgr_last_kick_time and
	 * primary_display_is_idle()
	 */
	if (need_lock)
		primary_display_manual_lock();

	/* update kick timestamp */
	idlemgr_pgc->idlemgr_last_kick_time = sched_clock();

	if (primary_display_is_idle()) {
		primary_display_idlemgr_leave_idle_nolock();
		primary_display_set_idle_stat(0);

		mmprofile_log_ex(ddp_mmp_get_events()->idlemgr,
				 MMPROFILE_FLAG_END, 0, 0);
		/* wake up idlemgr process to monitor next idle stat */
		wake_up_interruptible(&idlemgr_pgc->idlemgr_wait_queue);
	}

	if (need_lock)
		primary_display_manual_unlock();
}

void enter_share_sram(enum CMDQ_EVENT_ENUM resourceEvent)
{
	/* 1. register callback first */
	cmdq_mdp_set_resource_callback(resourceEvent,
				       _acquire_wrot_resource,
				       _release_wrot_resource);

	register_share_sram = 1;
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 0, 1);

	/* 2. try to allocate SRAM at the first time */
	_acquire_wrot_resource_nolock(resourceEvent);
}

void leave_share_sram(enum CMDQ_EVENT_ENUM resourceEvent)
{
	/* 1. unregister callback */
	cmdq_mdp_set_resource_callback(resourceEvent, NULL, NULL);

	register_share_sram = 0;
	mmprofile_log_ex(ddp_mmp_get_events()->share_sram,
		MMPROFILE_FLAG_PULSE, 0, 0);

	/* 2. try to release share SRAM */
	_release_wrot_resource_nolock(resourceEvent);
}

void set_hrtnum(unsigned int new_hrtnum)
{
	if (golden_setting_pgc->hrt_num == new_hrtnum)
		return;

	if ((golden_setting_pgc->hrt_num > golden_setting_pgc->hrt_magicnum &&
	     new_hrtnum <= golden_setting_pgc->hrt_magicnum) ||
	    (golden_setting_pgc->hrt_num <= golden_setting_pgc->hrt_magicnum &&
	     new_hrtnum > golden_setting_pgc->hrt_magicnum)) {
		/* should not on screenidle when set hrtnum */
		if (new_hrtnum > golden_setting_pgc->hrt_magicnum)
			golden_setting_pgc->fifo_mode = 2;
		else
			golden_setting_pgc->fifo_mode = 1;
	}
	golden_setting_pgc->hrt_num = new_hrtnum;
}

/* set enterulps flag after power on & power off */
void set_enterulps(unsigned int flag)
{
	idlemgr_pgc->enterulps = flag;
}

void set_is_dc(unsigned int is_dc)
{
	if (golden_setting_pgc->is_dc != is_dc)
		golden_setting_pgc->is_dc = is_dc;
}

/* return 0: no change / return 1: change */
unsigned int set_one_layer(unsigned int is_onelayer)
{
	if (golden_setting_pgc->is_one_layer == is_onelayer)
		return 0;

	golden_setting_pgc->is_one_layer = is_onelayer;

	return 1;
}

void set_rdma_width_height(unsigned int width, unsigned int height)
{
	golden_setting_pgc->rdma_width = width;
	golden_setting_pgc->rdma_height = height;
}

void enable_idlemgr(unsigned int flag)
{
	if (flag) {
		DISPCHECK("[LP]enable idlemgr\n");
		atomic_set(&idlemgr_task_active, 1);
		wake_up_interruptible(&idlemgr_pgc->idlemgr_wait_queue);
	} else {
		DISPCHECK("[LP]disable idlemgr\n");
		atomic_set(&idlemgr_task_active, 0);
		primary_display_idlemgr_kick((char *)__func__, 1);
	}
}

unsigned int get_idlemgr_flag(void)
{
	unsigned int idlemgr_flag;

	idlemgr_flag = atomic_read(&idlemgr_task_active);
	return idlemgr_flag;
}

unsigned int set_idlemgr(unsigned int flag, int need_lock)
{
	unsigned int old_flag = atomic_read(&idlemgr_task_active);

	if (flag) {
		DISPCHECK("[LP]enable idlemgr\n");
		atomic_set(&idlemgr_task_active, 1);
		wake_up_interruptible(&idlemgr_pgc->idlemgr_wait_queue);
	} else {
		DISPCHECK("[LP]disable idlemgr\n");
		atomic_set(&idlemgr_task_active, 0);
		primary_display_idlemgr_kick((char *)__func__, need_lock);
	}
	return old_flag;
}

unsigned int get_us_perline(unsigned int width)
{
	unsigned int PLLCLK = 0;
	unsigned int datarate = 0;
	unsigned int pixclk = 0;
	unsigned int tline = 0;
	unsigned int overhead = 12; /* 1.2 */

	PLLCLK = primary_get_lcm()->params->dsi.PLL_CLOCK;
	PLLCLK = PLLCLK * LINE_ACCURACY;
	PLLCLK = PLLCLK * 10 / overhead;

	datarate = PLLCLK * 2;

	pixclk = datarate * primary_get_lcm()->params->dsi.LANE_NUM;
	pixclk = pixclk / 24; /* dsi output RGB888 */

	tline = width * LINE_ACCURACY * LINE_ACCURACY / pixclk;

	return tline;
}

unsigned int time_to_line(unsigned int ms, unsigned int width)
{
	unsigned int tline_us = 0;
	unsigned long long time_us = 0;
	unsigned int line = 0;

	tline_us = get_us_perline(width);
	time_us = (unsigned long long)ms * 1000 * LINE_ACCURACY;
#if defined(__LP64__) || defined(_LP64)
	line = time_us / tline_us;
#else
	line = div_u64(time_us, tline_us);
#endif

	return line;
}

/* ------------------ external display ------------------ */

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
int external_display_idlemgr_init(void)
{
	wake_up_process(idlemgr_pgc->external_display_idlemgr_task);
	return 0;
}

static int external_display_set_idle_stat(int is_idle)
{
	int old_stat = idlemgr_pgc->is_external_idle;

	idlemgr_pgc->is_external_idle = is_idle;
	return old_stat;
}

/* Need blocking for stopping trigger loop */
int _ext_blocking_flush(void)
{
	int ret = 0;
	struct cmdqRecStruct *qhandle = NULL;

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &qhandle);
	if (ret) {
		DISP_PR_ERR("%s:%d, create cmdq handle fail! ret=%d\n",
			    __func__, __LINE__, ret);
		return -1;
	}
	/* Set fake cmdq engineflag for judge path scenario */
	cmdqRecSetEngine(qhandle, ((1LL << CMDQ_ENG_DISP_OVL1) |
				  (1LL << CMDQ_ENG_DISP_WDMA1)));

	cmdqRecReset(qhandle);

	_ext_cmdq_insert_wait_frame_done_token(qhandle);

	cmdqRecFlush(qhandle);
	cmdqRecDestroy(qhandle);
	qhandle = NULL;

	return ret;
}

void _external_display_disable_mmsys_clk(void)
{
	/* blocking flush before stopping trigger loop */
	_ext_blocking_flush();
	/* no need lock now */
	if (ext_disp_cmdq_enabled()) {
		DISPINFO("[LP]1.external dpy cmdq trigger loop stop[begin]\n");
		_cmdq_stop_extd_trigger_loop();
		DISPINFO("[LP]1.external_dpy cmdq trigger loop stop[end]\n");
	}

	dpmgr_path_stop(ext_disp_get_dpmgr_handle(), CMDQ_DISABLE);
	DISPINFO("[LP]2.external display path stop[end]\n");

	dpmgr_path_reset(ext_disp_get_dpmgr_handle(), CMDQ_DISABLE);

	/* can not release fence here */
	dpmgr_path_power_off_bypass_pwm(ext_disp_get_dpmgr_handle(),
					CMDQ_DISABLE);

	DISPINFO("[LP]3.external dpmanager path power off[end]\n");
}

static void _external_display_enable_mmsys_clk(void)
{
	struct disp_ddp_path_config *pconfig;
	struct ddp_io_golden_setting_arg gset_arg;

	/* do something */
	DISPINFO("[LP]1.external dpmanager path power on[begin]\n");
	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
		dpmgr_path_get_dst_module_type(ext_disp_get_dpmgr_handle());

	dpmgr_path_init(ext_disp_get_dpmgr_handle(), CMDQ_DISABLE);
	DISPINFO("[LP]1.external dpmanager path power on[end]\n");

	pconfig = dpmgr_path_get_last_config(ext_disp_get_dpmgr_handle());

	pconfig->dst_dirty = 1;
	pconfig->ovl_dirty = 1;
	pconfig->rdma_dirty = 1;
	dpmgr_path_config(ext_disp_get_dpmgr_handle(), pconfig, NULL);

	dpmgr_path_ioctl(ext_disp_get_dpmgr_handle(), NULL,
			 DDP_OVL_GOLDEN_SETTING, &gset_arg);

	DISPINFO("[LP]2.external dpmanager path config[end]\n");

	dpmgr_path_start(ext_disp_get_dpmgr_handle(), CMDQ_DISABLE);

	DISPINFO("[LP]3.external dpmgr path start[end]\n");

	if (dpmgr_path_is_busy(ext_disp_get_dpmgr_handle()))
		DISP_PR_ERR("[LP]3.didn't trigger ext path but already busy\n");

	if (ext_disp_cmdq_enabled()) {
		DISPINFO("[LP]4.start external cmdq[begin]\n");
		_cmdq_start_extd_trigger_loop();
		DISPINFO("[LP]4.start external cmdq[end]\n");
	}
}

void _ext_cmd_mode_enter_idle(void)
{
	DISPMSG("[LP]%s\n", __func__);

	/* please keep last */
	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS)) {
		/* need delay to make sure done??? */
		_external_display_disable_mmsys_clk();
	}
}

void _ext_cmd_mode_leave_idle(void)
{
	DISPMSG("[LP]%s\n", __func__);

	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
		_external_display_enable_mmsys_clk();
}

int external_display_idlemgr_enter_idle_nolock(void)
{
	int ret = 0;

	_ext_cmd_mode_enter_idle();
	return ret;
}

void external_display_idlemgr_leave_idle_nolock(void)
{
	_ext_cmd_mode_leave_idle();
}

static int _external_path_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	unsigned long long diff;

	msleep(16000);
	while (1) {
		msleep_interruptible(100); /* 100ms */
		ret = wait_event_interruptible(
					idlemgr_pgc->ext_idlemgr_wait_queue,
					atomic_read(&ext_idlemgr_task_wakeup));

		ext_disp_manual_lock();

		if (ext_disp_get_state() != EXTD_RESUME) {
			ext_disp_manual_unlock();
			ext_disp_wait_state(EXTD_RESUME, MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if (external_display_is_idle()) {
			ext_disp_manual_unlock();
			continue;
		}

		diff = local_clock() - idlemgr_pgc->ext_idlemgr_last_kick_time;
		if ((diff  / 1000) < 100 * 1000) {
			/* kicked in 100ms, it's not idle */
			ext_disp_manual_unlock();
			continue;
		}
		DISPINFO("[LP]external enter idle state\n");

		/* enter idle state */
		if (external_display_idlemgr_enter_idle_nolock() < 0) {
			ext_disp_manual_unlock();
			continue;
		}
		external_display_set_idle_stat(1);

		ext_disp_manual_unlock();

		wait_event_interruptible(idlemgr_pgc->ext_idlemgr_wait_queue,
					 !external_display_is_idle());

		if (kthread_should_stop())
			break;
	}

	return 0;
}

void external_display_sodi_rule_init(void)
{
	/* enable SODI when display driver is ready */
}

int external_display_lowpower_init(void)
{
	/* init idlemgr */
	if (disp_helper_get_option(DISP_OPT_IDLE_MGR)
		/* get_boot_mode() == NORMAL_BOOT */
		)
		external_display_idlemgr_init();

	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		external_display_sodi_rule_init();

	return 0;
}

int external_display_is_idle(void)
{
	return idlemgr_pgc->is_external_idle;
}

void enable_ext_idlemgr(unsigned int flag)
{
	if (flag) {
		DISPCHECK("[LP]enable ext_idlemgr\n");
		atomic_set(&ext_idlemgr_task_wakeup, 1);
		wake_up_interruptible(&(idlemgr_pgc->ext_idlemgr_wait_queue));
	} else {
		DISPCHECK("[LP]disable ext_idlemgr\n");
		atomic_set(&ext_idlemgr_task_wakeup, 0);
		external_display_idlemgr_kick((char *)__func__, 1);
	}
}

void external_display_idlemgr_kick(const char *source, int need_lock)
{
	char log[128] = "";

	/* DISP_SYSTRACE_BEGIN("%s\n", __func__); */
	scnprintf(log, sizeof(log), "[kick]%s kick at %lld\n",
		  source, sched_clock());
	kick_logger_dump(log);
	/*
	 * get primary lock to protect idlemgr_last_kick_time and
	 * primary_display_is_idle()
	 */
	if (need_lock)
		ext_disp_manual_lock();
	/* update kick timestamp */
	idlemgr_pgc->ext_idlemgr_last_kick_time = sched_clock();
	if (external_display_is_idle()) {
		external_display_idlemgr_leave_idle_nolock();
		external_display_set_idle_stat(0);

		/* wake up idlemgr process to monitor next idle stat */
		wake_up_interruptible(&(idlemgr_pgc->ext_idlemgr_wait_queue));
	}
	if (need_lock)
		ext_disp_manual_unlock();
	/* DISP_SYSTRACE_END(); */
}

#endif /* CONFIG_MTK_DUAL_DISPLAY_SUPPORT */
