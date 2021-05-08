/*
 * Copyright (C) 2015 MediaTek Inc.
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
#ifndef __ION_FB_HEAP_H__
#define __ION_FB_HEAP_H__
#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#endif
int ion_get_domain_id(int from_kernel, int *port);
struct ion_fb_buffer_info {
	int module_id;
	int fix_module_id;
	unsigned int security;
	unsigned int coherent;
	unsigned int mva_cnt;
	void *VA;
	unsigned long MVA[DOMAIN_NUM];
	unsigned long FIXED_MVA[DOMAIN_NUM];
	unsigned long iova_start[DOMAIN_NUM];
	unsigned long iova_end[DOMAIN_NUM];
	int port[DOMAIN_NUM];
	struct ion_mm_buf_debug_info dbg_info;
	ion_phys_addr_t priv_phys;
	pid_t pid;
	struct mutex lock;	/*mutex lock on fb buffer */
};

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data);
void ion_fb_heap_destroy(struct ion_heap *heap);
#endif
