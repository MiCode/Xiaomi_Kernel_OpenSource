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
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include "hdmitx.h"

#include "mtkfb_info.h"
#include "mtkfb.h"
#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_info.h"
#include "m4u.h"
#include "m4u_port.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_manager.h"
#include "ddp_mmp.h"
#include "ddp_dpi.h"
#include "extd_platform.h"
/*#include "extd_drv.h"*/
#include "extd_drv_log.h"
#include "extd_lcm.h"
#include "extd_utils.h"
#include "extd_ddp.h"
#include "extd_kernel_drv.h"
#include "disp_session.h"
#include "display_recorder.h"

#include "ion_drv.h"
#include <linux/slab.h>
#include "mtk_ion.h"
#include "disp_drv_platform.h"

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#include "ddp_debug.h"
#endif

#include "mtkfb_fence.h"
#include "mtk_sync.h"

#include "ddp_rdma.h"
/*for irq handle register*/
#include "ddp_irq.h"
#include "ddp_reg.h"


#define DISP_INTERNAL_BUFFER_COUNT 3
/* split 4k buffer into non-4k buffer(according to 1080p res), max count is 4, support 2/3/4 now */
#define DISP_SPLIT_COUNT 4
#define HDMI_MAX_WIDTH 4096
#define HDMI_MAX_HEIGHT 2160

unsigned int g_ext_PresentFenceIndex = 0;

static struct mutex vsync_mtx;

unsigned long framebuffer_mva;
unsigned long framebuffer_va;

static int first_build_path_decouple = 1;
static unsigned long dc_vAddr[DISP_INTERNAL_BUFFER_COUNT];
static unsigned long split_dc_vAddr[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
static struct disp_internal_buffer_info *decouple_buffer_info[DISP_INTERNAL_BUFFER_COUNT];

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
DISP_BUFFER_TYPE g_wdma_rdma_security = DISP_NORMAL_BUFFER;
static DISP_BUFFER_TYPE g_rdma_security = DISP_NORMAL_BUFFER;
unsigned int gDebugSvpHdmiAlwaysSec = 0;
#endif

struct disp_internal_buffer_info {
	struct list_head list;
	struct ion_handle *handle;
	struct sync_fence *pfence;
	uint32_t fence_id;
	uint32_t mva;
	void *va;
	uint32_t size;
	uint32_t output_fence_id;
	uint32_t interface_fence_id;
	unsigned long long timestamp;
};

int ext_disp_use_cmdq = CMDQ_ENABLE;
int ext_disp_use_m4u = 1;
#ifdef HDMI_SUB_PATH
EXT_DISP_PATH_MODE ext_disp_mode = EXTD_DECOUPLE_MODE;
#else
EXT_DISP_PATH_MODE ext_disp_mode = EXTD_DIRECT_LINK_MODE;
#endif
static int ext_disp_use_frc = 1;
struct task_struct *ext_frame_update_task = NULL;
wait_queue_head_t ext_frame_update_wq;
atomic_t ext_frame_update_event = ATOMIC_INIT(0);

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

enum EXTD_POWER_STATE {
	EXTD_DEINIT = 0,
	EXTD_INIT,
	EXTD_RESUME,
	EXTD_SUSPEND
};

typedef struct {
	enum EXTD_POWER_STATE state;
	int init;
	unsigned int session;
	int need_trigger_overlay;
	EXT_DISP_PATH_MODE mode;
	unsigned int last_vsync_tick;
	struct mutex lock;
	extd_drv_handle *plcm;
	cmdqRecHandle cmdq_handle_config;
	cmdqRecHandle cmdq_rdma_handle_config;
	cmdqRecHandle cmdq_handle_trigger;
	disp_path_handle dpmgr_handle;
	disp_path_handle ovl2mem_path_handle;
	unsigned int dc_buf_id;
	unsigned int dc_rdma_buf_id;
	unsigned int dc_buf[DISP_INTERNAL_BUFFER_COUNT];
	unsigned int dc_split_buf[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	unsigned int cur_wdma_sec_buf_id;	/* get wdma secure buffer offset */
	unsigned int cur_rdma_sec_buf_id;	/* get rdma secure buffer offset */
	unsigned int dc_sec_buf[DISP_INTERNAL_BUFFER_COUNT];
	unsigned int dc_split_sec_buf[DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT];
#endif
	cmdqBackupSlotHandle cur_config_fence;
	cmdqBackupSlotHandle subtractor_when_free;
	cmdqBackupSlotHandle ovl_status_info;
} ext_disp_path_context;

#define pgc	_get_context()

static int is_context_inited;

static void clear_ext_disp_path_context(ext_disp_path_context *pcontext)
{
	/* clear all members except wdma/rdma mva */
	pcontext->state = 0;
	pcontext->init = 0;
	pcontext->session = 0;
	pcontext->need_trigger_overlay = 0;
	pcontext->mode = 0;
	pcontext->last_vsync_tick = 0;
	memset((void *)&pcontext->lock, 0, sizeof(pcontext->lock));
	pcontext->plcm = NULL;
	pcontext->cmdq_handle_config = NULL;
	pcontext->cmdq_rdma_handle_config = NULL;
	pcontext->cmdq_handle_trigger = NULL;
	pcontext->dpmgr_handle = NULL;
	pcontext->ovl2mem_path_handle = NULL;
	pcontext->dc_buf_id = 0;
	pcontext->dc_rdma_buf_id = 0;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	pcontext->cur_wdma_sec_buf_id = 0;
	pcontext->cur_rdma_sec_buf_id = 0;
#endif
	pcontext->cur_config_fence = 0;
	pcontext->subtractor_when_free = 0;
}

static ext_disp_path_context *_get_context(void)
{
	static ext_disp_path_context g_context;

	if (!is_context_inited) {
		clear_ext_disp_path_context(&g_context);
		is_context_inited = 1;
		DISPMSG("_get_context set is_context_inited\n");
	}

	return &g_context;
}

EXT_DISP_PATH_MODE get_ext_disp_path_mode(void)
{
	return ext_disp_mode;
}

static void _ext_disp_path_lock(void)
{
	extd_sw_mutex_lock(NULL);	/* /(&(pgc->lock)); */
}

static void _ext_disp_path_unlock(void)
{
	extd_sw_mutex_unlock(NULL);	/* (&(pgc->lock)); */
}

static DISP_MODULE_ENUM _get_dst_module_by_lcm(extd_drv_handle *plcm)
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
		return DISP_MODULE_DPI1;
	}

	DISPERR("can't find ext display path dst module\n");
	return DISP_MODULE_UNKNOWN;
}

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

static int _should_wait_path_idle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
	*** 1.wait idle:	          N         N        Y        Y					*/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 0;

	} else {
		if (ext_disp_is_video_mode())
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		else
			return dpmgr_path_is_busy(pgc->dpmgr_handle);

	}
}

static int _should_update_lcm(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 2.lcm update:          N         Y       N        Y        **/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 0;
		/* TODO: lcm_update can't use cmdq now */
		return 0;

	} else {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 1;

	}
}

static int _should_start_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 3.path start:	idle->Y      Y    idle->Y     Y        ***/

#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	return 1;
#else
	if (ext_disp_is_video_mode())
		return dpmgr_path_is_idle(pgc->dpmgr_handle);
	else
		return 1;

#endif
}

static int _should_trigger_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 4.path trigger:     idle->Y      Y    idle->Y     Y
*** 5.mutex enable:        N         N    idle->Y     Y        ***/

	/* this is not a perfect design, we can't decide path trigger(ovl/rdma/dsi..) separately with mutex enable */
	/* but it's lucky because path trigger and mutex enable is the same w/o cmdq, and it's correct w/ CMDQ(Y+N). */

#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	return 1;
#else
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 0;

	} else {
		if (ext_disp_is_video_mode())
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		else
			return 1;

	}
#endif
}

static int _should_set_cmdq_dirty(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 6.set cmdq dirty:	    N         Y       N        N     ***/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 1;

	} else {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 0;

	}
}

static int _should_flush_cmdq_config_handle(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N        ***/
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 1;
		else
			return 1;

	} else {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 0;

	}
}

static int _should_reset_cmdq_config_handle(void)
{
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 1;
		else
			return 1;

	} else {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 0;

	}
}

static int _should_insert_wait_frame_done_token(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU
*** 7.flush cmdq:          Y         Y       N        N      */
	if (ext_disp_cmdq_enabled()) {
		if (ext_disp_is_video_mode())
			return 1;
		else
			return 1;

	} else {
		if (ext_disp_is_video_mode())
			return 0;
		else
			return 0;

	}
}

static int _should_trigger_interface(void)
{
	if (pgc->mode == EXTD_DECOUPLE_MODE)
		return 0;
	else
		return 1;

}

static int _should_config_ovl_input(void)
{
	/* should extend this when display path dynamic switch is ready */
	if (pgc->mode == EXTD_SINGLE_LAYER_MODE || pgc->mode == EXTD_DEBUG_RDMA_DPI_MODE)
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

#if 0
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

static int _init_vsync_fake_monitor(int fps)
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
#endif

static int config_display_m4u_port(M4U_PORT_ID id, DISP_MODULE_ENUM module)
{
	int ret;
	M4U_PORT_STRUCT sPort;

	sPort.ePortID = id;
	sPort.Virtuality = ext_disp_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret != 0) {
		DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",
			  ddp_get_module_name(module),
			  ext_disp_use_m4u ? "virtual" : "physical", ret);
		return -1;
	}

	return 0;
}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	DISPFUNC();
	pgc->mode = EXTD_DIRECT_LINK_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	/* mark for svp, config in primary_display just once */
	/* config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1); */

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	return ret;
}

