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
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program;                                            */
/*                                                                           */
/*****************************************************************************/
#if defined(CONFIG_MTK_HDMI_SUPPORT)
#define TMFL_TDA19989

#define _tx_c_
/* /#include <linux/autoconf.h> */
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
/*#include <linux/earlysuspend.h>*/
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include "disp_assert_layer.h"
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>


#ifdef MTK_SMARTBOOK_SUPPORT
#ifdef CONFIG_HAS_SBSUSPEND
#include <linux/sbsuspend.h>
#endif
#endif
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <asm/cacheflush.h>
#include <linux/io.h>
/*#include <mach/dma.h>*/
/* #include <mach/irqs.h> */
#include <asm/tlbflush.h>
#include <asm/page.h>

#include "m4u.h"
#include <linux/types.h>
/*#include <mach/mt_reg_base.h> */
/* #include <mach/mt_clkmgr.h> */
#include <linux/clk.h>
#include "hdmitx.h"

#include <mt-plat/mt_boot.h>
/*#include "mach/eint.h" */
/* #include "mach/irqs.h" */

#include "mtkfb_info.h"
#include "mtkfb.h"
#include "disp_session.h"

#include "ddp_dpi.h"
#include "ddp_info.h"
#include "ddp_rdma.h"
#include "ddp_irq.h"
#include "ddp_mmp.h"

#include "extd_platform.h"
/*#include "extd_drv.h"*/
#include "extd_kernel_drv.h"
#include "extd_drv_log.h"
#include "extd_utils.h"
#include "extd_ddp.h"

#include "disp_drv_platform.h"

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
#include "internal_hdmi_drv.h"
#elif defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
#include "inter_mhl_drv.h"
#else
#include "hdmi_drv.h"
#endif

#ifdef MTK_EXT_DISP_SYNC_SUPPORT
#include "display_recorder.h"
#include "mtkfb_fence.h"
#endif

#ifdef MTK_SMARTBOOK_SUPPORT
#include "smartbook.h"
#endif

#ifdef I2C_DBG
#include "tmbslHdmiTx_types.h"
#include "tmbslTDA9989_local.h"
#endif

#define HDMI_DEVNAME "hdmitx"

#define HW_OVERLAY_COUNT (4)

#define HDMI_DPI(suffix)        DPI  ## suffix
#define HMID_DEST_DPI           DISP_MODULE_DPI1
/* static int hdmi_bpp = 4; */

spinlock_t hdmi_lock;
DEFINE_SPINLOCK(hdmi_lock);

static bool factory_mode;

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
#define hdmi_abs(a) (((a) < 0) ? -(a) : (a))

static int hdmi_log_on;
static int hdmi_bufferdump_on = 1;
static int hdmi_hwc_on = 1;

static unsigned long hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
static unsigned long force_reschange = 0xffff;


static struct switch_dev hdmi_switch_data;
static struct switch_dev hdmires_switch_data;
static struct switch_dev hdmi_cec_switch_data;
static struct switch_dev hdmi_audio_switch_data;

#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
static unsigned int hdmi_res = HDMI_VIDEO_1280x720p_50Hz;
#endif




struct HDMI_PARAMS _s_hdmi_params = { 0 };

struct HDMI_PARAMS *hdmi_params = &_s_hdmi_params;
static struct HDMI_DRIVER *hdmi_drv;

static size_t hdmi_colorspace = HDMI_RGB;

int hdmi_is_interlace = 0;
int hdmi_res_is_4k = 0;

int flag_resolution_interlace(HDMI_VIDEO_RESOLUTION resolution)
{
	if ((resolution == HDMI_VIDEO_1920x1080i_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i_50Hz) ||
#if defined(MTK_INTERNAL_MHL_SUPPORT)
	    (resolution == HDMI_VIDEO_1920x1080i3d_sbs_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_sbs_50Hz) ||
#endif
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz))
		return true;
	else
		return false;
}

int flag_resolution_3d(HDMI_VIDEO_RESOLUTION resolution)
{
	if ((resolution == HDMI_VIDEO_1280x720p3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1280x720p3d_50Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080i3d_60Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080p3d_24Hz) ||
	    (resolution == HDMI_VIDEO_1920x1080p3d_23Hz))
		return true;
	else
		return false;
}

int flag_resolution_4k(HDMI_VIDEO_RESOLUTION resolution)
{
	if ((resolution == HDMI_VIDEO_2160P_23_976HZ) ||
	    (resolution == HDMI_VIDEO_2160P_24HZ) ||
	    (resolution == HDMI_VIDEO_2160P_25HZ) ||
	    (resolution == HDMI_VIDEO_2160P_29_97HZ) ||
	    (resolution == HDMI_VIDEO_2160P_30HZ) ||
	    (resolution == HDMI_VIDEO_2161P_24HZ))
		return true;
	else
		return false;
}

int flag_resolution_fps(HDMI_VIDEO_RESOLUTION resolution)
{
	int fps = 60;

	switch (resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
		fps = 60;
		break;

	case HDMI_VIDEO_720x576p_50Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
		fps = 50;
		break;

	case HDMI_VIDEO_1920x1080p_30Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_2160P_29_97HZ:
	case HDMI_VIDEO_2160P_30HZ:
		fps = 30;
		break;

	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_2160P_25HZ:
		fps = 25;
		break;

	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_2160P_23_976HZ:
	case HDMI_VIDEO_2160P_24HZ:
	case HDMI_VIDEO_2161P_24HZ:
		fps = 24;
		break;

	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		fps = 23;
		break;

	default:
		HDMI_ERR("error resolution!\n");
		break;
	}

	return fps;
}

static int tv_fps;

int hdmi_get_tv_fps(void)
{
	return tv_fps;
}

void hdmi_log_enable(int enable)
{
	DISPMSG("hdmi log %s\n", enable ? "enabled" : "disabled");
	hdmi_log_on = enable;
	hdmi_drv->log_enable(enable);
}

void hdmi_mmp_enable(int enable)
{
	DISPMSG("hdmi log %s\n", enable ? "enabled" : "disabled");
	hdmi_bufferdump_on = enable;
}

void hdmi_hwc_enable(int enable)
{
	DISPMSG("hdmi log %s\n", enable ? "enabled" : "disabled");
	hdmi_hwc_on = enable;
}

DEFINE_SEMAPHORE(hdmi_update_mutex);
typedef struct {
	bool is_reconfig_needed;	/* whether need to reset HDMI memory */
	bool is_enabled;	/* whether HDMI is enabled or disabled by user */
	bool is_force_disable;	/* used for camera scenario. */
	bool is_clock_on;	/* DPI is running or not */
	bool is_mhl_video_on;	/* DPI is running or not */
	atomic_t state;		/* HDMI_POWER_STATE state */
	int lcm_width;		/* LCD write buffer width */
	int lcm_height;		/* LCD write buffer height */
	bool lcm_is_video_mode;
	int hdmi_width;		/* DPI read buffer width */
	int hdmi_height;	/* DPI read buffer height */
	int bg_width;		/* DPI read buffer width */
	int bg_height;		/* DPI read buffer height */
	HDMI_VIDEO_RESOLUTION output_video_resolution;
	enum HDMI_AUDIO_FORMAT output_audio_format;
	/* MDP's orientation, 0 means 0 degree, 1 means 90 degree, 2 means 180 degree, 3 means 270 degree */
	int orientation;
	enum HDMI_OUTPUT_MODE output_mode;
	int scaling_factor;
} _t_hdmi_context;



static _t_hdmi_context hdmi_context;
static _t_hdmi_context *p = &hdmi_context;
/* static struct list_head HDMI_Buffer_List; */
static unsigned int hdmi_layer_num;

#define IS_HDMI_ON()            (HDMI_POWER_STATE_ON == atomic_read(&p->state))
#define IS_HDMI_OFF()           (HDMI_POWER_STATE_OFF == atomic_read(&p->state))
#define IS_HDMI_STANDBY()       (HDMI_POWER_STATE_STANDBY == atomic_read(&p->state))

#define IS_HDMI_NOT_ON()        (HDMI_POWER_STATE_ON != atomic_read(&p->state))
#define IS_HDMI_NOT_OFF()       (HDMI_POWER_STATE_OFF != atomic_read(&p->state))
#define IS_HDMI_NOT_STANDBY()   (HDMI_POWER_STATE_STANDBY != atomic_read(&p->state))

#define SET_HDMI_ON()           atomic_set(&p->state, HDMI_POWER_STATE_ON)
#define SET_HDMI_OFF()          atomic_set(&p->state, HDMI_POWER_STATE_OFF)
#define SET_HDMI_STANDBY()      atomic_set(&p->state, HDMI_POWER_STATE_STANDBY)


#define IS_HDMI_FAKE_PLUG_IN()  (true == atomic_read(&hdmi_fake_in))
#define SET_HDMI_FAKE_PLUG_IN() (atomic_set(&hdmi_fake_in, true))
#define SET_HDMI_NOT_FAKE()     (atomic_set(&hdmi_fake_in, false))

#ifdef MTK_HDMI_SCREEN_CAPTURE
bool capture_screen = false;
unsigned long capture_addr;
#endif
unsigned int hdmi_va, hdmi_mva_r;


static dev_t hdmi_devno;
static struct cdev *hdmi_cdev;
static struct class *hdmi_class;
static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int hdmi_open(struct inode *inode, struct file *file);
static int hdmi_release(struct inode *inode, struct file *file);
static int hdmi_probe(struct platform_device *pdev);
static int hdmi_remove(struct platform_device *pdev);
#if CONFIG_COMPAT
static long hdmi_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg);
#endif

const struct file_operations hdmi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdmi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdmi_ioctl_compat,
#endif
	.open = hdmi_open,
	.release = hdmi_release,
};

static const struct of_device_id hdmi_of_ids[] = {
	{.compatible = "mediatek,mt8173-hdmitx",},
	{},
};


static struct platform_driver hdmi_driver = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.driver = {
		   .name = HDMI_DEVNAME,
		   .owner = THIS_MODULE,
		   .of_match_table = hdmi_of_ids,
		   },
};


#include <mmprofile.h>
#include "display_recorder.h"

disp_ddp_path_config extd_dpi_params;

