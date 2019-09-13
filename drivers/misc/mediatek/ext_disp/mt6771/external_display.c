/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/vmalloc.h>

#include "mtkfb_info.h"
#include "mtkfb.h"

#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_info.h"

#include <m4u.h>
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "ddp_manager.h"
#include "ddp_mmp.h"
#include "ddp_ovl.h"
#include "ddp_reg.h"
#include "lcm_drv.h"

#include "extd_platform.h"
#include "extd_log.h"
#include "extd_utils.h"
#include "extd_hdmi_types.h"
#include "external_display.h"

#include "disp_session.h"
#include "disp_lowpower.h"
#include "display_recorder.h"
#include "extd_info.h"

#include "mtkfb_fence.h"
#include "disp_drv_log.h"

#ifdef EXTD_SHADOW_REGISTER_SUPPORT
#include "disp_helper.h"
#endif



int ext_disp_use_cmdq;
int ext_disp_use_m4u;
enum EXT_DISP_PATH_MODE ext_disp_mode;

static int is_context_inited;
static int init_roi;
static unsigned int gCurrentPresentFenceIndex = -1;

struct ext_disp_path_context {
	enum EXTD_POWER_STATE state;
	enum EXTD_OVL_REQ_STATUS ovl_req_state;
	int init;
	unsigned int session;
	int need_trigger_overlay;
	int suspend_config;
	enum EXT_DISP_PATH_MODE mode;
	unsigned int last_vsync_tick;
	struct mutex lock;
	struct mutex vsync_lock;
	struct cmdqRecStruct *cmdq_handle_config;
	struct cmdqRecStruct *cmdq_handle_trigger;
	disp_path_handle dpmgr_handle;
	disp_path_handle ovl2mem_path_handle;
	cmdqBackupSlotHandle ext_cur_config_fence;
	cmdqBackupSlotHandle ext_subtractor_when_free;
	/*
	 * cmdqBackupSlotHandle ext_input_config_info
	 * bit0-7: layer_type
	 * bit8: 0 = source is ovl, 1 = source is rdma
	 */
	cmdqBackupSlotHandle ext_input_config_info;
#ifdef EXTD_DEBUG_SUPPORT
	cmdqBackupSlotHandle ext_ovl_rdma_status_info;
#endif
};

#define pgc	_get_context()

struct LCM_PARAMS extd_lcm_params;

atomic_t g_extd_trigger_ticket = ATOMIC_INIT(1);
atomic_t g_extd_release_ticket = ATOMIC_INIT(1);

static struct ext_disp_path_context *_get_context(void)
{
	static struct ext_disp_path_context g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0, sizeof(g_context));
		is_context_inited = 1;
		EXT_DISP_LOG("%s set is_context_inited\n", __func__);
	}

	return &g_context;
}

/*********************** Upper Layer To HAL ******************************/
static struct EXTERNAL_DISPLAY_UTIL_FUNCS external_display_util = { 0 };

void
extd_disp_drv_set_util_funcs(const struct EXTERNAL_DISPLAY_UTIL_FUNCS *util)
{
	memcpy(&external_display_util, util, sizeof(external_display_util));
}

enum EXT_DISP_PATH_MODE ext_disp_path_get_mode(unsigned int session)
{
	return ext_disp_mode;
}

void ext_disp_path_set_mode(enum EXT_DISP_PATH_MODE mode, unsigned int session)
{
	ext_disp_mode = EXTD_DIRECT_LINK_MODE;
	init_roi = (mode == EXTD_DIRECT_LINK_MODE ? 1 : 0);
}

static void _ext_disp_path_lock(void)
{
	extd_sw_mutex_lock(NULL); /* (&(pgc->lock)); */
}

static void _ext_disp_path_unlock(void)
{
	extd_sw_mutex_unlock(NULL); /* (&(pgc->lock)); */
}

static void _ext_disp_vsync_lock(unsigned int session)
{
	mutex_lock(&(pgc->vsync_lock));
}

static void _ext_disp_vsync_unlock(unsigned int session)
{
	mutex_unlock(&(pgc->vsync_lock));
}

static enum DISP_MODULE_ENUM _get_dst_module_by_lcm(disp_path_handle pHandle)
{
	return DISP_MODULE_DSI1;
}

/*
 * trigger operation:   VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 1.wait idle:         N         N        Y        Y
 * 2.lcm update:        N         Y        N        Y
 * 3.path start:        idle->Y   Y        idle->Y  Y
 * 4.path trigger:      idle->Y   Y        idle->Y  Y
 * 5.mutex enable:      N         N        idle->Y  Y
 * 6.set cmdq dirty:    N         Y        N        N
 * 7.flush cmdq:        Y         Y        N        N
 * 8.reset cmdq:        Y         Y        N        N
 * 9.cmdq insert token: Y         Y        N        N
 */

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 *	1.wait idle:	     N         N        Y        Y
 */
static int _should_wait_path_idle(void)
{
	if (ext_disp_cmdq_enabled())
		return 0;

	return dpmgr_path_is_busy(pgc->dpmgr_handle);
}

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 3.path start:       idle->Y   Y        idle->Y  Y
 */
static int _should_start_path(void)
{
	if (ext_disp_is_video_mode())
		return dpmgr_path_is_idle(pgc->dpmgr_handle);

	return 1;
}

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 4. path trigger:    idle->Y   Y        idle->Y  Y
 * 5. mutex enable:    N         N        idle->Y  Y
 */
