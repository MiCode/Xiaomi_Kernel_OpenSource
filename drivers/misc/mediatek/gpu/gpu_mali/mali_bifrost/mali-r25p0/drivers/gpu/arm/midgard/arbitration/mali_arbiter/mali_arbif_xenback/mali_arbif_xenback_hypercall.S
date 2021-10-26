// SPDX-License-Identifier: GPL-2.0

/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file mali_arbif_xenback_hypercall.S
 * Xen hypercall function
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <xen/interface/xen.h>

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
ENTRY(HYPERVISOR_arb_gpu_op)
	mov r12, #__HYPERVISOR_arb_gpu_op;
	__HVC(XEN_IMM);
	ret lr;
ENDPROC(HYPERVISOR_arb_gpu_op)
#endif

#ifdef __aarch64__
ENTRY(HYPERVISOR_arb_gpu_op)
	mov x16, #__HYPERVISOR_arb_gpu_op;
	hvc XEN_IMM;
	ret;
ENDPROC(HYPERVISOR_arb_gpu_op)
#endif
