/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Huiguo.Zhu <huiguo.zhu@mediatek.com>
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

#ifndef __MTK_NR_DEF_H__
#define __MTK_NR_DEF_H__

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>

#define MTK_NR_MODULE_NAME		"mtk-nr"
#define MTK_NR_SHUTDOWN_TIMEOUT	((100*HZ)/1000)

#define MTK_NR_PARAMS			(1 << 0)
#define MTK_NR_SRC_FMT			(1 << 1)
#define MTK_NR_DST_FMT			(1 << 2)
#define MTK_NR_CTX_M2M			(1 << 3)
#define MTK_NR_CTX_STOP_REQ		(1 << 6)
#define MTK_NR_CTX_ABORT		(1 << 7)

#define MTK_NR_CLK_CNT          7

enum mtk_nr_dev_flags {
	/* for global */
	ST_SUSPEND,

	/* for m2m node */
	ST_M2M_OPEN,
	ST_M2M_RUN,
	ST_M2M_PEND,
	ST_M2M_SUSPENDED,
	ST_M2M_SUSPENDING,
};


#define mtk_nr_m2m_active(dev)      test_bit(ST_M2M_RUN, &(dev)->state)
#define mtk_nr_m2m_pending(dev)     test_bit(ST_M2M_PEND, &(dev)->state)
#define mtk_nr_m2m_opened(dev)      test_bit(ST_M2M_OPEN, &(dev)->state)

/**
 * struct mtk_nr_fmt - the driver's internal color format data
 * @mbus_code: Media Bus pixel code, -1 if not applicable
 * @name: format description
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @yorder: Y/C order
 * @corder: Chrominance order control
 * @num_planes: number of physically non-contiguous data planes
 * @nr_comp: number of physically contiguous data planes
 * @depth: per plane driver's private 'number of bits per pixel'
 * @flags: flags indicating which operation mode format applies to
 */
struct mtk_nr_fmt {
	u32 pixelformat;
	u16 num_planes;
	u16 num_comp;
	u8 depth[VIDEO_MAX_PLANES];
	u32 flags;
};

/**
 * struct mtk_nr_addr - the nr physical address set
 * @y: luminance plane address
 * @cb: cbcr plane address
 */
struct mtk_nr_addr {
	dma_addr_t y;
	dma_addr_t c;
};

/**
 * struct mtk_nr_frame - source/target frame properties
 * @f_width: SRC : SRCIMG_WIDTH, DST : OUTPUTDMA_WHOLE_IMG_WIDTH
 * @f_height: SRC : SRCIMG_HEIGHT, DST : OUTPUTDMA_WHOLE_IMG_HEIGHT
 * @crop: cropped(source)/scaled(destination) size
 * @payload: image size in bytes (w x h x bpp)
 * @pitch: bytes per line of image in memory
 */
struct mtk_nr_frame {
	u32 f_width;
	u32 f_height;
	struct v4l2_rect crop;
	unsigned long payload[VIDEO_MAX_PLANES];
	unsigned int pitch[VIDEO_MAX_PLANES];
	const struct mtk_nr_fmt *fmt;
};

/*
 * struct mtk_nr_m2m_device - v4l2 memory-to-memory device data
 * @vfd: the video device node for v4l2 m2m mode
 * @m2m_dev: v4l2 memory-to-memory device data
 * @ctx: hardware context data
 * @refcnt: the reference counter
 */
struct mtk_nr_m2m_device {
	struct v4l2_m2m_dev *m2m_dev;
	int refcnt;
};

/*
 *  struct mtk_nr_pix_max - picture pixel size limits in various IP configurations
 *  @org_w: max  source pixel width
 *  @org_h: max  source pixel height
 *  @target_w: max  output pixel height
 *  @target_h: max  output pixel height
 */
