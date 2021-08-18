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
#include "mtk_cam-sv.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-ipi.h"
#include "imgsensor-user.h"
#include "mtk_cam-seninf-drv.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-ctrl.h"
#include "mtk_cam-debug.h"
#include "mtk_camera-v4l2-controls.h"

#define MTK_CAM_REQ_MAX_S_DATA	2

/* for cq working buffers */
#define CQ_BUF_SIZE  0x8000
#define CAM_CQ_BUF_NUM 16
#define CAMSV_WORKING_BUF_NUM 64
#define IPI_FRAME_BUF_SIZE 0x8000

/* for time-sharing camsv working buffer, (1inner+2backendprogramming+2backup)*/
#define CAM_IMG_BUF_NUM (5)

#define CCD_READY 1

#define MAX_STAGGER_EXP_AMOUNT 3

#define MAX_PIPES_PER_STREAM 5
#define MAX_SV_PIPES_PER_STREAM (MAX_PIPES_PER_STREAM-1)

struct platform_device;
struct mtk_rpmsg_device;
struct mtk_cam_debug_fs;
struct mtk_cam_request;

#define SENSOR_FMT_MASK			0xFFFF

/* flags of mtk_cam_request */
#define MTK_CAM_REQ_FLAG_SENINF_CHANGED			BIT(0)

#define MTK_CAM_REQ_FLAG_SENINF_IMMEDIATE_UPDATE	BIT(1)

/* flags of mtk_cam_request_stream_data */
#define MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH		BIT(0)

#define MTK_CAM_REQ_S_DATA_FLAG_META1_INDEPENDENT	BIT(1)

struct mtk_cam_working_buf {
	void *va;
	dma_addr_t iova;
	int size;
};

struct mtk_cam_msg_buf {
	void *va;
	int size;
};

struct mtk_cam_dmao_buf {
	void *va;
	dma_addr_t iova;
	int size;
	int fd;
};

/* TODO: remove this entry wrapper */
struct mtk_cam_working_buf_entry {
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_working_buf buffer;
	struct mtk_cam_dmao_buf meta_buffer;
	struct mtk_cam_msg_buf msg_buffer;
	struct list_head list_entry;
	int cq_desc_offset;
	int cq_desc_size;
	int sub_cq_desc_offset;
	int sub_cq_desc_size;
};

struct mtk_cam_img_working_buf_entry {
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_working_buf img_buffer;
	struct list_head list_entry;
};

struct mtk_cam_working_buf_list {
	struct list_head list;
	u32 cnt;
	spinlock_t lock; /* protect the list and cnt */
};

struct mtk_camsv_working_buf_entry {
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request_stream_data *s_data;
	struct list_head list_entry;
};

struct mtk_camsv_working_buf_list {
	struct list_head list;
	u32 cnt;
	spinlock_t lock; /* protect the list and cnt */
};

struct mtk_cam_req_work {
	struct work_struct work;
	struct mtk_cam_request_stream_data *s_data;
	struct list_head list;
	atomic_t is_queued;
};

static inline struct mtk_cam_request_stream_data*
mtk_cam_req_work_get_s_data(struct mtk_cam_req_work *work)
{
	return work->s_data;
}

struct mtk_cam_req_feature {
	int raw_feature;
	int switch_feature_type;
};

/*
 * struct mtk_cam_request_stream_data - per stream members of a request
 *
 * @pad_fmt: pad format configurtion for sensor switch.
 * @frame_params: The frame info. & address info. of enabled DMA nodes.
 * @frame_work: work queue entry for frame transmission to SCP.
 * @working_buf: command queue buffer associated to this request
 * @mtk_cam_exposure: exposure value of sensor of mstream
 *
 */
