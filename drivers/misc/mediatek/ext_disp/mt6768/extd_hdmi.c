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

/********************************/
/********************************/
#include "extd_info.h"

#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/string.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif

#include "mt-plat/mtk_boot_common.h"
#include "mtkfb_info.h"
#include "mtkfb.h"

#include "mtkfb_fence.h"
#include "display_recorder.h"

#include "ddp_info.h"
#include "ddp_irq.h"
#include "ddp_mmp.h"

#include "disp_session.h"

#include "extd_platform.h"
#include "extd_hdmi.h"
#include "extd_factory.h"
#include "extd_log.h"
#include "extd_utils.h"
#include "extd_hdmi_types.h"
#include "external_display.h"

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include <linux/sbsuspend.h>
#include "smartbook.h"
#endif

#ifdef MHL_DYNAMIC_VSYNC_OFFSET
#include "ged_dvfs.h"
#endif

#if defined(CONFIG_MTK_DCS)
#include "mt-plat/mtk_meminfo.h"
#endif


/* ~~~~~~~~~~the static variable~~~~~~~~~ */

static atomic_t hdmi_fake_in = ATOMIC_INIT(false);

static int wait_vsync_enable;
static unsigned long hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
static unsigned long force_reschange = 0xffff;
int enable_ut;
unsigned int dst_is_dsi;
static struct switch_dev hdmi_switch_data;
static struct switch_dev hdmires_switch_data;

static struct HDMI_DRIVER *hdmi_drv;
static struct _t_hdmi_context hdmi_context;
static struct _t_hdmi_context *p = &hdmi_context;

struct task_struct *hdmi_3d_config_task;
wait_queue_head_t hdmi_3d_config_wq;
atomic_t hdmi_3d_config_event = ATOMIC_INIT(0);

#if defined(CONFIG_MTK_DCS)
struct task_struct *dcs_switch_to_4ch_task;
wait_queue_head_t dcs_switch_to_4ch_wq;
atomic_t dcs_4ch_switch_event = ATOMIC_INIT(0);

wait_queue_head_t hdmi_video_config_wq;
atomic_t dcs_4ch_switch_done_event = ATOMIC_INIT(0);
#endif

static unsigned int hdmi_layer_num;
static unsigned int hdmi_resolution_param_table[][3] = {
	{720, 480, 60},
	{1280, 720, 60},
	{1920, 1080, 30},
	{1920, 1080, 60},
};

unsigned int project_is_bsp;
enum HDMI_VIDEO_RESOLUTION hdmi_max_resolution;


DEFINE_SEMAPHORE(hdmi_update_mutex);
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~the gloable variable~~~~~~~~~~ */
struct HDMI_PARAMS _s_hdmi_params = { 0 };

struct HDMI_PARAMS *hdmi_params = &_s_hdmi_params;
struct disp_ddp_path_config extd_dpi_params;

struct task_struct *hdmi_wait_vsync_task;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~the definition~~~~~~~~~~~~~ */
#define IS_HDMI_ON()		\
	(atomic_read(&p->state) == HDMI_POWER_STATE_ON)
#define IS_HDMI_OFF()		\
	(atomic_read(&p->state) == HDMI_POWER_STATE_OFF)
#define IS_HDMI_STANDBY()		\
	(atomic_read(&p->state) == HDMI_POWER_STATE_STANDBY)

#define IS_HDMI_NOT_ON()		\
	(atomic_read(&p->state) != HDMI_POWER_STATE_ON)
#define IS_HDMI_NOT_OFF()		\
	(atomic_read(&p->state) != HDMI_POWER_STATE_OFF)
#define IS_HDMI_NOT_STANDBY()		\
	(atomic_read(&p->state) != HDMI_POWER_STATE_STANDBY)

#define SET_HDMI_ON()			\
	atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()			\
	atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY()		\
	atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)


#define IS_HDMI_FAKE_PLUG_IN()		\
	(atomic_read(&hdmi_fake_in) == true)
