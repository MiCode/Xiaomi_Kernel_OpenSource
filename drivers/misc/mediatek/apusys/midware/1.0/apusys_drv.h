/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
	uint32_t mem_support; // bitmap
	uint64_t dev_support; // bitmap

	uint32_t dev_type_max;
};

struct hs_query_dev {
	uint32_t type;
	uint32_t num;
};

struct apusys_ioctl_hs {
	uint64_t magic_num;
	uint8_t cmd_version;

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
	APUSYS_MAP,
	APUSYS_MAX,
};

enum {
	APUSYS_CACHE_NONE,
	APUSYS_CACHE_SYNC,
	APUSYS_CACHE_INVALIDATE,
	APUSYS_CACHE_MAX,
};

enum {
	APUSYS_MAP_NONE,
	APUSYS_MAP_KVA,
	APUSYS_UNMAP_KVA,
	APUSYS_MAP_IOVA,
	APUSYS_UNMAP_IOVA,
	APUSYS_MAP_PA,
	APUSYS_UNMAP_PA,
	APUSYS_MAP_MAX,
};

enum {
	APUSYS_MEM_DRAM_ION,
	APUSYS_MEM_DRAM_DMA,
	APUSYS_MEM_TCM,

	APUSYS_MEM_MAX,
};


struct apusys_cache_param {
	int cache_type;
};

struct apusys_map_param {
	int map_type;
};

struct apusys_mem_ctl {
	int cmd;
	union {
		struct apusys_cache_param cache_param;
		struct apusys_map_param map_param;
	};
};

struct apusys_ion_info {
	int ion_share_fd;
	uint64_t ion_khandle;
	int ion_uhandle;
};


struct apusys_mem {
	uint64_t uva;
	uint64_t kva;
	uint32_t iova;
	uint32_t size;

	uint32_t align;
	uint32_t cache;

	int mem_type;
	struct apusys_ion_info ion_data;
	struct apusys_mem_ctl ctl_data;
};

/* for APUSYS_IOCTL_RUN_CMD_SYNC */
struct apusys_ioctl_cmd {
	uint64_t cmd_id;
	//uint8_t priority;
	uint64_t uva; // user va
	uint64_t kva; // kernel va
	uint32_t iova; // device va
	uint32_t size;
};

/* for APUSYS_IOCTL_SET_POWER */
struct apusys_ioctl_power {
	int dev_type;
	uint32_t idx;
	uint32_t boost_val;
};

/* for APUSYS_IOCTL_DEVICE */
struct apusys_ioctl_dev {
	int dev_type;
	uint32_t num;

	int dev_idx;

	uint64_t handle;
};

/* for APUSYS_IOCTL_FW */
struct apusys_ioctl_fw {
	int dev_type;
	int idx;

	char name[32];

	uint64_t kva;
	uint32_t iova;
	uint32_t size;
};

#define APUSYS_MAGICNO 'A'
#define APUSYS_IOCTL_HANDSHAKE \
	_IOWR(APUSYS_MAGICNO, 0, struct apusys_ioctl_hs)
#define APUSYS_IOCTL_MEM_ALLOC \
	_IOWR(APUSYS_MAGICNO, 1, struct apusys_mem)
#define APUSYS_IOCTL_MEM_FREE \
	_IOWR(APUSYS_MAGICNO, 2, struct apusys_mem)
#define APUSYS_IOCTL_MEM_CTL \
	_IOWR(APUSYS_MAGICNO, 3, struct apusys_mem)
#define APUSYS_IOCTL_RUN_CMD_SYNC \
	_IOW(APUSYS_MAGICNO, 4, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_RUN_CMD_ASYNC \
	_IOWR(APUSYS_MAGICNO, 5, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_WAIT_CMD \
	_IOW(APUSYS_MAGICNO, 6, struct apusys_ioctl_cmd)
#define APUSYS_IOCTL_SET_POWER \
	_IOW(APUSYS_MAGICNO, 7, struct apusys_ioctl_power)
#define APUSYS_IOCTL_DEVICE_ALLOC \
	_IOWR(APUSYS_MAGICNO, 8, struct apusys_ioctl_dev)
#define APUSYS_IOCTL_DEVICE_FREE \
	_IOW(APUSYS_MAGICNO, 9, struct apusys_ioctl_dev)
#define APUSYS_IOCTL_FW_LOAD \
	_IOW(APUSYS_MAGICNO, 10, struct apusys_ioctl_fw)
#define APUSYS_IOCTL_FW_UNLOAD \
	_IOW(APUSYS_MAGICNO, 11, struct apusys_ioctl_fw)

#endif
