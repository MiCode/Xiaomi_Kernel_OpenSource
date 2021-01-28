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

#include "extd_info.h"

#if defined(CONFIG_MTK_HDMI_SUPPORT)
#define _tx_c_
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
/* #include <linux/rtpm_prio.h> */
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
/*#include <linux/switch.h>*/
#include <linux/extcon.h>

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/string.h>

/* #include <mach/irqs.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/* #include "mach/irqs.h" */

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif

#include "mt-plat/mtk_boot_common.h"
/* #include "mt-plat/mt_boot.h" */
#include "mtkfb_info.h"
#include "mtkfb.h"

#include "mtkfb_fence.h"
#include "display_recorder.h"

#include "ddp_info.h"
#include "ddp_irq.h"
#include "ddp_mmp.h"
#include <uapi/linux/sched/types.h>

#include "disp_session.h"

#include "extd_platform.h"
#include "extd_hdmi.h"
#include "extd_factory.h"
#include "extd_log.h"
#include "extd_utils.h"
#include "extd_hdmi_types.h"
#include "external_display.h"

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#  include <linux/sbsuspend.h>
#  include "smartbook.h"
#endif

#ifdef I2C_DBG
#  include "tmbslHdmiTx_types.h"
#  include "tmbslTDA9989_local.h"
#endif

#ifdef MHL_DYNAMIC_VSYNC_OFFSET
#  include "ged_dvfs.h"
#endif

#if defined(CONFIG_MTK_DCS)
#  include "mt-plat/mtk_meminfo.h"
#endif

/* the static variable */
static atomic_t hdmi_fake_in = ATOMIC_INIT(false);

static int first_frame_done;
static int wait_vsync_enable;
static bool otg_enable_status;
static bool hdmi_vsync_flag;
static wait_queue_head_t hdmi_vsync_wq;
static unsigned long hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
static unsigned long force_reschange = 0xffff;
int enable_ut;
unsigned int dst_is_dsi;
/*
 *static struct switch_dev hdmi_notify_data;
 *static struct switch_dev hdmires_notify_data;
 */


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

DEFINE_SEMAPHORE(hdmi_update_mutex);

/* global variables */
#ifdef EXTD_DBG_USE_INNER_BUF
unsigned long hdmi_va, hdmi_mva_r;
#endif

struct HDMI_PARAMS _s_hdmi_params;
struct HDMI_PARAMS *hdmi_params = &_s_hdmi_params;

static int rdmafpscnt;

struct disp_ddp_path_config extd_dpi_params;

struct task_struct *hdmi_fence_release_task;
wait_queue_head_t hdmi_fence_release_wq;
atomic_t hdmi_fence_release_event = ATOMIC_INIT(0);

struct task_struct *hdmi_wait_vsync_task;

/* definition */
#define IS_HDMI_ON()	(atomic_read(&p->state) == HDMI_POWER_STATE_ON)
#define IS_HDMI_OFF()	(atomic_read(&p->state) == HDMI_POWER_STATE_OFF)
#define IS_HDMI_STANDBY() (atomic_read(&p->state) == HDMI_POWER_STATE_STANDBY)

#define IS_HDMI_NOT_ON() (atomic_read(&p->state) != HDMI_POWER_STATE_ON)
#define IS_HDMI_NOT_OFF() (atomic_read(&p->state) != HDMI_POWER_STATE_OFF)
#define IS_HDMI_NOT_STANDBY()		\
	(atomic_read(&p->state) != HDMI_POWER_STATE_STANDBY)

#define SET_HDMI_ON()	atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()	atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY() atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)


#define IS_HDMI_FAKE_PLUG_IN()	(atomic_read(&hdmi_fake_in) == true)
#define SET_HDMI_FAKE_PLUG_IN()	(atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()	(atomic_set(&hdmi_fake_in, false))

#define MHL_SESSION_ID		(0x20001)

/* extern declare */
/* extern unsigned char kara_1280x720[2764800]; */

/* Information Dump Routines */
/*~~~~~~~~~~~~~~~~~~kernel-4.9 define switch dev~~~~~~~~~~~~~~~~~~ */
struct notify_dev {
	const char	*name;
	struct device *dev;
	int index;
	int state;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
};


struct notify_dev hdmi_notify_data;
struct notify_dev hdmires_notify_data;
struct class *switch_class;
static atomic_t device_count;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR(state, 0444, state_show, NULL);
static DEVICE_ATTR(name, 0444, name_show, NULL);

static int create_switch_class(void)
{
	if (!switch_class) {
		switch_class = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_class))
			return PTR_ERR(switch_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}


