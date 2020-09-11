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

#ifndef __MTK_RSC_CTX_H__
#define __MTK_RSC_CTX_H__

#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-subdev.h>

#define MTK_RSC_CTX_QUEUES (16)
#define MTK_RSC_CTX_FRAME_BUNDLE_BUFFER_MAX (MTK_RSC_CTX_QUEUES)
#define MTK_RSC_CTX_DESC_MAX (MTK_RSC_CTX_QUEUES)

#define MTK_RSC_CTX_MODE_DEBUG_OFF (0)
#define MTK_RSC_CTX_MODE_DEBUG_BYPASS_JOB_TRIGGER (1)
#define MTK_RSC_CTX_MODE_DEBUG_BYPASS_ALL (2)

#define MTK_RSC_GET_CTX_ID_FROM_SEQUENCE(sequence) \
	((sequence) >> 16 & 0x0000FFFF)

#define MTK_RSC_CTX_META_BUF_DEFAULT_SIZE (1110 * 1024)

struct mtk_rsc_ctx;
struct mtk_rsc_ctx_finish_param;

/**
 * Attributes setup by device context owner
 */
struct mtk_rsc_ctx_queue_desc {
	int id;
	/* id of the context queue */
	char *name;
	/* Will be exported to media entity name */
	int capture;
	/**
	 * 1 for capture queue (device to user),
	 * 0 for output queue (from user to device)
	 */
	int image;
	/* 1 for image, 0 for meta data */
	unsigned int dma_port;
	/*The dma port associated to the buffer*/
	struct mtk_rsc_ctx_format *fmts;
	int num_fmts;
	/* Default format of this queue */
	int default_fmt_idx;
};

/**
 * Supported format and the information used for
 * size calculation
 */
struct mtk_rsc_ctx_meta_format {
	u32 dataformat;
	u32 max_buffer_size;
	u8 flags;
};

/**
 * MDP module's private format definitation
 * (the same as struct mdp_format)
 * It will be removed and changed to MDP's external interface
 * after the integration with MDP module.
 */
struct mtk_rsc_ctx_mdp_format {
	u32	pixelformat;
	u32	mdp_color;
	u8	depth[VIDEO_MAX_PLANES];
	u8	row_depth[VIDEO_MAX_PLANES];
	u8	num_planes;
	u8	walign;
	u8	halign;
	u8	salign;
	u32	flags;
};

struct mtk_rsc_ctx_format {
	union {
		struct mtk_rsc_ctx_meta_format meta;
		struct mtk_rsc_ctx_mdp_format img;
	} fmt;
};

union mtk_v4l2_fmt {
	struct v4l2_pix_format_mplane pix_mp;
	struct v4l2_meta_format	meta;
};

/* Attributes setup by device context owner */
struct mtk_rsc_ctx_queues_setting {
	int master;
	/* The master input node to trigger the frame data enqueue */
	struct mtk_rsc_ctx_queue_desc *output_queue_descs;
	int total_output_queues;
	struct mtk_rsc_ctx_queue_desc *capture_queue_descs;
	int total_capture_queues;
};

struct mtk_rsc_ctx_queue_attr {
	int master;
	int input_offset;
	int total_num;
};

/**
 * Video node context. Since we use
 * mtk_rsc_ctx_frame_bundle to manage enqueued
 * buffers by frame now, we don't use bufs filed of
 * mtk_rsc_ctx_queue now
 */
struct mtk_rsc_ctx_queue {
	union mtk_v4l2_fmt fmt;
	struct mtk_rsc_ctx_format *ctx_fmt;

	unsigned int width_pad;
	/* bytesperline, reserved */
	struct mtk_rsc_ctx_queue_desc desc;
	unsigned int buffer_usage;
	/* Current buffer usage of the queue */
	int rotation;
	struct list_head bufs;
	/* Reserved, not used now */
};

enum mtk_rsc_ctx_frame_bundle_state {
	MTK_RSC_CTX_FRAME_NEW,
	/* Not allocated */
	MTK_RSC_CTX_FRAME_PREPARED,
	/* Allocated but has not be processed */
	MTK_RSC_CTX_FRAME_PROCESSING,
	/* Queued, waiting to be filled */
};

/**
 * The definiation is compatible with RSC driver's state definiation
 * currently and will be decoupled after further integration
 */
enum mtk_rsc_ctx_frame_data_state {
	MTK_RSC_CTX_FRAME_DATA_EMPTY = 0, /* FRAME_STATE_INIT */
	MTK_RSC_CTX_FRAME_DATA_DONE = 3, /* FRAME_STATE_DONE */
	MTK_RSC_CTX_FRAME_DATA_STREAMOFF_DONE = 4, /*FRAME_STATE_STREAMOFF*/
	MTK_RSC_CTX_FRAME_DATA_ERROR = 5, /*FRAME_STATE_ERROR*/
};

struct mtk_rsc_ctx_frame_bundle {
	struct mtk_rsc_ctx_buffer*
		buffers[MTK_RSC_CTX_FRAME_BUNDLE_BUFFER_MAX];
	int id;
	int num_img_capture_bufs;
	int num_img_output_bufs;
	int num_meta_capture_bufs;
	int num_meta_output_bufs;
	int last_index;
	int state;
	struct list_head list;
};

struct mtk_rsc_ctx_frame_bundle_list {
	struct list_head list;
};

