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

#ifndef __MT_INNERCACHE_H
#define __MT_INNERCACHE_H

extern void __inner_flush_dcache_all(void);
extern void __inner_flush_dcache_L1(void);
extern void __inner_flush_dcache_L2(void);
extern void __disable_dcache(void);

#ifdef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL
#define CACHE_FLUSH_BY_SETWAY	1
#define CACHE_FLUSH_BY_MVA	2
#define CACHE_FLUSH_TIMEOUT	1000
#endif

#endif