int hdmi_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_class) {
		ret = create_switch_class();
		if (ret == 0) {
			HDMI_LOG("[hdmi]w-y create_switch_class susesess\n");
		} else {
			HDMI_LOG("[hdmi]w-y create_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count);
	HDMI_LOG("[hdmi]w-y index:0x%x\n", sdev->index);

	sdev->dev = device_create(switch_class, NULL,
		 MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		HDMI_LOG("[hdmi]w-y device create ok,index:0x%x\n",
			sdev->index);
		ret = 0;
	} else {
		HDMI_LOG("[hdmi]w-y device create fail,index:0x%x\n",
			sdev->index);
		ret = -1;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_class, MKDEV(0, sdev->index));
		HDMI_LOG("switch: Failed to register driver %s\n",
			sdev->name);
	}
	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		HDMI_LOG("switch: Failed to register driver %s\n",
			sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;

}

int notify_uevent_user(struct notify_dev *sdev, int state)
{
	char *envp[3];
	char name_buf[120];
	char state_buf[120];

	if (sdev == NULL)
		return -1;

	HDMI_LOG("[hdmi]w-y notify state:0x%x\n", state);

	if (sdev->state != state)
		sdev->state = state;

	snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	envp[0] = name_buf;
	snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	envp[1] = state_buf;
	envp[2] = NULL;
	HDMI_LOG("[hdmi]w-y name:%s ,state:%s\n", envp[0], envp[1]);
	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	HDMI_LOG("[hdmi]w-y kobj_name:%s\n", sdev->dev->kobj.name);
	HDMI_LOG("[hdmi]w-y init:0x%x\n", sdev->dev->kobj.state_initialized);
	HDMI_LOG("[hdmi]w-y sysfs:0x%x\n",
		sdev->dev->kobj.state_in_sysfs);
	HDMI_LOG("[hdmi]w-y uevent sent:0x%x\n",
		sdev->dev->kobj.state_add_uevent_sent);
	HDMI_LOG("[hdmi]w-y remove sent:0x%x\n",
		sdev->dev->kobj.state_remove_uevent_sent);
	HDMI_LOG("[hdmi]w-y uevent suppress:0x%x\n",
		sdev->dev->kobj.uevent_suppress);

	return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


void hdmi_force_on(int from_uart_drv)
{
}

/* params & 0xff: resolution, params & 0xff00 : 3d support */
void hdmi_force_resolution(int params)
{
	force_reschange = params;
	if ((force_reschange > 0xff) && (force_reschange < 0x0fff))
		hdmi_params->is_3d_support = 1;
	else
		hdmi_params->is_3d_support = 0;

	/*HDMI_LOG("%s params:0x%lx, 3d:%d\n", __func__, force_reschange,
	 *	 hdmi_params->is_3d_support);
	 */
}

/*#ifdef MM_MHL_DVFS
 *#include "mmdvfs_mgr.h"
 *static void hdmi_enable_dvfs(int enable)
 *{
 *	mmdvfs_mhl_enable(enable);
 *}
 *#else
 *static void hdmi_enable_dvfs(int enable)
 *{
 *}
 *#endif
 */

/* for debug */
void hdmi_cable_fake_plug_in(void)
{
	SET_HDMI_FAKE_PLUG_IN();
	HDMI_LOG("[HDMIFake]Cable Plug In\n");

	if (p->is_force_disable == true)
		return;

	if (!IS_HDMI_STANDBY())
		return;

#ifdef MHL_DYNAMIC_VSYNC_OFFSET
	/*ged_dvfs_vsync_offset_event_switch(GED_DVFS_VSYNC_OFFSET_MHL_EVENT,
	 *				   true);
	 */
#endif
	hdmi_resume();
	/*hdmi_enable_dvfs(true);*/
	/* msleep(1000); */
	hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
	/* notify_uevent_user(&hdmi_notify_data,
	 *HDMI_STATE_ACTIVE);
	 */
	extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_ACTIVE);
	notify_uevent_user(&hdmi_notify_data,
				HDMI_STATE_ACTIVE);
}

void hdmi_cable_fake_plug_out(void)
{
	SET_HDMI_NOT_FAKE();
	HDMI_LOG("[HDMIFake]Disable\n");

	if (p->is_force_disable == false && IS_HDMI_ON()) {
		if ((hdmi_drv->get_state() != HDMI_STATE_ACTIVE) ||
		    (enable_ut == 1)) {
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
			/*ged_dvfs_vsync_offset_event_switch(
			 *	GED_DVFS_VSYNC_OFFSET_MHL_EVENT, false);
			 */
#endif
			hdmi_suspend();
			/*hdmi_enable_dvfs(false);*/
				/* notify_uevent_user(&hdmi_notify_data,
				 *		 HDMI_STATE_NO_DEVICE);
				 *notify_uevent_user(&hdmires_notify_data, 0);
				 */
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmires_notify_data, 0);
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

int hdmi_allocate_hdmi_buffer(void)
{
	return 0;
}

int hdmi_free_hdmi_buffer(void)
{
	return 0;
}

static int hdmi_wait_vsync_kthread(void *data)
{
	struct disp_session_vsync_config vsync_config;

	struct sched_param param = { .sched_priority = 94 };

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

	if (enable && hdmi_wait_vsync_task == NULL) {
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

	if (hdmi_drv && hdmi_drv->dump)
		hdmi_drv->dump();
	return ret;
}

bool is_hdmi_active(void)
{
	bool active = IS_HDMI_ON() && p->is_clock_on;

	return active;
}

unsigned int hdmi_get_width(void)
{
	return p->hdmi_width;
}

unsigned int hdmi_get_height(void)
{
	return p->hdmi_height;
}

int hdmi_waitVsync(void)
{
	unsigned int session_id = ext_disp_get_sess_id();
	struct disp_session_sync_info *session_info =
			disp_get_session_sync_info_for_debug(session_id);

	if (session_info)
		dprec_start(&session_info->event_waitvsync, 0, 0);

	if (p->is_clock_on == false) {
		HDMI_ERR("[hdmi]:hdmi has suspend, return directly\n");
		msleep(20);
		return 0;
	}

	hdmi_vsync_flag = 0;

	if (wait_event_interruptible_timeout(hdmi_vsync_wq, hdmi_vsync_flag,
					     HZ / 10) == 0)
		HDMI_ERR("[hdmi] Wait VSync timeout. early_suspend=%d\n",
			 p->is_clock_on);

	if (session_info)
		dprec_done(&session_info->event_waitvsync, 1, 0);

	return 0;
}

int hdmi_get_support_info(void)
{
	int value = 0, temp = 0;

#ifdef USING_SCALE_ADJUSTMENT
	value |= HDMI_SCALE_ADJUSTMENT_SUPPORT;
#endif

#ifdef MHL_PHONE_GPIO_REUSAGE
	value |= HDMI_PHONE_GPIO_REUSAGE;
#endif

#ifdef MTK_AUDIO_MULTI_CHANNEL_SUPPORT
	//temp = hdmi_drv->get_external_device_capablity();
	temp = 0x2 << 3;
#else
	temp = 0x2 << 3;
#endif

	value |= temp;
	value |= HDMI_FACTORY_MODE_NEW;

	HDMI_LOG("value is 0x%x\n", value);
	return value;
}

/* Configure video attribute */
int hdmi_video_config(enum HDMI_VIDEO_RESOLUTION vformat,
		      enum HDMI_VIDEO_INPUT_FORMAT vin,
		      enum HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	if (p->is_mhl_video_on == true && (p->vout == vout))
		return 0;

	HDMI_LOG("%s video_on=%d\n", __func__, p->is_mhl_video_on);
	if (IS_HDMI_NOT_ON()) {
		HDMI_LOG("return in %d\n", __LINE__);
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
	if (vformat == HDMI_VIDEO_2160p_DSC_30Hz ||
	    vformat == HDMI_VIDEO_2160p_DSC_24Hz) {
		/* wait DCS switch to 4ch */
		wait_event_interruptible(hdmi_video_config_wq,
					 atomic_read
					 (&dcs_4ch_switch_done_event));
		atomic_set(&dcs_4ch_switch_done_event, 0);
		HDMI_LOG("%s wait DCS switch to 4ch done\n", __func__);
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

	HDMI_LOG("channel_count: %d, sampleRate: %d, bitWidth: %d\n",
		 channel_count, sampleRate, bitWidth);

	if (bitWidth == HDMI_MAX_BITWIDTH_16) {
		sampleBit = 16;
		HDMI_LOG("HDMI_MAX_BITWIDTH_16\n");
	} else if (bitWidth == HDMI_MAX_BITWIDTH_24) {
		sampleBit = 24;
		HDMI_LOG("HDMI_MAX_BITWIDTH_24\n");
	}

	if (channel_count == HDMI_MAX_CHANNEL_2 &&
	    sampleRate == HDMI_MAX_SAMPLERATE_44) {
		audio_format = HDMI_AUDIO_44K_2CH;
		HDMI_LOG("AUDIO_44K_2CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_2 &&
		   sampleRate == HDMI_MAX_SAMPLERATE_48) {
		audio_format = HDMI_AUDIO_48K_2CH;
		HDMI_LOG("AUDIO_48K_2CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8 &&
		   sampleRate == HDMI_MAX_SAMPLERATE_32) {
		audio_format = HDMI_AUDIO_32K_8CH;
		HDMI_LOG("AUDIO_32K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8 &&
		   sampleRate == HDMI_MAX_SAMPLERATE_44) {
		audio_format = HDMI_AUDIO_44K_8CH;
		HDMI_LOG("AUDIO_44K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8 &&
		   sampleRate == HDMI_MAX_SAMPLERATE_48) {
		audio_format = HDMI_AUDIO_48K_8CH;
		HDMI_LOG("AUDIO_48K_8CH\n");
	} else if (channel_count == HDMI_MAX_CHANNEL_8 &&
		   sampleRate == HDMI_MAX_SAMPLERATE_96) {
		audio_format = HDMI_AUDIO_96K_8CH;
		HDMI_LOG("AUDIO_96K_8CH\n");
	}

	else if (channel_count == HDMI_MAX_CHANNEL_8 &&
		 sampleRate == HDMI_MAX_SAMPLERATE_192) {
		audio_format = HDMI_AUDIO_192K_8CH;
		HDMI_LOG("AUDIO_192K_8CH\n");
	} else {
		HDMI_LOG("audio format is not supported\n");
	}

	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return 0;
	}

	hdmi_drv->audio_config(audio_format, sampleBit);

	return 0;
}

static void _hdmi_rdma_irq_handler(enum DISP_MODULE_ENUM module,
				   unsigned int param)
{
	if (!is_hdmi_active())
		return;

	if (param & 0x2) { /* start */
		atomic_set(&hdmi_fence_release_event, 1);
		wake_up_interruptible(&hdmi_fence_release_wq);

		if (hdmi_params->cabletype == MHL_SMB_CABLE) {
			hdmi_vsync_flag = 1;
			wake_up_interruptible(&hdmi_vsync_wq);
		}
	}

	/* frame done */
	if (param & 0x4 && first_frame_done == 0)
		first_frame_done = 1;
}

static int hdmi_fence_release_kthread(void *data)
{
	struct sched_param param = { .sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);
	for (;;) {
		wait_event_interruptible(hdmi_fence_release_wq,
					 atomic_read
					 (&hdmi_fence_release_event));
		atomic_set(&hdmi_fence_release_event, 0);

		rdmafpscnt++;
		if (kthread_should_stop())
			break;
	}

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
	struct sched_param param = { .sched_priority = 94 };
	enum HDMI_VIDEO_RESOLUTION vformat = HDMI_VIDEO_RESOLUTION_NUM;

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_3d_config_wq,
					 atomic_read(&hdmi_3d_config_event));
		atomic_set(&hdmi_3d_config_event, 0);

		HDMI_LOG("video_on=%d fps %d, %d, %d\n", p->is_mhl_video_on,
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
	struct sched_param param = { .sched_priority = 94 };
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
			ret = dcs_get_dcs_status_lock(&dcs_channel,
						      &dcs_status);
			if (dcs_channel == 4 && dcs_status == 0) {
				HDMI_LOG("dcs switch success! channel=%d\n",
					 dcs_channel);
				dcs_get_dcs_status_unlock();
				atomic_set(&dcs_4ch_switch_done_event, 1);
				wake_up_interruptible(&hdmi_video_config_wq);
				goto done;
			}
			dcs_get_dcs_status_unlock();
		}
		/* dcs switch fail */
		HDMI_ERR("ERROR! dsc switch fail! channel=%d dcs_status=%d\n",
			 dcs_channel, dcs_status);

done:
		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static enum HDMI_STATUS hdmi_drv_init(void)
{
	enum HDMI_VIDEO_RESOLUTION vfmt;

	HDMI_FUNC();

	vfmt = hdmi_params->init_config.vformat;
	p->hdmi_width = hdmi_resolution_param_table[vfmt][0];
	p->hdmi_height = hdmi_resolution_param_table[vfmt][1];
	p->bg_width = 0;
	p->bg_height = 0;

	p->output_video_resolution = vfmt;
	p->output_audio_format = hdmi_params->init_config.aformat;
	p->scaling_factor = hdmi_params->scaling_factor < 10 ?
				hdmi_params->scaling_factor : 10;

	p->is_clock_on = false;	/* <--Donglei */

	if (!hdmi_fence_release_task) {
		disp_register_module_irq_callback(DISP_MODULE_RDMA,
						  _hdmi_rdma_irq_handler);
		hdmi_fence_release_task =
				kthread_create(hdmi_fence_release_kthread, NULL,
					       "hdmi_fence_release_kthread");
		wake_up_process(hdmi_fence_release_task);
	}

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
	HDMI_FUNC();
	hdmi_free_hdmi_buffer();

	return HDMI_STATUS_OK;
}

/* Reset HDMI Driver state */
static void hdmi_state_reset(void)
{
	HDMI_FUNC();

	if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
		if (enable_ut != 1) {
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_ACTIVE);
			notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_ACTIVE);
		}
		/*hdmi_enable_dvfs(true);*/
		hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
	} else {
		if (enable_ut != 1) {
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmi_notify_data,
					 HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmires_notify_data, 0);
		}
	/*hdmi_enable_dvfs(false);*/
	}
}

/* static */ void hdmi_suspend(void)
{
	int session_id = 0;

	HDMI_FUNC();
	if (IS_HDMI_NOT_ON())
		return;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 Plugout, 0);

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_ERR("[hdmi][HDMI] can't get semaphore in %s()\n",
			 __func__);
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

	first_frame_done = 0;
	rdmafpscnt = 0;
	up(&hdmi_update_mutex);

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 Plugout, 0);
}

/* static */ void hdmi_resume(void)
{
	HDMI_LOG("p->state is %d,(0:off, 1:on, 2:standby)\n",
		 atomic_read(&p->state));

	if (IS_HDMI_NOT_STANDBY())
		return;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 Plugin, 0);

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_ERR("[hdmi][HDMI] can't get semaphore in %s()\n",
			 __func__);
		return;
	}

	SET_HDMI_ON();
	p->is_clock_on = true;
	hdmi_drv->resume();
	up(&hdmi_update_mutex);

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 Plugin, 0);
}

void hdmi_power_on(void)
{
	HDMI_FUNC();

	if (IS_HDMI_NOT_OFF())
		return;

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_ERR("[hdmi][HDMI] can't get semaphore in %s()\n",
			 __func__);
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
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_ACTIVE);
			notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_ACTIVE);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		} else {
			/* this is just a ugly workaround for some tv sets.. */
			if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
				hdmi_drv->get_params(hdmi_params);
				hdmi_resume();
			}

			hdmi_state_reset();
		}
	}
}

