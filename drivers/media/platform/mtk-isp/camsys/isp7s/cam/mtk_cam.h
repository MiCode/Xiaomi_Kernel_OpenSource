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

/* for seninf pad enum... */
#include "imgsensor-user.h"

#include "mtk_cam-ctrl.h"
#include "mtk_cam-debug_option.h"
#include "mtk_cam-hsf-def.h"
#include "mtk_cam-ipi_7_1.h"
#include "mtk_cam-job.h"
#include "mtk_cam-larb.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-request.h"
#include "mtk_cam-seninf-drv.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-sv.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_cam-dvfs_qos.h"

#define CCD_READY 1


struct mtk_cam_debug_fs;
struct mtk_cam_request;
struct mtk_raw_pipeline;
struct mtk_sv_pipeline;
struct mtk_mraw_pipeline;

struct mtk_cam_device;
struct mtk_rpmsg_device;


#define JOB_NUM_PER_STREAM 5
#define MAX_PIPES_PER_STREAM 5

struct mtk_cam_driver_buf_desc {
	int ipi_fmt;
	int width;
	int height;
	int stride[3];
	size_t size;

	/* for userspace only */
	dma_addr_t daddr;
	int fd;
};

struct mtk_cam_ctx {
	struct mtk_cam_device *cam;
	unsigned int stream_id;

	/* v4l2 related */
	unsigned int enabled_node_cnt;
	unsigned int streaming_node_cnt;
	int has_raw_subdev;

	struct media_pipeline pipeline;
	struct v4l2_subdev *sensor;
	//struct v4l2_subdev *prev_sensor;
	struct v4l2_subdev *seninf;
	//struct v4l2_subdev *prev_seninf;
	struct v4l2_subdev *pipe_subdevs[MAX_PIPES_PER_STREAM];

	/* TODO */
	int raw_subdev_idx;
	int camsv_subdev_idx[2];
	/* stored raw data for switch exp case : prev : 1exp , next: 2exp */
	struct mtk_raw_ctrl_data ctldata_stored;
	s64 feature_config;
	/* job pool */
	struct mtk_cam_job_data jobs[JOB_NUM_PER_STREAM];
	struct mtk_cam_pool job_pool;
	int available_jobs; /* cached value for enque */

	/* rpmsg related */
	struct rpmsg_channel_info rpmsg_channel;
	struct mtk_rpmsg_device *rpmsg_dev;
	struct work_struct session_work;
	int ipi_id;
	bool session_created;
	struct completion session_complete;

	struct task_struct *sensor_worker_task;
	struct kthread_worker sensor_worker;
	struct workqueue_struct *composer_wq;
	struct workqueue_struct *frame_done_wq;

	struct mtk_cam_device_buf cq_buffer;
	struct mtk_cam_device_buf ipi_buffer;

	struct mtk_cam_device_buf hdr_buffer;
	struct mtk_cam_driver_buf_desc hdr_buf_desc;

	struct mtk_cam_pool	cq_pool;
	struct mtk_cam_pool	ipi_pool;

	/* TODO:
	 * life-cycle of work buffer during switch
	 * e.g., PDI/camsv's imgo
	 */

	atomic_t streaming;
	int used_pipe;
	int used_engine;

	bool not_first_job;
	bool configured;
	struct mtkcam_ipi_config_param ipi_config;

	struct device *hw_raw;
	struct device *hw_sv[2];
	//struct mtk_raw_pipeline *pipe;
	//struct mtk_camsv_pipeline *sv_pipe[MAX_SV_PIPES_PER_STREAM];
	//struct mtk_mraw_pipeline *mraw_pipe[MAX_MRAW_PIPES_PER_STREAM];
	struct mtk_cam_ctrl cam_ctrl;
	/* list for struct mtk_cam_job */
};

struct mtk_cam_v4l2_pipelines {
	int num_raw;
	struct mtk_raw_pipeline *raw;

	int num_camsv;
	struct mtk_camsv_pipeline *camsv;

	int num_mraw;
	struct mtk_mraw_pipeline *mraw;
};
int ctx_stream_on_seninf_sensor_hdr(struct mtk_cam_ctx *ctx,
	int enable, int seninf_pad, int pixel_mode, int tg_idx);

int ctx_stream_on_seninf_sensor(struct mtk_cam_ctx *ctx, int enable);

struct mtk_cam_engines {
	int num_seninf_devices;

	int num_raw_devices;
	int num_camsv_devices;
	int num_mraw_devices;
	int num_larb_devices;

	/* raw */
	struct device **raw_devs;
	struct device **yuv_devs;

	/* camsv */
	struct device **sv_devs;

	/* mraw */
	struct device **mraw_devs;

	/* larb */
	struct device **larb_devs;

	int occupied_engine;
};

struct mtk_cam_device {
	struct device *dev;
	void __iomem *base;

	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	struct media_device media_dev;

	atomic_t initialize_cnt;