struct task_struct *hdmi_rdma_config_task = NULL;
wait_queue_head_t hdmi_rdma_config_wq;
atomic_t hdmi_rdma_config_event = ATOMIC_INIT(0);

static unsigned int hdmi_resolution_param_table[][3] = {
	{720, 480, 60},
	{1280, 720, 60},
	{1920, 1080, 30},
	{1920, 1080, 60},
};

static unsigned int ovl_config_address[4];
#define ENABLE_HDMI_BUFFER_LOG 1
#if ENABLE_HDMI_BUFFER_LOG
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...) \
	do { \
		if (enable_hdmi_buffer_log) { \
			DISPMSG("[hdmi_buffer] "); \
		DISPMSG(fmt, ##arg); } \
	} while (0)
#else
bool enable_hdmi_buffer_log = 0;
#define HDMI_BUFFER_LOG(fmt, arg...)
#endif

static bool otg_enable_status;
static wait_queue_head_t hdmi_vsync_wq;
static bool hdmi_vsync_flag;
static int hdmi_vsync_cnt;
static atomic_t hdmi_fake_in = ATOMIC_INIT(false);
static int rdmafpscnt;
struct timer_list timer;


static void hdmi_udelay(unsigned int us)
{
	udelay(us);
}

static void hdmi_mdelay(unsigned int ms)
{
	msleep(ms);
}

unsigned int hdmi_get_width(void)
{
	return p->hdmi_width;
}

unsigned int hdmi_get_height(void)
{
	return p->hdmi_height;
}



/* For Debugfs */
void hdmi_cable_fake_plug_in(void)
{
	SET_HDMI_FAKE_PLUG_IN();
	HDMI_LOG("[HDMIFake]Cable Plug In\n");

	if (p->is_force_disable == false) {
		if (IS_HDMI_STANDBY()) {
			hdmi_resume();
			/* /msleep(1000); */
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
		}
	}
}

/* For Debugfs */
void hdmi_cable_fake_plug_out(void)
{
	SET_HDMI_NOT_FAKE();
	HDMI_LOG("[HDMIFake]Disable\n");

	if (p->is_force_disable == false) {
		if (IS_HDMI_ON()) {
			if (hdmi_drv->get_state() != HDMI_STATE_ACTIVE) {
				hdmi_suspend();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmires_switch_data, 0);
			}
		}
	}
}

void hdmi_force_resolution(int res)
{
	HDMI_LOG("hdmi_force_resolution %d\n", res);

	force_reschange = res;
}

int get_hdmi_dev_info(HDMI_QUERY_TYPE type)
{
	switch (type) {
	case HDMI_CHARGE_CURRENT:
		{
			if ((p->is_enabled == false)
			    || hdmi_params->cabletype == HDMI_CABLE) {
				return 0;
			} else if (hdmi_params->cabletype == MHL_CABLE) {
				return 500;
			} else if (hdmi_params->cabletype == MHL_2_CABLE) {
				return 900;
			}

		}

	default:
		return 0;
	}

}


void hdmi_waitVsync(void)
{
	unsigned int session_id = ext_disp_get_sess_id();
	disp_session_sync_info *session_info = disp_get_session_sync_info_for_debug(session_id);

	if (session_info)
		dprec_start(&session_info->event_waitvsync, 0, 0);

	if (p->is_clock_on == false) {
		HDMI_LOG("[hdmi]:hdmi has suspend, return directly\n");
		msleep(20);
		return;
	}

	hdmi_vsync_cnt++;

	hdmi_vsync_flag = 0;
	if (wait_event_interruptible_timeout(hdmi_vsync_wq, hdmi_vsync_flag, HZ / 10) == 0)
		HDMI_LOG("[hdmi] Wait VSync timeout. early_suspend=%d\n", p->is_clock_on);

	if (session_info)
		dprec_done(&session_info->event_waitvsync, hdmi_vsync_cnt, 0);

	hdmi_vsync_cnt--;
}

bool is_hdmi_active(void)
{
	return IS_HDMI_ON() && p->is_clock_on && p->is_enabled;
}

int get_extd_fps_time(void)
{

	if (hdmi_reschange == HDMI_VIDEO_1920x1080p_30Hz)
		return 34000;
	else
		return 16700;

}

static void _hdmi_rdma_irq_handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if (!is_hdmi_active())
		return;

#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* In TEE, we have to protect WDMA registers, so we can't enable WDMA interrupt */
	/* here we use ovl frame done interrupt instead */
	if ((module == DISP_MODULE_OVL1) && ext_disp_is_decouple_mode()) {
		/* OVL1 frame done */
		if (param & 0x2) {
			atomic_set(&hdmi_rdma_config_event, 1);
			wake_up_interruptible(&hdmi_rdma_config_wq);
		}
	}
#else
	if ((module == DISP_MODULE_WDMA1) && ext_disp_is_decouple_mode()) {
		/* wdma1 frame done */
		if (param & 0x1) {
			atomic_set(&hdmi_rdma_config_event, 1);
			wake_up_interruptible(&hdmi_rdma_config_wq);
		}
	}
#endif

#else

	if (module == DISP_MODULE_RDMA1) {
		if (param & 0x2) {	/* start */
			/* /MMProfileLogEx(ddp_mmp_get_events()->Extd_IrqStatus, MMProfileFlagPulse, module, param); */

			atomic_set(&hdmi_rdma_config_event, 1);
			wake_up_interruptible(&hdmi_rdma_config_wq);

			if (hdmi_params->cabletype == MHL_SMB_CABLE) {
				hdmi_vsync_flag = 1;
				wake_up_interruptible(&hdmi_vsync_wq);
			}
		}
	}
#endif
}

int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
		      enum HDMI_VIDEO_OUTPUT_FORMAT vout);
static int hdmi_rdma_config_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 }; /* RTPM_PRIO_SCRN_UPDATE */
	int layid = 0;
	unsigned int session_id = 0;
	int fence_idx = 0;
	bool ovl_reg_updated = false;

	sched_setscheduler(current, SCHED_RR, &param);
	for (;;) {
		wait_event_interruptible(hdmi_rdma_config_wq, atomic_read(&hdmi_rdma_config_event));
		atomic_set(&hdmi_rdma_config_event, 0);
		ovl_reg_updated = false;

		session_id = ext_disp_get_sess_id();
		fence_idx = -1;
		if (get_ext_disp_path_mode() == EXTD_DEBUG_RDMA_DPI_MODE) {
			fence_idx =
			    disp_sync_find_fence_idx_by_addr(session_id, 0,
							     ddp_dpi_get_cur_addr(true, 0));
			mtkfb_release_fence(session_id, 0, fence_idx);
		} else {
			for (layid = 0; layid < HW_OVERLAY_COUNT; layid++) {
				if (ovl_config_address[layid] != ddp_dpi_get_cur_addr(false, layid)) {
					ovl_config_address[layid] =
					    ddp_dpi_get_cur_addr(false, layid);
					ovl_reg_updated = true;
				}

				/* use cmdq slot to release fence */
				fence_idx =
				    get_cur_config_fence(layid) - get_subtractor_when_free(layid);
				if (fence_idx)
					mtkfb_release_fence(session_id, layid, fence_idx);
				MMProfileLogEx(ddp_mmp_get_events()->Extd_fence_release[layid],
					       MMProfileFlagPulse,
					       DISP_SESSION_TYPE(session_id), fence_idx);
			}

			if (ovl_reg_updated == false)
				MMProfileLogEx(ddp_mmp_get_events()->Extd_trigger,
					       MMProfileFlagPulse, ddp_dpi_get_cur_addr(false, 0),
					       ddp_dpi_get_cur_addr(false, 1));

			MMProfileLogEx(ddp_mmp_get_events()->Extd_UsedBuff, MMProfileFlagPulse,
				       ddp_dpi_get_cur_addr(false, 0), ddp_dpi_get_cur_addr(false,
											    1));
		}

		rdmafpscnt++;
		hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888,
				  HDMI_VOUT_FORMAT_RGB888);

		if (kthread_should_stop())
			break;
	}

	return 0;
}


/* Allocate memory, set M4U, LCD, MDP, DPI */
/* LCD overlay to memory -> MDP resize and rotate to memory -> DPI read to HDMI */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_init(void)
{
	/* int lcm_width, lcm_height; */
	/* int tmpBufferSize; */
	/* M4U_PORT_STRUCT m4uport; */

	HDMI_FUNC();
	/* /hdmi_get_width(hdmi_params->init_config.vformat); */
	p->hdmi_width = hdmi_resolution_param_table[hdmi_params->init_config.vformat][0];
	/* /hdmi_get_height(hdmi_params->init_config.vformat); */
	p->hdmi_height = hdmi_resolution_param_table[hdmi_params->init_config.vformat][1];
	p->bg_width = 0;
	p->bg_height = 0;

	p->output_video_resolution = hdmi_params->init_config.vformat;
	p->output_audio_format = hdmi_params->init_config.aformat;
	p->scaling_factor = hdmi_params->scaling_factor < 10 ? hdmi_params->scaling_factor : 10;

	/* /ddp_dpi_init(DISP_MODULE_DPI1, 0); */

	hdmi_dpi_power_switch(false);	/* but dpi power is still off */

	if (!hdmi_rdma_config_task) {
#ifdef HDMI_SUB_PATH_PRESENT_FENCE_SUPPORT
		disp_register_module_irq_callback(DISP_MODULE_OVL1, _hdmi_rdma_irq_handler);
		disp_register_module_irq_callback(DISP_MODULE_WDMA1, _hdmi_rdma_irq_handler);
#else
		disp_register_module_irq_callback(DISP_MODULE_RDMA2, _hdmi_rdma_irq_handler);
		disp_register_module_irq_callback(DISP_MODULE_RDMA1, _hdmi_rdma_irq_handler);
#endif

		hdmi_rdma_config_task =
		    kthread_create(hdmi_rdma_config_kthread, NULL, "hdmi_rdma_config_kthread");
		wake_up_process(hdmi_rdma_config_task);
	}


	return HDMI_STATUS_OK;
}