void hdmi_power_off(void)
{
	HDMI_FUNC();
	if (IS_HDMI_OFF())
		return;

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_ERR("[hdmi][HDMI] can't get semaphore in %s()\n",
			 __func__);
		return;
	}

	hdmi_drv->power_off();
	ext_disp_suspend(MHL_SESSION_ID);
	p->is_clock_on = false;
	SET_HDMI_OFF();
	up(&hdmi_update_mutex);

	notify_uevent_user(&hdmires_notify_data, 0);
	/*hdmi_enable_dvfs(false);*/

#if defined(CONFIG_MTK_DCS)
	/* Nortify DCS can switch to 2ch */
	dcs_exit_perf(DCS_KICKER_MHL);
#endif
}

static void hdmi_resolution_setting(int arg)
{
	HDMI_FUNC();
	extd_dpi_params.dispif_config.dpi.dsc_enable = 0;
	if (hdmi_drv && hdmi_drv->get_params) {
		hdmi_params->init_config.vformat = arg;
		hdmi_drv->get_params(hdmi_params);

		if (dst_is_dsi) {
			memcpy(&extd_dpi_params.dispif_config.dsi,
			    (void *)(&hdmi_params->dsi_params),
			      sizeof(struct LCM_DSI_PARAMS));

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

			if ((arg == HDMI_VIDEO_2160p_DSC_24Hz) ||
			    (arg == HDMI_VIDEO_2160p_DSC_30Hz)) {
				struct LCM_DSI_PARAMS *dsi_ctx;
				struct LCM_DSC_CONFIG_PARAMS *dsc_cfg;

				dsi_ctx =
					&(extd_dpi_params.dispif_config.dsi);
				dsc_cfg = &dsi_ctx->dsc_params;
				memset(dsc_cfg, 0,
					sizeof(struct LCM_DSC_CONFIG_PARAMS));

				dsi_ctx->dsc_enable = 1;
				/* width/(slice_mode's slice) */
				dsc_cfg->slice_width = 1920;
				dsc_cfg->slice_hight = 8;
				/* 128: 1/3 compress; 192: 1/2 compress */
				dsc_cfg->bit_per_pixel = 128;
				/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
				dsc_cfg->slice_mode = 1;
				dsc_cfg->rgb_swap = 0;
				dsc_cfg->xmit_delay = 0x200;
				dsc_cfg->dec_delay = 0x4c0;
				dsc_cfg->scale_value = 0x20;
				dsc_cfg->increment_interval = 0x11e;
				dsc_cfg->decrement_interval = 0x1a;
				dsc_cfg->nfl_bpg_offset = 0xdb7;
				dsc_cfg->slice_bpg_offset = 0x394;
				dsc_cfg->final_offset = 0x10f0;
				dsc_cfg->line_bpg_offset = 0xc;
				dsc_cfg->bp_enable = 0x0;
				dsc_cfg->rct_on = 0x0;

			}
		} else {
			extd_dpi_params.dispif_config.dpi.clk_pol =
			    (enum LCM_POLARITY)hdmi_params->clk_pol;
			extd_dpi_params.dispif_config.dpi.de_pol =
			    (enum LCM_POLARITY)hdmi_params->de_pol;
			extd_dpi_params.dispif_config.dpi.hsync_pol =
			    (enum LCM_POLARITY)hdmi_params->hsync_pol;
			extd_dpi_params.dispif_config.dpi.vsync_pol =
			    (enum LCM_POLARITY)hdmi_params->vsync_pol;

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
				struct LCM_DPI_PARAMS *dpi_ctx;
				struct LCM_DSC_CONFIG_PARAMS *dsc_cfg;

				dpi_ctx =
					&(extd_dpi_params.dispif_config.dpi);
				dsc_cfg = &dpi_ctx->dsc_params;
				memset(dsc_cfg, 0,
					sizeof(struct LCM_DSC_CONFIG_PARAMS));

				dpi_ctx->dsc_enable = 1;
				dpi_ctx->width = p->hdmi_width / 3;
				/* width/(slice_mode's slice) */
				dsc_cfg->slice_width = 1920;
				dsc_cfg->slice_hight = 8;
				/* 128: 1/3 compress; 192: 1/2 compress */
				dsc_cfg->bit_per_pixel = 128;
				/* 0: 1 slice; 1: 2 slice; 2: 3 slice */
				dsc_cfg->slice_mode = 1;
				dsc_cfg->rgb_swap = 0;
				dsc_cfg->xmit_delay = 0x200;
				dsc_cfg->dec_delay = 0x4c0;
				dsc_cfg->scale_value = 0x20;
				dsc_cfg->increment_interval = 0x11e;
				dsc_cfg->decrement_interval = 0x1a;
				dsc_cfg->nfl_bpg_offset = 0xdb7;
				dsc_cfg->slice_bpg_offset = 0x394;
				dsc_cfg->final_offset = 0x10f0;
				dsc_cfg->line_bpg_offset = 0xc;
				dsc_cfg->bp_enable = 0x0;
				dsc_cfg->rct_on = 0x0;
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
	HDMI_LOG("hdmi_resolution_setting_res (%d)\n", arg);
}

int hdmi_check_resolution(int src_w, int src_h, int physical_w, int physical_h)
{
	int ret = 0;

	if (physical_w <= 0 || physical_h <= 0 || src_w > physical_w ||
	    src_h > physical_h) {
		HDMI_LOG("%s fail\n", __func__);
		ret = -1;
	}

	return ret;
}

int hdmi_recompute_bg(int src_w, int src_h)
{
	int ret = 0;

	return ret;
}

/* HDMI Driver state callback function */
void hdmi_state_callback(enum HDMI_STATE state)
{
	HDMI_LOG("[hdmi]%s, state = %d\n", __func__, state);
	if ((p->is_force_disable == true) || (IS_HDMI_FAKE_PLUG_IN()))
		return;

	switch (state) {
	case HDMI_STATE_NO_DEVICE:
	{
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
		/*ged_dvfs_vsync_offset_event_switch(
		 *		GED_DVFS_VSYNC_OFFSET_MHL_EVENT, false);
		 */
#endif
		hdmi_suspend();
		extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
		notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_NO_DEVICE);
		notify_uevent_user(&hdmires_notify_data, 0);
		/*hdmi_enable_dvfs(false);*/

#if defined(CONFIG_MTK_SMARTBOOK_SUPPORT) && defined(CONFIG_HAS_SBSUSPEND)
		if (hdmi_params->cabletype == MHL_SMB_CABLE)
			sb_plug_out();
#endif
		HDMI_LOG("[hdmi]%s, state = %d out!\n", __func__, state);
		break;
	}
	case HDMI_STATE_ACTIVE:
	{
		if (IS_HDMI_ON()) {
			HDMI_LOG("[hdmi]%s, already on(%d) !\n",
				 __func__, atomic_read(&p->state));
			break;
		}

		hdmi_drv->get_params(hdmi_params);
		hdmi_resume();

		if (atomic_read(&p->state) > HDMI_POWER_STATE_OFF) {
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_ACTIVE);
			notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_ACTIVE);
			/*hdmi_enable_dvfs(true);*/
		}
#if defined(CONFIG_MTK_SMARTBOOK_SUPPORT) && defined(CONFIG_HAS_SBSUSPEND)
		if (hdmi_params->cabletype == MHL_SMB_CABLE)
			sb_plug_in();
#endif
#ifdef MHL_DYNAMIC_VSYNC_OFFSET
		/*ged_dvfs_vsync_offset_event_switch(
		 *		GED_DVFS_VSYNC_OFFSET_MHL_EVENT, true);
		 */
#endif
		HDMI_LOG("[hdmi]%s, state = %d out!\n", __func__, state);
		break;
	}
	default:
	{
		HDMI_LOG("[hdmi]%s, state not support\n", __func__);
		break;
	}
	}
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
	HDMI_FUNC();
	if (enable) {
		if (p->is_enabled) {
			HDMI_LOG("[hdmi] hdmi already enable, %s()\n",
				 __func__);
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
		extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
		notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_NO_DEVICE);

		p->is_enabled = false;

		if (hdmi_drv->exit)
			hdmi_drv->exit();
	}

	return 0;
}