static int _should_trigger_path(void)
{
	if (ext_disp_is_video_mode())
		return dpmgr_path_is_idle(pgc->dpmgr_handle);
	else if (ext_disp_cmdq_enabled())
		return 0;
	else
		return 1;
}

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 6. set cmdq dirty:  N         Y        N        N
 */
static int _should_set_cmdq_dirty(void)
{
	if (ext_disp_cmdq_enabled() && (ext_disp_is_video_mode() == 0))
		return 1;

	return 0;
}

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 7. flush cmdq:      Y         Y        N        N
 */
static int _should_flush_cmdq_config_handle(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

/*
 * trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 8. reset cmdq:      Y         Y        N        N
 */
static int _should_reset_cmdq_config_handle(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

/*
 * trigger operation:      VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 9. cmdq insert token:   Y         Y        N        N
 */
static int _should_insert_wait_frame_done_token(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

static int _should_trigger_interface(void)
{
	if (pgc->mode == EXTD_DECOUPLE_MODE)
		return 0;

	return 1;
}

static int _should_config_ovl_input(void)
{
	if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE ||
	    ext_disp_mode == EXTD_RDMA_DPI_MODE)
		return 0;

	return 1;
}

static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static int _build_path_direct_link(void)
{
	int ret = 0;
	struct M4U_PORT_STRUCT sPort = { 0 };

	EXT_DISP_FUNC();
	pgc->mode = EXTD_DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_DISP,
					      pgc->cmdq_handle_config);
	if (!pgc->dpmgr_handle) {
		EXT_DISP_LOG("dpmgr create path FAIL\n");
		return -1;
	}
	EXT_DISP_LOG("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);

	sPort.ePortID = M4U_PORT_DISP_2L_OVL1_LARB0;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
#ifdef MTKFB_M4U_SUPPORT
	ret = m4u_config_port(&sPort);
#endif
	if (ret) {
		EXT_DISP_LOG("config M4U Port %s to %s FAIL(ret=%d)\n",
			     ddp_get_module_name(DISP_MODULE_OVL1_2L),
			     ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}
	EXT_DISP_LOG("config M4U Port %s to %s SUCCESS\n",
		     ddp_get_module_name(DISP_MODULE_OVL1_2L),
		     ext_disp_use_m4u ? "virtual" : "physical");

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, NULL);
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

static int _build_path_rdma_dpi(void)
{
	int ret = 0;
	struct M4U_PORT_STRUCT sPort = { 0 };

	enum DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = EXTD_RDMA_DPI_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP,
					      pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		EXT_DISP_LOG("dpmgr create path SUCCESS(%p)\n",
			     pgc->dpmgr_handle);
	} else {
		EXT_DISP_LOG("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->dpmgr_handle);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	EXT_DISP_LOG("dpmgr set dst module FINISHED(%s)\n",
		     ddp_get_module_name(dst_module));

	sPort.ePortID = M4U_PORT_DISP_RDMA1;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
#ifdef MTKFB_M4U_SUPPORT
	ret = m4u_config_port(&sPort);
#endif
	if (ret == 0) {
		EXT_DISP_LOG("config M4U Port %s to %s SUCCESS\n",
			     ddp_get_module_name(DISP_MODULE_RDMA1),
			     ext_disp_use_m4u ? "virtual" : "physical");
	} else {
		EXT_DISP_LOG("config M4U Port %s to %s FAIL(ret=%d)\n",
			     ddp_get_module_name(DISP_MODULE_RDMA1),
			     ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, NULL);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;

	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
	EXT_DISP_LOG("ext_disp path trigger thread cmd handle=%p\n",
		     pgc->cmdq_handle_trigger);
	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (ext_disp_is_video_mode()) {
		/*
		 * wait and clear stream_done, HW will assert mutex enable
		 * automatically in frame done reset.
		 * TODO: should let dpmanager to decide wait which mutex's eof.
		 */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
				  dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				  CMDQ_EVENT_MUTEX0_STREAM_EOF);

		/* for some moduleto read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF, 0);
	} else {
		/* DSI command mode need use CMDQ token instead */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
				  CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		/* for operations before frame transfer, */
		/* such as waiting for DSI TE */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_BEFORE_STREAM_SOF, 0);

		/*
		 * clear frame done token, now the config thread will not be
		 * allowed to config registers.
		 * remember that config thread's priority is higher than
		 * trigger thread
		 * so all the config queued before will be applied
		 * then STREAM_EOF token be cleared
		 * this is what CMDQ did as "Merge"
		 */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
					     CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* enable mutex, only cmd mode need this */
		/* this is what CMDQ did as "Trigger" */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				   CMDQ_ENABLE);

		/*
		 * waiting for frame done,
		 * because we can't use mutex stream eof here
		 * so need to let dpmanager help to
		 * decide which event to wait
		 * most time we wait rdmax frame done event.
		 */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
				  CMDQ_EVENT_DISP_RDMA1_EOF);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_STREAM_EOF_EVENT, 0);

		/*
		 * dsi is not idle rightly after rdma frame done,
		 * so we need to polling about 1us for dsi returns to idle
		 * do not polling dsi idle directly
		 * which will decrease CMDQ performance
		 */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_CHECK_IDLE_AFTER_STREAM_EOF, 0);

		/* for some module to read hw register after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF, 0);

		/*
		 * now frame done,
		 * config thread is allowed to config register now
		 */
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger,
					   CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* RUN forever!!!! */
		WARN_ON(ret < 0);
	}

	/* dump trigger loop instructions to check */
	/* whether dpmgr_path_build_cmdq works correctly */
	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	EXT_DISP_LOG("ext display BUILD cmdq trigger loop finished\n");
}

