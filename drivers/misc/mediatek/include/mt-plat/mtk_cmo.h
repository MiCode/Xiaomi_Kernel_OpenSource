/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_CMO_H__
#define __MTK_CMO_H__

#ifdef CONFIG_MEDIATEK_SOLUTION
extern void inner_dcache_flush_all(void);
extern void inner_dcache_flush_L1(void);
extern void inner_dcache_flush_L2(void);
extern void inner_dcache_disable(void);
extern void smp_inner_dcache_flush_all(void);
#endif

#endif /* __MTK_CMO_H__ */