struct mtk_cam_request_stream_data {
	struct mtk_cam_request *req;
	struct mtk_cam_ctx *ctx;
	int pipe_id;
	unsigned int frame_seq_no;
	unsigned int flags;
	u64 timestamp;
	u64 timestamp_mono;
	struct mtk_cam_buffer *bufs[MTK_RAW_TOTAL_NODES];
	struct v4l2_subdev *seninf_old;
	struct v4l2_subdev *seninf_new;
	u32 pad_fmt_update;
	u32 vdev_fmt_update;
	u32 vdev_selection_update;
	struct v4l2_subdev_format pad_fmt[MTK_RAW_PIPELINE_PADS_NUM];
	struct v4l2_format vdev_fmt[MTK_RAW_TOTAL_NODES];
	struct v4l2_selection vdev_selection[MTK_RAW_TOTAL_NODES];
	struct mtkcam_ipi_frame_param frame_params;
	struct mtk_camsv_frame_params sv_frame_params;
	struct mtk_cam_req_work frame_work;
	struct mtk_cam_req_work sensor_work;
	struct mtk_cam_req_work meta1_done_work;
	struct mtk_cam_req_work frame_done_work;
	struct mtk_cam_req_work sv_work;
	struct mtk_camsys_ctrl_state state;
	struct mtk_cam_working_buf_entry *working_buf;
	unsigned int no_frame_done_cnt;
	atomic_t seninf_dump_state;
	struct mtk_cam_req_dbg_work dbg_work;
	struct mtk_cam_req_dbg_work dbg_exception_work;
	struct mtk_cam_req_feature feature;
	struct mtk_cam_tg_flash_config tg_flash_config;
	bool frame_done_queue_work;
	struct mtk_cam_shutter_gain mtk_cam_exposure;
};

struct mtk_cam_req_pipe {
	int s_data_num;
	int req_seq;
	struct mtk_cam_request_stream_data s_data[MTK_CAM_REQ_MAX_S_DATA];
};

/*
 * struct mtk_cam_request - MTK camera request.
 *
 * @req: Embedded struct media request.
 * @ctx_used: conctext used in this request
 * @ctx_link_update: contexts have update link
 * @pipe_used: pipe used in this request. Two or more pipes may share
 * the same context.
 * @frame_params: The frame info. & address info. of enabled DMA nodes.
 * @frame_work: work queue entry for frame transmission to SCP.
 * @list: List entry of the object for @struct mtk_cam_device:
 *        pending_job_list or running_job_list.
 * @time_syscall_enque: log the request enqueue time
 * @mtk_cam_request_stream_data: stream context related to the request
 *
 */
struct mtk_cam_request {
	struct media_request req;
	unsigned int pipe_used;
	unsigned int ctx_used;
	unsigned int ctx_link_update;
	unsigned int flags;
	unsigned int done_status;
	spinlock_t done_status_lock;
	unsigned int fs_on_cnt; /*0:init X:sensor_fs_on*/
	struct mutex fs_op_lock;
	struct list_head list;
	struct work_struct link_work;
	u64 time_syscall_enque;
	struct mtk_cam_req_pipe p_data[MTKCAM_SUBDEV_MAX];
	s64 sync_id;
};

struct mtk_cam_working_buf_pool {
	struct mtk_cam_ctx *ctx;

	struct dma_buf *working_buf_dmabuf;

	void *working_buf_va;
	void *msg_buf_va;
	dma_addr_t working_buf_iova;
	int working_buf_fd;
	int working_buf_size;
	int msg_buf_fd;
	int msg_buf_size;

	struct mtk_cam_working_buf_entry working_buf[CAM_CQ_BUF_NUM];
	struct mtk_cam_working_buf_list cam_freelist;

	struct mtk_camsv_working_buf_entry sv_working_buf[CAMSV_WORKING_BUF_NUM];
	struct mtk_camsv_working_buf_list sv_freelist;
};

struct mtk_cam_img_working_buf_pool {
	struct mtk_cam_ctx *ctx;
	struct dma_buf *working_img_buf_dmabuf;
	void *working_img_buf_va;
	dma_addr_t working_img_buf_iova;
	int working_img_buf_fd;
	int working_img_buf_size;
	struct mtk_cam_img_working_buf_entry img_working_buf[CAM_IMG_BUF_NUM];
	struct mtk_cam_working_buf_list cam_freeimglist;
};