struct mtk_rsc_ctx {
	struct platform_device *pdev;
	struct platform_device *smem_device;
	unsigned short ctx_id;
	char device_name[12];
	const struct mtk_rsc_ctx_ops *ops;
	struct mtk_rsc_dev_node_mapping *mtk_rsc_dev_node_map;
	unsigned int dev_node_num;
	struct mtk_rsc_ctx_queue queue[MTK_RSC_CTX_QUEUES];
	struct mtk_rsc_ctx_queue_attr queues_attr;
	atomic_t frame_param_sequence;
	int streaming;
	void *img_vb2_alloc_ctx;
	void *smem_vb2_alloc_ctx;
	struct v4l2_subdev_fh *fh;
	struct mtk_rsc_ctx_frame_bundle frame_bundles[VB2_MAX_FRAME];
	struct mtk_rsc_ctx_frame_bundle_list processing_frames;
	struct mtk_rsc_ctx_frame_bundle_list free_frames;
	int enabled_dma_ports;
	int num_frame_bundle;
	int mode; /* Reserved for debug */
	spinlock_t qlock;
};

enum mtk_rsc_ctx_buffer_state {
	MTK_RSC_CTX_BUFFER_NEW,
	MTK_RSC_CTX_BUFFER_PROCESSING,
	MTK_RSC_CTX_BUFFER_DONE,
	MTK_RSC_CTX_BUFFER_FAILED,
};

struct mtk_rsc_ctx_buffer {
	union mtk_v4l2_fmt fmt;
	struct mtk_rsc_ctx_format *ctx_fmt;
	int capture;
	int image;
	int frame_id;
	int user_sequence; /* Sequence number assigned by user */
	dma_addr_t daddr;
	void *vaddr;
	phys_addr_t paddr;
	unsigned int queue;
	unsigned int buffer_usage;
	enum mtk_rsc_ctx_buffer_state state;
	int rotation;
	struct list_head list;
};

struct mtk_rsc_ctx_desc {
	char *proc_dev_phandle;
	/* The context device's compatble string name in device tree*/
	int (*init)(struct mtk_rsc_ctx *ctx);
	/* configure the core functions of the device context */
};

struct mtk_rsc_ctx_init_table {
	int total_dev_ctx;
	struct mtk_rsc_ctx_desc *ctx_desc_tbl;
};

struct mtk_rsc_ctx_finish_param {
	unsigned int frame_id;
	u64 timestamp;
	unsigned int state;
};

bool mtk_rsc_ctx_is_streaming(struct mtk_rsc_ctx *ctx);

int mtk_rsc_ctx_core_job_finish(struct mtk_rsc_ctx *ctx,
			       struct mtk_rsc_ctx_finish_param *param);

int mtk_rsc_ctx_core_init(struct mtk_rsc_ctx *ctx,
			 struct platform_device *pdev, int ctx_id,
			 struct mtk_rsc_ctx_desc *ctx_desc,
			 struct platform_device *proc_pdev,
			 struct platform_device *smem_pdev);

int mtk_rsc_ctx_core_exit(struct mtk_rsc_ctx *ctx);

void mtk_rsc_ctx_buf_init(struct mtk_rsc_ctx_buffer *b, unsigned int queue,
			 dma_addr_t daddr);

enum mtk_rsc_ctx_buffer_state
	mtk_rsc_ctx_get_buffer_state(struct mtk_rsc_ctx_buffer *b);

int mtk_rsc_ctx_next_global_frame_sequence(struct mtk_rsc_ctx *ctx,
	int locked);

int mtk_rsc_ctx_core_queue_setup
	(struct mtk_rsc_ctx *ctx,
	 struct mtk_rsc_ctx_queues_setting *queues_setting);

int mtk_rsc_ctx_core_finish_param_init(void *param, int frame_id, int state);

int mtk_rsc_ctx_finish_frame(struct mtk_rsc_ctx *dev_ctx,
			    struct mtk_rsc_ctx_frame_bundle *frame_bundle,
			    int done);

int mtk_rsc_ctx_frame_bundle_init(
	struct mtk_rsc_ctx_frame_bundle *frame_bundle);

void mtk_rsc_ctx_frame_bundle_add(struct mtk_rsc_ctx *ctx,
				 struct mtk_rsc_ctx_frame_bundle *bundle,
				 struct mtk_rsc_ctx_buffer *ctx_buf);

int mtk_rsc_ctx_trigger_job(struct mtk_rsc_ctx *dev_ctx,
			   struct mtk_rsc_ctx_frame_bundle *bundle_data);

int mtk_rsc_ctx_fmt_set_img(struct mtk_rsc_ctx *dev_ctx, int queue_id,
			   struct v4l2_pix_format_mplane *user_fmt,
			   struct v4l2_pix_format_mplane *node_fmt);

int mtk_rsc_ctx_fmt_set_meta(struct mtk_rsc_ctx *dev_ctx, int queue_id,
			    struct v4l2_meta_format *user_fmt,
			    struct v4l2_meta_format *node_fmt);

int mtk_rsc_ctx_format_load_default_fmt(struct mtk_rsc_ctx_queue *queue,
				       struct v4l2_format *fmt_to_fill);

int mtk_rsc_ctx_streamon(struct mtk_rsc_ctx *dev_ctx);
int mtk_rsc_ctx_streamoff(struct mtk_rsc_ctx *dev_ctx);
int mtk_rsc_ctx_release(struct mtk_rsc_ctx *dev_ctx);
int mtk_rsc_ctx_open(struct mtk_rsc_ctx *dev_ctx);

#endif /*__MTK_RSC_CTX_H__*/