struct mtk_nr_pix_max {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

/*
 *  struct mtk_nr_pix_min - picture pixel size limits in various IP configurations
 *
 *  @org_w: minimum source pixel width
 *  @org_h: minimum source pixel height
 *  @target_w: minimum output pixel height
 *  @target_h: minimum output pixel height
 */
struct mtk_nr_pix_min {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};

struct mtk_nr_pix_align {
	u16 org_w;
	u16 org_h;
	u16 target_w;
	u16 target_h;
};


/*
 * struct mtk_nr_variant - video quality variant information
 */
struct mtk_nr_variant {
	const struct mtk_nr_pix_max *pix_max;
	const struct mtk_nr_pix_min *pix_min;
	const struct mtk_nr_pix_align *pix_align;
};

/*
 * struct mtk_nr_dev - abstraction for image processor entity
 * @lock: the mutex protecting this data structure
 * @nrlock: the mutex protecting the communication with nr
 * @pdev: pointer to the image processor platform device
 * @variant: the IP variant information
 * @clks: clocks required for the video quality operation
 * @irq_queue: interrupt handler waitqueue
 * @m2m: memory-to-memory V4L2 device information
 * @alloc_ctx: videobuf2 memory allocator context
 * @vdev: video device for image processor instance
 * @larb: clocks required for the video quality operation
 * @workqueue: decode work queue
 * @nr_dev: nr platform device
 * @state: flags used to synchronize m2m and capture mode operation
 * @id: the video quality device index (0..MTK_NR_MAX_DEVS)
 */
struct mtk_nr_dev {
	struct mutex lock;
	struct mutex nrlock;
	struct platform_device *pdev;
	const struct mtk_nr_variant *variant;
	struct clk *clks[MTK_NR_CLK_CNT];
	wait_queue_head_t irq_queue;
	struct mtk_nr_m2m_device m2m;
	struct video_device vdev;
	struct v4l2_device v4l2_dev;
	struct device *larb[2];
	struct workqueue_struct *workqueue;
	struct platform_device *nr_dev;

	void  *nr_reg_base;
	struct regmap *bdpsys_reg_base;

	int irq;
	wait_queue_head_t wait_nr_irq_handle;
	atomic_t wait_nr_irq_flag;

	unsigned long state;
	u16 id;
};

/*
 * struct mtk_nr_buf - buffer information record
 * @vb: vb record point
 * @addr: addr record
 */
struct mtk_nr_buf {
	struct vb2_buffer *vb;
	struct mtk_nr_addr addr;
};

struct mtk_nr_level {
	unsigned int u4TotalLevel;
	unsigned int u4BnrLevel;
	unsigned int u4MnrLevel;
	unsigned int u4FnrLevel;
};

/*
 * mtk_nr_ctx - the device context data
 * @s_frame: source frame properties
 * @d_frame: destination frame properties
 * @nr_dev: the video quality device which this context applies to
 * @m2m_ctx: memory-to-memory device context
 * @fh: v4l2 file handle
 * @ctrl_handler: v4l2 controls handler
 * @ctrls: image processor control set
 * @qlock: vb2 queue lock
 * @slock: the mutex protecting this data structure
 * @work: worker for image processing
 * @s_buf: source buffer information record
 * @d_addr: dest frame buffer physical addresses
 * @s_idx_for_next: source buffer idx for next
 * @flags: additional flags for image conversion
 * @state: flags to keep track of user configuration
 * @ctrls_rdy: true if the control handler is initialized
 * @field_mode: deinterlace by specified field mode
 */
struct mtk_nr_ctx {
	struct mtk_nr_frame s_frame;
	struct mtk_nr_frame d_frame;
	struct mtk_nr_dev *nr_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler ctrl_handler;

	struct mutex qlock;
	struct mutex slock;
	struct work_struct work;
	struct mtk_nr_buf s_buf;
	struct mtk_nr_addr d_addr;

	struct mtk_nr_level nr_level;

	u32 flags;
	u32 state;
	bool ctrls_rdy;
	enum v4l2_field field_mode;
};

#endif				//__MTK_NR_DEF_H__
