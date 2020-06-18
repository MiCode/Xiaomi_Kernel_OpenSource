/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_V1_X_H__
#define __MDLA_CMD_V1_X_H__

#include <linux/types.h>

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

#endif /* __MDLA_CMD_V1_X_H__ */

