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
#include "ddp_clkmgr.h"
#include "lcm_drv.h"

#include "extd_platform.h"
#include "extd_log.h"
#include "extd_utils.h"
#include "extd_hdmi_types.h"
#include "external_display.h"

#include "disp_session.h"
#include "disp_lowpower.h"
#include "disp_recovery.h"
#include "display_recorder.h"
#include "extd_info.h"

#include "mtkfb_fence.h"
#include "disp_drv_log.h"


#if (defined CONFIG_MTK_HDMI_SUPPORT)
#else
unsigned int dst_is_dsi;
#endif

int ext_disp_use_cmdq;
int ext_disp_use_m4u;
enum EXT_DISP_PATH_MODE ext_disp_mode;

static struct disp_lcm_handle *plcm_interface;
static int is_context_inited;
static int init_roi;
static unsigned int gCurrentPresentFenceIndex = -1;

static struct mutex esd_check_lock;
struct ext_disp_path_context {
	enum EXTD_POWER_STATE state;
	enum EXTD_OVL_REQ_STATUS ovl_req_state;
	enum EXTD_LCM_STATE lcm_state;
	int init;
	unsigned int session;
	int need_trigger_overlay;
	int suspend_config;
	enum EXT_DISP_PATH_MODE mode;
	unsigned int last_vsync_tick;
	struct mutex lock;
	char *mutex_locker;
	struct disp_lcm_handle *plcm;
	struct cmdqRecStruct *cmdq_handle_config;
	struct cmdqRecStruct *cmdq_handle_trigger;
	disp_path_handle dpmgr_handle;
	disp_path_handle ovl2mem_path_handle;
	cmdqBackupSlotHandle ext_cur_config_fence;
	cmdqBackupSlotHandle ext_subtractor_when_free;
	/*
	 **cmdqBackupSlotHandle ext_input_config_info
	 **bit0-7: layer_type
	 **bit8: 0 = source is ovl, 1 = source is rdma
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

unsigned int g_extd_mobilelog;

static struct ext_disp_path_context *_get_context(void)
{
	static struct ext_disp_path_context g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0,
		       sizeof(struct ext_disp_path_context));
		is_context_inited = 1;
		EXTDMSG("_get_context set is_context_inited\n");
	}

	return &g_context;
}

/************************** Upper Layer To HAL*********************************/
static struct EXTERNAL_DISPLAY_UTIL_FUNCS external_display_util = { 0 };

void extd_disp_drv_set_util_funcs(const struct EXTERNAL_DISPLAY_UTIL_FUNCS
				  *util)
{
	memcpy(&external_display_util, util,
	       sizeof(struct EXTERNAL_DISPLAY_UTIL_FUNCS));
}

enum EXT_DISP_PATH_MODE ext_disp_path_get_mode(unsigned int session)
{
	return ext_disp_mode;
}

void ext_disp_path_set_mode(enum EXT_DISP_PATH_MODE mode, unsigned int session)
{
	ext_disp_mode = EXTD_DIRECT_LINK_MODE;	/*mode; */

	init_roi = 1;
}

static void _ext_disp_path_lock(const char *caller)
{
	extd_sw_mutex_lock(&(pgc->lock));
	pgc->mutex_locker = (char *)caller;
/*	EXTDINFO("_ext_disp_path_lock caller: %s\n", pgc->mutex_locker);*/
}

static void _ext_disp_path_unlock(const char *caller)
{
	pgc->mutex_locker = NULL;
	extd_sw_mutex_unlock(&(pgc->lock));
/*	EXTDINFO("_ext_disp_path_unlock caller: %s\n", pgc->mutex_locker);*/
}

int ext_disp_manual_lock(void)
{
	_ext_disp_path_lock(__func__);

	return 0;
}

int ext_disp_manual_unlock(void)
{
	_ext_disp_path_unlock(__func__);

	return 0;
}

/*
 * trigger operation:    VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 1.wait idle:         N         N        Y        Y
 * 2.lcm update:        N         Y        N        Y
 * 3.path start:       idle->Y    Y       idle->Y   Y
 * 4.path trigger:     idle->Y    Y       idle->Y   Y
 * 5.mutex enable:      N         N       idle->Y   Y
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
	else
		return dpmgr_path_is_busy(pgc->dpmgr_handle);
}

/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 3.path start:      idle->Y   Y        idle->Y  Y
 */
static int _should_start_path(void)
{
	if (ext_disp_is_video_mode())
		return dpmgr_path_is_idle(pgc->dpmgr_handle);
	else
		return 1;
}

/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
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

/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 6. set cmdq dirty: N         Y        N        N
 */
static int _should_set_cmdq_dirty(void)
{
	if (ext_disp_cmdq_enabled() && (ext_disp_is_video_mode() == 0))
		return 1;
	else
		return 0;
}

/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 7. flush cmdq:     Y         Y        N        N
 */
static int _should_flush_cmdq_config_handle(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

/* trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 8. reset cmdq:     Y         Y        N        N
 */
static int _should_reset_cmdq_config_handle(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

/* trigger operation:       VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
 * 9. cmdq insert token:   Y         Y        N        N
 */
static int _should_insert_wait_frame_done_token(void)
{
	return ext_disp_cmdq_enabled() ? 1 : 0;
}

/*
 * static int _should_trigger_interface(void)
 * {
 *	if (pgc->mode == EXTD_DECOUPLE_MODE)
 *		return 0;
 *	else
 *		return 1;
 * }
 */
static int _should_config_ovl_input(void)
{
	if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE
	    || ext_disp_mode == EXTD_RDMA_DPI_MODE)
		return 0;
	else
		return 1;
}

/*
 *
	static int _is_dsc_enable(unsigned int session)
	{
		int ret = 0;

		if (DISP_SESSION_DEV(session) == DEV_LCM)
			ret = extd_lcm_params.dsi.dsc_enable;
		else
			ret = extd_lcm_params.dpi.dsc_enable;

		return ret;
	}
 */

static int _build_path_direct_link(unsigned int session)
{
	int ret = 0;
	M4U_PORT_STRUCT sPort;

	EXTDFUNC();
	pgc->mode = EXTD_DIRECT_LINK_MODE;

	pgc->dpmgr_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle)
		EXTDINFO("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	else {
		EXTDERR("dpmgr create path FAIL\n");
		return -1;
	}

	sPort.ePortID = M4U_PORT_UNKNOWN;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret == 0) {
		EXTDINFO("config M4U Port %s to %s SUCCESS\n",
			 ddp_get_module_name(DISP_MODULE_OVL1_2L),
			 ext_disp_use_m4u ? "virtual" : "physical");
	} else {
		EXTDERR("config M4U Port %s to %s FAIL(ret=%d)\n", "ovl1_2l",
			ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

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
	M4U_PORT_STRUCT sPort;

	EXTDFUNC();
	pgc->mode = EXTD_RDMA_DPI_MODE;

	pgc->dpmgr_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP,
			      pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle)
		EXTDINFO("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	else {
		EXTDERR("dpmgr create path FAIL\n");
		return -1;
	}

	sPort.ePortID = M4U_PORT_UNKNOWN;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret == 0) {
		EXTDINFO("config M4U Port %s to %s SUCCESS\n",
			 ddp_get_module_name(DISP_MODULE_RDMA1),
			 ext_disp_use_m4u ? "virtual" : "physical");
	} else {
		EXTDERR("config M4U Port %s to %s FAIL(ret=%d)\n",
			ddp_get_module_name(DISP_MODULE_RDMA1),
			ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;

	EXTDFUNC();
	if (pgc->cmdq_handle_trigger == NULL) {
		ret =
		    cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP,
				  &(pgc->cmdq_handle_trigger));
		if (ret) {
			EXTDERR("%s:%d, create cmdq handle fail!ret=%d\n",
				__func__, __LINE__, ret);
			ASSERT(0);
		}
	}
	/* Set fake cmdq engineflag for judge path scenario */
	cmdqRecSetEngine(pgc->cmdq_handle_trigger,
			 ((1LL << CMDQ_ENG_DISP_OVL1) |
			  (1LL << CMDQ_ENG_DISP_WDMA1)));

	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (ext_disp_is_video_mode()) {
		/* wait and clear stream_done, */
		/* HW will assert mutex enable automatically */
		/* in frame done reset. */
		/* todo: should let dpmanager to decide */
		/* wait which mutex's eof. */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger,
				  dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				  CMDQ_EVENT_MUTEX0_STREAM_EOF);

		/* for some moduleto read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF, 0);
	} else {
		/* DSI command mode need use CMDQ token instead */
		ret =
		    cmdqRecWait(pgc->cmdq_handle_trigger,
				CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_LCM_TE, 0);

		ret =
		    cmdqRecWaitNoClear(pgc->cmdq_handle_trigger,
				       CMDQ_SYNC_TOKEN_EXT_CABC_EOF);
		/* cleat frame done token, */
		/* now the config thread will not allowed */
		/* to config registers. */
		/* remember that config thread's priority */
		/* is higher than trigger thread */
		/* so all the config queued before will be applied */
		/* then STREAM_EOF token be cleared */
		/* this is what CMDQ did as "Merge" */
		ret =
		    cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
					   CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);

