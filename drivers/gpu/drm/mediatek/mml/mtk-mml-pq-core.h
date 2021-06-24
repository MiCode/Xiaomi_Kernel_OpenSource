/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_CORE_H__
#define __MTK_MML_PQ_CORE_H__

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>

struct mml_task;

struct mml_pq_sub_task {
	struct mutex lock;
	void *result;
	struct wait_queue_head wq;
	struct list_head mbox_list;
	bool job_cancelled;
	u64 job_id;
	bool inited;
};

struct mml_pq_task {
	struct mml_task *task;
	atomic_t ref_cnt;
	struct mutex lock;
	struct mml_pq_sub_task tile_init;
};

/*
 * mml_pq_core_init - Initialize PQ core
 */
void mml_pq_core_init(void);

/*
 * destroy_pq_task - for adaptor to call before destroy mml_task
 *
 * @task:	task data, include pq_task inside
 */
void destroy_pq_task(struct mml_task *task);

/*
 * mml_pq_tile_inti - noify from MML core through MML PQ driver
 *   to update frame information for tile start
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug 
 */
int mml_pq_tile_init(struct mml_task *task);

/*
 * mml_pq_get_tile_init_result - wait for result and update data
 *
 * @task:	task data, include pq parameters and frame info
 * @timeout_ms:	timeout setting to get result, unit: ms
 *
 * Return:	if value < 0, means PQ update failed should debug 
 */
int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms);

#endif	/* __MTK_MML_PQ_CORE_H__ */