/* Release memory */
/* Will only be used in ioctl(MTK_HDMI_AUDIO_VIDEO_ENABLE) */
static HDMI_STATUS hdmi_drv_deinit(void)
{
	/* int temp_va_size; */

	HDMI_FUNC();

	hdmi_dpi_power_switch(false);
	hdmi_free_hdmi_buffer();

	return HDMI_STATUS_OK;
}
/*------------------------------ */
/*#ifdef EXTD_DBG_USE_INNER_BUF*/
/*extern unsigned char kara_1280x720[2764800]; */
/*#endif */


int hdmi_allocate_hdmi_buffer(void)
{
	/* M4U_PORT_STRUCT m4uport; */
	/* int ret = 0; */
	/* int hdmiPixelSize = p->hdmi_width * p->hdmi_height; */
	/* int hdmiDataSize = hdmiPixelSize * 4; *//* //hdmi_bpp; */
	/* int hdmiBufferSize = hdmiDataSize * 1; *//* /hdmi_params->intermediat_buffer_num; */

	HDMI_FUNC();
#ifdef EXTD_DBG_USE_INNER_BUF

	if ((hdmi_va)) {
		HDMI_LOG("return in %d\n", __LINE__);
		return 0;
	}

	hdmi_va = (unsigned int)vmalloc(hdmiBufferSize);
	if (((void *)hdmi_va) == NULL) {
		HDMI_ERR("vmalloc %d bytes fail!!!\n", hdmiBufferSize);
		return -1;
	}

	m4u_client_t *client = NULL;

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		HDMI_ERR("create client fail!\n");

	ret =
	    m4u_alloc_mva(client, M4U_PORT_DISP_OVL1, hdmi_va, 0, hdmiBufferSize,
			  M4U_PROT_READ | M4U_PROT_WRITE, 0, &hdmi_mva_r);

	/* memcpy(hdmi_va, kara_1280x720, 2764800); */
	HDMI_LOG("hdmi_va=0x%08x, hdmi_mva_r=0x%08x, size %d\n", hdmi_va, hdmi_mva_r,
		 hdmiBufferSize);
#endif
	return 0;

}


int hdmi_free_hdmi_buffer(void)
{
	/* int hdmi_va_size =
	   p->hdmi_width * p->hdmi_height * hdmi_bpp * hdmi_params->intermediat_buffer_num; */
	return 0;
}

/* Switch DPI Power for HDMI Driver */
/*static*/ void hdmi_dpi_power_switch(bool enable)
{
	/* int ret = 0; */
	int i = 0;
	int session_id = ext_disp_get_sess_id();

	HDMI_LOG("hdmi_dpi_power_switch, current state: %d  -> target state: %d\n", p->is_clock_on,
		 enable);

	if (enable) {
		if (p->is_clock_on == true) {
			HDMI_LOG("power on request while already powered on!\n");
			return;
		}

		ext_disp_resume();

		p->is_clock_on = true;
	} else {
		p->is_clock_on = false;
		ext_disp_suspend();
		if (IS_HDMI_ON()) {
			for (i = 0; i < HW_OVERLAY_COUNT; i++)
				mtkfb_release_layer_fence(session_id, i);
		}
	}
}

/* Configure video attribute */
int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
		      enum HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	if (p->is_mhl_video_on == true)
		return 0;

	HDMI_LOG("hdmi_video_config video_on=%d fps %d\n", p->is_mhl_video_on, rdmafpscnt);
	if (IS_HDMI_NOT_ON()) {
		HDMI_ERR("return in %d\n", __LINE__);
		return 0;
	}
	hdmi_allocate_hdmi_buffer();
	p->is_mhl_video_on = true;

	if (IS_HDMI_FAKE_PLUG_IN())
		return 0;

	return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
int hdmi_audio_config(int samplerate)
{
	HDMI_FUNC();
	if (!p->is_enabled) {
		HDMI_ERR("return in %d\n", __LINE__);
		return 0;
	}
	if (IS_HDMI_NOT_ON()) {
		HDMI_ERR("return in %d\n", __LINE__);
		return 0;
	}

	HDMI_LOG("sample rate=%d\n", samplerate);

	if (samplerate == 48000)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_48000;
	else if (samplerate == 44100)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_44100;
	else if (samplerate == 32000)
		p->output_audio_format = HDMI_AUDIO_PCM_16bit_32000;
	else
		HDMI_ERR("samplerate not support:%d\n", samplerate);


	hdmi_drv->audio_config(p->output_audio_format);

	return 0;
}

/* No one will use this function */
/*static*/ int hdmi_video_enable(bool enable)
{
	HDMI_FUNC();

	return hdmi_drv->video_enable(enable);
}

/* No one will use this function */
/*static*/ int hdmi_audio_enable(bool enable)
{
	HDMI_FUNC();

	return hdmi_drv->audio_enable(enable);
}


/* Reset HDMI Driver state */
#if !defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
static void hdmi_state_reset(void)
{
	HDMI_FUNC();

	if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
		switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
		hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
	} else {
		switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
		switch_set_state(&hdmires_switch_data, 0);
	}
}
#endif
/* HDMI Driver state callback function */
void hdmi_state_callback(enum HDMI_STATE state)
{

	DISPMSG("[hdmi]%s, state = %d\n", __func__, state);

	if (p->is_force_disable == true) {
		HDMI_ERR("return in %d\n", __LINE__);
		return;
	}

	if (IS_HDMI_FAKE_PLUG_IN()) {
		HDMI_ERR("return in %d\n", __LINE__);
		return;
	}

	switch (state) {
	case HDMI_STATE_NO_DEVICE:
		{
			hdmi_suspend();
			switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
			switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_NO_DEVICE);
			switch_set_state(&hdmires_switch_data, 0);
			break;
		}

	case HDMI_STATE_ACTIVE:
		{

			if (IS_HDMI_ON()) {
				HDMI_LOG("[hdmi]%s, already on(%d) !\n", __func__,
					 atomic_read(&(p->state)));
				break;
			}

			hdmi_drv->get_params(hdmi_params);
			hdmi_resume();

			if (atomic_read(&p->state) > HDMI_POWER_STATE_OFF) {
				switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
				switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_ACTIVE);
			}
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			break;
		}

	case HDMI_STATE_NO_DEVICE_IN_BOOT:
		{
			HDMI_LOG("[hdmi]uevent in boot:HDMI_STATE_NO_DEVICE\n");
			switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_NO_DEVICE);
			break;
		}

	case HDMI_STATE_ACTIVE_IN_BOOT:
		{
			HDMI_LOG("[hdmi]uevent in boot:HDMI_STATE_ACTIVE_IN_BOOT\n");
			switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_ACTIVE);
			break;
		}

	default:
		{
			HDMI_ERR("[hdmi]%s, state not support\n", __func__);
			break;
		}
	}

}

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
void hdmi_cec_state_callback(enum HDMI_CEC_STATE state)
{
	DISPMSG("[hdmi]%s, cec_state = %d\n", __func__, state);
	switch_set_state(&hdmi_cec_switch_data, 0xff);
	switch (state) {
	case HDMI_CEC_STATE_PLUG_OUT:
		switch_set_state(&hdmi_cec_switch_data, HDMI_CEC_STATE_PLUG_OUT);
		break;
	case HDMI_CEC_STATE_GET_PA:
		switch_set_state(&hdmi_cec_switch_data, HDMI_CEC_STATE_GET_PA);
		break;
	case HDMI_CEC_STATE_TX_STS:
		switch_set_state(&hdmi_cec_switch_data, HDMI_CEC_STATE_TX_STS);
		break;
	case HDMI_CEC_STATE_GET_CMD:
		switch_set_state(&hdmi_cec_switch_data, HDMI_CEC_STATE_GET_CMD);
		break;
	default:
		DISPMSG("[hdmi]%s, cec_state not support\n", __func__);
		break;
	}
}
#endif

/*static*/ void hdmi_power_on(void)
{
	HDMI_FUNC();

	if (IS_HDMI_NOT_OFF()) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	if (down_interruptible(&hdmi_update_mutex)) {
		DISPMSG("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	SET_HDMI_STANDBY();

	hdmi_drv->power_on();

	up(&hdmi_update_mutex);

#if !defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	if (p->is_force_disable == false) {
		if (IS_HDMI_FAKE_PLUG_IN()) {
			/* FixMe, deadlock may happened here, due to recursive use mutex */
			hdmi_resume();
			msleep(1000);
			switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
			hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
		} else {
			/* this is just a ugly workaround for some tv sets... */
			if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {	/* / && (factory_mode == true)) */
				hdmi_drv->get_params(hdmi_params);
				hdmi_resume();
			}
			hdmi_state_reset();
		}
	}
#endif
	return;
}

/*static*/ void hdmi_power_off(void)
{
	HDMI_FUNC();

	switch_set_state(&hdmires_switch_data, 0);

	if (IS_HDMI_OFF()) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	if (down_interruptible(&hdmi_update_mutex)) {
		DISPMSG("[hdmi][HDMI] can't get semaphore in %s()\n", __func__);
		return;
	}

	hdmi_drv->power_off();

	/* /ext_disp_suspend(); */
	hdmi_dpi_power_switch(false);
	SET_HDMI_OFF();
	up(&hdmi_update_mutex);
}

/*static*/ void hdmi_suspend(void)
{
	HDMI_FUNC();
	if (IS_HDMI_NOT_ON()) {
		HDMI_LOG("return in %d\n", __LINE__);
		return;
	}

	if (hdmi_bufferdump_on > 0)
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart, Plugout, 0);

	hdmi_drv->suspend();
	p->is_mhl_video_on = false;

	hdmi_dpi_power_switch(false);
	SET_HDMI_STANDBY();
	/* /ext_disp_suspend(); */

	/* /disp_module_clock_off(DISP_MODULE_RDMA2, "HDMI"); */
	/* up(&hdmi_update_mutex); */

	ext_disp_deinit(NULL);

	if (hdmi_bufferdump_on > 0)
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd, Plugout, 0);
}