int hdmi_power_enable(int enable)
{
	HDMI_FUNC();
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return 0;
	}

	if (enable) {
		if (otg_enable_status) {
			HDMI_LOG("return in %d\n", __LINE__);
			return 0;
		}
		hdmi_power_on();
	} else {
		hdmi_power_off();
		extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
		notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_NO_DEVICE);
	}

	return 0;
}

void hdmi_force_disable(int enable)
{
	HDMI_FUNC();
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}
	if (IS_HDMI_OFF()) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	if (enable) {
		if (p->is_force_disable == true)
			return;

		if (IS_HDMI_FAKE_PLUG_IN() ||
		    (hdmi_drv->get_state() == HDMI_STATE_ACTIVE)) {
			hdmi_suspend();
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmi_notify_data,
					 HDMI_STATE_NO_DEVICE);
			notify_uevent_user(&hdmires_notify_data, 0);
		}

		p->is_force_disable = true;
	} else {
		if (p->is_force_disable == false)
			return;

		if (IS_HDMI_FAKE_PLUG_IN() ||
		    (hdmi_drv->get_state() == HDMI_STATE_ACTIVE)) {
			hdmi_resume();
			msleep(1000);
			extcon_set_state_sync(hdmi_extcon,
				EXTCON_DISP_HDMI, HDMI_STATE_ACTIVE);
			notify_uevent_user(&hdmi_notify_data,
					HDMI_STATE_ACTIVE);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		}

		p->is_force_disable = false;
	}
}

