/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_RSC_DEV_H__
#define __MTK_RSC_DEV_H__

#include <linux/platform_device.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include "mtk_rsc-ctx.h"

/* Added the macro for early stage verification */
/* based on kernel 4.4 environment. */
/* I will remove the version check after getting */
/* the devlopment platform based on 4.14 */
#define MTK_RSC_KERNEL_BASE_VERSION KERNEL_VERSION(4, 14, 0)

#define MTK_RSC_DEV_NODE_MAX			(MTK_RSC_CTX_QUEUES)

#define MTK_RSC_INPUT_MIN_WIDTH			0U
#define MTK_RSC_INPUT_MIN_HEIGHT		0U
#define MTK_RSC_INPUT_MAX_WIDTH			480U
#define MTK_RSC_INPUT_MAX_HEIGHT		640U
#define MTK_RSC_OUTPUT_MIN_WIDTH		2U
#define MTK_RSC_OUTPUT_MIN_HEIGHT		2U
#define MTK_RSC_OUTPUT_MAX_WIDTH		288U
#define MTK_RSC_OUTPUT_MAX_HEIGHT		512U

#define file_to_mtk_rsc_node(__file) \
	container_of(video_devdata(__file),\
	struct mtk_rsc_dev_video_device, vdev)

#define mtk_rsc_ctx_to_dev(__ctx) \
	container_of(__ctx,\
	struct mtk_rsc_dev, ctx)

#define mtk_rsc_m2m_to_dev(__m2m) \
	container_of(__m2m,\
	struct mtk_rsc_dev, mem2mem2)

#define mtk_rsc_subdev_to_dev(__sd) \
	container_of(__sd, \
	struct mtk_rsc_dev, mem2mem2.subdev)

#define mtk_rsc_vbq_to_isp_node(__vq) \
	container_of(__vq, \
	struct mtk_rsc_dev_video_device, vbq)

#define mtk_rsc_ctx_buf_to_dev_buf(__ctx_buf) \
	container_of(__ctx_buf, \
	struct mtk_rsc_dev_buffer, ctx_buf)

#define mtk_rsc_vb2_buf_to_dev_buf(__vb) \
	container_of(vb, \
	struct mtk_rsc_dev_buffer, \
	m2m2_buf.vbb.vb2_buf)

#define mtk_rsc_vb2_buf_to_m2m_buf(__vb) \
	container_of(__vb, \
	struct mtk_rsc_mem2mem2_buffer, \
	vbb.vb2_buf)

#define mtk_rsc_subdev_to_m2m(__sd) \
	container_of(__sd, \
	struct mtk_rsc_mem2mem2_device, subdev)

struct mtk_rsc_mem2mem2_device;

struct mtk_rsc_mem2mem2_buffer {
	struct vb2_v4l2_buffer vbb;
	struct list_head list;
};

struct mtk_rsc_dev_buffer {
	struct mtk_rsc_mem2mem2_buffer m2m2_buf;
	/* Intenal part */
	struct mtk_rsc_ctx_buffer ctx_buf;
};

struct mtk_rsc_dev_video_device {
	const char *name;
	int output;
	int immutable;
	int enabled;
	int queued;
	struct v4l2_format vdev_fmt;
	struct video_device vdev;
	struct media_pad vdev_pad;
	struct v4l2_mbus_framefmt pad_fmt;
	struct vb2_queue vbq;
	struct list_head buffers;
	struct mutex lock; /* Protect node data */
	atomic_t sequence;
};

struct mtk_rsc_mem2mem2_device {
	const char *name;
	const char *model;
	struct device *dev;
	int num_nodes;
	struct mtk_rsc_dev_video_device *nodes;
	const struct vb2_mem_ops *vb2_mem_ops;
	unsigned int buf_struct_size;
	int streaming;
	struct v4l2_device *v4l2_dev;
	struct media_device *media_dev;
	struct media_pipeline pipeline;
	struct v4l2_subdev subdev;
	struct media_pad *subdev_pads;
	struct v4l2_file_operations v4l2_file_ops;
	const struct file_operations fops;
};

struct mtk_rsc_dev {
	struct platform_device *pdev;
	struct mtk_rsc_dev_video_device mem2mem2_nodes[MTK_RSC_DEV_NODE_MAX];
	int queue_enabled[MTK_RSC_DEV_NODE_MAX];
	struct mtk_rsc_mem2mem2_device mem2mem2;
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct mtk_rsc_ctx ctx;
	struct mutex lock; /* queue protection */
	atomic_t qbuf_barrier;
	struct {
		struct v4l2_rect eff;
		struct v4l2_rect bds;
		struct v4l2_rect gdc;
	} rect;
	int suspend_in_stream;
	wait_queue_head_t buf_drain_wq;
};

int mtk_rsc_media_register(struct device *dev,
			  struct media_device *media_dev,
			  const char *model);

int mtk_rsc_v4l2_register(struct device *dev,
			 struct media_device *media_dev,
			 struct v4l2_device *v4l2_dev);

int mtk_rsc_v4l2_unregister(struct mtk_rsc_dev *dev);

int mtk_rsc_mem2mem2_v4l2_register(struct mtk_rsc_dev *dev,
				  struct media_device *media_dev,
				  struct v4l2_device *v4l2_dev);

void mtk_rsc_v4l2_buffer_done(struct vb2_buffer *vb,
			     enum vb2_buffer_state state);

int mtk_rsc_dev_queue_buffers(struct mtk_rsc_dev *dev, bool initial);

int mtk_rsc_dev_get_total_node(struct mtk_rsc_dev *mtk_rsc_dev);

char *mtk_rsc_dev_get_node_name(struct mtk_rsc_dev *mtk_rsc_dev_obj, int node);

int mtk_rsc_dev_init(struct mtk_rsc_dev *rsc_dev,
		    struct platform_device *pdev,
		    struct media_device *media_dev,
		    struct v4l2_device *v4l2_dev);

void mtk_rsc_dev_mem2mem2_exit(struct mtk_rsc_dev *mtk_rsc_dev_obj);

int mtk_rsc_dev_mem2mem2_init(struct mtk_rsc_dev *rsc_dev,
			     struct media_device *media_dev,
			     struct v4l2_device *v4l2_dev);

int mtk_rsc_dev_get_queue_id_of_dev_node(struct mtk_rsc_dev *mtk_rsc_dev_obj,
					struct mtk_rsc_dev_video_device *node);

int mtk_rsc_dev_core_init(struct platform_device *pdev,
			 struct mtk_rsc_dev *rsc_dev,
			 struct mtk_rsc_ctx_desc *ctx_desc);

int mtk_rsc_dev_core_init_ext(struct platform_device *pdev,
			     struct mtk_rsc_dev *rsc_dev,
			     struct mtk_rsc_ctx_desc *ctx_desc,
			     struct media_device *media_dev,
			     struct v4l2_device *v4l2_dev);

int mtk_rsc_dev_core_release(struct platform_device *pdev,
			    struct mtk_rsc_dev *rsc_dev);

#endif /* __MTK_RSC_DEV_H__ */