static unsigned int extd_display_get_width(void)
{
	return hdmi_get_width();
}

static unsigned int extd_display_get_height(void)
{
	return hdmi_get_height();
}

static unsigned int extd_display_get_bpp(void)
{
	return 24;
}

int _is_hdmi_decouple_mode(EXT_DISP_PATH_MODE mode)
{
	if (mode == EXTD_DECOUPLE_MODE)
		return 1;
	else
		return 0;
}

int ext_disp_is_decouple_mode(void)
{
	return _is_hdmi_decouple_mode(pgc->mode);
}

int ext_disp_get_mutex_id(void)
{
	return dpmgr_path_get_mutex(pgc->dpmgr_handle);
}

static struct disp_internal_buffer_info *allocate_decouple_buffer(unsigned int size)
{
	void *buffer_va = NULL;
	unsigned long buffer_mva = 0;
	unsigned int mva_size = 0;
	struct ion_mm_data mm_data;
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	struct disp_internal_buffer_info *buf_info = NULL;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	if (gDebugSvpHdmiAlwaysSec == 1) {
		DISPMSG("decouple NORMAL buffer : don't allocate, ALWAYS use secure!\n");
		return NULL;
	}
#endif

	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	client = ion_client_create(g_ion_device, "disp_decouple");
	buf_info = kzalloc(sizeof(struct disp_internal_buffer_info), GFP_KERNEL);
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

	return buf_info;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
KREE_SESSION_HANDLE extd_disp_secure_memory_session_handle(void)
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
	ret = KREE_AllocSecurechunkmemWithTag(extd_disp_secure_memory_session_handle(),
				       &mem_handle, 0, buffer_size, "extd_disp");
	if (ret != TZ_RESULT_SUCCESS) {
		DISPERR("KREE_AllocSecurechunkmemWithTag fail, ret = 0x%x\n", ret);
		return -1;
	}
	DISPDBG("KREE_AllocSecurechunkmemWithTag handle = 0x%x\n", mem_handle);

	return mem_handle;
}
#endif

int ext_disp_set_frame_buffer_address(unsigned long va, unsigned long mva)
{

	DISPMSG("extd disp framebuffer va 0x%lx, mva 0x%lx\n", va, mva);
	framebuffer_va = va;
	framebuffer_mva = mva;

	return 0;
}

unsigned long ext_disp_get_frame_buffer_mva_address(void)
{
	return framebuffer_mva;
}

unsigned long ext_disp_get_frame_buffer_va_address(void)
{
	return framebuffer_va;
}

static void init_decouple_buffers(void)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	/* allocate normal buffer */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		decouple_buffer_info[i] = allocate_decouple_buffer(buffer_size);
		if (decouple_buffer_info[i] != NULL) {
			pgc->dc_buf[i] = decouple_buffer_info[i]->mva;
			dc_vAddr[i] = (unsigned long)decouple_buffer_info[i]->va;
			DISPMSG
			    ("decouple NORMAL buffer : pgc->dc_buf[%d] = 0x%x dc_vAddr[%d] = 0x%lx\n",
			     i, pgc->dc_buf[i], i, dc_vAddr[i]);
		}
	}

	/* split 4k normal buffer in non-4k resolution */
	/* eg. if 4k buffer is 0/1, splited into 2 non-4k buffer is 0/1/2/3 */
	/* eg. if 4k buffer is 0/1, splited into 4 non-4k buffer is 0/1/2/3/4/5/6/7 */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		if (decouple_buffer_info[i] != NULL) {
			for (j = 0; j < DISP_SPLIT_COUNT; j++) {
				pgc->dc_split_buf[DISP_SPLIT_COUNT * i + j] =
				    pgc->dc_buf[i] + buffer_size / DISP_SPLIT_COUNT * j;
				DISPMSG
				    ("decouple SPLIT NORMAL buffer : pgc->dc_split_buf[%d] = 0x%x\n",
				     DISP_SPLIT_COUNT * i + j,
				     pgc->dc_split_buf[DISP_SPLIT_COUNT * i + j]);
			}
		}
	}

	/* split normal buffer va for dump */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		if (decouple_buffer_info[i] != NULL) {
			for (j = 0; j < DISP_SPLIT_COUNT; j++) {
				split_dc_vAddr[DISP_SPLIT_COUNT * i + j] =
				    dc_vAddr[i] + buffer_size / DISP_SPLIT_COUNT * j;
				DISPMSG
				    ("decouple SPLIT NORMAL buffer(va) : split_dc_vAddr[%d] = 0x%lx\n",
				     DISP_SPLIT_COUNT * i + j,
				     split_dc_vAddr[DISP_SPLIT_COUNT * i + j]);
			}
		}
	}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* allocate secure buffer */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT; i++) {
		pgc->dc_sec_buf[i] = allocate_decouple_sec_buffer(buffer_size);
		DISPMSG("decouple SECURE buffer : pgc->dc_sec_buf[%d] = 0x%x\n", i,
			pgc->dc_sec_buf[i]);
	}

	/* split 4k secure buffer in non-4k resolution */
	/* eg. if 4k buffer is 0/1, splited 2 non-4k buffer is 0/0/1/1 */
	/* eg. if 4k buffer is 0/1, splited 4 non-4k buffer is 0/0/0/0/1/1/1/1 */
	for (i = 0; i < DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT; i++) {
		pgc->dc_split_sec_buf[i] = pgc->dc_sec_buf[i / DISP_SPLIT_COUNT];
		DISPMSG("decouple SPLIT SECURE buffer : pgc->dc_split_sec_buf[%d] = 0x%x\n", i,
			pgc->dc_split_sec_buf[i]);
	}
#endif
}

static void copy_fbmem_to_dc_buffer(void)
{
	unsigned int line = 0;
	unsigned int w = extd_display_get_width();
	unsigned int h = extd_display_get_height();
	unsigned int primary_w = primary_display_get_width();
	unsigned int primary_h = primary_display_get_height();
	unsigned int bpp = extd_display_get_bpp();
	void *fb_va = (void *)primary_display_get_frame_buffer_va_address();

	DISPMSG("extd w %u h %u bpp %u fb_va 0x%p dc_vAddr[0] 0x%lx\n", w, h, bpp, fb_va,
		dc_vAddr[0]);
	DISPMSG("primary w %u h %u\n", primary_w, primary_h);
	if ((w >= primary_w) && (h >= primary_h)) {
		DISPMSG("memcpy from FBMEM to DC buffer for video mode flash\n");
		for (line = 0; line < primary_h; line++) {
			memcpy((void *)(dc_vAddr[0] + line * w * bpp / 8),
			       fb_va + line * ALIGN_TO(primary_w, MTK_FB_ALIGNMENT) * bpp / 8,
			       w * bpp / 8);
		}
	}
}

static int _build_path_decouple(void)
{
	int ret = 0;
	disp_ddp_path_config *pconfig = NULL;
	DISP_MODULE_ENUM dst_module = 0;
	uint32_t writing_mva = 0;
	unsigned rdma_bpp = 3;
	DpColorFormat rdma_fmt = eRGB888;

	DISPFUNC();
	pgc->mode = EXTD_DECOUPLE_MODE;

	pgc->dpmgr_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP, pgc->cmdq_rdma_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create interface path SUCCESS(0x%lx)\n",
			  (unsigned long)pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
#ifdef CONFIG_USE_CMDQ
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_ENABLE);
#else
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);
#endif

	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	pgc->ovl2mem_path_handle =
	    dpmgr_create_path(DDP_SCENARIO_SUB_OVL_MEMOUT, pgc->cmdq_handle_config);
	if (pgc->ovl2mem_path_handle) {
		DISPCHECK("dpmgr create ovl memout path SUCCESS(0x%lx)\n",
			  (unsigned long)pgc->ovl2mem_path_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
#ifdef CONFIG_USE_CMDQ
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
#else
	dpmgr_path_init(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
#endif

	/* config used module m4u port */
	/* mark for svp, config in primary_display just once */
#if 0
	config_display_m4u_port(M4U_PORT_DISP_OVL1, DISP_MODULE_OVL1);
	config_display_m4u_port(M4U_PORT_DISP_RDMA1, DISP_MODULE_RDMA1);
	config_display_m4u_port(M4U_PORT_DISP_WDMA1, DISP_MODULE_WDMA1);
#endif

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	mutex_lock(&vsync_mtx);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	mutex_unlock(&vsync_mtx);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_COMPLETE);
	dpmgr_enable_event(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_START);

	if (first_build_path_decouple) {
		DISPMSG("first_build_path_decouple just come here once!!!!\n");
		init_decouple_buffers();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (!gDebugSvpHdmiAlwaysSec)
			copy_fbmem_to_dc_buffer();
#else
		copy_fbmem_to_dc_buffer();
#endif
		first_build_path_decouple = 0;
	}

	/* ovl need dst_dirty to set background color */
	pconfig = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	pconfig->dst_w = extd_display_get_width();
	pconfig->dst_h = extd_display_get_height();
	pconfig->dst_dirty = 1;
#ifdef CONFIG_USE_CMDQ
	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, pgc->cmdq_handle_config);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_ENABLE);
