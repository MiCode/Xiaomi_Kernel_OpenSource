/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

enum smem_type {
	SMEM_ION,
};

enum smem_cache_prop {
	SMEM_CACHED,
	SMEM_UNCACHED,
};

struct msm_smem {
	int mem_type;
	size_t size;
	void *kvaddr;
	unsigned long device_addr;
	int domain;
	int partition_num;
	void *smem_priv;
};

void *msm_smem_new_client(enum smem_type mtype);
struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags,
		int domain, int partition, int map_kernel);
void msm_smem_free(void *clt, struct msm_smem *mem);
void msm_smem_delete_client(void *clt);
struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset, int
		domain, int partition);
int msm_smem_clean_invalidate(void *clt, struct msm_smem *mem);
#endif
