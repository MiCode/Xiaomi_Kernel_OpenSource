/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_JOB_H
#define __MTK_CAM_JOB_H

#include <linux/list.h>
#include <media/media-request.h>

#include "mtk_cam-pool.h"
#include "mtk_cam-ipi_7_1.h"

struct mtk_cam_job;
enum MTK_CAM_JOB_ACTION {
	/* send_event : try_set_sensor */
	CAM_JOB_APPLY_SENSOR = 0,		/* BIT(0) = 1 */
	CAM_JOB_APPLY_FS,			/* BIT(1) = 2 */
	/* send_event : frame_start */
	CAM_JOB_APPLY_CQ,			/* BIT(2) = 4 */
	CAM_JOB_SETUP_TIMER,			/* BIT(3) = 8 */
	CAM_JOB_READ_DEQ_NO,			/* BIT(4) = 16 */
	CAM_JOB_VSYNC,			/* BIT(5) = 32 */
	CAM_JOB_SENSOR_DELAY,			/* BIT(6) = 64 */
	CAM_JOB_CQ_DELAY,			/* BIT(7) = 128 */
	CAM_JOB_HW_DELAY,			/* BIT(8) = 256 */
	/* send_event : setting_done*/
	CAM_JOB_STREAM_ON,			/* BIT(9) = 512 */
	CAM_JOB_EXP_NUM_SWITCH,			/* BIT(10) = 1024 */
	CAM_JOB_SENSOR_SWITCH,			/* BIT(11) = 2048 */
	/* send_event : frame_done/meta_done */
	CAM_JOB_DEQUE_ALL,			/* BIT(12) = 4096 */
	CAM_JOB_DEQUE_META1,			/* BIT(13) = 8192 */
};
enum MTK_CAMSYS_ENGINE_TYPE {
	CAMSYS_ENGINE_RAW,
	CAMSYS_ENGINE_MRAW,
	CAMSYS_ENGINE_CAMSV,
	CAMSYS_ENGINE_SENINF,
};

enum MTK_CAMSYS_IRQ_EVENT {
	/* with normal_data */
	CAMSYS_IRQ_SETTING_DONE = 0,
	CAMSYS_IRQ_FRAME_START,
	CAMSYS_IRQ_AFO_DONE,
	CAMSYS_IRQ_FRAME_DONE,
	CAMSYS_IRQ_TRY_SENSOR_SET,
	CAMSYS_IRQ_FRAME_DROP,
	CAMSYS_IRQ_FRAME_START_DCIF_MAIN,
	CAMSYS_IRQ_FRAME_SKIPPED,

	/* with error_data */
	CAMSYS_IRQ_ERROR,
};
enum EXP_CHANGE_TYPE {
	EXPOSURE_CHANGE_NONE = 0,
	EXPOSURE_CHANGE_3_to_2,
	EXPOSURE_CHANGE_3_to_1,
	EXPOSURE_CHANGE_2_to_3,
	EXPOSURE_CHANGE_2_to_1,
	EXPOSURE_CHANGE_1_to_3,
	EXPOSURE_CHANGE_1_to_2,
	MSTREAM_EXPOSURE_CHANGE = (1 << 4),
};

struct mtk_camsys_irq_normal_data {
};

struct mtk_camsys_irq_error_data {
	int err_status;
};

struct mtk_camsys_irq_info {
	enum MTK_CAMSYS_IRQ_EVENT irq_type;
	int engine;
	int ctx_id;
	u64 ts_ns;
	int frame_idx;
	int frame_idx_inner;
	int write_cnt;
	int fbc_cnt;
	int isp_request_seq_no;
	int reset_seq_no;
	union {
		struct mtk_camsys_irq_normal_data	n;
		struct mtk_camsys_irq_error_data	e;
	};
};

struct mtk_cam_request;
struct mtk_cam_ctx;

struct mtk_cam_job_work {
	struct work_struct work;
	atomic_t is_queued;
};
struct mtk_cam_req_feature {
	int feature_config; /* pack stage*/
	int raw_feature;
	int prev_feature;
	int switch_feature_type;
	bool switch_prev_frame_done;
	bool switch_curr_setting_done;
	bool switch_done;
};


struct mtk_cam_job {
	struct mtk_cam_request *req;
	struct list_head list; /* entry in state_list */

	/* Note:
	 * it's dangerous to fetch info from src_ctx
	 * src_ctx is just kept to access worker/workqueue.
	 */
	struct mtk_cam_ctx *src_ctx;