		ret =
		    cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
					   CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY);

		/* clear rdma EOF token before wait */
		ret =
		    cmdqRecClearEventToken(pgc->cmdq_handle_trigger,
					   CMDQ_EVENT_DISP_RDMA1_EOF);

		/* for operations before frame transfer, */
		/* such as waiting for DSI TE */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_BEFORE_STREAM_SOF, 0);

		/* enable mutex, only cmd mode need this */
		/* this is what CMDQ did as "Trigger" */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				   CMDQ_ENABLE);

		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_SOF, 1);

		/* waiting for frame done, */
		/* because we can't use mutex stream eof here */
		/* so need to let dpmanager help to */
		/* decide which event to wait */
		/* most time we wait rdmax frame done event. */
		ret =
		    cmdqRecWait(pgc->cmdq_handle_trigger,
				CMDQ_EVENT_DISP_RDMA1_EOF);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_STREAM_EOF_EVENT, 0);

		/* dsi is not idle rightly after rdma frame done, */
		/* so we need to polling about 1us for dsi returns to idle */
		/* do not polling dsi idle directly */
		/* which will decrease CMDQ performance */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_CHECK_IDLE_AFTER_STREAM_EOF, 0);

		/* for some module to read hw register after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF, 0);

		/* reset some modules to enhance robusty */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle,
				      pgc->cmdq_handle_trigger,
				      CMDQ_RESET_AFTER_STREAM_EOF, 0);

		/* now frame done, */
		/* config thread is allowed to config register now */
		ret =
		    cmdqRecSetEventToken(pgc->cmdq_handle_trigger,
					 CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);
		ret =
		    cmdqRecSetEventToken(pgc->cmdq_handle_trigger,
					 CMDQ_SYNC_TOKEN_EXT_CABC_EOF);

		/* RUN forever!!!! */
		WARN_ON(ret < 0);
	}

	/* dump trigger loop instructions to check */
	/* whether dpmgr_path_build_cmdq works correctly */
	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	EXTDMSG("ext display BUILD cmdq trigger loop finished\n");
}

void _cmdq_start_extd_trigger_loop(void)
{
	int ret = 0;

	EXTDFUNC();
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!ext_disp_is_video_mode()) {
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_EXT_CABC_EOF);
	}

	EXTDMSG("START cmdq trigger loop finished\n");
}

void _cmdq_stop_extd_trigger_loop(void)
{
	int ret = 0;

	EXTDFUNC();
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);

	EXTDMSG("ext display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if (!ext_disp_is_video_mode()) {
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(pgc->cmdq_handle_config,
				     CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY);
		/* /dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY); */
	}
}

static void _cmdq_handle_clear_dirty(struct cmdqRecStruct *cmdq_handle)
{
	if (!ext_disp_is_video_mode())
		cmdqRecClearEventToken(cmdq_handle,
				       CMDQ_SYNC_TOKEN_EXT_CONFIG_DIRTY);
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	/* /dprec_event_op(DPREC_EVENT_CMDQ_RESET); */
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
	/* dprec_event_op(DPREC_EVENT_CMDQ_FLUSH); */
}

#if 0
static void _cmdq_insert_wait_frame_done_token(int clear_event)
{
	if (ext_disp_is_video_mode()) {
		if (clear_event == 0) {
			cmdqRecWaitNoClear(pgc->cmdq_handle_config,
					   dpmgr_path_get_mutex(pgc->
								dpmgr_handle) +
					   CMDQ_EVENT_MUTEX0_STREAM_EOF);
		} else {
			cmdqRecWait(pgc->cmdq_handle_config,
				    dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				    CMDQ_EVENT_MUTEX0_STREAM_EOF);
		}
	} else {
		if (clear_event == 0)
			cmdqRecWaitNoClear(pgc->cmdq_handle_config,
					   CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);
		else
			cmdqRecWait(pgc->cmdq_handle_config,
				    CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);
	}

	/* /dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF); */
}
#endif

void _ext_cmdq_insert_wait_frame_done_token(void *handle)
{
	if (ext_disp_is_video_mode()) {
		cmdqRecWaitNoClear(handle,
				   dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				   CMDQ_EVENT_MUTEX0_STREAM_EOF);
	} else
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_EXT_STREAM_EOF);
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
		EXTDERR("%s src(0x%p) or dst(0x%p) is null\n", __func__,
			src, dst);
		return -1;
	}

	if (!src->layer_enable)
		return 0;

	dst->idx = src->next_buff_idx;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	ufmt_disable_X_channel(tmp_fmt, &dst->inputFormat, NULL);

	Bpp = UFMT_GET_Bpp(dst->inputFormat);
	mva_offset =
	    (src->src_offset_x + src->src_offset_y * src->src_pitch) * Bpp;

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
		EXTDERR("%s src(0x%p) or dst(0x%p) is null\n", __func__,
			src, dst);
		return -1;
	}

	dst->layer = src->layer_id;
	dst->isDirty = 1;
	dst->buff_idx = src->next_buff_idx;
	dst->layer_en = src->layer_enable;

	/* if layer is disable, we just needs config above params. */
	if (!src->layer_enable)
		return 0;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	/* display don't support X channel, like XRGB8888
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
	} else if (src->buffer_source == DISP_BUFFER_ION
		   || src->buffer_source == DISP_BUFFER_MVA) {
		dst->source = OVL_LAYER_SOURCE_MEM;
	} else {
		EXTDERR("unknown source = %d", src->buffer_source);
		dst->source = OVL_LAYER_SOURCE_MEM;
	}
	dst->ext_sel_layer = src->ext_sel_layer;

	return 0;
}

static int _ext_disp_trigger(int blocking, void *callback,
			     unsigned int userdata)
{
	bool reg_flush = false;

	EXTDFUNC();

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	if (_should_start_path()) {
		reg_flush = true;
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
				 MMPROFILE_FLAG_PULSE, Trigger, 1);
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle()) {
		if (reg_flush)
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
					 MMPROFILE_FLAG_PULSE, Trigger, 2);

		_cmdq_flush_config_handle(blocking, callback, userdata);
	}

	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
				   ext_disp_cmdq_enabled());
	}

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	if (_should_insert_wait_frame_done_token())
		_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);

	EXTDINFO("_ext_disp_trigger done\n");
	return 0;
}

static int _ext_disp_trigger_EPD(int blocking, void *callback,
				 unsigned int userdata)
{
	EXTDFUNC();

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	if (_should_start_path()) {
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_State,
				 MMPROFILE_FLAG_PULSE, Trigger, 1);
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
				   ext_disp_cmdq_enabled());
	}

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	if (_should_insert_wait_frame_done_token())
		_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);

	EXTDINFO("_ext_disp_trigger_EPD done\n");
	return 0;
}

static int _ext_disp_trigger_LCM(int blocking, void *callback,
				 unsigned int userdata)
{
	EXTDFUNC();

	if (pgc->lcm_state == EXTD_LCM_NO_INIT) {
/*		disp_lcm_init(pgc->plcm, 1);	*/
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		external_display_esd_check_enable(1);
#endif

		pgc->lcm_state = EXTD_LCM_INITED;
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	if (_should_flush_cmdq_config_handle())
		_cmdq_flush_config_handle(blocking, callback, userdata);

	if (_should_trigger_path())
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
				   ext_disp_cmdq_enabled());

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	/* clear cmdq dirty in case trigger loop starts here */
	if (_should_set_cmdq_dirty())
		_cmdq_handle_clear_dirty(pgc->cmdq_handle_config);


	if (_should_insert_wait_frame_done_token())
		_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);

	EXTDINFO("_ext_disp_trigger_LCM done\n");
	return 0;
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

