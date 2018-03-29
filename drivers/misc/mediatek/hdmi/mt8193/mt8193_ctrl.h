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

#ifndef __mt8193_ctrl_h__
#define __mt8193_ctrl_h__
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
/*#include <linux/earlysuspend.h>*/
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
#include <linux/types.h>
#include <linux/types.h>

#include "mach/irqs.h"

#include "hdmi_drv.h"

#include "ddp_hal.h"
#include "mt8193hdcp.h"

extern unsigned int mt8193_log_on;

#define hdmiplllog         (0x1)
#define hdmidgilog         (0x2)
#define hdmitxhotpluglog   (0x4)
#define hdmitxvideolog     (0x8)
#define hdmitxaudiolog     (0x10)
#define hdmihdcplog        (0x20)
#define hdmiceclog         (0x40)
#define hdmiddclog         (0x80)
#define hdmiedidlog        (0x100)
#define hdmidrvlog         (0x200)
#define hdmideflog         (0x400)

#define mt8193hdmialllog   (0xffff)

#define hdmi_print pr_debug

#define TRUE true
#define FALSE false
/* ////////////////////////////////////////////PLL////////////////////////////////////////////////////// */
#define MT8193_PLL_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmiplllog) {\
			hdmi_print("[hdmi_pll]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_PLL_FUNC()	\
	do {\
		if (mt8193_log_on&hdmiplllog) {\
			hdmi_print("[hdmi_pll] %s\n", __func__); } \
	} while (0)

/* ////////////////////////////////////////////DGI////////////////////////////////////////////////////// */

#define MT8193_DGI_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmidgilog) {\
			hdmi_print("[hdmi_dgi1]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_DGI_FUNC()	\
	do {\
		if (mt8193_log_on&hdmidgilog) {\
			hdmi_print("[hdmi_dgi1] %s\n", __func__); } \
	} while (0)

/* ////////////////////////////////////////////PLUG////////////////////////////////////////////////////// */

#define MT8193_PLUG_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmitxhotpluglog) {\
			hdmi_print("[hdmi_plug]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_PLUG_FUNC()	\
	do {\
		if (mt8193_log_on&hdmitxhotpluglog) {\
			hdmi_print("[hdmi_plug] %s\n", __func__); } \
	} while (0)

/* //////////////////////////////////////////////VIDEO//////////////////////////////////////////////////// */

#define MT8193_VIDEO_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmitxvideolog) {\
			hdmi_print("[hdmi_video]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_VIDEO_FUNC()	\
	do {\
		if (mt8193_log_on&hdmitxvideolog) {\
			hdmi_print("[hdmi_video] %s\n", __func__); } \
	} while (0)

/* //////////////////////////////////////////////AUDIO//////////////////////////////////////////////////// */

#define MT8193_AUDIO_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmitxaudiolog) {\
			hdmi_print("[hdmi_audio]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_AUDIO_FUNC()	\
	do {\
		if (mt8193_log_on&hdmitxaudiolog) {\
			hdmi_print("[hdmi_audio] %s\n", __func__); } \
	} while (0)
/* ///////////////////////////////////////////////HDCP/////////////////////////////////////////////////// */

#define MT8193_HDCP_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmihdcplog) {\
			hdmi_print("[hdmi_hdcp]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_HDCP_FUNC()	\
	do {\
		if (mt8193_log_on&hdmihdcplog) {\
			hdmi_print("[hdmi_hdcp] %s\n", __func__); } \
	} while (0)

/* ///////////////////////////////////////////////CEC/////////////////////////////////////////////////// */

#define MT8193_CEC_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmiceclog) {\
			hdmi_print("[hdmi_cec]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_CEC_FUNC()	\
	do {\
		if (mt8193_log_on&hdmiceclog) {\
			hdmi_print("[hdmi_cec] %s\n", __func__); } \
	} while (0)
/* ///////////////////////////////////////////////DDC////////////////////////////////////////////////// */
#define MT8193_DDC_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmiddclog) {\
			hdmi_print("[hdmi_ddc]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_DDC_FUNC()	\
	do {\
		if (mt8193_log_on&hdmiddclog) {\
			hdmi_print("[hdmi_ddc] %s\n", __func__); } \
	} while (0)
/* ///////////////////////////////////////////////EDID////////////////////////////////////////////////// */
#define MT8193_EDID_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmiedidlog) {\
			hdmi_print("[hdmi_edid]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_EDID_FUNC()	\
	do {\
		if (mt8193_log_on&hdmiedidlog) {\
			hdmi_print("[hdmi_edid] %s\n", __func__); } \
	} while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */

#define MT8193_DRV_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmidrvlog) {\
			hdmi_print("[hdmi_drv]%s,%d ", __func__, __LINE__);\
			hdmi_print(fmt, ##arg); } \
	} while (0)

#define MT8193_DRV_FUNC()	\
	do {\
		if (mt8193_log_on&hdmidrvlog) {\
			hdmi_print("[hdmi_drv] %s\n", __func__); } \
	} while (0)
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */
#define HDMI_DEF_LOG(fmt, arg...) \
	do {\
		if (mt8193_log_on&hdmideflog) {\
			pr_debug(fmt, ##arg); } \
	} while (0)
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */
extern int mt8193_i2c_read(unsigned short addr, unsigned int *data);
extern int mt8193_i2c_write(unsigned short addr, unsigned int data);
extern void mt8193_write(unsigned short u2Reg, unsigned int u4Data);
extern void vSetClk(void);
extern unsigned long dispsys_reg[DISP_REG_NUM];
extern HDMI_CTRL_STATE_T e_hdmi_ctrl_state;
extern HDCP_CTRL_STATE_T e_hdcp_ctrl_state;
extern size_t mt8193_TmrValue[MAX_HDMI_TMR_NUMBER];
extern size_t mt8193_hdmiCmd;

#endif
#endif