struct mtk_cam_device;
struct mtk_camsys_ctrl;
struct mtk_cam_ctx {
	struct mtk_cam_device *cam;
	unsigned int stream_id;
	unsigned int streaming;
	unsigned int synced;
	struct media_pipeline pipeline;
	struct mtk_raw_pipeline *pipe;
	struct mtk_camsv_pipeline *sv_pipe[MAX_SV_PIPES_PER_STREAM];
	unsigned int enabled_node_cnt;
	unsigned int streaming_pipe;
	unsigned int streaming_node_cnt;
	atomic_t running_s_data_cnt;
	struct v4l2_subdev *sensor;
	struct v4l2_subdev *prev_sensor;
	struct v4l2_subdev *seninf;
	struct v4l2_subdev *prev_seninf;
	struct v4l2_subdev *pipe_subdevs[MAX_PIPES_PER_STREAM];
	struct mtk_camsys_sensor_ctrl sensor_ctrl;

	unsigned int used_raw_num;
	unsigned int used_raw_dev;
	unsigned int used_raw_dmas;

	unsigned int used_sv_num;
	unsigned int used_sv_dev[MAX_SV_PIPES_PER_STREAM];

	struct workqueue_struct *composer_wq;
	struct workqueue_struct *frame_done_wq;
	struct workqueue_struct *sv_wq;

	struct rpmsg_channel_info rpmsg_channel;
	struct mtk_rpmsg_device *rpmsg_dev;

	//  TODO: how to support multi-stream with frame-sync?
	struct mtk_cam_working_buf_pool buf_pool;
	struct mtk_cam_working_buf_list using_buffer_list;
	struct mtk_cam_working_buf_list composed_buffer_list;
	struct mtk_cam_working_buf_list processing_buffer_list;

	struct mtk_camsv_working_buf_list sv_using_buffer_list[MAX_SV_PIPES_PER_STREAM];
	struct mtk_camsv_working_buf_list sv_processing_buffer_list[MAX_SV_PIPES_PER_STREAM];

	/* sensor image buffer pool handling from kernel */
	struct mtk_cam_img_working_buf_pool img_buf_pool;
	struct mtk_cam_working_buf_list processing_img_buffer_list;

	unsigned int enqueued_frame_seq_no;
	unsigned int composed_frame_seq_no;
	unsigned int dequeued_frame_seq_no;

	/* mstream */
	unsigned int enqueued_request_cnt;
	unsigned int next_sof_mask_frame_seq_no;
	unsigned int working_request_seq;

	unsigned int sv_dequeued_frame_seq_no[MAX_SV_PIPES_PER_STREAM];

	spinlock_t streaming_lock;
	spinlock_t m2m_lock;

	/* To support debug dump */
	struct mtkcam_ipi_config_param config_params;

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

	struct workqueue_struct *link_change_wq;
	unsigned int composer_cnt;

	unsigned int num_seninf_drivers;
	unsigned int num_raw_drivers;
	unsigned int num_larb_drivers;
	unsigned int num_camsv_drivers;

	struct mtk_raw raw;
	struct mtk_larb larb;
	struct mtk_camsv sv;

	//TODO: mraw
	//int mraw_num;
	//struct mraw_device mraw;

	/* To protect topology-related operations & ctx */
	struct mutex op_lock;

	unsigned int max_stream_num;
	unsigned int streaming_ctx;
	unsigned int streaming_pipe;
	struct mtk_cam_ctx *ctxs;

	/* request related */
	struct list_head pending_job_list;
	spinlock_t pending_job_lock;
	struct list_head running_job_list;
	unsigned int running_job_count;
	spinlock_t running_job_lock;
	struct mtk_camsys_ctrl camsys_ctrl;

	struct mtk_cam_debug_fs *debug_fs;
	struct workqueue_struct *debug_wq;
	struct workqueue_struct *debug_exception_wq;
	wait_queue_head_t debug_exception_waitq;

};

static inline struct mtk_cam_request_stream_data*
mtk_cam_ctrl_state_to_req_s_data(struct mtk_camsys_ctrl_state *state)
{
	return container_of(state, struct mtk_cam_request_stream_data, state);
}

static inline struct mtk_cam_request*
mtk_cam_ctrl_state_get_req(struct mtk_camsys_ctrl_state *state)
{
	struct mtk_cam_request_stream_data *request_stream_data;

	request_stream_data = mtk_cam_ctrl_state_to_req_s_data(state);
	return request_stream_data->req;
}

static inline int
mtk_cam_req_get_num_s_data(struct mtk_cam_request *req, int pipe_id)
{
	if (pipe_id < 0 || pipe_id > MTKCAM_SUBDEV_MAX)
		return 0;

	return req->p_data[pipe_id].s_data_num;
}

