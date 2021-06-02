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

/**
 * NOTICE:
 * MUST BE consistent with bionic/libc/kernel/common/linux/disp_svp.h
 */
#ifndef __DISP_SVP_H
#define __DISP_SVP_H

#define DISP_NO_ION_FD                 ((int)(~0U>>1))
#define DISP_NO_USE_LAEYR_ID           ((int)(~0U>>1))

#define MAX_INPUT_CONFIG		4
#define MAKE_DISP_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))

/* /========================================================================= */
/* structure declarations */
/* /=========================== */

enum DISP_IF_TYPE {
	DISP_IF_TYPE_DBI = 0,
	DISP_IF_TYPE_DPI,
	DISP_IF_TYPE_DSI0,
	DISP_IF_TYPE_DSI1,
	DISP_IF_TYPE_DSIDUAL,
	DISP_IF_HDMI = 7,
	DISP_IF_HDMI_SMARTBOOK,
	DISP_IF_MHL,
	DISP_IF_EPD,
	DISP_IF_SLIMPORT
};

enum DISP_IF_FORMAT {
	DISP_IF_FORMAT_RGB565 = 0,
	DISP_IF_FORMAT_RGB666,
	DISP_IF_FORMAT_RGB888
};

enum DISP_IF_MODE {
	DISP_IF_MODE_VIDEO = 0,
	DISP_IF_MODE_COMMAND
};


enum DISP_ORIENTATION {
	DISP_ORIENTATION_0 = 0,
	DISP_ORIENTATION_90 = 1,
	DISP_ORIENTATION_180 = 2,
	DISP_ORIENTATION_270 = 3,
};

enum DISP_FORMAT {
	DISP_FORMAT_UNKNOWN = 0,

	DISP_FORMAT_RGB565 = MAKE_DISP_FORMAT_ID(1, 2),
	DISP_FORMAT_RGB888 = MAKE_DISP_FORMAT_ID(2, 3),
	DISP_FORMAT_BGR888 = MAKE_DISP_FORMAT_ID(3, 3),
	DISP_FORMAT_ARGB8888 = MAKE_DISP_FORMAT_ID(4, 4),
	DISP_FORMAT_ABGR8888 = MAKE_DISP_FORMAT_ID(5, 4),
	DISP_FORMAT_RGBA8888 = MAKE_DISP_FORMAT_ID(6, 4),
	DISP_FORMAT_BGRA8888 = MAKE_DISP_FORMAT_ID(7, 4),
	DISP_FORMAT_XRGB8888 = MAKE_DISP_FORMAT_ID(8, 4),
	DISP_FORMAT_XBGR8888 = MAKE_DISP_FORMAT_ID(9, 4),

	/* Packed YUV Formats */
	DISP_FORMAT_YUV444 = MAKE_DISP_FORMAT_ID(10, 3),
	/* Same as UYVY, but replace Y/U/V */
	DISP_FORMAT_YVYU = MAKE_DISP_FORMAT_ID(11, 2),
	DISP_FORMAT_VYUY = MAKE_DISP_FORMAT_ID(12, 2),
	DISP_FORMAT_UYVY = MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_Y422 = MAKE_DISP_FORMAT_ID(13, 2),
	/* Same as UYVY but replace U/V */
	DISP_FORMAT_YUYV = MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YUY2 = MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YUV422 = MAKE_DISP_FORMAT_ID(14, 2),  /* Will be removed */
	DISP_FORMAT_GREY = MAKE_DISP_FORMAT_ID(15, 1),	/* Single Y plane */
	DISP_FORMAT_Y800 = MAKE_DISP_FORMAT_ID(15, 1),
	DISP_FORMAT_Y8 = MAKE_DISP_FORMAT_ID(15, 1),

	/* Planar YUV Formats */
	DISP_FORMAT_YV12 = MAKE_DISP_FORMAT_ID(16, 1),	/* BPP = 1.5 */
	/* Same as YV12 but replace U/V */
	DISP_FORMAT_I420 = MAKE_DISP_FORMAT_ID(17, 1),
	DISP_FORMAT_IYUV = MAKE_DISP_FORMAT_ID(17, 1),
	DISP_FORMAT_NV12 = MAKE_DISP_FORMAT_ID(18, 1),	/* BPP = 1.5 */
	/* Same as NV12 but replace U/V */
	DISP_FORMAT_NV21 = MAKE_DISP_FORMAT_ID(19, 1),

	DISP_FORMAT_BPP_MASK = 0xFFFF,
};

enum DISP_LAYER_TYPE {
	DISP_LAYER_2D = 0,
	DISP_LAYER_3D_SBS_0 = 0x1,
	DISP_LAYER_3D_SBS_90 = 0x2,
	DISP_LAYER_3D_SBS_180 = 0x3,
	DISP_LAYER_3D_SBS_270 = 0x4,
	DISP_LAYER_3D_TAB_0 = 0x10,
	DISP_LAYER_3D_TAB_90 = 0x20,
	DISP_LAYER_3D_TAB_180 = 0x30,
	DISP_LAYER_3D_TAB_270 = 0x40,
};

enum DISP_BUFFER_TYPE {
	/* normal memory */
	DISP_NORMAL_BUFFER = 0,
	/* secure memory */
	DISP_SECURE_BUFFER = 1,
	/* normal memory but should not be dumpped within screenshot */
	DISP_PROTECT_BUFFER = 2,
	DISP_SECURE_BUFFER_SHIFT = 0x10001
};

enum DISP_ALPHA_TYPE {
	DISP_ALPHA_ONE = 0,
	DISP_ALPHA_SRC = 1,
	DISP_ALPHA_SRC_INVERT = 2,
	DISP_ALPHA_INVALID = 3,
};

