/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __APUSYS_DRV_H__
#define __APUSYS_DRV_H__

#include <linux/ioctl.h>
#include <linux/types.h>

/* for APUSYS_IOCTL_HANDSHAKE */
enum {
	APUSYS_HANDSHAKE_NONE,
	APUSYS_HANDSHAKE_BEGIN,
	APUSYS_HANDSHAKE_QUERY_DEV,

	APUSYS_HANDSHAKE_RESERVE0,
	APUSYS_HANDSHAKE_RESERVE1,

	APUSYS_HANDSHAKE_MAX,
};

struct hs_begin {
	unsigned long long dev_support; // bitmap
	unsigned int dev_type_max;

	unsigned int mem_support; // bitmap
	unsigned int vlm_start;
	unsigned int vlm_size;
};

struct hs_query_dev {
	unsigned int type;
	unsigned int num;
};

struct apusys_ioctl_hs {
	unsigned long long magic_num;
	unsigned char cmd_version;

	int type;

	union{
		struct hs_begin begin;
		struct hs_query_dev dev;
	};
};

/* for APUSYS_IOCTL_MEM */
enum {
	APUSYS_NONE,
	APUSYS_CACHE,
	APUSYS_MAX,
};

enum {
	APUSYS_CACHE_NONE,
	APUSYS_CACHE_SYNC,
	APUSYS_CACHE_INVALIDATE,
	APUSYS_CACHE_MAX,
};

enum {
	APUSYS_MEM_DRAM_ION,
	APUSYS_MEM_DRAM_DMA,
	APUSYS_MEM_VLM,
	APUSYS_MEM_DRAM_ION_AOSP,

	APUSYS_MEM_MAX,
};

struct apusys_cache_param {
	int cache_type;
};

struct apusys_mem_ctl {
	int cmd;
	union {
		struct apusys_cache_param cache_param;
	};
};

struct apusys_mem {
	unsigned long long uva;
	unsigned int iova;
	unsigned int size;
	unsigned int iova_size;

	unsigned int align;
	unsigned int cache;

	int mem_type;
	int fd;
	unsigned long long khandle;

	struct apusys_mem_ctl ctl_data;
};

/* for APUSYS_IOCTL_RUN_CMD_SYNC */
struct apusys_ioctl_cmd {
	unsigned long long cmd_id;

	int mem_fd; //  memory buffer fd
	unsigned int offset;
	unsigned int size;
};

/* for APUSYS_IOCTL_SET_POWER */
struct apusys_ioctl_power {
	int dev_type;
	unsigned int idx;
	unsigned int boost_val;
};

/* for APUSYS_IOCTL_DEVICE */
struct apusys_ioctl_dev {
	int dev_type;
	unsigned int num;

	int dev_idx;

	unsigned long long handle;
};

/* for APUSYS_IOCTL_FW */
struct apusys_ioctl_fw {
	unsigned int magic;
	int dev_type;
	int idx;

	char name[32];

	int mem_fd;
	unsigned int offset;
	unsigned int size;
};

/* for APUSYS_IOCTL_USER_CMD */
struct apusys_ioctl_ucmd {
	int dev_type;
	int idx;
	int mem_fd; //  memory buffer fd
	unsigned int offset;
	unsigned int size;
};

/* for APUSYS_IOCTL_SEC_DEVICE */
struct apusys_ioctl_sec {
	int dev_type;
	unsigned int core_num;

	unsigned int reserved0;
	unsigned int reserved1;
	unsigned int reserved2;
	unsigned int reserved3;
	unsigned int reserved4;
	unsigned int reserved5;
};

#define APUSYS_MAGICNO 'A'
#define APUSYS_IOCTL_HANDSHAKE \
	_IOWR(APUSYS_MAGICNO, 0, struct apusys_ioctl_hs)
#define APUSYS_IOCTL_MEM_ALLOC \
	_IOWR(APUSYS_MAGICNO, 1, struct apusys_mem)