#else
	dpmgr_path_reset(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, NULL);
	ret = dpmgr_path_start(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
#endif

	/* config rdma1 to load logo from lk */
	pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	/* need get dpi params, otherwise will don't wait mutex1 eof */
	memcpy(&(pconfig->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));

#if HDMI_SUB_PATH_PROB_V2
	writing_mva = framebuffer_mva;
	rdma_fmt = eBGRA8888;
	rdma_bpp = 4;
#else
	writing_mva = pgc->dc_buf[pgc->dc_buf_id];
#endif

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	if (gDebugSvpHdmiAlwaysSec)
		writing_mva = primary_display_get_frame_buffer_mva_address();
#endif
	pconfig->rdma_config.address = (unsigned int)writing_mva;
	pconfig->rdma_config.width = extd_display_get_width();
	pconfig->rdma_config.height = extd_display_get_height();
	pconfig->rdma_config.inputFormat = rdma_fmt;
	pconfig->rdma_config.pitch = extd_display_get_width() * rdma_bpp;
	pconfig->rdma_dirty = 1;
#ifdef CONFIG_USE_CMDQ
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, pgc->cmdq_handle_config);
#else
	/*ret = dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE); */
	ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, CMDQ_DISABLE);
	/* need start and trigger */
	ret = dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	ret = dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	/* just test show boot logo on tablet */
	/* msleep(3000); */
#endif

	DISPMSG("build_path_decouple dst(%d %d) rdma(%d %d) hdmi(%d %d)\n",
		pconfig->dst_w,
		pconfig->dst_h,
		pconfig->rdma_config.width,
		pconfig->rdma_config.height, extd_display_get_width(), extd_display_get_height());

	DISPCHECK("build decouple path finished\n");
	return ret;
}

static int _build_path_single_layer(void)
{
	return 0;
}

static int _build_path_debug_rdma_dpi(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;

	pgc->mode = EXTD_DEBUG_RDMA_DPI_MODE;

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA2_DISP, pgc->cmdq_handle_config);
	if (pgc->dpmgr_handle) {
		DISPCHECK("dpmgr create path SUCCESS(0x%p)\n", pgc->dpmgr_handle);
	} else {
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}

	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dst_module = DISP_MODULE_DPI0;
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));

	/* config used module m4u port */
	config_display_m4u_port(M4U_PORT_DISP_RDMA2, DISP_MODULE_RDMA2);

	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;

	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
	DISPMSG("ext_disp path trigger thread cmd handle=0x%p\n", pgc->cmdq_handle_trigger);
	cmdqRecReset(pgc->cmdq_handle_trigger);

	if (ext_disp_is_video_mode()) {
		/* wait and clear stream_done, HW will assert mutex enable automatically in frame done reset. */
		/* todo: should let dpmanager to decide wait which mutex's eof. */
		ret =
		    cmdqRecWait(pgc->cmdq_handle_trigger,
				dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				CMDQ_EVENT_MUTEX0_STREAM_EOF);
		/* /dpmgr_path_get_mutex(pgc->dpmgr_handle) */

		/* for some module(like COLOR) to read hw register to GPR after frame done */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_AFTER_STREAM_EOF);
	} else {
		/* DSI command mode doesn't have mutex_stream_eof, need use CMDQ token instead */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		/* ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MDP_DSI0_TE_SOF); */
		/* for operations before frame transfer, such as waiting for DSI TE */
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_BEFORE_STREAM_SOF);

		/* cleat frame done token, now the config thread will not allowed to config registers. */
		/* remember that config thread's priority is higher than trigger thread, so all the config */
		/*queued before will be applied then STREAM_EOF token be cleared */
		/* this is what CMDQ did as "Merge" */
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		/* enable mutex, only cmd mode need this */
		/* this is what CMDQ did as "Trigger" */
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger, CMDQ_ENABLE);


		/* waiting for frame done, because we can't use mutex stream eof here, */
		/*so need to let dpmanager help to decide which event to wait */
		/* most time we wait rdmax frame done event. */
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA1_EOF);
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,
				      CMDQ_WAIT_STREAM_EOF_EVENT);

		/* dsi is not idle rightly after rdma frame done, */
		/*so we need to polling about 1us for dsi returns to idle */
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
	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	DISPCHECK("ext display BUILD cmdq trigger loop finished\n");

}

static void _cmdq_start_trigger_loop(void)
{
	int ret = 0;

	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if (!ext_disp_is_video_mode()) {
		/* need to set STREAM_EOF for the first time, otherwise we will stuck in dead loop */
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		/* /dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW); */
	}

	DISPCHECK("START cmdq trigger loop finished\n");
}

static void _cmdq_stop_trigger_loop(void)
{
	int ret = 0;

	/* this should be called only once because trigger loop will nevet stop */
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);

	DISPCHECK("ext display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if (!ext_disp_is_video_mode()) {
		/* only command mode need to set dirty */
		cmdqRecSetEventToken(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		/* /dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY); */
	}
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	/* /dprec_event_op(DPREC_EVENT_CMDQ_RESET); */
}

static void _cmdq_flush_config_handle(int blocking, void *callback, unsigned int userdata)
{
	if (blocking)
		cmdqRecFlush(pgc->cmdq_handle_config);	/* it will be blocked until mutex done */
	else {
		if (callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config, callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}
	/* dprec_event_op(DPREC_EVENT_CMDQ_FLUSH); */
}

static void _cmdq_insert_wait_frame_done_token(void)
{
	if (ext_disp_is_video_mode()) {
		cmdqRecWaitNoClear(pgc->cmdq_handle_config,
				   dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				   CMDQ_EVENT_MUTEX0_STREAM_EOF);
		/* /CMDQ_EVENT_MUTEX1_STREAM_EOF  dpmgr_path_get_mutex() */
	} else {
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}

	/* /dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF); */
}

static void _cmdq_insert_wait_frame_done_token_mira(void *handle)
{
	/* pgc->dpmgr_handle use DDP_SCENARIO_SUB_RDMA1_DISP scenario */
	if (ext_disp_is_video_mode())
		cmdqRecWaitNoClear(handle,
				   dpmgr_path_get_mutex(pgc->dpmgr_handle) +
				   CMDQ_EVENT_MUTEX0_STREAM_EOF);
	else
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
}

static int _convert_disp_input_to_rdma(RDMA_CONFIG_STRUCT *dst, ext_disp_input_config *src)
{
	if (src && dst) {
		dst->inputFormat = src->fmt;
		dst->address = src->addr;
		dst->width = src->src_w;
		dst->height = src->src_h;
		dst->pitch = src->src_pitch;
		dst->buf_offset = 0;
		if (hdmi_is_interlace) {
			dst->height /= 2;
			dst->pitch *= 2;
		}
		return 0;
	}

	DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
	return -1;
}

static int _convert_disp_input_to_ovl(OVL_CONFIG_STRUCT *dst, ext_disp_input_config *src)
{
	if (src && dst) {
		dst->layer = src->layer;
		dst->layer_en = src->layer_en;
		dst->fmt = src->fmt;
		dst->addr = src->addr;
		dst->vaddr = src->vaddr;
		dst->src_x = src->src_x;
		dst->src_ori_x = src->src_x;
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
		/*if (dst->layer == 0 && gDebugSvp == 2) */
		/*dst->security = DISP_SECURE_BUFFER; */
#endif

		dst->yuv_range = src->yuv_range;

		if (hdmi_is_interlace && !_is_hdmi_decouple_mode(pgc->mode)) {
			dst->dst_h /= 2;
			dst->src_pitch *= 2;
			dst->dst_y /= 2;
		}

		return 0;
	}

	DISPERR("src(0x%p) or dst(0x%p) is null\n", src, dst);
	return -1;
}

static int _extd_cmdq_finish_callback(uint32_t userdata)
{
	int ret = 0;

	if (ext_disp_is_video_mode() == 1
	    && ext_disp_is_decouple_mode() == 0) {
		unsigned int ovl_status[2];

		cmdqBackupReadSlot(pgc->ovl_status_info, 0, &ovl_status[0]);
		cmdqBackupReadSlot(pgc->ovl_status_info, 1, &ovl_status[1]);

		if ((ovl_status[0] & 0x1) != 0) {
			/* ovl is not idle !! */
			DISPERR
			    ("extd ovl status(0x%x)(0x%x) error!config maybe not finish during blanking\n",
			     ovl_status[0], ovl_status[1]);
		#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "EXTD", "gce late");
		#endif
			ret = -1;
		}
	}

	return ret;
}

static int _trigger_display_interface(int blocking, void *callback, unsigned int userdata)
{
	/* /DISPFUNC(); */
	/* int i = 0; */

	bool reg_flush = false;

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);


	if (_should_update_lcm()) {
		extd_drv_update(pgc->plcm, 0, 0, pgc->plcm->params->width,
				pgc->plcm->params->height, 0);
	}

	if (_should_start_path()) {
		reg_flush = true;
		dpmgr_path_start(pgc->dpmgr_handle, ext_disp_cmdq_enabled());
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagPulse, Trigger, 1);
	}

	if (_should_trigger_path()) {
		/* trigger_loop_handle is used only for build trigger loop,*/
		/* which should always be NULL for config thread */
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, ext_disp_cmdq_enabled());
	}

	if (_should_set_cmdq_dirty())
		_cmdq_set_config_handle_dirty();

	/* /if(reg_flush == false) */
	{
#if 0
		if (reg_flush == false) {
			if (_should_insert_wait_frame_done_token())
				_cmdq_insert_wait_frame_done_token();
		}

		if (_should_flush_cmdq_config_handle())
			_cmdq_flush_config_handle(reg_flush);

		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		if (reg_flush == true) {
			if (_should_insert_wait_frame_done_token())
				_cmdq_insert_wait_frame_done_token();
		}
		/* /cmdqRecDumpCommand(cmdqRecHandle handle) */
#else

		if (_should_flush_cmdq_config_handle()) {
			if (reg_flush) {
				MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagPulse,
					       Trigger, 2);
			}
			_cmdq_flush_config_handle(blocking, callback, userdata);

			/* /if(reg_flush) */
		}

		if (_should_reset_cmdq_config_handle())
			_cmdq_reset_config_handle();

		if (_should_insert_wait_frame_done_token() && (!_is_hdmi_decouple_mode(pgc->mode)))
			_cmdq_insert_wait_frame_done_token();
