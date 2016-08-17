/*
 * arch/arm/mach-tegra/include/mach/asm_macros.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_ASM_MACROS_H_
#define _MACH_TEGRA_ASM_MACROS_H_

#ifdef __ASSEMBLY__

/* waits until the microsecond counter (base) ticks, for exact timing loops */
.macro  wait_for_us, rd, base, tmp
	ldr    \rd, [\base]
1001:   ldr    \tmp, [\base]
	cmp    \rd, \tmp
	beq    1001b
	mov    \tmp, \rd
.endm

/* waits until the microsecond counter (base) is > rn */
.macro	wait_until, rn, base, tmp
	add	\rn, \rn, #1
1002:	ldr	\tmp, [\base]
	sub	\tmp, \tmp, \rn
	ands	\tmp, \tmp, #0x80000000
	dmb
	bne	1002b
.endm

/* returns the offset of the flow controller halt register for a cpu */
.macro cpu_to_halt_reg rd, rcpu
	cmp	\rcpu, #0
	subne	\rd, \rcpu, #1
	movne	\rd, \rd, lsl #3
	addne	\rd, \rd, #0x14
	moveq	\rd, #0
.endm

/* returns the offset of the flow controller csr register for a cpu */
.macro cpu_to_csr_reg rd, rcpu
	cmp	\rcpu, #0
	subne	\rd, \rcpu, #1
	movne	\rd, \rd, lsl #3
	addne	\rd, \rd, #0x18
	moveq	\rd, #8
.endm

/* returns the ID of the current processor */
.macro cpu_id, rd
	mrc	p15, 0, \rd, c0, c0, 5
	and	\rd, \rd, #0xF
.endm

/* loads a 32-bit value into a register without a data access */
.macro mov32, reg, val
	movw	\reg, #:lower16:\val
	movt	\reg, #:upper16:\val
.endm

#endif
#endif
