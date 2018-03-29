/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *         Daniel Hsiao <daniel.hsiao@mediatek.com>
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

#ifndef _VENC_H264_IF_H_
#define _VENC_H264_IF_H_

#include "venc_drv_base.h"

/**
 * enum venc_h264_vpu_work_buf - h264 encoder buffer index
 */
enum venc_h264_vpu_work_buf {
	VENC_H264_VPU_WORK_BUF_RC_INFO,
	VENC_H264_VPU_WORK_BUF_RC_CODE,
	VENC_H264_VPU_WORK_BUF_REC_LUMA,
	VENC_H264_VPU_WORK_BUF_REC_CHROMA,
	VENC_H264_VPU_WORK_BUF_REF_LUMA,
	VENC_H264_VPU_WORK_BUF_REF_CHROMA,
	VENC_H264_VPU_WORK_BUF_MV_INFO_1,
	VENC_H264_VPU_WORK_BUF_MV_INFO_2,
	VENC_H264_VPU_WORK_BUF_SKIP_FRAME,
	VENC_H264_VPU_WORK_BUF_SRC_LUMA,
	VENC_H264_VPU_WORK_BUF_SRC_CHROMA,
	VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CB,
	VENC_H264_VPU_WORK_BUF_SRC_CHROMA_CR,
	VENC_H264_VPU_WORK_BUF_MAX,
};

/**
 * enum venc_h264_bs_mode - for bs_mode argument in h264_enc_vpu_encode
 */
enum venc_h264_bs_mode {
	H264_BS_MODE_SPS,
	H264_BS_MODE_PPS,
	H264_BS_MODE_FRAME,
};

/*
 * struct venc_h264_vpu_config - Structure for h264 encoder configuration
 * @input_fourcc: input fourcc
 * @bitrate: target bitrate (in bps)
 * @pic_w: picture width
 * @pic_h: picture height
 * @buf_w: buffer width
 * @buf_h: buffer height
 * @intra_period: intra frame period
 * @framerate: frame rate
 * @profile: as specified in standard
 * @level: as specified in standard
 * @wfd: WFD mode 1:on, 0:off
 */
struct venc_h264_vpu_config {
	u32 input_fourcc;
	u32 bitrate;
	u32 pic_w;
	u32 pic_h;
	u32 buf_w;
	u32 buf_h;
	u32 intra_period;
	u32 framerate;
	u32 profile;
	u32 level;
	u32 wfd;
};

/*
 * struct venc_h264_vpu_buf - Structure for buffer information
 * @align: buffer alignment (in bytes)
 * @iova: IO virtual address
 * @vpua: VPU side memory addr which is used by RC_CODE
 * @size: buffer size (in bytes)
 */
struct venc_h264_vpu_buf {
	u32 align;
	u32 iova;
	u32 vpua;
	u32 size;
};

/*
 * struct venc_h264_vpu_drv - Structure for VPU driver control and info share
 * This structure is allocated in VPU side and shared to AP side.
 * @config: h264 encoder configuration
 * @work_bufs: working buffer information in VPU side
 * The work_bufs here is for storing the 'size' info shared to AP side.
 * The similar item in struct venc_h264_inst is for memory allocation
 * in AP side. The AP driver will copy the 'size' from here to the one in
 * struct mtk_vcodec_mem, then invoke mtk_vcodec_mem_alloc to allocate
 * the buffer. After that, bypass the 'dma_addr' to the 'iova' field here for
 * register setting in VPU side.
 */
struct venc_h264_vpu_drv {
	struct venc_h264_vpu_config config;
	struct venc_h264_vpu_buf work_bufs[VENC_H264_VPU_WORK_BUF_MAX];
};

/*
 * struct venc_h264_vpu_inst - h264 encoder VPU driver instance
 * @wq_hd: wait queue used for vpu cmd trigger then wait vpu interrupt done
 * @signaled: flag used for checking vpu interrupt done
 * @failure: flag to show vpu cmd succeeds or not
 * @state: enum venc_ipi_msg_enc_state
 * @bs_size: bitstream size for skip frame case usage
 * @wait_int: flag to wait interrupt done (0: for skip frame case, 1: normal
 *	      case)
 * @id: VPU instance id
 * @drv: driver structure allocated by VPU side and shared to AP side for
 *	 control and info share
 */
struct venc_h264_vpu_inst {
	wait_queue_head_t wq_hd;
	int signaled;
	int failure;
	int state;
	int bs_size;
	int wait_int;
	unsigned int id;
	struct venc_h264_vpu_drv *drv;
};

/*
 * struct venc_h264_inst - h264 encoder AP driver instance
 * @hw_base: h264 encoder hardware register base
 * @work_bufs: working buffer
 * @pps_buf: buffer to store the pps bitstream
 * @work_buf_allocated: working buffer allocated flag
 * @frm_cnt: encoded frame count
 * @prepend_hdr: when the v4l2 layer send VENC_SET_PARAM_PREPEND_HEADER cmd
 *  through h264_enc_set_param interface, it will set this flag and prepend the
 *  sps/pps in h264_enc_encode function.
 * @is_key_frm: key frame flag
 * @vpu_inst: VPU instance to exchange information between AP and VPU
 * @ctx: context for v4l2 layer integration
 * @dev: device for v4l2 layer integration
 */
struct venc_h264_inst {
	void __iomem *hw_base;
	struct mtk_vcodec_mem work_bufs[VENC_H264_VPU_WORK_BUF_MAX];
	struct mtk_vcodec_mem pps_buf;
	bool work_buf_allocated;
	unsigned int frm_cnt;
	unsigned int prepend_hdr;
	unsigned int is_key_frm;
	struct venc_h264_vpu_inst vpu_inst;
	void *ctx;
	struct platform_device *dev;
};

struct venc_common_if *get_h264_enc_comm_if(void);

#endif