#endif
	}

	return 0;
}

static void _trigger_ovl_to_memory(disp_path_handle disp_handle,
				   cmdqRecHandle cmdq_handle, void *callback, unsigned int data)
{
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, cmdq_handle, CMDQ_ENABLE);
	cmdqRecWait(cmdq_handle, CMDQ_EVENT_DISP_WDMA1_EOF);
	cmdqRecFlushAsyncCallback(cmdq_handle, (CmdqAsyncFlushCB) callback, data);
	cmdqRecReset(cmdq_handle);
}

#if 0
static int _trigger_overlay_engine(void)
{
	/* maybe we need a simple merge mechanism for CPU config. */
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL, ext_disp_use_cmdq);
	return 0;
}
#endif

#ifdef HDMI_SUB_PATH
static unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
	return 0;
}

static unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
	return 0;
}

static unsigned int cmdqDdpDumpInfo(uint64_t engineFlag, char *pOutBuf, unsigned int bufSize)
{
	DISPERR("extd cmdq timeout:%llu\n", engineFlag);
	ext_disp_diagnose();
	return 0;
}

static unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
	return 0;
}
#endif


#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
static struct task_struct *ext_disp_present_fence_release_worker_task;
wait_queue_head_t ext_disp_irq_wq;
atomic_t ext_disp_irq_event = ATOMIC_INIT(0);

static void ext_disp_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	/* RET_VOID_IF_NOLOG(!is_hdmi_active()); */

	if (module == DISP_MODULE_RDMA1) {
		if (param & 0x4) {	/* frame done */
			/* /MMProfileLogEx(ddp_mmp_get_events()->Extd_IrqStatus, MMProfileFlagPulse, module, param); */

			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* In TEE, we have to protect WDMA registers, so we can't enable WDMA interrupt */
	/* here we use ovl frame done interrupt instead */
	if ((module == DISP_MODULE_OVL1) && ext_disp_is_decouple_mode()) {
		/* OVL1 frame done */
		if (param & 0x2) {
			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#else
	if ((module == DISP_MODULE_WDMA1) && ext_disp_is_decouple_mode()) {
		/* wdma1 frame done */
		if (param & 0x1) {
			atomic_set(&ext_disp_irq_event, 1);
			wake_up_interruptible(&ext_disp_irq_wq);
		}
	}
#endif

}

void ext_disp_update_present_fence(unsigned int fence_idx)
{
	g_ext_PresentFenceIndex = fence_idx;
}

static int ext_disp_present_fence_release_worker_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 87 }; /* RTPM_PRIO_FB_THREAD */

	sched_setscheduler(current, SCHED_RR, &param);

#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
#else
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
#endif

	while (1) {
#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
		wait_event_interruptible(ext_disp_irq_wq, atomic_read(&ext_disp_irq_event));
		atomic_set(&ext_disp_irq_event, 0);
#else
		/* ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC); */
		ret =
		    dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ / 25);
#endif

		/*  release present fence in vsync callback */
		{
			int fence_increment = 0;
			disp_sync_info *layer_info =
			    _get_sync_info(MAKE_DISP_SESSION(DISP_SESSION_EXTERNAL, 1),
					   disp_sync_get_present_timeline_id());
			if (layer_info == NULL) {
				DISPERR("_get_sync_info fail in present_fence_release thread\n");
				continue;
			}

			_ext_disp_path_lock();
			fence_increment = g_ext_PresentFenceIndex - layer_info->timeline->value;
			if (fence_increment > 0) {
				timeline_inc(layer_info->timeline, fence_increment);
				MMProfileLogEx(ddp_mmp_get_events()->Extd_release_present_fence,
					       MMProfileFlagPulse, g_ext_PresentFenceIndex,
					       fence_increment);

				DISP_PRINTF(DDP_FENCE1_LOG,
					    " release_present_fence idx %d timeline->value %d\n",
					    g_ext_PresentFenceIndex, layer_info->timeline->value);
			}


			_ext_disp_path_unlock();
		}
	}
	return 0;
}

#endif

struct task_struct *hdmi_config_rdma_task = NULL;
wait_queue_head_t hdmi_config_rdma_wq;
atomic_t hdmi_config_rdma_event = ATOMIC_INIT(0);

static void _hdmi_config_rdma_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (!is_hdmi_active()) {
		DISPMSG("hdmi is not plugin, exit _hdmi_config_rdma_irq_handler\n");
		return;
	}

	if (module == DISP_MODULE_RDMA1) {
		if (param & 0x2) {	/* frame start */
			atomic_set(&hdmi_config_rdma_event, 1);
			wake_up_interruptible(&hdmi_config_rdma_wq);
		}
	}
}

static int hdmi_config_rdma_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 }; /* RTPM_PRIO_SCRN_UPDATE */

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_event_interruptible(hdmi_config_rdma_wq, atomic_read(&hdmi_config_rdma_event));
		atomic_set(&hdmi_config_rdma_event, 0);

		/* hdmi config rdma indepenent, instead of config wdma and rdma in the same cmdq task */
		hdmi_config_rdma();

		if (kthread_should_stop())
			break;
	}

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

static void ovl_wdma_callback(void)
{
	if (!ext_disp_use_frc) {
		DISPMSG("ext_disp_use_frc close\n");
		hdmi_config_rdma();
	}
}

static int video_fps;

void hdmi_set_video_fps(int fps)
{
	video_fps = fps;
	HDMI_FRC_LOG("[FRC] : video_fps = %d\n", video_fps);
}

static enum FRC_TYPE get_frc_type(void)
{
	enum FRC_TYPE frc_type = FRC_UNSOPPORT_MODE;
	int tv_fps = hdmi_get_tv_fps();

	if (!video_fps && !tv_fps)
		frc_type = FRC_1_TO_1_MODE;
	else if ((video_fps * 2) == tv_fps)
		frc_type = FRC_1_TO_2_MODE;	/* 25->50 / 30->60 */
	else if (24 == video_fps && 60 == tv_fps)
		frc_type = FRC_2_TO_5_MODE;	/* 24->60 */
	else
		frc_type = FRC_1_TO_1_MODE;	/* other */

	DISPDBG("[FRC] : frc_type = %d tv_fps = %d video_fps = %d\n", frc_type, tv_fps, video_fps);

	return frc_type;
}

static int get_rdma_cur_buf_id(void)
{
	int index = 0;
	int cur_buf_id;

	/* get rdma buf id */
	index = pgc->dc_rdma_buf_id;
	index++;
	if (hdmi_res_is_4k)
		index %= DISP_INTERNAL_BUFFER_COUNT;
	else
		index %= (DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT);

	/* compare rdma buf id with wdma, get rdma cur buf id */
	if (pgc->dc_rdma_buf_id == pgc->dc_buf_id) {
		cur_buf_id = pgc->dc_rdma_buf_id;
	} else {
		cur_buf_id = index;
		pgc->dc_rdma_buf_id = index;
	}

	DISPDBG("get_config_rdma_cur_buf_id : cur_buf_id = %d\n", cur_buf_id);

	return cur_buf_id;
}

