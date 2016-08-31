/*
 * Copyright (c) 2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/linkage.h>
#include <linux/init.h>

ENTRY(tlk_irq_handler)
	movw	r0, #0x5
	movt	r0, #0x3200	@ TE_SMC_NS_IRQ_DONE
	mov	r1, #0
	mov	r2, #0
	smc	#0
ENDPROC(tlk_irq_handler)