static void deinit_cmdq_slots(cmdqBackupSlotHandle hSlot)
{
	cmdqBackupFreeSlot(hSlot);
}

/*
 *
	static int ext_disp_cmdq_dump(uint64_t engineFlag, int level)
	{
		EXTDFUNC();

		if (pgc->dpmgr_handle != NULL)
			ext_disp_diagnose();
		else
			EXTDMSG("external display dpmgr_handle == NULL\n");

		return 0;
	}
*/
static int ext_disp_init_hdmi(unsigned int session)
{
	struct disp_ddp_path_config *data_config = NULL;
	enum EXT_DISP_STATUS ret;
	enum DISP_MODULE_ENUM dst_module = 0;

	EXTDFUNC();
	ret = EXT_DISP_STATUS_OK;

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_handle_config));
	if (ret) {
		EXTDERR("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}
	EXTDMSG("cmdqRecCreate SUCCESS, g_cmdq_handle=%p\n",
		pgc->cmdq_handle_config);

	/* Set fake cmdq engineflag for judge path scenario */
	cmdqRecSetEngine(pgc->cmdq_handle_config,
			 ((1LL << CMDQ_ENG_DISP_OVL1) |
			  (1LL << CMDQ_ENG_DISP_WDMA1)));

	cmdqRecReset(pgc->cmdq_handle_config);

	if (dst_is_dsi)
		dst_module = DISP_MODULE_DSI1;
	else
		dst_module = DISP_MODULE_DPI;
	/* If DP max resolution is 4K, It need use  DDP_SCENARIO_SUB_DISP_4K. */
	ddp_set_dst_module(DDP_SCENARIO_SUB_DISP, dst_module);

	if (ext_disp_mode == EXTD_DIRECT_LINK_MODE)
		_build_path_direct_link(session);
	else if (ext_disp_mode == EXTD_DECOUPLE_MODE)
		_build_path_decouple();
	else if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE)
		_build_path_single_layer();
	else if (ext_disp_mode == EXTD_RDMA_DPI_MODE)
		_build_path_rdma_dpi();
	else
		EXTDERR("ext_disp display mode is WRONG\n");

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		if (DISP_SESSION_DEV(session) != DEV_EINK + 1) {
			_cmdq_build_trigger_loop();
			_cmdq_start_extd_trigger_loop();
		}
	}
	pgc->session = session;

	EXTDINFO("ext_disp display START cmdq trigger loop finished\n");

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, ext_disp_is_video_mode());
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	if (data_config) {
		memset((void *)data_config, 0,
		       sizeof(struct disp_ddp_path_config));
		memcpy(&(data_config->dispif_config), &extd_lcm_params,
		       sizeof(struct LCM_PARAMS));

		if (dst_is_dsi) {
			data_config->dst_w = extd_lcm_params.width;
			data_config->dst_h = extd_lcm_params.height;
			if (extd_lcm_params.dsi.dsc_enable == 1)
				data_config->dst_w = extd_lcm_params.width;
		} else {
			data_config->dst_w = extd_lcm_params.dpi.width;
			data_config->dst_h = extd_lcm_params.dpi.height;
			if (extd_lcm_params.dpi.dsc_enable == 1)
				data_config->dst_w =
				    extd_lcm_params.dpi.width * 3;
		}
		data_config->dst_dirty = 1;
		data_config->p_golden_setting_context =
		    get_golden_setting_pgc();
		data_config->p_golden_setting_context->ext_dst_width =
		    data_config->dst_w;
		data_config->p_golden_setting_context->ext_dst_height =
		    data_config->dst_h;

		init_roi = 0;
		ret =
		    dpmgr_path_config(pgc->dpmgr_handle, data_config,
				      NULL);
		EXTDMSG("ext_disp_init roi w:%d, h:%d\n",
			data_config->dst_w, data_config->dst_h);
	} else
		EXTDERR("allocate buffer failed!!!\n");

	/* this will be set to always enable cmdq later */
	if (ext_disp_is_video_mode()) {
		if (dst_is_dsi)
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
					       DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_RDMA1_DONE);
		else
			dpmgr_map_event_to_irq(pgc->dpmgr_handle,
					       DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_DPI_VSYNC);
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_reset_config_handle();

	atomic_set(&g_extd_trigger_ticket, 1);
	atomic_set(&g_extd_release_ticket, 0);

	pgc->state = EXTD_INIT;
	pgc->ovl_req_state = EXTD_OVL_NO_REQ;
done:

	EXTDMSG("ext_disp_init_hdmi done\n");
	return ret;
}

static int ext_disp_init_lcm(char *lcm_name, unsigned int session)
{
	int ret = 0;
	struct disp_ddp_path_config *data_config = NULL;
	struct LCM_PARAMS *lcm_param = NULL;

	EXTDFUNC();

	if (pgc->plcm == NULL) {
		pgc->plcm = plcm_interface;
		if (pgc->plcm == NULL) {
			EXTDERR("Does not found lcm!\n");
			ret = EXT_DISP_STATUS_ERROR;
			goto done;
		}
	}

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (lcm_param == NULL) {
		EXTDERR("get lcm params FAILED\n");
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else {
		memcpy(&extd_lcm_params, lcm_param, sizeof(struct LCM_PARAMS));
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		ret =
		    cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP,
				  &(pgc->cmdq_handle_config));
		if (ret) {
			EXTDERR("cmdqRecCreate FAIL, ret=%d\n", ret);
			ret = EXT_DISP_STATUS_ERROR;
			goto done;
		}
		EXTDMSG("cmdqRecCreate SUCCESS, g_cmdq_handle=%p\n",
			pgc->cmdq_handle_config);

		/* Set fake cmdq engineflag for judge path scenario */
		cmdqRecSetEngine(pgc->cmdq_handle_config,
				 ((1LL << CMDQ_ENG_DISP_OVL1) |
				  (1LL << CMDQ_ENG_DISP_WDMA1)));

		cmdqRecReset(pgc->cmdq_handle_config);
	}

	ddp_set_dst_module(DDP_SCENARIO_SUB_DISP, DISP_MODULE_DSI1);

	if (ext_disp_mode == EXTD_DIRECT_LINK_MODE) {
		_build_path_direct_link(session);
		dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);
	} else if (ext_disp_mode == EXTD_RDMA_DPI_MODE) {
		_build_path_rdma_dpi();
		dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);
	} else
		EXTDERR("ext_disp display mode is WRONG\n");

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		_cmdq_build_trigger_loop();
		_cmdq_start_extd_trigger_loop();
	}

	dpmgr_path_set_video_mode(pgc->dpmgr_handle,
				  disp_lcm_is_video_mode(pgc->plcm));
	dpmgr_path_init(pgc->dpmgr_handle, ext_disp_use_cmdq);
	dpmgr_path_reset(pgc->dpmgr_handle, ext_disp_use_cmdq);

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	memset(&(lcm_param->dpi), 0, sizeof(struct LCM_DPI_PARAMS));
	memcpy(&(data_config->dispif_config), lcm_param,
	       sizeof(struct LCM_PARAMS));
	data_config->dst_w = ext_disp_get_width(session);
	data_config->dst_h = ext_disp_get_height(session);
	data_config->p_golden_setting_context = get_golden_setting_pgc();

	if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
		data_config->lcm_bpp = 24;
	else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
		data_config->lcm_bpp = 16;
	else if (lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
		data_config->lcm_bpp = 18;

	/*data_config->fps = lcm_fps; */
	data_config->dst_dirty = 1;

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
				ext_disp_use_cmdq ? pgc->
				cmdq_handle_config : NULL);
	EXTDMSG("ext_disp_init roi w:%d, h:%d\n", data_config->dst_w,
		data_config->dst_h);

	dpmgr_path_start(pgc->dpmgr_handle, ext_disp_use_cmdq);

	if (disp_lcm_is_video_mode(pgc->plcm)) {
		/*dpmgr_path_trigger(pgc->dpmgr_handle, NULL, 0); */
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
				       DISP_PATH_EVENT_IF_VSYNC,
				       DDP_IRQ_RDMA1_DONE);
	} else