void hdmi_set_USBOTG_status(int status)
{
	HDMI_LOG("MTK_HDMI_USBOTG_STATUS, arg=%d, enable %d\n", status,
		 p->is_enabled);
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}
	if (hdmi_params->cabletype != MHL_CABLE) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	if (status) {
		otg_enable_status = true;
	} else {
		otg_enable_status = false;
		if (p->is_force_disable) {
			HDMI_LOG("return in %d\n", __LINE__);
			return;
		}
		hdmi_power_on();
	}
}

int hdmi_set_audio_enable(int enable)
{
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return 0;
	}

	hdmi_drv->audio_enable(enable);
	return 0;
}

void hdmi_set_video_enable(int enable)
{
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	hdmi_drv->video_enable(enable);
}

int hdmi_set_resolution(int res)
{
	int extd_path_state = 0;
	int i = 0;
	int session_id = 0;

	HDMI_LOG("video res config, res:%d, old res:%ld, video_on:%d\n",
		 res, hdmi_reschange, p->is_mhl_video_on);
	if (!p->is_enabled) {
		HDMI_LOG("return in %d\n", __LINE__);
		return 0;
	}

	/* just for debug */
	if ((force_reschange & 0xff) < 0xff)
		res = force_reschange & 0xff;

	if (hdmi_reschange == res) {
		HDMI_LOG("hdmi_reschange=%ld\n", hdmi_reschange);
		return 0;
	}
#if defined(CONFIG_MTK_DCS)
	if (res == HDMI_VIDEO_2160p_DSC_30Hz ||
	    res == HDMI_VIDEO_2160p_DSC_24Hz) {
		/* Notify the DCS switch to 4ch */
		dcs_enter_perf(DCS_KICKER_MHL);
		atomic_set(&dcs_4ch_switch_event, 1);
		HDMI_LOG("Notify the DCS switch to 4ch\n");
		/* wake up dcs_switch_to_4ch_kthread for wait switch done */
		wake_up_interruptible(&dcs_switch_to_4ch_wq);
	}
#endif

	p->is_clock_on = false;
	hdmi_reschange = res;

	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_START,
			 ResChange, res);

	if (down_interruptible(&hdmi_update_mutex)) {
		HDMI_LOG("[HDMI] can't get semaphore in\n");
		return 0;
	}

	extd_path_state = ext_disp_is_alive();

	if (extd_path_state == EXTD_RESUME) {
		if (hdmi_drv && hdmi_drv->suspend)
			hdmi_drv->suspend();
		ext_disp_suspend(MHL_SESSION_ID);

		session_id = ext_disp_get_sess_id();

		for (i = 0; i < EXTD_OVERLAY_CNT; i++)
			mtkfb_release_layer_fence(session_id, i);
	}

	hdmi_resolution_setting(res);
	p->is_mhl_video_on = false;
	first_frame_done = 0;
	rdmafpscnt = 0;

	up(&hdmi_update_mutex);
	if (enable_ut != 1)
		notify_uevent_user(&hdmires_notify_data,
					hdmi_reschange + 1);
	p->is_clock_on = true;
	mmprofile_log_ex(ddp_mmp_get_events()->Extd_State, MMPROFILE_FLAG_END,
			 ResChange, hdmi_reschange + 1);

	return 0;
}