/**
 * Be used operation between request reinit and enqueue.
 * For example, request-based set fmt and selection.
 */
static inline struct mtk_cam_request_stream_data*
mtk_cam_req_get_s_data_no_chk(struct mtk_cam_request *req, int pipe_id, int idx)
{
	return &req->p_data[pipe_id].s_data[idx];
}

static inline struct mtk_cam_request_stream_data*
mtk_cam_req_get_s_data(struct mtk_cam_request *req, int pipe_id, int idx)
{
	if (!req || pipe_id < 0 || pipe_id > MTKCAM_SUBDEV_MAX)
		return NULL;

	if (idx < 0 || idx >= req->p_data[pipe_id].s_data_num)
		return NULL;

	return mtk_cam_req_get_s_data_no_chk(req, pipe_id, idx);
}

static inline struct mtk_cam_request_stream_data*
mtk_cam_wbuf_get_s_data(struct mtk_cam_working_buf_entry *buf_entry)
{
	return buf_entry->s_data;
}

static inline void
mtk_cam_img_wbuf_set_s_data(struct mtk_cam_img_working_buf_entry *buf_entry,
	struct mtk_cam_request_stream_data *s_data)
{
	buf_entry->s_data = s_data;
}

static inline void
mtk_cam_sv_wbuf_set_s_data(struct mtk_camsv_working_buf_entry *buf_entry,
			   struct mtk_cam_request_stream_data *s_data)
{
	buf_entry->s_data = s_data;
}


static inline struct mtk_cam_ctx*
mtk_cam_s_data_get_ctx(struct mtk_cam_request_stream_data *s_data)
{
	return s_data->ctx;
}

static inline char*
mtk_cam_s_data_get_dbg_str(struct mtk_cam_request_stream_data *s_data)
{
	return s_data->req->req.debug_str;
}

static inline struct mtk_cam_request*
mtk_cam_s_data_get_req(struct mtk_cam_request_stream_data *s_data)
{
	return s_data->req;
}

static inline int
mtk_cam_s_data_get_vbuf_idx(struct mtk_cam_request_stream_data *s_data,
			    int node_id)
{

	if (s_data->pipe_id >= MTKCAM_SUBDEV_RAW_START &&
		s_data->pipe_id < MTKCAM_SUBDEV_RAW_END)
		return node_id - MTK_RAW_SINK_NUM;

	if (s_data->pipe_id >= MTKCAM_SUBDEV_CAMSV_START &&
		s_data->pipe_id < MTKCAM_SUBDEV_CAMSV_END)
		return  node_id - MTK_CAMSV_SINK_NUM;

	return -1;
}


static inline void
mtk_cam_s_data_set_vbuf(struct mtk_cam_request_stream_data *s_data,
			struct mtk_cam_buffer *buf, int node_id)
{
	int idx = mtk_cam_s_data_get_vbuf_idx(s_data, node_id);

	if (idx >= 0)
		s_data->bufs[idx] = buf;
}


static inline struct mtk_cam_buffer*
mtk_cam_s_data_get_vbuf(struct mtk_cam_request_stream_data *s_data,
			int node_id)
{
	int idx = mtk_cam_s_data_get_vbuf_idx(s_data, node_id);

	if (idx >= 0)
		return s_data->bufs[idx];

	return NULL;
}

static inline void
mtk_cam_s_data_reset_vbuf(struct mtk_cam_request_stream_data *s_data, int node_id)
{
	int idx = mtk_cam_s_data_get_vbuf_idx(s_data, node_id);

	if (idx >= 0)
		s_data->bufs[idx] = NULL;
}

static inline void
mtk_cam_s_data_set_wbuf(struct mtk_cam_request_stream_data *s_data,
			struct mtk_cam_working_buf_entry *buf_entry)
{
	buf_entry->s_data = s_data;
	s_data->working_buf = buf_entry;
}

static inline void
mtk_cam_s_data_reset_wbuf(struct mtk_cam_request_stream_data *s_data)
{
	if (!s_data->working_buf)
		return;

	s_data->working_buf->s_data = NULL;
	s_data->working_buf = NULL;
}

static inline struct mtk_cam_seninf_dump_work*
to_mtk_cam_seninf_dump_work(struct work_struct *work)
{
	return container_of(work, struct mtk_cam_seninf_dump_work, work);
}