	struct mtk_cam_pool_buffer cq;
	struct mtk_cam_pool_buffer ipi;
	struct mtkcam_ipi_frame_ack_result cq_rst;
	int used_engine;
	bool do_ipi_config;
	struct mtkcam_ipi_config_param ipi_config;
	bool stream_on_seninf;

	/* job private start */
	int state;		/* job state machine used - only job layer */
	int job_type;	/* job type - only job layer */
	int ctx_id;
	int frame_seq_no;
	unsigned int flags;
	bool composed;
	int sensor_set_margin;	/* allow apply sensor before SOF + x (ms)*/
	struct mtk_cam_job_work frame_done_work;
	struct mtk_cam_job_work meta1_done_work;
	struct mtk_cam_req_feature feature;
	u64 timestamp;
	u64 timestamp_mono;
	struct media_request_object *sensor_hdl_obj;	/* for complete only - TBC*/
	struct v4l2_subdev *sensor;
	int link_engine;
	int proc_engine;
	atomic_t seninf_dump_state;

	u64 (*timestamp_buf)[128];

	/* hw devices */
	struct device *hw_raw;
	/* job private end */

	struct {
		/* job control */
		int (*wait_done)(struct mtk_cam_job *job);
		int (*cancel)(struct mtk_cam_job *job);
		int (*dump)(struct mtk_cam_job *job /*, ... */);

		/* should alway be called for clean-up resources */
		void (*finalize)(struct mtk_cam_job *job);

		/* event handle */
		int (*update_event)(struct mtk_cam_job *job,
				    struct mtk_camsys_irq_info *irq_info,
				    int *action);
		/* action */
		int (*stream_on)(struct mtk_cam_job *job,
						bool on);
		int (*reset)(struct mtk_cam_job *job);
		int (*compose)(struct mtk_cam_job *job);

		/* only do job->composed = true */
		int (*compose_done)(struct mtk_cam_job *job,
				    struct mtkcam_ipi_frame_ack_result *cq_ret);

		int (*apply_sensor)(struct mtk_cam_job *job);
		int (*apply_fs)(struct mtk_cam_job *job);
		int (*apply_isp)(struct mtk_cam_job *job);
		int (*apply_exp_switch)(struct mtk_cam_job *job);
		int (*frame_done)(struct mtk_cam_job *job);
		int (*afo_done)(struct mtk_cam_job *job);
	} ops;
};

#define CALL_JOB(job, func, ...) \
({\
	typeof(job) _job = (job);\
	_job->ops.func ? _job->ops.func(_job, ##__VA_ARGS__) : -EINVAL;\
})

#define CALL_JOB_OPT(job, func, ...)\
({\
	typeof(job) _job = (job);\
	_job->ops.func ? _job->ops.func(_job, ##__VA_ARGS__) : 0;\
})


struct mtk_cam_normal_job {
	struct mtk_cam_job job; /* always on top */

	/* TODO */
};
struct mtk_cam_stagger_job {
	struct mtk_cam_job job; /* always on top */

	/* TODO */
};
struct mtk_cam_mstream_job {
	struct mtk_cam_job job; /* always on top */

	/* TODO */
};
struct mtk_cam_timeshare_job {
	struct mtk_cam_job job; /* always on top */

	/* TODO */
};

struct mtk_cam_pool_job {
	struct mtk_cam_pool_priv priv;
	struct mtk_cam_job_data *job_data;
};

/* this struct is for job-pool */
struct mtk_cam_job_data {
	struct mtk_cam_pool_job pool_job;

	union {
		struct mtk_cam_normal_job n;
		struct mtk_cam_stagger_job s;
		struct mtk_cam_mstream_job m;
		struct mtk_cam_timeshare_job t;
	};
};
unsigned int decode_fh_reserved_data_to_ctx(u32 data_in);
unsigned int decode_fh_reserved_data_to_seq(u32 ref_near_by, u32 data_in);
unsigned int encode_fh_reserved_data(u32 ctx_id_in, u32 seq_no_in);



static inline struct mtk_cam_job_data *job_to_data(struct mtk_cam_job *job)
{
	return container_of(job, struct mtk_cam_job_data, m.job);
}

static inline struct mtk_cam_job *data_to_job(struct mtk_cam_job_data *data)
{
	return &data->n.job;
}

static inline void mtk_cam_job_return(struct mtk_cam_job *job)
{
	struct mtk_cam_job_data *data = job_to_data(job);

	mtk_cam_pool_return(&data->pool_job, sizeof(data->pool_job));
}

int mtk_cam_job_pack(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req);

#endif //__MTK_CAM_JOB_H
