/* SPDX-License-Identifier: GPL-2.0 */
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
       .text

#include <linux/linkage.h>

ENTRY(dis_D_inner_flush_all)
	/* disable data cache*/
	ARM(	stmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
	THUMB(	stmfd	sp!, {r4-r7, r9-r11, lr}	)

	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb

	bl	v7_flush_dcache_all

	ARM(	ldmfd	sp!, {r4-r5, r7, r9-r11, lr}	)
	THUMB(	ldmfd	sp!, {r4-r7, r9-r11, lr}	)
	bx	lr
ENDPROC(dis_D_inner_flush_all)

        .end
