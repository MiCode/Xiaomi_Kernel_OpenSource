/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_APU_IOCTL_H__
#define __MTK_APU_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define APUSYS_MAGICNO 'A'

enum mdw_hs_ioctl_op {
	MDW_HS_IOCTL_OP_BASIC,
	MDW_HS_IOCTL_OP_DEV,
	MDW_HS_IOCTL_OP_MEM,
};

struct mdw_hs_in {
	uint32_t op; //enum mdw_hs_ioctl_op
	uint64_t flags;
	union {
		struct {
			uint32_t type;
		} dev;
		struct {
			uint32_t type;
		} mem;
	};
};

#define MDW_DEV_META_SIZE (32)

struct mdw_hs_out {
	union {
		struct {
			uint64_t version;
			uint64_t dev_bitmask;
			uint64_t mem_bitmask;
			uint64_t flags;
			uint32_t meta_size;
			uint32_t reserved;
		} basic;

		struct {
			uint32_t type;
			uint32_t num;
			char meta[MDW_DEV_META_SIZE];
		} dev;

		struct {
			uint32_t type;
			uint32_t reserved;
			uint64_t start;
			uint32_t size;
		} mem;
	};
};

union mdw_hs_args {
	struct mdw_hs_in in;
	struct mdw_hs_out out;
};

#define APU_MDW_IOCTL_HANDSHAKE \
	_IOWR(APUSYS_MAGICNO, 32, union mdw_hs_args)

enum mdw_mem_ioctl_op {
	MDW_MEM_IOCTL_ALLOC,
	MDW_MEM_IOCTL_FREE,
	MDW_MEM_IOCTL_MAP,
	MDW_MEM_IOCTL_UNMAP,
	MDW_MEM_IOCTL_FLUSH,
	MDW_MEM_IOCTL_INVALIDATE,
};

enum mdw_mem_flag {
	MDW_MEM_FLAG_CACHEABLE = 0,
	MDW_MEM_FLAG_32BIT = 1,
	MDW_MEM_FLAG_HIGHADDR = 2,
};
#define F_MDW_MEM_CACHEABLE (1ULL << MDW_MEM_FLAG_CACHEABLE)
#define F_MDW_MEM_32BIT (1ULL << MDW_MEM_FLAG_32BIT)
#define F_MDW_MEM_HIGHADDR (1ULL << MDW_MEM_FLAG_HIGHADDR)

enum mdw_mem_type {
	MDW_MEM_TYPE_MAIN,
	MDW_MEM_TYPE_VLM,
	MDW_MEM_TYPE_LOCAL,
	MDW_MEM_TYPE_SYSTEM,
	MDW_MEM_TYPE_SYSTEM_ISP,
	MDW_MEM_TYPE_SYSTEM_APU,

	MDW_MEM_TYPE_MAX,
};

struct mdw_mem_in {
	uint32_t op; //enum mdw_mem_ioctl_op
	uint64_t flags;
	union {
		/* alloc */
		struct {
			uint32_t type; //enum mdw_mem_type
			uint32_t size;
			uint32_t align;
			uint64_t flags;
		} alloc;
		struct {
			uint64_t handle;
		} free;

		/* map */
		struct {
			uint64_t handle;
			uint32_t size;
		} map;
		struct {
			uint64_t handle;
		} unmap;

		/* cache operation */
		struct {
			uint64_t handle;
			uint32_t offset;
			uint32_t size;
		} flush;
		struct {
			uint64_t handle;
			uint32_t offset;
			uint32_t size;
		} invalidate;
	};
};

struct mdw_mem_out {
	union {
		struct {
			uint64_t handle;
		} alloc;
		struct {
			uint32_t type;
			uint64_t device_va;
		} map;
		struct {
			uint64_t device_va;
			uint32_t size;
		} import;
	};
};

union mdw_mem_args {
	struct mdw_mem_in in;
	struct mdw_mem_out out;
};
#define APU_MDW_IOCTL_MEM \
	_IOWR(APUSYS_MAGICNO, 33, union mdw_mem_args)

enum mdw_cmd_ioctl_op {
	MDW_CMD_IOCTL_RUN,
};

enum {
	/* cmdbuf copy in before execution and copy out after exection */
	MDW_CB_BIDIRECTIONAL,
	/* cmdbuf copy in before execution */
	MDW_CB_IN,
	/* cmdbuf copy out after execution */
	MDW_CB_OUT,
};

struct mdw_subcmd_exec_info {
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t ip_start_ts;
	uint32_t ip_end_ts;
	uint32_t bw;
	uint32_t boost;
	uint32_t tcm_usage;
	int32_t ret;
};

struct mdw_cmd_exec_info {
	uint64_t sc_rets;
	int64_t ret;
	uint64_t total_us;
	uint64_t reserved;
};

struct mdw_subcmd_cmdbuf {
	uint64_t handle;
	uint32_t size;
	uint32_t align;
	uint32_t direction;
};

struct mdw_subcmd_info {
	uint32_t type;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t turbo_boost;
	uint32_t min_boost;
	uint32_t max_boost;
	uint32_t hse_en;
	uint32_t pack_id;
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;

	/* cmdbufs */
	uint32_t num_cmdbufs;
	uint64_t cmdbufs;
};

struct mdw_cmd_in {
	uint32_t op; //enum mdw_cmd_ioctl_op
	union {
		struct {
			uint64_t usr_id;
			uint64_t uid;
			uint32_t priority;
			uint32_t hardlimit;
			uint32_t softlimit;
			uint32_t power_save;
			uint32_t power_plcy;
			uint32_t power_dtime;
			uint32_t app_type;
			uint32_t flags;
			uint32_t num_subcmds;
			uint64_t subcmd_infos;
			uint64_t adj_matrix;
			uint64_t fence;
			uint64_t exec_infos;
		} exec;
	};
};

struct mdw_cmd_out {
	union {
		struct {
			uint64_t id;
			uint64_t fence;
		} exec;
	};
};

union mdw_cmd_args {
	struct mdw_cmd_in in;
	struct mdw_cmd_out out;
};

#define APU_MDW_IOCTL_CMD \
	_IOWR(APUSYS_MAGICNO, 34, union mdw_cmd_args)

enum mdw_util_ioctl_op {
	MDW_UTIL_IOCTL_SETPOWER,
	MDW_UTIL_IOCTL_UCMD,
};

struct mdw_util_in {
	uint32_t op; //enum mdw_util_ioctl_op
	union {
		struct {
			uint32_t dev_type;
			uint32_t core_idx;
			uint32_t boost;
			uint64_t reserve;
		} power;
		struct {
			uint32_t dev_type;
			uint32_t size;
			uint64_t handle;
		} ucmd;
	};
};

union mdw_util_args {
	struct mdw_util_in in;
};

#define APU_MDW_IOCTL_UTIL \
	_IOWR(APUSYS_MAGICNO, 35, union mdw_util_args)

#endif
