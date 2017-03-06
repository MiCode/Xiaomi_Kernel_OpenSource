/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _UAPI__ASM_SIGCONTEXT_H
#define _UAPI__ASM_SIGCONTEXT_H

#ifdef CONFIG_64BIT
#include <linux/types.h>

/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.
 */
struct sigcontext {
	__u64 fault_address;
	/* AArch64 registers */
	__u64 regs[31];
	__u64 sp;
	__u64 pc;
	__u64 pstate;
	/* 4K reserved for FP/SIMD state and future expansion */
	__u8 __reserved[4096] __attribute__((__aligned__(16)));
};

/*
 * Header to be used at the beginning of structures extending the user
 * context. Such structures must be placed after the rt_sigframe on the stack
 * and be 16-byte aligned. The last structure must be a dummy one with the
 * magic and size set to 0.
 */
struct _aarch64_ctx {
	__u32 magic;
	__u32 size;
};

#define FPSIMD_MAGIC	0x46508001

struct fpsimd_context {
	struct _aarch64_ctx head;
	__u32 fpsr;
	__u32 fpcr;
	__uint128_t vregs[32];
};

/* ESR_EL1 context */
#define ESR_MAGIC	0x45535201

struct esr_context {
	struct _aarch64_ctx head;
	__u64 esr;
};

#else /* CONFIG_64BIT */

/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.  Note: only add new entries
 * to the end of the structure.
 */
struct sigcontext {
	unsigned long trap_no;
	unsigned long error_code;
	unsigned long oldmask;
	unsigned long arm_r0;
	unsigned long arm_r1;
	unsigned long arm_r2;
	unsigned long arm_r3;
	unsigned long arm_r4;
	unsigned long arm_r5;
	unsigned long arm_r6;
	unsigned long arm_r7;
	unsigned long arm_r8;
	unsigned long arm_r9;
	unsigned long arm_r10;
	unsigned long arm_fp;
	unsigned long arm_ip;
	unsigned long arm_sp;
	unsigned long arm_lr;
	unsigned long arm_pc;
	unsigned long arm_cpsr;
	unsigned long fault_address;
};
#endif /* CONFIG_64BIT */
#endif /* _UAPI__ASM_SIGCONTEXT_H */
