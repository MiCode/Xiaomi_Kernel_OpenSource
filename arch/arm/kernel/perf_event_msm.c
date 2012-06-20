/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpumask.h>
#include <asm/cp15.h>
#include <asm/vfp.h>
#include <asm/system.h>
#include "../vfp/vfpinstr.h"

#ifdef CONFIG_CPU_V7
#define SCORPION_EVT_PREFIX 1
#define SCORPION_MAX_L1_REG 4

#define SCORPION_EVTYPE_EVENT 0xfffff

static u32 scorpion_evt_type_base[] = {0x4c, 0x50, 0x54, 0x58, 0x5c};

enum scorpion_perf_common {
	SCORPION_EVT_START_IDX			= 0x4c,
	SCORPION_ICACHE_EXPL_INV		= 0x4c,
	SCORPION_ICACHE_MISS			= 0x4d,
	SCORPION_ICACHE_ACCESS			= 0x4e,
	SCORPION_ICACHE_CACHEREQ_L2		= 0x4f,
	SCORPION_ICACHE_NOCACHE_L2		= 0x50,
	SCORPION_HIQUP_NOPED			= 0x51,
	SCORPION_DATA_ABORT			= 0x52,
	SCORPION_IRQ				= 0x53,
	SCORPION_FIQ				= 0x54,
	SCORPION_ALL_EXCPT			= 0x55,
	SCORPION_UNDEF				= 0x56,
	SCORPION_SVC				= 0x57,
	SCORPION_SMC				= 0x58,
	SCORPION_PREFETCH_ABORT			= 0x59,
	SCORPION_INDEX_CHECK			= 0x5a,
	SCORPION_NULL_CHECK			= 0x5b,
	SCORPION_ICIMVAU_IMPL_ICIALLU		= 0x5c,
	SCORPION_NONICIALLU_BTAC_INV		= 0x5d,
	SCORPION_IMPL_ICIALLU			= 0x5e,
	SCORPION_EXPL_ICIALLU			= 0x5f,
	SCORPION_SPIPE_ONLY_CYCLES		= 0x60,
	SCORPION_XPIPE_ONLY_CYCLES		= 0x61,
	SCORPION_DUAL_CYCLES			= 0x62,
	SCORPION_DISPATCH_ANY_CYCLES		= 0x63,
	SCORPION_FIFO_FULLBLK_CMT		= 0x64,
	SCORPION_FAIL_COND_INST			= 0x65,
	SCORPION_PASS_COND_INST			= 0x66,
	SCORPION_ALLOW_VU_CLK			= 0x67,
	SCORPION_VU_IDLE			= 0x68,
	SCORPION_ALLOW_L2_CLK			= 0x69,
	SCORPION_L2_IDLE			= 0x6a,
	SCORPION_DTLB_IMPL_INV_SCTLR_DACR	= 0x6b,
	SCORPION_DTLB_EXPL_INV			= 0x6c,
	SCORPION_DTLB_MISS			= 0x6d,
	SCORPION_DTLB_ACCESS			= 0x6e,
	SCORPION_ITLB_MISS			= 0x6f,
	SCORPION_ITLB_IMPL_INV			= 0x70,
	SCORPION_ITLB_EXPL_INV			= 0x71,
	SCORPION_UTLB_D_MISS			= 0x72,
	SCORPION_UTLB_D_ACCESS			= 0x73,
	SCORPION_UTLB_I_MISS			= 0x74,
	SCORPION_UTLB_I_ACCESS			= 0x75,
	SCORPION_UTLB_INV_ASID			= 0x76,
	SCORPION_UTLB_INV_MVA			= 0x77,
	SCORPION_UTLB_INV_ALL			= 0x78,
	SCORPION_S2_HOLD_RDQ_UNAVAIL		= 0x79,
	SCORPION_S2_HOLD			= 0x7a,
	SCORPION_S2_HOLD_DEV_OP			= 0x7b,
	SCORPION_S2_HOLD_ORDER			= 0x7c,
	SCORPION_S2_HOLD_BARRIER		= 0x7d,
	SCORPION_VIU_DUAL_CYCLE			= 0x7e,
	SCORPION_VIU_SINGLE_CYCLE		= 0x7f,
	SCORPION_VX_PIPE_WAR_STALL_CYCLES	= 0x80,
	SCORPION_VX_PIPE_WAW_STALL_CYCLES	= 0x81,
	SCORPION_VX_PIPE_RAW_STALL_CYCLES	= 0x82,
	SCORPION_VX_PIPE_LOAD_USE_STALL		= 0x83,
	SCORPION_VS_PIPE_WAR_STALL_CYCLES	= 0x84,
	SCORPION_VS_PIPE_WAW_STALL_CYCLES	= 0x85,
	SCORPION_VS_PIPE_RAW_STALL_CYCLES	= 0x86,
	SCORPION_EXCEPTIONS_INV_OPERATION	= 0x87,
	SCORPION_EXCEPTIONS_DIV_BY_ZERO		= 0x88,
	SCORPION_COND_INST_FAIL_VX_PIPE		= 0x89,
	SCORPION_COND_INST_FAIL_VS_PIPE		= 0x8a,
	SCORPION_EXCEPTIONS_OVERFLOW		= 0x8b,
	SCORPION_EXCEPTIONS_UNDERFLOW		= 0x8c,
	SCORPION_EXCEPTIONS_DENORM		= 0x8d,
};

