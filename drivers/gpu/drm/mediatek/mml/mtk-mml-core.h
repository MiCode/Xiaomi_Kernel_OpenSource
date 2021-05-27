/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */


#ifndef __MTK_MML_CORE_H__
#define __MTK_MML_CORE_H__

#include <linux/file.h>
#include <linux/list.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/mailbox_client.h>
#include <linux/types.h>
#include <linux/time.h>

#include "mtk-mml.h"
#include "mtk-mml-driver.h"

extern int mtk_mml_msg;
#define mml_msg(fmt, args...) \
do { \
	if (mtk_mml_msg) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_log(fmt, args...) \
do { \
	pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_err(fmt, args...) \
do { \
	pr_notice("[mml][err]" fmt "\n", ##args); \
} while (0)

#define MML_PIPE_CNT		2
#define MML_MAX_PATH_ENGINES	10

struct mml_task;

struct mml_task_ops {
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
};

struct mml_cap {
	enum mml_mode target;
	enum mml_mode running;
};

struct mml_frame_config {
	struct list_head entry;
	struct mml_frame_info info;
	struct mutex task_mutex;
	struct list_head tasks;
	struct list_head done_tasks;

	/* platform driver */
	struct mml_dev *mml;

	/* drm adaptor */
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;
};

struct mml_file_buf {
	struct file *f[MML_MAX_PLANES];
	u32 size[MML_MAX_PLANES];
	u8 cnt;
	struct file *fence;
	u32 usage;
};

struct mml_task_buffer {
	struct mml_file_buf src;
	struct mml_file_buf dest[MML_MAX_OUTPUTS];
	u8 dest_cnt;
};

enum mml_task_state {
	MML_TASK_INITIAL,
	MML_TASK_REUSE,
	MML_TASK_RUNNING,
	MML_TASK_IDLE
};

struct mml_task {
	struct list_head entry;
	struct mml_job job;
	struct mml_frame_config *config;
	struct mml_task_buffer buf;
	struct timespec64 end_time;
	struct file *fence;
	enum mml_task_state state;

	/* mml context */
	void *ctx;

	/* command */
	struct cmdq_pkt pkts[MML_PIPE_CNT];
};

struct mml_comp_tile_ops {

};

struct mml_comp_config_ops {

};

struct mml_comp_hw_ops {

};

struct mml_comp_debug_ops {

};

struct mml_comp {
	u32 comp_id;
	const struct mml_comp_tile_ops *tile_ops;
	const struct mml_comp_config_ops *config_ops;
	const struct mml_comp_hw_ops *hw_ops;
	const struct mml_comp_debug_ops *debug_ops;
};

/**
 * mml_core_create_task -
 *
 * Return:
 */
struct mml_task *mml_core_create_task(void);

/**
 * mml_core_destroy_task -
 * @task:
 *
 */
void mml_core_destroy_task(struct mml_task *task);

/**
 * mml_core_submit_task -
 * @frame_config:
 * @task:
 *
 * Return:
 */
s32 mml_core_submit_task(struct mml_frame_config *frame_config,
			 struct mml_task *task);


#endif	/* __MTK_MML_CORE_H__ */
