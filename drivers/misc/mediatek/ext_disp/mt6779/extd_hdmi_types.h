/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __EXTD_HDMI_TYPES_H__
#define __EXTD_HDMI_TYPES_H__

#include <linux/list.h>
#include <linux/atomic.h>

#include "hdmi_drv.h"
#include "mtk_sync.h"
#include "extd_log.h"

/* typedef definition declare */
enum HDMI_STATUS {
	HDMI_STATUS_OK = 0,
	HDMI_STATUS_NOT_IMPLEMENTED,
	HDMI_STATUS_ALREADY_SET,
	HDMI_STATUS_ERROR,
};

enum SMART_BOOK_STATE {
	SMART_BOOK_DISCONNECTED = 0,
	SMART_BOOK_CONNECTED,
};

enum HDMI_POWER_STATE {
	HDMI_POWER_STATE_OFF = 0,
	HDMI_POWER_STATE_ON,
	HDMI_POWER_STATE_STANDBY,
};

enum hdmi_device_type {
	HDMI_TO_TV = 0x0,
	HDMI_TO_SMB,
};

enum hdmi_connect_status {
	HDMI_IS_DISCONNECTED = 0,
	HDMI_IS_CONNECTED = 1,
	HDMI_IS_RES_CHG = 0x11,
};

#define MAKE_MTK_HDMI_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))
enum MTK_HDMI_FORMAT {
	MTK_HDMI_FORMAT_UNKNOWN = 0,

	MTK_HDMI_FORMAT_RGB565 = MAKE_MTK_HDMI_FORMAT_ID(1, 2),
	MTK_HDMI_FORMAT_RGB888 = MAKE_MTK_HDMI_FORMAT_ID(2, 3),
	MTK_HDMI_FORMAT_BGR888 = MAKE_MTK_HDMI_FORMAT_ID(3, 3),
	MTK_HDMI_FORMAT_ARGB8888 = MAKE_MTK_HDMI_FORMAT_ID(4, 4),
	MTK_HDMI_FORMAT_ABGR8888 = MAKE_MTK_HDMI_FORMAT_ID(5, 4),
	MTK_HDMI_FORMAT_YUV422 = MAKE_MTK_HDMI_FORMAT_ID(6, 2),
	MTK_HDMI_FORMAT_XRGB8888 = MAKE_MTK_HDMI_FORMAT_ID(7, 4),
	MTK_HDMI_FORMAT_XBGR8888 = MAKE_MTK_HDMI_FORMAT_ID(8, 4),
	MTK_HDMI_FORMAT_BPP_MASK = 0xFF,
};

struct hdmi_video_buffer_info {
	void *src_base_addr;
	void *src_phy_addr;
	int src_fmt;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	int next_buff_idx;
	int identity;
	int connected_type;
	unsigned int security;
};

struct hdmi_buffer_info {
	int ion_fd;
	unsigned int index;	/* fence count */
	int fence_fd;		/* fence fd */
};

enum Extd_State {
	Plugout = 0,
	Plugin,
	ResChange,
	Devinfo,
	Power_on = 4,
	Power_off,
	Config,
	Trigger = 7,
	Suspend,
	Resume
};

struct _t_hdmi_context {
	bool is_enabled;
	/* whether HDMI is enabled or disabled by user */
	bool is_force_disable;
	/* used for camera scenario */
	bool is_clock_on;
	/* DPI is running or not */
	bool is_mhl_video_on;
	/* DPI is running or not */
	atomic_t state;
	/* HDMI_POWER_STATE state */
	int lcm_width;
	/* LCD write buffer width */
	int lcm_height;
	/* LCD write buffer height */
	int hdmi_width;
	/* DPI read buffer width */
	int hdmi_height;
	/* DPI read buffer height */
	int bg_width;
	/* DPI read buffer width */
	int bg_height;
	/* DPI read buffer height */
	int orientation;
	int scaling_factor;
	enum HDMI_VIDEO_RESOLUTION output_video_resolution;
	enum HDMI_AUDIO_FORMAT output_audio_format;
	enum HDMI_OUTPUT_MODE output_mode;
	enum HDMI_VIDEO_INPUT_FORMAT vin;
	enum HDMI_VIDEO_OUTPUT_FORMAT vout;
};

/* the definition */
#define HDMI_DEVNAME "hdmitx"

#define HDMI_DPI(suffix)        DPI  ## suffix
#if defined(CONFIG_MACH_MT8167)
#define HMID_DEST_DPI           DISP_MODULE_DPI1
#else
#define HMID_DEST_DPI           DISP_MODULE_DPI
#endif

#define MTK_HDMI_NO_FENCE_FD	((int)(-1)) /* ((int)(~0U>>1)) */
#define MTK_HDMI_NO_ION_FD	((int)(-1)) /* ((int)(~0U>>1)) */

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))
#define hdmi_abs(a) (((a) < 0) ? -(a) : (a))

#endif /* __EXTD_HDMI_TYPES_H__ */