static int get_frc_rdma_cur_buf_id(void)
{
	static int count_1_to_2_mode;
	static int count_2_to_5_mode;
	static int last_frc_type = FRC_UNSOPPORT_MODE;
	static int cur_frc_type = FRC_UNSOPPORT_MODE;
	static int cur_buf_id;

	if (!ext_disp_use_frc) {
		cur_buf_id = pgc->dc_buf_id;
		return cur_buf_id;
	}

	last_frc_type = cur_frc_type;
	cur_frc_type = get_frc_type();
	if (cur_frc_type != last_frc_type) {
		DISPMSG("[FRC] : switch from [%d] to [%d]\n", last_frc_type, cur_frc_type);
		count_1_to_2_mode = 0;
		count_2_to_5_mode = 0;
	}

	if (cur_frc_type == FRC_1_TO_2_MODE) {
		count_1_to_2_mode %= 2;
		if (0 == count_1_to_2_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		count_1_to_2_mode++;
	} else if (cur_frc_type == FRC_2_TO_5_MODE) {
		count_2_to_5_mode %= 5;
		if (0 == count_2_to_5_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		if (2 == count_2_to_5_mode)
			cur_buf_id = get_rdma_cur_buf_id();
		count_2_to_5_mode++;
	} else {
		cur_buf_id = get_rdma_cur_buf_id();
	}

	/* check if FRC_1_TO_2_MODE/FRC_2_TO_5_MODE work properly by log
	 * FRC_1_TO_2_MODE id : 0 0 1 1 2 2 3 3 ...
	 * FRC_2_TO_5_MODE id : 0 0 1 1 1 2 2 3 3 3 ...
	 */
	if (cur_frc_type != FRC_1_TO_1_MODE && video_fps)
		HDMI_FRC_LOG("[FRC] : RDMA cur_buf_id = %d\n", cur_buf_id);

	return cur_buf_id;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static uint32_t get_rdma_secure_buffer_handle(void)
{
	uint32_t writing_mva = 0;

	int cur_buf_id;

	cur_buf_id = get_frc_rdma_cur_buf_id();
	/* for calculate rdma secure buffer offset */
	pgc->cur_rdma_sec_buf_id = cur_buf_id;

	/* get rdma secure buffer handle according to cur buf id */
	if (hdmi_res_is_4k)
		writing_mva = pgc->dc_sec_buf[cur_buf_id];
	else
		writing_mva = pgc->dc_split_sec_buf[cur_buf_id];
	return writing_mva;
}

static unsigned int get_rdma_secure_buffer_offset(void)
{
	unsigned int offset = 0;
	unsigned int offset_index = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	if (hdmi_res_is_4k) {
		offset = 0;
	} else {
		offset_index = pgc->cur_rdma_sec_buf_id % DISP_SPLIT_COUNT;
		offset = (buffer_size / DISP_SPLIT_COUNT) * offset_index;
	}

	DISPDBG("%s : rdma secure buffer offset = %d\n", __func__, offset);
	return offset;
}
#endif

static unsigned int get_input_bpp(DpColorFormat fmt)
{
	int bpp = 4;

	switch (fmt) {
	case eBGR565:
	case eRGB565:
	case eVYUY:
	case eYVYU:
	case eUYVY:
	case eYUY2:
		bpp = 2;
		break;
	case eRGB888:
	case eBGR888:
		bpp = 3;
		break;
	case eRGBA8888:
	case eBGRA8888:
	case eARGB8888:
	case eABGR8888:
		bpp = 4;
		break;
	default:
		break;
	}
	return bpp;
}

static int ext_disp_trigger_for_interlace(void)
{
	int i = 0;
	int ret = 0;
	unsigned int bpp;
	disp_ddp_path_config *data_config;

	if (!hdmi_is_interlace || _is_hdmi_decouple_mode(pgc->mode))
		return 0;

	if ((is_hdmi_active() == false) || (pgc->state != EXTD_INIT && pgc->state != EXTD_RESUME))
		return -2;

	_ext_disp_path_lock();

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	if (_should_config_ovl_input()) {
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			data_config->ovl_config[i].src_x = data_config->ovl_config[i].src_ori_x;
			bpp = get_input_bpp(data_config->ovl_config[i].fmt)*2;
			if (data_config->ovl_config[i].layer_en && DPI0_IS_TOP_FIELD())
				data_config->ovl_config[i].src_x += data_config->ovl_config[i].src_pitch/bpp;
		}
		data_config->ovl_dirty = 1;
	} else {
	#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		if (data_config->rdma_config.security == DISP_SECURE_BUFFER)
			data_config->rdma_config.buf_offset = get_rdma_secure_buffer_offset();
	#endif
		bpp = get_input_bpp(data_config->rdma_config.inputFormat);
		if (DPI0_IS_TOP_FIELD())
				data_config->rdma_config.buf_offset += extd_display_get_width() * bpp;

		data_config->rdma_dirty = 1;
	}
	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	if (_should_trigger_interface()) {
		_trigger_display_interface(0, NULL, 0);
	} else {
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
				       pgc->cmdq_handle_config, ovl_wdma_callback, 0);
	}

	_ext_disp_path_unlock();

	return ret;
}

static void ext_disp_frame_update_irq_callback(DISP_MODULE_ENUM module, unsigned int param)
{
	if (module == DISP_MODULE_RDMA1) {

		if ((param & 0x2) && hdmi_is_interlace) {
			/* rdma0 frame start */
			atomic_set(&ext_frame_update_event, 1);
			wake_up_interruptible(&ext_frame_update_wq);
		}
	}
}

static int ext_disp_frame_update_kthread(void *data)
{

	struct sched_param param = {.sched_priority = 94 }; /* RTPM_PRIO_SCRN_UPDATE */

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(ext_frame_update_wq,
					 atomic_read(&ext_frame_update_event));
		atomic_set(&ext_frame_update_event, 0);

		if (hdmi_is_interlace)
			ext_disp_trigger_for_interlace();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int ext_disp_init(struct platform_device *dev, char *lcm_name, unsigned int session)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;
	/* DISP_MODULE_ENUM dst_module = 0; */

	LCM_PARAMS *lcm_param = NULL;
	/* LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED; */
	disp_ddp_path_config *data_config = NULL;

	DISPFUNC();
	dpmgr_init();

	init_cmdq_slots(&(pgc->cur_config_fence), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->subtractor_when_free), DISP_SESSION_TIMELINE_COUNT, 0);
	init_cmdq_slots(&(pgc->ovl_status_info), 2, 0);

	mutex_init(&vsync_mtx);
	extd_mutex_init(&(pgc->lock));
	_ext_disp_path_lock();

	pgc->state = EXTD_INIT;
	pgc->plcm = extd_drv_probe(lcm_name, LCM_INTERFACE_NOTDEFINED);
	if (pgc->plcm == NULL) {
		DISPCHECK("disp_lcm_probe returns null\n");
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("disp_lcm_probe SUCCESS\n");
	}


	lcm_param = extd_drv_get_params(pgc->plcm);

	if (lcm_param == NULL) {
		DISPERR("get lcm params FAILED\n");
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}
#ifdef HDMI_SUB_PATH
	ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,
				 (CmdqClockOnCB) cmdqDdpClockOn, (CmdqDumpInfoCB) cmdqDdpDumpInfo,
				 (CmdqResetEngCB) cmdqDdpResetEng,
				 (CmdqClockOffCB) cmdqDdpClockOff);
#endif

	if (ret) {
		DISPERR("cmdqCoreRegisterCB failed, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	}

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else {
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_handle=0x%p\n", pgc->cmdq_handle_config);
	}

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_rdma_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_rdma_handle=0x%p\n",
			  pgc->cmdq_rdma_handle_config);

	ret = cmdqRecCreate(CMDQ_SCENARIO_MHL_DISP, &(pgc->cmdq_rdma_handle_config));
	if (ret) {
		DISPCHECK("cmdqRecCreate FAIL, ret=%d\n", ret);
		ret = EXT_DISP_STATUS_ERROR;
		goto done;
	} else
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_rdma_handle=0x%p\n",
			  pgc->cmdq_rdma_handle_config);

	if (ext_disp_mode == EXTD_DIRECT_LINK_MODE) {
		_build_path_direct_link();

		DISPCHECK("ext_disp display is DIRECT LINK MODE\n");
	} else if (ext_disp_mode == EXTD_DECOUPLE_MODE) {
		_build_path_decouple();

		DISPCHECK("ext_disp display is DECOUPLE MODE\n");
	} else if (ext_disp_mode == EXTD_SINGLE_LAYER_MODE) {
		_build_path_single_layer();

		DISPCHECK("ext_disp display is SINGLE LAYER MODE\n");
	} else if (ext_disp_mode == EXTD_DEBUG_RDMA_DPI_MODE) {
		_build_path_debug_rdma_dpi();

		DISPCHECK("ext_disp display is DEBUG RDMA to dpi MODE\n");
	} else {
		DISPCHECK("ext_disp display mode is WRONG\n");
	}

	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	pgc->session = session;

	DISPCHECK("ext_disp display START cmdq trigger loop finished\n");

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, ext_disp_is_video_mode());

	if (ext_frame_update_task == NULL) {
		init_waitqueue_head(&ext_frame_update_wq);
		disp_register_module_irq_callback(DISP_MODULE_RDMA1,
						  ext_disp_frame_update_irq_callback);

		ext_frame_update_task =
		    kthread_create(ext_disp_frame_update_kthread, NULL,
				   "ext_frame_update_worker");
		wake_up_process(ext_frame_update_task);
	}

	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);

	if (ext_disp_use_cmdq == CMDQ_ENABLE) {
		_cmdq_build_trigger_loop();

		DISPCHECK("ext_disp display BUILD cmdq trigger loop finished\n");

		_cmdq_start_trigger_loop();
	}

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));

	data_config->dst_w = lcm_param->width;
	data_config->dst_h = lcm_param->height;
	if (hdmi_is_interlace && !_is_hdmi_decouple_mode(pgc->mode))
		data_config->dst_h /= 2;
	data_config->dst_dirty = 1;

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, CMDQ_DISABLE);

	if (!extd_drv_is_inited(pgc->plcm))
		ret = extd_drv_init(dev, pgc->plcm);

	/* this will be set to always enable cmdq later */
	if (ext_disp_is_video_mode()) {
		/* /ext_disp_use_cmdq = CMDQ_ENABLE; */
		if (ext_disp_mode == EXTD_DEBUG_RDMA_DPI_MODE)
			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_RDMA2_DONE);
		else
			dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC,
					       DDP_IRQ_RDMA1_DONE);
	}

	if (ext_disp_use_cmdq == CMDQ_ENABLE && (!_is_hdmi_decouple_mode(pgc->mode))) {
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}

	pgc->state = EXTD_RESUME;

done:

	/* /dpmgr_check_status(pgc->dpmgr_handle); */

	_ext_disp_path_unlock();

#if HDMI_SUB_PATH_BOOT || HDMI_SUB_PATH_PROB_V2
#else
	/* dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE); */
#endif


#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
	if ((ext_disp_present_fence_release_worker_task == NULL)
	    && !boot_up_with_facotry_mode()) {

#if RELEASE_PRESENT_FENCE_WITH_IRQ_HANDLE
		init_waitqueue_head(&ext_disp_irq_wq);

		disp_register_module_irq_callback(DISP_MODULE_RDMA1, ext_disp_irq_handler);
#endif

		ext_disp_present_fence_release_worker_task =
		    kthread_create(ext_disp_present_fence_release_worker_kthread,
				   NULL, "ext_disp_present_fence_worker");
		wake_up_process(ext_disp_present_fence_release_worker_task);

		DISPMSG("wake_up ext_disp_present_fence_worker\n");

	}
