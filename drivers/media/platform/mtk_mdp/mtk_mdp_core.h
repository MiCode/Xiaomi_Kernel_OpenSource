/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __MTK_MDP_CORE_H__
#define __MTK_MDP_CORE_H__

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>
#include <linux/module.h>

#include "mtk_mdp_vpu.h"
#ifdef CONFIG_ARM
#include <asm/dma-iommu.h>
#endif
#include <linux/iommu.h>

#define MTK_MDP_MODULE_NAME		"mtk-mdp"

#define MTK_MDP_SHUTDOWN_TIMEOUT	((100*HZ)/1000)
#define MTK_MDP_MAX_DEVS		4
#define MTK_MDP_MAX_CTRL_NUM		10
#define MTK_MDP_SC_ALIGN_4		4
#define MTK_MDP_SC_ALIGN_2		2
#define DEFAULT_CSC_EQ			1
#define DEFAULT_CSC_RANGE		1

#define MTK_MDP_PARAMS			(1 << 0)
#define MTK_MDP_SRC_FMT			(1 << 1)
#define MTK_MDP_DST_FMT			(1 << 2)
#define MTK_MDP_CTX_M2M			(1 << 3)
#define MTK_MDP_CTX_STOP_REQ		(1 << 6)
#define MTK_MDP_CTX_ABORT		(1 << 7)


enum mtk_mdp_color_fmt {
	MTK_MDP_RGB = 0x1,
	MTK_MDP_YUV420 = 0x2,
	MTK_MDP_YUV422 = 0x3,
	MTK_MDP_YUV444 = 0x4,
	MTK_MDP_YUV420_BLK = 0x5,
};

enum mtk_mdp_yuv_fmt {
	MTK_MDP_LSB_Y = 0x10,
	MTK_MDP_LSB_C,
	MTK_MDP_CBCR = 0x20,
	MTK_MDP_CRCB,
};

#define is_rgb(x) (!!((x) & 0x1))
#define is_yuv420(x) (!!((x) & 0x2))
#define is_yuv422(x) (!!((x) & 0x4))

/**
 * struct mtk_mdp_fmt - the driver's internal color format data
 * @mbus_code: Media Bus pixel code, -1 if not applicable
 * @name: format description
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @yorder: Y/C order * @corder: Chrominance order control
 * @num_planes: number of physically non-contiguous data planes
 * @nr_comp: number of physically contiguous data planes
 * @depth: per plane driver's private 'number of bits per pixel'
 * @flags: flags indicating which operation mode format applies to
 */
struct mtk_mdp_fmt {
	char				*name;
	u32				pixelformat;
	u32				color;
	u32				yorder;
	u32				corder;
	u16				num_planes;
	u16				num_comp;
	u8				depth[VIDEO_MAX_PLANES];
	u32				flags;
};

/**
 * struct mtk_mdp_addr - the image processor physical address set
 * @y:	 luminance plane address * @cb:	 Cb plane address
 * @cr:	 Cr plane address
 */
struct mtk_mdp_addr {
	dma_addr_t y;
	dma_addr_t cb;
	dma_addr_t cr;
};

/* struct mdp_ctrls - the image processor control value
 *@value: control attribute value
 */
struct mdp_ctrl {
	u32 val;
};

struct mtk_mdp_ctrls {
	struct mdp_ctrl *rotate;
	struct mdp_ctrl *hflip;
	struct mdp_ctrl *vflip;
	struct mdp_ctrl *global_alpha;
};

struct mtk_mdp_rect {
	 u32 left;
	 u32 top;
	 u32 width;
	 u32 height;
};

/**
* struct mtk_mdp_frame - source/target frame properties
* @f_width:	SRC : SRCIMG_WIDTH, DST : OUTPUTDMA_WHOLE_IMG_WIDTH
* @f_height:	SRC : SRCIMG_HEIGHT, DST : OUTPUTDMA_WHOLE_IMG_HEIGHT
* @crop:	cropped(source)/scaled(destination) size
* @payload:	image size in bytes (w x h x bpp)
* @pitch:	bytes per line of image in memory
* @addr:	image frame buffer physical addresses
* @fmt:	color format pointer
* @colorspace: value indicating v4l2_colorspace
* @alpha:	frame's alpha value
*/
struct mtk_mdp_frame {
	u32				f_width;
	u32				f_height;
	struct mtk_mdp_rect		crop;
	unsigned long			payload[VIDEO_MAX_PLANES];
	unsigned int			pitch[VIDEO_MAX_PLANES];
	struct mtk_mdp_addr		addr;
	struct mtk_mdp_fmt		*fmt;
	u32				colorspace;
	u8				alpha;
};

struct mtk_mdp_pix_max {
	u16 org_scaler_bypass_w;
	u16 org_scaler_bypass_h;
	u16 org_scaler_input_w;
	u16 org_scaler_input_h;
	u16 real_rot_dis_w;
	u16 real_rot_dis_h;
	u16 real_rot_en_w;
	u16 real_rot_en_h;
	u16 target_rot_dis_w;
	u16 target_rot_dis_h;
	u16 target_rot_en_w;
	u16 target_rot_en_h;
};

