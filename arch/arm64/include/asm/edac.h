/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef ASM_EDAC_H
#define ASM_EDAC_H

#ifdef CONFIG_EDAC_CORTEX_ARM64
void arm64_erp_local_dbe_handler(void);
#else
static inline void arm64_erp_local_dbe_handler(void) { }
#endif

static inline void atomic_scrub(void *addr, int size)
{
	return;
}

#endif
