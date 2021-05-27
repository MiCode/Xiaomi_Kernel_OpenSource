// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2019-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/**
 * @file mali_arbif_xenback_hypercall.S
 * Xen hypercall function
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <xen/interface/xen.h>
#include <linux/version.h>

#define XEN_IMM 0xEA1

/* This macro is defined also in include/xen/interface/xen.h
 * Please keep them in sync
 */
#ifndef __HYPERVISOR_arb_gpu_op
#define __HYPERVISOR_arb_gpu_op 63
#endif /* __HYPERVISOR_arb_gpu_op */

                .text

/**
 * For these functions, the caller constructs a xen_arb_gpu_op on the
 * stack and passes x0/r0 which points to it as if it is making a normal
 * function call. These will be used by the hvc hypercall instruction.
 * x16/r12 is the first intra-procedure-call scratch register and is
 * used to pass the address of the function that xen will call when the
 * hypercall is received (__HYPERVISOR_arb_gpu_op)
 */
#ifdef __arm__
#if (KERNEL_VERSION(5, 8, 0) > LINUX_VERSION_CODE)

ENTRY(HYPERVISOR_arb_gpu_op)
	mov r12, #__HYPERVISOR_arb_gpu_op;
	__HVC(XEN_IMM);
	ret lr;
ENDPROC(HYPERVISOR_arb_gpu_op)

#else /* LINUX_VERSION_CODE >= 5.8 */

SYM_FUNC_START(HYPERVISOR_arb_gpu_op)
	mov r12, #__HYPERVISOR_arb_gpu_op;
	__HVC(XEN_IMM);
	ret lr;
SYM_FUNC_END(HYPERVISOR_arb_gpu_op)

#endif /* LINUX_VERSION_CODE */
#endif /* __arm__ */

#ifdef __aarch64__
#if (KERNEL_VERSION(5, 8, 0) > LINUX_VERSION_CODE)

ENTRY(HYPERVISOR_arb_gpu_op)
	mov x16, #__HYPERVISOR_arb_gpu_op;
	hvc XEN_IMM;
	ret;
ENDPROC(HYPERVISOR_arb_gpu_op)

#else /* LINUX_VERSION_CODE >= 5.8 */

SYM_FUNC_START(HYPERVISOR_arb_gpu_op)
	mov x16, #__HYPERVISOR_arb_gpu_op;
	hvc XEN_IMM;
	ret;
SYM_FUNC_END(HYPERVISOR_arb_gpu_op)

#endif /* LINUX_VERSION_CODE */
#endif /* __aarch64__ */
