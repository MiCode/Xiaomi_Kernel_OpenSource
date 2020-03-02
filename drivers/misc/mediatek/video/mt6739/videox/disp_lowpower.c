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
#include "ion_drv.h"
#include "mtk_ion.h"
/* #include "mtk_idle.h" */
/* #include "mt_spm_reg.h" */ /* FIXME: tmp comment */
#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
/* #include "pcm_def.h" */ /* FIXME: tmp comment */
/* #include "mtk_spm_idle.h" */
#include "mt-plat/mtk_smi.h"
#include "m4u.h"

#include "disp_drv_platform.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "disp_session.h"
#include "primary_display.h"
#include "disp_helper.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_manager.h"
#include "disp_lcm.h"
#include "ddp_clkmgr.h"
#include "mtk_smi.h"
#include "disp_drv_log.h"
#include "disp_lowpower.h"
#include "disp_arr.h"
#include "disp_rect.h"
#include "ddp_reg.h"
#include "layering_rule.h"
/*#include "mtk_dramc.h"*/
#include "disp_partial.h"
#ifdef MTK_FB_MMDVFS_SUPPORT
#include "mmdvfs_mgr.h"
#endif
#ifdef MTK_FB_SPM_SUPPORT
#include "mtk_spm_idle.h"
#endif

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

#define MMSYS_CLK_LOW (0)
#define MMSYS_CLK_HIGH (1)

#define idlemgr_pgc _get_idlemgr_context()
#define golden_setting_pgc _get_golden_setting_context()

#define kick_dump_max_length (1024 * 16 * 4)
static unsigned char kick_string_buffer_analysize[kick_dump_max_length] = {0};
static unsigned int kick_buf_length;
static atomic_t idlemgr_task_wakeup = ATOMIC_INIT(1);
#ifdef MTK_FB_MMDVFS_SUPPORT
/* dvfs */
static atomic_t dvfs_ovl_req_status = ATOMIC_INIT(HRT_LEVEL_DEFAULT);
#endif

/* Local API */
/*****************************************************************************/
static int _primary_path_idlemgr_monitor_thread(void *data);

static struct disp_idlemgr_context *_get_idlemgr_context(void)
{
	static int is_inited;
	static struct disp_idlemgr_context idlemgr_ctx;

	if (is_inited)
		return &idlemgr_ctx;

	init_waitqueue_head(&(idlemgr_ctx.idlemgr_wait_queue));
	idlemgr_ctx.session_mode_before_enter_idle = DISP_INVALID_SESSION_MODE;
	idlemgr_ctx.is_primary_idle = 0;
	idlemgr_ctx.enterulps = 0;
	idlemgr_ctx.idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx.cur_lp_cust_mode = 0;
	idlemgr_ctx.primary_display_idlemgr_task =
		kthread_create(_primary_path_idlemgr_monitor_thread,
			       NULL, "disp_idlemgr");

	is_inited = 1;

	return &idlemgr_ctx;
}

int primary_display_idlemgr_init(void)
{
	wake_up_process(idlemgr_pgc->primary_display_idlemgr_task);
	return 0;
}

static struct golden_setting_context *_get_golden_setting_context(void)
{
	static int is_inited;
	static struct golden_setting_context gs_ctx;

	if (is_inited)
		return &gs_ctx;

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

	return &gs_ctx;
}

static void set_fps(unsigned int fps)
{
	golden_setting_pgc->fps = fps;
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

/* Need blocking for stop trigger loop  */
int _blocking_flush(void)
{
	int ret = 0;
	struct cmdqRecStruct *handle = NULL;

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	if (ret) {
		DISPERR("%s:%d, create cmdq handle fail!ret=%d\n",
			__func__, __LINE__, ret);
		return -1;
	}
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);
	return ret;
}

