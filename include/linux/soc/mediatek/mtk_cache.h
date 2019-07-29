/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_CACHE__
#define __MTK_CACHE__

/*
 * Be careful!!
 *
 * functions here are used for limited drivers which need
 * special cache operation.
 * please make sure you need these non-standard operations
 * Anything in doubt, please discuss with the system service
 * member.
 *
 */
void __inner_flush_dcache_L1(void);
void __inner_flush_dcache_L2(void);
void __inner_flush_dcache_all(void);
void __inner_clean_dcache_L1(void);
void __inner_clean_dcache_L2(void);
void __inner_clean_dcache_all(void);
void __inner_inv_dcache_L1(void);
void __inner_inv_dcache_L2(void);
void __inner_inv_dcache_all(void);
void __disable_dcache__inner_flush_dcache_L1(void);
void __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);
void __disable_dcache__inner_clean_dcache_L1__inner_clean_dcache_L2(void);
void dis_D_inner_fL1L2(void);
void dis_D_inner_flush_all(void);
void __flush_dcache_user_area(void *start, unsigned int size);
void __clean_dcache_user_area(void *start, unsigned int size);
void __inval_dcache_user_area(void *start, unsigned int size);

#endif /* end of __MTK_CACHE__ */
