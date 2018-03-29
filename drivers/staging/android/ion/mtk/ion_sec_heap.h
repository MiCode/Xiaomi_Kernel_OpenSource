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

#ifndef __ION_SEC_HEAP_H__
#define __ION_SEC_HEAP_H__

typedef struct {
	struct mutex lock;
	int eModuleID;
	unsigned int security;
	unsigned int coherent;
	void *pVA;
	unsigned int MVA;
	ion_phys_addr_t priv_phys;
	ion_mm_buf_debug_info_t dbg_info;
	ion_mm_sf_buf_info_t sf_buf_info;
} ion_sec_buffer_info;

#endif
