/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_DATA_V1_0_H__
#define __MDLA_CMD_DATA_V1_0_H__

#include <linux/types.h>

#define USE_IOCTL_STRUCT 1

#if USE_IOCTL_STRUCT /* same as struct ioctl_xxx */

#define get_code_buf_kva(apusys_hd, run_cmd)\
			((run_cmd)->kva + (run_cmd)->offset)

struct mdla_run_cmd {
	struct {
		uint32_t size;
		uint32_t mva;
		void *pa;
		void *kva;
		uint32_t u_id;
		uint8_t type;
		void *data;

		int ion_share_fd;
		int ion_handle;       /* user space handle */
		uint64_t ion_khandle; /* kernel space handle */
	};

	uint32_t offset;           /* [in] command byte offset in buf */
	uint32_t count;            /* [in] # of commands */
	uint32_t id;               /* [out] command id */
	uint8_t priority;          /* [in] dvfs priority */
	uint8_t boost_value;       /* [in] dvfs boost value */
};

struct mdla_wait_cmd {
	uint32_t id;           /* [in] command id */
	int32_t result;        /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
};

struct mdla_wait_entry {
	uint32_t async_id;
	struct list_head list;
	struct mdla_wait_cmd wt;
};

#else /* use mt6885 struct definition */

#define get_code_buf_kva(apusys_hd, run_cmd)\
			((apusys_hd)->cmd_entry + (run_cmd)->offset_code_buf)

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

#endif /* USE_IOCTL_STRUCT */

#endif /* __MDLA_CMD_DATA_V1_0_H__ */

