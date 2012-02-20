/*
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#ifndef _FMEM_H_
#define _FMEM_H_

#include <linux/vmalloc.h>

struct fmem_platform_data {
	unsigned long phys;
	unsigned long size;
	unsigned long reserved_size_low;
	unsigned long reserved_size_high;
};

struct fmem_data {
	unsigned long phys;
	void *virt;
	struct vm_struct *area;
	unsigned long size;
	unsigned long reserved_size_low;
	unsigned long reserved_size_high;
};

enum fmem_state {
	FMEM_UNINITIALIZED = 0,
	FMEM_C_STATE,
	FMEM_T_STATE,
	FMEM_O_STATE,
};

#ifdef CONFIG_QCACHE
struct fmem_data *fmem_get_info(void);
int fmem_set_state(enum fmem_state);
void lock_fmem_state(void);
void unlock_fmem_state(void);
void *fmem_map_virtual_area(int cacheability);
void fmem_unmap_virtual_area(void);
#else
static inline struct fmem_data *fmem_get_info(void) { return NULL; }
static inline int fmem_set_state(enum fmem_state f) { return -ENODEV; }
static inline void lock_fmem_state(void) { return; }
static inline void unlock_fmem_state(void) { return; }
static inline void *fmem_map_virtual_area(int cacheability) { return NULL; }
static inline void fmem_unmap_virtual_area(void) { return; }
#endif

int request_fmem_c_region(void *unused);
int release_fmem_c_region(void *unused);
#endif