int primary_display_dsi_vfp_change(int state)
{
	int ret = 0;
	struct cmdqRecStruct *handle = NULL;
	struct LCM_PARAMS *params;

	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);

	/* make sure token rdma_sof is clear */
	cmdqRecClearEventToken(handle, CMDQ_EVENT_DISP_RDMA0_SOF);

	/*
	 * wait RDMA0_SOF: only used for video mode & trigger loop
	 * need wait and clear RDMA0 SOF
	 */
	cmdqRecWaitNoClear(handle, CMDQ_EVENT_DISP_RDMA0_SOF);

	params = primary_get_lcm()->params;
	if (state == 1) {
		/* need calculate fps by vdo mode params */
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), handle,
				 DDP_DSI_PORCH_CHANGE,
				 &params->dsi.vertical_frontporch_for_low_power);
	} else if (state == 0) {
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), handle,
			DDP_DSI_PORCH_CHANGE,
			&params->dsi.vertical_frontporch);
	}
	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);
	return ret;
}

void _idle_set_golden_setting(void)
{
	struct cmdqRecStruct *handle;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	/* no need lock */
	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	cmdqRecReset(handle);

	/* 2.wait mutex0_stream_eof: only used for video mode */
	cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);

	/* 3.golden setting */
	dpmgr_path_ioctl(phandle, handle, DDP_RDMA_GOLDEN_SETTING, pconfig);

	/* 4.flush */
	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);
}

/* Share wrot sram for vdo mode increase enter sodi ratio */
void _acquire_wrot_resource_nolock(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct cmdqRecStruct *handle;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	int32_t acquireResult;
	struct disp_ddp_path_config *pconfig = NULL;

	pconfig = dpmgr_path_get_last_config_notclear(phandle);
	DISPINFO("[disp_lowpower]%s\n", __func__);
	if (use_wrot_sram())
		return;

	if (is_mipi_enterulps())
		return;

	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	cmdqRecReset(handle);

	/* 2. wait eof */
	_cmdq_insert_wait_frame_done_token_mira(handle);

	/* 3.try to share wrot sram */
	acquireResult = cmdqRecWriteForResource(handle, resourceEvent,
		disp_addr_convert(DISP_REG_RDMA_SRAM_SEL), 1, ~0);

	if (acquireResult < 0) {
		/* acquire resource fail */
		DISPINFO("acquire resource fail\n");
		cmdqRecDestroy(handle);
		return;

	}
	/* acquire resource success */
	DISPMSG("share SRAM success\n");

	/* set rdma golden setting parameters*/
	set_share_sram(1);

	/* add instr for modification rdma fifo regs */
	/* dpmgr_handle can cover both dc & dl */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(phandle, handle, DDP_RDMA_GOLDEN_SETTING,
				 pconfig);

	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);
}

static int32_t _acquire_wrot_resource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	primary_display_manual_lock();
	_acquire_wrot_resource_nolock(resourceEvent);
	primary_display_manual_unlock();

	return 0;
}

static int32_t _release_wrot_resource_nolock(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct cmdqRecStruct *handle;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;
	unsigned int rdma0_shadow_mode = 0;

	DISPMSG("[disp_lowpower]%s:use_wrot_sram:%d\n",
		__func__, use_wrot_sram());
	pconfig = dpmgr_path_get_last_config_notclear(phandle);

	if (use_wrot_sram() == 0)
		return -1;

	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	cmdqRecReset(handle);

	/* 2.wait eof */
	_cmdq_insert_wait_frame_done_token_mira(handle);

	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER) &&
	    (disp_helper_get_option(DISP_OPT_SHADOW_MODE)) != 2) {
		/*
		 * In CMD mode, after release WROT resource, primary display
		 * maybe enter idle mode, so the RDMA0 register only be written
		 * into shadow register. It will bring about RDMA0 and WROT1
		 * use SRAM in the same time. Fix this bug by bypass shadow
		 * register.
		 */
		/*
		 * RDMA0 backup shadow mode and change to shadow
		 * register bypass mode
		 */
		rdma0_shadow_mode = DISP_REG_GET(DISP_REG_RDMA_SHADOW_UPDATE);
		DISP_REG_SET(handle, DISP_REG_RDMA_SHADOW_UPDATE,
			     (0x1 << 1) | (0x0 << 2));

		/* 3.disable RDMA0 share sram */
		DISP_REG_SET(handle, DISP_REG_RDMA_SRAM_SEL, 0);

		/* RDMA0 recover shadow mode*/
		DISP_REG_SET(handle, DISP_REG_RDMA_SHADOW_UPDATE,
			     rdma0_shadow_mode);
	}

	/* 4.release share sram resourceEvent*/
	cmdqRecReleaseResource(handle, resourceEvent);

	/* set rdma golden setting parameters*/
	set_share_sram(0);

	/* 5.add instr for modification rdma fifo regs */
	/* rdma: dpmgr_handle can cover both dc & dl */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(phandle, handle, DDP_RDMA_GOLDEN_SETTING,
				 pconfig);

	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);

	return 0;
}

