/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_CTRL_H
#define __MTK_CAM_CTRL_H

#include <linux/hrtimer.h>
#include <linux/timer.h>
#include "mtk_cam-dvfs_qos.h"

#define MAX_STREAM_NUM 12 /* rawx2, mrawx4 camsvx6 */
struct mtk_cam_device;
struct mtk_raw_device;

#define MTK_CAMSYS_ENGINE_IDXMASK    0xF0
#define MTK_CAMSYS_ENGINE_RAW_TAG    0x10
#define MTK_CAMSYS_ENGINE_MRAW_TAG   0x20
#define MTK_CAMSYS_ENGINE_CAMSV_TAG  0x30
#define MTK_CAMSYS_ENGINE_SENINF_TAG	0x40

enum MTK_CAMSYS_ENGINE_IDX {
	CAMSYS_ENGINE_RAW_BEGIN = MTK_CAMSYS_ENGINE_RAW_TAG,
	CAMSYS_ENGINE_RAW_A = CAMSYS_ENGINE_RAW_BEGIN,
	CAMSYS_ENGINE_RAW_B,
	CAMSYS_ENGINE_RAW_END,
	CAMSYS_ENGINE_MRAW_BEGIN = MTK_CAMSYS_ENGINE_MRAW_TAG,
	CAMSYS_ENGINE_MRAW_0 = CAMSYS_ENGINE_MRAW_BEGIN,
	CAMSYS_ENGINE_MRAW_1,
	CAMSYS_ENGINE_MRAW_END,
	CAMSYS_ENGINE_CAMSV_BEGIN = MTK_CAMSYS_ENGINE_CAMSV_TAG,
	CAMSYS_ENGINE_CAMSV_0 = CAMSYS_ENGINE_CAMSV_BEGIN,
	CAMSYS_ENGINE_CAMSV_1,
	CAMSYS_ENGINE_CAMSV_2,
	CAMSYS_ENGINE_CAMSV_3,
	CAMSYS_ENGINE_CAMSV_4,
	CAMSYS_ENGINE_CAMSV_5,
	CAMSYS_ENGINE_CAMSV_END,
	CAMSYS_ENGINE_SENINF_BEGIN = MTK_CAMSYS_ENGINE_SENINF_TAG,
	CAMSYS_ENGINE_SENINF = CAMSYS_ENGINE_SENINF_BEGIN,
	CAMSYS_ENGINE_SENINF_END,
};

enum MTK_CAMSYS_IRQ_EVENT {
	CAMSYS_IRQ_SETTING_DONE = 0,
	CAMSYS_IRQ_FRAME_START,
	CAMSYS_IRQ_FRAME_DONE,
	CAMSYS_IRQ_SUBSAMPLE_SENSOR_SET,
	CAMSYS_IRQ_FRAME_DROP,
};

struct mtk_camsys_irq_info {
	enum MTK_CAMSYS_IRQ_EVENT irq_type;
	enum MTK_CAMSYS_ENGINE_IDX engine_id;
	int frame_idx;
	int frame_inner_idx;
	bool slave_engine;
};

/*For state analysis and controlling for request*/
enum MTK_CAMSYS_STATE_IDX {
	E_STATE_READY = 0x0,
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
};

struct mtk_camsys_ctrl_state {
	enum MTK_CAMSYS_STATE_IDX estate;
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;
	struct list_head state_element;
	u64 time_composing;
	u64 time_swirq_composed;
	u64 time_swirq_timer;
	u64 time_sensorset;
	u64 time_irq_sof1;
	u64 time_cqset;
	u64 time_irq_outer;
	u64 time_irq_sof2;
	u64 time_irq_done;
	u64 time_deque;
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
	struct workqueue_struct *sensorsetting_wq;
	struct hrtimer sensor_deadline_timer;
	u64 sof_time;
	int timer_req_sensor;
	int timer_req_event;
	int sensor_request_seq_no;
	int isp_request_seq_no;
	int initial_cq_done;
	int initial_drop_frame_cnt;
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
};

struct mtk_camsys_ctrl {
	struct mtk_raw_device *raw_dev[CAMSYS_ENGINE_RAW_END -
		CAMSYS_ENGINE_RAW_BEGIN]; /* per hw */
	struct mtk_camsv_device *camsv_dev[CAMSYS_ENGINE_CAMSV_END -
		CAMSYS_ENGINE_CAMSV_BEGIN]; /* per hw */
	/* resource ctrl */
	struct mtk_camsys_dvfs dvfs_info;
};
void mtk_camsys_state_delete(struct mtk_cam_ctx *ctx,
				struct mtk_camsys_sensor_ctrl *sensor_ctrl,
				struct mtk_cam_request *req);
void mtk_camsys_frame_done(struct mtk_cam_ctx *ctx,
				  unsigned int frame_seq_no,
				  unsigned int pipe_id);

int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 struct mtk_camsys_irq_info *irq_info);
void mtk_cam_initial_sensor_setup(struct mtk_cam_request *req,
					struct mtk_cam_ctx *ctx);
void mtk_cam_req_ctrl_setup(struct mtk_cam_ctx *ctx,
					struct mtk_cam_request *req);
int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx); /* ctx_stream_on */
void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx); /* ctx_stream_off */
void mtk_cam_frame_done_work(struct work_struct *work);
void mtk_cam_m2m_enter_cq_state(struct mtk_camsys_ctrl_state *ctrl_state);

#endif