#if 0	/* cervino no DSI1 */
		dpmgr_map_event_to_irq(pgc->dpmgr_handle,
			DISP_PATH_EVENT_IF_VSYNC,
			DDP_IRQ_DSI1_EXT_TE);
#endif
	if (ext_disp_use_cmdq) {
		_cmdq_flush_config_handle(0, NULL, 0);
		_cmdq_reset_config_handle();
		_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);
	}

	atomic_set(&g_extd_trigger_ticket, 1);
	atomic_set(&g_extd_release_ticket, 0);

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_lowpower_init();
#endif

	pgc->state = EXTD_INIT;
	pgc->ovl_req_state = EXTD_OVL_NO_REQ;
	pgc->session = session;
done:
	EXTDMSG("ext_disp_init_lcm done\n");
	return ret;
}

void ext_disp_esd_check_lock(void)
{
	mutex_lock(&esd_check_lock);
}

void ext_disp_esd_check_unlock(void)
{
	mutex_unlock(&esd_check_lock);
}

/* external display ESD RECOVERY */
int ext_disp_esd_recovery(void)
{
	int ret = 0;
	struct LCM_PARAMS *lcm_param = NULL;
	struct cmdqRecStruct *handle = NULL;

	EXTDFUNC();

	_ext_disp_path_lock(__func__);

	if (pgc->plcm == NULL) {
		pgc->plcm = plcm_interface;
		if (pgc->plcm == NULL) {
			EXTDERR("Does not found lcm!\n");
			ret = EXT_DISP_STATUS_ERROR;
			goto done;
		}
	}

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if (pgc->state != EXTD_RESUME) {
		EXTDERR
		    ("[ESD]esd recovery but extd path is slept?\n");
		goto done;
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_idlemgr_kick((char *)__func__, 0);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->esd_recovery_t,
			 MMPROFILE_FLAG_PULSE, 0, 2);

	/* blocking flush before stop trigger loop */
	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &handle);
	if (ret) {
		EXTDERR("%s:%d, create cmdq handle fail!ret=%d\n", __func__,
			__LINE__, ret);
		return -1;
	}
	/* Set fake cmdq engineflag for judge path scenario */
	cmdqRecSetEngine(handle,
			 ((1LL << CMDQ_ENG_DISP_OVL1) |
			  (1LL << CMDQ_ENG_DISP_WDMA1)));

	cmdqRecReset(handle);
	_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	EXTDINFO("[ESD]display cmdq trigger loop stop[begin]\n");
	_cmdq_stop_extd_trigger_loop();
	EXTDINFO("[ESD]display cmdq trigger loop stop[end]\n");

	EXTDINFO("[ESD]stop dpmgr path[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	EXTDINFO("[ESD]stop dpmgr path[end]\n");

	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		EXTDINFO("[ESD]external display path is busy after stop\n");
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ * 1);
		EXTDINFO("[ESD]wait frame done ret:%d\n", ret);
	}

	EXTDINFO("[ESD]reset display path[begin]\n");
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	EXTDINFO("[ESD]reset display path[end]\n");

	EXTDINFO("[ESD]lcm suspend[begin]\n");
	disp_lcm_suspend(pgc->plcm);
	EXTDINFO("[ESD]lcm force init[begin]\n");
	disp_lcm_init(pgc->plcm, 1);
	EXTDINFO("[ESD]lcm force init[end]\n");

	EXTDINFO("[ESD]start dpmgr path[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	EXTDINFO("[ESD]start dpmgr path[end]\n");
	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		EXTDERR
		    ("[ESD]Error! we didn't trigger but it's already busy\n");
		ret = -1;
	}

	EXTDINFO("[ESD]start cmdq trigger loop[begin]\n");
	_cmdq_start_extd_trigger_loop();
	EXTDINFO("[ESD]start cmdq trigger loop[end]\n");
	if (disp_lcm_is_video_mode(pgc->plcm)) {
		/* for video mode, we need to force trigger here */
		/* for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW */
		/* when trigger loop start */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}

done:
	_ext_disp_path_unlock(__func__);
	EXTDINFO("[ESD]ESD recovery end\n");

	return ret;
}

void ext_disp_probe(void)
{
	EXTDFUNC();

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	if (plcm_interface == NULL) {
		plcm_interface =
		    disp_ext_lcm_probe(ext_mtkfb_lcm_name,
				       LCM_INTERFACE_NOTDEFINED, 0);
		if (plcm_interface == NULL)
			EXTDERR("disp_ext_lcm_probe returns null\n");
		else
			EXTDMSG("disp_ext_lcm_probe SUCCESS. lcm name:%s\n",
				plcm_interface->drv->name);
	}
#endif

	ext_disp_use_cmdq = CMDQ_ENABLE;
	ext_disp_use_m4u = 1;
	ext_disp_mode = EXTD_DIRECT_LINK_MODE;

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_check_recovery_init();
	mutex_init(&esd_check_lock);
#endif
	extd_mutex_init(&(pgc->lock));
}

int ext_disp_init(char *lcm_name, unsigned int session)
{
	int ret = 0;

	EXTDFUNC();

	dpmgr_init();

	_ext_disp_path_lock(__func__);

	if (pgc->state == EXTD_DEINIT) {
		init_cmdq_slots(&(pgc->ext_cur_config_fence), EXTD_OVERLAY_CNT,
				0);
		init_cmdq_slots(&(pgc->ext_subtractor_when_free),
				EXTD_OVERLAY_CNT, 0);
		init_cmdq_slots(&(pgc->ext_input_config_info), 1, 0);
#ifdef EXTD_DEBUG_SUPPORT
		init_cmdq_slots(&(pgc->ext_ovl_rdma_status_info), 1, 0);
#endif
	}

	if (pgc->state != EXTD_DEINIT)
		EXTDERR("status is not EXTD_DEINIT!\n");

	/* Register external session cmdq dump callback */
	/* dpmgr_register_cmdq_dump_callback(ext_disp_cmdq_dump); */

	if (DISP_SESSION_DEV(session) == DEV_LCM)
		ret = ext_disp_init_lcm(lcm_name, session);
	else
		ret = ext_disp_init_hdmi(session);

	_ext_disp_path_unlock(__func__);

	return ret;
}