static void _cmdq_start_extd_trigger_loop(void)
{
	int ret = 0;

	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!ext_disp_is_video_mode())
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);

	EXT_DISP_LOG("START cmdq trigger loop finished\n");
}

static void _cmdq_stop_extd_trigger_loop(void)
{
	int ret = 0;

	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);

	EXT_DISP_LOG("ext display STOP cmdq trigger loop finished\n");
}

static void _cmdq_set_config_handle_dirty(void)
{
	if (!ext_disp_is_video_mode())
		cmdqRecSetEventToken(pgc->cmdq_handle_config,
				     CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
}

static void _cmdq_flush_config_handle(int blocking, void *callback,
				      unsigned int userdata)
{
	if (blocking) {
		/* it will be blocked until mutex done */
		cmdqRecFlush(pgc->cmdq_handle_config);
	} else {
		if (callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config,
						  callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}
}

static void _cmdq_insert_wait_frame_done_token(int clear_event)
{
	if (ext_disp_is_video_mode()) {
		if (clear_event == 0) {
			cmdqRecWaitNoClear(pgc->cmdq_handle_config,
				   dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				   CMDQ_EVENT_MUTEX0_STREAM_EOF);
		} else {
			cmdqRecWait(pgc->cmdq_handle_config,
				    dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				    CMDQ_EVENT_MUTEX0_STREAM_EOF);
		}
	} else {
		if (clear_event == 0)
			cmdqRecWaitNoClear(pgc->cmdq_handle_config,
					   CMDQ_SYNC_TOKEN_STREAM_EOF);
		else
			cmdqRecWait(pgc->cmdq_handle_config,
				    CMDQ_SYNC_TOKEN_STREAM_EOF);
	}
}

static int _convert_disp_input_to_rdma(struct RDMA_CONFIG_STRUCT *dst,
				       struct disp_input_config *src,
				       unsigned int screen_w,
				       unsigned int screen_h)
{
	unsigned int Bpp = 0;
	unsigned long mva_offset = 0;
	enum UNIFIED_COLOR_FMT tmp_fmt;

	if (!src || !dst) {
		EXT_DISP_ERR("%s src(0x%p) or dst(0x%p) is null\n", __func__,
			     src, dst);
		return -1;
	}

	dst->idx = src->next_buff_idx;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	ufmt_disable_X_channel(tmp_fmt, &dst->inputFormat, NULL);

	Bpp = UFMT_GET_Bpp(dst->inputFormat);
	mva_offset = (src->src_offset_x + src->src_offset_y *
		      src->src_pitch) * Bpp;

	dst->address = (unsigned long)src->src_phy_addr + mva_offset;
	dst->pitch = src->src_pitch * Bpp;

	dst->width = min(src->src_width, src->tgt_width);
	dst->height = min(src->src_height, src->tgt_height);
	dst->security = src->security;
	dst->yuv_range = src->yuv_range;

	dst->dst_y = src->tgt_offset_y;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_h = screen_h;
	dst->dst_w = screen_w;

	return 0;
}

static int _convert_disp_input_to_ovl(struct OVL_CONFIG_STRUCT *dst,
				      struct disp_input_config *src)
{
	int force_disable_alpha = 0;
	enum UNIFIED_COLOR_FMT tmp_fmt;
	unsigned int Bpp = 0;

	if (!src || !dst) {
		EXT_DISP_ERR("%s src(0x%p) or dst(0x%p) is null\n", __func__,
			     src, dst);
		return -1;
	}

	dst->layer = src->layer_id;
	dst->isDirty = 1;
	dst->buff_idx = src->next_buff_idx;
	dst->layer_en = src->layer_enable;

	/* if layer is disable, we just need config above params. */
	if (!src->layer_enable)
		return 0;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	/*
	 * display does not support X channel, like XRGB8888
	 * we need to enable const_bld
	 */
	ufmt_disable_X_channel(tmp_fmt, &dst->fmt, &dst->const_bld);
#if 0
	if (tmp_fmt != dst->fmt)
		force_disable_alpha = 1;
#endif
	Bpp = UFMT_GET_Bpp(dst->fmt);

	dst->addr = (unsigned long)src->src_phy_addr;
	dst->vaddr = (unsigned long)src->src_base_addr;
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->src_pitch = src->src_pitch * Bpp;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;

	/* dst W/H should <= src W/H */
	dst->dst_w = min(src->src_width, src->tgt_width);
	dst->dst_h = min(src->src_height, src->tgt_height);

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
		dst->source = OVL_LAYER_SOURCE_RESERVED;
	} else if (src->buffer_source == DISP_BUFFER_ION ||
		   src->buffer_source == DISP_BUFFER_MVA) {
		dst->source = OVL_LAYER_SOURCE_MEM;
	} else {
		EXT_DISP_ERR("unknown source = %d", src->buffer_source);
		dst->source = OVL_LAYER_SOURCE_MEM;
	}
#ifdef EXTD_SMART_OVL_SUPPORT
	dst->ext_sel_layer = src->ext_sel_layer;
#endif

	return 0;
}

static int _ext_disp_trigger(int blocking, void *callback,
			     unsigned int userdata)
{
	bool reg_flush = false;

	EXT_DISP_FUNC();

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	if (_should_start_path()) {
		reg_flush = true;
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
				 MMPROFILE_FLAG_PULSE, Trigger, 1);
	}


	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
				   ext_disp_cmdq_enabled());
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle()) {
		if (reg_flush)
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
					 MMPROFILE_FLAG_PULSE, Trigger, 2);

		_cmdq_flush_config_handle(blocking, callback, userdata);
	}

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