/**
 *  struct mtk_mdp_pix_min - image pixel size limits in various IP configurations
 *
 *  @org_w: minimum source pixel width
 *  @org_h: minimum source pixel height
 *  @real_w: minimum input crop pixel width
 *  @real_h: minimum input crop pixel height
 *  @target_rot_dis_w: minimum output scaled pixel height when rotator is off
 *  @target_rot_dis_h: minimum output scaled pixel height when rotator is off
 *  @target_rot_en_w: minimum output scaled pixel height when rotator is on
 *  @target_rot_en_h: minimum output scaled pixel height when rotator is on
 */
struct mtk_mdp_pix_min {
	u16 org_w;
	u16 org_h;
	u16 real_w;
	u16 real_h;
	u16 target_rot_dis_w;
	u16 target_rot_dis_h;
	u16 target_rot_en_w;
	u16 target_rot_en_h;
};

struct mtk_mdp_pix_align {
	u16 org_h;
	u16 org_w;
	u16 offset_h;
	u16 real_w;
	u16 real_h;
	u16 target_w;
	u16 target_h;
};

/**
* struct mtk_mdp_variant - image processor variant information
*/
struct mtk_mdp_variant {
	const struct mtk_mdp_pix_max		*pix_max;
	const struct mtk_mdp_pix_min		*pix_min;
	const struct mtk_mdp_pix_align	*pix_align;
	u16				in_buf_cnt;
	u16				out_buf_cnt;
	u16				h_sc_up_max;
	u16				v_sc_up_max;
	u16				h_sc_down_max;
	u16				v_sc_down_max;
};

/**
* struct mtk_mdp_dev - abstraction for image processor entity
* @lock:	the mutex protecting this data structure
* @vpulock:	the mutex protecting the communication with VPU
* @pdev:	pointer to the image processor platform device
* @variant:	the IP variant information
* @id:		image processor device index (0..MTK_MDP_MAX_DEVS)
* @clks:	clocks required for image processor operation
* @irq_queue:	interrupt handler waitqueue
* @m2m:	memory-to-memory V4L2 device information
* @state:	flags used to synchronize m2m and capture mode operation
* @alloc_ctx:	videobuf2 memory allocator context
* @vdev:	video device for image processor instance
* @larb:	clocks required for image processor operation
* @workqueue:	decode work queue
* @vpu_dev:	VPU platform device
*/
struct mtk_mdp_dev {
	struct mutex			vpulock;
	struct platform_device		*pdev;
	const struct mtk_mdp_variant		*variant;
	u16				id;
	unsigned long			state;
	struct platform_device		*vpu_dev;
	dev_t mtk_mdp_devno;
	struct cdev *mtk_mdp_cdev;
	struct class *mtk_mdp_class;
};

/**
 * mtk_mdp_ctx - the device context data
 * @s_frame:		source frame properties
 * @d_frame:		destination frame properties
 * @flags:		additional flags for image conversion
 * @state:		flags to keep track of user configuration
 * @rotation:		rotates the image by specified angle
 * @hflip:		mirror the picture horizontally
 * @vflip:		mirror the picture vertically
 * @mdp_dev:		the image processor device this context applies to
 * @m2m_ctx:		memory-to-memory device context
 * @fh:			v4l2 file handle
 * @ctrl_handler:	v4l2 controls handler
 * @ctrls		image processor control set
 * @ctrls_rdy:		true if the control handler is initialized
 * @vpu:		VPU instance
 * @qlock:		vb2 queue lock
 * @slock:		the mutex protecting this data structure
 * @work:		worker for image processing
 */
struct mtk_mdp_ctx {
	struct mtk_mdp_frame		s_frame;
	struct mtk_mdp_frame		d_frame;
	u32				flags;
	u32				state;
	int				rotation;
	u32				hflip:1;
	u32				vflip:1;
	struct mtk_mdp_dev		*mdp_dev;
	struct mtk_mdp_ctrls		ctrls;
	bool				ctrls_rdy;
	unsigned long			drv_handle;

	struct mtk_mdp_vpu		vpu;
};




void mtk_mdp_hw_set_input_addr(struct mtk_mdp_ctx *ctx,
			       struct mtk_mdp_addr *addr);
void mtk_mdp_hw_set_output_addr(struct mtk_mdp_ctx *ctx,
				struct mtk_mdp_addr *addr);
void mtk_mdp_hw_set_in_size(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_in_image_format(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_out_size(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_out_image_format(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_rotation(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_global_alpha(struct mtk_mdp_ctx *ctx);
void mtk_mdp_hw_set_sfr_update(struct mtk_mdp_ctx *ctx);
#endif /* __MTK_MDP_CORE_H__ */
