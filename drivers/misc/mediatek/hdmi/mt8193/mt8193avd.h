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

#ifndef __mt8193avd_h__
#define __mt8193avd_h__
#ifdef HDMI_MT8193_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "hdmi_drv.h"

enum _AV_D_HDMI_DRV_SET_TYPE_T {
	HDMI_SET_VIDEO_RES_CHG = 1,
	HDMI_SET_AUDIO_OUT_CHANNEL,
	HDMI_SET_AUDIO_OUTPUT_TYPE,
	HDMI_SET_AUDIO_PACKET_OFF,
	HDMI_SET_VIDEO_COLOR_SPACE,
	HDMI_SET_VIDEO_CONTRAST,
	HDMI_SET_VIDEO_BRIGHTNESS,
	HDMI_SET_VIDEO_HUE,
	HDMI_SET_VIDEO_SATURATION,
	HDMI_SET_ASPECT_RATIO,
	HDMI_SET_AVD_NFY_FCT,
	HDMI_SET_TURN_OFF_TMDS,
	HDMI_SET_AUDIO_CHG_SETTING,
	HDMI_SET_AVD_INF_ADDRESS,
	HDMI_SET_HDCP_INITIAL_AUTH,
	HDMI_SET_VPLL,
	HDMI_SET_SOFT_NCTS,
	HDMI_SET_HDCP_OFF,
	HDMI_SET_VIDEO_SHARPNESS,
	HDMI_SET_TMDS,
	HDMI_SENT_GCP
};
#define AV_D_HDMI_DRV_SET_TYPE_T enum _AV_D_HDMI_DRV_SET_TYPE_T

extern void av_hdmiset(AV_D_HDMI_DRV_SET_TYPE_T e_set_type, const void *pv_set_info,
		       unsigned char z_set_info_len);

#endif
#endif