#ifdef EXTD_SHADOW_REGISTER_SUPPORT
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER) &&
	    disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
		/* next frame starts here */
	} else {
		if (_should_insert_wait_frame_done_token())
			_cmdq_insert_wait_frame_done_token(0);
	}
#else
	if (_should_insert_wait_frame_done_token())
		_cmdq_insert_wait_frame_done_token(0);
#endif

	return 0;
}

static int _ext_disp_trigger_EPD(int blocking, void *callback,
				 unsigned int userdata)
{
	/* EXT_DISP_FUNC(); */

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	if (_should_start_path()) {
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
				 MMPROFILE_FLAG_PULSE, Trigger, 1);
	}


	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
				   ext_disp_cmdq_enabled());
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

#ifdef EXTD_SHADOW_REGISTER_SUPPORT
	if (disp_helper_get_option(DISP_OPT_SHADOW_REGISTER) &&
	    disp_helper_get_option(DISP_OPT_SHADOW_MODE) == 0) {
		/* next frame starts here */
	} else {
		if (_should_insert_wait_frame_done_token())
			_cmdq_insert_wait_frame_done_token(1);
	}
#else
	if (_should_insert_wait_frame_done_token())
		_cmdq_insert_wait_frame_done_token(0);
#endif

	return 0;
}

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);

	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);

	return 0;
}

void ext_disp_probe(void)
{
	EXT_DISP_FUNC();

	ext_disp_use_cmdq = CMDQ_ENABLE;
	ext_disp_use_m4u = 1;
	ext_disp_mode = EXTD_DIRECT_LINK_MODE;
}

int ext_disp_init(char *lcm_name, unsigned int session)
{
	struct disp_ddp_path_config *pconfig = NULL;
	enum EXT_DISP_STATUS ret;

	EXT_DISP_FUNC();
	ret = EXT_DISP_STATUS_OK;

	dpmgr_init();

	init_cmdq_slots(&(pgc->ext_cur_config_fence), EXTD_OVERLAY_CNT, 0);
	init_cmdq_slots(&(pgc->ext_subtractor_when_free), EXTD_OVERLAY_CNT, 0);
	init_cmdq_slots(&(pgc->ext_input_config_info), 1, 0);
#ifdef EXTD_DEBUG_SUPPORT
	init_cmdq_slots(&(pgc->ext_ovl_rdma_status_info), 1, 0);
#endif

	extd_mutex_init(&pgc->lock);
	_ext_disp_path_lock();

#if 0
	ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP, cmdqDdpClockOn,
				 cmdqDdpDumpInfo, cmdqDdpResetEng,
				 cmdqDdpClockOff);
	if (ret) {
		EXT_DISP_ERR("cmdqCoreRegisterCB failed, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}
#endif
	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &pgc->cmdq_handle_config);
	if (ret) {
		EXT_DISP_LOG("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}
	EXT_DISP_LOG("cmdqRecCreate SUCCESS, g_cmdq_handle=%p\n",
		     pgc->cmdq_handle_config);

	if (ext_disp_mode == EXTD_DIRECT_LINK_MODE)
		_build_path_direct_link();
	else if (ext_disp_mode == EXTD_DECOUPLE_MODE)
		_build_path_decouple();
	else if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE)
		_build_path_single_layer();
	else if (ext_disp_mode == EXTD_RDMA_DPI_MODE)
		_build_path_rdma_dpi();
	else
		EXT_DISP_LOG("ext_disp display mode is WRONG\n");

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		if (DISP_SESSION_DEV(session) != DEV_EINK + 1) {
			_cmdq_build_trigger_loop();
			_cmdq_start_extd_trigger_loop();
		}
	}
	pgc->session = session;

	EXT_DISP_LOG("ext_disp display START cmdq trigger loop finished\n");

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, ext_disp_is_video_mode());
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);

	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	if (pconfig) {
		memset((void *)pconfig, 0, sizeof(*pconfig));
		memcpy(&(pconfig->dispif_config), &extd_lcm_params,
		       sizeof(struct LCM_PARAMS));

		pconfig->dst_w = extd_lcm_params.dpi.width;
		pconfig->dst_h = extd_lcm_params.dpi.height;
		if (extd_lcm_params.dpi.dsc_enable == 1)
			pconfig->dst_w = extd_lcm_params.dpi.width * 3;
		pconfig->dst_dirty = 1;
		pconfig->p_golden_setting_context = get_golden_setting_pgc();
		pconfig->p_golden_setting_context->ext_dst_width =
							pconfig->dst_w;
		pconfig->p_golden_setting_context->ext_dst_height =
							pconfig->dst_h;

		init_roi = 0;
		ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig,
					NULL);
		EXT_DISP_LOG("%s roi w:%d, h:%d\n",
			     __func__, pconfig->dst_w, pconfig->dst_h);
	} else {
		EXT_DISP_LOG("allocate buffer failed!!!\n");
	}

	/* this will be set to always enable cmdq later */
	if (ext_disp_is_video_mode())
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				       DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA1_DONE);

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_reset_config_handle();

	atomic_set(&g_extd_trigger_ticket, 1);
	atomic_set(&g_extd_release_ticket, 0);

	mutex_init(&(pgc->vsync_lock));
	pgc->state = EXTD_INIT;
	pgc->ovl_req_state = EXTD_OVL_NO_REQ;

done:
	_ext_disp_path_unlock();

	EXT_DISP_LOG("%s done\n", __func__);
	return ret;
}

