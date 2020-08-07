/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_VIDEO_H
#define __MTK_CAM_VIDEO_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_cam-ipi.h"

#define MAX_PLANE_NUM 3

struct mtk_cam_device;

/*
 * struct mtk_cam_buffer - MTK camera device buffer.
 *
 * @vb: Embedded struct vb2_v4l2_buffer.
 * @queue: List entry of the object
 * @daddr: The DMA address of this buffer.
 * @scp_addr: The SCP address of this buffer which
 *            is only supported for meta input node.
 *
 */
struct mtk_cam_buffer {
	struct vb2_v4l2_buffer vbb;
	struct list_head list;

	dma_addr_t daddr;
	dma_addr_t scp_addr;
};

/*
 * struct mtk_cam_dev_node_desc - MTK camera device node descriptor
 *
 * @id: id of the node
 * @name: name of the node
 * @cap: supported V4L2 capabilities
 * @buf_type: supported V4L2 buffer type
 * @dma_port: the dma ports associated to the node
 * @link_flags: default media link flags
 * @smem_alloc: using the smem_dev as alloc device or not
 * @image: true for image node, false for meta node
 * @num_fmts: the number of supported node formats
 * @default_fmt_idx: default format of this node
 * @max_buf_count: maximum VB2 buffer count
 * @ioctl_ops:  mapped to v4l2_ioctl_ops
 * @fmts: supported format
 * @frmsizes: supported V4L2 frame size number
 *
 */
struct mtk_cam_dev_node_desc {
	u8 id;
	const char *name;
	u32 cap;
	u32 buf_type;
	u32 dma_port;
	u32 link_flags;
	u8 smem_alloc:1;
	u8 image:1;
	u8 num_fmts;
	u8 default_fmt_idx;
	u8 max_buf_count;
	const struct v4l2_ioctl_ops *ioctl_ops;
	const struct v4l2_format *fmts;
	const struct v4l2_frmsizeenum *frmsizes;
};

/*
 * struct mtk_cam_video_device - Mediatek video device structure.
 *
 * FIXME
 *
 */
struct mtk_cam_video_device {
	struct mtkcam_ipi_uid uid;
	struct mtk_cam_dev_node_desc desc;
	unsigned int enabled;

	struct vb2_queue vb2_q;
	struct video_device vdev;
	struct media_pad pad;
	struct v4l2_format active_fmt;
	struct v4l2_format pending_fmt;
	/* Serializes vb2 queue and video device operations */
	struct mutex q_lock;

	/* cached ctx info */
	struct mtk_cam_ctx *ctx;

	/* Lock used to protect  buffer list */
	spinlock_t buf_list_lock;
	struct list_head buf_list;
};

int mtk_cam_video_register(struct mtk_cam_video_device *video,
			   struct v4l2_device *v4l2_dev);
void mtk_cam_video_unregister(struct mtk_cam_video_device *video);

static inline struct mtk_cam_video_device *
file_to_mtk_cam_node(struct file *__file)
{
	return container_of(video_devdata(__file),
		struct mtk_cam_video_device, vdev);
}

static inline struct mtk_cam_buffer *
mtk_cam_vb2_buf_to_dev_buf(struct vb2_buffer *__vb)
{
	return container_of(__vb, struct mtk_cam_buffer, vbb.vb2_buf);
}

static inline struct mtk_cam_video_device *
mtk_cam_vbq_to_vdev(struct vb2_queue *__vq)
{
	return container_of(__vq, struct mtk_cam_video_device, vb2_q);
}

const struct v4l2_format *
mtk_cam_dev_find_fmt(struct mtk_cam_dev_node_desc *desc, u32 format);

int mtk_cam_vidioc_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap);

int mtk_cam_vidioc_enum_framesizes(struct file *filp, void *priv,
				   struct v4l2_frmsizeenum *sizes);

int mtk_cam_vidioc_enum_fmt(struct file *file, void *fh,
			    struct v4l2_fmtdesc *f);

int mtk_cam_vidioc_g_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

int mtk_cam_vidioc_s_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f);

int video_try_fmt(struct mtk_cam_video_device *node, struct v4l2_format *f);

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f);

int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f);

/* Utility functions to convert format enum */
unsigned int mtk_cam_get_sensor_pixel_id(unsigned int fmt);

unsigned int mtk_cam_get_sensor_fmt(unsigned int fmt);

unsigned int mtk_cam_get_pixel_bits(unsigned int pix_fmt);

unsigned int mtk_cam_get_img_fmt(unsigned int fourcc);

#endif /*__MTK_CAM_VIDEO_H*/