int ext_disp_deinit(unsigned int session)
{
	int loop_cnt = 0;

	EXTDFUNC();

	_ext_disp_path_lock(__func__);

	if (pgc->state == EXTD_DEINIT)
		goto deinit_exit;

	while (((atomic_read(&g_extd_trigger_ticket) -
		 atomic_read(&g_extd_release_ticket)) != 1)
	       && (loop_cnt < 10)) {
		_ext_disp_path_unlock(__func__);
		usleep_range(5000, 6000);
		_ext_disp_path_lock(__func__);
		/* wait the last configuration done */
		loop_cnt++;
	}

	if (DISP_SESSION_DEV(session) == DEV_LCM) {
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		external_display_esd_check_enable(0);
#endif
		if (pgc->state == EXTD_RESUME) {
			_ext_disp_path_unlock(__func__);
			ext_disp_suspend(session);
			_ext_disp_path_lock(__func__);
		}
	}

	if (pgc->state == EXTD_SUSPEND)
		dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_destroy_path_handle(pgc->dpmgr_handle);

	/* Release present timeline fence */
	/* cervino no external display */
	/* mtkfb_release_present_timeline_fence(pgc->session); */

	cmdqRecDestroy(pgc->cmdq_handle_config);
	cmdqRecDestroy(pgc->cmdq_handle_trigger);
	pgc->cmdq_handle_config = NULL;
	pgc->cmdq_handle_trigger = NULL;

	if (pgc->state != EXTD_DEINIT) {
		deinit_cmdq_slots(pgc->ext_cur_config_fence);
		deinit_cmdq_slots(pgc->ext_subtractor_when_free);
		deinit_cmdq_slots(pgc->ext_input_config_info);
#ifdef EXTD_DEBUG_SUPPORT
		deinit_cmdq_slots(pgc->ext_ovl_rdma_status_info);
#endif
	}

	/* Unregister external session cmdq dump callback */
	/* dpmgr_unregister_cmdq_dump_callback(ext_disp_cmdq_dump); */

	pgc->state = EXTD_DEINIT;

deinit_exit:
	_ext_disp_path_unlock(__func__);
	EXTDMSG("ext_disp_deinit done\n");
	return 0;
}

int ext_disp_wait_for_vsync(void *config, unsigned int session)
{
	int ret = 0;

	/* EXTDFUNC(); */

	if (pgc->state != EXTD_RESUME && pgc->state != EXTD_INIT) {
		EXTDERR("%s: External display path is suspended\n", __func__);
		mdelay(20);
		return -1;
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	if ((pgc->lcm_state == EXTD_LCM_SUSPEND)
	    || (pgc->lcm_state == EXTD_LCM_NO_INIT)) {
		EXTDERR("%s: SUB LCM is suspended\n", __func__);
		return -1;
	}

	/* kick idle manager here to ensure sodi is disabled */
	/* when screen update begin(not 100% ensure) */
	external_display_idlemgr_kick((char *)__func__, 1);
#endif

	ret =
	    dpmgr_wait_event_timeout(pgc->dpmgr_handle,
				     DISP_PATH_EVENT_IF_VSYNC, HZ / 10);

	if (ret == -2) {
		EXTDERR("vsync for ext display path not enabled yet\n");
		return -1;
	}
	/*EXTDINFO("ext_disp_wait_for_vsync - vsync signaled\n"); */

	return ret;
}

static int ext_disp_suspend_release_fence(unsigned int session)
{
	unsigned int i = 0;

	EXTDFUNC();

	/* Release input fence */
	for (i = 0; i < EXTERNAL_SESSION_INPUT_LAYER_COUNT; i++) {
		DISPPR_FENCE
		    ("ext_disp_suspend_release_fence sess=0x%x,layerid=%d\n",
		     session, i);
		mtkfb_release_layer_fence(session, i);
	}
	EXTDINFO("ext_disp_suspend_release_fence done\n");
	return 0;
}

int ext_disp_suspend(unsigned int session)
{
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	EXTDFUNC();

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	ext_disp_esd_check_lock();
#endif
	_ext_disp_path_lock(__func__);

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND
	    || session != pgc->session) {
		EXTDERR("status is not EXTD_RESUME or session is not match\n");
		goto done;
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_idlemgr_kick((char *)__func__, 0);
#endif

	pgc->need_trigger_overlay = 0;

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 10);

	if (ext_disp_use_cmdq == CMDQ_ENABLE
	    && DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_stop_extd_trigger_loop();

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (DISP_SESSION_DEV(session) == DEV_LCM) {
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		external_display_esd_check_enable(0);
#endif
		EXTDMSG("lcm suspend[begin]\n");
		disp_lcm_suspend(pgc->plcm);
		EXTDMSG("lcm suspend[end]\n");
		pgc->lcm_state = EXTD_LCM_SUSPEND;
		ext_disp_suspend_release_fence(session);
	}

	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);

	/* Unregister external session cmdq dump callback */
	/* dpmgr_unregister_cmdq_dump_callback(ext_disp_cmdq_dump); */

	pgc->state = EXTD_SUSPEND;
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	ext_disp_set_state(EXTD_SUSPEND);
#endif
done:
	_ext_disp_path_unlock(__func__);
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	ext_disp_esd_check_unlock();
#endif

	EXTDMSG("ext_disp_suspend done\n");
	return ret;
}

int ext_disp_resume(unsigned int session)
{
	struct disp_ddp_path_config *data_config;
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;
	int i = 0;

	EXTDFUNC();

	_ext_disp_path_lock(__func__);

	if (pgc->state != EXTD_SUSPEND || session != pgc->session) {
		EXTDERR("EXTD_DEINIT/EXTD_INIT/EXTD_RESUME\n");
		goto done;
	}

	/* Register external session cmdq dump callback */
	/* dpmgr_register_cmdq_dump_callback(ext_disp_cmdq_dump); */

	init_roi = 1;

	if (_should_reset_cmdq_config_handle()
	    && DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_reset_config_handle();

	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (DISP_SESSION_DEV(session) == DEV_LCM) {
		data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		memcpy(&(data_config->dispif_config), &extd_lcm_params,
		       sizeof(struct LCM_PARAMS));
		data_config->dst_w = ext_disp_get_width(session);
		data_config->dst_h = ext_disp_get_height(session);
		data_config->p_golden_setting_context =
		    get_golden_setting_pgc();

		if (extd_lcm_params.dsi.data_format.format ==
		    LCM_DSI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if (extd_lcm_params.dsi.data_format.format ==
			 LCM_DSI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		else if (extd_lcm_params.dsi.data_format.format ==
			 LCM_DSI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;

		/*data_config->fps = lcm_fps; */
		data_config->dst_dirty = 1;

		/* disable all ovl layers to show black screen */
		for (i = 0; i < ARRAY_SIZE(data_config->ovl_config); i++)
			data_config->ovl_config[i].layer_en = 0;

		data_config->ovl_dirty = 1;

		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		data_config->dst_dirty = 0;

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			EXTDERR
			    ("Error! we didn't start but already busy\n");
			ret = EXT_DISP_STATUS_ERROR;
		}

		if (DISP_SESSION_DEV(session) == DEV_LCM) {
			EXTDMSG("[POWER]lcm resume[begin]\n");
			disp_lcm_resume(pgc->plcm);
			EXTDMSG("[POWER]lcm resume[end]\n");
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
			external_display_esd_check_enable(1);
#endif

			pgc->lcm_state = EXTD_LCM_RESUME;
		}

		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

		if (ext_disp_use_cmdq == CMDQ_ENABLE)
			_cmdq_build_trigger_loop();

		if (disp_lcm_is_video_mode(pgc->plcm)) {
			/* for video mode, we need to force trigger here */
			/* for cmd mode, just set */
			/* DPREC_EVENT_CMDQ_SET_EVENT_ALLOW */
			/* when trigger loop start */
			if (_should_insert_wait_frame_done_token())
				_ext_cmdq_insert_wait_frame_done_token
					(pgc->cmdq_handle_config);

			dpmgr_path_trigger(pgc->dpmgr_handle, NULL,
					   CMDQ_DISABLE);
		}
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE
	    && DISP_SESSION_DEV(session) != DEV_EINK + 1)
		_cmdq_start_extd_trigger_loop();

	if (DISP_SESSION_DEV(session) != DEV_LCM) {
		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			EXTDERR("stop display path failed, still busy\n");
			ret = -1;
			goto done;
		}
	}

	if (DISP_SESSION_DEV(session) == DEV_EINK + 1)
		pgc->suspend_config = 0;

	pgc->state = EXTD_RESUME;
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
			(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	ext_disp_set_state(EXTD_RESUME);
#endif

done:
	_ext_disp_path_unlock(__func__);
	EXTDMSG("ext_disp_resume done\n");
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

	EXTDFUNC();

	cmdqBackupReadSlot(pgc->ext_input_config_info, 0, &input_config_info);

	if (input_config_info & 0x100) {	/*input source is rdma */
		if (ext_disp_get_ovl_req_status(pgc->session) ==
		    EXTD_OVL_REMOVED)
			ext_disp_path_change(EXTD_OVL_NO_REQ, pgc->session);
	} else {
		if (ext_disp_get_ovl_req_status(pgc->session) ==
		    EXTD_OVL_INSERTED)
			ext_disp_path_change(EXTD_OVL_NO_REQ, pgc->session);
	}

	_ext_disp_path_lock(__func__);

	for (i = 0; i < EXTD_OVERLAY_CNT; i++) {
		cmdqBackupReadSlot(pgc->ext_cur_config_fence, i, &fence_idx);
		cmdqBackupReadSlot(pgc->ext_subtractor_when_free, i,
				   &subtractor);

		mtkfb_release_fence(pgc->session, i, fence_idx - subtractor);

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_UsedBuff,
				 MMPROFILE_FLAG_PULSE, fence_idx, i);
	}

	/* Release present fence */
#if 0
	if (gCurrentPresentFenceIndex != -1)
		mtkfb_release_present_fence(pgc->session,
			gCurrentPresentFenceIndex);
#endif
#ifdef EXTD_DEBUG_SUPPORT
	/* check last ovl/rdma status: should be idle when config */
	cmdqBackupReadSlot(pgc->ext_ovl_rdma_status_info, 0, &status);
	if (input_config_info & 0x100) {	/*input source is rdma */
		if ((status & 0x1000) != 0) {
			/* rdma smi is not idle !! */
			EXTDERR("extd rdma-smi status error!! stat=0x%x\n",
				status);
			ext_disp_diagnose();
			ret = -1;
		}
	} else {
		if ((status & 0x1) != 0) {
			/* ovl is not idle !! */
			EXTDERR("extd ovl status error!! stat=0x%x\n", status);
			ext_disp_diagnose();
			ret = -1;
		}
	}
#endif

#if defined(CONFIG_MTK_HDMI_SUPPORT)
	if (pgc->state == EXTD_RESUME)
		/* hdmi video config with layer_type */
		external_display_util.
		    hdmi_video_format_config(input_config_info & 0xff);
	else
		EXTDMSG
		    ("ext_fence_release_callback ext display is not resume\n");
#endif

	atomic_set(&g_extd_release_ticket, userdata);

	_ext_disp_path_unlock(__func__);

	EXTDINFO("ext_fence_release_callback done\n");

	return ret;
}

int ext_disp_trigger(int blocking, void *callback, unsigned int userdata,
		     unsigned int session)
{
	int ret = 0;

	/* EXTDFUNC(); */
	_ext_disp_path_lock(__func__);

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND
	    || pgc->need_trigger_overlay < 1) {
		EXTDERR("trigger ext display is already slept\n");
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Trigger, 0);
		_ext_disp_path_unlock(__func__);
		return -1;
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_idlemgr_kick((char *)__func__, 0);
#endif

	if (pgc->mode == EXTD_DECOUPLE_MODE)
		ret =
		    dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL,
				       ext_disp_use_cmdq);
	else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL
		 && DISP_SESSION_DEV(session) == DEV_MHL + 1)
		ret =
		    _ext_disp_trigger(blocking, callback,
				      atomic_read(&g_extd_trigger_ticket));
	else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL
		 && DISP_SESSION_DEV(session) == DEV_EINK + 1)
		ret =
		    _ext_disp_trigger_EPD(blocking, callback,
					  atomic_read(&g_extd_trigger_ticket));
	else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL
		 && DISP_SESSION_DEV(session) == DEV_LCM)
		ret =
		    _ext_disp_trigger_LCM(blocking, callback,
					  atomic_read(&g_extd_trigger_ticket));
	else
		goto done;

	atomic_add(1, &g_extd_trigger_ticket);

	pgc->state = EXTD_RESUME;