/*static*/ void hdmi_resume(void)
{
	HDMI_LOG("p->state is %d,(0:off, 1:on, 2:standby)\n", atomic_read(&(p->state)));

	if (get_boot_mode() != FACTORY_BOOT) {
		if (IS_HDMI_NOT_STANDBY()) {
			HDMI_ERR("return in %d\n", __LINE__);
			return;
		}
		if (IS_HDMI_ON()) {
			HDMI_LOG("return in %d\n", __LINE__);
			return;
		}
	}
	if (hdmi_bufferdump_on > 0)
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart, Plugin, 0);

	hdmi_dpi_power_switch(true);
	SET_HDMI_ON();
	/* /ext_disp_resume(); */
	hdmi_drv->resume();
	/* up(&hdmi_update_mutex); */

	if (hdmi_bufferdump_on > 0)
		MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd, Plugin, 0);
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	HDMI_FUNC();
	return 0;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
	HDMI_FUNC();
	return 0;
}

static bool hdmi_drv_init_context(void);

void dpi_setting_res(u8 arg)
{
	DPI_POLARITY clk_pol = 0, de_pol = 0, hsync_pol = 0, vsync_pol = 0;
	unsigned int hsync_pulse_width = 0, hsync_back_porch = 0,
	    hsync_front_porch = 0, vsync_pulse_width = 0, vsync_back_porch = 0, vsync_front_porch =
	    0;
	/* intermediat_buffer_num ; */

	switch (arg) {

	case HDMI_VIDEO_720x480p_60Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			hsync_pulse_width = 62;
			hsync_back_porch = 60;
			hsync_front_porch = 16;

			vsync_pulse_width = 6;
			vsync_back_porch = 30;
			vsync_front_porch = 9;

			p->bg_height = ((480 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((720 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 720 - p->bg_width;
			p->hdmi_height = 480 - p->bg_height;
			p->output_video_resolution = HDMI_VIDEO_720x480p_60Hz;
			break;
		}

	case HDMI_VIDEO_720x576p_50Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_RISING;
			vsync_pol = HDMI_POLARITY_RISING;

			hsync_pulse_width = 64;
			hsync_back_porch = 68;
			hsync_front_porch = 12;

			vsync_pulse_width = 5;
			vsync_back_porch = 39;
			vsync_front_porch = 5;

			p->bg_height = ((576 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((720 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 720 - p->bg_width;
			p->hdmi_height = 576 - p->bg_height;
			p->output_video_resolution = HDMI_VIDEO_720x576p_50Hz;
			break;
		}

	case HDMI_VIDEO_1280x720p_60Hz:
		{

			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 110;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->bg_height = ((720 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1280 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1280 - p->bg_width;
			p->hdmi_height = 720 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1280x720p_60Hz;
			break;
		}

	case HDMI_VIDEO_1280x720p_50Hz:
		{

			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 40;
			hsync_back_porch = 220;
			hsync_front_porch = 440;

			vsync_pulse_width = 5;
			vsync_back_porch = 20;
			vsync_front_porch = 5;

			p->bg_height = ((720 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1280 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1280 - p->bg_width;
			p->hdmi_height = 720 - p->bg_height;
			p->output_video_resolution = HDMI_VIDEO_1280x720p_50Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080i_60Hz:
		{
			/* fgInterlace = TRUE; */
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 15;
			vsync_front_porch = 2;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080i_60Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080i_50Hz:
		{
			/* fgInterlace = TRUE; */
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 15;
			vsync_front_porch = 2;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;
			p->output_video_resolution = HDMI_VIDEO_1920x1080i_50Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_23Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_23Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_24Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 638;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_24Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_25Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_25Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_29Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_29Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_30Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_30Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_60Hz:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 88;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_60Hz;
			break;
		}

	case HDMI_VIDEO_1920x1080p_50Hz:
		{
			clk_pol = HDMI_POLARITY_FALLING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 44;
			hsync_back_porch = 148;
			hsync_front_porch = 528;

			vsync_pulse_width = 5;
			vsync_back_porch = 36;
			vsync_front_porch = 4;

			p->bg_height = ((1080 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((1920 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 1920 - p->bg_width;
			p->hdmi_height = 1080 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_1920x1080p_50Hz;
			break;
		}

	case HDMI_VIDEO_2160P_30HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 176;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((3840 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 3840 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2160P_30HZ;
			break;
		}

	case HDMI_VIDEO_2160P_29_97HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 176;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((3840 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 3840 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2160P_29_97HZ;
			break;
		}

	case HDMI_VIDEO_2160P_25HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 1056;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((3840 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 3840 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2160P_25HZ;
			break;
		}

	case HDMI_VIDEO_2160P_24HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 1276;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((3840 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 3840 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2160P_24HZ;
			break;
		}

	case HDMI_VIDEO_2160P_23_976HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 1276;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((3840 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 3840 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2160P_23_976HZ;
			break;
		}

	case HDMI_VIDEO_2161P_24HZ:
		{
			clk_pol = HDMI_POLARITY_RISING;
			de_pol = HDMI_POLARITY_RISING;
			hsync_pol = HDMI_POLARITY_FALLING;
			vsync_pol = HDMI_POLARITY_FALLING;

			hsync_pulse_width = 88;
			hsync_back_porch = 296;
			hsync_front_porch = 1020;

			vsync_pulse_width = 10;
			vsync_back_porch = 72;
			vsync_front_porch = 8;

			p->bg_height = ((2160 * p->scaling_factor) / 100 >> 2) << 2;
			p->bg_width = ((4096 * p->scaling_factor) / 100 >> 2) << 2;
			p->hdmi_width = 4096 - p->bg_width;
			p->hdmi_height = 2160 - p->bg_height;

			p->output_video_resolution = HDMI_VIDEO_2161P_24HZ;
			break;
		}

	default:
		break;
	}

	extd_dpi_params.dispif_config.dpi.width = p->hdmi_width;
	extd_dpi_params.dispif_config.dpi.height = p->hdmi_height;
	extd_dpi_params.dispif_config.dpi.bg_width = p->bg_width;
	extd_dpi_params.dispif_config.dpi.bg_height = p->bg_width;

	extd_dpi_params.dispif_config.dpi.clk_pol = clk_pol;
	extd_dpi_params.dispif_config.dpi.de_pol = de_pol;
	extd_dpi_params.dispif_config.dpi.vsync_pol = vsync_pol;
	extd_dpi_params.dispif_config.dpi.hsync_pol = hsync_pol;

	extd_dpi_params.dispif_config.dpi.hsync_pulse_width = hsync_pulse_width;
	extd_dpi_params.dispif_config.dpi.hsync_back_porch = hsync_back_porch;
	extd_dpi_params.dispif_config.dpi.hsync_front_porch = hsync_front_porch;
	extd_dpi_params.dispif_config.dpi.vsync_pulse_width = vsync_pulse_width;
	extd_dpi_params.dispif_config.dpi.vsync_back_porch = vsync_back_porch;
	extd_dpi_params.dispif_config.dpi.vsync_front_porch = vsync_front_porch;

	extd_dpi_params.dispif_config.dpi.format = LCM_DPI_FORMAT_RGB888;
	extd_dpi_params.dispif_config.dpi.rgb_order = LCM_COLOR_ORDER_RGB;
	extd_dpi_params.dispif_config.dpi.i2x_en = true;
	extd_dpi_params.dispif_config.dpi.i2x_edge = 2;
	extd_dpi_params.dispif_config.dpi.embsync = false;
	extd_dpi_params.dispif_config.dpi.dpi_clock = arg;

	HDMI_LOG("dpi_setting_res:%d\n", arg);

}

void hdmi_set_layer_num(int layer_num)
{
	hdmi_layer_num = layer_num;
}

int _get_ext_disp_info(void *info)
{
	disp_session_info *dispif_info = (disp_session_info *) info;

	memset((void *)dispif_info, 0, sizeof(disp_session_info));

	if ((get_ext_disp_path_mode() == EXTD_DIRECT_LINK_MODE)
	    || (get_ext_disp_path_mode() == EXTD_DECOUPLE_MODE))
		dispif_info->maxLayerNum = 4;
	else
		dispif_info->maxLayerNum = 1;

	dispif_info->isOVLDisabled = (hdmi_layer_num == 1) ? 1 : 0;
	dispif_info->displayFormat = DISPIF_FORMAT_RGB888;
	dispif_info->displayHeight = p->hdmi_height;
	dispif_info->displayWidth = p->hdmi_width;
	dispif_info->displayMode = DISP_IF_MODE_VIDEO;

	if (hdmi_params->cabletype == MHL_SMB_CABLE) {
		dispif_info->displayType = DISP_IF_HDMI_SMARTBOOK;
		if (IS_HDMI_OFF())
			dispif_info->displayType = DISP_IF_MHL;
	} else if (hdmi_params->cabletype == MHL_CABLE) {
		dispif_info->displayType = DISP_IF_MHL;
	} else {
		dispif_info->displayType = DISP_IF_HDMI;
	}

#ifdef HDMI_SUB_PATH
	dispif_info->isHwVsyncAvailable = 1;
#else
	dispif_info->isHwVsyncAvailable = 0;
#endif
	dispif_info->vsyncFPS = 60;

	if (dispif_info->displayWidth * dispif_info->displayHeight <= 240 * 432)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (dispif_info->displayWidth * dispif_info->displayHeight <= 320 * 480)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (dispif_info->displayWidth * dispif_info->displayHeight <= 480 * 854)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;

	dispif_info->isConnected = 1;

#ifdef HDMI_SUB_PATH
	DISP_PRINTF(DDP_RESOLUTION_LOG, "ext DISP Info: %d x %d maxLayerNum %d\n",
		    dispif_info->displayWidth, dispif_info->displayHeight,
		    dispif_info->maxLayerNum);
#endif
	/* dispif_info->isHDCPSupported = (unsigned int)hdmi_params->HDCPSupported; */
	/* /HDMI_LOG("_get_ext_disp_info lays %d, type %d, H %d, hdcp %d\n", dispif_info->maxLayerNum ,
	   dispif_info->displayType, dispif_info->displayHeight, dispif_info->isHDCPSupported); */
	return 0;
}


int _ioctl_get_ext_disp_info(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	disp_session_info info;

	if (copy_from_user(&info, argp, sizeof(info))) {
		HDMI_ERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	_get_ext_disp_info(&info);

	if (copy_to_user(argp, &info, sizeof(info))) {
		HDMI_ERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}


static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	/* HDMI_EDID_T pv_get_info; */
	int r = 0;

#if (defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
	hdmi_device_write w_info;
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT)\
	&& defined(CONFIG_MTK_DRM_KEY_MNG_SUPPORT))
	hdmi_hdcp_drmkey key;
#else
	hdmi_hdcp_key key;
#endif
	send_slt_data send_sltdata;
	CEC_SLT_DATA get_sltdata;
	hdmi_para_setting data_info;
	HDMI_EDID_T pv_get_info;
	CEC_FRAME_DESCRIPTION_IO cec_frame;
	CEC_ADDRESS_IO cecaddr;
	CEC_DRV_ADDR_CFG cecsetAddr;
	CEC_SEND_MSG cecsendframe;
	READ_REG_VALUE regval;
	unsigned int tmp;
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	CEC_USR_CMD_T cec_usr_cmd;
	APK_CEC_ACK_INFO cec_tx_status;
#endif
#endif

#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	RW_VALUE stSpi;
	unsigned int addr, u4data;
	stMhlCmd_st stMhlCmd;
	HDMI_EDID_T pv_get_info;
	unsigned char pdata[16];
	MHL_3D_INFO_T pv_3d_info;
	hdmi_para_setting data_info;
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT)\
	&& defined(CONFIG_MTK_DRM_KEY_MNG_SUPPORT))
	hdmi_hdcp_drmkey key;
#else
	hdmi_hdcp_key key;
#endif

#endif

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	HDMITX_AUDIO_PARA audio_para;
#endif

	HDMI_LOG("hdmi ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);

	switch (cmd) {

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case MTK_HDMI_AUDIO_SETTING:
		if (copy_from_user(&audio_para, (void __user *)arg, sizeof(audio_para))) {
			HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			if (IS_HDMI_NOT_ON()) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			if (down_interruptible(&hdmi_update_mutex)) {
				HDMI_ERR("[HDMI] can't get semaphore in\n");
				return -EAGAIN;
			}
			hdmi_drv->audiosetting(&audio_para);
			up(&hdmi_update_mutex);
		}
		break;
#endif
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	case MTK_HDMI_WRITE_DEV:
		{
			if (copy_from_user(&w_info, (void __user *)arg, sizeof(w_info))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->write(w_info.u4Addr, w_info.u4Data);
			}
			break;
		}
	case MTK_HDMI_INFOFRAME_SETTING:
		{
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				if (IS_HDMI_NOT_ON()) {
					HDMI_ERR("return in %d\n", __LINE__);
					return 0;
				}
				hdmi_drv->InfoframeSetting(data_info.u4Data1 & 0xFF,
							   data_info.u4Data2 & 0xFF);
			}
			break;
		}

	case MTK_HDMI_HDCP_KEY:
		{
			if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->hdcpkey((UINT8 *) &key);
			}
			break;
		}

	case MTK_HDMI_SETLA:
		{
			if (copy_from_user(&cecsetAddr, (void __user *)arg, sizeof(cecsetAddr))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->setcecla(&cecsetAddr);
			}
			break;
		}

	case MTK_HDMI_SENDSLTDATA:
		{
			if (copy_from_user(&send_sltdata, (void __user *)arg, sizeof(send_sltdata))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->sendsltdata((UINT8 *) &send_sltdata);
			}
			break;
		}

	case MTK_HDMI_SET_CECCMD:
		{
			if (copy_from_user(&cecsendframe, (void __user *)arg, sizeof(cecsendframe))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->setceccmd(&cecsendframe);
			}
			break;
		}

	case MTK_HDMI_CEC_ENABLE:
		{
			hdmi_drv->cecenable(arg & 0xFF);
			break;
		}

	case MTK_HDMI_GET_CECCMD:
		{
			hdmi_drv->getceccmd(&cec_frame);
			if (copy_to_user((void __user *)arg, &cec_frame, sizeof(cec_frame))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_CECSTS:
		{
			hdmi_drv->getcectxstatus(&cec_tx_status);
			if (copy_to_user
			    ((void __user *)arg, &cec_tx_status, sizeof(APK_CEC_ACK_INFO))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_CEC_USR_CMD:
		{
			if (copy_from_user(&cec_usr_cmd, (void __user *)arg, sizeof(CEC_USR_CMD_T)))
				r = -EFAULT;
			else
				hdmi_drv->cecusrcmd(cec_usr_cmd.cmd, &(cec_usr_cmd.result));

			if (copy_to_user((void __user *)arg, &cec_usr_cmd, sizeof(CEC_USR_CMD_T))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

#if 0
	case MTK_HDMI_SET_3D_STRUCT:
		{
			if (hdmi_3d_strcut != (arg & 0xFF)) {
				hdmi_3d_strcut = (arg & 0xFF);
				hdmi_3d_changed = TRUE;
			}
			hdmi_drv->set3dstruct(arg & 0xFF);
			break;
		}
#endif
	case MTK_HDMI_GET_SLTDATA:
		{
			hdmi_drv->getsltdata(&get_sltdata);
			if (copy_to_user((void __user *)arg, &get_sltdata, sizeof(get_sltdata))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_CECADDR:
		{
			hdmi_drv->getcecaddr(&cecaddr);
			if (copy_to_user((void __user *)arg, &cecaddr, sizeof(cecaddr))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_COLOR_DEEP:
		{
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {

				HDMI_LOG("MTK_HDMI_COLOR_DEEP: %d %d\n", data_info.u4Data1,
					 data_info.u4Data2);

				hdmi_drv->colordeep(data_info.u4Data1 & 0xFF,
						    data_info.u4Data2 & 0xFF);
			}
#if (defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			hdmi_colorspace = (unsigned char)data_info.u4Data1;
			/* DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace)(hdmi_colorspace, hdmi_res)); */
#endif
			break;
		}

	case MTK_HDMI_READ_DEV:
		{
			if (copy_from_user(&regval, (void __user *)arg, sizeof(regval))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->read(regval.u1address, &regval.pu1Data);
			}

			if (copy_to_user((void __user *)arg, &regval, sizeof(regval))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_ENABLE_LOG:
		{
			hdmi_drv->log_enable(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_ENABLE_HDCP:
		{
			hdmi_drv->enablehdcp(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_CECRX_MODE:
		{
			hdmi_drv->setcecrxmode(arg & 0xFFFF);
			break;
		}

	case MTK_HDMI_STATUS:
		{
			hdmi_drv->hdmistatus();
			break;
		}

	case MTK_HDMI_CHECK_EDID:

		hdmi_drv->checkedid(arg & 0xFF);
		break;

#if 0
	case MTK_HDMI_GET_HDMI_STATUS:

		temp = hdmi_drv->gethdmistatus();
		if (copy_to_user((void __user *)arg, &temp, sizeof(unsigned int))) {
			HDMI_LOG("copy_to_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		}
		break;

#endif
#elif defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case MTK_HDMI_READ:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->read(stSpi.u4Addr, &(stSpi.u4Data));

			if (copy_to_user((void __user *)arg, &stSpi, sizeof(RW_VALUE))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}
	case MTK_HDMI_WRITE:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->write(stSpi.u4Addr, stSpi.u4Data);

			break;
		}
	case MTK_HDMI_DUMP:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;

			break;
		}
	case MTK_HDMI_STATUS:
		{
			hdmi_drv->hdmistatus();
			break;
		}
	case MTK_HDMI_DUMP6397:
		{
			hdmi_drv->dump6397();
			break;
		}
	case MTK_HDMI_DUMP6397_W:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->write6397(stSpi.u4Addr, stSpi.u4Data);

			break;
		}
	case MTK_HDMI_DUMP6397_R:
		{
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE)))
				r = -EFAULT;
			else
				hdmi_drv->read6397(stSpi.u4Addr, &(stSpi.u4Data));


			if (copy_to_user((void __user *)arg, &stSpi, sizeof(RW_VALUE))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}

			break;
		}
	case MTK_HDMI_CBUS_STATUS:
		{
			hdmi_drv->cbusstatus();
			break;
		}
	case MTK_HDMI_CMD:
		{
			HDMI_LOG("MTK_HDMI_CMD\n");
			if (copy_from_user(&stMhlCmd, (void __user *)arg, sizeof(stMhlCmd_st))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			HDMI_LOG("[MHL]cmd=%x%x%x%x\n", stMhlCmd.u4Cmd, stMhlCmd.u4Para,
			       stMhlCmd.u4Para1, stMhlCmd.u4Para2);
			hdmi_drv->mhl_cmd(stMhlCmd.u4Cmd, stMhlCmd.u4Para, stMhlCmd.u4Para1,
					  stMhlCmd.u4Para2);
			break;
		}
	case MTK_HDMI_HDCP:
		{
			if (arg)
				hdmi_drv->enablehdcp(3);
			else
				hdmi_drv->enablehdcp(0);

			break;
		}
	case MTK_HDMI_HDCP_KEY:
		{
			if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
				HDMI_ERR("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->hdcpkey((UINT8 *) &key);
			}
			break;
		}
	case MTK_HDMI_CONNECT_STATUS:
		{
			tmp = hdmi_drv->get_state();
			if (copy_to_user((void __user *)arg, &tmp, sizeof(unsigned int))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_MHL_GET_DCAP:
		{
			hdmi_drv->getdcapdata(pdata);
			if (copy_to_user((void __user *)arg, pdata, sizeof(pdata))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}
	case MTK_MHL_GET_3DINFO:
		{
			hdmi_drv->get3dinfo(&pv_3d_info);
			if (copy_to_user((void __user *)arg, &pv_3d_info, sizeof(pv_3d_info))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}
	case MTK_HDMI_COLOR_DEEP:
		{
/*
	   if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
	       HDMI_LOG("copy_from_user failed! line:%d\n", __LINE__);
	       r = -EFAULT;
	   } else {

		HDMI_LOG("MTK_HDMI_COLOR_DEEP: %d %d %d\n",data_info.u4Data1,data_info.u4Data2,hdmi_res);


		hdmi_colorspace = (unsigned char)(data_info.u4Data1 & 0xFF);
		if((hdmi_res == HDMI_VIDEO_1920x1080p_60Hz)
		||(hdmi_res == HDMI_VIDEO_1920x1080p_50Hz)
		||(hdmi_res == HDMI_VIDEO_1280x720p3d_60Hz)
		||(hdmi_res == HDMI_VIDEO_1280x720p3d_50Hz)
		||(hdmi_res == HDMI_VIDEO_1920x1080p3d_24Hz)
		||(hdmi_res == HDMI_VIDEO_1920x1080p3d_23Hz)
		)
		{
			hdmi_colorspace = HDMI_YCBCR_422;
		}
	       hdmi_drv->colordeep(hdmi_colorspace);
	   }
	    DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace)(hdmi_colorspace, hdmi_res));
	    */
			break;
		}
#endif


	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
		{
			if (arg) {
				if (p->is_enabled)
					break;

#ifdef HDMI_SUB_PATH
				if (factory_mode == false)
#endif
					HDMI_CHECK_RET(hdmi_drv_init());

				if (hdmi_drv->enter)
					hdmi_drv->enter();

				hdmi_power_on();
				p->is_enabled = true;

				if (get_boot_mode() == FACTORY_BOOT) {
					for (tmp = 0; (tmp < 60 && !IS_HDMI_ON()); tmp++)
						msleep(100);
				}
			} else {
				if (!p->is_enabled)
					break;

				p->is_enabled = false;

				/* wait hdmi finish update */
				if (down_interruptible(&hdmi_update_mutex)) {
					DISPMSG("[hdmi][HDMI] can't get semaphore in %s()\n",
					       __func__);
					return -EFAULT;
				}

				/* hdmi_video_buffer_info temp; */
				up(&hdmi_update_mutex);
				hdmi_power_off();

				/* wait hdmi finish update */
				if (down_interruptible(&hdmi_update_mutex)) {
					DISPMSG("[hdmi][HDMI] can't get semaphore in %s()\n",
					       __func__);
					return -EFAULT;
				}

				HDMI_CHECK_RET(hdmi_drv_deinit());

				up(&hdmi_update_mutex);

				if (hdmi_drv->exit)
					hdmi_drv->exit();

				/* when disable hdmi, HPD is disabled */
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_NO_DEVICE);
				HDMI_LOG("[hdmi] done power off\n");

			}

			break;
		}

	case MTK_HDMI_FORCE_FULLSCREEN_ON:
		/* case MTK_HDMI_FORCE_CLOSE: */
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			if (IS_HDMI_OFF()) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (p->is_force_disable == true)
				break;

			if (IS_HDMI_FAKE_PLUG_IN()) {
				hdmi_suspend();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmires_switch_data, 0);
			} else {
				if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
					hdmi_suspend();
					switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
					switch_set_state(&hdmires_switch_data, 0);
				}
			}

			p->is_force_disable = true;

			break;
		}

	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
		/* case MTK_HDMI_FORCE_OPEN: */
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			if (IS_HDMI_OFF()) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (p->is_force_disable == false)
				break;

			if (IS_HDMI_FAKE_PLUG_IN()) {
				hdmi_resume();
				msleep(1000);
				switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
				hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
			} else {
				if (hdmi_drv->get_state() == HDMI_STATE_ACTIVE) {
					hdmi_resume();
					msleep(1000);
					switch_set_state(&hdmi_switch_data, HDMI_STATE_ACTIVE);
					hdmi_reschange = HDMI_VIDEO_RESOLUTION_NUM;
				}
			}

			p->is_force_disable = false;

			break;
		}

	case MTK_HDMI_POWER_ENABLE:
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			if (arg) {
				if (otg_enable_status) {
					HDMI_ERR("return in %d\n", __LINE__);
					return 0;
				}
				hdmi_power_on();
			} else {
				hdmi_power_off();
				switch_set_state(&hdmi_switch_data, HDMI_STATE_NO_DEVICE);
				switch_set_state(&hdmi_audio_switch_data, HDMI_STATE_NO_DEVICE);
			}

			break;
		}

/*
	case MTK_HDMI_USBOTG_STATUS:
	{
	    HDMI_LOG("MTK_HDMI_USBOTG_STATUS, arg=%d, enable %d\n", arg, p->is_enabled);

	    RETIF(!p->is_enabled, 0);
	    RETIF((hdmi_params->cabletype != MHL_CABLE), 0);

	    if (arg)
	    {
		otg_enable_status = true;
	    }
	    else
	    {
		otg_enable_status = false;
		RETIF(p->is_force_disable, 0);
		hdmi_power_on();
	    }

	    break;
	}
*/

	case MTK_HDMI_AUDIO_ENABLE:
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (arg)
				HDMI_CHECK_RET(hdmi_audio_enable(true));
			else
				HDMI_CHECK_RET(hdmi_audio_enable(false));

			break;
		}

	case MTK_HDMI_VIDEO_ENABLE:
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			break;
		}

	case MTK_HDMI_VIDEO_CONFIG:
		{

			HDMI_LOG("video resolution configuration, ");
			HDMI_LOG
			    ("arg:%ld, origial resolution:%ld,factory_mode:%d, is_video_on:%d\n",
			     arg, hdmi_reschange, factory_mode, p->is_mhl_video_on);

#if HDMI_MAIN_PATH
			arg = HDMI_VIDEO_1920x1080p_60Hz;

			HDMI_LOG("box fix video resolution configuration %lu ", arg);
#endif

			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (IS_HDMI_NOT_ON()) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (flag_resolution_4k(arg)) {
				hdmi_res_is_4k = 1;
				HDMI_LOG("hdmi res is 4k, so don't split buffer\n");
			} else {
				hdmi_res_is_4k = 0;
				HDMI_LOG("hdmi res is not 4k, so need split buffer\n");
			}

			tv_fps = flag_resolution_fps(arg);

			/* just for debug */
			if (force_reschange < 0xff)
				arg = force_reschange;

			if (hdmi_reschange == arg) {
				HDMI_LOG("hdmi_reschange=%ld\n", hdmi_reschange);
				break;
			}

			hdmi_reschange = arg;
			p->is_clock_on = false;

			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}
			if (IS_HDMI_NOT_ON()) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			if (hdmi_bufferdump_on > 0)
				MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagStart,
					       ResChange, arg);

			hdmi_dpi_power_switch(false);
			if (down_interruptible(&hdmi_update_mutex)) {
				HDMI_ERR("[HDMI] can't get semaphore in\n");
				return -EFAULT;
			}
			/* hdmi_video_buffer_info temp; */
			dpi_setting_res((u8) arg);

			if (flag_resolution_interlace(arg))
				hdmi_is_interlace = 1;
			else
				hdmi_is_interlace = 0;

			p->is_mhl_video_on = false;

			if (factory_mode == true) {
				/* /ext_disp_init(NULL); */
				/* /hdmi_dpi_power_switch(true); */
#if 1
				/* ddp_dpi_power_on(DISP_MODULE_DPI0, NULL); */
				/* ddp_dpi_stop(DISP_MODULE_DPI0, NULL); */
				ddp_dpi_init(DISP_MODULE_DPI0, NULL);

				ddp_dpi_config(DISP_MODULE_DPI0, &extd_dpi_params, NULL);
				/*ddp_dpi_EnableColorBar(DISP_MODULE_DPI0); */
				ddp_dpi_trigger(DISP_MODULE_DPI0, NULL);
#endif
				hdmi_hwc_on = 0;
			} else {
				/* /hdmi_video_config(p->output_video_resolution,
				   HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888); */
#if 0
				ext_disp_init(NULL, NULL);

				hdmi_dpi_power_switch(true);
				/* /ext_disp_resume(); */

				ddp_dpi_stop(DISP_MODULE_DPI0, NULL);
#endif
			}
			up(&hdmi_update_mutex);
			hdmi_drv->tmdsonoff(0);
			udelay(300);
			/* hdmi_video_config(p->output_video_resolution, HDMI_VIN_FORMAT_RGB888,
			   HDMI_VOUT_FORMAT_RGB888); */

			if (factory_mode == false) {
				if (hdmi_hwc_on)
					switch_set_state(&hdmires_switch_data, hdmi_reschange + 1);
			}
			p->is_clock_on = true;

			rdmafpscnt = 0;
			if (hdmi_bufferdump_on > 0) {
				MMProfileLogEx(ddp_mmp_get_events()->Extd_State, MMProfileFlagEnd,
					       ResChange, hdmi_reschange + 1);
			}
			break;
		}

	case MTK_HDMI_AUDIO_CONFIG:
		{
			if (!p->is_enabled) {
				HDMI_ERR("return in %d\n", __LINE__);
				return 0;
			}

			break;
		}

	case MTK_HDMI_IS_FORCE_AWAKE:
		{
			if (!hdmi_drv_init_context()) {
				HDMI_ERR("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
				return HDMI_STATUS_NOT_IMPLEMENTED;
			}

			r = copy_to_user(argp, &hdmi_params->is_force_awake,
					 sizeof(hdmi_params->is_force_awake)) ? -EFAULT : 0;
			break;
		}
	case MTK_HDMI_FACTORY_MODE_ENABLE:
		{
			if (arg == 1) {
				if (hdmi_drv->power_on()) {
					r = -EAGAIN;
					HDMI_ERR("Enable HDMI factory mode test fail\n");
				}
			} else {
				HDMI_ERR("Disable HDMI factory mode test fail\n");
				hdmi_drv->power_off();
			}
			break;
		}

	case MTK_HDMI_FACTORY_GET_STATUS:
		{
			int hdmi_status = 0;
#ifdef HDMI_SUB_PATH
			hdmi_status = 2;
#else
			if (p->is_clock_on == true)
				hdmi_status = 1;
#endif
			if ((!hdmi_drv->checkedidheader()) && (hdmi_status == 0x1))
				hdmi_status = 0;
#ifdef HDMI_SUB_PATH
			else if ((!hdmi_drv->checkedidheader()) && (hdmi_status == 0x2))
				hdmi_status = 2;
			else if (hdmi_drv->checkedidheader() && (hdmi_status == 0x2))
				hdmi_status = 3;
#endif
			HDMI_LOG("MTK_HDMI_FACTORY_GET_STATUS is %d\n", hdmi_status);

			if (copy_to_user((void __user *)arg, &hdmi_status, sizeof(hdmi_status))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_FACTORY_DPI_TEST:
		{
			if (down_interruptible(&hdmi_update_mutex)) {
				HDMI_ERR("[HDMI] can't get semaphore in\n");
				return -EAGAIN;
			}

			/* ddp_dpi_power_on(DISP_MODULE_DPI1, NULL); */
			/* ddp_dpi_stop(DISP_MODULE_DPI1, NULL); */
			/* ddp_dpi_config(DISP_MODULE_DPI1, &extd_dpi_params, NULL); */
			ddp_dpi_EnableColorBar(DISP_MODULE_DPI0);
			/* ddp_dpi_trigger(DISP_MODULE_DPI1, NULL); */
			hdmi_hwc_on = 0;

			/* ddp_dpi_EnableColorBar(DISP_MODULE_DPI1); */
			ddp_dpi_start(DISP_MODULE_DPI0, NULL);

			up(&hdmi_update_mutex);
			ddp_dpi_dump(DISP_MODULE_DPI0, 1);

			if (IS_HDMI_FAKE_PLUG_IN()) {
				HDMI_ERR("fake cable in to return line:%d\n", __LINE__);
			} else {
				msleep(50);
				hdmi_video_config(p->output_video_resolution,
						  HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
			}
			ddp_dpi_dump(DISP_MODULE_DPI0, 1);
			break;
		}

	case MTK_HDMI_GET_DEV_INFO:
		{
			int displayid = 0;
			mtk_dispif_info_t hdmi_info;

			/* /_get_ext_disp_info() */
			if (hdmi_bufferdump_on > 0) {
				MMProfileLogEx(ddp_mmp_get_events()->Extd_DevInfo,
					       MMProfileFlagStart, p->is_enabled, p->is_clock_on);
			}

			HDMI_LOG("DEV_INFO configuration get +\n");

			if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) {
				if (hdmi_bufferdump_on > 0) {
					MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo,
						       MMProfileFlagPulse, Devinfo, 0);
				}

				HDMI_ERR(": copy_from_user failed! line:%d\n", __LINE__);
				return -EAGAIN;
			}

			if (displayid != MTKFB_DISPIF_HDMI) {
				/* if (hdmi_bufferdump_on > 0)
				   /MMProfileLogEx(HDMI_MMP_Events.GetDevInfo, MMProfileFlagPulse, 0xff, 0xff2); */

				HDMI_ERR(": invalid display id:%d\n", displayid);
				/* /return -EAGAIN; */
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
			else
				hdmi_info.displayType = HDMI;

			hdmi_info.isHwVsyncAvailable = 1;
			hdmi_info.vsyncFPS = 60;

			if (copy_to_user((void __user *)arg, &hdmi_info, sizeof(hdmi_info))) {
				if (hdmi_bufferdump_on > 0) {
					MMProfileLogEx(ddp_mmp_get_events()->Extd_ErrorInfo,
						       MMProfileFlagPulse, Devinfo, 1);
				}

				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}

			if (hdmi_bufferdump_on > 0) {
				MMProfileLogEx(ddp_mmp_get_events()->Extd_DevInfo, MMProfileFlagEnd,
					       p->is_enabled, hdmi_info.displayType);
			}

			HDMI_LOG("DEV_INFO configuration get displayType-%d\n",
				 hdmi_info.displayType);

			break;
		}
	case MTK_HDMI_SCREEN_CAPTURE:
		{
			int capture_wait_times = 0;

			capture_screen = true;

			if (copy_from_user(&capture_addr, (void __user *)arg, sizeof(capture_addr))) {
				HDMI_ERR(": copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}

			while (capture_wait_times < 3) {
				msleep(20);
				capture_wait_times++;

				if (capture_screen == false)
					break;
			}

			if (capture_screen == true) {
				HDMI_ERR("capture scree fail,is_enabled(%d), wait_times(%d)\n",
					 p->is_clock_on, capture_wait_times);
			} else {
				HDMI_LOG("screen_capture done,is_enabled(%d), wait_times(%d)\n",
					 p->is_clock_on, capture_wait_times);
			}

			capture_screen = false;
			break;
		}

	case MTK_HDMI_GET_EDID:
		{
			memset(&pv_get_info, 0, sizeof(pv_get_info));
			if (hdmi_drv->getedid)
				hdmi_drv->getedid(&pv_get_info);

			if (copy_to_user((void __user *)arg, &pv_get_info, sizeof(pv_get_info))) {
				HDMI_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}

			break;
		}
	default:
		{
			HDMI_ERR("hdmi ioctl(%d) arguments is not support\n", cmd & 0xff);
			r = -EFAULT;
			break;
		}
	}

	HDMI_LOG("hdmi ioctl = %s(%d) done\n", _hdmi_ioctl_spy(cmd), cmd & 0xff);
	return r;
}

void hdmi_switch_resolution(int res)
{
	HDMI_LOG("hdmi_switch_resolution %d\n", res);
	hdmi_ioctl(NULL, MTK_HDMI_VIDEO_CONFIG, res);
}

#if CONFIG_COMPAT
#if 0
static int compat_get_hdmitx_audio_para(COMPAT_HDMITX_AUDIO_PARA __user *data32,
					HDMITX_AUDIO_PARA __user *data)
{
	unsigned char c;
	int err;

	err = get_user(c, &data32->e_hdmi_aud_in);
	err |= put_user(c, &data->e_hdmi_aud_in);

	err |= get_user(c, &data32->e_iec_frame);
	err |= put_user(c, &data->e_iec_frame);

	err |= get_user(c, &data32->e_hdmi_fs);
	err |= put_user(c, &data->e_hdmi_fs);

	err |= get_user(c, &data32->e_aud_code);
	err |= put_user(c, &data->e_aud_code);

	err = get_user(c, &data32->u1Aud_Input_Chan_Cnt);
	err |= put_user(c, &data->u1Aud_Input_Chan_Cnt);

	err |= get_user(c, &data32->e_I2sFmt);
	err |= put_user(c, &data->e_I2sFmt);

	err |= get_user(c, &data32->u1HdmiI2sMclk);
	err |= put_user(c, &data->u1HdmiI2sMclk);

	err |= get_user(c, &data32->bhdmi_LCh_status);
	err |= put_user(c, &data->bhdmi_LCh_status);

	return err;
}
#endif

static int compat_put_edid(struct COMPAT_HDMI_EDID_T __user *data32, HDMI_EDID_T __user *data)
{
	compat_uint_t u;
	int err, i;

	err = get_user(u, &data->ui4_ntsc_resolution);
	err |= put_user(u, &data32->ui4_ntsc_resolution);

	err |= get_user(u, &data->ui4_pal_resolution);
	err |= put_user(u, &data32->ui4_pal_resolution);

	err |= get_user(u, &data->ui4_sink_native_ntsc_resolution);
	err |= put_user(u, &data32->ui4_sink_native_ntsc_resolution);

	err |= get_user(u, &data->ui4_sink_cea_ntsc_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_ntsc_resolution);

	err |= get_user(u, &data->ui4_sink_cea_pal_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_pal_resolution);

	err |= get_user(u, &data->ui4_sink_dtd_ntsc_resolution);
	err |= put_user(u, &data32->ui4_sink_dtd_ntsc_resolution);

	err |= get_user(u, &data->ui4_sink_1st_dtd_pal_resolution);
	err |= put_user(u, &data32->ui4_sink_1st_dtd_pal_resolution);

	err |= get_user(u, &data->ui2_sink_colorimetry);
	err |= put_user(u, &data32->ui2_sink_colorimetry);

	err |= get_user(u, &data->ui1_sink_rgb_color_bit);
	err |= put_user(u, &data32->ui1_sink_rgb_color_bit);

	err |= get_user(u, &data->ui1_sink_ycbcr_color_bit);
	err |= put_user(u, &data32->ui1_sink_ycbcr_color_bit);

	err |= get_user(u, &data->ui2_sink_aud_dec);
	err |= put_user(u, &data32->ui2_sink_aud_dec);

	err |= get_user(u, &data->ui1_sink_is_plug_in);
	err |= put_user(u, &data32->ui1_sink_is_plug_in);

	err |= get_user(u, &data->ui4_hdmi_pcm_ch_type);
	err |= put_user(u, &data32->ui4_hdmi_pcm_ch_type);

	err |= get_user(u, &data->ui4_hdmi_pcm_ch3ch4ch5ch7_type);
	err |= put_user(u, &data32->ui4_hdmi_pcm_ch3ch4ch5ch7_type);

	err |= get_user(u, &data->ui4_dac_pcm_ch_type);
	err |= put_user(u, &data32->ui4_dac_pcm_ch_type);

	err |= get_user(u, &data->ui1_sink_i_latency_present);
	err |= put_user(u, &data32->ui1_sink_i_latency_present);

	err |= get_user(u, &data->ui1_sink_p_audio_latency);
	err |= put_user(u, &data32->ui1_sink_p_audio_latency);

	err |= get_user(u, &data->ui1_sink_p_video_latency);
	err |= put_user(u, &data32->ui1_sink_p_video_latency);

	err |= get_user(u, &data->ui1_sink_i_audio_latency);
	err |= put_user(u, &data32->ui1_sink_i_audio_latency);

	err |= get_user(u, &data->ui1ExtEdid_Revision);
	err |= put_user(u, &data32->ui1ExtEdid_Revision);

	err |= get_user(u, &data->ui1Edid_Version);
	err |= put_user(u, &data32->ui1Edid_Version);

	err |= get_user(u, &data->ui1Edid_Revision);
	err |= put_user(u, &data32->ui1Edid_Revision);

	err |= get_user(u, &data->ui1_Display_Horizontal_Size);
	err |= put_user(u, &data32->ui1_Display_Horizontal_Size);

	err |= get_user(u, &data->ui1_Display_Vertical_Size);
	err |= put_user(u, &data32->ui1_Display_Vertical_Size);

	err |= get_user(u, &data->ui4_ID_Serial_Number);
	err |= put_user(u, &data32->ui4_ID_Serial_Number);

	err |= get_user(u, &data->ui4_sink_cea_3D_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_3D_resolution);

	err |= get_user(u, &data->ui1_sink_support_ai);
	err |= put_user(u, &data32->ui1_sink_support_ai);

	err |= get_user(u, &data->ui2_sink_cec_address);
	err |= put_user(u, &data32->ui2_sink_cec_address);

	err |= get_user(u, &data->ui1_sink_max_tmds_clock);
	err |= put_user(u, &data32->ui1_sink_max_tmds_clock);

	err |= get_user(u, &data->ui2_sink_3D_structure);
	err |= put_user(u, &data32->ui2_sink_3D_structure);

	err |= get_user(u, &data->ui4_sink_cea_FP_SUP_3D_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_FP_SUP_3D_resolution);

	err |= get_user(u, &data->ui4_sink_cea_TOB_SUP_3D_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_TOB_SUP_3D_resolution);

	err |= get_user(u, &data->ui4_sink_cea_SBS_SUP_3D_resolution);
	err |= put_user(u, &data32->ui4_sink_cea_SBS_SUP_3D_resolution);

	err |= get_user(u, &data->ui2_sink_ID_product_code);
	err |= put_user(u, &data32->ui2_sink_ID_product_code);

	err |= get_user(u, &data->ui4_sink_ID_serial_number);
	err |= put_user(u, &data32->ui4_sink_ID_serial_number);

	err |= get_user(u, &data->ui1_sink_week_of_manufacture);
	err |= put_user(u, &data32->ui1_sink_week_of_manufacture);

	err |= get_user(u, &data->ui1_sink_year_of_manufacture);
	err |= put_user(u, &data32->ui1_sink_year_of_manufacture);

	err |= get_user(u, &data->b_sink_SCDC_present);
	err |= put_user(u, &data32->b_sink_SCDC_present);

	err |= get_user(u, &data->b_sink_LTE_340M_sramble);
	err |= put_user(u, &data32->b_sink_LTE_340M_sramble);

	err |= get_user(u, &data->ui4_sink_hdmi_4k2kvic);
	err |= put_user(u, &data32->ui4_sink_hdmi_4k2kvic);

	for (i = 0; i < 512; i++) {
		err |= get_user(u, &data->ui1rawdata_edid[i]);
		err |= put_user(u, &data32->ui1rawdata_edid[i]);
	}

	return err;
}

static long hdmi_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	HDMI_LOG(">> hdmi_ioctl_compat\n");

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_MTK_HDMI_AUDIO_SETTING:
		{
			struct COMPAT_HDMITX_AUDIO_PARA __user *data32;	/* userspace passed argument */
			HDMITX_AUDIO_PARA __user *data;	/* kernel used */

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));

			if (data == NULL)
				return -EFAULT;

			/*For hdmi_audio_setting, unsigned char type dont need to be convert */
			ret =
			    file->f_op->unlocked_ioctl(file, MTK_HDMI_AUDIO_SETTING,
						       (unsigned long)data32);
			return ret;
		}
	case COMPAT_MTK_HDMI_GET_EDID:
		{
			struct COMPAT_HDMI_EDID_T __user *data32;
			HDMI_EDID_T __user *data;
			int err = 0;

			data32 = compat_ptr(arg);	/* userspace passed argument */
			data = compat_alloc_user_space(sizeof(*data));

			if (data == NULL)
				return -EFAULT;

			ret =
			    file->f_op->unlocked_ioctl(file, MTK_HDMI_GET_EDID,
						       (unsigned long)data);
			err = compat_put_edid(data32, data);

			return ret ? ret : err;
		}
	default:
		HDMI_ERR("Unknown ioctl compat\n");
		break;

	}
	return ret;
}
#endif


static int hdmi_remove(struct platform_device *pdev)
{
	return 0;
}

static bool hdmi_drv_init_context(void)
{
	static const struct HDMI_UTIL_FUNCS hdmi_utils = {
		.udelay = hdmi_udelay,
		.mdelay = hdmi_mdelay,
		.state_callback = hdmi_state_callback,
#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
		.cec_state_callback = hdmi_cec_state_callback,
#endif
	};

	if (hdmi_drv != NULL)
		return TRUE;

	hdmi_drv = (struct HDMI_DRIVER *) HDMI_GetDriver();

	if (NULL == hdmi_drv)
		return FALSE;

	hdmi_drv->set_util_funcs(&hdmi_utils);
	hdmi_drv->get_params(hdmi_params);

	return TRUE;
}

static void hdmi_power_enable(int enable)
{
	hdmi_ioctl(NULL, MTK_HDMI_POWER_ENABLE, enable);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmi_early_suspend(struct early_suspend *h)
{
	hdmi_power_enable(0);
}

static void hdmi_late_resume(struct early_suspend *h)
{
	hdmi_power_enable(1);
}

static struct early_suspend hdmi_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	.suspend = hdmi_early_suspend,
	.resume = hdmi_late_resume,
};
#endif

#ifdef CONFIG_PM_AUTOSLEEP
static void hdmi_blank_suspend(void)
{
	hdmi_power_enable(0);
}

static void hdmi_blank_resume(void)
{
	hdmi_power_enable(1);
}


static int mtk_ext_event_notify(struct notifier_block *self,
				unsigned long action, void *data)
{
	if (action != FB_EARLY_EVENT_BLANK)
		return 0;

	{
		struct fb_event *event = data;
		int blank_mode = *((int *)event->data);

		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
			hdmi_blank_resume();
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			break;
		case FB_BLANK_POWERDOWN:
			hdmi_blank_suspend();
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}


static struct notifier_block mtk_ext_event_notifier = {
	.notifier_call  = mtk_ext_event_notify,
};
#endif

static void __exit hdmi_exit(void)
{

	device_destroy(hdmi_class, hdmi_devno);
	class_destroy(hdmi_class);
	cdev_del(hdmi_cdev);
	unregister_chrdev_region(hdmi_devno, 1);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hdmi_early_suspend_handler);
#endif
#ifdef CONFIG_PM_AUTOSLEEP
	fb_unregister_client(&mtk_ext_event_notifier);
#endif
}


static int hdmi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct class_device *class_dev = NULL;

	DISPMSG("[hdmi]%s\n", __func__);

	/* Allocate device number for hdmi driver */
	ret = alloc_chrdev_region(&hdmi_devno, 0, 1, HDMI_DEVNAME);

	if (ret) {
		DISPERR("[hdmi]alloc_chrdev_region fail\n");
		return -1;
	}

	/* For character driver register to system, device number binded to file operations */
	hdmi_cdev = cdev_alloc();
	hdmi_cdev->owner = THIS_MODULE;
	hdmi_cdev->ops = &hdmi_fops;
	ret = cdev_add(hdmi_cdev, hdmi_devno, 1);

	/* For device number binded to device name(hdmitx), one class is corresponeded to one node */
	hdmi_class = class_create(THIS_MODULE, HDMI_DEVNAME);
	/* mknod /dev/hdmitx */
	class_dev =
	    (struct class_device *)device_create(hdmi_class, NULL, hdmi_devno, NULL, HDMI_DEVNAME);

	/* DISPMSG("[hdmi][%s] current=0x%08x\n", __func__, (unsigned int)(unsigned long)current); */

	if (!hdmi_drv_init_context()) {
		DISPMSG("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
	ret = hdmi_drv->hdmidrv_probe(pdev, hdmi_reschange);
	if (ret) {
		HDMI_ERR("Fail to probe internal hdmi\n");
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}
#endif
#if defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	hdmi_drv->probe();
#endif
	init_waitqueue_head(&hdmi_rdma_config_wq);
	init_waitqueue_head(&hdmi_vsync_wq);

#if defined(HDMI_SUB_PATH) && defined(HDMI_SUB_PATH_PROB)
	DISPMSG("%s dpi_setting_res\n", __func__);

	dpi_setting_res(0xb);

#endif

#if HDMI_SUB_PATH_BOOT

	/* hdmi_ioctl(NULL, MTK_HDMI_POWER_ENABLE, 1); */

	hdmi_ioctl(NULL, MTK_HDMI_AUDIO_VIDEO_ENABLE, 1);

#endif


	return 0;
}


static int __init hdmi_init(void)
{
	int ret = 0;
	int tmp_boot_mode;

	DISPMSG("[hdmi]%s\n", __func__);

	memset((void *)&hdmi_context, 0, sizeof(_t_hdmi_context));
	memset((void *)&extd_dpi_params, 0, sizeof(extd_dpi_params));

	SET_HDMI_OFF();

	if (!hdmi_drv_init_context()) {
		DISPMSG("[hdmi]%s, hdmi_drv_init_context fail\n", __func__);
		return HDMI_STATUS_NOT_IMPLEMENTED;
	}

	tmp_boot_mode = get_boot_mode();
	if ((tmp_boot_mode == FACTORY_BOOT) || (tmp_boot_mode == ATE_FACTORY_BOOT))
		factory_mode = true;

	p->output_mode = hdmi_params->output_mode;
	p->orientation = 0;
#ifdef HDMI_SUB_PATH
	p->is_mhl_video_on = true;
#else
	p->is_mhl_video_on = false;
#endif
	if (factory_mode == false)
		p->is_enabled = true;

	hdmi_drv->init();

	HDMI_DBG_Init();

	hdmi_switch_data.name = "hdmi";
	hdmi_switch_data.index = 0;
	hdmi_switch_data.state = NO_DEVICE;

	/* for support hdmi hotplug, inform AP the event */
	ret = switch_dev_register(&hdmi_switch_data);

	hdmires_switch_data.name = "res_hdmi";
	hdmires_switch_data.index = 0;
	hdmires_switch_data.state = 0;

	/* for support hdmi hotplug, inform AP the event */
	ret = switch_dev_register(&hdmires_switch_data);

	hdmi_cec_switch_data.name = "cec_hdmi";
	hdmi_cec_switch_data.index = 0;
	hdmi_cec_switch_data.state = 0;
	ret = switch_dev_register(&hdmi_cec_switch_data);

	hdmi_audio_switch_data.name = "audio_hdmi";
	hdmi_audio_switch_data.index = 0;
	hdmi_audio_switch_data.state = 0;
	ret = switch_dev_register(&hdmi_audio_switch_data);

	if (ret) {
		DISPMSG("[hdmi][HDMI]switch_dev_register returned:%d!\n", ret);
		return 1;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hdmi_early_suspend_handler);
#endif
#ifdef CONFIG_PM_AUTOSLEEP
	fb_register_client(&mtk_ext_event_notifier);
#endif

	if (platform_driver_register(&hdmi_driver)) {
		pr_err("[hdmi]failed to register mtkfb driver\n");
		return -1;
	}

	return 0;
}
module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_AUTHOR("www.mediatek.com>");
MODULE_DESCRIPTION("HDMI Driver");
MODULE_LICENSE("GPL");

#endif
