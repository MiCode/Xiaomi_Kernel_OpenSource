/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
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

#if defined(CONFIG_EDAC_CORTEX_ARM64) && \
	!defined(CONFIG_EDAC_CORTEX_ARM64_DBE_IRQ_ONLY)
void arm64_check_cache_ecc(void *info);
#else
static inline void arm64_check_cache_ecc(void *info) { }
#endif

static inline void atomic_scrub(void *addr, int size) { }

#endif
