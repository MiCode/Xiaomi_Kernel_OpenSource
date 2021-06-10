/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_DIP_HW_H_
#define _MTK_DIP_HW_H_

#include <linux/clk.h>
#include "mtk-img-ipi.h"
#include "mtkdip.h"

#define MTK_MDP_SUPPORT			(1)

#define DIP_COMPOSING_MAX_NUM		8

#define MTK_DIP_CLK_NUM			2

#define SUPPORT_SINGLE_DEVICE_MODE


#define DIP_SUB_FRM_SZ		0 //(68*1024) /*  tdri:64k + tpipe_cfg:4k  */
/*  isp register:48k + cq_descriptor:4k  */
#define DIP_TUNING_SZ		0 //(52*1024)
#define DIP_COMP_SZ		0 //((sizeof(struct img_config) + 1024) & ~1024)
#define DIP_FRAMEPARAM_SZ \
			((sizeof(struct img_ipi_frameparam) + 1024) & ~1024)
#define DIP_FRM_SZ (DIP_SUB_FRM_SZ+DIP_TUNING_SZ+DIP_COMP_SZ+DIP_FRAMEPARAM_SZ)


#define DIP_TUNING_OFFSET		DIP_SUB_FRM_SZ
#define DIP_COMP_OFFSET			(DIP_TUNING_OFFSET + DIP_TUNING_SZ)
#define DIP_FRAMEPARAM_OFFSET		(DIP_COMP_OFFSET + DIP_COMP_SZ)
#define DIP_SUB_FRM_DATA_NUM		24
#define DIP_SCP_WORKINGBUF_OFFSET	(5 * 1024 * 1024)
#define DIP_V4l2_META_BUF_OFFSET	(DIP_SCP_WORKINGBUF_OFFSET + \
					 DIP_SUB_FRM_DATA_NUM * DIP_FRM_SZ)

#ifdef MDP_COLOR
/*
 * MDP native color code
 * Plane count: 1, 2, 3
 * H-subsample: 0, 1, 2
 * V-subsample: 0, 1
 * Color group: 0-RGB, 1-YUV, 2-raw
 */
#define DIP_MDP_COLOR(PACKED, LOOSE, VIDEO, PLANE, HF, VF, BITS, GROUP, SWAP, \
	ID) \
	(((PACKED) << 27) | ((LOOSE) << 26) | ((VIDEO) << 23) |\
	((PLANE) << 21) | ((HF) << 19) | ((VF) << 18) | ((BITS) << 8) |\
	((GROUP) << 6) | ((SWAP) << 5) | ((ID) << 0))

#define DIP_MCOLOR_IS_BLOCK_MODE(c)	((0x00800000 & (c)) >> 23)
#define DIP_MCOLOR_GET_PLANE_COUNT(c)	((0x00600000 & (c)) >> 21)
#define DIP_MCOLOR_GET_H_SUBSAMPLE(c)	((0x00180000 & (c)) >> 19)
#define DIP_MCOLOR_GET_V_SUBSAMPLE(c)	((0x00040000 & (c)) >> 18)
#define DIP_MCOLOR_BITS_PER_PIXEL(c)	((0x0003ff00 & (c)) >>  8)
#define DIP_MCOLOR_GET_GROUP(c)		((0x000000c0 & (c)) >>  6)
#define DIP_MCOLOR_IS_RGB(c)		(DIP_MCOLOR_GET_GROUP(c) == 0)
#define DIP_MCOLOR_IS_YUV(c)		(DIP_MCOLOR_GET_GROUP(c) == 1)
#define DIP_MCOLOR_IS_UV_COPLANE(c)	((DIP_MCOLOR_GET_PLANE_COUNT(c) == \
					  2) && \
					 DIP_MCOLOR_IS_YUV(c))

enum DIP_MDP_COLOR {
	DIP_MCOLOR_UNKNOWN	= 0,