static int32_t _release_wrot_resource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	int32_t ret = 0;
	/* need lock  */
	primary_display_manual_lock();
	ret = _release_wrot_resource_nolock(resourceEvent);
	primary_display_manual_unlock();

	return ret;
}

int _switch_mmsys_clk_callback(unsigned int need_disable_pll)
{
#if 0
	/* disable vencpll */
	if (need_disable_pll == MM_VENCPLL) {
		ddp_clk_set_parent(MUX_MM, SYSPLL2_D2);
		ddp_clk_disable_unprepare(MUX_MM);
		ddp_clk_disable_unprepare(MM_VENCPLL);
	} else if (need_disable_pll == SYSPLL2_D2) {
		ddp_clk_set_parent(MUX_MM, MM_VENCPLL);
		ddp_clk_disable_unprepare(MUX_MM);
		ddp_clk_disable_unprepare(SYSPLL2_D2);
	}

#endif
	return 0;
}

int _switch_mmsys_clk(int mmsys_clk_old, int mmsys_clk_new)
{
	int ret = 0;
	struct cmdqRecStruct *handle;
	disp_path_handle phandle = primary_get_dpmgr_handle();
	struct disp_ddp_path_config *pconfig = NULL;

	DISPMSG("[disp_lowpower]%s\n", __func__);
	pconfig = dpmgr_path_get_last_config_notclear(phandle);
	if (mmsys_clk_new == get_mmsys_clk())
		return ret;

	if (primary_get_state() != DISP_ALIVE || is_mipi_enterulps()) {
		DISPERR("[disp_lowpower]_switch_mmsys_clk when display suspend old = %d & new = %d.\n",
			mmsys_clk_old, mmsys_clk_new);
		return ret;
	}
	/* 1.create and reset cmdq */
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);

	cmdqRecReset(handle);

	if (mmsys_clk_old == MMSYS_CLK_HIGH && mmsys_clk_new == MMSYS_CLK_LOW) {
		/* 2.wait sof */
		_cmdq_insert_wait_frame_done_token_mira(handle);
		/* set rdma golden setting parameters */
		set_mmsys_clk(MMSYS_CLK_LOW);
		/*need_disable_pll = MM_VENCPLL;*/

	} else if (mmsys_clk_old == MMSYS_CLK_LOW &&
		   mmsys_clk_new == MMSYS_CLK_HIGH) {
		/* 2.wait sof */
		_cmdq_insert_wait_frame_done_token_mira(handle);
		/* set rdma golden setting parameters */
		set_mmsys_clk(MMSYS_CLK_HIGH);
		/*need_disable_pll = SYSPLL2_D2;*/

	} else {
		goto cmdq_d;
	}

	/* 4.add instr for modification rdma fifo regs */
	/* rdma: dpmgr_handle can cover both dc & dl */
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		dpmgr_path_ioctl(primary_get_dpmgr_handle(), handle,
				 DDP_RDMA_GOLDEN_SETTING, pconfig);

	cmdqRecFlush(handle);