int ext_disp_deinit(unsigned int session)
{
	int loop_cnt = 0;

	EXT_DISP_FUNC();

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT)
		goto out;

	while (((atomic_read(&g_extd_trigger_ticket) -
		 atomic_read(&g_extd_release_ticket)) != 1) &&
	       (loop_cnt < 10)) {
		_ext_disp_path_unlock();
		usleep_range(5000, 6000);
		_ext_disp_path_lock();
		/* wait the last configuration done */
		loop_cnt++;
	}

	if (pgc->state == EXTD_SUSPEND)
		dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_destroy_path_handle(pgc->dpmgr_handle);

	cmdqRecDestroy(pgc->cmdq_handle_config);
	cmdqRecDestroy(pgc->cmdq_handle_trigger);

	pgc->state = EXTD_DEINIT;

out:
	_ext_disp_path_unlock();
	is_context_inited = 0;
	EXT_DISP_LOG("%s done\n", __func__);
	return 0;
}

int ext_disp_wait_for_vsync(void *config, unsigned int session)
{
	int ret = 0;
	struct disp_session_vsync_config *c = NULL;

	c = (struct disp_session_vsync_config *)config;

	/* EXT_DISP_FUNC(); */

	_ext_disp_path_lock();
	if (pgc->state == EXTD_DEINIT) {
		_ext_disp_path_unlock();
		msleep(20);
		return -1;
	}
	_ext_disp_path_unlock();

	_ext_disp_vsync_lock(session);
	ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				       DISP_PATH_EVENT_IF_VSYNC, 60);
	if (ret == -2) {
		EXT_DISP_LOG("vsync for ext display path not enabled yet\n");
		_ext_disp_vsync_unlock(session);
		return -1;
	}

	/* EXT_DISP_LOG("ext_disp_wait_for_vsync - vsync signaled\n"); */
	c->vsync_ts = get_current_time_us();
	c->vsync_cnt++;

	_ext_disp_vsync_unlock(session);
	return ret;
}

int ext_disp_suspend(unsigned int session)
{
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	EXT_DISP_FUNC();

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND ||
	    session != pgc->session) {
		EXT_DISP_ERR("status: not EXTD_RESUME or session: not match\n");
		goto done;
	}

	pgc->need_trigger_overlay = 0;

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	if (ext_disp_use_cmdq == CMDQ_ENABLE &&
	    DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_stop_extd_trigger_loop();

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);

	pgc->state = EXTD_SUSPEND;

done:
	_ext_disp_path_unlock();

	EXT_DISP_LOG("%s done\n", __func__);
	return ret;
}

int ext_disp_resume(unsigned int session)
{
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	EXT_DISP_FUNC();

	_ext_disp_path_lock();

	if (pgc->state != EXTD_SUSPEND) {
		EXT_DISP_ERR("EXTD_DEINIT/EXTD_INIT/EXTD_RESUME\n");
		goto done;
	}

	init_roi = 1;

	if (_should_reset_cmdq_config_handle() &&
	    DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_reset_config_handle();

	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (ext_disp_use_cmdq == CMDQ_ENABLE &&
	    DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_start_extd_trigger_loop();

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		EXT_DISP_LOG("stop display path failed, still busy\n");
		ret = -1;
		goto done;
	}

	if (DISP_SESSION_DEV(session) == DEV_EINK + 1)
		pgc->suspend_config = 0;

	pgc->state = EXTD_RESUME;

done:
	_ext_disp_path_unlock();
	EXT_DISP_LOG("%s done\n", __func__);
	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_PULSE,
			 Resume, 1);
	return ret;
}

int ext_fence_release_callback(unsigned long userdata)
{
	int i = 0;
	int ret = 0;
	int fence_idx = 0;
	int subtractor = 0;
	unsigned int input_config_info = 0;
#ifdef EXTD_DEBUG_SUPPORT
	unsigned int status = 0;
#endif

	/* EXT_DISP_FUNC(); */

	cmdqBackupReadSlot(pgc->ext_input_config_info, 0, &input_config_info);

	if (input_config_info & 0x100) { /* input source is rdma */
		if (ext_disp_get_ovl_req_status(pgc->session) ==
		    EXTD_OVL_REMOVED)
			ext_disp_path_change(EXTD_OVL_NO_REQ, pgc->session);
	} else {
		if (ext_disp_get_ovl_req_status(pgc->session) ==
		    EXTD_OVL_INSERTED)
			ext_disp_path_change(EXTD_OVL_NO_REQ, pgc->session);
	}

	_ext_disp_path_lock();

	for (i = 0; i < EXTD_OVERLAY_CNT; i++) {
		cmdqBackupReadSlot(pgc->ext_cur_config_fence, i, &fence_idx);
		cmdqBackupReadSlot(pgc->ext_subtractor_when_free, i,
				   &subtractor);

		mtkfb_release_fence(pgc->session, i, fence_idx - subtractor);

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_UsedBuff,
				 MMPROFILE_FLAG_PULSE, fence_idx, i);
	}
	/* Release present fence */
	if (gCurrentPresentFenceIndex != -1)
		mtkfb_release_present_fence(pgc->session,
				 gCurrentPresentFenceIndex);

#ifdef EXTD_DEBUG_SUPPORT
	/* check last ovl/rdma status: should be idle when config */
	cmdqBackupReadSlot(pgc->ext_ovl_rdma_status_info, 0, &status);
	if (input_config_info & 0x100) { /* input source is rdma */
		if (status & 0x1000) {
			/* rdma smi is not idle !! */
			EXT_DISP_ERR("extd rdma-smi status error!! stat=0x%x\n",
				     status);
			ext_disp_diagnose();
			ret = -1;
		}
	} else {
		if (status & 0x1) {
			/* ovl is not idle !! */
			EXT_DISP_ERR("extd ovl status error!! stat=0x%x\n",
				     status);
			ext_disp_diagnose();
			ret = -1;
		}
	}