static inline struct mtk_cam_request *
to_mtk_cam_req(struct media_request *__req)
{
	return container_of(__req, struct mtk_cam_request, req);
}

static inline void
mtk_cam_pad_fmt_enable(struct v4l2_mbus_framefmt *framefmt,
								bool enable)
{
	if (enable)
		framefmt->flags |= V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
	else
		framefmt->flags &= ~V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
}

static inline bool
mtk_cam_is_pad_fmt_enable(struct v4l2_mbus_framefmt *framefmt)
{
	return framefmt->flags & V4L2_MBUS_FRAMEFMT_PAD_ENABLE;
}

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
void mtk_cam_dev_req_cleanup(struct mtk_cam_ctx *ctx, int pipe_id);

void mtk_cam_dev_req_try_queue(struct mtk_cam_device *cam);

void mtk_cam_s_data_update_timestamp(struct mtk_cam_ctx *ctx,
				     struct mtk_cam_buffer *buf,
				     struct mtk_cam_request_stream_data *s_data);

bool mtk_cam_dequeue_req_frame(struct mtk_cam_ctx *ctx,
			       unsigned int dequeued_frame_seq_no,
			       int pipe_id);

void mtk_cam_dev_job_done(struct mtk_cam_ctx *ctx,
			  struct mtk_cam_request *req,
			  int pipe_id,
			  enum vb2_buffer_state state);

int mtk_cam_dev_config(struct mtk_cam_ctx *ctx, bool streaming, bool config_pipe);

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);

struct mtk_cam_request *mtk_cam_get_req(struct mtk_cam_ctx *ctx,
					unsigned int frame_seq_no);

void mtk_cam_req_update_seq(struct mtk_cam_ctx *ctx, struct mtk_cam_request *req,
			    int seq);

struct mtk_cam_request_stream_data*
mtk_cam_get_req_s_data(struct mtk_cam_ctx *ctx,
					unsigned int pipe_id, unsigned int frame_seq_no);

struct mtk_raw_pipeline *mtk_cam_dev_get_raw_pipeline(struct mtk_cam_device *cam,
						      unsigned int id);

int get_main_sv_pipe_id(struct mtk_cam_device *cam, int used_dev_mask);
int get_sub_sv_pipe_id(struct mtk_cam_device *cam, int used_dev_mask);
int get_last_sv_pipe_id(struct mtk_cam_device *cam, int used_dev_mask);

struct mtk_raw_device *get_master_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe);
struct mtk_raw_device *get_slave_raw_dev(struct mtk_cam_device *cam,
					 struct mtk_raw_pipeline *pipe);
struct mtk_raw_device *get_slave2_raw_dev(struct mtk_cam_device *cam,
					  struct mtk_raw_pipeline *pipe);
void isp_composer_create_session(struct mtk_cam_ctx *ctx);
s32 get_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp);
void set_format_request_fd(struct v4l2_pix_format_mplane *fmt_mp, s32 request_fd);
s32 get_crop_request_fd(struct v4l2_selection *crop);
void set_crop_request_fd(struct v4l2_selection *crop, s32 request_fd);

int PipeIDtoTGIDX(int pipe_id);
int mtk_cam_is_time_shared(struct mtk_cam_ctx *ctx);
int mtk_cam_is_stagger(struct mtk_cam_ctx *ctx);
int mtk_cam_is_stagger_m2m(struct mtk_cam_ctx *ctx);
int mtk_cam_is_mstream(struct mtk_cam_ctx *ctx);
int feature_is_mstream(int feature);
int feature_change_is_mstream(int feature_change);
int mtk_cam_node_is_mstream(struct mtk_cam_video_device *node);
void mstream_seamless_buf_update(struct mtk_cam_ctx *ctx,
				struct mtk_cam_request *req, int pipe_id,
				int previous_feature);
int mtk_cam_is_subsample(struct mtk_cam_ctx *ctx);
int mtk_cam_is_2_exposure(struct mtk_cam_ctx *ctx);
int mtk_cam_is_3_exposure(struct mtk_cam_ctx *ctx);
int mtk_cam_get_sensor_exposure_num(u32 raw_feature);

#endif /*__MTK_CAM_H*/
