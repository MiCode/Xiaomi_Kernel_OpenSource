/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_JOB_H
#define __MTK_CAM_JOB_H

#include <linux/list.h>

#include "mtk_cam-pool.h"

struct mtk_cam_job;
struct mtk_cam_pool_job {
	struct mtk_cam_pool_priv priv;
	struct mtk_cam_job_data *job_data;
};

struct mtk_cam_request;
struct mtk_cam_ctx;
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

	int used_engine;
	bool do_config;
	struct mtkcam_ipi_config_param ipi_config;

	struct {
		/* job control */
		int (*wait_done)(struct mtk_cam_job *job);
		int (*cancel)(struct mtk_cam_job *job);
		int (*dump)(struct mtk_cam_job *job /*, ... */);

		/* event handle */
		int (*update_event)(struct mtk_cam_job *job,
				    /* struct event, */
				    int *action);
		/* action */
		int (*reset)(struct mtk_cam_job *job);
		int (*apply_sensor)(struct mtk_cam_job *job);
		int (*apply_isp)(struct mtk_cam_job *job);
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
	// TODO
};

struct mtk_cam_job_data {
	struct mtk_cam_pool_job pool_job;
	struct mtk_cam_job job;

	int job_type;
	union {
		struct mtk_cam_normal_job n;
	};
};

static inline struct mtk_cam_job_data *
mtk_cam_job_to_data(struct mtk_cam_job *job)
{
	return container_of(job, struct mtk_cam_job_data, job);
}

static inline void mtk_cam_job_return(struct mtk_cam_job *job)
{
	struct mtk_cam_job_data *data = mtk_cam_job_to_data(job);

	mtk_cam_pool_return(&data->pool_job, sizeof(data->pool_job));
}

int mtk_cam_job_pack(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req);

#endif //__MTK_CAM_JOB_H
