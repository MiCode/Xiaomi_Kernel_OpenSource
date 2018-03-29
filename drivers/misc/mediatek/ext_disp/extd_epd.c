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

/*****************************************************************************/
/*****************************************************************************/
#include "extd_info.h"

#if defined(CONFIG_MTK_EPD_SUPPORT)
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kthread.h>
/*#include <linux/rtpm_prio.h>*/

#include <linux/atomic.h>
#include <linux/io.h>

/*#include "mach/eint.h"*/
#include "mach/irqs.h"

#include "ddp_irq.h"
#include "ddp_info.h"
#include "mtkfb_fence.h"
#include "mtkfb_info.h"
#include "epd_drv.h"
#include "external_display.h"
#include "extd_log.h"
#include "extd_platform.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~the static variable~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static int epd_layer_num;
atomic_t epd_state = ATOMIC_INIT(0);
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~~the gloable variable~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
LCM_EPD_PARAMS EPD_Params;
disp_ddp_path_config extd_epd_params;

struct task_struct *epd_fence_release_task = NULL;
wait_queue_head_t epd_fence_release_wq;
atomic_t epd_fence_release_event = ATOMIC_INIT(0);

wait_queue_head_t epd_vsync_wq;
atomic_t epd_vsync_event = ATOMIC_INIT(0);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~~the definition~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
enum EPD_POWER_STATE {
	EPD_STATE_OFF = 0,
	EPD_STATE_ON,
	EPD_STATE_STANDBY,
};

#define IS_EPD_ON()            (EPD_STATE_ON == atomic_read(&epd_state))
#define IS_EPD_OFF()          (EPD_STATE_OFF == atomic_read(&epd_state))
#define IS_EPD_STANDBY()      (EPD_STATE_STANDBY == atomic_read(&epd_state))

#define SET_EPD_ON()          atomic_set(&epd_state, EPD_STATE_ON)
#define SET_EPD_OFF()         atomic_set(&epd_state, EPD_STATE_OFF)
#define SET_EPD_STANDBY()     atomic_set(&epd_state, EPD_STATE_STANDBY)
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~~~extern declare~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void _epd_rdma_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	/*RET_VOID_IF_NOLOG(!IS_EPD_ON());*/
	if (!IS_EPD_ON())
		return;

	if (param & 0x2) {	/* start */
		atomic_set(&epd_fence_release_event, 1);
		wake_up_interruptible(&epd_fence_release_wq);

		/* vsync */
		atomic_set(&epd_vsync_event, 1);
		wake_up_interruptible(&epd_vsync_wq);

	}
}

