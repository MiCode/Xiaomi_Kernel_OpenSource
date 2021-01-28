/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ION_FB_HEAP_H__
#define __ION_FB_HEAP_H__

struct ion_fb_buffer_info {
	struct mutex lock;	/*mutex lock on fb buffer */
	int module_id;
	unsigned int security;
	unsigned int coherent;
	void *VA;
	unsigned int MVA;
	unsigned int FIXED_MVA;
	unsigned long iova_start;
	unsigned long iova_end;
	ion_phys_addr_t priv_phys;
	struct ion_mm_buf_debug_info dbg_info;
};

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data);
void ion_fb_heap_destroy(struct ion_heap *heap);
#endif
