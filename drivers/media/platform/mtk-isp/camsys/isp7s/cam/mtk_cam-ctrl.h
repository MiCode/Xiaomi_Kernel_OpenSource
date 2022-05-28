/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_CTRL_H
#define __MTK_CAM_CTRL_H

#include <linux/hrtimer.h>
#include <linux/timer.h>

#include "mtk_cam-job.h"
struct mtk_cam_device;
struct mtk_raw_device;
struct mtk_camsv_device;


/*per stream (sensor) */
struct mtk_cam_ctrl {
	struct mtk_cam_ctx *ctx;
	struct kthread_work work;
	struct hrtimer sensor_deadline_timer;
	u64 sof_time;
	int timer_req_event;
	atomic_t reset_seq_no;
	atomic_t enqueued_frame_seq_no;		/* enque job counter - ctrl maintain */
	atomic_t dequeued_frame_seq_no;		/* deque job counter - ctrl maintain */
	atomic_t sensor_request_seq_no;		/* sensor set counter - decided by job */
	atomic_t last_drained_seq_no;		/* if try set sensor at enque job - ctrl maintain */
	atomic_t isp_request_seq_no;		/* dbload counter - ctrl maintain */
	atomic_t isp_enq_seq_no;		/* subsample mode used - cq trigger counter */
	atomic_t isp_update_timer_seq_no;	/* mstream mode reserved */
	atomic_t stopped;
	atomic_t ref_cnt;
	int initial_cq_done;
	int vsync_engine;
	int component_dequeued_frame_seq_no;
	struct list_head camsys_state_list;
	spinlock_t camsys_state_lock;
	wait_queue_head_t stop_wq;
};
struct mtk_camsys_irq_normal_data {
};

struct mtk_camsys_irq_error_data {
	int err_status;
};

struct mtk_camsys_irq_info {
	enum MTK_CAMSYS_IRQ_EVENT irq_type;
	u64 ts_ns;
	int frame_idx;
	int frame_idx_inner;
	int write_cnt;
	int fbc_cnt;
	union {
		struct mtk_camsys_irq_normal_data	n;
		struct mtk_camsys_irq_error_data	e;
	};
};

int mtk_cam_ctrl_isr_event(struct mtk_cam_device *cam,
	enum MTK_CAMSYS_ENGINE_TYPE engine_type, unsigned int engine_id,
	struct mtk_camsys_irq_info *irq_info);

/* ctx_stream_on */
void mtk_cam_ctrl_start(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_ctx *ctx);
/* ctx_stream_off */
void mtk_cam_ctrl_stop(struct mtk_cam_ctrl *cam_ctrl);
/* enque job */
void mtk_cam_ctrl_job_enque(struct mtk_cam_ctrl *cam_ctrl,
	struct mtk_cam_job *job);
/* inform job composed */
void mtk_cam_ctrl_job_composed(struct mtk_cam_ctrl *cam_ctrl,
	int frame_seq, struct mtkcam_ipi_frame_ack_result *cq_ret);



#endif