done:
	_ext_disp_path_unlock(__func__);
	/* EXTDINFO("ext_disp_trigger done\n"); */

	return ret;
}

int ext_disp_suspend_trigger(void *callback, unsigned int userdata,
			     unsigned int session)
{
	enum EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	EXTDFUNC();

	_ext_disp_path_lock(__func__);

	if (pgc->state != EXTD_RESUME) {
		EXTDERR("trigger ext display is already slept\n");
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Trigger, 0);
		_ext_disp_path_unlock(__func__);
		return -1;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_PULSE,
			 Suspend, 0);

	if (_should_reset_cmdq_config_handle())
		_cmdq_reset_config_handle();

	if (_should_insert_wait_frame_done_token())
		_ext_cmdq_insert_wait_frame_done_token(pgc->cmdq_handle_config);

	pgc->need_trigger_overlay = 0;

	if (ext_disp_use_cmdq == CMDQ_ENABLE
	    && DISP_SESSION_DEV(session) != DEV_EINK + 1)
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

	_ext_disp_path_unlock(__func__);

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_PULSE,
			 Suspend, 1);
	return ret;
}

int ext_disp_frame_cfg_input(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	int i = 0;
	int layer_cnt = 0;
	int config_layer_id = 0;
	M4U_PORT_STRUCT sPort;
	struct disp_ddp_path_config *data_config;
	unsigned int ext_last_fence, ext_cur_fence, ext_sub, input_source;
	struct ddp_io_golden_setting_arg gset_arg;

	unsigned int session = cfg->session_id;

	EXTDFUNC();
	_ext_disp_path_lock(__func__);

	if (pgc->state != EXTD_INIT && pgc->state != EXTD_RESUME
	    && pgc->suspend_config != 1) {
		EXTDERR("config ext disp is already slept, state:%d\n",
			pgc->state);
		mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
				 MMPROFILE_FLAG_PULSE, Config,
				 cfg->input_cfg[0].next_buff_idx);
		_ext_disp_path_unlock(__func__);
		return -2;
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_idlemgr_kick((char *)__func__, 0);
#endif

	for (i = 0; i < cfg->input_layer_num; i++) {
		if (cfg->input_cfg[i].layer_enable)
			layer_cnt++;
	}

	if (layer_cnt == 1) {
		ext_disp_path_change(EXTD_OVL_REMOVE_REQ, session);
		if (ext_disp_get_ovl_req_status(session) ==
				EXTD_OVL_REMOVE_REQ) {
			EXTDINFO("config M4U Port DISP_MODULE_RDMA1\n");
			sPort.ePortID = M4U_PORT_UNKNOWN;
			sPort.Virtuality = 1;
			sPort.Security = 0;
			sPort.Distance = 1;
			sPort.Direction = 0;
			ret = m4u_config_port(&sPort);
			if (ret != 0)
				EXTDERR
				    ("config M4U Port %d FAIL\n",
				    sPort.ePortID);

			if (DISP_SESSION_DEV(session) == DEV_LCM)
				ddp_set_dst_module(DDP_SCENARIO_SUB_RDMA1_DISP,
						   DISP_MODULE_DSI1);

			ext_disp_path_set_mode(EXTD_RDMA_DPI_MODE, session);

			pgc->ovl_req_state = EXTD_OVL_REMOVING;
		}
	} else if (layer_cnt > 1) {
		ext_disp_path_change(EXTD_OVL_INSERT_REQ, session);
		if (ext_disp_get_ovl_req_status(session) ==
				EXTD_OVL_INSERT_REQ) {
			EXTDINFO("config M4U Port DISP_MODULE_OVL1_2L\n");
			sPort.ePortID = M4U_PORT_UNKNOWN;
			sPort.Virtuality = 1;
			sPort.Security = 0;
			sPort.Distance = 1;
			sPort.Direction = 0;
			ret = m4u_config_port(&sPort);
			if (ret != 0)
				EXTDERR
				    ("config M4U Port DISP_MODULE_OVL1 FAIL\n");

			if (DISP_SESSION_DEV(session) == DEV_LCM)
				ddp_set_dst_module(DDP_SCENARIO_SUB_DISP,
						   DISP_MODULE_DSI1);

			ext_disp_path_set_mode(EXTD_DIRECT_LINK_MODE, session);

			pgc->ovl_req_state = EXTD_OVL_INSERTING;
		}
	}
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	external_display_idlemgr_kick((char *)__func__, 0);
#endif

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	if (dst_is_dsi) {
		data_config->dst_w = extd_lcm_params.width;
		data_config->dst_h = extd_lcm_params.height;
		if (extd_lcm_params.dsi.dsc_enable == 1)
			data_config->dst_w = extd_lcm_params.width;
	} else {
		data_config->dst_w = ext_disp_get_width(session);
		data_config->dst_h = ext_disp_get_height(session);
		if (extd_lcm_params.dpi.dsc_enable == 1)
			data_config->dst_w = extd_lcm_params.dpi.width * 3;
	}

	/* hope we can use only 1 input struct for input config, */
	/* just set layer number */
	if (_should_config_ovl_input()) {
		for (i = 0; i < cfg->input_layer_num; i++) {
			config_layer_id = cfg->input_cfg[i].layer_id;
			ret =
			    _convert_disp_input_to_ovl(&
						       (data_config->
							ovl_config
							[config_layer_id]),
						       &(cfg->input_cfg[i]));
			dprec_mmp_dump_ovl_layer(&
						 (data_config->
						  ovl_config[config_layer_id]),
						 config_layer_id, 2);

			if (init_roi == 1) {
				memcpy(&(data_config->dispif_config),
				       &extd_lcm_params,
				       sizeof(struct LCM_PARAMS));

				if (DISP_SESSION_DEV(session) == DEV_LCM)
					EXTDINFO("set dest w:%d, h:%d\n",
						 data_config->dst_w,
						 data_config->dst_h);
				else
					EXTDINFO("set dest w:%d, h:%d\n",
						 extd_lcm_params.dpi.width,
						 extd_lcm_params.dpi.height);

				data_config->dst_dirty = 1;
				data_config->rdma_config.address = 0;
			}
			data_config->ovl_dirty = 1;
			pgc->need_trigger_overlay = 1;
		}
		input_source = 0;

		memset(&gset_arg, 0, sizeof(gset_arg));
		gset_arg.dst_mod_type =
		    dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
		gset_arg.is_decouple_mode = 1;

		dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
				 DDP_OVL_GOLDEN_SETTING, &gset_arg);
	} else {
		struct OVL_CONFIG_STRUCT ovl_config;

		_convert_disp_input_to_ovl(&ovl_config, &(cfg->input_cfg[0]));
		dprec_mmp_dump_ovl_layer(&ovl_config,
					 cfg->input_cfg[0].layer_id, 2);

		ret =
		    _convert_disp_input_to_rdma(&(data_config->rdma_config),
						&(cfg->input_cfg[0]),
						data_config->dst_w,
						data_config->dst_h);
		if (data_config->rdma_config.address) {
			data_config->rdma_dirty = 1;
			pgc->need_trigger_overlay = 1;
		}
		input_source = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	memcpy(&(data_config->dispif_config), &extd_lcm_params,
	       sizeof(struct LCM_PARAMS));
	ret =
	    dpmgr_path_config(pgc->dpmgr_handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->
			      cmdq_handle_config : NULL);

	/* this is used for decouple mode, */
	/* to indicate whether we need to trigger ovl */
	/* pgc->need_trigger_overlay = 1; */
	init_roi = 0;

	for (i = 0; i < cfg->input_layer_num; i++) {
		cmdqBackupReadSlot(pgc->ext_cur_config_fence, i,
				   &ext_last_fence);
		ext_cur_fence = cfg->input_cfg[i].next_buff_idx;

		if (ext_cur_fence != -1 && ext_cur_fence > ext_last_fence) {
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
						pgc->ext_cur_config_fence, i,
						ext_cur_fence);
		}
		/* for dim_layer/disable_layer/no_fence_layer, */
		/* just release all fences configured */
		/* for other layers, release current_fence-1 */
		if (cfg->input_cfg[i].buffer_source == DISP_BUFFER_ALPHA
		    || cfg->input_cfg[i].layer_enable == 0
		    || ext_cur_fence == -1)
			ext_sub = 0;
		else
			ext_sub = 1;

		cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
					pgc->ext_subtractor_when_free, i,
					ext_sub);
	}

	cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
				pgc->ext_input_config_info, 0,
				(input_source << 8) | (cfg->input_cfg[0].
						       layer_type));

	/* Update present fence index */
	if (cfg->present_fence_idx != (unsigned int)-1)
		gCurrentPresentFenceIndex = cfg->present_fence_idx;

