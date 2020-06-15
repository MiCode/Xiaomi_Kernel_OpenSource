/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_V2_0_H__
#define __MDLA_CMD_V2_0_H__

#include <linux/types.h>

struct mdla_wait_cmd {
	__u32 id;              /* [in] command id */
	int  result;           /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};

struct mdla_run_cmd {
	uint32_t offset_code_buf;
	uint32_t reserved;
	uint32_t size;
	uint32_t mva;
	__u32 offset;        /* [in] command byte offset in buf */
	__u32 count;         /* [in] # of commands */
	__u32 id;            /* [out] command id */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
	__u32 mdla_id;
};

struct mdla_wait_entry {
	u32 async_id;
	struct list_head list;
	struct mdla_wait_cmd wt;
};

#endif /* __MDLA_CMD_V2_0_H__ */

