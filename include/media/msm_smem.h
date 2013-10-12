/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _MSM_SMEM_H_
#define _MSM_SMEM_H_

#include <linux/types.h>
#include <linux/msm_ion.h>

#define HAL_BUFFER_MAX 0xb

enum smem_type {
	SMEM_ION,
};

enum smem_prop {
	SMEM_CACHED = ION_FLAG_CACHED,
	SMEM_SECURE = ION_FLAG_SECURE,
};

enum hal_buffer {
	HAL_BUFFER_INPUT = 0x1,
	HAL_BUFFER_OUTPUT = 0x2,
	HAL_BUFFER_OUTPUT2 = 0x2,
	HAL_BUFFER_EXTRADATA_INPUT = 0x4,
	HAL_BUFFER_EXTRADATA_OUTPUT = 0x8,
	HAL_BUFFER_EXTRADATA_OUTPUT2 = 0x8,
	HAL_BUFFER_INTERNAL_SCRATCH = 0x10,
	HAL_BUFFER_INTERNAL_SCRATCH_1 = 0x20,
	HAL_BUFFER_INTERNAL_SCRATCH_2 = 0x40,
	HAL_BUFFER_INTERNAL_PERSIST = 0x80,
	HAL_BUFFER_INTERNAL_PERSIST_1 = 0x100,
	HAL_BUFFER_INTERNAL_CMD_QUEUE = 0x200,
};

struct msm_smem {
	int mem_type;
	size_t size;
	void *kvaddr;
	unsigned long device_addr;
	u32 flags;
	void *smem_priv;
	enum hal_buffer buffer_type;
};

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

void *msm_smem_new_client(enum smem_type mtype,
				void *platform_resources);
struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags,
		enum hal_buffer buffer_type, int map_kernel);
void msm_smem_free(void *clt, struct msm_smem *mem);
void msm_smem_delete_client(void *clt);
int msm_smem_cache_operations(void *clt, struct msm_smem *mem,
		enum smem_cache_ops);
struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset,
				enum hal_buffer buffer_type);
int msm_smem_clean_invalidate(void *clt, struct msm_smem *mem);
int msm_smem_get_domain_partition(void *clt, u32 flags, enum hal_buffer
		buffer_type, int *domain_num, int *partition_num);
#endif