#endif
#if defined(CONFIG_MTK_HDMI_SUPPORT)
		if (pgc->state == EXTD_RESUME)
			/* hdmi video config with layer_type */
			external_display_util.hdmi_video_format_config(
				input_config_info & 0xff);
		else
			EXT_DISP_ERR
				("%s ext display is not resume\n", __func__);
#endif

	atomic_set(&g_extd_release_ticket, userdata);

	_ext_disp_path_unlock();

	/* EXT_DISP_LOG("ext_fence_release_callback done\n"); */

	return ret;
}

int ext_disp_trigger(int blocking, void *callback, unsigned int userdata,
		     unsigned int session)
{
	int ret = 0;

	/* EXT_DISP_FUNC(); */

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND ||
	    pgc->need_trigger_overlay < 1) {
		EXT_DISP_LOG("trigger ext display is already slept\n");
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Trigger, 0);
		return -1;
	}

	_ext_disp_path_lock();

	if (_should_trigger_interface()) {
		if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL &&
		    DISP_SESSION_DEV(session) == DEV_MHL + 1)
			_ext_disp_trigger(blocking, callback,
					  atomic_read(&g_extd_trigger_ticket));
		else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL &&
			 DISP_SESSION_DEV(session) == DEV_EINK + 1)
			_ext_disp_trigger_EPD(blocking, callback,
					atomic_read(&g_extd_trigger_ticket));
	} else {
		dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL,
				   ext_disp_use_cmdq);
	}

	atomic_add(1, &g_extd_trigger_ticket);

	pgc->state = EXTD_RESUME;

	_ext_disp_path_unlock();
	/* EXT_DISP_LOG("ext_disp_trigger done\n"); */

	return ret;
}

int ext_disp_suspend_trigger(void *callback, unsigned int userdata,
			     unsigned int session)
{
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	EXT_DISP_FUNC();

	if (pgc->state != EXTD_RESUME) {
		EXT_DISP_LOG("trigger ext display is already slept\n");
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Trigger, 0);
		return -1;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_PULSE,
			 Suspend, 0);

	_ext_disp_path_lock();

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	if (_should_insert_wait_frame_done_token())
		_cmdq_insert_wait_frame_done_token(0);

	pgc->need_trigger_overlay = 0;

	if (ext_disp_use_cmdq == CMDQ_ENABLE &&
	    DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_stop_extd_trigger_loop();

	dpmgr_path_stop(pgc->dpmgr_handle, ext_disp_cmdq_enabled());

	if (_should_flush_cmdq_config_handle()) {
		_cmdq_flush_config_handle(1, 0, 0);
		_cmdq_reset_config_handle();
	}

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (DISP_SESSION_DEV(session) == DEV_EINK + 1)
		pgc->suspend_config = 1;

	pgc->state = EXTD_SUSPEND;

	_ext_disp_path_unlock();

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_PULSE,
			 Suspend, 1);
	return ret;
}