	//TODO: for real SCP
	//struct device *smem_dev;
	//struct platform_device *scp_pdev; /* only for scp case? */
	phandle rproc_phandle;
	struct rproc *rproc_handle;

	phandle rproc_ccu_phandle;
	struct rproc *rproc_ccu_handle;

	struct mtk_cam_v4l2_pipelines	pipelines;
	struct mtk_cam_engines		engines;

	/* to guarantee the enque sequence */
	atomic_t is_queuing;

	unsigned int max_stream_num;
	struct mtk_cam_ctx *ctxs;

	spinlock_t streaming_lock;
	int streaming_ctx;
	//int streaming_pipe;

	/* request related */
	struct list_head pending_job_list;
	spinlock_t pending_job_lock;

	struct list_head running_job_list;
	unsigned int running_job_count;
	spinlock_t running_job_lock;

	struct mtk_camsys_dvfs dvfs;
	//struct mtk_cam_debug_fs *debug_fs;
	//struct workqueue_struct *debug_wq;
	//struct workqueue_struct *debug_exception_wq;
	//wait_queue_head_t debug_exception_waitq;
};

static inline struct device *subdev_to_cam_dev(struct v4l2_subdev *sd)
{
	return sd->v4l2_dev->dev;
}

static inline struct mtk_cam_device *subdev_to_cam_device(struct v4l2_subdev *sd)
{
	return dev_get_drvdata(subdev_to_cam_dev(sd));
}

struct device *mtk_cam_root_dev(void);

int mtk_cam_set_dev_raw(struct device *dev, int idx,
			struct device *raw, struct device *yuv);
int mtk_cam_set_dev_sv(struct device *dev, int idx, struct device *sv);
int mtk_cam_set_dev_mraw(struct device *dev, int idx, struct device *mraw);
 /* special case: larb dev is push back into array */
int mtk_cam_set_dev_larb(struct device *dev, struct device *larb);
struct device *mtk_cam_get_larb(struct device *dev, int larb_id);

bool mtk_cam_is_any_streaming(struct mtk_cam_device *cam);
bool mtk_cam_are_all_streaming(struct mtk_cam_device *cam, int stream_mask);

int mtk_cam_get_available_engine(struct mtk_cam_device *cam);
int mtk_cam_update_engine_status(struct mtk_cam_device *cam, int engine_mask,
				  bool available);
static inline int mtk_cam_release_engine(struct mtk_cam_device *cam, int engines)
{
	return mtk_cam_update_engine_status(cam, engines, true);
}

static inline int mtk_cam_occupy_engine(struct mtk_cam_device *cam, int engines)
{
	return mtk_cam_update_engine_status(cam, engines, false);
}

int mtk_cam_pm_runtime_engines(struct mtk_cam_engines *eng,
			       int engine_mask, int enable);

/* note: flag V4L2_MBUS_FRAMEFMT_PAD_ENABLE is defined by mtk internally */
static inline void
mtk_cam_pad_fmt_enable(struct v4l2_mbus_framefmt *framefmt, bool enable)
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

struct mtk_cam_ctx *mtk_cam_find_ctx(struct mtk_cam_device *cam,
				     struct media_entity *entity);
struct mtk_cam_ctx *mtk_cam_start_ctx(struct mtk_cam_device *cam,
				      struct mtk_cam_video_device *node);
void mtk_cam_stop_ctx(struct mtk_cam_ctx *ctx, struct media_entity *entity);
int mtk_cam_ctx_all_nodes_streaming(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_all_nodes_idle(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_stream_on(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_stream_off(struct mtk_cam_ctx *ctx);
int mtk_cam_ctx_send_raw_event(struct mtk_cam_ctx *ctx,
			       struct v4l2_event *event);
void mtk_cam_ctx_job_finish(struct mtk_cam_ctx *ctx, struct mtk_cam_job *job);

int isp_composer_create_session(struct mtk_cam_ctx *ctx);
void isp_composer_destroy_session(struct mtk_cam_ctx *ctx);

int mtk_cam_call_seninf_set_pixelmode(struct mtk_cam_ctx *ctx,
				      struct v4l2_subdev *sd,
				      int pad_id, int pixel_mode);

int mtk_cam_dev_req_enqueue(struct mtk_cam_device *cam,
			    struct mtk_cam_request *req);

/* use ipi pipe/node id */
void mtk_cam_req_buffer_done(struct mtk_cam_request *req,
			     int pipe_id, int buf_state, int node_id, u64 ts);
//void mtk_cam_dev_req_cleanup(struct mtk_cam_ctx *ctx, int pipe_id, int buf_state);
//void mtk_cam_dev_req_clean_pending(struct mtk_cam_device *cam, int pipe_id,
//				   int buf_state);

int mtk_cam_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_seninf_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_sv_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);
int mtk_cam_mraw_link_validate(struct v4l2_subdev *sd,
			  struct media_link *link,
			  struct v4l2_subdev_format *source_fmt,
			  struct v4l2_subdev_format *sink_fmt);

#endif /*__MTK_CAM_H*/
