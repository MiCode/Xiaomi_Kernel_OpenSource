/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_CORE_H__
#define __MTK_MML_PQ_CORE_H__

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "mtk-mml-core.h"

#define HDR_CURVE_NUM (1024)
#define AAL_CURVE_NUM (544)
#define AAL_HIST_NUM (768)
#define AAL_DUAL_INFO_NUM (16)
#define CMDQ_GPR_UPDATE	(2)

#define HDR_HIST_NUM (58)

#define MML_PQ_RB_ENGINE (2)
#define MAX_ENG_RB_BUF (8)
#define TOTAL_RB_BUF_NUM (MML_PQ_RB_ENGINE*MML_PIPE_CNT*MAX_ENG_RB_BUF)
#define INVALID_OFFSET_ADDR (4096*TOTAL_RB_BUF_NUM)

extern int mml_pq_msg;

#define mml_pq_msg(fmt, args...) \
do { \
	if (mml_pq_msg) \
		pr_notice("[flow]" fmt "\n", ##args); \
} while (0)

#define mml_pq_log(fmt, args...) \
	pr_notice("[flow]" fmt "\n", ##args)

#define mml_pq_err(fmt, args...) \
	pr_notice("[flow][err]" fmt "\n", ##args)

extern int mml_pq_dump;

#define mml_pq_dump(fmt, args...) \
do { \
	if (mml_pq_dump) \
		pr_notice("[param_dump]" fmt "\n", ##args); \
} while (0)

extern int mml_pq_rb_msg;

#define mml_pq_rb_msg(fmt, args...) \
do { \
	if (mml_pq_rb_msg) \
		pr_notice("[rb_flow]" fmt "\n", ##args); \
} while (0)


/* mml pq ftrace */
extern int mml_pq_trace;

#define mml_pq_trace_ex_begin(fmt, args...) do { \
	if (mml_pq_trace) \
		mml_trace_begin(fmt, ##args); \
} while (0)

#define mml_pq_trace_ex_end() do { \
	if (mml_pq_trace) \
		mml_trace_end(); \
} while (0)

struct mml_task;

enum mml_pq_vcp_engine {
	MML_PQ_HDR0 = 0,
	MML_PQ_HDR1,
	MML_PQ_AAL0,
	MML_PQ_AAL1,
};

enum mml_pq_readback_engine {
	MML_PQ_HDR = 0,
	MML_PQ_AAL,
	MML_PQ_DC,
};

struct mml_pq_readback_buffer {
	dma_addr_t pa;
	u32 *va;
	u32 va_offset;
	struct list_head buffer_list;
};

struct mml_pq_readback_data {
	bool is_dual;
	atomic_t pipe_cnt;
	u32 cut_pos_x;
	u32 *pipe0_hist;
	u32 *pipe1_hist;
};

struct mml_pq_frame_data {
	struct mml_frame_info info;
	struct mml_pq_param pq_param[MML_MAX_OUTPUTS];
	struct mml_frame_size frame_out[MML_MAX_OUTPUTS];
};

struct mml_pq_sub_task {
	struct mutex lock;
	atomic_t queued;
	void *result;
	atomic_t result_ref;
	struct mml_pq_readback_data readback_data;
	struct mml_pq_frame_data frame_data;
	struct wait_queue_head wq;
	struct list_head mbox_list;
	bool job_cancelled;
	bool first_job;
	u32 mml_task_jobid;
	u64 job_id;
};

struct mml_pq_task {
	struct mml_task *task;
	struct mutex buffer_mutex;
	struct mml_pq_readback_buffer *aal_hist[MML_PIPE_CNT];
	struct mml_pq_readback_buffer *hdr_hist[MML_PIPE_CNT];
	struct kref ref;
	struct mml_pq_sub_task tile_init;
	struct mml_pq_sub_task comp_config;
	struct mml_pq_sub_task aal_readback;
	struct mml_pq_sub_task hdr_readback;
	struct mml_pq_sub_task rsz_callback;
};

/*
 * mml_pq_core_init - Initialize PQ core
 *
 * Return:	Error of initialization
 */
int mml_pq_core_init(void);

/*
 * mml_pq_core_uninit - Uninitialize PQ core
 */
void mml_pq_core_uninit(void);

/*
 * mml_pq_task_create - create and initial pq task
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, create pq task failed should debug
 */
s32 mml_pq_task_create(struct mml_task *task);

/*
 * mml_pq_task_release - for adaptor to call before destroy mml_task
 *
 * @task:	task data, include pq_task inside
 */
void mml_pq_task_release(struct mml_task *task);

/*
 * mml_pq_get_vcp_buf_offset - get vcp readback buffer offset
 *
 * @task:	task data, include pq_task inside
 * @engine:	module id, engine
 * @hist	hist id, readback hist
 */
void mml_pq_get_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist);

/*
 * mml_pq_put_vcp_buf_offset - put vcp readback buffer offset
 *
 * @task:	task data, include pq_task inside
 * @engine:	module id, know engine id
 * @hist	hist id, readback hist
 */
void mml_pq_put_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist);

/*
 * mml_pq_get_readback_buffer - get readback buffer
 *
 * @task:	task data, include pq_task inside
 * @pipe:	pipe id, use in dual pipe
 * @engine	engine id, readback engine
 */
void mml_pq_get_readback_buffer(struct mml_task *task, u8 pipe,
				struct mml_pq_readback_buffer **hist);

/*
 * mml_pq_get_readback_buffer - put readback buffer
 *
 * @task:	task data, include pq_task inside
 * @engine	engine id, readback engine
 */
void mml_pq_put_readback_buffer(struct mml_task *task, u8 pipe,
				struct mml_pq_readback_buffer *hist);

/*
 * mml_pq_set_tile_init - noify from MML core through MML PQ driver
 *	to update frame information for tile start
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_set_tile_init(struct mml_task *task);

/*
 * mml_pq_get_tile_init_result - wait for result
 *
 * @task:	task data, include pq parameters and frame info
 * @timeout_ms:	timeout setting to get result, unit: ms
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms);

/*
 * mml_pq_put_tile_init_result - put away result
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_put_tile_init_result(struct mml_task *task);

/*
 * mml_pq_set_comp_config - noify from MML core through MML PQ driver
 *	to update frame information for frame config
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_set_comp_config(struct mml_task *task);

/*
 * mml_pq_get_comp_config_result - wait for result
 *
 * @task:	task data, include pq parameters and frame info
 * @timeout_ms:	timeout setting to get result, unit: ms
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_get_comp_config_result(struct mml_task *task, u32 timeout_ms);

/*
 * mml_pq_put_comp_config_result - put away result
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_put_comp_config_result(struct mml_task *task);

/*
 * mml_pq_sub_task_clear - remove invalid sub_task from list
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_comp_config_clear(struct mml_task *task);

/*
 * mml_pq_aal_readback - noify from MML core through MML PQ driver
 *	to update histogram
 *
 * @task:	task data, include pq parameters and frame info
 * @pipe:	pipe id
 * @phist:	Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_aal_readback(struct mml_task *task, u8 pipe, u32 *phist);

/*
 * mml_pq_hdr_readback - noify from MML core through MML PQ driver
 *   to update histogram
 *
 * @task:	task data, include pq parameters and frame info
 * @pipe:   pipe id
 * @phist:  Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

int mml_pq_hdr_readback(struct mml_task *task, u8 pipe, u32 *phist);


/*
 * mml_pq_rsz_callback - noify from MML core through MML PQ driver
 *   that second output finished
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

int mml_pq_rsz_callback(struct mml_task *task);
#endif	/* __MTK_MML_PQ_CORE_H__ */