	DIP_MCOLOR_FULLG8_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 8, 2, 0, 21),
	DIP_MCOLOR_FULLG8_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 8, 2, 0, 21),
	DIP_MCOLOR_FULLG8_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 8, 2, 0, 21),
	DIP_MCOLOR_FULLG8_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 8, 2, 0, 21),
	DIP_MCOLOR_FULLG8      = DIP_MCOLOR_FULLG8_BGGR,

	DIP_MCOLOR_FULLG10_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 10, 2, 0, 21),
	DIP_MCOLOR_FULLG10_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 10, 2, 0, 21),
	DIP_MCOLOR_FULLG10_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 10, 2, 0, 21),
	DIP_MCOLOR_FULLG10_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 10, 2, 0, 21),
	DIP_MCOLOR_FULLG10	= DIP_MCOLOR_FULLG10_BGGR,

	DIP_MCOLOR_FULLG12_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 12, 2, 0, 21),
	DIP_MCOLOR_FULLG12_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 12, 2, 0, 21),
	DIP_MCOLOR_FULLG12_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 12, 2, 0, 21),
	DIP_MCOLOR_FULLG12_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 12, 2, 0, 21),
	DIP_MCOLOR_FULLG12	= DIP_MCOLOR_FULLG12_BGGR,

	DIP_MCOLOR_FULLG14_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 14, 2, 0, 21),
	DIP_MCOLOR_FULLG14_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 14, 2, 0, 21),
	DIP_MCOLOR_FULLG14_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 14, 2, 0, 21),
	DIP_MCOLOR_FULLG14_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 14, 2, 0, 21),
	DIP_MCOLOR_FULLG14	= DIP_MCOLOR_FULLG14_BGGR,

	DIP_MCOLOR_BAYER8_RGGB  = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 8, 2, 0, 20),
	DIP_MCOLOR_BAYER8_GRBG  = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 8, 2, 0, 20),
	DIP_MCOLOR_BAYER8_GBRG  = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 8, 2, 0, 20),
	DIP_MCOLOR_BAYER8_BGGR  = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 8, 2, 0, 20),
	DIP_MCOLOR_BAYER8	= DIP_MCOLOR_BAYER8_BGGR,

	DIP_MCOLOR_BAYER10_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 10, 2, 0, 20),
	DIP_MCOLOR_BAYER10_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 10, 2, 0, 20),
	DIP_MCOLOR_BAYER10_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 10, 2, 0, 20),
	DIP_MCOLOR_BAYER10_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 10, 2, 0, 20),
	DIP_MCOLOR_BAYER10	= DIP_MCOLOR_BAYER10_BGGR,

	DIP_MCOLOR_BAYER12_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 12, 2, 0, 20),
	DIP_MCOLOR_BAYER12_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 12, 2, 0, 20),
	DIP_MCOLOR_BAYER12_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 12, 2, 0, 20),
	DIP_MCOLOR_BAYER12_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 12, 2, 0, 20),
	DIP_MCOLOR_BAYER12	= DIP_MCOLOR_BAYER12_BGGR,

	DIP_MCOLOR_BAYER14_RGGB = DIP_MDP_COLOR(0, 0, 0, 1, 0, 0, 14, 2, 0, 20),
	DIP_MCOLOR_BAYER14_GRBG = DIP_MDP_COLOR(0, 0, 0, 1, 0, 1, 14, 2, 0, 20),
	DIP_MCOLOR_BAYER14_GBRG = DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 14, 2, 0, 20),
	DIP_MCOLOR_BAYER14_BGGR = DIP_MDP_COLOR(0, 0, 0, 1, 1, 1, 14, 2, 0, 20),
	DIP_MCOLOR_BAYER14	= DIP_MCOLOR_BAYER14_BGGR,

	DIP_MCOLOR_UYVY		= DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 16, 1, 0, 4),
	DIP_MCOLOR_VYUY		= DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 16, 1, 1, 4),
	DIP_MCOLOR_YUYV		= DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 16, 1, 0, 5),
	DIP_MCOLOR_YVYU		= DIP_MDP_COLOR(0, 0, 0, 1, 1, 0, 16, 1, 1, 5),

	DIP_MCOLOR_I420		= DIP_MDP_COLOR(0, 0, 0, 3, 1, 1,  8, 1, 0, 8),
	DIP_MCOLOR_YV12		= DIP_MDP_COLOR(0, 0, 0, 3, 1, 1,  8, 1, 1, 8),

	DIP_MCOLOR_NV12		= DIP_MDP_COLOR(0, 0, 0, 2, 1, 1,  8, 1, 0, 12),
};
#endif

#define FRAME_STATE_INIT		0
#define FRAME_STATE_HW_TIMEOUT		1

enum STREAM_TYPE_ENUM {
	STREAM_UNKNOWN,
	STREAM_BITBLT,
	STREAM_GPU_BITBLT,
	STREAM_DUAL_BITBLT,
	STREAM_2ND_BITBLT,
	STREAM_ISP_IC,
	STREAM_ISP_VR,
	STREAM_ISP_ZSD,
	STREAM_ISP_IP,
	STREAM_ISP_VSS,
	STREAM_ISP_ZSD_SLOW,
	STREAM_WPE,
	STREAM_WPE2,
};

struct mtk_imgsys_hw_working_buf {
	dma_addr_t scp_daddr;
	void *vaddr;
	dma_addr_t isp_daddr;
};

struct mtk_imgsys_hw_subframe {
	struct mtk_imgsys_hw_working_buf buffer;
	int size;
	struct mtk_imgsys_hw_working_buf config_data;
	struct mtk_imgsys_hw_working_buf tuning_buf;
	struct mtk_imgsys_hw_working_buf frameparam;
	struct list_head list_entry;
};

struct mtk_imgsys_hw_working_buf_list {
	struct list_head list;
	u32 cnt;
	spinlock_t lock; /* protect the list and cnt */
};

struct mtk_imgsys_init_array {
	unsigned int    ofset;
	unsigned int    val;
};
#endif /* _MTK_DIP_HW_H_ */

