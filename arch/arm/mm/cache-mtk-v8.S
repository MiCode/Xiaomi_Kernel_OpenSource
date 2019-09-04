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
#include <linux/linkage.h>

#include <asm/assembler.h>

/*
 *      __xxxx_dcache_user_area(start,size)
 *
 *      Ensure that any D-cache lines for the interval [start, start+size)
 *      are cleaned or/and invalidated to the PoC.
 *
 *      - start        - virtual start address of region (EL0/EL1)
 *      - size         - size in question
 */

ENTRY(__clean_dcache_user_area)
	uaccess_enable r2
	add	r1, r1, r0
	b	v7_dma_clean_range
	uaccess_disable r2
ENDPROC(__clean_dcache_user_area)

ENTRY(__clean_dcache_area_poc)
	add	r1, r1, r0
	b	v7_dma_clean_range
ENDPROC(__clean_dcache_area_poc)

ENTRY(__flush_dcache_user_area)
	uaccess_enable r2
	add	r1, r1, r0
	b	v7_dma_flush_range
	uaccess_disable r2
ENDPROC(__flush_dcache_user_area)

ENTRY(__flush_dcache_area)
	add	r1, r1, r0
	b	v7_dma_flush_range
ENDPROC(__flush_dcache_area)

ENTRY(__inval_dcache_user_area)
	uaccess_enable r2
	add	r1, r1, r0
	b	v7_dma_inv_range
	uaccess_disable r2
ENDPROC(__inval_dcache_user_area)

ENTRY(__inval_dcache_area)
	add	r1, r1, r0
	b	v7_dma_inv_range
ENDPROC(__inval_dcache_area)