cmdq_d:
	cmdqRecDestroy(handle);

	/*_switch_mmsys_clk_callback(need_disable_pll);*/
	return get_mmsys_clk();
}

int primary_display_switch_mmsys_clk(int mmsys_clk_old, int mmsys_clk_new)
{
	/* need lock */
	DISPMSG("[disp_lowpower]%s\n", __func__);
	primary_display_manual_lock();
	_switch_mmsys_clk(mmsys_clk_old, mmsys_clk_new);
	primary_display_manual_unlock();

	return 0;
}

void _primary_display_disable_mmsys_clk(void)
{
	disp_path_handle phandle = primary_get_dpmgr_handle();
	if (primary_get_sess_mode() == DISP_SESSION_RDMA_MODE) {
		/* switch back to DL mode before suspend */
		do_primary_display_switch_mode(DISP_SESSION_DIRECT_LINK_MODE,
					       primary_get_sess_id(), 0,
					       NULL, 1);
	}
	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE)
		return;

	/* blocking flush before stop trigger loop */
	_blocking_flush();
	/* no  need lock now */
	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPINFO("[LP]1.display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPINFO("[LP]1.display cmdq trigger loop stop[end]\n");
	}

	DISPINFO("[LP]2.primary display path stop[begin]\n");
	dpmgr_path_stop(phandle, CMDQ_DISABLE);
	DISPINFO("[LP]2.primary display path stop[end]\n");

	if (dpmgr_path_is_busy(phandle)) {
		DISPERR("[LP]2.stop display path failed, still busy\n");
		dpmgr_path_reset(phandle, CMDQ_DISABLE);
		/* even path is busy(stop fail), we still need to continue power
		 * off other module/devices
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
			DISPERR("display is decouple mode, but ovl2mem_path_handle is null\n");

		DISPINFO("[LP]3.1 dpmanager path power off: ovl2men [end]\n");
	}
	DISPCHECK("[LP]3.dpmanager path power off[end]\n");
	if (disp_helper_get_option(DISP_OPT_MET_LOG))
		set_enterulps(1);
}

void _primary_display_enable_mmsys_clk(void)
{
	disp_path_handle phandle = primary_get_dpmgr_handle();
	disp_path_handle ovl2mem_phandle = primary_get_ovl2mem_handle();
	struct disp_ddp_path_config *data_config;
	struct ddp_io_golden_setting_arg gset_arg;

	if (primary_get_sess_mode() != DISP_SESSION_DIRECT_LINK_MODE)
		return;

	/* do something */
	DISPINFO("[LP]1.dpmanager path power on[begin]\n");
	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type = dpmgr_path_get_dst_module_type(phandle);
	if (primary_display_is_decouple_mode()) {
		if (ovl2mem_phandle == NULL) {
			DISPERR("display is decouple mode, but ovl2mem_path_handle is null\n");
			return;
		}

		gset_arg.is_decouple_mode = 1;
		DISPINFO("[LP]1.1 dpmanager path power on: ovl2men [begin]\n");
		dpmgr_path_power_on(ovl2mem_phandle, CMDQ_DISABLE);
		DISPINFO("[LP]1.1 dpmanager path power on: ovl2men [end]\n");
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

	dpmgr_path_connect(primary_get_dpmgr_handle(), CMDQ_DISABLE);
	if (primary_display_is_decouple_mode())
		dpmgr_path_connect(ovl2mem_phandle, CMDQ_DISABLE);

	data_config = dpmgr_path_get_last_config(phandle);
	if (disp_partial_is_support())
		primary_display_config_full_roi(data_config, phandle, NULL);

	data_config->dst_dirty = 1;
	data_config->ovl_dirty = 1;
	data_config->rdma_dirty = 1;
	dpmgr_path_config(phandle, data_config, NULL);

	if (primary_display_is_decouple_mode()) {

		data_config = dpmgr_path_get_last_config(phandle);
		data_config->rdma_dirty = 1;
		dpmgr_path_config(phandle, data_config, NULL);

		data_config = dpmgr_path_get_last_config(ovl2mem_phandle);
		data_config->dst_dirty = 1;
		dpmgr_path_config(ovl2mem_phandle, data_config, NULL);
		dpmgr_path_ioctl(ovl2mem_phandle, NULL, DDP_OVL_GOLDEN_SETTING,
				 &gset_arg);
	} else {
		dpmgr_path_ioctl(phandle, NULL, DDP_OVL_GOLDEN_SETTING,
				 &gset_arg);
	}

	DISPCHECK("[LP]2.dpmanager path config[end]\n");

	DISPDBG("[LP]3.dpmgr path start[begin]\n");
	dpmgr_path_start(primary_get_dpmgr_handle(), CMDQ_DISABLE);

	if (primary_display_is_decouple_mode())
		dpmgr_path_start(primary_get_ovl2mem_handle(), CMDQ_DISABLE);

	DISPINFO("[LP]3.dpmgr path start[end]\n");
	if (dpmgr_path_is_busy(primary_get_dpmgr_handle()))
		DISPERR("[LP]3.Fatal error, we didn't trigger display path but it's already busy\n");

	if (disp_helper_get_option(DISP_OPT_USE_CMDQ)) {
		DISPDBG("[LP]4.start cmdq[begin]\n");
		_cmdq_start_trigger_loop();
		DISPINFO("[LP]4.start cmdq[end]\n");
	}

	/* (in suspend) when we stop trigger loop*/
	/* if no other thread is running, cmdq may disable its clock*/
	/* all cmdq event will be cleared after suspend */
	cmdqCoreSetEvent(CMDQ_EVENT_DISP_WDMA0_EOF);
}

/* Share wrot sram end */
void _vdo_mode_enter_idle(void)
{
	int fps = 0;

	DISPMSG("[disp_lowpower]%s\n", __func__);

	/* backup for DL <-> DC */
	idlemgr_pgc->session_mode_before_enter_idle = primary_get_sess_mode();

	/* DL -> DC*/
	if (!primary_is_sec() &&
	    primary_get_sess_mode() == DISP_SESSION_DIRECT_LINK_MODE &&
	    (disp_helper_get_option(DISP_OPT_IDLEMGR_SWTCH_DECOUPLE) ||
	     disp_helper_get_option(DISP_OPT_SMART_OVL))) {

		/* smart_ovl_try_switch_mode_nolock(); */
		/* switch to decouple mode */
		do_primary_display_switch_mode(
			DISP_SESSION_DECOUPLE_MODE,
			primary_get_sess_id(), 0, NULL, 0);

		set_is_dc(1);
	}

	/* Disable irq & increase vfp */
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
				set_fps(45);
				primary_display_dsi_vfp_change(1);
				idlemgr_pgc->cur_lp_cust_mode = 1;
				break;
			case PERFORMANC_MODE: /* 60 */
				set_fps(primary_display_get_fps_nolock() / 100);
				primary_display_dsi_vfp_change(0);
				idlemgr_pgc->cur_lp_cust_mode = 0;
				break;
			}
		} else {
			struct LCM_PARAMS *params = primary_get_lcm()->params;

			if (get_backup_vfp() !=
			    params->dsi.vertical_frontporch_for_low_power)
				params->dsi.vertical_frontporch_for_low_power =
					get_backup_vfp();

			if (params->dsi.vertical_frontporch_for_low_power) {
#if 1
				if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
					fps = primary_display_get_min_refresh_rate();
					DISPMSG("vdo_mode_enter_idle fps to be %d\n",
						fps);
					/* second param 1 means enter idle */
					primary_display_force_set_vsync_fps(fps,
									    1);
				} else {
					set_fps(45);
					primary_display_dsi_vfp_change(1);
				}
#endif
				idlemgr_pgc->cur_lp_cust_mode = 1;
			}
		}
	}

	/* DC homeidle share wrot sram */

	/* set golden setting  , merge fps/dc */
	set_is_display_idle(1);
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		_idle_set_golden_setting();

	/* Enable sodi - need wait golden setting done ??? */
}