#define SET_HDMI_FAKE_PLUG_IN()		\
	(atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()		\
	(atomic_set(&hdmi_fake_in, false))

#define MHL_SESSION_ID          (0x20001)
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* ------------------------------------------------ */
/* Information Dump Routines */
/* ------------------------------------------------ */

void hdmi_force_on(int from_uart_drv)
{

}

/*params & 0xff: resolution,  params & 0xff00 : 3d support*/
void hdmi_force_resolution(int params)
{
	force_reschange = params;
	if ((force_reschange > 0xff) && (force_reschange < 0x0fff))
		hdmi_params->is_3d_support = 1;
	else
		hdmi_params->is_3d_support = 0;

	EXTDMSG("%s params:0x%lx, 3d:%d\n", __func__, force_reschange,
		hdmi_params->is_3d_support);
}

#ifdef MM_MHL_DVFS
#include "mmdvfs_mgr.h"
static void hdmi_enable_dvfs(int enable)
{
	mmdvfs_mhl_enable(enable);
}
#else
static void hdmi_enable_dvfs(int enable)
{
}
#endif

/* <--for debug */
void hdmi_cable_fake_plug_in(void)
{
	SET_HDMI_FAKE_PLUG_IN();
	EXTDMSG("[HDMIFake]Cable Plug In\n");

	if (p->is_force_disable == false) {
		if (IS_HDMI_STANDBY()) {
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
			ged_dvfs_vsync_offset_event_switch
			    (GED_DVFS_VSYNC_OFFSET_MHL_EVENT, true);
#endif
			hdmi_resume();
			hdmi_enable_dvfs(true);
			/* /msleep(1000); */
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			if (project_is_bsp == 1) {
				EXTDMSG
				    ("[HDMIFake]set res for BSP project\n");
				hdmi_set_resolution(HDMI_VIDEO_1280x720p_60Hz);
			}
		}
	}
}

void hdmi_cable_fake_plug_out(void)
{
	SET_HDMI_NOT_FAKE();
	EXTDMSG("[HDMIFake]Disable\n");

	if (p->is_force_disable == false) {
		if (IS_HDMI_ON()) {
			if ((hdmi_drv->get_state() != HDMI_STATE_ACTIVE)
			    || (enable_ut == 1)) {
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
				ged_dvfs_vsync_offset_event_switch
				    (GED_DVFS_VSYNC_OFFSET_MHL_EVENT, false);
#endif
				hdmi_suspend();
				hdmi_enable_dvfs(false);
				switch_set_state(&hdmi_switch_data,
						 HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmires_switch_data, 0);
			}
		}
	}
	force_reschange = 0xffff;
}

int hdmi_cable_fake_connect(int connect)
{
	if (connect == 0)
		hdmi_cable_fake_plug_out();
	else
		hdmi_cable_fake_plug_in();

	return 0;
}

static int hdmi_wait_vsync_kthread(void *data)
{
	struct disp_session_vsync_config vsync_config;

	struct sched_param param = {.sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		ext_disp_wait_for_vsync((void *)&vsync_config, MHL_SESSION_ID);

		if (wait_vsync_enable == 0)
			break;

		if (kthread_should_stop())
			break;
	}

	hdmi_wait_vsync_task = NULL;
	return 0;
}

int hdmi_wait_vsync_debug(int enable)
{
	wait_vsync_enable = enable;

	if (enable != 0 && hdmi_wait_vsync_task == NULL) {
		hdmi_wait_vsync_task =
		    kthread_create(hdmi_wait_vsync_kthread, NULL,
				   "hdmi_wait_vsync_kthread");
		wake_up_process(hdmi_wait_vsync_task);
	}

	return 0;
}

int hdmi_dump_vendor_chip_register(void)
{
	int ret = 0;

	if (hdmi_drv != NULL && hdmi_drv->dump != NULL)
		hdmi_drv->dump();
	return ret;
}

int hdmi_get_support_info(void)
{
	int value = 0, temp = 0;

	EXTDFUNC();
#ifdef USING_SCALE_ADJUSTMENT
	value |= HDMI_SCALE_ADJUSTMENT_SUPPORT;
#endif

#ifdef MHL_PHONE_GPIO_REUSAGE
	value |= HDMI_PHONE_GPIO_REUSAGE;
#endif

#ifdef MTK_AUDIO_MULTI_CHANNEL_SUPPORT
	temp = hdmi_drv->get_external_device_capablity();
#else
	temp = 0x2 << 3;
#endif

	value |= temp;
	value |= HDMI_FACTORY_MODE_NEW;

	EXTDINFO("value is 0x%x\n", value);
	return value;
}


/* Configure video attribute */
int hdmi_video_config(enum HDMI_VIDEO_RESOLUTION vformat,
		      enum HDMI_VIDEO_INPUT_FORMAT vin,
		      enum HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	if (p->is_mhl_video_on == true && (p->vout == vout))
		return 0;

	EXTDMSG("%s video_on=%d\n", __func__, p->is_mhl_video_on);
	if (IS_HDMI_NOT_ON()) {
		EXTDMSG("return in %d\n", __LINE__);
		return 0;
	}

	if ((p->is_mhl_video_on == true) && (p->vout != vout)) {
		p->vout = vout;
		p->vin = vin;
		atomic_set(&hdmi_3d_config_event, 1);
		wake_up_interruptible(&hdmi_3d_config_wq);
		return 0;
	}

	p->vout = vout;
	p->vin = vin;

#if defined(CONFIG_MTK_DCS)
	if (vformat == HDMI_VIDEO_2160p_DSC_30Hz
	    || vformat == HDMI_VIDEO_2160p_DSC_24Hz) {
		/* wait DCS switch to 4ch */
		wait_event_interruptible(hdmi_video_config_wq,
					 atomic_read
					 (&dcs_4ch_switch_done_event));
		atomic_set(&dcs_4ch_switch_done_event, 0);
		EXTDMSG("%s wait DCS switch to 4ch done\n", __func__);
	}
#endif

	p->is_mhl_video_on = true;

	if (IS_HDMI_FAKE_PLUG_IN())
		return 0;

	return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
int hdmi_audio_config(int format)
{
	enum HDMI_AUDIO_FORMAT audio_format = HDMI_AUDIO_44K_2CH;
	unsigned int channel_count = format & 0x0F;
	unsigned int sampleRate = (format & 0x70) >> 4;
	unsigned int bitWidth = (format & 0x180) >> 7, sampleBit = 0;

	EXTDFUNC();
	EXTDMSG("channel_count: %d, sampleRate: %d, bitWidth: %d\n",
		channel_count, sampleRate, bitWidth);

	if (bitWidth == HDMI_MAX_BITWIDTH_16) {
		sampleBit = 16;
		EXTDMSG("HDMI_MAX_BITWIDTH_16\n");
	} else if (bitWidth == HDMI_MAX_BITWIDTH_24) {
		sampleBit = 24;
		EXTDMSG("HDMI_MAX_BITWIDTH_24\n");
	}

	if (channel_count == HDMI_MAX_CHANNEL_2
	    && sampleRate == HDMI_MAX_SAMPLERATE_44) {
		audio_format = HDMI_AUDIO_44K_2CH;
		EXTDMSG("AUDIO_44K_2CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_2
		   && sampleRate == HDMI_MAX_SAMPLERATE_48) {
		audio_format = HDMI_AUDIO_48K_2CH;
		EXTDMSG("AUDIO_48K_2CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8
		   && sampleRate == HDMI_MAX_SAMPLERATE_32) {
		audio_format = HDMI_AUDIO_32K_8CH;
		EXTDMSG("AUDIO_32K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8
		   && sampleRate == HDMI_MAX_SAMPLERATE_44) {
		audio_format = HDMI_AUDIO_44K_8CH;
		EXTDMSG("AUDIO_44K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8
		   && sampleRate == HDMI_MAX_SAMPLERATE_48) {
		audio_format = HDMI_AUDIO_48K_8CH;
		EXTDMSG("AUDIO_48K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8
		   && sampleRate == HDMI_MAX_SAMPLERATE_96) {
		audio_format = HDMI_AUDIO_96K_8CH;
		EXTDMSG("AUDIO_96K_8CH\n");
	}

	else if (channel_count == HDMI_MAX_CHANNEL_8
		 && sampleRate == HDMI_MAX_SAMPLERATE_192) {
		audio_format = HDMI_AUDIO_192K_8CH;
		EXTDMSG("AUDIO_192K_8CH\n");
	} else
		EXTDERR("audio format is not supported\n");

	if (!p->is_enabled) {
		EXTDMSG("return in %d\n", __LINE__);
		return 0;
	}

	hdmi_drv->audio_config(audio_format, sampleBit);
	return 0;
}

static void hdmi_video_format_config(unsigned int layer_3d_format)
{
	if ((force_reschange > 0xff) && (force_reschange < 0x0fff))
		layer_3d_format = force_reschange >> 8;

	if (layer_3d_format >= DISP_LAYER_3D_TAB_0)
		layer_3d_format = HDMI_VOUT_FORMAT_3D_TAB;
	else if (layer_3d_format >= DISP_LAYER_3D_SBS_0)
		layer_3d_format = HDMI_VOUT_FORMAT_3D_SBS;
	else
		layer_3d_format = HDMI_VOUT_FORMAT_2D;

	hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888,
			  HDMI_VOUT_FORMAT_RGB888 | layer_3d_format);
}

static int hdmi_3d_config_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };
	enum HDMI_VIDEO_RESOLUTION vformat = HDMI_VIDEO_RESOLUTION_NUM;

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_3d_config_wq,
					 atomic_read(&hdmi_3d_config_event));
		atomic_set(&hdmi_3d_config_event, 0);

		EXTDMSG("video_on=%d fps %d, %d, %d\n", p->is_mhl_video_on,
			rdmafpscnt, p->vin, p->vout);

		if (p->vout >= HDMI_VOUT_FORMAT_2D)
			hdmi_drv->video_config(vformat, p->vin, p->vout);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

#if defined(CONFIG_MTK_DCS)
static int hdmi_dcs_switch_to_4ch_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };
	int dcs_channel = 0;
	enum dcs_status dcs_status;
	int ret = 0;
	int i = 10;

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(dcs_switch_to_4ch_wq,
					 atomic_read(&dcs_4ch_switch_event));
		atomic_set(&dcs_4ch_switch_event, 0);

		/* polling dcs switch done */
		i = 10;
		while (i--) {
			msleep(100);
			ret =
			    dcs_get_dcs_status_lock(&dcs_channel, &dcs_status);
			if (dcs_channel == 4 && dcs_status == 0) {
				EXTDMSG
				    ("dcs switch successfully! channel=%d\n",
				     dcs_channel);
				dcs_get_dcs_status_unlock();
				atomic_set(&dcs_4ch_switch_done_event, 1);
				/* wake up hdmi_video_config_wq */
				wake_up_interruptible(&hdmi_video_config_wq);
				goto Finish;
			}
			dcs_get_dcs_status_unlock();
		}
		/* dcs switch fail */
		EXTDERR
		    ("ERROR! dsc switch fail! channel=%d dcs_sta=%d\n",
		     dcs_channel, dcs_status);

Finish:
		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static enum HDMI_STATUS hdmi_drv_init(void)
{
	EXTDFUNC();

	p->hdmi_width =
	    hdmi_resolution_param_table[hdmi_params->init_config.vformat][0];
	p->hdmi_height =
	    hdmi_resolution_param_table[hdmi_params->init_config.vformat][1];
	p->bg_width = 0;
	p->bg_height = 0;

	p->output_video_resolution = hdmi_params->init_config.vformat;
	p->output_audio_format = hdmi_params->init_config.aformat;
	p->scaling_factor =
	    hdmi_params->scaling_factor < 10 ? hdmi_params->scaling_factor : 10;

	p->is_clock_on = false;

	if (!hdmi_3d_config_task) {
		hdmi_3d_config_task =
		    kthread_create(hdmi_3d_config_kthread, NULL,
				   "hdmi_3d_config_kthread");
		wake_up_process(hdmi_3d_config_task);
	}
#if defined(CONFIG_MTK_DCS)
	if (!dcs_switch_to_4ch_task) {
		dcs_switch_to_4ch_task =
		    kthread_create(hdmi_dcs_switch_to_4ch_kthread, NULL,
				   "hdmi_dcs_switch_to_4ch_kthread");
		wake_up_process(dcs_switch_to_4ch_task);
	}
#endif

	return HDMI_STATUS_OK;
}

/* Release memory */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static enum HDMI_STATUS hdmi_drv_deinit(void)
{
	EXTDFUNC();

	return HDMI_STATUS_OK;
}

/* Reset HDMI Driver state */
static void hdmi_state_reset(void)
{
	EXTDFUNC();

	if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
		if (enable_ut != 1)
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
		hdmi_enable_dvfs(true);
		hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		/* Set max resolution for BSP project */
		if (project_is_bsp == 1) {
			EXTDMSG("set max resolution for BSP project\n");
			hdmi_get_edid(NULL);
			hdmi_set_resolution(hdmi_max_resolution);
		}
	} else {
		if (enable_ut != 1) {
			switch_set_state(&hdmi_switch_data,
					 HDMI_STATE_NO_DEVICE);
			switch_set_state(&hdmires_switch_data, 0);
		}
		hdmi_enable_dvfs(false);
	}
}

/*static*/ void hdmi_suspend(void)
{
	int session_id = 0;

	EXTDFUNC();
	if (IS_HDMI_NOT_ON())
		return;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 Plugout, 0);

	if (down_interruptible(&hdmi_update_mutex)) {
		EXTDERR("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	SET_HDMI_STANDBY();
	hdmi_drv->suspend();
	p->is_mhl_video_on = false;
	p->is_clock_on = false;

	ext_disp_suspend(MHL_SESSION_ID);
	session_id = ext_disp_get_sess_id();

#if defined(CONFIG_MTK_DCS)
	/* Nortify DCS can switch to 2ch */
	dcs_exit_perf(DCS_KICKER_MHL);
#endif

	up(&hdmi_update_mutex);

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 Plugout, 0);
	EXTDINFO("hdmi_suspend done\n");
}

/*static*/ void hdmi_resume(void)
{
	EXTDFUNC();
	EXTDMSG("p->state is %d,(0:off, 1:on, 2:standby)\n",
		atomic_read(&p->state));
	if (IS_HDMI_NOT_STANDBY())
		return;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 Plugin, 0);

	if (down_interruptible(&hdmi_update_mutex)) {
		EXTDERR("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	SET_HDMI_ON();
	p->is_clock_on = true;
	hdmi_drv->resume();
	up(&hdmi_update_mutex);

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 Plugin, 0);
	EXTDINFO("hdmi_resume done\n");
}

/*static*/ void hdmi_power_on(void)
{
	EXTDFUNC();

	if (IS_HDMI_NOT_OFF())
		return;

	if (down_interruptible(&hdmi_update_mutex)) {
		EXTDERR("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	SET_HDMI_STANDBY();
	hdmi_drv->power_on();
	ext_disp_resume(MHL_SESSION_ID);
	up(&hdmi_update_mutex);

	if (p->is_force_disable == false) {
		if (IS_HDMI_FAKE_PLUG_IN()) {
			hdmi_resume();
			msleep(1000);
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			/* Set max resolution for BSP project */
			if (project_is_bsp == 1) {
				EXTDMSG
				    ("[HDMIFake]set res for BSP project\n");
				hdmi_set_resolution(HDMI_VIDEO_1280x720p_60Hz);
			}
		} else {
			/* this is just a ugly workaround for some tv sets... */
			if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
				hdmi_drv->get_params(hdmi_params);
				hdmi_resume();
			}

			hdmi_state_reset();
		}
	}
	EXTDINFO("%s done\n", __func__);
}

/*static*/ void hdmi_power_off(void)
{
	EXTDFUNC();
	if (IS_HDMI_OFF())
		return;

	if (down_interruptible(&hdmi_update_mutex)) {
		EXTDERR("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	hdmi_drv->power_off();
	ext_disp_suspend(MHL_SESSION_ID);
	p->is_clock_on = false;
	SET_HDMI_OFF();
	up(&hdmi_update_mutex);

	switch_set_state(&hdmires_switch_data, 0);
	hdmi_enable_dvfs(false);

#if defined(CONFIG_MTK_DCS)
	/* Nortify DCS can switch to 2ch */
	dcs_exit_perf(DCS_KICKER_MHL);
#endif
	EXTDINFO("%s done\n", __func__);
}

static void hdmi_resolution_setting(int arg)
{
	EXTDFUNC();
	struct LCM_DSC_CONFIG_PARAMS  *extd_dsc_params;

	extd_dpi_params.dispif_config.dpi.dsc_enable = 0;
	if (hdmi_drv && hdmi_drv->get_params) {
		hdmi_params->init_config.vformat = arg;
		hdmi_drv->get_params(hdmi_params);

		if (dst_is_dsi) {
			memcpy(&extd_dpi_params.dispif_config.dsi,
			       (void *)(&hdmi_params->dsi_params),
			       sizeof(LCM_DSI_PARAMS));

			p->bg_height =
			    ((hdmi_params->height * p->scaling_factor) /
			     100 >> 2) << 2;
			p->bg_width =
			    ((hdmi_params->width * p->scaling_factor) /
			     100 >> 2) << 2;
			p->hdmi_width = hdmi_params->width - p->bg_width;
			p->hdmi_height = hdmi_params->height - p->bg_height;

			extd_dpi_params.dispif_config.width = p->hdmi_width;
			extd_dpi_params.dispif_config.height = p->hdmi_height;

			extd_dpi_params.dispif_config.type = LCM_TYPE_DSI;

			p->output_video_resolution =
			    hdmi_params->init_config.vformat;

			extd_dsc_params =
			&extd_dpi_params.dispif_config.dsi.dsc_params;
			if ((arg == HDMI_VIDEO_2160p_DSC_24Hz)
			    || (arg == HDMI_VIDEO_2160p_DSC_30Hz)) {
				memset(extd_dsc_params,
				       0, sizeof(LCM_DSC_CONFIG_PARAMS));
				extd_dpi_params.dispif_config.dsi.dsc_enable =
				    1;
				/* width/(slice_mode's slice) */
				extd_dsc_params->slice_width = 1920;
				extd_dsc_params->slice_hight = 8;
				/* 128: 1/3 compress; 192: 1/2 compress */
				extd_dsc_params->bit_per_pixel = 128;
				/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
				extd_dsc_params->slice_mode = 1;
				extd_dsc_params->rgb_swap = 0;
				extd_dsc_params->xmit_delay = 0x200;
				extd_dsc_params->dec_delay = 0x4c0;
				extd_dsc_params->scale_value = 0x20;
				extd_dsc_params->increment_interval = 0x11e;
				extd_dsc_params->decrement_interval = 0x1a;
				extd_dsc_params->nfl_bpg_offset = 0xdb7;
				extd_dsc_params->slice_bpg_offset = 0x394;
				extd_dsc_params->final_offset = 0x10f0;
				extd_dsc_params->line_bpg_offset = 0xc;
				extd_dsc_params->bp_enable = 0x0;
				extd_dsc_params->rct_on = 0x0;

			}
		} else {
			extd_dpi_params.dispif_config.dpi.clk_pol =
			    hdmi_params->clk_pol;
			extd_dpi_params.dispif_config.dpi.de_pol =
			    hdmi_params->de_pol;
			extd_dpi_params.dispif_config.dpi.hsync_pol =
			    hdmi_params->hsync_pol;
			extd_dpi_params.dispif_config.dpi.vsync_pol =
			    hdmi_params->vsync_pol;

			extd_dpi_params.dispif_config.dpi.hsync_pulse_width =
			    hdmi_params->hsync_pulse_width;
			extd_dpi_params.dispif_config.dpi.hsync_back_porch =
			    hdmi_params->hsync_back_porch;
			extd_dpi_params.dispif_config.dpi.hsync_front_porch =
			    hdmi_params->hsync_front_porch;

			extd_dpi_params.dispif_config.dpi.vsync_pulse_width =
			    hdmi_params->vsync_pulse_width;
			extd_dpi_params.dispif_config.dpi.vsync_back_porch =
			    hdmi_params->vsync_back_porch;
			extd_dpi_params.dispif_config.dpi.vsync_front_porch =
			    hdmi_params->vsync_front_porch;

			extd_dpi_params.dispif_config.dpi.dpi_clock =
			    hdmi_params->input_clock;

			p->bg_height =
			    ((hdmi_params->height * p->scaling_factor) /
			     100 >> 2) << 2;
			p->bg_width =
			    ((hdmi_params->width * p->scaling_factor) /
			     100 >> 2) << 2;
			p->hdmi_width = hdmi_params->width - p->bg_width;
			p->hdmi_height = hdmi_params->height - p->bg_height;

			p->output_video_resolution =
			    hdmi_params->init_config.vformat;

			extd_dpi_params.dispif_config.dpi.width = p->hdmi_width;
			extd_dpi_params.dispif_config.dpi.height =
			    p->hdmi_height;
			if ((arg == HDMI_VIDEO_2160p_DSC_24Hz)
			    || (arg == HDMI_VIDEO_2160p_DSC_30Hz)) {
				memset(extd_dsc_params,
				       0, sizeof(LCM_DSC_CONFIG_PARAMS));
				extd_dpi_params.dispif_config.dpi.dsc_enable =
				    1;
				extd_dpi_params.dispif_config.dpi.width =
				    p->hdmi_width / 3;
				/* width/(slice_mode's slice) */
				extd_dsc_params->slice_width = 1920;
				extd_dsc_params->slice_hight = 8;
				/* 128: 1/3 compress; 192: 1/2 compress */
				extd_dsc_params->bit_per_pixel = 128;
				/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
				extd_dsc_params->slice_mode = 1;
				extd_dsc_params->rgb_swap = 0;
				extd_dsc_params->xmit_delay = 0x200;
				extd_dsc_params->dec_delay = 0x4c0;
				extd_dsc_params->scale_value = 0x20;
				extd_dsc_params->increment_interval = 0x11e;
				extd_dsc_params->decrement_interval = 0x1a;
				extd_dsc_params->nfl_bpg_offset = 0xdb7;
				extd_dsc_params->slice_bpg_offset = 0x394;
				extd_dsc_params->final_offset = 0x10f0;
				extd_dsc_params->line_bpg_offset = 0xc;
				extd_dsc_params->bp_enable = 0x0;
				extd_dsc_params->rct_on = 0x0;

			}

			extd_dpi_params.dispif_config.dpi.bg_width =
			    p->bg_width;
			extd_dpi_params.dispif_config.dpi.bg_height =
			    p->bg_height;

			extd_dpi_params.dispif_config.dpi.format =
			    LCM_DPI_FORMAT_RGB888;
			extd_dpi_params.dispif_config.dpi.rgb_order =
			    LCM_COLOR_ORDER_RGB;
			extd_dpi_params.dispif_config.dpi.i2x_en = true;
			extd_dpi_params.dispif_config.dpi.i2x_edge = 2;
			extd_dpi_params.dispif_config.dpi.embsync = false;
		}
	}

	ext_disp_set_lcm_param(&(extd_dpi_params.dispif_config));
	EXTDMSG("hdmi_res_setting_res (%d)\n", arg);
}

int hdmi_recompute_bg(int src_w, int src_h)
{
	int ret = 0;

	return ret;
}

/* HDMI Driver state callback function */
void hdmi_state_callback(enum HDMI_STATE state)
{
	EXTDMSG("[hdmi]%s, state = %d\n", __func__, state);
	if ((p->is_force_disable == true) || (IS_HDMI_FAKE_PLUG_IN()))
		return;

	switch (state) {
	case HDMI_STATE_NO_DEVICE:
		{
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
			ged_dvfs_vsync_offset_event_switch
			    (GED_DVFS_VSYNC_OFFSET_MHL_EVENT, false);
#endif
			hdmi_suspend();
			switch_set_state(&hdmi_switch_data,
					 HDMI_STATE_NO_DEVICE);
			switch_set_state(&hdmires_switch_data, 0);
			hdmi_enable_dvfs(false);

#if defined(CONFIG_MTK_SMARTBOOK_SUPPORT) && defined(CONFIG_HAS_SBSUSPEND)
			if (hdmi_params->cabletype == MHL_SMB_CABLE)
				sb_plug_out();
#endif
			EXTDMSG("[hdmi]%s, state = %d out!\n", __func__, state);
			break;
		}
	case HDMI_STATE_ACTIVE:
		{
			if (IS_HDMI_ON()) {
				EXTDMSG("[hdmi]%s, already on(%d) !\n",
					__func__, atomic_read(&p->state));
				break;
			}

			hdmi_drv->get_params(hdmi_params);
			hdmi_resume();

			if (atomic_read(&p->state) > HDMI_POWER_STATE_OFF) {
				hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
				switch_set_state(&hdmi_switch_data,
						 HDMI_STATE_ACTIVE);
				hdmi_enable_dvfs(true);
				/* Set max resolution for BSP project */
				if (project_is_bsp == 1) {
					EXTDMSG
					    ("set max reso for BSP project\n");
					hdmi_get_edid(NULL);
					hdmi_set_resolution
					    (hdmi_max_resolution);
				}
			}
#if defined(CONFIG_MTK_SMARTBOOK_SUPPORT) && defined(CONFIG_HAS_SBSUSPEND)
			if (hdmi_params->cabletype == MHL_SMB_CABLE)
				sb_plug_in();
#endif
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
			ged_dvfs_vsync_offset_event_switch
			    (GED_DVFS_VSYNC_OFFSET_MHL_EVENT, true);
#endif
			EXTDMSG("[hdmi]%s, state = %d out!\n", __func__, state);
			break;
		}
	default:
		{
			EXTDMSG("[hdmi]%s, state not support\n", __func__);
			break;
		}
	}
	EXTDINFO("%s done\n", __func__);
}

void hdmi_set_layer_num(int layer_num)
{
	if (layer_num >= 0)
#ifdef FIX_EXTD_TO_OVL_PATH
		hdmi_layer_num = FIX_EXTD_TO_OVL_PATH;
#else
		hdmi_layer_num = layer_num;
#endif
}

int hdmi_enable(int enable)
{
	EXTDFUNC();
	if (enable) {
		if (p->is_enabled) {
			EXTDMSG("[hdmi] hdmi already enable, %s()\n", __func__);
			return 0;
		}

		if (hdmi_drv->enter)
			hdmi_drv->enter();

		hdmi_drv_init();
		hdmi_power_on();
		p->is_enabled = true;
	} else {
		if (!p->is_enabled)
			return 0;

		hdmi_power_off();
		hdmi_drv_deinit();
		/* when disable hdmi, HPD is disabled */
		switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);

		p->is_enabled = false;

		if (hdmi_drv->exit)
			hdmi_drv->exit();
	}
	EXTDINFO("%s done\n", __func__);
	return 0;
}

int hdmi_power_enable(int enable)
{
	EXTDFUNC();
	if (!p->is_enabled) {
		EXTDMSG("return in %d\n", __LINE__);
		return 0;
	}

	if (enable) {
		hdmi_power_on();
	} else {
		hdmi_power_off();
		switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
	}
	EXTDINFO("%s done\n", __func__);
	return 0;
}

int hdmi_set_audio_enable(int enable)
{
	if (!p->is_enabled) {
		EXTDMSG("return in %d\n", __LINE__);
		return 0;
	}

	hdmi_drv->audio_enable(enable);
	return 0;
}

int hdmi_set_resolution(int res)
{
	int extd_path_state = 0;
	int i = 0;
	int session_id = 0;

	EXTDFUNC();
	EXTDMSG
	    ("video res conf, res:%d, old res:%ld, video_on:%d\n",
	     res, hdmi_reschange, p->is_mhl_video_on);
	if (!p->is_enabled) {
		EXTDMSG("return in %d\n", __LINE__);
		return 0;
	}

	/* just for debug */
	if ((force_reschange & 0xff) < 0xff)
		res = force_reschange & 0xff;

	if (hdmi_reschange == res) {
		EXTDMSG("hdmi_reschange=%ld\n", hdmi_reschange);
		return 0;
	}
#if defined(CONFIG_MTK_DCS)
	if (res == HDMI_VIDEO_2160p_DSC_30Hz
	    || res == HDMI_VIDEO_2160p_DSC_24Hz) {
		/* Notify the DCS switch to 4ch */
		dcs_enter_perf(DCS_KICKER_MHL);
		atomic_set(&dcs_4ch_switch_event, 1);
		EXTDMSG("Notify the DCS switch to 4ch\n");
		/* wake up dcs_switch_to_4ch_kthread for wait switch done */
		wake_up_interruptible(&dcs_switch_to_4ch_wq);
	}
#endif

	p->is_clock_on = false;
	hdmi_reschange = res;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 ResChange, res);

	if (down_interruptible(&hdmi_update_mutex)) {
		EXTDERR("[HDMI] can't get semaphore in\n");
		return 0;
	}

	/* if ddp path already start, stop ddp path, */
	/* and release all fence of external session */
	extd_path_state = ext_disp_is_alive();

	if (extd_path_state == EXTD_RESUME) {
		if (hdmi_drv != 0)
			if ((hdmi_drv->suspend) != 0)
				hdmi_drv->suspend();

		ext_disp_suspend(MHL_SESSION_ID);

		session_id = ext_disp_get_sess_id();

		for (i = 0; i < EXTD_OVERLAY_CNT; i++)
			mtkfb_release_layer_fence(session_id, i);
	}

	hdmi_resolution_setting(res);
	p->is_mhl_video_on = false;

	up(&hdmi_update_mutex);
	if (enable_ut != 1)
		switch_set_state(&hdmires_switch_data, hdmi_reschange + 1);
	p->is_clock_on = true;
	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 ResChange, hdmi_reschange + 1);

	EXTDINFO("%s done\n", __func__);
	return 0;
}

int hdmi_get_dev_info(int is_sf, void *info)
{
	int ret = 0;

	if (is_sf == AP_GET_INFO) {
		int displayid = 0;
		struct mtk_dispif_info hdmi_info;

		if (!info) {
			EXTDERR("ioctl pointer is NULL\n");
			return -EFAULT;
		}

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_DevInfo,
				 MMPROFILE_FLAG_START, p->is_enabled,
				 p->is_clock_on);

		if (copy_from_user(&displayid, info, sizeof(displayid))) {
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
					 MMPROFILE_FLAG_PULSE, Devinfo, 0);
			EXTDERR(": copy_from_user failed! line:%d\n", __LINE__);
			return -EAGAIN;
		}

		memset(&hdmi_info, 0, sizeof(hdmi_info));
		hdmi_info.displayFormat = DISPIF_FORMAT_RGB888;
		hdmi_info.displayHeight = p->hdmi_height;
		hdmi_info.displayWidth = p->hdmi_width;
		hdmi_info.display_id = displayid;
		hdmi_info.isConnected = 1;
		hdmi_info.displayMode = DISPIF_MODE_COMMAND;

		if (hdmi_params->cabletype == MHL_SMB_CABLE)
			hdmi_info.displayType = HDMI_SMARTBOOK;
		else if (hdmi_params->cabletype == MHL_CABLE)
			hdmi_info.displayType = MHL;
		else if (hdmi_params->cabletype == SLIMPORT_CABLE)
			hdmi_info.displayType = SLIMPORT;
		else
			hdmi_info.displayType = HDMI;

		hdmi_info.isHwVsyncAvailable = HW_DPI_VSYNC_SUPPORT;

		if ((hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz)
		    || (hdmi_reschange == HDMI_VIDEO_2160p_DSC_30Hz))
			hdmi_info.vsyncFPS = 3000;
		else if (hdmi_reschange == HDMI_VIDEO_2160p_DSC_24Hz)
			hdmi_info.vsyncFPS = 2400;
		else
			hdmi_info.vsyncFPS = 6000;

		if (copy_to_user(info, &hdmi_info, sizeof(hdmi_info))) {
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
					 MMPROFILE_FLAG_PULSE, Devinfo, 1);
			EXTDERR("copy_to_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		}

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_DevInfo,
				 MMPROFILE_FLAG_END, p->is_enabled,
				 hdmi_info.displayType);
		EXTDINFO("DEV_INFO configuration get displayType-%d\n",
			 hdmi_info.displayType);
	} else if (is_sf == SF_GET_INFO) {
		struct disp_session_info *dispif_info =
		    (struct disp_session_info *)info;

		memset((void *)dispif_info, 0,
		       sizeof(struct disp_session_info));

		dispif_info->isOVLDisabled = (hdmi_layer_num == 1) ? 1 : 0;
		dispif_info->maxLayerNum = hdmi_layer_num;
		dispif_info->displayFormat = DISPIF_FORMAT_RGB888;
		dispif_info->displayHeight = p->hdmi_height;
		dispif_info->displayWidth = p->hdmi_width;
		dispif_info->displayMode = DISP_IF_MODE_VIDEO;

		if (hdmi_params->cabletype == MHL_SMB_CABLE) {
			dispif_info->displayType = DISP_IF_HDMI_SMARTBOOK;
			if (IS_HDMI_OFF())
				dispif_info->displayType = DISP_IF_MHL;
		} else if (hdmi_params->cabletype == MHL_CABLE)
			dispif_info->displayType = DISP_IF_MHL;
		else if (hdmi_params->cabletype == SLIMPORT_CABLE)
			dispif_info->displayType = DISP_IF_SLIMPORT;
		else
			dispif_info->displayType = DISP_IF_HDMI;

		dispif_info->isHwVsyncAvailable = HW_DPI_VSYNC_SUPPORT;

		if ((hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz)
		    || (hdmi_reschange == HDMI_VIDEO_2160p_DSC_30Hz))
			dispif_info->vsyncFPS = 3000;
		else if (hdmi_reschange == HDMI_VIDEO_2160p_DSC_24Hz)
			dispif_info->vsyncFPS = 2400;
		else
			dispif_info->vsyncFPS = 6000;

		if (dispif_info->displayWidth * dispif_info->displayHeight <=
		    240 * 432)
			dispif_info->physicalHeight =
			    dispif_info->physicalWidth = 0;
		else if (dispif_info->displayWidth *
			 dispif_info->displayHeight <= 320 * 480)
			dispif_info->physicalHeight =
			    dispif_info->physicalWidth = 0;
		else if (dispif_info->displayWidth *
			 dispif_info->displayHeight <= 480 * 854)
			dispif_info->physicalHeight =
			    dispif_info->physicalWidth = 0;
		else
			dispif_info->physicalHeight =
			    dispif_info->physicalWidth = 0;

		dispif_info->isConnected = 1;
		dispif_info->isHDCPSupported = hdmi_params->HDCPSupported;

		/* fake 3d assert for debug */
		if ((force_reschange > 0xff) && (force_reschange < 0xffff))
			hdmi_params->is_3d_support = 1;

		dispif_info->is3DSupport = hdmi_params->is_3d_support;
	}

	return ret;
}

