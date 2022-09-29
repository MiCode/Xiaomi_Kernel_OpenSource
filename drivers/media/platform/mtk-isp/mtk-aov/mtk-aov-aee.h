/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_AOV_AEE_H
#define MTK_AOV_AEE_H

#include <linux/types.h>

#define AOV_AEE_MAX_BUFFER_SIZE   (3072)
#define AOV_AEE_MAX_RECORD_COUNT  (80)

// Forward declaration
struct mtk_aov;

struct proc_info {
	uint8_t buffer[AOV_AEE_MAX_BUFFER_SIZE];
	size_t count;
};

enum aov_op {
	SCP_READY = AOV_SCP_CMD_READY,
	AOV_START = AOV_SCP_CMD_START,
	AOV_PWR_ON = AOV_SCP_CMD_PWR_ON,
	AOV_PWR_OFF = AOV_SCP_CMD_PWR_OFF,
	AOV_FRAME = AOV_SCP_CMD_FRAME,
	AOV_NOTIFY = AOV_SCP_CMD_NOTIFY,
	AOV_STOP = AOV_SCP_CMD_STOP,
	SCP_STOP
};

struct op_data {
	uint64_t op_time;
	int op_seq;
	enum aov_op op_code;
};

struct aee_record {
	spinlock_t lock;
	int head;
	int tail;
	struct op_data data[AOV_AEE_MAX_RECORD_COUNT];
};

struct aov_aee {
	struct proc_dir_entry *entry;
	struct proc_dir_entry *kernel;
	struct aee_record record;
	struct proc_info buffer;
};

int aov_aee_init(struct mtk_aov *aov_dev);

int aov_aee_record(struct mtk_aov *aov_dev,
	int op_seq, enum aov_op op_code);

int aov_aee_flush(struct mtk_aov *aov_dev);

int aov_aee_uninit(struct mtk_aov *aov_dev);

#endif  // MTK_AOV_AEE_H