int hdmi_get_dev_info(int is_sf, void *info)
{
	int ret = 0;

	if (is_sf == AP_GET_INFO) {
		int displayid = 0;
		struct mtk_dispif_info hdmi_info;

		if (!info) {
			HDMI_LOG("ioctl pointer is NULL\n");
			return -EFAULT;
		}

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_DevInfo,
				 MMPROFILE_FLAG_START, p->is_enabled,
				 p->is_clock_on);

		if (copy_from_user(&displayid, info, sizeof(displayid))) {
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
					 MMPROFILE_FLAG_PULSE, Devinfo, 0);
			HDMI_ERR(": copy_from_user failed! line:%d\n",
				 __LINE__);
			return -EAGAIN;
		}

		if (displayid != MTKFB_DISPIF_HDMI)
			HDMI_LOG(": invalid display id:%d\n", displayid);

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

		if ((hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz) ||
		    (hdmi_reschange == HDMI_VIDEO_2160p_DSC_30Hz))
			hdmi_info.vsyncFPS = 3000;
		else if (hdmi_reschange == HDMI_VIDEO_2160p_DSC_24Hz)
			hdmi_info.vsyncFPS = 2400;
		else
			hdmi_info.vsyncFPS = 6000;

		if (copy_to_user(info, &hdmi_info, sizeof(hdmi_info))) {
			mmprofile_log_ex(ddp_mmp_get_events()->Extd_ErrorInfo,
					 MMPROFILE_FLAG_PULSE, Devinfo, 1);
			HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		}

		mmprofile_log_ex(ddp_mmp_get_events()->Extd_DevInfo,
				 MMPROFILE_FLAG_END, p->is_enabled,
				 hdmi_info.displayType);
		HDMI_LOG("DEV_INFO configuration get displayType-%d\n",
			 hdmi_info.displayType);
	} else if (is_sf == SF_GET_INFO) {
		struct disp_session_info *dispif_info = NULL;

		dispif_info = (struct disp_session_info *)info;
		memset((void *)dispif_info, 0, sizeof(*dispif_info));

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

		if ((hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz) ||
		    (hdmi_reschange == HDMI_VIDEO_2160p_DSC_30Hz))
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

	query_type = hdmi_get_support_info();

	if (copy_to_user(info, &query_type, sizeof(query_type))) {
		HDMI_LOG(": copy_to_user error! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	HDMI_LOG("[hdmi][HDMI] query_type  done %x\n", query_type);

	return ret;
}

int hdmi_get_edid(void *edid_info)
{
	int ret = 0;
	struct _HDMI_EDID_T pv_get_info;

	memset(&pv_get_info, 0, sizeof(pv_get_info));

	if (!edid_info) {
		HDMI_LOG("ioctl pointer is NULL\n");
		return -EFAULT;
	}

	if (hdmi_drv->getedid) {
		hdmi_drv->getedid(&pv_get_info);

#ifdef MHL_RESOLUTION_LIMIT_720P_60
		pv_get_info.ui4_pal_resolution &= (~SINK_1080P60);
		pv_get_info.ui4_pal_resolution &= (~SINK_1080P30);
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

	if (copy_to_user(edid_info, &pv_get_info, sizeof(pv_get_info))) {
		HDMI_LOG("copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

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

	/* HDMI_LOG("hdmi_ioctl ioctl_cmd:%d\n", ioctl_cmd); */
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
		HDMI_LOG("%s unknown command\n", __func__);
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

	HDMI_LOG("%s start\n", __func__);
	/* for support hdmi hotplug, inform AP the event */
	hdmi_notify_data.name = "hdmi";
	hdmi_notify_data.index = 0;
	hdmi_notify_data.state = HDMI_STATE_NO_DEVICE;
	ret = hdmi_uevent_dev_register(&hdmi_notify_data);

	hdmires_notify_data.name = "res_hdmi";
	hdmires_notify_data.index = 0;
	hdmires_notify_data.state = 0;
	ret = hdmi_uevent_dev_register(&hdmires_notify_data);

	node = of_find_compatible_node(NULL, NULL, "mediatek,extd_dev");
	if (!node)
		EXT_MGR_ERR("Failed to find device node mediatek,extd_dev\n");
	else
		of_property_read_string(node, "interface_type", &type);

	EXT_MGR_LOG("interface_type is %s\n", type);
	if (!strncmp(type, "DPI", 3)) {
		EXT_MGR_LOG("interface_type is DPI\n");
		dst_is_dsi = 0;
	} else if (!strncmp(type, "DSI", 3)) {
		EXT_MGR_LOG("interface_type is DSI\n");
		dst_is_dsi = 1;
	}

	return 0;
}

int hdmi_post_init(void)
{
	int boot_mode = 0;
	const struct EXTD_DRIVER *extd_factory_driver = NULL;

	static const struct EXTERNAL_DISPLAY_UTIL_FUNCS	extd_utils = {
		.hdmi_video_format_config = hdmi_video_format_config,
	};

	static const struct HDMI_UTIL_FUNCS hdmi_utils = {
		.udelay = hdmi_udelay,
		.mdelay = hdmi_mdelay,
		.state_callback = hdmi_state_callback,
	};

	hdmi_drv = (struct HDMI_DRIVER *)HDMI_GetDriver();
	if (!hdmi_drv) {
		HDMI_ERR("[hdmi]%s, hdmi_init fail\n", __func__);
		return -1;
	}

	hdmi_drv->set_util_funcs(&hdmi_utils);

	extd_disp_drv_set_util_funcs(&extd_utils);

	hdmi_params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
	hdmi_drv->get_params(hdmi_params);

	hdmi_drv->init();
	if (hdmi_drv->register_callback) {
		boot_mode = (int)get_boot_mode();
		if (boot_mode == FACTORY_BOOT ||
		    boot_mode == ATE_FACTORY_BOOT) {
			extd_factory_driver = EXTD_Factory_HDMI_Driver();
			if (extd_factory_driver)
				extd_factory_driver->init();
		} else {
			hdmi_drv->register_callback(hdmi_state_callback);
		}
	}

	memset((void *)&hdmi_context, 0, sizeof(hdmi_context));
	memset((void *)&extd_dpi_params, 0, sizeof(extd_dpi_params));

	p->output_mode = hdmi_params->output_mode;

	SET_HDMI_OFF();

	init_waitqueue_head(&hdmi_fence_release_wq);
	init_waitqueue_head(&hdmi_vsync_wq);

	init_waitqueue_head(&hdmi_3d_config_wq);

#if defined(CONFIG_MTK_DCS)
	init_waitqueue_head(&dcs_switch_to_4ch_wq);
	init_waitqueue_head(&hdmi_video_config_wq);
#endif

	extd_dbg_init();
	return 0;
}
#endif /* CONFIG_MTK_HDMI_SUPPORT */

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
		.wait_vsync = hdmi_waitVsync,
		.fake_connect = hdmi_cable_fake_connect,
		.factory_mode_test = NULL,
		.ioctl = hdmi_ioctl,
#else
		.init = 0,
#endif
	};

	return &extd_driver_hdmi;
}