int ext_disp_frame_cfg_input(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	int i = 0;
	int layer_cnt = 0;
	struct M4U_PORT_STRUCT sPort = { 0 };
	struct disp_ddp_path_config *pconfig;
	unsigned int input_source;
	struct ddp_io_golden_setting_arg gset_arg;

	unsigned int session = cfg->session_id;

	/* EXT_DISP_FUNC(); */

	if (pgc->state != EXTD_INIT && pgc->state != EXTD_RESUME &&
	    pgc->suspend_config != 1) {
		EXT_DISP_LOG("config ext disp is already slept, state:%d\n",
			     pgc->state);
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Config,
				 cfg->input_cfg[0].next_buff_idx);
		return -2;
	}

	for (i = 0; i < cfg->input_layer_num; i++) {
		if (cfg->input_cfg[i].layer_enable)
			layer_cnt++;
	}

	/* config m4u port */
	if (layer_cnt == 1) {
		ext_disp_path_change(EXTD_OVL_REMOVE_REQ, session);
		if (ext_disp_get_ovl_req_status(session) ==
		    EXTD_OVL_REMOVE_REQ) {
			EXT_DISP_LOG("config M4U Port DISP_MODULE_RDMA1\n");
			sPort.ePortID = M4U_PORT_DISP_RDMA1;
			sPort.Virtuality = 1;
			sPort.Security = 0;
			sPort.Distance = 1;
			sPort.Direction = 0;
#ifdef MTKFB_M4U_SUPPORT
			ret = m4u_config_port(&sPort);
#endif
			if (ret)
				EXT_DISP_LOG("config M4U Port RDMA1 FAIL\n");

			pgc->ovl_req_state = EXTD_OVL_REMOVING;
		}
	} else if (layer_cnt > 1) {
		ext_disp_path_change(EXTD_OVL_INSERT_REQ, session);
		if (ext_disp_get_ovl_req_status(session) ==
		    EXTD_OVL_INSERT_REQ) {
			EXT_DISP_LOG("config M4U Port DISP_MODULE_OVL1_2L\n");
			sPort.ePortID = M4U_PORT_DISP_2L_OVL1_LARB0;
			sPort.Virtuality = 1;
			sPort.Security = 0;
			sPort.Distance = 1;
			sPort.Direction = 0;
#ifdef MTKFB_M4U_SUPPORT
			ret = m4u_config_port(&sPort);
#endif
			if (ret)
				EXT_DISP_LOG("config M4U Port OVL1_2L FAIL\n");
		}
	}

	_ext_disp_path_lock();

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	pconfig->dst_w = extd_lcm_params.dpi.width;
	pconfig->dst_h = extd_lcm_params.dpi.height;
	if (extd_lcm_params.dpi.dsc_enable == 1)
		pconfig->dst_w = extd_lcm_params.dpi.width * 3;

	if (_should_config_ovl_input()) {
		for (i = 0; i < cfg->input_layer_num; i++) {
			int id = 0;

			id = cfg->input_cfg[i].layer_id;
			ret = _convert_disp_input_to_ovl(
					&(pconfig->ovl_config[id]),
					&(cfg->input_cfg[i]));
			dprec_mmp_dump_ovl_layer(
					&(pconfig->ovl_config[id]), id, 2);
		}

		if (init_roi == 1) {
			memcpy(&(pconfig->dispif_config),
			       &extd_lcm_params, sizeof(struct LCM_PARAMS));

			EXT_DISP_LOG("set dest w:%d, h:%d\n",
				     extd_lcm_params.dpi.width,
				     extd_lcm_params.dpi.height);

			pconfig->dst_dirty = 1;
			pconfig->rdma_config.address = 0;
		}
		pconfig->ovl_dirty = 1;
		pgc->need_trigger_overlay = 1;

		input_source = 0;

		/* update golden setting */
		memset(&gset_arg, 0, sizeof(gset_arg));
		gset_arg.dst_mod_type = dpmgr_path_get_dst_module_type(
							pgc->dpmgr_handle);
		gset_arg.is_decouple_mode = 1;
		dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
				 DDP_OVL_GOLDEN_SETTING, &gset_arg);
	} else {
		struct OVL_CONFIG_STRUCT ovl_config;

		_convert_disp_input_to_ovl(&ovl_config, &(cfg->input_cfg[0]));
		dprec_mmp_dump_ovl_layer(&ovl_config,
					 cfg->input_cfg[0].layer_id, 2);

		ret = _convert_disp_input_to_rdma(&(pconfig->rdma_config),
						  &(cfg->input_cfg[0]),
						  pconfig->dst_w,
						  pconfig->dst_h);
		if (pconfig->rdma_config.address) {
			pconfig->rdma_dirty = 1;
			pgc->need_trigger_overlay = 1;
		}
		input_source = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	memcpy(&(pconfig->dispif_config), &extd_lcm_params,
	       sizeof(struct LCM_PARAMS));

	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig,
				ext_disp_cmdq_enabled() ?
				pgc->cmdq_handle_config : NULL);

	/*
	 * this is used for decouple mode,
	 * to indicate whether we need to trigger ovl
	 */
	/* pgc->need_trigger_overlay = 1; */

	init_roi = 0;

	for (i = 0; i < cfg->input_layer_num; i++) {
		unsigned int last_fence, cur_fence, sub;

		cmdqBackupReadSlot(pgc->ext_cur_config_fence, i, &last_fence);
		cur_fence = cfg->input_cfg[i].next_buff_idx;

		if (cur_fence != -1 && cur_fence > last_fence) {
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
						pgc->ext_cur_config_fence, i,
						cur_fence);
		}

		/*
		 * for dim_layer/disable_layer/no_fence_layer,
		 * just release all fences configured
		 * for other layers, release current_fence-1
		 */
		if (cfg->input_cfg[i].buffer_source == DISP_BUFFER_ALPHA ||
		    cfg->input_cfg[i].layer_enable == 0 || cur_fence == -1)
			sub = 0;
		else
			sub = 1;

		cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
					pgc->ext_subtractor_when_free, i, sub);
	}

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
				pgc->ext_input_config_info, 0,
				(input_source << 8) |
				(cfg->input_cfg[0].layer_type));
	/* Update present fence index */
	if (cfg->present_fence_idx != (unsigned int)-1)
		gCurrentPresentFenceIndex = cfg->present_fence_idx;

#ifdef EXTD_DEBUG_SUPPORT
	if (_should_config_ovl_input()) {
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config,
					    pgc->ext_ovl_rdma_status_info, 0,
					    disp_addr_convert(
							DISPSYS_OVL1_2L_BASE +
							DISP_REG_OVL_STA));
	} else {
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config,
					    pgc->ext_ovl_rdma_status_info, 0,
					    disp_addr_convert(
						DISP_REG_RDMA_GLOBAL_CON +
						DISP_RDMA_INDEX_OFFSET));
	}
#endif

	_ext_disp_path_unlock();

	if (pconfig->ovl_dirty) {
		EXT_DISP_LOG("%s idx:%d -w:%d, h:%d, pitch:%d\n", __func__,
			     cfg->input_cfg[0].next_buff_idx,
			     pconfig->ovl_config[0].src_w,
			     pconfig->ovl_config[0].src_h,
			     pconfig->ovl_config[0].src_pitch);
	} else {
		EXT_DISP_LOG("%s idx:%d -w:%d, h:%d, pitch:%d, mva:0x%lx\n",
			     __func__, cfg->input_cfg[0].next_buff_idx,
			     pconfig->rdma_config.width,
			     pconfig->rdma_config.height,
			     pconfig->rdma_config.pitch,
			     pconfig->rdma_config.address);
	}
	return ret;
}

int ext_disp_get_max_layer(void)
{
	if (_should_config_ovl_input())
		return EXTERNAL_SESSION_INPUT_LAYER_COUNT;
	else
		return 1;
}

