/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
 *         PoChun Lin <pochun.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VENC_VP8_IF_H_
#define _VENC_VP8_IF_H_

#include "venc_drv_base.h"

/**
 * enum venc_vp8_vpu_work_buf - vp8 encoder buffer index
 */
enum venc_vp8_vpu_work_buf {
	VENC_VP8_VPU_WORK_BUF_LUMA,
	VENC_VP8_VPU_WORK_BUF_LUMA2,
	VENC_VP8_VPU_WORK_BUF_LUMA3,
	VENC_VP8_VPU_WORK_BUF_CHROMA,
	VENC_VP8_VPU_WORK_BUF_CHROMA2,
	VENC_VP8_VPU_WORK_BUF_CHROMA3,
	VENC_VP8_VPU_WORK_BUF_MV_INFO,
	VENC_VP8_VPU_WORK_BUF_BS_HD,
	VENC_VP8_VPU_WORK_BUF_PROB_BUF,
	VENC_VP8_VPU_WORK_BUF_RC_INFO,
	VENC_VP8_VPU_WORK_BUF_RC_CODE,
	VENC_VP8_VPU_WORK_BUF_RC_CODE2,
	VENC_VP8_VPU_WORK_BUF_RC_CODE3,
	VENC_VP8_VPU_WORK_BUF_SRC_LUMA,
	VENC_VP8_VPU_WORK_BUF_SRC_CHROMA,
	VENC_VP8_VPU_WORK_BUF_SRC_CHROMA_CB,
	VENC_VP8_VPU_WORK_BUF_SRC_CHROMA_CR,
	VENC_VP8_VPU_WORK_BUF_MAX,
};

/*
 * struct venc_vp8_vpu_config - Structure for vp8 encoder configuration
 * @input_fourcc: input fourcc
 * @bitrate: target bitrate (in bps)
 * @pic_w: picture width
 * @pic_h: picture height
 * @buf_w: buffer width (with 16 alignment)
 * @buf_h: buffer height (with 16 alignment)
 * @intra_period: intra frame period
 * @framerate: frame rate
 * @ts_mode: temporal scalability mode (0: disable, 1: enable)
 *	     support three temporal layers - 0: 7.5fps 1: 7.5fps 2: 15fps.
 */
struct venc_vp8_vpu_config {
	u32 input_fourcc;
	u32 bitrate;
	u32 pic_w;
	u32 pic_h;
	u32 buf_w;
	u32 buf_h;
	u32 intra_period;
	u32 framerate;
	u32 ts_mode;
};

/*
 * struct venc_vp8_vpu_buf -Structure for buffer information
 * @align: buffer alignment (in bytes)
 * @iova: IO virtual address
 * @vpua: VPU side memory addr which is used by RC_CODE
 * @size: buffer size (in bytes)
 */
struct venc_vp8_vpu_buf {
	u32 align;
	u32 iova;
	u32 vpua;
	u32 size;
};

/*
 * struct venc_vp8_vpu_drv - Structure for VPU driver control and info share
 * This structure is allocated in VPU side and shared to AP side.
 * @config: vp8 encoder configuration
 * @work_bufs: working buffer information in VPU side
 * The work_bufs here is for storing the 'size' info shared to AP side.
 * The similar item in struct venc_vp8_inst is for memory allocation
 * in AP side. The AP driver will copy the 'size' from here to the one in
 * struct mtk_vcodec_mem, then invoke mtk_vcodec_mem_alloc to allocate
 * the buffer. After that, bypass the 'dma_addr' to the 'iova' field here for
 * register setting in VPU side.
 */
struct venc_vp8_vpu_drv {
	struct venc_vp8_vpu_config config;
	struct venc_vp8_vpu_buf work_bufs[VENC_VP8_VPU_WORK_BUF_MAX];
};

/*
 * struct venc_vp8_vpu_inst - vp8 encoder VPU driver instance
 * @wq_hd: wait queue used for vpu cmd trigger then wait vpu interrupt done
 * @signaled: flag used for checking vpu interrupt done
 * @failure: flag to show vpu cmd succeeds or not
 * @id: VPU instance id
 * @drv: driver structure allocated by VPU side and shared to AP side for
 *	 control and info share
 */
struct venc_vp8_vpu_inst {
	wait_queue_head_t wq_hd;
	int signaled;
	int failure;
	unsigned int id;
	struct venc_vp8_vpu_drv *drv;
};

/*
 * struct venc_vp8_inst - vp8 encoder AP driver instance
 * @hw_base: vp8 encoder hardware register base
 * @work_bufs: working buffer
 * @work_buf_allocated: working buffer allocated flag
 * @frm_cnt: encoded frame count, it's used for I-frame judgement and
 *	     reset when force intra cmd received.
 * @ts_mode: temporal scalability mode (0: disable, 1: enable)
 *	     support three temporal layers - 0: 7.5fps 1: 7.5fps 2: 15fps.
 * @vpu_inst: VPU instance to exchange information between AP and VPU
 * @ctx: context for v4l2 layer integration
 * @dev: device for v4l2 layer integration
 */
struct venc_vp8_inst {
	void __iomem *hw_base;
	struct mtk_vcodec_mem work_bufs[VENC_VP8_VPU_WORK_BUF_MAX];
	bool work_buf_allocated;
	unsigned int frm_cnt;
	unsigned int ts_mode;
	struct venc_vp8_vpu_inst vpu_inst;
	void *ctx;
	struct platform_device *dev;
};

struct venc_common_if *get_vp8_enc_comm_if(void);

#endif
