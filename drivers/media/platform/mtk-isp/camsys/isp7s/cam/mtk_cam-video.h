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

#include "mtk_cam-ipi_7_1.h"
#include "mtk_camera-videodev2.h"

struct mtk_cam_device;
struct mtk_cam_resource;
struct mtk_raw_pde_config;

struct mtk_cam_cached_image_info {
	unsigned int v4l2_pixelformat;
	unsigned int width;
	unsigned int height;
	unsigned int bytesperline[4];
	unsigned int size[4];
	struct v4l2_rect crop;
};

struct mtk_cam_cached_meta_info {
	unsigned int v4l2_pixelformat;
	unsigned int buffersize;
};

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

	union {
		struct mtk_cam_cached_image_info image_info;
		struct mtk_cam_cached_meta_info meta_info;
	};
};

struct mtk_cam_format_desc {
	struct v4l2_format vfmt;
	struct v4l2_mbus_framefmt pfmt;
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
 * @need_cache_sync_on_prepare: do cache sync at buf_prepare
 * @need_cache_sync_on_finish: do cache sync at buf_finish
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
	u8 need_cache_sync_on_prepare:1;
	u8 need_cache_sync_on_finish:1;
	u8 image:1;
	u8 num_fmts;
	u8 default_fmt_idx;
	u8 max_buf_count;
	const struct v4l2_ioctl_ops *ioctl_ops;
	const struct mtk_cam_format_desc *fmts;
	const struct v4l2_frmsizeenum *frmsizes;
	struct mtk_cam_pad_ops *pad_ops;
	u8 hsf_en;
};

/*
 * struct mtk_cam_video_device - Mediatek video device structure.
 *
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
	struct v4l2_selection active_crop;
	/* Serializes vb2 queue and video device operations */
	struct mutex q_lock;

	/* these are update in s_fmt/s_selection callbacks. */
	union {
		/* select info according to desc.image */
		struct mtk_cam_cached_image_info image_info;
		struct mtk_cam_cached_meta_info meta_info;
	};

	/* TODO: debug only */
	struct v4l2_format prev_fmt;
	struct v4l2_selection prev_crop;
};

#define media_entity_to_mtk_vdev(ent)	\
({					\
	typeof(ent) _ent = (ent);	\
	_ent ? container_of(_ent, struct mtk_cam_video_device, vdev.entity) : \
		NULL; \
})

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

static inline struct mtk_cam_video_device *
mtk_cam_buf_to_vdev(struct mtk_cam_buffer *buf)
{
	WARN_ON(!buf->vbb.vb2_buf.vb2_queue);
	return mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
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

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f);

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
				struct v4l2_selection *s);

int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f);

/* Utility functions to convert format enum */
int mtk_cam_get_fmt_size_factor(unsigned int ipi_fmt);

const struct mtk_format_info *mtk_format_info(u32 format);

void mtk_cam_mark_pipe_used(int *used_mask, int ipi_pipe_id);

#endif /*__MTK_CAM_VIDEO_H*/
