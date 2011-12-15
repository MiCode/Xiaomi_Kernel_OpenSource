/*
 *
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

struct fmem_platform_data {
	unsigned long phys;
	unsigned long size;
};

struct fmem_data {
	unsigned long phys;
	void *virt;
	unsigned long size;
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
#else
static inline struct fmem_data *fmem_get_info(void) { return NULL; }
static inline int fmem_set_state(enum fmem_state f) { return -ENODEV; }
static inline void lock_fmem_state(void) { return; }
static inline void unlock_fmem_state(void) { return; }
#endif


#endif