#ifdef EXTD_DEBUG_SUPPORT
	if (_should_config_ovl_input())
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config,
					    pgc->ext_ovl_rdma_status_info,
					    0,
					    disp_addr_convert
					    (DDP_REG_BASE_DISP_OVL1 +
					     DISP_REG_OVL_STA));
	else
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config,
					    pgc->ext_ovl_rdma_status_info,
					    0,
					    disp_addr_convert
					    (DISP_REG_RDMA_GLOBAL_CON +
					     DISP_RDMA_INDEX_OFFSET));
#endif

	if (data_config->ovl_dirty)
		EXTDINFO
		    ("%s idx:%d -w:%d, h:%d, pitch:%d\n",
			 __func__,
		     cfg->input_cfg[0].next_buff_idx,
		     data_config->ovl_config[0].src_w,
		     data_config->ovl_config[0].src_h,
		     data_config->ovl_config[0].src_pitch);
	else
		EXTDINFO
		    ("%s idx:%d -w:%d, h:%d, pitch:%d, mva:0x%lx\n",
		     __func__,
		     cfg->input_cfg[0].next_buff_idx,
		     data_config->rdma_config.width,
		     data_config->rdma_config.height,
		     data_config->rdma_config.pitch,
		     data_config->rdma_config.address);

	_ext_disp_path_unlock(__func__);

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

	/* EXTDFUNC(); */
	_ext_disp_path_lock(__func__);
	temp = pgc->state;
	_ext_disp_path_unlock(__func__);

	return temp;
}

int ext_disp_is_sleepd(void)
{
	unsigned int temp = 0;

	/* EXTDFUNC(); */
	_ext_disp_path_lock(__func__);
	temp = !pgc->state;
	_ext_disp_path_unlock(__func__);

	return temp;
}

int ext_disp_get_width(unsigned int session)
{
	int ret = extd_lcm_params.dpi.width;

	if (DISP_SESSION_DEV(session) == DEV_LCM) {
		if (pgc->plcm && pgc->plcm->params)
			ret = pgc->plcm->params->width;
	}
	return ret;
}

int ext_disp_get_height(unsigned int session)
{
	int ret = extd_lcm_params.dpi.height;

	if (DISP_SESSION_DEV(session) == DEV_LCM) {
		if (pgc->plcm && pgc->plcm->params)
			ret = pgc->plcm->params->height;
	}
	return ret;
}

unsigned int ext_disp_get_sess_id(void)
{
	unsigned int session_id = is_context_inited > 0 ? pgc->session : 0;
	return session_id;
}

int ext_disp_is_video_mode(void)
{
	int ret = 1;

	if (pgc->plcm)
		ret = disp_lcm_is_video_mode(pgc->plcm);

	return ret;
}

int ext_disp_diagnose(void)
{
	int ret = 0;

	if (is_context_inited > 0) {
		EXTDMSG("ext_disp_diagnose, is_context_inited --%d\n",
			is_context_inited);
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
	_ext_disp_path_lock(__func__);

	ext_disp_use_cmdq = use_cmdq;
	EXTDMSG("display driver use %s to config register now\n",
		(use_cmdq == CMDQ_ENABLE) ? "CMDQ" : "CPU");

	_ext_disp_path_unlock(__func__);
	return ext_disp_use_cmdq;
}

void ext_disp_get_curr_addr(unsigned long *input_curr_addr, int module)
{
	dpmgr_get_input_address(pgc->dpmgr_handle, input_curr_addr);
}

int ext_disp_factory_test(int mode, void *config)
{
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
/*	dpmgr_set_ovl1_status(status);*/
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

/*	_ext_disp_path_lock();*/
	ret = pgc->ovl_req_state;
/*	_ext_disp_path_unlock();*/

	return ret;
}

int ext_disp_path_change(enum EXTD_OVL_REQ_STATUS action, unsigned int session)
{
/*	EXTDFUNC();*/

	if (EXTD_OVERLAY_CNT > 0) {
/*		_ext_disp_path_lock();*/
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
		case EXTD_OVL_INSERTING:
			pgc->ovl_req_state = EXTD_OVL_INSERTING;
			break;
		case EXTD_OVL_INSERTED:
			pgc->ovl_req_state = EXTD_OVL_INSERTED;
			break;
		default:
			break;
		}

/*		_ext_disp_path_unlock();*/
	}

	return 0;
}

