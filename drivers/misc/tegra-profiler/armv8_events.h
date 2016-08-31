/*
 * drivers/misc/tegra-profiler/armv8_events.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __ARMV8_EVENTS_H
#define __ARMV8_EVENTS_H

#define QUADD_AA64_PMUVER_PMUV3		0x01

#define QUADD_AA64_CPU_IMP_NVIDIA	'N'

#define QUADD_AA64_CPU_IDCODE_CORTEX_A57	0x01


enum {
	QUADD_AA64_CPU_TYPE_UNKNOWN = 1,
	QUADD_AA64_CPU_TYPE_ARM,
	QUADD_AA64_CPU_TYPE_CORTEX_A57,
	QUADD_AA64_CPU_TYPE_UNKNOWN_IMP,
	QUADD_AA64_CPU_TYPE_DENVER,
};

/*
 * Performance Monitors Control Register
 */

/* All counters, including PMCCNTR_EL0, are disabled/enabled */
#define QUADD_ARMV8_PMCR_E		(1 << 0)
/* Reset all event counters, not including PMCCNTR_EL0, to 0 */
#define QUADD_ARMV8_PMCR_P		(1 << 1)
/* Reset PMCCNTR_EL0 to 0 */
#define QUADD_ARMV8_PMCR_C		(1 << 2)
/* Clock divider: PMCCNTR_EL0 counts every clock cycle/every 64 clock cycles */
#define QUADD_ARMV8_PMCR_D		(1 << 3)
/* Export of events is disabled/enabled */
#define QUADD_ARMV8_PMCR_X		(1 << 4)
/* Disable cycle counter, PMCCNTR_EL0 when event counting is prohibited */
#define QUADD_ARMV8_PMCR_DP		(1 << 5)
/* Long cycle count enable */
#define QUADD_ARMV8_PMCR_LC		(1 << 6)

/* Number of event counters */
#define	QUADD_ARMV8_PMCR_N_SHIFT	16
#define	QUADD_ARMV8_PMCR_N_MASK		0x1f

/* Identification code */
#define	QUADD_ARMV8_PMCR_IDCODE_SHIFT	11
#define	QUADD_ARMV8_PMCR_IDCODE_MASK	0xff

/* Implementer code */
#define	QUADD_ARMV8_PMCR_IMP_SHIFT	24

/* Mask for writable bits */
#define	QUADD_ARMV8_PMCR_WR_MASK	0x3f

/* Cycle counter */
#define QUADD_ARMV8_CCNT_BIT		31
#define QUADD_ARMV8_CCNT		(1 << QUADD_ARMV8_CCNT_BIT)

/*
 * Performance Counter Selection Register mask
 */
#define QUADD_ARMV8_SELECT_MASK	0x1f

/*
 * EVTSEL Register mask
 */
#define QUADD_ARMV8_EVTSEL_MASK		0xff

#define QUADD_ARMV8_COUNTERS_MASK_PMUV3	0x3f

/*
 * ARMv8 PMUv3 Performance Events handling code.
 * Common event types.
 */
enum {
	/* Required events. */
	QUADD_ARMV8_HW_EVENT_PMNC_SW_INCR			= 0x00,
	QUADD_ARMV8_HW_EVENT_L1_DCACHE_REFILL			= 0x03,
	QUADD_ARMV8_HW_EVENT_L1_DCACHE_ACCESS			= 0x04,
	QUADD_ARMV8_HW_EVENT_PC_BRANCH_MIS_PRED			= 0x10,
	QUADD_ARMV8_HW_EVENT_CLOCK_CYCLES			= 0x11,
	QUADD_ARMV8_HW_EVENT_PC_BRANCH_PRED			= 0x12,

	/* At least one of the following is required. */
	QUADD_ARMV8_HW_EVENT_INSTR_EXECUTED			= 0x08,
	QUADD_ARMV8_HW_EVENT_OP_SPEC				= 0x1B,

	/* Common architectural events. */
	QUADD_ARMV8_HW_EVENT_MEM_READ				= 0x06,
	QUADD_ARMV8_HW_EVENT_MEM_WRITE				= 0x07,
	QUADD_ARMV8_HW_EVENT_EXC_TAKEN				= 0x09,
	QUADD_ARMV8_HW_EVENT_EXC_EXECUTED			= 0x0A,
	QUADD_ARMV8_HW_EVENT_CID_WRITE				= 0x0B,
	QUADD_ARMV8_HW_EVENT_PC_WRITE				= 0x0C,
	QUADD_ARMV8_HW_EVENT_PC_IMM_BRANCH			= 0x0D,
	QUADD_ARMV8_HW_EVENT_PC_PROC_RETURN			= 0x0E,
	QUADD_ARMV8_HW_EVENT_MEM_UNALIGNED_ACCESS		= 0x0F,
	QUADD_ARMV8_HW_EVENT_TTBR_WRITE				= 0x1C,

	/* Common microarchitectural events. */
	QUADD_ARMV8_HW_EVENT_L1_ICACHE_REFILL			= 0x01,
	QUADD_ARMV8_HW_EVENT_ITLB_REFILL			= 0x02,
	QUADD_ARMV8_HW_EVENT_DTLB_REFILL			= 0x05,
	QUADD_ARMV8_HW_EVENT_MEM_ACCESS				= 0x13,
	QUADD_ARMV8_HW_EVENT_L1_ICACHE_ACCESS			= 0x14,
	QUADD_ARMV8_HW_EVENT_L1_DCACHE_WB			= 0x15,
	QUADD_ARMV8_HW_EVENT_L2_CACHE_ACCESS			= 0x16,
	QUADD_ARMV8_HW_EVENT_L2_CACHE_REFILL			= 0x17,
	QUADD_ARMV8_HW_EVENT_L2_CACHE_WB			= 0x18,
	QUADD_ARMV8_HW_EVENT_BUS_ACCESS				= 0x19,
	QUADD_ARMV8_HW_EVENT_MEM_ERROR				= 0x1A,
	QUADD_ARMV8_HW_EVENT_BUS_CYCLES				= 0x1D,
};

#define QUADD_ARMV8_UNSUPPORTED_EVENT	0xff00
#define QUADD_ARMV8_CPU_CYCLE_EVENT	0xffff

#endif	/* __ARMV8_EVENTS_H */