#define APUSYS_IOCTL_MEM_FREE \
	_IOWR(APUSYS_MAGICNO, 2, struct apusys_mem)
#define APUSYS_IOCTL_MEM_IMPORT \
	_IOWR(APUSYS_MAGICNO, 3, struct apusys_mem)
#define APUSYS_IOCTL_MEM_UNIMPORT \
	_IOWR(APUSYS_MAGICNO, 4, struct apusys_mem)
#define APUSYS_IOCTL_MEM_CTL \
	_IOWR(APUSYS_MAGICNO, 5, struct apusys_mem)
#define APUSYS_IOCTL_RUN_CMD_SYNC \
	_IOW(APUSYS_MAGICNO, 6, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_RUN_CMD_ASYNC \
	_IOWR(APUSYS_MAGICNO, 7, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_WAIT_CMD \
	_IOW(APUSYS_MAGICNO, 8, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_SET_POWER \
	_IOW(APUSYS_MAGICNO, 9, struct apusys_ioctl_power)
#define APUSYS_IOCTL_DEVICE_ALLOC \
	_IOWR(APUSYS_MAGICNO, 10, struct apusys_ioctl_dev)
#define APUSYS_IOCTL_DEVICE_FREE \
	_IOW(APUSYS_MAGICNO, 11, struct apusys_ioctl_dev)
#define APUSYS_IOCTL_FW_LOAD \
	_IOW(APUSYS_MAGICNO, 12, struct apusys_ioctl_fw)
#define APUSYS_IOCTL_FW_UNLOAD \
	_IOW(APUSYS_MAGICNO, 13, struct apusys_ioctl_fw)
#define APUSYS_IOCTL_USER_CMD \
	_IOW(APUSYS_MAGICNO, 14, struct apusys_ioctl_ucmd)
#define APUSYS_IOCTL_MEM_MAP \
	_IOWR(APUSYS_MAGICNO, 15, struct apusys_mem)
#define APUSYS_IOCTL_MEM_UNMAP \
	_IOWR(APUSYS_MAGICNO, 16, struct apusys_mem)

#define APUSYS_IOCTL_SEC_DEVICE_LOCK \
	_IOW(APUSYS_MAGICNO, 60, int)
#define APUSYS_IOCTL_SEC_DEVICE_UNLOCK \
	_IOW(APUSYS_MAGICNO, 61, int)

//----------------------------------------------
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
};

enum MDW_MEM_IOCTL_ALLOC_BITMASK {
	MDW_MEM_IOCTL_ALLOC_CACHEABLE,
	MDW_MEM_IOCTL_ALLOC_32BIT,
};

struct mdw_mem_in {
	enum mdw_mem_ioctl_op op;
	uint64_t flags;
	union {
		struct {
			uint32_t size;
			uint32_t align;
			uint64_t flags; //enum MDW_MEM_IOCTL_ALLOC_BITMASK
		} alloc;
		struct {
			uint64_t handle;
		} free;
		struct {
			uint64_t handle;
			uint32_t size;
		} map;
		struct {
			uint64_t handle;
		} unmap;
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
	MDW_CMD_IOCTL_RUNASYNC,
	MDW_CMD_IOCTL_WAIT,
	MDW_CMD_IOCTL_RUNFENCE,
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
	uint32_t kernel_time;
	uint32_t driver_time;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t pack_id;

	/* cmdbufs */
	uint32_t num_cmdbufs;
	uint64_t cmdbufs;
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
			uint64_t cmd_uid;
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
		} exec;

		struct {
			uint64_t id;
		} wait;
	};
};

struct mdw_cmd_out {
	union {
		struct {
			uint64_t id;
		} build;

		struct {
			uint32_t kernel_duration;
			uint32_t mdw_duration;
			uint32_t driver_duration;
		} done;

		struct {
			uint64_t fence;
		} fence;
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
