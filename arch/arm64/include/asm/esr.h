/*
 * Copyright (C) 2013 - ARM Ltd
 * Copyright (C) 2018 XiaoMi, Inc.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
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

#ifndef __ASM_ESR_H
#define __ASM_ESR_H

#define ESR_EL1_WRITE		(1 << 6)
#define ESR_EL1_CM		(1 << 8)
#define ESR_EL1_IL		(1 << 25)

#define ESR_EL1_EC_SHIFT	(26)
#define ESR_EL1_EC_UNKNOWN	(0x00)
#define ESR_EL1_EC_WFI		(0x01)
#define ESR_EL1_EC_CP15_32	(0x03)
#define ESR_EL1_EC_CP15_64	(0x04)
#define ESR_EL1_EC_CP14_MR	(0x05)
#define ESR_EL1_EC_CP14_LS	(0x06)
#define ESR_EL1_EC_FP_ASIMD	(0x07)
#define ESR_EL1_EC_CP10_ID	(0x08)
#define ESR_EL1_EC_CP14_64	(0x0C)
#define ESR_EL1_EC_ILL_ISS	(0x0E)
#define ESR_EL1_EC_SVC32	(0x11)
#define ESR_EL1_EC_SVC64	(0x15)
#define ESR_EL1_EC_SYS64	(0x18)
#define ESR_EL1_EC_IABT_EL0	(0x20)
#define ESR_EL1_EC_IABT_EL1	(0x21)
#define ESR_EL1_EC_PC_ALIGN	(0x22)
#define ESR_EL1_EC_DABT_EL0	(0x24)
#define ESR_EL1_EC_DABT_EL1	(0x25)
#define ESR_EL1_EC_SP_ALIGN	(0x26)
#define ESR_EL1_EC_FP_EXC32	(0x28)
#define ESR_EL1_EC_FP_EXC64	(0x2C)
#define ESR_EL1_EC_SERROR	(0x2F)
#define ESR_EL1_EC_BREAKPT_EL0	(0x30)
#define ESR_EL1_EC_BREAKPT_EL1	(0x31)
#define ESR_EL1_EC_SOFTSTP_EL0	(0x32)
#define ESR_EL1_EC_SOFTSTP_EL1	(0x33)
#define ESR_EL1_EC_WATCHPT_EL0	(0x34)
#define ESR_EL1_EC_WATCHPT_EL1	(0x35)
#define ESR_EL1_EC_BKPT32	(0x38)
#define ESR_EL1_EC_BRK64	(0x3C)

/* ISS field definitions for System instruction traps */
#define ESR_ELx_SYS64_ISS_RES0_SHIFT	22
#define ESR_ELx_SYS64_ISS_RES0_MASK	(UL(0x7) << ESR_ELx_SYS64_ISS_RES0_SHIFT)
#define ESR_ELx_SYS64_ISS_DIR_MASK	0x1
#define ESR_ELx_SYS64_ISS_DIR_READ	0x1
#define ESR_ELx_SYS64_ISS_DIR_WRITE	0x0

#define ESR_ELx_SYS64_ISS_RT_SHIFT	5
#define ESR_ELx_SYS64_ISS_RT_MASK	(UL(0x1f) << ESR_ELx_SYS64_ISS_RT_SHIFT)
#define ESR_ELx_SYS64_ISS_CRM_SHIFT	1
#define ESR_ELx_SYS64_ISS_CRM_MASK	(UL(0xf) << ESR_ELx_SYS64_ISS_CRM_SHIFT)
#define ESR_ELx_SYS64_ISS_CRN_SHIFT	10
#define ESR_ELx_SYS64_ISS_CRN_MASK	(UL(0xf) << ESR_ELx_SYS64_ISS_CRN_SHIFT)
#define ESR_ELx_SYS64_ISS_OP1_SHIFT	14
#define ESR_ELx_SYS64_ISS_OP1_MASK	(UL(0x7) << ESR_ELx_SYS64_ISS_OP1_SHIFT)
#define ESR_ELx_SYS64_ISS_OP2_SHIFT	17
#define ESR_ELx_SYS64_ISS_OP2_MASK	(UL(0x7) << ESR_ELx_SYS64_ISS_OP2_SHIFT)
#define ESR_ELx_SYS64_ISS_OP0_SHIFT	20
#define ESR_ELx_SYS64_ISS_OP0_MASK	(UL(0x3) << ESR_ELx_SYS64_ISS_OP0_SHIFT)
#define ESR_ELx_SYS64_ISS_SYS_MASK	(ESR_ELx_SYS64_ISS_OP0_MASK | \
					 ESR_ELx_SYS64_ISS_OP1_MASK | \
					 ESR_ELx_SYS64_ISS_OP2_MASK | \
					 ESR_ELx_SYS64_ISS_CRN_MASK | \
					 ESR_ELx_SYS64_ISS_CRM_MASK)
#define ESR_ELx_SYS64_ISS_SYS_VAL(op0, op1, op2, crn, crm) \
					(((op0) << ESR_ELx_SYS64_ISS_OP0_SHIFT) | \
					 ((op1) << ESR_ELx_SYS64_ISS_OP1_SHIFT) | \
					 ((op2) << ESR_ELx_SYS64_ISS_OP2_SHIFT) | \
					 ((crn) << ESR_ELx_SYS64_ISS_CRN_SHIFT) | \
					 ((crm) << ESR_ELx_SYS64_ISS_CRM_SHIFT))

#define ESR_ELx_SYS64_ISS_SYS_OP_MASK	(ESR_ELx_SYS64_ISS_SYS_MASK | \
					 ESR_ELx_SYS64_ISS_DIR_MASK)

#define ESR_ELx_SYS64_ISS_SYS_CNTVCT	(ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 2, 14, 0) | \
					 ESR_ELx_SYS64_ISS_DIR_READ)

#define ESR_ELx_SYS64_ISS_SYS_CNTFRQ	(ESR_ELx_SYS64_ISS_SYS_VAL(3, 3, 0, 14, 0) | \
					 ESR_ELx_SYS64_ISS_DIR_READ)

#endif /* __ASM_ESR_H */