#endif

	if (!hdmi_config_rdma_task && !boot_up_with_facotry_mode() && ext_disp_use_frc
	    && ext_disp_mode != EXTD_DIRECT_LINK_MODE) {
		init_waitqueue_head(&hdmi_config_rdma_wq);

		disp_register_module_irq_callback(DISP_MODULE_RDMA1, _hdmi_config_rdma_irq_handler);

		hdmi_config_rdma_task =
		    kthread_create(hdmi_config_rdma_kthread, NULL, "hdmi_config_rdma_kthread");
		wake_up_process(hdmi_config_rdma_task);
	}

	DISPMSG("ext_disp_init done\n");
	return ret;
}


int ext_disp_deinit(char *lcm_name)
{
	int sleep_cnt = 0;

	DISPFUNC();

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT)
		goto deinit_exit;

#if 0
	/* wait frame done before destroy path */
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);
#endif

	/* check ovl1 if idle to avoid GCE can not wait WDMA1 EOF */
	do {
		sleep_cnt++;
		if (sleep_cnt > 50) {
			DISPMSG("check ovl1 idle timeout(50ms), can stop display hw!\n");
			break;
		}
		udelay(1000);
		DISPMSG("check ovl1 idle, sleep_cnt = %d\n", sleep_cnt);
	} while ((DISP_REG_GET(DISP_OVL_INDEX_OFFSET + DISP_REG_OVL_STA) & 0x1));

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	mutex_lock(&vsync_mtx);
	if (ext_disp_mode == EXTD_DECOUPLE_MODE)
		dpmgr_disable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_destroy_path(pgc->dpmgr_handle);
	mutex_unlock(&vsync_mtx);
	/* destroy path and release mutex, avoid acquire mutex from 0 again */
	if (_is_hdmi_decouple_mode(pgc->mode)) {
		dpmgr_path_stop(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_path_reset(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_path_deinit(pgc->ovl2mem_path_handle, CMDQ_DISABLE);
		dpmgr_destroy_path(pgc->ovl2mem_path_handle);
	}

	cmdqRecDestroy(pgc->cmdq_handle_config);
	cmdqRecDestroy(pgc->cmdq_handle_trigger);

	pgc->state = EXTD_DEINIT;


deinit_exit:
	_ext_disp_path_unlock();
	is_context_inited = 0;
	DISPMSG("ext_disp_deinit done\n");
	return 0;
}

/* register rdma done event */
int ext_disp_wait_for_idle(void)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	DISPFUNC();

	_ext_disp_path_lock();

/* done: */
	_ext_disp_path_unlock();
	return ret;
}

int ext_disp_wait_for_dump(void)
{
	return 0;
}

int ext_disp_wait_for_vsync(void *config)
{
	disp_session_vsync_config *c = (disp_session_vsync_config *) config;
	int ret = 0;

	if (pgc->state == EXTD_DEINIT) {
		DISPDBG("ext_disp path destroy, should not wait vsync\n");
		return -1;
	}

	if (pgc->dpmgr_handle == NULL) {
		DISP_PRINTF(DDP_VYSNC_LOG, "vsync for ext display path not ready yet(1)\n");
		return -1;
	}

	mutex_lock(&vsync_mtx);
	if (pgc->dpmgr_handle == NULL) {
		DISP_PRINTF(DDP_VYSNC_LOG, "vsync for ext display path not ready yet(1)\n");
		mutex_unlock(&vsync_mtx);
		return -1;
	}
	ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ / 10);
	/* /dprec_logger_trigger(DPREC_LOGGER_VSYNC); */
	if (ret == -2) {
		DISPCHECK("vsync for ext display path not enabled yet(2)\n");
		mutex_unlock(&vsync_mtx);
		return -1;
	}
	mutex_unlock(&vsync_mtx);
	/* DISPMSG("vsync signaled\n"); */
	c->vsync_ts = get_current_time_us();
	c->vsync_cnt++;

	return ret;
}

int ext_disp_suspend(void)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	DISPFUNC();

	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT || pgc->state == EXTD_SUSPEND) {
		DISPERR("EXTD_DEINIT or EXTD_SUSPEND\n");
		goto done;
	}

	pgc->need_trigger_overlay = 0;
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_stop_trigger_loop();
	/* temp solution for fix m4u tanslation fault */

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 30);
	/* /if(dpmgr_path_is_busy(pgc->dpmgr_handle)) */
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

#if 0				/* /(dpmgr_path_is_busy2(pgc->dpmgr_handle)) */
	{
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		dpmgr_check_status(pgc->dpmgr_handle);
	}
#endif

	extd_drv_suspend(pgc->plcm);
	/* /dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE); */

	pgc->state = EXTD_SUSPEND;


done:
	_ext_disp_path_unlock();

	DISPMSG("ext_disp_suspend done\n");
	return ret;
}

int ext_disp_resume(void)
{
	EXT_DISP_STATUS ret = EXT_DISP_STATUS_OK;

	_ext_disp_path_lock();

	if (pgc->state < EXTD_INIT) {
		DISPERR("EXTD_DEINIT\n");
		goto done;
	}

	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);

	extd_drv_resume(pgc->plcm);

	/* /dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE); */

	if (ext_disp_use_cmdq == CMDQ_ENABLE)
		_cmdq_start_trigger_loop();


	if (dpmgr_path_is_busy(pgc->dpmgr_handle)) {
		DISPCHECK("stop display path failed, still busy\n");
		ret = -1;
		/* FIXME : bypass dpmgr_path_is_busy temp */
		/* goto done; */
	}

	pgc->state = EXTD_RESUME;

done:
	_ext_disp_path_unlock();
	DISPMSG("ext_disp_resume done\n");
	return ret;
}

static uint32_t get_wdma_normal_buffer_mva(void)
{
	uint32_t writing_mva;
	int index = 0;

	index = pgc->dc_buf_id;
	index++;
	if (hdmi_res_is_4k) {
		index %= DISP_INTERNAL_BUFFER_COUNT;
		writing_mva = pgc->dc_buf[index];
	} else {
		index %= (DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT);
		writing_mva = pgc->dc_split_buf[index];
	}
	pgc->dc_buf_id = index;

	DISPDBG("%s : pgc->dc_buf_id = %d\n", __func__, pgc->dc_buf_id);

	return writing_mva;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
static uint32_t get_wdma_secure_buffer_handle(void)
{
	uint32_t writing_mva = 0;

	int index = 0;

	index = pgc->dc_buf_id;
	index++;
	if (hdmi_res_is_4k) {
		index %= DISP_INTERNAL_BUFFER_COUNT;
		writing_mva = pgc->dc_sec_buf[index];
	} else {
		index %= (DISP_INTERNAL_BUFFER_COUNT * DISP_SPLIT_COUNT);
		writing_mva = pgc->dc_split_sec_buf[index];
		/* for calculate wdma secure buffer offset */
		pgc->cur_wdma_sec_buf_id = index;
	}
	pgc->dc_buf_id = index;
	return writing_mva;
}

static unsigned int get_wdma_secure_buffer_offset(void)
{
	unsigned int offset = 0;
	unsigned int offset_index = 0;
	unsigned int height = HDMI_MAX_HEIGHT;
	unsigned int width = HDMI_MAX_WIDTH;
	unsigned int bpp = extd_display_get_bpp();
	unsigned int buffer_size = width * height * bpp / 8;

	if (hdmi_res_is_4k) {
		offset = 0;
	} else {
		offset_index = pgc->cur_wdma_sec_buf_id % DISP_SPLIT_COUNT;
		offset = (buffer_size / DISP_SPLIT_COUNT) * offset_index;
	}

	DISPDBG("%s : wdma secure buffer offset = %d\n", __func__, offset);
	return offset;
}
#endif

static int extd_disp_config_output(void)
{
	int ret = 0;
	disp_ddp_path_config *pconfig = NULL;
	uint32_t writing_mva = 0;
	void *cmdq_handle = NULL;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	static int last_wdma_security;
	static int cur_wdma_security;
#endif

	_ext_disp_path_lock();

	/* config ovl1->wdma1 */
	cmdq_handle = pgc->cmdq_handle_config;
	pconfig = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
	{
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* 0 : nonsec/sec(default) 1 : always sec 2 : always normal, for svp debug */
		if (gDebugSvpHdmiAlwaysSec == 1)
			g_wdma_rdma_security = DISP_SECURE_BUFFER;
		else if (gDebugSvpHdmiAlwaysSec == 2)
			g_wdma_rdma_security = DISP_NORMAL_BUFFER;

		/* wdma switch between nonsec and sec */
		last_wdma_security = cur_wdma_security;
		cur_wdma_security = g_wdma_rdma_security;
		if (cur_wdma_security != last_wdma_security) {
			DISPMSG("[SVP] : wdma switch from [%d] to [%d], index = %d\n",
				last_wdma_security, cur_wdma_security, pgc->dc_buf_id);
			pgc->dc_buf_id = 0;
		}

		pconfig->wdma_config.security = g_wdma_rdma_security;
		if (pconfig->wdma_config.security == DISP_SECURE_BUFFER) {
			/* get secure buffer handle and offset */
			writing_mva = get_wdma_secure_buffer_handle();
			pconfig->wdma_config.split_buf_offset = get_wdma_secure_buffer_offset();
			g_rdma_security = DISP_SECURE_BUFFER;
		} else {
			writing_mva = get_wdma_normal_buffer_mva();
			g_rdma_security = DISP_NORMAL_BUFFER;
		}
#else
		writing_mva = get_wdma_normal_buffer_mva();
#endif

		if (writing_mva)
			pconfig->wdma_config.dstAddress = writing_mva;
		else
			DISPERR("wdma input address is null!!\n");
		pconfig->wdma_config.srcHeight = extd_display_get_height();
		pconfig->wdma_config.srcWidth = extd_display_get_width();
		pconfig->wdma_config.clipX = 0;
		pconfig->wdma_config.clipY = 0;
		pconfig->wdma_config.clipHeight = extd_display_get_height();
		pconfig->wdma_config.clipWidth = extd_display_get_width();
		pconfig->wdma_config.outputFormat = eRGB888;
		pconfig->wdma_config.useSpecifiedAlpha = 1;
		pconfig->wdma_config.alpha = 0xFF;
		pconfig->wdma_config.dstPitch =
		    extd_display_get_width() * DP_COLOR_BITS_PER_PIXEL(eRGB888) / 8;
	}
	pconfig->wdma_dirty = 1;

	if ((pconfig->wdma_config.srcWidth != pconfig->dst_w)
	    || (pconfig->wdma_config.srcHeight != pconfig->dst_h)
	    ) {
		DISPMSG("============ ovl_roi(%d %d) != wdma(%d %d) ============\n",
			pconfig->dst_w,
			pconfig->dst_h,
			pconfig->wdma_config.srcWidth, pconfig->wdma_config.srcHeight);

		pconfig->wdma_config.srcWidth = pconfig->dst_w;
		pconfig->wdma_config.srcHeight = pconfig->dst_h;

		pconfig->wdma_config.clipWidth = pconfig->dst_w;
		pconfig->wdma_config.clipHeight = pconfig->dst_h;


	}

	DISP_PRINTF(DDP_RESOLUTION_LOG, "config_wdma w %d, h %d hdmi(%d %d)\n",
		    pconfig->wdma_config.srcWidth,
		    pconfig->wdma_config.srcHeight,
		    extd_display_get_width(), extd_display_get_height());

	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, cmdq_handle);

	_ext_disp_path_unlock();

	return ret;
}