int ext_disp_wait_ovl_available(int ovl_num)
{
	int ret = 0;

	if (EXTD_OVERLAY_CNT > 0) {
		/*wait OVL can be used by external display */
		ret = dpmgr_wait_ovl_available(ovl_num);
	}

	return ret;
}

bool ext_disp_path_source_is_RDMA(unsigned int session)
{
	bool is_rdma = false;

	if ((ext_disp_mode == EXTD_RDMA_DPI_MODE
	     && pgc->ovl_req_state != EXTD_OVL_REMOVE_REQ
	     && pgc->ovl_req_state != EXTD_OVL_REMOVING)
	    || (ext_disp_mode == EXTD_DIRECT_LINK_MODE
		&& pgc->ovl_req_state == EXTD_OVL_INSERT_REQ)) {
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

void extd_disp_get_interface(struct disp_lcm_handle **plcm)
{
	if (plcm_interface == NULL) {
		plcm_interface =
		    disp_ext_lcm_probe(NULL, LCM_INTERFACE_NOTDEFINED, 0);
		if (plcm_interface == NULL)
			EXTDERR("disp_lcm_probe returns null\n");
	}

	*plcm = plcm_interface;
}

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) &&	\
		(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
static DECLARE_WAIT_QUEUE_HEAD(ext_disp_state_wait_queue);

enum EXTD_POWER_STATE ext_disp_get_state(void)
{
	return pgc->state;
}

enum EXTD_POWER_STATE ext_disp_set_state(enum EXTD_POWER_STATE new_state)
{
	enum EXTD_POWER_STATE old_state = pgc->state;

	pgc->state = new_state;
	EXTDINFO("%s %d to %d\n", __func__, old_state, new_state);
	wake_up(&ext_disp_state_wait_queue);
	return old_state;
}

/* use MAX_SCHEDULE_TIMEOUT to wait for ever
 * NOTES: _ext_disp_path_lock should NOT be held when call this func !!!!!!!!
 */
#define __ext_disp_wait_state(condition, timeout) \
	wait_event_timeout(ext_disp_state_wait_queue, condition, timeout)

long ext_disp_wait_state(enum EXTD_POWER_STATE state, long timeout)
{
	long ret;

	ret = __ext_disp_wait_state(ext_disp_get_state() == state, timeout);
	return ret;
}

void *ext_disp_get_dpmgr_handle(void)
{
	return pgc->dpmgr_handle;
}

static void _cmdq_flush_config_handle_mira(void *handle, int blocking)
{
	if (blocking)
		cmdqRecFlush(handle);
	else
		cmdqRecFlushAsync(handle);
}

static int _set_backlight_by_cmdq(unsigned int level)
{
	int ret = 0;
	struct cmdqRecStruct *cmdq_handle_backlight = NULL;

	EXTDFUNC();

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &cmdq_handle_backlight);
	if (ret) {
		EXTDERR("%s:%d, create cmdq handle fail!ret=%d\n", __func__,
			__LINE__, ret);
		ret = -1;
		goto done;
	}
	EXTDINFO("external backlight, handle=%p\n", cmdq_handle_backlight);
	cmdqRecSetEngine(cmdq_handle_backlight,
			 ((1LL << CMDQ_ENG_DISP_OVL1) |
			  (1LL << CMDQ_ENG_DISP_WDMA1)));

	cmdqRecReset(cmdq_handle_backlight);

	if (ext_disp_is_video_mode()) {
		_ext_cmdq_insert_wait_frame_done_token(cmdq_handle_backlight);
		disp_lcm_set_backlight(pgc->plcm, cmdq_handle_backlight, level);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		EXTDMSG("[BL]_set_backlight_by_cmdq ret=%d\n", ret);
	} else {
		cmdqRecWait(cmdq_handle_backlight,
			    CMDQ_SYNC_TOKEN_EXT_CABC_EOF);
		_cmdq_handle_clear_dirty(cmdq_handle_backlight);
		_ext_cmdq_insert_wait_frame_done_token(cmdq_handle_backlight);
		disp_lcm_set_backlight(pgc->plcm, cmdq_handle_backlight, level);
		cmdqRecSetEventToken(cmdq_handle_backlight,
				     CMDQ_SYNC_TOKEN_EXT_CABC_EOF);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		EXTDMSG("[BL]_set_backlight_by_cmdq ret=%d\n", ret);
	}
	cmdqRecDestroy(cmdq_handle_backlight);
	cmdq_handle_backlight = NULL;

done:
	return ret;
}

static int _set_backlight_by_cpu(unsigned int level)
{
	int ret = 0;

	EXTDFUNC();

	if (ext_disp_is_video_mode()) {
		disp_lcm_set_backlight(pgc->plcm, NULL, level);
	} else {
		EXTDMSG("[BL]display cmdq trigger loop stop[begin]\n");
		if (ext_disp_cmdq_enabled())
			_cmdq_stop_extd_trigger_loop();

		EXTDMSG("[BL]display cmdq trigger loop stop[end]\n");

		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			EXTDMSG("[BL]external display path is busy\n");
			ret =
			    dpmgr_wait_event_timeout(pgc->dpmgr_handle,
						     DISP_PATH_EVENT_FRAME_DONE,
						     HZ * 1);
			EXTDMSG("[BL]wait frame done ret:%d\n", ret);
		}

		EXTDMSG("[BL]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		EXTDMSG("[BL]stop dpmgr path[end]\n");
		if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
			EXTDMSG
			    ("[BL]external display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle,
						 DISP_PATH_EVENT_FRAME_DONE,
						 HZ * 1);
			EXTDMSG("[BL]wait frame done ret:%d\n", ret);
		}
		EXTDMSG("[BL]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		EXTDMSG("[BL]reset display path[end]\n");

		disp_lcm_set_backlight(pgc->plcm, NULL, level);

		EXTDMSG("[BL]start dpmgr path[begin]\n");
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		EXTDMSG("[BL]start dpmgr path[end]\n");

		if (ext_disp_cmdq_enabled()) {
			EXTDMSG("[BL]start cmdq trigger loop[begin]\n");
			_cmdq_start_extd_trigger_loop();
		}
		EXTDMSG("[BL]start cmdq trigger loop[end]\n");
	}

	return ret;
}

int external_display_setbacklight(unsigned int level)
{
	int ret = 0;
	static unsigned int last_level;

	EXTDFUNC();

	if (last_level == level)
		return 0;

	ext_disp_esd_check_lock();
	_ext_disp_path_lock(__func__);

	if (pgc->state == EXTD_SUSPEND) {
		EXTDERR("%s: external sleep state set backlight invald\n",
			__func__);
	} else if ((pgc->lcm_state == EXTD_LCM_SUSPEND)
		   || (pgc->lcm_state == EXTD_LCM_NO_INIT)) {
		EXTDERR("%s: SUB LCM is suspended\n", __func__);
	} else {
		external_display_idlemgr_kick((char *)__func__, 0);
		if (ext_disp_cmdq_enabled()) {
			if (ext_disp_is_video_mode())
				disp_lcm_set_backlight(pgc->plcm, NULL, level);
			else
				_set_backlight_by_cmdq(level);
		} else {
			_set_backlight_by_cpu(level);
		}
		last_level = level;
	}

	_ext_disp_path_unlock(__func__);
	ext_disp_esd_check_unlock();

	EXTDINFO("external_display_setbacklight done\n");
	return ret;
}
#endif