enum DISP_SESSION_TYPE {
	DISP_SESSION_PRIMARY = 1,
	DISP_SESSION_EXTERNAL = 2,
	DISP_SESSION_MEMORY = 3
};

enum DISP_MODE {
	/* single output */
	DISP_SESSION_DIRECT_LINK_MODE = 1,
	DISP_SESSION_DECOUPLE_MODE = 2,

	/* two ouputs */
	DISP_SESSION_DIRECT_LINK_MIRROR_MODE = 3,
	DISP_SESSION_DECOUPLE_MIRROR_MODE = 4,
};

enum EXTD_TRIGGER_MODE {
	TRIGGER_NORMAL,
	TRIGGER_SUSPEND,
	TRIGGER_RESUME,

	TRIGGER_MODE_MAX_NUM
};

struct disp_session_config {
	enum DISP_SESSION_TYPE type;
	unsigned int device_id;
	enum DISP_MODE mode;
	unsigned int session_id;
	enum EXTD_TRIGGER_MODE tigger_mode;
};

struct disp_session_vsync_config {
	unsigned int session_id;
	unsigned int vsync_cnt;
	long int vsync_ts;
};

struct disp_input_config {
	unsigned int layer_id;
	unsigned int layer_enable;

	void *src_base_addr;
	void *src_phy_addr;
	unsigned int src_direct_link;
	enum DISP_FORMAT src_fmt;
	unsigned int src_use_color_key;
	unsigned int src_color_key;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	unsigned int tgt_offset_x, tgt_offset_y;
	unsigned int tgt_width, tgt_height;
	enum DISP_ORIENTATION layer_rotation;
	enum DISP_LAYER_TYPE layer_type;
	enum DISP_ORIENTATION video_rotation;

	/* if 1, go through tdshp first, then layer blending, then to color */
	unsigned int isTdshp;

	unsigned int next_buff_idx;
	int identity;
	int connected_type;
	enum DISP_BUFFER_TYPE security;
	unsigned int alpha_enable;
	unsigned int alpha;
	unsigned int sur_aen;
	DISP_ALPHA_TYPE src_alpha;
	DISP_ALPHA_TYPE dst_alpha;
};

struct disp_output_config {
	unsigned int va;
	unsigned int pa;
	enum DISP_FORMAT fmt;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int pitchUV;
	enum DISP_BUFFER_TYPE security;
	unsigned int buff_idx;
};

#define MAX_INPUT_CONFIG 4

struct disp_session_input_config {
	unsigned int session_id;
	unsigned int config_layer_num;
	disp_input_config config[MAX_INPUT_CONFIG];
};

struct disp_session_output_config {
	unsigned int session_id;
	disp_output_config config;
};

struct disp_session_layer_num_config {
	unsigned int session_id;
	unsigned int max_layer_num;
};

struct disp_session_info {
	unsigned int session_id;
	unsigned int maxLayerNum;
	unsigned int isHwVsyncAvailable;
	enum DISP_IF_TYPE displayType;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	enum DISP_IF_MODE displayMode;
	unsigned int vsyncFPS;
	unsigned int physicalWidth;	/* length: mm, for legacy use */
	unsigned int physicalHeight;	/* length: mm, for legacy use */
	/* length: um, for more precise precision */
	unsigned int physicalWidthUm;
	/* length: um, for more precise precision */
	unsigned int physicalHeightUm;
	unsigned int isConnected;
};

struct disp_buffer_info {
	/* Session */
	unsigned int session_id;
	/* Input */
	unsigned int layer_id;
	unsigned int layer_en;
	int ion_fd;
	unsigned int cache_sync;
	/* Output */
	unsigned int index;
	int fence_fd;
};
/* IOCTL commands. */
#define DISP_IOW(num, dtype)     _IOW('O', num, dtype)
#define DISP_IOR(num, dtype)     _IOR('O', num, dtype)
#define DISP_IOWR(num, dtype)    _IOWR('O', num, dtype)
#define DISP_IO(num)             _IO('O', num)


#define	DISP_IOCTL_CREATE_SESSION      DISP_IOW(201, struct disp_session_config)
#define	DISP_IOCTL_DESTROY_SESSION     DISP_IOW(202, struct disp_session_config)
#define	DISP_IOCTL_TRIGGER_SESSION     DISP_IOW(203, struct disp_session_config)
#define DISP_IOCTL_PREPARE_INPUT_BUFFER	  DISP_IOW(204, struct disp_buffer_info)
#define DISP_IOCTL_PREPARE_OUTPUT_BUFFER  DISP_IOW(205, struct disp_buffer_info)
#define DISP_IOCTL_SET_INPUT_BUFFER  \
				 DISP_IOW(206, struct disp_session_input_config)
#define DISP_IOCTL_SET_OUTPUT_BUFFER \
				DISP_IOW(207, struct disp_session_output_config)
#define	DISP_IOCTL_GET_SESSION_INFO      DISP_IOW(208, struct disp_session_info)

#define	DISP_IOCTL_SET_SESSION_MODE    DISP_IOW(209, struct disp_session_config)
#define	DISP_IOCTL_GET_SESSION_MODE    DISP_IOW(210, struct disp_session_config)
#define	DISP_IOCTL_SET_SESSION_TYPE    DISP_IOW(211, struct disp_session_config)
#define	DISP_IOCTL_GET_SESSION_TYPE    DISP_IOW(212, struct disp_session_config)
#define	DISP_IOCTL_WAIT_FOR_VSYNC  \
				 DISP_IOW(213, struct disp_session_vsync_config)
#define	DISP_IOCTL_SET_MAX_LAYER_NUM \
			     DISP_IOW(214, struct disp_session_layer_num_config)


#endif				/* __DISP_SVP_H */
