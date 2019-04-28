/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_VCODEC_DEC_H_
#define _MTK_VCODEC_DEC_H_

#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#define VCODEC_CAPABILITY_4K_DISABLED	0x10
#define VCODEC_DEC_4K_CODED_WIDTH	4096U
#define VCODEC_DEC_4K_CODED_HEIGHT	2304U
#define MTK_VDEC_MAX_W	2048U
#define MTK_VDEC_MAX_H	1088U
#define MTK_VDEC_MIN_W	64U
#define MTK_VDEC_MIN_H	64U

#define MTK_VDEC_IRQ_STATUS_DEC_SUCCESS        0x10000

/**
 * struct vdec_fb  - decoder frame buffer
 * @fb_base	: planes memory info
 * @status      : frame buffer status (vdec_fb_status)
 * @num_planes		:frame buffer plane number
 * @index		:frame buffer index in vb2 queue
 */
struct vdec_fb {
	struct mtk_vcodec_mem fb_base[VIDEO_MAX_PLANES];
	unsigned int	status;
	unsigned int    num_planes;
	unsigned int    index;
};

/**
 * enum eos_types  - decoder different eos types
 * @NON_EOS     : no eos, normal frame
 * @EOS_WITH_DATA      : early eos , mean this frame need to decode
 * @EOS : byteused of the last frame is zero
 */
enum eos_types {
	NON_EOS = 0,
	EOS_WITH_DATA,
	EOS
};

/**
 * struct mtk_video_dec_buf - Private data related to each VB2 buffer.
 * @vb:	VB2 buffer
 * @list:	link list
 * @used:	Capture buffer contain decoded frame data and keep in
 *			codec data structure
 * @queued_in_vb2:	Capture buffer is queue in vb2
 * @queued_in_v4l2:	Capture buffer is in v4l2 driver, but not in vb2
 *			queue yet
 * @lastframe:		Intput buffer is last buffer - EOS
 * @error:		An unrecoverable error occurs on this buffer.
 * @frame_buffer:	Decode status, and buffer information of Capture buffer
 * @bs_buffer:	Output buffer info
 *
 * Note : These status information help us track and debug buffer state
 */
struct mtk_video_dec_buf {
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;

	bool	used;
	bool	queued_in_vb2;
	bool	queued_in_v4l2;
	enum eos_types	lastframe;

	bool	error;

	union {
		struct vdec_fb	frame_buffer;
		struct mtk_vcodec_mem	bs_buffer;
	};
	int flags;
};

extern const struct v4l2_ioctl_ops mtk_vdec_ioctl_ops;
extern const struct v4l2_m2m_ops mtk_vdec_m2m_ops;
extern const struct media_device_ops mtk_vcodec_media_ops;


/*
 * mtk_vdec_lock/mtk_vdec_unlock are for ctx instance to
 * get/release lock before/after access decoder hw.
 * mtk_vdec_lock get decoder hw lock and set curr_ctx
 * to ctx instance that get lock
 */
void mtk_vdec_unlock(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_lock(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq);
void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_dec_empty_queues(struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_dec_release(struct mtk_vcodec_ctx *ctx);
int mtk_vdec_set_param(struct mtk_vcodec_ctx *ctx);
struct mtk_video_fmt *mtk_find_fmt_by_pixel(unsigned int pixelformat);

int mtk_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl);

/*
 * VB2 ops
 */
int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[],
				struct device *alloc_devs[]);
int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb);
void vb2ops_vdec_buf_finish(struct vb2_buffer *vb);
int vb2ops_vdec_buf_init(struct vb2_buffer *vb);
int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count);
void vb2ops_vdec_stop_streaming(struct vb2_queue *q);


#endif /* _MTK_VCODEC_DEC_H_ */
