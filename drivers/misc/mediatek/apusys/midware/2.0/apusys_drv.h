/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APUSYS_DRV_H__
#define __APUSYS_DRV_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define APUSYS_MAGICNO 'A'

enum mdw_hs_ioctl_op {
	MDW_HS_IOCTL_OP_BASIC,
	MDW_HS_IOCTL_OP_DEV,
};

struct mdw_hs_in {
	enum mdw_hs_ioctl_op op;
	uint64_t flags;
	union {
		struct {
			uint32_t type;
		} dev;
	};
};

#define MDW_DEV_META_SIZE (32)

struct mdw_hs_out {
	union {
		struct {
			uint64_t version;
			uint64_t dev_bitmask;
			uint64_t flags;
			uint32_t meta_size;
			uint64_t vlm_start;
			uint32_t vlm_size;
		} basic;

		struct {
			uint32_t type;
			uint32_t num;
			char meta[MDW_DEV_META_SIZE];
		} dev;
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

enum MDW_MEM_IOCTL_ALLOC_BITMASK {
	MDW_MEM_IOCTL_ALLOC_CACHEABLE,
	MDW_MEM_IOCTL_ALLOC_32BIT,
};

struct mdw_mem_in {
	enum mdw_mem_ioctl_op op;
	uint64_t flags;
	union {
		/* alloc */
		struct {
			uint32_t size;
			uint32_t align;
			uint64_t flags; //enum MDW_MEM_IOCTL_ALLOC_BITMASK
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
		} flush;
		struct {
			uint64_t handle;
		} invalidate;
	};
};

struct mdw_mem_out {
	union {
		struct {
			uint64_t handle;
		} alloc;
		struct {
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
	MDW_CMD_IOCTL_BUILD,
	MDW_CMD_IOCTL_RELEASE,
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

struct mdw_subcmd_info {
	uint32_t type;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t pack_id;

	/* cmdbufs */
	uint32_t num_cmdbufs;
	uint64_t cmdbufs;

	/* out */
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t ip_start_ts;
	uint32_t ip_end_ts;
	uint32_t bw;
};

struct mdw_subcmd_cmdbuf {
	uint64_t handle;
	uint32_t size;
	uint32_t align;
	uint32_t direction;
};

struct mdw_cmd_in {
	enum mdw_cmd_ioctl_op op;
	union {
		struct {
			uint64_t usr_id;
			uint64_t uid;
			uint32_t priority;
			uint32_t hardlimit;
			uint32_t softlimit;
			uint32_t power_save;
			uint32_t flags;
			uint32_t num_subcmds;
			uint64_t subcmd_infos;
			uint64_t adj_matrix;
		} build;

		struct {
			uint64_t id;
		} release;

		struct {
			uint64_t id;
			uint32_t priority;
			uint32_t hardlimit;
			uint32_t softlimit;
			uint32_t power_save;
			uint64_t fence;
		} exec;
	};
};

struct mdw_cmd_out {
	union {
		struct {
			uint64_t id;
		} build;

		struct {
			/* fence fd */
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
	enum mdw_util_ioctl_op op;
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
