/*
 * Copyright (C) 2020 MediaTek Inc.
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

#endif
