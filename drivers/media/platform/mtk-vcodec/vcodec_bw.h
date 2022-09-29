/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __VCODEC_BW_H__
#define __VCODEC_BW_H__

#include <linux/types.h>
#include <linux/list.h>

#define DEFAULT_VENC_CONFIG -1000

enum vcodec_port_type {
	VCODEC_PORT_BITSTREAM = 0,
	VCODEC_PORT_PICTURE_Y = 1,
	VCODEC_PORT_PICTURE_UV = 2,
	VCODEC_PORT_PICTURE_ALL = 3,
	VCODEC_PORT_RCPU = 4,
	VCODEC_PORT_WORKING = 5,
	VCODEC_PORT_LARB_SUM = 6
};

enum vcodec_larb_type {
	VCODEC_LARB_READ = 0,
	VCODEC_LARB_WRITE = 1,
	VCODEC_LARB_READ_WRITE = 2,
	VCODEC_LARB_SUM = 3
};

struct vcodec_port_bw {
	int port_type;
	u32 port_base_bw;
	u32 larb;
};

struct vcodec_larb_bw {
	int larb_type;
	u32 larb_base_bw;
	u32 larb_id;
};

#endif
