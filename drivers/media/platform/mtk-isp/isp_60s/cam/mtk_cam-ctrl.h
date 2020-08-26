/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_CTRL_H
#define __MTK_CAM_CTRL_H

#include <linux/hrtimer.h>
#include <linux/timer.h>

#define MAX_STREAM_NUM 4 /* rawx2, mrawx2 */
struct mtk_cam_device;
struct mtk_raw_device;

#define MTK_CAMSYS_ENGINE_IDXMASK    0xF0
#define MTK_CAMSYS_ENGINE_RAW_TAG    0x10
#define MTK_CAMSYS_ENGINE_MRAW_TAG   0x20
#define MTK_CAMSYS_ENGINE_CAMSV_TAG  0x30

enum MTK_CAMSYS_ENGINE_IDX {
	CAMSYS_ENGINE_RAW_BEGIN = MTK_CAMSYS_ENGINE_RAW_TAG,
	CAMSYS_ENGINE_RAW_A = CAMSYS_ENGINE_RAW_BEGIN,
	CAMSYS_ENGINE_RAW_B,
	CAMSYS_ENGINE_RAW_C,// will be removed on isp7
	CAMSYS_ENGINE_RAW_END,
	CAMSYS_ENGINE_MRAW_BEGIN = MTK_CAMSYS_ENGINE_MRAW_TAG,
	CAMSYS_ENGINE_MRAW_A = CAMSYS_ENGINE_RAW_BEGIN,
	CAMSYS_ENGINE_MRAW_B,
	CAMSYS_ENGINE_MRAW_END,
	CAMSYS_ENGINE_CAMSV_BEGIN = MTK_CAMSYS_ENGINE_CAMSV_TAG,
	CAMSYS_ENGINE_CAMSV_A = CAMSYS_ENGINE_CAMSV_BEGIN,
	CAMSYS_ENGINE_CAMSV_B,
	CAMSYS_ENGINE_CAMSV_END,
};

enum MTK_CAMSYS_IRQ_EVENT {
	CAMSYS_IRQ_SETTING_DONE = 0,
	CAMSYS_IRQ_FRAME_START,
	CAMSYS_IRQ_FRAME_DONE,
};

struct mtk_camsys_irq_info {
	enum MTK_CAMSYS_IRQ_EVENT irq_type;
	enum MTK_CAMSYS_ENGINE_IDX engine_id;
	int frame_idx;
	int frame_inner_idx;
};

/*For state analysis and controlling for request*/
enum MTK_CAMSYS_STATE_IDX {
	E_STATE_READY,
	E_STATE_SENSOR,
	E_STATE_CQ,
	E_STATE_OUTER,
	E_STATE_INNER,
	E_STATE_DONE_NORMAL,
	E_STATE_CQ_SCQ_DELAY,
	E_STATE_OUTER_HW_DELAY,
	E_STATE_INNER_HW_DELAY,
	E_STATE_DONE_MISMATCH,
};

struct mtk_camsys_ctrl_state {
	enum MTK_CAMSYS_STATE_IDX estate;
	struct list_head state_element;
};

#define ISP_CLK_LEVEL_CNT 10

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
	struct list_head camsys_state_list;
	spinlock_t camsys_state_lock;
	/* link change ctrl */
	struct mtk_camsys_link_ctrl link_ctrl;
	struct mtk_cam_request *link_change_req;
	u8 link_change_state;
	spinlock_t link_change_lock;
};

enum {
	LINK_CHANGE_IDLE,
	LINK_CHANGE_PREPARING,
	LINK_CHANGE_QUEUED,
};


struct mtk_camsys_dvfs {
	struct device *dev;
	struct regulator *reg;
	unsigned int clklv_num;
	unsigned int clklv[ISP_CLK_LEVEL_CNT];
	unsigned int voltlv[ISP_CLK_LEVEL_CNT];
	unsigned int clklv_idx;
	unsigned int clklv_target;
};

struct mtk_camsys_ctrl {
	/* per stream (ctx) */
	struct mtk_camsys_sensor_ctrl sensor_ctrl[MAX_STREAM_NUM];
	struct mtk_raw_device *raw_dev[CAMSYS_ENGINE_RAW_END -
		CAMSYS_ENGINE_RAW_BEGIN]; /* per hw */
	/**
	 * per hw:
	 * struct mtk_mraw_device *
	 *	mraw_dev[CAMSYS_ENGINE_MRAW_END-CAMSYS_ENGINE_MRAW_BEGIN];
	 * struct mtk_camsv_device *
	 *	camsv_dev[CAMSYS_ENGINE_CAMSV_END-CAMSYS_ENGINE_CAMSV_BEGIN];
	 */
	/* resource ctrl */
	struct mtk_camsys_dvfs dvfs_info;

};

void mtk_cam_dvfs_init(struct mtk_cam_device *cam);
void mtk_cam_dvfs_uninit(struct mtk_cam_device *cam);


int mtk_camsys_isr_event(struct mtk_cam_device *cam,
			 struct mtk_camsys_irq_info *irq_info);
void mtk_cam_initial_sensor_setup(struct mtk_cam_request *req,
				  struct mtk_cam_ctx *ctx);
int mtk_camsys_ctrl_start(struct mtk_cam_ctx *ctx); /* ctx_stream_on */
void mtk_camsys_ctrl_stop(struct mtk_cam_ctx *ctx); /* ctx_stream_off */

#endif