static uint32_t get_rdma_normal_buffer_mva(void)
{
	uint32_t writing_mva;
	int cur_buf_id;

	cur_buf_id = get_frc_rdma_cur_buf_id();

	/* get rdma normal buffer mva according to cur buf id */
	if (hdmi_res_is_4k)
		writing_mva = pgc->dc_buf[cur_buf_id];
	else
		writing_mva = pgc->dc_split_buf[cur_buf_id];

	return writing_mva;
}

void hdmi_config_rdma(void)
{
	disp_ddp_path_config *pconfig = NULL;
	uint32_t writing_mva = 0;
	cmdqRecHandle cmdq_handle;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	static int last_rdma_security;
	static int cur_rdma_security;
#endif

	/* DISPFUNC(); */
	_ext_disp_path_lock();

	if (pgc->state == EXTD_DEINIT) {
		DISPMSG("EXTD_DEINIT : exit hdmi_config_rdma!\n");
		goto done;
	}

	{
		cmdq_handle = pgc->cmdq_rdma_handle_config;
		if (cmdq_handle == NULL)
			DISPERR("hdmi_config_rdma : cmdq_handle is NULL!\n");
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);

		pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		pconfig->rdma_config.buf_offset = 0;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
		/* 0 : nonsec/sec(default) 1 : always sec 2 : always normal, for svp debug */
		if (gDebugSvpHdmiAlwaysSec == 1)
			g_rdma_security = DISP_SECURE_BUFFER;
		else if (gDebugSvpHdmiAlwaysSec == 2)
			g_rdma_security = DISP_NORMAL_BUFFER;

		/* rdma switch between nonsec and sec */
		last_rdma_security = cur_rdma_security;
		cur_rdma_security = g_rdma_security;
		if (cur_rdma_security != last_rdma_security) {
			DISPMSG("[SVP] : rdma switch from [%d] to [%d], index = %d\n",
				last_rdma_security, cur_rdma_security, pgc->dc_rdma_buf_id);
			pgc->dc_rdma_buf_id = 0;
		}

		pconfig->rdma_config.security = g_rdma_security;
		if (pconfig->rdma_config.security == DISP_SECURE_BUFFER) {
			/* get secure buffer handle and offset */
			writing_mva = get_rdma_secure_buffer_handle();
			pconfig->rdma_config.buf_offset = get_rdma_secure_buffer_offset();
		} else
			writing_mva = get_rdma_normal_buffer_mva();
#else
		writing_mva = get_rdma_normal_buffer_mva();
#endif

		if (writing_mva)
			pconfig->rdma_config.address = (unsigned int)writing_mva;
		else
			DISPERR("rdma input address is null!!\n");
		pconfig->rdma_config.width = extd_display_get_width();
		pconfig->rdma_config.height = extd_display_get_height();
		pconfig->rdma_config.inputFormat = eRGB888;
		pconfig->rdma_config.pitch = extd_display_get_width() * 3;
		if (hdmi_is_interlace && _is_hdmi_decouple_mode(pgc->mode)) {
			pconfig->rdma_config.height /= 2;
			pconfig->rdma_config.pitch *= 2;
			if (1 == DPI0_IS_TOP_FIELD())
				pconfig->rdma_config.buf_offset += extd_display_get_width() * 3;
		}

		pconfig->rdma_dirty = 1;
		dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_ENABLE);
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_ENABLE);
		cmdqRecFlushAsyncCallback(cmdq_handle, NULL, 0);
		cmdqRecReset(cmdq_handle);
	}

done:
	_ext_disp_path_unlock();

	DISPDBG("hdmi_config_rdma done\n");
}

int ext_disp_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;
	/* DISPFUNC(); */

#ifdef HDMI_SUB_PATH
	/*
	   DISPMSG("%s hdmi_active %d state %d handle 0x%p 0x%p fac %d\n",
	   __func__,
	   is_hdmi_active(),
	   pgc->state,
	   pgc->dpmgr_handle,
	   pgc->ovl2mem_path_handle,
	   boot_up_with_facotry_mode());
	 */

	if (boot_up_with_facotry_mode()) {
		DISPMSG("%s is_hdmi_active %d state %d dpmgr_handle 0x%p\n",
			__func__, is_hdmi_active(), pgc->state, pgc->dpmgr_handle);

		DISPMSG("%s boot_up_with_facotry_mode\n", __func__);
	} else if (pgc->dpmgr_handle == NULL) {
		DISPMSG("%s is_hdmi_active %d state %d dpmgr_handle not init yet!!!!\n",
			__func__, is_hdmi_active(), pgc->state);

		return -1;
	} else if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)
		   || pgc->need_trigger_overlay < 1) {
		DISPMSG("trigger ext display is already slept 0x%p 0x%p\n",
			pgc->dpmgr_handle, pgc->ovl2mem_path_handle);


		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Trigger,
			       0);
		return -1;
	}
#else
	if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)
	    || pgc->need_trigger_overlay < 1) {
		int i = 0;
		DISPMSG("trigger ext display is already slept\n");
		for (i = 0; i < HW_OVERLAY_COUNT; i++)
			mtkfb_release_layer_fence(ext_disp_get_sess_id(), i);
		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Trigger,
			       0);
		return -1;
	}
#endif

	if (hdmi_is_interlace && !_is_hdmi_decouple_mode(pgc->mode) && !_should_start_path())
		return 0;

	if (_is_hdmi_decouple_mode(pgc->mode))
		extd_disp_config_output();

	_ext_disp_path_lock();

	if (_should_trigger_interface()) {
		_trigger_display_interface(blocking, _extd_cmdq_finish_callback, userdata);
	} else {
		/* _trigger_overlay_engine(); */
		/* _trigger_display_interface(FALSE, ovl_wdma_callback, 0); */
		_trigger_ovl_to_memory(pgc->ovl2mem_path_handle,
				       pgc->cmdq_handle_config, ovl_wdma_callback, 0);
	}

	_ext_disp_path_unlock();

	/* for pan display : factory/recovery mode */
	if (_is_hdmi_decouple_mode(pgc->mode) && boot_up_with_facotry_mode())
		hdmi_config_rdma();

	DISPDBG("ext_disp_trigger done\n");

	return ret;
}

int ext_disp_config_input(ext_disp_input_config *input)
{
	int ret = 0;
	/* int i = 0; */
	/* int layer = 0; */
	/* /DISPFUNC(); */

	disp_ddp_path_config *data_config;

	/* all dirty should be cleared in dpmgr_path_get_last_config() */

	disp_path_handle *handle;

	if (_is_hdmi_decouple_mode(pgc->mode))
		handle = pgc->ovl2mem_path_handle;
	else
		handle = pgc->dpmgr_handle;

	_ext_disp_path_lock();
#ifdef HDMI_SUB_PATH
	DISPMSG("%s ext disp is slept %d is_hdmi_active %d\n", __func__,
		ext_disp_is_sleepd(), is_hdmi_active());
#else
	if ((is_hdmi_active() == false) || ext_disp_is_sleepd()) {
		DISPMSG("ext disp is already slept\n");
		_ext_disp_path_unlock();
		return 0;
	}
#endif

	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

#ifdef EXTD_DBG_USE_INNER_BUF
	if (input->fmt == eYUY2) {
		/* /input->layer_en = 1; */
		/* /memset(input, 0, sizeof(ext_disp_input_config)); */
		input->layer_en = 1;
		input->addr = hdmi_mva_r;
		input->vaddr = hdmi_va;
		input->fmt = eRGB888;	/* /eRGBA8888  eYUY2 */
		input->src_w = 1280;
		input->src_h = 720;
		input->src_x = 0;
		input->src_y = 0;
		input->src_pitch = 1280 * 3;
		input->dst_w = 1280;
		input->dst_h = 720;
		input->dst_x = 0;
		input->dst_y = 0;
		input->aen = 0;
		input->alpha = 0xff;
	}
#endif


	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
		data_config->ovl_dirty = 1;
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);


	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	ret =
	    dpmgr_path_config(handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;
	/* /DISPMSG("ext_disp_config_input done\n"); */

	_ext_disp_path_unlock();


	return ret;
}