int ext_disp_is_alive(void)
{
	unsigned int temp = 0;

	EXT_DISP_FUNC();
	_ext_disp_path_lock();
	temp = pgc->state;
	_ext_disp_path_unlock();

	return temp;
}

int ext_disp_is_sleepd(void)
{
	unsigned int temp = 0;

	/* EXT_DISP_FUNC(); */
	_ext_disp_path_lock();
	temp = !pgc->state;
	_ext_disp_path_unlock();

	return temp;
}

int ext_disp_get_width(void)
{
	return 0;
}

int ext_disp_get_height(void)
{
	return 0;
}

unsigned int ext_disp_get_sess_id(void)
{
	unsigned int session_id = is_context_inited > 0 ? pgc->session : 0;

	return session_id;
}

int ext_disp_is_video_mode(void)
{
	return true;
}

int ext_disp_diagnose(void)
{
	int ret = 0;

	if (is_context_inited > 0) {
		EXT_DISP_LOG("%s, is_context_inited --%d\n",
			     __func__, is_context_inited);
		dpmgr_check_status(pgc->dpmgr_handle);
	}

	return ret;
}

enum CMDQ_SWITCH ext_disp_cmdq_enabled(void)
{
	return ext_disp_use_cmdq;
}

int ext_disp_switch_cmdq(enum CMDQ_SWITCH use_cmdq)
{
	_ext_disp_path_lock();

	ext_disp_use_cmdq = use_cmdq;
	EXT_DISP_LOG("display driver use %s to config register now\n",
		     (use_cmdq == CMDQ_ENABLE) ? "CMDQ" : "CPU");

	_ext_disp_path_unlock();
	return ext_disp_use_cmdq;
}

void ext_disp_get_curr_addr(unsigned long *input_curr_addr, int module)
{
	if (module == 1)
		ovl_get_address(DISP_MODULE_OVL1_2L, input_curr_addr);
	else
		dpmgr_get_input_address(pgc->dpmgr_handle, input_curr_addr);
}

int ext_disp_factory_test(int mode, void *config)
{
#if 0
	if (mode == 1)
		dpmgr_factory_mode_reset(DISP_MODULE_DSI1, NULL, config);
	else
		dpmgr_factory_mode_test(DISP_MODULE_DSI1, NULL, config);
#endif
	if (mode == 1)
		dpmgr_factory_mode_reset(DISP_MODULE_DPI, NULL, config);
	else
		dpmgr_factory_mode_test(DISP_MODULE_DPI, NULL, config);

	return 0;
}

int ext_disp_get_handle(disp_path_handle *dp_handle,
			struct cmdqRecStruct **pHandle)
{
	*dp_handle = pgc->dpmgr_handle;
	*pHandle = pgc->cmdq_handle_config;
	return pgc->mode;
}

int ext_disp_set_ovl1_status(int status)
{
	/* dpmgr_set_ovl1_status(status);*/
	return 0;
}

int ext_disp_set_lcm_param(struct LCM_PARAMS *pLCMParam)
{
	if (pLCMParam)
		memcpy(&extd_lcm_params, pLCMParam, sizeof(struct LCM_PARAMS));

	return 0;
}

enum EXTD_OVL_REQ_STATUS ext_disp_get_ovl_req_status(unsigned int session)
{
	enum EXTD_OVL_REQ_STATUS ret = EXTD_OVL_NO_REQ;

	_ext_disp_path_lock();
	ret = pgc->ovl_req_state;
	_ext_disp_path_unlock();

	return ret;
}

int ext_disp_path_change(enum EXTD_OVL_REQ_STATUS action, unsigned int session)
{
	/* EXT_DISP_FUNC(); */

	if (EXTD_OVERLAY_CNT <= 0)
		return 0;

	_ext_disp_path_lock();
	switch (action) {
	case EXTD_OVL_NO_REQ:
		break;
	case EXTD_OVL_REQUSTING_REQ:
		break;
	case EXTD_OVL_IDLE_REQ:
		break;
	case EXTD_OVL_SUB_REQ:
		break;
	case EXTD_OVL_REMOVE_REQ:
		break;
	case EXTD_OVL_REMOVING:
		pgc->ovl_req_state = EXTD_OVL_REMOVING;
		break;
	case EXTD_OVL_REMOVED:
		pgc->ovl_req_state = EXTD_OVL_REMOVED;
		break;
	case EXTD_OVL_INSERT_REQ:
		break;
	case EXTD_OVL_INSERTED:
		pgc->ovl_req_state = EXTD_OVL_INSERTED;
		break;
	default:
		break;
	}
	_ext_disp_path_unlock();

	return 0;
}

int ext_disp_wait_ovl_available(int ovl_num)
{
	int ret = 0;

	if (EXTD_OVERLAY_CNT > 0) {
		/* wait OVL can be used by external display */
		ret = dpmgr_wait_ovl_available(ovl_num);
	}

	return ret;
}

bool ext_disp_path_source_is_RDMA(unsigned int session)
{
	bool is_rdma = false;

	if ((ext_disp_mode == EXTD_RDMA_DPI_MODE &&
	     pgc->ovl_req_state != EXTD_OVL_REMOVE_REQ &&
	     pgc->ovl_req_state != EXTD_OVL_REMOVING) ||
	    (ext_disp_mode == EXTD_DIRECT_LINK_MODE &&
	     pgc->ovl_req_state == EXTD_OVL_INSERT_REQ)) {
		/* path source module is RDMA */
		is_rdma = true;
	}

	return is_rdma;
}

int ext_disp_is_dim_layer(unsigned long mva)
{
	int ret = 0;

	ret = is_dim_layer(mva);

	return ret;
}