int hdmi_get_capability(void *info)
{
	int ret = 0;
	int query_type = 0;

	EXTDFUNC();
	query_type = hdmi_get_support_info();

	if (copy_to_user(info, &query_type, sizeof(query_type))) {
		EXTDERR(": copy_to_user error! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	EXTDMSG("[hdmi][HDMI] query_type  done %x\n", query_type);

	return ret;
}

void hdmi_get_max_resolution(struct _HDMI_EDID_T *edid_info)
{
	if (edid_info->ui4_pal_resolution & SINK_2160p30)
		hdmi_max_resolution = HDMI_VIDEO_2160p_DSC_30Hz;
	else if (edid_info->ui4_pal_resolution & SINK_2160p24)
		hdmi_max_resolution = HDMI_VIDEO_2160p_DSC_24Hz;
	else if (edid_info->ui4_pal_resolution & SINK_1080P60)
		hdmi_max_resolution = HDMI_VIDEO_1920x1080p_60Hz;
	else if (edid_info->ui4_pal_resolution & SINK_1080P30)
		hdmi_max_resolution = HDMI_VIDEO_1920x1080p_30Hz;
	else if (edid_info->ui4_pal_resolution & SINK_720P60)
		hdmi_max_resolution = HDMI_VIDEO_1280x720p_60Hz;
	else if (edid_info->ui4_pal_resolution & SINK_480P)
		hdmi_max_resolution = HDMI_VIDEO_720x480p_60Hz;
	else {
		hdmi_max_resolution = HDMI_VIDEO_720x480p_60Hz;
		EXTDERR("hdmi resolution is not match!\n");
	}

	EXTDINFO("hdmi max resolution is %d\n", hdmi_max_resolution);
}

int hdmi_get_edid(void *edid_info)
{
	int ret = 0;
	struct _HDMI_EDID_T pv_get_info;

	EXTDFUNC();
	memset(&pv_get_info, 0, sizeof(pv_get_info));

	if (hdmi_drv->getedid) {
		hdmi_drv->getedid(&pv_get_info);

#ifdef MHL_RESOLUTION_LIMIT_720P_60
		pv_get_info.ui4_pal_resolution &= (~SINK_1080P60);
		pv_get_info.ui4_pal_resolution &= (~SINK_1080P30);
		pv_get_info.ui4_pal_resolution &= (~SINK_2160p30);
		pv_get_info.ui4_pal_resolution &= (~SINK_2160p24);
#endif

#ifdef MHL_RESOLUTION_LIMIT_1080P_30
		if (pv_get_info.ui4_pal_resolution & SINK_1080P60)
			pv_get_info.ui4_pal_resolution &= (~SINK_1080P60);

		pv_get_info.ui4_pal_resolution &= (~SINK_2160p30);
		pv_get_info.ui4_pal_resolution &= (~SINK_2160p24);
#endif

		if (pv_get_info.ui4_pal_resolution & SINK_1080P60)
			pv_get_info.ui4_pal_resolution &= (~SINK_1080P30);
	}

	if (project_is_bsp != 1) {
		if (!edid_info) {
			EXTDERR("ioctl pointer is NULL\n");
			return -EFAULT;
		}

		if (copy_to_user(edid_info, &pv_get_info,
				sizeof(pv_get_info))) {
			EXTDERR("copy_to_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		}
	} else {
		/* Get max resolution for BSP project */
		hdmi_get_max_resolution(&pv_get_info);
	}

	EXTDINFO("%s done\n", __func__);
	return ret;
}

int hdmi_get_device_type(void)
{
	int device_type = -1;

	if (IS_HDMI_ON()) {
		if (hdmi_params->cabletype == MHL_SMB_CABLE)
			device_type = DISP_IF_HDMI_SMARTBOOK;
		else if (hdmi_params->cabletype == MHL_CABLE)
			device_type = DISP_IF_MHL;
		else if (hdmi_params->cabletype == SLIMPORT_CABLE)
			device_type = DISP_IF_SLIMPORT;
	}

	return device_type;
}

int hdmi_ioctl(unsigned int ioctl_cmd, int param1, int param2,
	       unsigned long *params)
{
	int ret = 0;

	/* EXTDINFO("hdmi_ioctl ioctl_cmd:%d\n", ioctl_cmd); */
	switch (ioctl_cmd) {
	case RECOMPUTE_BG_CMD:
		ret = hdmi_recompute_bg(param1, param2);
		break;
	case GET_DEV_TYPE_CMD:
		ret = hdmi_get_device_type();
		break;
	case SET_LAYER_NUM_CMD:
		hdmi_set_layer_num(param1);
		break;
	default:
		EXTDERR("%s unknown command\n", __fun__);
		break;
	}

	return ret;
}

void hdmi_udelay(unsigned int us)
{
	udelay(us);
}

void hdmi_mdelay(unsigned int ms)
{
	msleep(ms);
}

int hdmi_init(void)
{
	int ret = 0;
	struct device_node *node;
	const char interface_type[10];
	const char *type = interface_type;
	const char project_type[10];
	const char *p_project_type = project_type;

	EXTDMSG("%s start\n", __func__);
	/* for support hdmi hotplug, inform AP the event */
	hdmi_switch_data.name = "hdmi";
	hdmi_switch_data.index = 0;
	hdmi_switch_data.state = HDMI_STATE_NO_DEVICE;
	ret = switch_dev_register(&hdmi_switch_data);

	if (ret)
		EXTDERR
		    ("[hdmi][HDMI]switch_dev_register failed, returned:%d!\n",
		     ret);

	hdmires_switch_data.name = "res_hdmi";
	hdmires_switch_data.index = 0;
	hdmires_switch_data.state = 0;
	ret = switch_dev_register(&hdmires_switch_data);

	if (ret)
		EXTDERR
		    ("[hdmi][HDMI]switch_dev_register failed, returned:%d!\n",
		     ret);

	node = of_find_compatible_node(NULL, NULL, "mediatek,extd_dev");
	if (!node)
		EXTDERR("Failed to find device node mediatek,extd_dev\n");

	/* Get interface_type */
	if (node)
		of_property_read_string(node, "interface_type", &type);

	EXTDMSG("interface_type is %s\n", type);
	if (!strncmp(type, "DPI", 3)) {
		EXTDMSG("interface_type is DPI\n");
		dst_is_dsi = 0;
	} else if (!strncmp(type, "DSI", 3)) {
		EXTDMSG("interface_type is DSI\n");
		dst_is_dsi = 1;
	}

	/* Get project type */
	if (node)
		of_property_read_string(node, "project_type", &p_project_type);

	EXTDMSG("project_type is %s\n", p_project_type);
	if (!strncmp(p_project_type, "BSP", 3)) {
		EXTDMSG("project_type is BSP\n");
		project_is_bsp = 1;
	} else {
		EXTDMSG("project_type is not BSP\n");
		project_is_bsp = 0;
	}

	EXTDINFO("%s done\n", __func__);
	return 0;
}

int hdmi_post_init(void)
{
	int boot_mode = 0;
	const struct EXTD_DRIVER *extd_factory_driver = NULL;

	static const struct EXTERNAL_DISPLAY_UTIL_FUNCS extd_utils = {
		.hdmi_video_format_config = hdmi_video_format_config,
	};

	static const struct HDMI_UTIL_FUNCS hdmi_utils = {
		.udelay = hdmi_udelay,
		.mdelay = hdmi_mdelay,
		.state_callback = hdmi_state_callback,
	};

	EXTDFUNC();
	hdmi_drv = (struct HDMI_DRIVER *)HDMI_GetDriver();
	if (hdmi_drv == NULL) {
		EXTDERR("[hdmi]%s, hdmi_init fail\n", __func__);
		return -1;
	}

	hdmi_drv->set_util_funcs(&hdmi_utils);

	extd_disp_drv_set_util_funcs(&extd_utils);

	hdmi_params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
	hdmi_drv->get_params(hdmi_params);

	hdmi_drv->init();
	if (hdmi_drv->register_callback != NULL) {
		boot_mode = (int)get_boot_mode();
		if (boot_mode == FACTORY_BOOT ||
				boot_mode == ATE_FACTORY_BOOT) {
			extd_factory_driver = EXTD_Factory_HDMI_Driver();
			if (extd_factory_driver)
				extd_factory_driver->init();
		} else

			hdmi_drv->register_callback(hdmi_state_callback);
	}

	memset((void *)&hdmi_context, 0, sizeof(struct _t_hdmi_context));
	memset((void *)&extd_dpi_params, 0, sizeof(extd_dpi_params));

	p->output_mode = hdmi_params->output_mode;

	SET_HDMI_OFF();

	init_waitqueue_head(&hdmi_3d_config_wq);

#if defined(CONFIG_MTK_DCS)
	init_waitqueue_head(&dcs_switch_to_4ch_wq);
	init_waitqueue_head(&hdmi_video_config_wq);
#endif

	Extd_DBG_Init();
	if (project_is_bsp == 1) {
		EXTDMSG("hdmi enable for BSP\n");
		hdmi_enable(1);
	}

	EXTDINFO("%s done\n", __func__);
	return 0;
}
#endif

const struct EXTD_DRIVER *EXTD_HDMI_Driver(void)
{
	static const struct EXTD_DRIVER extd_driver_hdmi = {
#if defined(CONFIG_MTK_HDMI_SUPPORT)
		.init = hdmi_init,
		.post_init = hdmi_post_init,
		.deinit = NULL,
		.enable = hdmi_enable,
		.power_enable = hdmi_power_enable,
		.set_audio_enable = hdmi_set_audio_enable,
		.set_audio_format = hdmi_audio_config,
		.set_resolution = hdmi_set_resolution,
		.get_dev_info = hdmi_get_dev_info,
		.get_capability = hdmi_get_capability,
		.get_edid = hdmi_get_edid,
		.wait_vsync = NULL,
		.fake_connect = hdmi_cable_fake_connect,
		.factory_mode_test = NULL,
		.ioctl = hdmi_ioctl,
#else
		.init = 0,
#endif
	};

	return &extd_driver_hdmi;
}