void _vdo_mode_leave_idle(void)
{
	int fps = 0;

	DISPMSG("[disp_lowpower]%s\n", __func__);

	/* set golden setting */
	set_is_display_idle(0);
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
		_idle_set_golden_setting();

	/* DC homeidle share wrot sram */

	/*if (disp_helper_get_option(DISP_OPT_SHARE_SRAM)*/
	/*&& (primary_get_sess_mode() == DISP_SESSION_DECOUPLE_MODE*/
	/*|| primary_get_sess_mode() == DISP_SESSION_RDMA_MODE))*/
	/*leave_share_sram(CMDQ_SYNC_RESOURCE_WROT0);*/

	/* Enable irq & restore vfp */
	if (!primary_is_sec()) {

		if (idlemgr_pgc->cur_lp_cust_mode != 0) {
#if 1
			if (disp_helper_get_option(DISP_OPT_ARR_PHASE_1)) {
				fps = primary_display_get_max_refresh_rate();
				DISPMSG("vdo_mode_leave_idle, fps to be 60\n");
				/* second parameter: 2 means leave ilde */
				primary_display_force_set_vsync_fps(fps, 2);
			} else {
				set_fps(primary_display_get_fps_nolock() / 100);
				primary_display_dsi_vfp_change(0);
			}
			idlemgr_pgc->cur_lp_cust_mode = 0;
			if (disp_helper_get_option(
				    DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
				_idle_set_golden_setting();
#endif
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
	    !disp_helper_get_option(DISP_OPT_SMART_OVL) &&
	    primary_get_sess_mode() == DISP_SESSION_DECOUPLE_MODE) {
		/* switch to the mode before idle */
		do_primary_display_switch_mode(
			idlemgr_pgc->session_mode_before_enter_idle,
			primary_get_sess_id(), 0, NULL, 0);

		set_is_dc(0);
		if (disp_helper_get_option(
			    DISP_OPT_DYNAMIC_RDMA_GOLDEN_SETTING))
			_idle_set_golden_setting();
	}
}

void _cmd_mode_enter_idle(void)
{
	DISPMSG("[disp_lowpower]%s\n", __func__);

	/* need leave share sram for disable mmsys clk */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		leave_share_sram(CMDQ_SYNC_RESOURCE_WROT0);

	/* please keep last */
	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS)) {
		_primary_display_disable_mmsys_clk();
	}
#ifdef MTK_FB_SPM_SUPPORT
	/*enter PD mode*/
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		spm_sodi_mempll_pwr_mode(0);
#endif
}

void _cmd_mode_leave_idle(void)
{
	DISPMSG("[disp_lowpower]%s\n", __func__);
#ifdef MTK_FB_SPM_SUPPORT
	/*Exit PD mode*/
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		spm_sodi_mempll_pwr_mode(1);
#endif
	if (disp_helper_get_option(DISP_OPT_IDLEMGR_ENTER_ULPS))
		_primary_display_enable_mmsys_clk();

	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		enter_share_sram(CMDQ_SYNC_RESOURCE_WROT0);
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

int primary_display_request_dvfs_perf(int scenario, int req)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	int step = MMDVFS_FINE_STEP_UNREQUEST;

	mmprofile_log_ex(ddp_mmp_get_events()->dvfs, MMPROFILE_FLAG_PULSE,
			 scenario, req);

	if ((scenario != MMDVFS_SCEN_DISP) ||
	    (atomic_read(&dvfs_ovl_req_status) != req)) {
		switch (req) {
		case HRT_LEVEL_UHPM:
			step = MMDVFS_FINE_STEP_OPP0;
			break;
		case HRT_LEVEL_HPM:
			step = MMDVFS_FINE_STEP_OPP3;
			break;
		case HRT_LEVEL_DEFAULT:
			step = MMDVFS_FINE_STEP_UNREQUEST;
			break;
		default:
			break;
		}
		mmdvfs_set_fine_step(scenario, step);

		if (scenario == MMDVFS_SCEN_DISP)
			atomic_set(&dvfs_ovl_req_status, req);
	}
#endif
	return 0;
}

static int _primary_path_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	unsigned long long t_idle;

	msleep(16000);
	while (1) {
		msleep_interruptible(100); /* 100ms */
		ret = wait_event_interruptible(
			idlemgr_pgc->idlemgr_wait_queue,
			atomic_read(&idlemgr_task_wakeup));

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

#ifdef CONFIG_MTK_DISPLAY_120HZ_SUPPORT
		if (primary_display_get_lcm_refresh_rate() == 120) {
			primary_display_manual_unlock();
			continue;
		}
#endif
		t_idle = local_clock() - idlemgr_pgc->idlemgr_last_kick_time;
		if ((t_idle / 1000) < 100 * 1000) {
			/* kicked in 500ms, it's not idle */
			primary_display_manual_unlock();
			continue;
		}
		mmprofile_log_ex(ddp_mmp_get_events()->idlemgr,
				 MMPROFILE_FLAG_START, 0, 0);
		DISPINFO("[disp_lowpower]primary enter idle state\n");
		dprec_logger_start(DPREC_LOGGER_IDLEMGR, 0, 0);

		/* enter idle state */
		primary_display_idlemgr_enter_idle_nolock();
		primary_display_set_idle_stat(1);

#ifdef MTK_FB_MMDVFS_SUPPORT
		/* when screen idle:let smi know */
		primary_display_request_dvfs_perf(SMI_BWC_SCEN_UI_IDLE,
						  HRT_LEVEL_HPM);
		primary_display_request_dvfs_perf(MMDVFS_SCEN_DISP,
						  HRT_LEVEL_HPM);

#endif

		primary_display_manual_unlock();

		wait_event_interruptible(idlemgr_pgc->idlemgr_wait_queue,
					 !primary_display_is_idle());

#ifdef MTK_FB_MMDVFS_SUPPORT
		/* when leave screen idle: reset to default */
		primary_display_request_dvfs_perf(SMI_BWC_SCEN_UI_IDLE,
						  HRT_LEVEL_DEFAULT);
#endif
		if (kthread_should_stop())
			break;
	}

	return 0;
}