/* extern int ddp_dpi_dump(DISP_MODULE_ENUM module, int level); */
static int epd_fence_release_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 }; /*RTPM_PRIO_SCRN_UPDATE*/

	sched_setscheduler(current, SCHED_RR, &param);

	unsigned int session_id = 0;
	int fence_idx = 0;
	unsigned long input_curr_addr;

	for (;;) {
		wait_event_interruptible(epd_fence_release_wq, atomic_read(&epd_fence_release_event));
		atomic_set(&epd_fence_release_event, 0);

		session_id = ext_disp_get_sess_id();
		fence_idx = -1;

		if (session_id == 0)
			continue;

		ext_disp_get_curr_addr(&input_curr_addr, 0);
		fence_idx = disp_sync_find_fence_idx_by_addr(session_id, 0, input_curr_addr);
		mtkfb_release_fence(session_id, 0, fence_idx);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int epd_waitVsync(void)
{

	unsigned int session_id = ext_disp_get_sess_id();
	disp_session_sync_info *session_info = disp_get_session_sync_info_for_debug(session_id);

	if (session_info)
		dprec_start(&session_info->event_waitvsync, 0, 0);

	if (!IS_EPD_ON()) {
		EPD_ERR("[epd]:epd has suspend, return directly\n");
		msleep(20);
		return 0;
	}

	if (wait_event_interruptible_timeout(epd_vsync_wq, atomic_read(&epd_vsync_event), HZ / 10) == 0)
		EPD_ERR("[epd] Wait VSync timeout. early_suspend=%d\n", IS_EPD_ON());

	atomic_set(&epd_vsync_event, 0);

	if (session_info)
		dprec_done(&session_info->event_waitvsync, 1, 0);

	return 0;
}

/*static*/ void epd_suspend(void)
{
	EPD_FUNC();
	/*RET_VOID_IF(!IS_EPD_ON());*/
	if (IS_EPD_ON())
		SET_EPD_STANDBY();
}

/*static*/ void epd_resume(void)
{
	EPD_FUNC();
	/*RET_VOID_IF(!IS_EPD_STANDBY());*/
	if (IS_EPD_STANDBY())
		SET_EPD_ON();
}

void epd_enable(int enable)
{
	EPD_FUNC();
}

void epd_power_enable(int enable)
{
	EPD_FUNC();

	if (enable) {
		/*RET_VOID_IF(!IS_EPD_OFF());*/
		/* need add actions */
		if (IS_EPD_OFF())
			SET_EPD_ON();
	} else {
		/*RET_VOID_IF(IS_EPD_OFF());*/
		/* need add actions */
		if (!IS_EPD_OFF())
			SET_EPD_OFF();
	}
}

int epd_get_dev_info(int is_sf, void *info)
{
	int ret = 0;
	unsigned int Eink_width = 0;
	unsigned int Eink_height = 0;

	EPD_DRIVER *epd_drv = (EPD_DRIVER *) EPD_GetDriver();

	if (epd_drv == NULL) {
		EPD_ERR("[epd]%s, can not get epd driver handle\n", __func__);
		return -EFAULT;
	}

	if (epd_drv->get_screen_size)
		epd_drv->get_screen_size(&Eink_width, &Eink_height);

	if (is_sf == AP_GET_INFO) {
		int displayid = 0;
		mtk_dispif_info_t epd_info;

		if (!info) {
			EPD_ERR("ioctl pointer is NULL\n");
			return -EFAULT;
		}

		if (copy_from_user(&displayid, info, sizeof(displayid))) {
			EPD_ERR(": copy_from_user failed! line:%d\n", __LINE__);
			return -EAGAIN;
		}

		memset(&epd_info, 0, sizeof(mtk_dispif_info_t));
		epd_info.displayFormat = DISPIF_FORMAT_RGB888;
		epd_info.displayHeight = Eink_height;
		epd_info.displayWidth = Eink_width;
		epd_info.display_id = displayid;
		epd_info.isConnected = 1;
		epd_info.displayMode = DISP_IF_MODE_VIDEO;
		epd_info.displayType = DISP_IF_EPD;
		epd_info.vsyncFPS = 60;
		epd_info.isHwVsyncAvailable = 1;

		if (copy_to_user(info, &epd_info, sizeof(mtk_dispif_info_t))) {
			EPD_ERR("copy_to_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		}

		HDMI_LOG("DEV_INFO configuration get displayType-%d\n", epd_info.displayType);
	} else if (is_sf == SF_GET_INFO) {
		disp_session_info *dispif_info = (disp_session_info *) info;

		memset((void *)dispif_info, 0, sizeof(disp_session_info));

		dispif_info->maxLayerNum = epd_layer_num;
		dispif_info->displayFormat = DISPIF_FORMAT_RGB888;
		dispif_info->displayHeight = Eink_height;
		dispif_info->displayWidth = Eink_width;
		dispif_info->displayMode = DISP_IF_MODE_VIDEO;
		dispif_info->isConnected = 1;
		dispif_info->displayType = DISP_IF_EPD;
		dispif_info->vsyncFPS = extd_epd_params.fps;
		dispif_info->isHwVsyncAvailable = 1;

		EPD_LOG("epd_get_dev_info lays:%d, type:%d, W:%d, H:%d\n", dispif_info->maxLayerNum,
			dispif_info->displayType, dispif_info->displayWidth, dispif_info->displayHeight);
	}

	return ret;
}

int epd_get_device_type(void)
{
	int device_type = -1;

	device_type = DISP_IF_EPD;

	return device_type;
}

void epd_set_layer_num(int layer_num)
{
	if (layer_num >= 0)
		epd_layer_num = (layer_num == 0 ? 0 : 1);
}

int epd_ioctl(unsigned int ioctl_cmd, int param1, int param2, unsigned long *params)
{
	EPD_LOG("epd_ioctl ioctl_cmd:%d\n", ioctl_cmd);
	int ret = 0;

	switch (ioctl_cmd) {
	case RECOMPUTE_BG_CMD:
		/*  */
		break;
	case GET_DEV_TYPE_CMD:
		ret = epd_get_device_type();
		break;
	case SET_LAYER_NUM_CMD:
		epd_set_layer_num(param1);
		break;
	default:
		EPD_LOG("epd_ioctl unknown command\n");
		break;
	}

	return ret;
}

void epd_init(void)
{
	EPD_LOG("epd_init in+!\n");
	memset((void *)&EPD_Params, 0, sizeof(LCM_EPD_PARAMS));
	memset((void *)&extd_epd_params, 0, sizeof(disp_ddp_path_config));

	EPD_DRIVER *epd_drv = (EPD_DRIVER *) EPD_GetDriver();

	if (epd_drv == NULL) {
		EPD_ERR("[epd]%s, can not get epd driver handle\n", __func__);
		return;
	}

	if (epd_drv->init) {
		/* need to remove to power on function Donglei */
		epd_drv->init();
	}

	if (epd_drv->get_params) {
		epd_drv->get_params(&EPD_Params);
		extd_epd_params.fps = EPD_Params.pannel_frq;

		extd_epd_params.dispif_config.dpi.hsync_pulse_width = EPD_Params.hsync_pulse_width;
		extd_epd_params.dispif_config.dpi.hsync_back_porch = EPD_Params.hsync_back_porch;
		extd_epd_params.dispif_config.dpi.hsync_front_porch = EPD_Params.hsync_front_porch;

		extd_epd_params.dispif_config.dpi.vsync_pulse_width = EPD_Params.vsync_pulse_width;
		extd_epd_params.dispif_config.dpi.vsync_back_porch = EPD_Params.vsync_back_porch;
		extd_epd_params.dispif_config.dpi.vsync_front_porch = EPD_Params.vsync_front_porch;

		extd_epd_params.dispif_config.dpi.dpi_clock = EPD_Params.PLL_CLOCK;

		extd_epd_params.dispif_config.dpi.clk_pol = EPD_Params.clk_pol;
		extd_epd_params.dispif_config.dpi.de_pol = EPD_Params.de_pol;
		extd_epd_params.dispif_config.dpi.hsync_pol = EPD_Params.hsync_pol;
		extd_epd_params.dispif_config.dpi.vsync_pol = EPD_Params.vsync_pol;

		extd_epd_params.dispif_config.dpi.width = EPD_Params.width;
		extd_epd_params.dispif_config.dpi.height = EPD_Params.height;

		extd_epd_params.dispif_config.dpi.format = LCM_DPI_FORMAT_RGB888;
		extd_epd_params.dispif_config.dpi.rgb_order = LCM_COLOR_ORDER_RGB;
		extd_epd_params.dispif_config.dpi.i2x_en = EPD_Params.i2x_en;
		extd_epd_params.dispif_config.dpi.i2x_edge = EPD_Params.i2x_edge;
		extd_epd_params.dispif_config.dpi.embsync = EPD_Params.embsync;
	}
	ext_disp_set_lcm_param(&(extd_epd_params.dispif_config));

	init_waitqueue_head(&epd_fence_release_wq);
	init_waitqueue_head(&epd_vsync_wq);

	if (!epd_fence_release_task) {
		disp_register_module_irq_callback(DISP_MODULE_RDMA, _epd_rdma_irq_handler);
		epd_fence_release_task = kthread_create(epd_fence_release_kthread, NULL, "epd_fence_release_kthread");
		wake_up_process(epd_fence_release_task);
	}

	SET_EPD_OFF();
}
#endif

const struct EXTD_DRIVER *EXTD_EPD_Driver(void)
{
	static const struct EXTD_DRIVER extd_driver_epd = {
#if defined(CONFIG_MTK_EPD_SUPPORT)
		.init = epd_init,
		.post_init = NULL,
		.deinit = NULL,
		.enable = epd_enable,
		.power_enable = epd_power_enable,
		.set_audio_enable = NULL,
		.set_resolution = NULL,
		.get_dev_info = epd_get_dev_info,
		.get_capability = NULL,
		.get_edid = NULL,
		.wait_vsync = epd_waitVsync,
		.fake_connect = NULL,
		.factory_mode_test = NULL,
		.ioctl = epd_ioctl
#else
		.init = 0,
		.post_init = 0
#endif
	};

	return &extd_driver_epd;
}