int get_cur_config_fence(int idx)
{
	int fence_idx;

	cmdqBackupReadSlot(pgc->cur_config_fence, idx, &fence_idx);
	return fence_idx;
}

int get_subtractor_when_free(int idx)
{
	int fence_idx;

	cmdqBackupReadSlot(pgc->subtractor_when_free, idx, &fence_idx);
	return fence_idx;
}

int ext_disp_config_input_multiple(ext_disp_input_config *input,
				   disp_session_input_config *session_input)
{
	int ret = 0;
	int i = 0;
	disp_ddp_path_config *data_config;
	disp_path_handle *handle;
	int idx = session_input->config[0].next_buff_idx;
	int fps = 0;

	/* DISPFUNC(); */
	if (_is_hdmi_decouple_mode(pgc->mode))
		handle = pgc->ovl2mem_path_handle;
	else
		handle = pgc->dpmgr_handle;

#ifdef HDMI_SUB_PATH
	if ((pgc->dpmgr_handle == NULL)
	    && !boot_up_with_facotry_mode()) {
		DISPMSG("%s is_hdmi_active %d state %d dpmgr_handle not init yet!!!!\n",
			__func__, is_hdmi_active(), pgc->state);

		DISPMSG("config ext disp is already slept 0x%p 0x%p\n",
			pgc->dpmgr_handle, pgc->ovl2mem_path_handle);

		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Config,
			       idx);
		return 0;
	}
#endif

	if ((is_hdmi_active() == false) || (pgc->state != EXTD_RESUME)) {
		DISPMSG("%s ext disp is already slept,is_hdmi_active %d, ext disp state %d\n",
			__func__, is_hdmi_active(), pgc->state);
		MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo, MMProfileFlagPulse, Config,
			       idx);
		return 0;
	}

	_ext_disp_path_lock();

	/* write fence_id/enable to DRAM using cmdq
	 * it will be used when release fence (put these after config registers done) */
	for (i = 0; i < session_input->config_layer_num; i++) {
		unsigned int last_fence, cur_fence;
		disp_input_config *input_cfg = &session_input->config[i];
		int layer = input_cfg->layer_id;

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

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;

	/* hope we can use only 1 input struct for input config, just set layer number */
	if (_should_config_ovl_input()) {
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			/* /dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG,
			   input->layer|(input->layer_en<<16), input->addr); */

			if (input[i].dirty) {
				dprec_mmp_dump_ovl_layer(&(data_config->ovl_config[input[i].layer]),
							 input[i].layer, 2);
				ret =
				    _convert_disp_input_to_ovl(&
							       (data_config->ovl_config
								[input[i].layer]), &input[i]);
			}
			/*
			   else
			   {
			   data_config->ovl_config[input[i].layer].layer_en = input[i].layer_en;
			   data_config->ovl_config[input[i].layer].layer = input[i].layer;
			   }
			 */
			data_config->ovl_dirty = 1;

			/* /dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input->src_x, input->src_y); */
		}

		/* get video fps from hwc */
		for (i = 0; i < HW_OVERLAY_COUNT; i++) {
			if (input[i].dirty) {
				fps = input[i].fps;
				if (fps != 0)
					break;
			}
		}
		hdmi_set_video_fps(fps);
	} else {
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty = 1;
	}

	if (_should_wait_path_idle())
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ / 2);

	memcpy(&(data_config->dispif_config), &(extd_dpi_params.dispif_config), sizeof(LCM_PARAMS));
	ret =
	    dpmgr_path_config(handle, data_config,
			      ext_disp_cmdq_enabled() ? pgc->cmdq_handle_config : NULL);

	/* this is used for decouple mode, to indicate whether we need to trigger ovl */
	pgc->need_trigger_overlay = 1;

	/* backup ovl status for debug */
	if (ext_disp_is_video_mode() && !ext_disp_is_decouple_mode()) {
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config, pgc->ovl_status_info, 1,
					    disp_addr_convert(DISP_REG_OVL_ADDCON_DBG + DISP_OVL_INDEX_OFFSET));
		cmdqRecBackupRegisterToSlot(pgc->cmdq_handle_config, pgc->ovl_status_info, 0,
					    disp_addr_convert(DISP_REG_OVL_STA + DISP_OVL_INDEX_OFFSET));
	}

	_ext_disp_path_unlock();
	DISPDBG("config_input_multiple idx %x -w %d, h %d\n", idx, data_config->ovl_config[0].src_w,
		data_config->ovl_config[0].src_h);

	DISP_PRINTF(DDP_RESOLUTION_LOG,
		    "config_input_multiple idx %x -l0(%d %d) l1(%d %d) l2(%d %d) l3(%d %d) dst(%d %d)\n",
		    idx, data_config->ovl_config[0].src_w, data_config->ovl_config[0].src_h,
		    data_config->ovl_config[1].src_w, data_config->ovl_config[1].src_h,
		    data_config->ovl_config[2].src_w, data_config->ovl_config[2].src_h,
		    data_config->ovl_config[3].src_w, data_config->ovl_config[3].src_h,
		    data_config->dst_w, data_config->dst_h);


	return ret;
}

int ext_disp_is_alive(void)
{
	unsigned int temp = 0;

	temp = pgc->state;

	return temp;
}

int ext_disp_is_sleepd(void)
{
	unsigned int temp = 0;

	temp = !pgc->state;

	return temp;
}



int ext_disp_get_width(void)
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

int ext_disp_get_height(void)
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

int ext_disp_get_bpp(void)
{
	return 32;
}

int ext_disp_get_info(void *info)
{
	return 0;
}

unsigned int ext_disp_get_sess_id(void)
{
	if (is_context_inited > 0)
		return pgc->session;
	else
		return 0;
}

int ext_disp_get_pages(void)
{
	return 3;
}

int ext_disp_is_video_mode(void)
{
	/* TODO: we should store the video/cmd mode in runtime, because ROME will support cmd/vdo dynamic switch */
	return extd_drv_is_video_mode(pgc->plcm);
}

int ext_disp_diagnose(void)
{
	int ret = 0;

	if (is_context_inited > 0) {
		DISPCHECK("ext_disp_diagnose, is_context_inited --%d\n", is_context_inited);
		DISPMSG("========================= dump hdmi begin =========================\n");
		if (_is_hdmi_decouple_mode(pgc->mode))
			dpmgr_check_status(pgc->ovl2mem_path_handle);
		dpmgr_check_status(pgc->dpmgr_handle);
		DISPMSG("========================= dump hdmi finish ========================\n");
	} else
		ddp_dpi_dump(DISP_MODULE_DPI1, 0);

	return ret;
}

CMDQ_SWITCH ext_disp_cmdq_enabled(void)
{
	return ext_disp_use_cmdq;
}

int ext_disp_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq)
{
	_ext_disp_path_lock();

	ext_disp_use_cmdq = use_cmdq;
	DISPCHECK("display driver use %s to config register now\n",
		  (use_cmdq == CMDQ_ENABLE) ? "CMDQ" : "CPU");

	_ext_disp_path_unlock();
	return ext_disp_use_cmdq;
}

/* for dump decouple internal buffer */
#define COPY_SIZE 512
void extd_disp_dump_decouple_buffer(void)
{
	char *file_name = "hdmidcbuf.bin";
	char fileName[20];
	mm_segment_t fs;
	struct file *fp = NULL;
	char buffer[COPY_SIZE];
	unsigned char *pBuffer;
	unsigned int bufferSize = extd_display_get_width() * extd_display_get_height() * 3;
	int i = 0;

	if (!_is_hdmi_decouple_mode(pgc->mode)) {
		DISPERR("extd_disp_dump_decouple_buffer : is not DECOUPLE mode, return\n");
		return;
	}

	_ext_disp_path_lock();

	if (hdmi_res_is_4k)
		pBuffer = (unsigned char *)dc_vAddr[pgc->dc_buf_id];
	else
		pBuffer = (unsigned char *)split_dc_vAddr[pgc->dc_buf_id];

	memset(fileName, 0, 20);
	if (NULL != file_name && *file_name != '\0')
		snprintf(fileName, 19, "/sdcard/%s", file_name);

	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0x644);

	/* write date */
	for (i = 0; i < bufferSize / COPY_SIZE; i++) {
		/* DISPMSG("[%4d] memcpy pBuffer(%p)\n", i+1, pBuffer); */
		memcpy(buffer, pBuffer, COPY_SIZE);
		fp->f_op->write(fp, buffer, COPY_SIZE, &fp->f_pos);
		pBuffer += COPY_SIZE;
	}

	filp_close(fp, NULL);
	set_fs(fs);

	_ext_disp_path_unlock();

	DISPMSG("extd_disp_dump_decouple_buffer end\n");
}
