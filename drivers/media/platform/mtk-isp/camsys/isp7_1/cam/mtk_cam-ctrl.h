/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_CTRL_H
#define __MTK_CAM_CTRL_H

#include <linux/hrtimer.h>
#include <linux/timer.h>
#include "mtk_cam-dvfs_qos.h"

#define MTK_CAM_INITIAL_REQ_SYNC 0

struct mtk_cam_device;
struct mtk_raw_device;

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
	CAMSYS_IRQ_SUBSAMPLE_SENSOR_SET,
	CAMSYS_IRQ_FRAME_DROP,
	CAMSYS_IRQ_FRAME_START_DCIF_MAIN,
	CAMSYS_IRQ_FRAME_SKIPPED,

	/* with error_data */
	CAMSYS_IRQ_ERROR,
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

/*For state analysis and controlling for request*/
enum MTK_CAMSYS_STATE_IDX {
	E_STATE_READY = 0x0,
	E_STATE_SENINF,
	E_STATE_SENSOR,
	E_STATE_CQ,
	E_STATE_OUTER,
	E_STATE_CAMMUX_OUTER_CFG,
	E_STATE_CAMMUX_OUTER,
	E_STATE_INNER,
	E_STATE_DONE_NORMAL,
	E_STATE_CQ_SCQ_DELAY,
	E_STATE_CAMMUX_OUTER_CFG_DELAY,
	E_STATE_OUTER_HW_DELAY,
	E_STATE_INNER_HW_DELAY,
	E_STATE_DONE_MISMATCH,
	E_STATE_SUBSPL_READY = 0x10,
	E_STATE_SUBSPL_SCQ,
	E_STATE_SUBSPL_OUTER,
	E_STATE_SUBSPL_SENSOR,
	E_STATE_SUBSPL_INNER,
	E_STATE_SUBSPL_DONE_NORMAL,
	E_STATE_SUBSPL_SCQ_DELAY,
	E_STATE_TS_READY = 0x20,
	E_STATE_TS_SENSOR,
	E_STATE_TS_SV,
	E_STATE_TS_MEM,
	E_STATE_TS_CQ,
	E_STATE_TS_INNER,
	E_STATE_TS_DONE_NORMAL,
	E_STATE_EXTISP_READY = 0x30,
	E_STATE_EXTISP_SENSOR,
	E_STATE_EXTISP_SV_OUTER,
	E_STATE_EXTISP_SV_INNER,
	E_STATE_EXTISP_CQ,
	E_STATE_EXTISP_OUTER,
	E_STATE_EXTISP_INNER,
	E_STATE_EXTISP_DONE_NORMAL,
};

struct mtk_camsys_ctrl_state {
	enum MTK_CAMSYS_STATE_IDX estate;
	struct list_head state_element;
};

struct mtk_camsys_link_ctrl {
	struct mtk_raw_pipeline *pipe;
	struct media_pad remote;
	struct mtk_cam_ctx *swapping_ctx;
	u8 active;
	u8 wait_exchange;
};

/*per stream (sensor) */
struct mtk_camsys_sensor_ctrl {
	struct mtk_cam_ctx *ctx;
	struct kthread_worker *sensorsetting_wq;
	struct kthread_work work;
	struct hrtimer sensor_deadline_timer;
	u64 sof_time;
	int timer_req_sensor;
	int timer_req_event;
	atomic_t reset_seq_no;
	atomic_t sensor_enq_seq_no;
	atomic_t sensor_request_seq_no;
	atomic_t isp_request_seq_no;
	atomic_t isp_enq_seq_no;
	atomic_t isp_update_timer_seq_no;
	atomic_t last_drained_seq_no;
	int initial_cq_done;
	atomic_t initial_drop_frame_cnt;
	struct list_head camsys_state_list;
	spinlock_t camsys_state_lock;
	/* link change ctrl */
	struct mtk_camsys_link_ctrl link_ctrl;
	struct mtk_cam_request *link_change_req;
};

enum {
	EXPOSURE_CHANGE_NONE = 0,
	EXPOSURE_CHANGE_3_to_2,
	EXPOSURE_CHANGE_3_to_1,
	EXPOSURE_CHANGE_2_to_3,
	EXPOSURE_CHANGE_2_to_1,
	EXPOSURE_CHANGE_1_to_3,
	EXPOSURE_CHANGE_1_to_2,

	MSTREAM_EXPOSURE_CHANGE = (1 << 4),
};

