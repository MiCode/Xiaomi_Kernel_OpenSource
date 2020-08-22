/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_H
#define __MTK_CAM_H

#include <linux/list.h>
#include <linux/of.h>
#include <linux/rpmsg.h>
#include <media/media-device.h>
#include <media/media-request.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_cam-raw.h"
#include "mtk_cam-ipi.h"
#include "mtk_cam-seninf-def.h"
#include "mtk_cam-seninf-drv.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-ctrl.h"

#define MAX_CONCURRENT_STREAM_NUM 3
#define MAX_MTKCAM_HW_CONFIG 6

#define MTK_ISP_CQ_BUFFER_COUNT			3
#define MTK_ISP_CQ_ADDRESS_OFFSET		0xfc8

#define CAM_SUB_FRM_DATA_NUM 32
#define WORKING_BUF_SIZE  0x4000

#define MTK_ISP_MIN_RESIZE_RATIO 25

struct platform_device;
struct mtk_rpmsg_device;

/* Supported image format list */
#define MTK_CAM_IMG_FMT_UNKNOWN		0x0000
#define MTK_CAM_IMG_FMT_BAYER8		0x2200
#define MTK_CAM_IMG_FMT_BAYER10		0x2201
#define MTK_CAM_IMG_FMT_BAYER12		0x2202
#define MTK_CAM_IMG_FMT_BAYER14		0x2203
#define MTK_CAM_IMG_FMT_FG_BAYER8	0x2204
#define MTK_CAM_IMG_FMT_FG_BAYER10	0x2205
#define MTK_CAM_IMG_FMT_FG_BAYER12	0x2206
#define MTK_CAM_IMG_FMT_FG_BAYER14	0x2207

/* Supported bayer pixel order */
#define MTK_CAM_RAW_PXL_ID_B		0
#define MTK_CAM_RAW_PXL_ID_GB		1
#define MTK_CAM_RAW_PXL_ID_GR		2
#define MTK_CAM_RAW_PXL_ID_R		3
#define MTK_CAM_RAW_PXL_ID_UNKNOWN	4

struct mtk_cam_working_buf {
	dma_addr_t pa;
	dma_addr_t va;
	dma_addr_t iova;
	int addr_offset;
	int size;
};

struct mtk_cam_working_buf_entry {
	struct mtk_cam_working_buf buffer;
	struct list_head list_entry;
	int cq_size;
};

struct mtk_cam_working_buf_list {
	struct list_head list;
	u32 cnt;
	spinlock_t lock; /* protect the list and cnt */
};

/*
 * struct mtk_cam_request - MTK camera request.
 *
 * @req: Embedded struct media request.
 * @ctx_used:
 * @frame_params: The frame info. & address info. of enabled DMA nodes.
 * @frame_work: work queue entry for frame transmission to SCP.
 * @list: List entry of the object for @struct mtk_cam_device:
 *        pending_job_list or running_job_list.
 * @buf_count: Buffer count in this media request.
 *
 */
struct mtk_cam_request {
	struct media_request req;
	unsigned int ctx_used;
	unsigned int frame_seq_no;
	struct mtkcam_ipi_frame_param frame_params;
	struct work_struct frame_work;
	struct list_head list;
	u64 timestamp;
	struct work_struct sensor_work;
	struct work_struct link_work;
	struct mtk_camsys_ctrl_state state;
};

static inline struct mtk_cam_request *
to_mtk_cam_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_request, req);
}

struct mtk_cam_device;
struct mtk_camsys_ctrl;
//TODO: modify it to correct value stream amount per pipe
#define MAX_PIPES_PER_STREAM 4
struct mtk_cam_ctx {
	struct mtk_cam_device *cam;
	int stream_id;
	unsigned int streaming;
	struct media_pipeline pipeline;
	struct mtk_raw_pipeline *pipe;
	unsigned int enabled_node_cnt;
	unsigned int streaming_node_cnt;
	struct v4l2_subdev *sensor;
	struct v4l2_subdev *prev_sensor;
	struct v4l2_subdev *seninf;
	struct v4l2_subdev *pipe_subdevs[MAX_PIPES_PER_STREAM];

	unsigned int used_raw_dev;
	unsigned int used_raw_dmas;

	//  TODO: how to support multi-stream with frame-sync?
	struct mtk_cam_working_buf_list using_buffer_list;
	struct mtk_cam_working_buf_list composed_buffer_list;
	struct mtk_cam_working_buf_list processing_buffer_list;

	unsigned int enqueued_frame_seq_no;
	unsigned int composed_frame_seq_no;
	unsigned int dequeued_frame_seq_no;

	spinlock_t streaming_lock;
};

struct mtk_cam_device {
	struct device *dev;

	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;
	void __iomem *base;
	//TODO: for real SCP
	//struct device *smem_dev;
	//struct platform_device *scp_pdev; /* only for scp case? */
	phandle rproc_phandle;
	struct rproc *rproc_handle;
	struct rpmsg_channel_info rpmsg_channel;
	struct mtk_rpmsg_device *rpmsg_dev;

	struct workqueue_struct *composer_wq;
	struct workqueue_struct *link_change_wq;
	unsigned int composer_cnt;

	//FIXME: temp, bad smelling, SCP
	dma_addr_t scp_mem_pa;
	dma_addr_t scp_mem_va;
	dma_addr_t scp_mem_iova;
	dma_addr_t scp_mem_fd;

	dma_addr_t working_buf_mem_pa;
	dma_addr_t working_buf_mem_va;
	dma_addr_t working_buf_mem_iova;
	dma_addr_t working_buf_mem_fd;
	int working_buf_mem_size;
	struct mtk_cam_working_buf_entry working_buf[CAM_SUB_FRM_DATA_NUM];
	struct mtk_cam_working_buf_list cam_freebufferlist;

	unsigned int num_mtkcam_seninf_drivers;
	unsigned int num_mtkcam_sub_drivers;

	struct mtk_raw raw;
	//TODO: camsv
	//int camsv_num;
	//struct camsv_device camsv;

	//TODO: mraw
	//int mraw_num;
	//struct mraw_device mraw;

	/* To protect topology-related operations & ctx */
	struct mutex op_lock;

	unsigned int max_stream_num;
	unsigned int streaming_ctx;
	struct mtk_cam_ctx *ctxs;

	/* request related */
	struct list_head pending_job_list;
	spinlock_t pending_job_lock;
	struct list_head running_job_list;
	unsigned int running_job_count;
	spinlock_t running_job_lock;
	struct mtk_camsys_ctrl camsys_ctrl;
};

//TODO: with spinlock or not? depends on how request works [TBD]
struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node);
struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity);
void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity);

int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx);

// FIXME: refine following
void mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			 struct mtk_cam_request *req);

void mtk_cam_dev_req_cleanup(struct mtk_cam_device *cam);

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam);

void mtk_cam_dequeue_req_frame(struct mtk_cam_device *cam,
				   struct mtk_cam_ctx *ctx);
int mtk_cam_dev_config(struct mtk_cam_ctx *ctx, unsigned int streaming);

struct mtk_cam_request *mtk_cam_dev_get_req(struct mtk_cam_device *cam,
					    struct mtk_cam_ctx *ctx,
					    unsigned int frame_seq_no);
void isp_composer_create_session(struct mtk_cam_device *cam,
					struct mtk_cam_ctx *ctx);

s32 get_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp);
void set_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp, s32 request_fd);

#endif /*__MTK_CAM_H*/