void kick_logger_dump(char *string)
{
	if (kick_buf_length + strlen(string) >= kick_dump_max_length)
		kick_logger_dump_reset();

	kick_buf_length +=
		scnprintf(kick_string_buffer_analysize + kick_buf_length,
			  kick_dump_max_length - kick_buf_length, string);
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
/*********************************************************************************************************************/
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

void primary_display_sodi_enable(int flag)
{
#ifdef MTK_FB_SPM_SUPPORT
	spm_enable_sodi(flag);
#endif
}

/* for met - end */
void primary_display_sodi_rule_init(void)
{
#ifdef MTK_FB_SPM_SUPPORT
	/* enable sodi when display driver is ready */
	if (primary_display_is_video_mode()) {
		spm_sodi_set_vdo_mode(1);
		spm_sodi_mempll_pwr_mode(1);
		spm_enable_sodi3(0);
		spm_enable_sodi(1);
	} else {
		spm_enable_sodi3(1);
		spm_enable_sodi(1);
		/*enable CG mode*/
		spm_sodi_mempll_pwr_mode(1);
	}
#endif
}

int primary_display_lowpower_init(void)
{
	struct LCM_PARAMS *params = primary_get_lcm()->params;

	set_fps(primary_display_get_fps_nolock() / 100);
	backup_vfp_for_lp_cust(params->dsi.vertical_frontporch_for_low_power);
/* init idlemgr */
#ifdef CONFIG_MTK_BOOT
	if (disp_helper_get_option(DISP_OPT_IDLE_MGR) &&
	    get_boot_mode() == NORMAL_BOOT)
		primary_display_idlemgr_init();
#endif
	if (disp_helper_get_option(DISP_OPT_SODI_SUPPORT))
		primary_display_sodi_rule_init();

	/* always enable share sram */
	if (disp_helper_get_option(DISP_OPT_SHARE_SRAM))
		enter_share_sram(CMDQ_SYNC_RESOURCE_WROT0);

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

	snprintf(log, sizeof(log), "[kick]%s kick at %lld\n",
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
		dprec_logger_done(DPREC_LOGGER_IDLEMGR, 0, 0);
		/* wake up idlemgr process to monitor next idle stat */
		wake_up_interruptible(&(idlemgr_pgc->idlemgr_wait_queue));
	}

	if (need_lock)
		primary_display_manual_unlock();
}

#if 0
void exit_pd_by_cmdq(struct cmdqRecStruct *handler)
{
	/* Enable SPM CG Mode(Force 30+ times to ensure write success, need find root cause and fix later) */
	cmdqRecWrite(handler, 0x100062B0, 0x2, ~0);
	/* Polling EMI Status to ensure EMI is enabled */
	cmdqRecPoll(handler, 0x1000611C, 0x10000, 0x10000);
}

void enter_pd_by_cmdq(struct cmdqRecStruct *handler)
{
	cmdqRecWrite(handler, 0x100062B0, 0x0, 0x2);
}
#endif

void __attribute__((weak)) exit_pd_by_cmdq(struct cmdqRecStruct *handler)
{
}

void __attribute__((weak)) enter_pd_by_cmdq(struct cmdqRecStruct *handler)
{
}

void enter_share_sram(enum CMDQ_EVENT_ENUM resourceEvent)
{
	/* 1. register call back first */
	cmdq_mdp_set_resource_callback(CMDQ_SYNC_RESOURCE_WROT0,
				       _acquire_wrot_resource,
				       _release_wrot_resource);

	/* 2. try to allocate sram at the fisrt time */
	_acquire_wrot_resource_nolock(CMDQ_SYNC_RESOURCE_WROT0);
}

void leave_share_sram(enum CMDQ_EVENT_ENUM resourceEvent)
{
	/* 1. unregister call back */
	cmdq_mdp_set_resource_callback(CMDQ_SYNC_RESOURCE_WROT0, NULL, NULL);

	/* 2. try to release share sram */
	_release_wrot_resource_nolock(CMDQ_SYNC_RESOURCE_WROT0);
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
		DISPCHECK("[disp_lowpower]enable idlemgr\n");
		atomic_set(&idlemgr_task_wakeup, 1);
		wake_up_interruptible(&(idlemgr_pgc->idlemgr_wait_queue));
	} else {
		DISPCHECK("[disp_lowpower]disable idlemgr\n");
		atomic_set(&idlemgr_task_wakeup, 0);
		primary_display_idlemgr_kick((char *)__func__, 1);
	}
}

unsigned int get_idlemgr_flag(void)
{
	unsigned int idlemgr_flag;

	idlemgr_flag = atomic_read(&idlemgr_task_wakeup);
	return idlemgr_flag;
}

unsigned int set_idlemgr(unsigned int flag, int need_lock)
{
	unsigned int old_flag = atomic_read(&idlemgr_task_wakeup);

	if (flag) {
		DISPCHECK("[disp_lowpower]enable idlemgr\n");
		atomic_set(&idlemgr_task_wakeup, 1);
		wake_up_interruptible(&(idlemgr_pgc->idlemgr_wait_queue));
	} else {
		DISPCHECK("[disp_lowpower]disable idlemgr\n");
		atomic_set(&idlemgr_task_wakeup, 0);
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
	pixclk = pixclk / 24; /* dsi out put RGB888 */

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