enum scorpion_perf_smp {
	SCORPIONMP_NUM_BARRIERS			= 0x8e,
	SCORPIONMP_BARRIER_CYCLES		= 0x8f,
};

enum scorpion_perf_up {
	SCORPION_BANK_AB_HIT			= 0x8e,
	SCORPION_BANK_AB_ACCESS			= 0x8f,
	SCORPION_BANK_CD_HIT			= 0x90,
	SCORPION_BANK_CD_ACCESS			= 0x91,
	SCORPION_BANK_AB_DSIDE_HIT		= 0x92,
	SCORPION_BANK_AB_DSIDE_ACCESS		= 0x93,
	SCORPION_BANK_CD_DSIDE_HIT		= 0x94,
	SCORPION_BANK_CD_DSIDE_ACCESS		= 0x95,
	SCORPION_BANK_AB_ISIDE_HIT		= 0x96,
	SCORPION_BANK_AB_ISIDE_ACCESS		= 0x97,
	SCORPION_BANK_CD_ISIDE_HIT		= 0x98,
	SCORPION_BANK_CD_ISIDE_ACCESS		= 0x99,
	SCORPION_ISIDE_RD_WAIT			= 0x9a,
	SCORPION_DSIDE_RD_WAIT			= 0x9b,
	SCORPION_BANK_BYPASS_WRITE		= 0x9c,
	SCORPION_BANK_AB_NON_CASTOUT		= 0x9d,
	SCORPION_BANK_AB_L2_CASTOUT		= 0x9e,
	SCORPION_BANK_CD_NON_CASTOUT		= 0x9f,
	SCORPION_BANK_CD_L2_CASTOUT		= 0xa0,
};

static const unsigned armv7_scorpion_perf_map[PERF_COUNT_HW_MAX] = {
	[PERF_COUNT_HW_CPU_CYCLES]	    = ARMV7_PERFCTR_CPU_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]	    = ARMV7_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_CACHE_MISSES]	    = HW_OP_UNSUPPORTED,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = ARMV7_PERFCTR_PC_WRITE,
	[PERF_COUNT_HW_BRANCH_MISSES]	    = ARMV7_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]	    = ARMV7_PERFCTR_CLOCK_CYCLES,
};