struct mtk_camsys_ctrl {
	/* resource ctrl */
	struct mtk_camsys_dvfs dvfs_info;
};
void mtk_camsys_composed_delay_enque(struct mtk_raw_device *raw_dev,
				       struct mtk_cam_ctx *ctx,
				       struct mtk_cam_request_stream_data *req_stream_data);

void mtk_camsys_state_delete(struct mtk_cam_ctx *ctx,
				struct mtk_camsys_sensor_ctrl *sensor_ctrl,
				struct mtk_cam_request *req);
void mtk_camsys_frame_done(struct mtk_cam_ctx *ctx,
				  unsigned int frame_seq_no,
				  unsigned int pipe_id);

int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 enum MTK_CAMSYS_ENGINE_TYPE engine_type,
			 unsigned int engine_id,
			 struct mtk_camsys_irq_info *irq_info);
bool mtk_cam_submit_kwork_in_sensorctrl(struct kthread_worker *worker,
				 struct mtk_camsys_sensor_ctrl *sensor_ctrl);

void mtk_cam_initial_sensor_setup(struct mtk_cam_request *req,
					struct mtk_cam_ctx *ctx);
void mtk_cam_mstream_initial_sensor_setup(struct mtk_cam_request *req,
					struct mtk_cam_ctx *ctx);
void mtk_cam_mstream_mark_incomplete_frame(struct mtk_cam_ctx *ctx,
			struct mtk_cam_request_stream_data *incomplete_s_data);
void mtk_cam_req_ctrl_setup(struct mtk_raw_pipeline *raw_pipe,
			    struct mtk_cam_request *req);
int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx); /* ctx_stream_on */
void mtk_camsys_ctrl_update(struct mtk_cam_ctx *ctx, int sensor_ctrl_factor);
void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx); /* ctx_stream_off */
void mtk_cam_req_seninf_change(struct mtk_cam_request *req);
void mtk_cam_frame_done_work(struct work_struct *work);
void mtk_cam_m2m_done_work(struct work_struct *work);
void mtk_cam_meta1_done_work(struct work_struct *work);
void mtk_cam_m2m_enter_cq_state(struct mtk_camsys_ctrl_state *ctrl_state);
bool is_first_request_sync(struct mtk_cam_ctx *ctx);
void
mtk_cam_set_sensor_full(struct mtk_cam_request_stream_data *s_data,
			struct mtk_camsys_sensor_ctrl *sensor_ctrl);

void state_transition(struct mtk_camsys_ctrl_state *state_entry,
		      enum MTK_CAMSYS_STATE_IDX from,
		      enum MTK_CAMSYS_STATE_IDX to);
void
mtk_cam_set_sensor_switch(struct mtk_cam_request_stream_data *s_data,
			  struct mtk_camsys_sensor_ctrl *sensor_ctrl);

/*EXT ISP*/
void mtk_cam_event_esd_recovery(struct mtk_raw_pipeline *pipeline,
				     unsigned int frame_seq_no);

int mtk_cam_extisp_prepare_meta(struct mtk_cam_ctx *ctx, int pad_src);
void mtk_cam_extisp_sv_stream_delayed(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *camsv_dev, int seninf_padidx);
void mtk_cam_extisp_sv_stream(struct mtk_cam_ctx *ctx, bool en);
void mtk_cam_extisp_initial_sv_enque(struct mtk_cam_ctx *ctx);
void mtk_cam_extisp_sv_frame_start(struct mtk_cam_ctx *ctx,
	struct mtk_camsys_irq_info *irq_info);
int mtk_camsys_extisp_state_handle(struct mtk_raw_device *raw_dev,
	struct mtk_camsys_sensor_ctrl *s_ctrl, struct mtk_camsys_ctrl_state **state,
	struct mtk_camsys_irq_info *irq_info);
void mtk_camsys_extisp_yuv_frame_start(struct mtk_camsv_device *camsv,
	struct mtk_cam_ctx *ctx, struct mtk_camsys_irq_info *irq_info);
void mtk_camsys_extisp_raw_frame_start(struct mtk_raw_device *raw_dev,
	struct mtk_cam_ctx *ctx, struct mtk_camsys_irq_info *irq_info);
void mtk_cam_extisp_handle_sv_tstamp(struct mtk_cam_ctx *ctx,
	struct mtk_cam_request_stream_data *s_data, struct mtk_camsys_irq_info *irq_info);
int is_extisp_sv_all_frame_start(struct mtk_camsv_device *camsv,
		struct mtk_cam_ctx *ctx);
void mtk_cam_state_add_wo_sensor(struct mtk_cam_ctx *ctx);
void mtk_cam_state_del_wo_sensor(struct mtk_cam_ctx *ctx,
							struct mtk_cam_request *req);

#endif
