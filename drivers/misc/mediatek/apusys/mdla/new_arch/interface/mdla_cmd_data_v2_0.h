/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_DATA_V2_0_H__
#define __MDLA_CMD_DATA_V2_0_H__

#include <linux/types.h>
#include <linux/list.h>

struct command_entry;

struct mdla_wait_cmd {
	uint32_t id;           /* [in] command id */
	int32_t  result;       /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};

struct mdla_run_cmd {
	uint32_t offset_code_buf;
	uint32_t reserved;
	uint32_t size;
	uint32_t mva;
	uint32_t offset;        /* [in] command byte offset in buf */
	uint32_t count;         /* [in] # of commands */
	uint32_t id;            /* [out] command id */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
	uint32_t mdla_id;
};

struct mdla_wait_entry {
	uint32_t async_id;
	struct list_head list;
	struct mdla_wait_cmd wt;
};

struct command_batch {
	struct list_head node;
	u32 index;
	u32 size;
};

struct sched_smp_ce {
	spinlock_t lock;
	u64 deadline;
};

struct mdla_scheduler {
	struct list_head ce_list[PRIORITY_LEVEL];
	struct command_entry *ce[PRIORITY_LEVEL];
	struct command_entry *pro_ce;

	spinlock_t lock;

	void (*sw_reset)(u32 core_id);
	void (*enqueue_ce)(u32 core_id, struct command_entry *ce, u32 resume);
	struct command_entry* (*dequeue_ce)(u32 core_id);
	void (*issue_ce)(u32 core_id);
	void (*issue_dual_lowce)(u32 core_id, uint64_t dual_cmd_id);
	int (*process_ce)(u32 core_id);
	void (*preempt_ce)(u32 core_id, struct command_entry *high_ce);
	void (*stop_ce)(u32 core_id, struct command_entry *ce);
	void (*complete_ce)(u32 core_id);
	u64 (*get_smp_deadline)(int priority);
	void (*set_smp_deadline)(int priority, u64 deadline);
};


#endif /* __MDLA_CMD_DATA_V2_0_H__ */