static unsigned armv7_scorpion_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					  [PERF_COUNT_HW_CACHE_OP_MAX]
					  [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	[C(L1D)] = {
		/*
		 * The performance counters don't differentiate between read
		 * and write accesses/misses so this isn't strictly correct,
		 * but it's the best we can do. Writes and reads get
		 * combined.
		 */
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_L1_DCACHE_ACCESS,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_L1_DCACHE_REFILL,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_L1_DCACHE_ACCESS,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_L1_DCACHE_REFILL,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(L1I)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= SCORPION_ICACHE_ACCESS,
			[C(RESULT_MISS)]	= SCORPION_ICACHE_MISS,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= SCORPION_ICACHE_ACCESS,
			[C(RESULT_MISS)]	= SCORPION_ICACHE_MISS,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(LL)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(DTLB)] = {
		/*
		 * Only ITLB misses and DTLB refills are supported.
		 * If users want the DTLB refills misses a raw counter
		 * must be used.
		 */
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= SCORPION_DTLB_ACCESS,
			[C(RESULT_MISS)]	= SCORPION_DTLB_MISS,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= SCORPION_DTLB_ACCESS,
			[C(RESULT_MISS)]	= SCORPION_DTLB_MISS,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(ITLB)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= SCORPION_ITLB_MISS,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= SCORPION_ITLB_MISS,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
	[C(BPU)] = {
		[C(OP_READ)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
		},
		[C(OP_WRITE)] = {
			[C(RESULT_ACCESS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
			[C(RESULT_MISS)]
					= ARMV7_PERFCTR_PC_BRANCH_PRED,
		},
		[C(OP_PREFETCH)] = {
			[C(RESULT_ACCESS)]	= CACHE_OP_UNSUPPORTED,
			[C(RESULT_MISS)]	= CACHE_OP_UNSUPPORTED,
		},
	},
};

static int msm_scorpion_map_event(struct perf_event *event)
{
	return map_cpu_event(event, &armv7_scorpion_perf_map,
			&armv7_scorpion_perf_cache_map, 0xfffff);
}


struct scorpion_evt {
	/*
	 * The scorpion_evt_type field corresponds to the actual Scorpion
	 * event codes. These map many-to-one to the armv7 defined codes
	 */
	u32 scorpion_evt_type;

	/*
	 * The group_setval field corresponds to the value that the group
	 * register needs to be set to. This value is deduced from the row
	 * and column that the event belongs to in the event table
	 */
	u32 group_setval;

	/*
	 * The groupcode corresponds to the group that the event belongs to.
	 * Scorpion has 5 groups of events LPM0, LPM1, LPM2, L2LPM and VLPM
	 * going from 0 to 4 in terms of the codes used
	 */
	u8 groupcode;

	/*
	 * The armv7_evt_type field corresponds to the armv7 defined event
	 * code that the Scorpion events map to
	 */
	u32 armv7_evt_type;
};

static const struct scorpion_evt scorpion_event[] = {
	{SCORPION_ICACHE_EXPL_INV,		0x80000500, 0, 0x4d},
	{SCORPION_ICACHE_MISS,			0x80050000, 0, 0x4e},
	{SCORPION_ICACHE_ACCESS,		0x85000000, 0, 0x4f},
	{SCORPION_ICACHE_CACHEREQ_L2,		0x86000000, 0, 0x4f},
	{SCORPION_ICACHE_NOCACHE_L2,		0x87000000, 0, 0x4f},
	{SCORPION_HIQUP_NOPED,			0x80080000, 0, 0x4e},
	{SCORPION_DATA_ABORT,			0x8000000a, 0, 0x4c},
	{SCORPION_IRQ,				0x80000a00, 0, 0x4d},
	{SCORPION_FIQ,				0x800a0000, 0, 0x4e},
	{SCORPION_ALL_EXCPT,			0x8a000000, 0, 0x4f},
	{SCORPION_UNDEF,			0x8000000b, 0, 0x4c},
	{SCORPION_SVC,				0x80000b00, 0, 0x4d},
	{SCORPION_SMC,				0x800b0000, 0, 0x4e},
	{SCORPION_PREFETCH_ABORT,		0x8b000000, 0, 0x4f},
	{SCORPION_INDEX_CHECK,			0x8000000c, 0, 0x4c},
	{SCORPION_NULL_CHECK,			0x80000c00, 0, 0x4d},
	{SCORPION_ICIMVAU_IMPL_ICIALLU,		0x8000000d, 0, 0x4c},
	{SCORPION_NONICIALLU_BTAC_INV,		0x80000d00, 0, 0x4d},
	{SCORPION_IMPL_ICIALLU,			0x800d0000, 0, 0x4e},
	{SCORPION_EXPL_ICIALLU,			0x8d000000, 0, 0x4f},

	{SCORPION_SPIPE_ONLY_CYCLES,		0x80000600, 1, 0x51},
	{SCORPION_XPIPE_ONLY_CYCLES,		0x80060000, 1, 0x52},
	{SCORPION_DUAL_CYCLES,			0x86000000, 1, 0x53},
	{SCORPION_DISPATCH_ANY_CYCLES,		0x89000000, 1, 0x53},
	{SCORPION_FIFO_FULLBLK_CMT,		0x8000000d, 1, 0x50},
	{SCORPION_FAIL_COND_INST,		0x800d0000, 1, 0x52},
	{SCORPION_PASS_COND_INST,		0x8d000000, 1, 0x53},
	{SCORPION_ALLOW_VU_CLK,			0x8000000e, 1, 0x50},
	{SCORPION_VU_IDLE,			0x80000e00, 1, 0x51},
	{SCORPION_ALLOW_L2_CLK,			0x800e0000, 1, 0x52},
	{SCORPION_L2_IDLE,			0x8e000000, 1, 0x53},

	{SCORPION_DTLB_IMPL_INV_SCTLR_DACR,	0x80000001, 2, 0x54},
	{SCORPION_DTLB_EXPL_INV,		0x80000100, 2, 0x55},
	{SCORPION_DTLB_MISS,			0x80010000, 2, 0x56},
	{SCORPION_DTLB_ACCESS,			0x81000000, 2, 0x57},
	{SCORPION_ITLB_MISS,			0x80000200, 2, 0x55},
	{SCORPION_ITLB_IMPL_INV,		0x80020000, 2, 0x56},
	{SCORPION_ITLB_EXPL_INV,		0x82000000, 2, 0x57},
	{SCORPION_UTLB_D_MISS,			0x80000003, 2, 0x54},
	{SCORPION_UTLB_D_ACCESS,		0x80000300, 2, 0x55},
	{SCORPION_UTLB_I_MISS,			0x80030000, 2, 0x56},
	{SCORPION_UTLB_I_ACCESS,		0x83000000, 2, 0x57},
	{SCORPION_UTLB_INV_ASID,		0x80000400, 2, 0x55},
	{SCORPION_UTLB_INV_MVA,			0x80040000, 2, 0x56},
	{SCORPION_UTLB_INV_ALL,			0x84000000, 2, 0x57},
	{SCORPION_S2_HOLD_RDQ_UNAVAIL,		0x80000800, 2, 0x55},
	{SCORPION_S2_HOLD,			0x88000000, 2, 0x57},
	{SCORPION_S2_HOLD_DEV_OP,		0x80000900, 2, 0x55},
	{SCORPION_S2_HOLD_ORDER,		0x80090000, 2, 0x56},
	{SCORPION_S2_HOLD_BARRIER,		0x89000000, 2, 0x57},

	{SCORPION_VIU_DUAL_CYCLE,		0x80000001, 4, 0x5c},
	{SCORPION_VIU_SINGLE_CYCLE,		0x80000100, 4, 0x5d},
	{SCORPION_VX_PIPE_WAR_STALL_CYCLES,	0x80000005, 4, 0x5c},
	{SCORPION_VX_PIPE_WAW_STALL_CYCLES,	0x80000500, 4, 0x5d},
	{SCORPION_VX_PIPE_RAW_STALL_CYCLES,	0x80050000, 4, 0x5e},
	{SCORPION_VX_PIPE_LOAD_USE_STALL,	0x80000007, 4, 0x5c},
	{SCORPION_VS_PIPE_WAR_STALL_CYCLES,	0x80000008, 4, 0x5c},
	{SCORPION_VS_PIPE_WAW_STALL_CYCLES,	0x80000800, 4, 0x5d},
	{SCORPION_VS_PIPE_RAW_STALL_CYCLES,	0x80080000, 4, 0x5e},
	{SCORPION_EXCEPTIONS_INV_OPERATION,	0x8000000b, 4, 0x5c},
	{SCORPION_EXCEPTIONS_DIV_BY_ZERO,	0x80000b00, 4, 0x5d},
	{SCORPION_COND_INST_FAIL_VX_PIPE,	0x800b0000, 4, 0x5e},
	{SCORPION_COND_INST_FAIL_VS_PIPE,	0x8b000000, 4, 0x5f},
	{SCORPION_EXCEPTIONS_OVERFLOW,		0x8000000c, 4, 0x5c},
	{SCORPION_EXCEPTIONS_UNDERFLOW,		0x80000c00, 4, 0x5d},
	{SCORPION_EXCEPTIONS_DENORM,		0x8c000000, 4, 0x5f},

#ifdef CONFIG_MSM_SMP
	{SCORPIONMP_NUM_BARRIERS,		0x80000e00, 3, 0x59},
	{SCORPIONMP_BARRIER_CYCLES,		0x800e0000, 3, 0x5a},
#else
	{SCORPION_BANK_AB_HIT,			0x80000001, 3, 0x58},
	{SCORPION_BANK_AB_ACCESS,		0x80000100, 3, 0x59},
	{SCORPION_BANK_CD_HIT,			0x80010000, 3, 0x5a},
	{SCORPION_BANK_CD_ACCESS,		0x81000000, 3, 0x5b},
	{SCORPION_BANK_AB_DSIDE_HIT,		0x80000002, 3, 0x58},
	{SCORPION_BANK_AB_DSIDE_ACCESS,		0x80000200, 3, 0x59},
	{SCORPION_BANK_CD_DSIDE_HIT,		0x80020000, 3, 0x5a},
	{SCORPION_BANK_CD_DSIDE_ACCESS,		0x82000000, 3, 0x5b},
	{SCORPION_BANK_AB_ISIDE_HIT,		0x80000003, 3, 0x58},
	{SCORPION_BANK_AB_ISIDE_ACCESS,		0x80000300, 3, 0x59},
	{SCORPION_BANK_CD_ISIDE_HIT,		0x80030000, 3, 0x5a},
	{SCORPION_BANK_CD_ISIDE_ACCESS,		0x83000000, 3, 0x5b},
	{SCORPION_ISIDE_RD_WAIT,		0x80000009, 3, 0x58},
	{SCORPION_DSIDE_RD_WAIT,		0x80090000, 3, 0x5a},
	{SCORPION_BANK_BYPASS_WRITE,		0x8000000a, 3, 0x58},
	{SCORPION_BANK_AB_NON_CASTOUT,		0x8000000c, 3, 0x58},
	{SCORPION_BANK_AB_L2_CASTOUT,		0x80000c00, 3, 0x59},
	{SCORPION_BANK_CD_NON_CASTOUT,		0x800c0000, 3, 0x5a},
	{SCORPION_BANK_CD_L2_CASTOUT,		0x8c000000, 3, 0x5b},
#endif
};

static unsigned int get_scorpion_evtinfo(unsigned int scorpion_evt_type,
					struct scorpion_evt *evtinfo)
{
	u32 idx;
	u8 prefix;
	u8 reg;
	u8 code;
	u8 group;

	prefix = (scorpion_evt_type & 0xF0000) >> 16;
	if (prefix == SCORPION_EVT_PREFIX) {
		reg   = (scorpion_evt_type & 0x0F000) >> 12;
		code  = (scorpion_evt_type & 0x00FF0) >> 4;
		group =  scorpion_evt_type & 0x0000F;

		if ((group > 3) || (reg > SCORPION_MAX_L1_REG))
			return -EINVAL;

		evtinfo->group_setval = 0x80000000 | (code << (group * 8));
		evtinfo->groupcode = reg;
		evtinfo->armv7_evt_type = scorpion_evt_type_base[reg] | group;

		return evtinfo->armv7_evt_type;
	}

	if (scorpion_evt_type < SCORPION_EVT_START_IDX || scorpion_evt_type >=
		(ARRAY_SIZE(scorpion_event) + SCORPION_EVT_START_IDX))
		return -EINVAL;

	idx = scorpion_evt_type - SCORPION_EVT_START_IDX;
	if (scorpion_event[idx].scorpion_evt_type == scorpion_evt_type) {
		evtinfo->group_setval = scorpion_event[idx].group_setval;
		evtinfo->groupcode = scorpion_event[idx].groupcode;
		evtinfo->armv7_evt_type = scorpion_event[idx].armv7_evt_type;
		return scorpion_event[idx].armv7_evt_type;
	}
	return -EINVAL;
}

static u32 scorpion_read_lpm0(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c15, c0, 0" : "=r" (val));
	return val;
}

static void scorpion_write_lpm0(u32 val)
{
	asm volatile("mcr p15, 0, %0, c15, c0, 0" : : "r" (val));
}

static u32 scorpion_read_lpm1(void)
{
	u32 val;

	asm volatile("mrc p15, 1, %0, c15, c0, 0" : "=r" (val));
	return val;
}

static void scorpion_write_lpm1(u32 val)
{
	asm volatile("mcr p15, 1, %0, c15, c0, 0" : : "r" (val));
}

static u32 scorpion_read_lpm2(void)
{
	u32 val;

	asm volatile("mrc p15, 2, %0, c15, c0, 0" : "=r" (val));
	return val;
}

static void scorpion_write_lpm2(u32 val)
{
	asm volatile("mcr p15, 2, %0, c15, c0, 0" : : "r" (val));
}

static u32 scorpion_read_l2lpm(void)
{
	u32 val;

	asm volatile("mrc p15, 3, %0, c15, c2, 0" : "=r" (val));
	return val;
}

static void scorpion_write_l2lpm(u32 val)
{
	asm volatile("mcr p15, 3, %0, c15, c2, 0" : : "r" (val));
}

static u32 scorpion_read_vlpm(void)
{
	u32 val;

	asm volatile("mrc p10, 7, %0, c11, c0, 0" : "=r" (val));
	return val;
}

static void scorpion_write_vlpm(u32 val)
{
	asm volatile("mcr p10, 7, %0, c11, c0, 0" : : "r" (val));
}

/*
 * The Scorpion processor supports performance monitoring for Venum unit.
 * In order to access the performance monitor registers corresponding to
 * VFP, CPACR and FPEXC registers need to be set up beforehand.
 * Also, they need to be recovered once the access is done.
 * This is the reason for having pre and post functions
 */

static DEFINE_PER_CPU(u32, venum_orig_val);
static DEFINE_PER_CPU(u32, fp_orig_val);

static void scorpion_pre_vlpm(void)
{
	u32 venum_new_val;
	u32 fp_new_val;
	u32 v_orig_val;
	u32 f_orig_val;

	/* CPACR Enable CP10 and CP11 access */
	v_orig_val = get_copro_access();
	venum_new_val = v_orig_val | CPACC_SVC(10) | CPACC_SVC(11);
	set_copro_access(venum_new_val);
	/* Store orig venum val */
	__get_cpu_var(venum_orig_val) = v_orig_val;

	/* Enable FPEXC */
	f_orig_val = fmrx(FPEXC);
	fp_new_val = f_orig_val | FPEXC_EN;
	fmxr(FPEXC, fp_new_val);
	/* Store orig fp val */
	__get_cpu_var(fp_orig_val) = f_orig_val;
}

static void scorpion_post_vlpm(void)
{
	/* Restore FPEXC */
	fmxr(FPEXC, __get_cpu_var(fp_orig_val));
	isb();
	/* Restore CPACR */
	set_copro_access(__get_cpu_var(venum_orig_val));
}

struct scorpion_access_funcs {
	u32 (*read) (void);
	void (*write) (u32);
	void (*pre) (void);
	void (*post) (void);
};

/*
 * The scorpion_functions array is used to set up the event register codes
 * based on the group to which an event belongs to.
 * Having the following array modularizes the code for doing that.
 */
struct scorpion_access_funcs scorpion_functions[] = {
	{scorpion_read_lpm0, scorpion_write_lpm0, NULL, NULL},
	{scorpion_read_lpm1, scorpion_write_lpm1, NULL, NULL},
	{scorpion_read_lpm2, scorpion_write_lpm2, NULL, NULL},
	{scorpion_read_l2lpm, scorpion_write_l2lpm, NULL, NULL},
	{scorpion_read_vlpm, scorpion_write_vlpm, scorpion_pre_vlpm,
		scorpion_post_vlpm},
};

static inline u32 scorpion_get_columnmask(u32 evt_code)
{
	const u32 columnmasks[] = {0xffffff00, 0xffff00ff, 0xff00ffff,
					0x80ffffff};

	return columnmasks[evt_code & 0x3];
}

static void scorpion_evt_setup(u32 gr, u32 setval, u32 evt_code)
{
	u32 val;

	if (scorpion_functions[gr].pre)
		scorpion_functions[gr].pre();
	val = scorpion_get_columnmask(evt_code) & scorpion_functions[gr].read();
	val = val | setval;
	scorpion_functions[gr].write(val);
	if (scorpion_functions[gr].post)
		scorpion_functions[gr].post();
}

static void scorpion_clear_pmuregs(void)
{
	scorpion_write_lpm0(0);
	scorpion_write_lpm1(0);
	scorpion_write_lpm2(0);
	scorpion_write_l2lpm(0);
	scorpion_pre_vlpm();
	scorpion_write_vlpm(0);
	scorpion_post_vlpm();
}

static void scorpion_clearpmu(u32 grp, u32 val, u32 evt_code)
{
	u32 orig_pmuval, new_pmuval;

	if (scorpion_functions[grp].pre)
		scorpion_functions[grp].pre();
	orig_pmuval = scorpion_functions[grp].read();
	val = val & ~scorpion_get_columnmask(evt_code);
	new_pmuval = orig_pmuval & ~val;
	scorpion_functions[grp].write(new_pmuval);
	if (scorpion_functions[grp].post)
		scorpion_functions[grp].post();
}

static void scorpion_pmu_disable_event(struct hw_perf_event *hwc, int idx)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long event;
	struct scorpion_evt evtinfo;
	struct pmu_hw_events *events = cpu_pmu->get_hw_events();


	/* Disable counter and interrupt */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	/*
	 * Clear lpm code (if destined for PMNx counters)
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV7_IDX_CYCLE_COUNTER) {
		val = hwc->config_base;
		val &= SCORPION_EVTYPE_EVENT;

		if (val > 0x40) {
			event = get_scorpion_evtinfo(val, &evtinfo);
			if (event == -EINVAL)
				goto scorpion_dis_out;
			val = evtinfo.group_setval;
			gr = evtinfo.groupcode;
			scorpion_clearpmu(gr, val, evtinfo.armv7_evt_type);
		}
	}
	/* Disable interrupt for this counter */
	armv7_pmnc_disable_intens(idx);

scorpion_dis_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void scorpion_pmu_enable_event(struct hw_perf_event *hwc,
		int idx, int cpu)
{
	unsigned long flags;
	u32 val = 0;
	u32 gr;
	unsigned long event;
	struct scorpion_evt evtinfo;
	unsigned long long prev_count = local64_read(&hwc->prev_count);
	struct pmu_hw_events *events = cpu_pmu->get_hw_events();

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/* Disable counter */
	armv7_pmnc_disable_counter(idx);

	/*
	 * Set event (if destined for PMNx counters)
	 * We don't need to set the event if it's a cycle count
	 */
	if (idx != ARMV7_IDX_CYCLE_COUNTER) {
		val = hwc->config_base;
		val &= SCORPION_EVTYPE_EVENT;

		if (val < 0x40) {
			armv7_pmnc_write_evtsel(idx, hwc->config_base);
		} else {
			event = get_scorpion_evtinfo(val, &evtinfo);

			if (event == -EINVAL)
				goto scorpion_out;
			/*
			 * Set event (if destined for PMNx counters)
			 * We don't need to set the event if it's a cycle count
			 */
			armv7_pmnc_write_evtsel(idx, event);
			val = 0x0;
			asm volatile("mcr p15, 0, %0, c9, c15, 0" : :
				"r" (val));
			val = evtinfo.group_setval;
			gr = evtinfo.groupcode;
			scorpion_evt_setup(gr, val, evtinfo.armv7_evt_type);
		}
	}

	/* Enable interrupt for this counter */
	armv7_pmnc_enable_intens(idx);

	/* Restore prev val */
	armv7pmu_write_counter(idx, prev_count & COUNT_MASK);

	/* Enable counter */
	armv7_pmnc_enable_counter(idx);

scorpion_out:
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void scorpion_pmu_reset(void *info)
{
	u32 idx, nb_cnt = cpu_pmu->num_events;

	/* Stop all counters and their interrupts */
	for (idx = 1; idx < nb_cnt; ++idx) {
		armv7_pmnc_disable_counter(idx);
		armv7_pmnc_disable_intens(idx);
	}

	/* Clear all pmresrs */
	scorpion_clear_pmuregs();

	/* Reset irq stat reg */
	armv7_pmnc_getreset_flags();

	/* Reset all ctrs to 0 */
	armv7_pmnc_write(ARMV7_PMNC_P | ARMV7_PMNC_C);
}

static struct arm_pmu scorpion_pmu = {
	.handle_irq		= armv7pmu_handle_irq,
	.request_pmu_irq	= msm_request_irq,
	.free_pmu_irq		= msm_free_irq,
	.enable			= scorpion_pmu_enable_event,
	.disable		= scorpion_pmu_disable_event,
	.read_counter		= armv7pmu_read_counter,
	.write_counter		= armv7pmu_write_counter,
	.map_event		= msm_scorpion_map_event,
	.get_event_idx		= armv7pmu_get_event_idx,
	.start			= armv7pmu_start,
	.stop			= armv7pmu_stop,
	.reset			= scorpion_pmu_reset,
	.test_set_event_constraints	= msm_test_set_ev_constraint,
	.clear_event_constraints	= msm_clear_ev_constraint,
	.max_period		= (1LLU << 32) - 1,
};

static struct arm_pmu *__init armv7_scorpion_pmu_init(void)
{
	scorpion_pmu.id		= ARM_PERF_PMU_ID_SCORPION;
	scorpion_pmu.name	= "ARMv7 Scorpion";
	scorpion_pmu.num_events	= armv7_read_num_pmnc_events();
	scorpion_pmu.pmu.attr_groups	= msm_l1_pmu_attr_grps;
	scorpion_clear_pmuregs();
	return &scorpion_pmu;
}

static struct arm_pmu *__init armv7_scorpionmp_pmu_init(void)
{
	scorpion_pmu.id		= ARM_PERF_PMU_ID_SCORPIONMP;
	scorpion_pmu.name	= "ARMv7 Scorpion-MP";
	scorpion_pmu.num_events	= armv7_read_num_pmnc_events();
	scorpion_pmu.pmu.attr_groups	= msm_l1_pmu_attr_grps;
	scorpion_clear_pmuregs();
	return &scorpion_pmu;
}
#else
static struct arm_pmu *__init armv7_scorpion_pmu_init(void)
{
	return NULL;
}
static struct arm_pmu *__init armv7_scorpionmp_pmu_init(void)
{
	return NULL;
}
#endif	/* CONFIG_CPU_V7 */
