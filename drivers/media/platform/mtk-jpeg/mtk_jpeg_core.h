/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
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


#ifndef _MTK_JPEG_CORE_H
#define _MTK_JPEG_CORE_H

#include <linux/interrupt.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#define MTK_JPEG_NAME		"mtk-jpeg"

#define MTK_JPEG_FMT_FLAG_DEC_OUTPUT	(1 << 0)
#define MTK_JPEG_FMT_FLAG_DEC_CAPTURE	(1 << 1)

#define MTK_JPEG_FMT_TYPE_OUTPUT	1
#define MTK_JPEG_FMT_TYPE_CAPTURE	2

#define MTK_JPEG_MIN_WIDTH	32
#define MTK_JPEG_MIN_HEIGHT	32
#define MTK_JPEG_MAX_WIDTH	8192
#define MTK_JPEG_MAX_HEIGHT	8192

#define MTK_JPEG_BENCHMARK	1

enum mtk_jpeg_ctx_state {
	MTK_JPEG_INIT = 0,
	MTK_JPEG_RUNNING,
	MTK_JPEG_SOURCE_CHANGE,
};

/**
 * struct mt_jpeg - JPEG IP abstraction
 * @lock:		the mutex protecting this structure
 * @dev_lock:		the mutex protecting JPEG IP
 * @irq_lock:		spinlock protecting the device contexts
 * @workqueue:		decode work queue
 * @dev:		JPEG device
 * @v4l2_dev:		v4l2 device for mem2mem mode
 * @m2m_dev:		v4l2 mem2mem device data
 * @alloc_ctx:		videobuf2 memory allocator's context
 * @dec_vdev:		video device node for decoder mem2mem mode
 * @dec_reg_base:	JPEG registers mapping
 * @clk_venc_jdec:	JPEG clock
 * @larb:		SMI device
 */
struct mtk_jpeg_dev {
	struct mutex		lock;
	struct mutex		dev_lock;
	spinlock_t		irq_lock;
	struct workqueue_struct	*workqueue;
	struct device		*dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	void			*alloc_ctx;
	struct video_device	*dec_vdev;
	void __iomem		*dec_reg_base;
	struct clk		*clk_venc_jdec;
	struct clk		*clk_venc_jdec_smi;
	struct device		*larb;
};

/**
 * struct jpeg_fmt - driver's internal color format data
 * @name:	format descritpion
 * @fourcc:	the fourcc code, 0 if not applicable
 * @depth:	number of bits per pixel
 * @colplanes:	number of color planes (1 for packed formats)
 * @h_align:	horizontal alignment order (align to 2^h_align)
 * @v_align:	vertical alignment order (align to 2^v_align)
 * @flags:	flags describing format applicability
 */
struct mtk_jpeg_fmt {
	char	*name;
	u32	fourcc;
	int	depth;
	int	colplanes;
	int	h_align;
	int	v_align;
	u32	flags;
};

/**
 * mtk_jpeg_q_data - parameters of one queue
 * @fmt:	driver-specific format of this queue
 * @w:		image width
 * @h:		image height
 * @size:	image buffer size in bytes
 */
struct mtk_jpeg_q_data {
	struct mtk_jpeg_fmt	*fmt;
	u32			w;
	u32			h;
	u32			size;
};

/**
 * mtk_jpeg_ctx - the device context data
 * @jpeg:		JPEG IP device for this context
 * @work:		decode work
 * @completion:		decode completion
 * @out_q:		source (output) queue information
 * @cap_q:		destination (capture) queue queue information
 * @fh:			V4L2 file handle
 * @dec_param		parameters for HW decoding
 * @dec_irq_ret		interrupt status
 * @state:		state of the context
 * @header_valid:	set if header has been parsed and valid
 */
struct mtk_jpeg_ctx {
	struct mtk_jpeg_dev		*jpeg;
	struct work_struct		work;
	struct completion		completion;
	struct mtk_jpeg_q_data		out_q;
	struct mtk_jpeg_q_data		cap_q;
	struct v4l2_fh			fh;
	u32				dec_irq_ret;
	enum mtk_jpeg_ctx_state		state;
#if MTK_JPEG_BENCHMARK
	struct timeval			jpeg_enc_dec_start;
	uint32_t			total_enc_dec_cnt;
	uint32_t			total_enc_dec_time;
	uint32_t			total_parse_cnt;
	uint32_t			total_parse_time;
#endif
};

#endif /* _MTK_JPEG_CORE_H */
