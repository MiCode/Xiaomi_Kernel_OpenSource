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
