/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef ASM_KRYO_EDAC_H
#define ASM_KRYO_EDAC_H

#if defined(CONFIG_EDAC_KRYO_ARM64)
void kryo_poll_cache_errors(void *info);
#else
static inline void kryo_poll_cache_errors(void *info) { }
#endif

#endif
