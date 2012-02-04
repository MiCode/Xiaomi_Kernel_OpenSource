/*
 * Copyright (c) 2011, 2012 Code Aurora Forum. All rights reserved.
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
#ifdef CONFIG_ARCH_MSM8X60

#include <linux/irq.h>

#define MAX_BB_L2_PERIOD	((1ULL << 32) - 1)
#define MAX_BB_L2_CTRS 5
#define BB_L2CYCLE_CTR_BIT 31
#define BB_L2CYCLE_CTR_EVENT_IDX 4
#define BB_L2CYCLE_CTR_RAW_CODE 0xfe
#define SCORPIONL2_PMNC_E       (1 << 0)	/* Enable all counters */
#define SCORPION_L2_EVT_PREFIX 3
#define SCORPION_MAX_L2_REG 4

/*
 * Lock to protect r/m/w sequences to the L2 PMU.
 */
DEFINE_RAW_SPINLOCK(bb_l2_pmu_lock);

static struct platform_device *bb_l2_pmu_device;

struct hw_bb_l2_pmu {
	struct perf_event *events[MAX_BB_L2_CTRS];
	unsigned long active_mask[BITS_TO_LONGS(MAX_BB_L2_CTRS)];
	raw_spinlock_t lock;
};

struct hw_bb_l2_pmu hw_bb_l2_pmu;

struct bb_l2_scorp_evt {
	u32 evt_type;
	u32 val;
	u8 grp;
	u32 evt_type_act;
};

enum scorpion_perf_types {
	SCORPIONL2_TOTAL_BANK_REQ = 0x90,
	SCORPIONL2_DSIDE_READ = 0x91,
	SCORPIONL2_DSIDE_WRITE = 0x92,
	SCORPIONL2_ISIDE_READ = 0x93,
	SCORPIONL2_L2CACHE_ISIDE_READ = 0x94,
	SCORPIONL2_L2CACHE_BANK_REQ = 0x95,
	SCORPIONL2_L2CACHE_DSIDE_READ = 0x96,
	SCORPIONL2_L2CACHE_DSIDE_WRITE = 0x97,
	SCORPIONL2_L2NOCACHE_DSIDE_WRITE = 0x98,
	SCORPIONL2_L2NOCACHE_ISIDE_READ = 0x99,
	SCORPIONL2_L2NOCACHE_TOTAL_REQ = 0x9a,
	SCORPIONL2_L2NOCACHE_DSIDE_READ = 0x9b,
	SCORPIONL2_DSIDE_READ_NOL1 = 0x9c,
	SCORPIONL2_L2CACHE_WRITETHROUGH = 0x9d,
	SCORPIONL2_BARRIERS = 0x9e,
	SCORPIONL2_HARDWARE_TABLE_WALKS = 0x9f,
	SCORPIONL2_MVA_POC = 0xa0,
	SCORPIONL2_L2CACHE_HW_TABLE_WALKS = 0xa1,
	SCORPIONL2_SETWAY_CACHE_OPS = 0xa2,
	SCORPIONL2_DSIDE_WRITE_HITS = 0xa3,
	SCORPIONL2_ISIDE_READ_HITS = 0xa4,
	SCORPIONL2_CACHE_DSIDE_READ_NOL1 = 0xa5,
	SCORPIONL2_TOTAL_CACHE_HITS = 0xa6,
	SCORPIONL2_CACHE_MATCH_MISS = 0xa7,
	SCORPIONL2_DREAD_HIT_L1_DATA = 0xa8,
	SCORPIONL2_L2LINE_LOCKED = 0xa9,
	SCORPIONL2_HW_TABLE_WALK_HIT = 0xaa,
	SCORPIONL2_CACHE_MVA_POC = 0xab,
	SCORPIONL2_L2ALLOC_DWRITE_MISS = 0xac,
	SCORPIONL2_CORRECTED_TAG_ARRAY = 0xad,
	SCORPIONL2_CORRECTED_DATA_ARRAY = 0xae,
	SCORPIONL2_CORRECTED_REPLACEMENT_ARRAY = 0xaf,
	SCORPIONL2_PMBUS_MPAAF = 0xb0,
	SCORPIONL2_PMBUS_MPWDAF = 0xb1,
	SCORPIONL2_PMBUS_MPBRT = 0xb2,
	SCORPIONL2_CPU0_GRANT = 0xb3,
	SCORPIONL2_CPU1_GRANT = 0xb4,
	SCORPIONL2_CPU0_NOGRANT = 0xb5,
	SCORPIONL2_CPU1_NOGRANT = 0xb6,
	SCORPIONL2_CPU0_LOSING_ARB = 0xb7,
	SCORPIONL2_CPU1_LOSING_ARB = 0xb8,
	SCORPIONL2_SLAVEPORT_NOGRANT = 0xb9,
	SCORPIONL2_SLAVEPORT_BPQ_FULL = 0xba,
	SCORPIONL2_SLAVEPORT_LOSING_ARB = 0xbb,
	SCORPIONL2_SLAVEPORT_GRANT = 0xbc,
	SCORPIONL2_SLAVEPORT_GRANTLOCK = 0xbd,
	SCORPIONL2_L2EM_STREX_PASS = 0xbe,
	SCORPIONL2_L2EM_STREX_FAIL = 0xbf,
	SCORPIONL2_LDREX_RESERVE_L2EM = 0xc0,
	SCORPIONL2_SLAVEPORT_LDREX = 0xc1,
	SCORPIONL2_CPU0_L2EM_CLEARED = 0xc2,
	SCORPIONL2_CPU1_L2EM_CLEARED = 0xc3,
	SCORPIONL2_SLAVEPORT_L2EM_CLEARED = 0xc4,
	SCORPIONL2_CPU0_CLAMPED = 0xc5,
	SCORPIONL2_CPU1_CLAMPED = 0xc6,
	SCORPIONL2_CPU0_WAIT = 0xc7,
	SCORPIONL2_CPU1_WAIT = 0xc8,
	SCORPIONL2_CPU0_NONAMBAS_WAIT = 0xc9,
	SCORPIONL2_CPU1_NONAMBAS_WAIT = 0xca,
	SCORPIONL2_CPU0_DSB_WAIT = 0xcb,
	SCORPIONL2_CPU1_DSB_WAIT = 0xcc,
	SCORPIONL2_AXI_READ = 0xcd,
	SCORPIONL2_AXI_WRITE = 0xce,

	SCORPIONL2_1BEAT_WRITE = 0xcf,
	SCORPIONL2_2BEAT_WRITE = 0xd0,
	SCORPIONL2_4BEAT_WRITE = 0xd1,
	SCORPIONL2_8BEAT_WRITE = 0xd2,
	SCORPIONL2_12BEAT_WRITE = 0xd3,
	SCORPIONL2_16BEAT_WRITE = 0xd4,
	SCORPIONL2_1BEAT_DSIDE_READ = 0xd5,
	SCORPIONL2_2BEAT_DSIDE_READ = 0xd6,
	SCORPIONL2_4BEAT_DSIDE_READ = 0xd7,
	SCORPIONL2_8BEAT_DSIDE_READ = 0xd8,
	SCORPIONL2_CSYS_READ_1BEAT = 0xd9,
	SCORPIONL2_CSYS_READ_2BEAT = 0xda,
	SCORPIONL2_CSYS_READ_4BEAT = 0xdb,
	SCORPIONL2_CSYS_READ_8BEAT = 0xdc,
	SCORPIONL2_4BEAT_IFETCH_READ = 0xdd,
	SCORPIONL2_8BEAT_IFETCH_READ = 0xde,
	SCORPIONL2_CSYS_WRITE_1BEAT = 0xdf,
	SCORPIONL2_CSYS_WRITE_2BEAT = 0xe0,
	SCORPIONL2_AXI_READ_DATA_BEAT = 0xe1,
	SCORPIONL2_AXI_WRITE_EVT1 = 0xe2,
	SCORPIONL2_AXI_WRITE_EVT2 = 0xe3,
	SCORPIONL2_LDREX_REQ = 0xe4,
	SCORPIONL2_STREX_PASS = 0xe5,
	SCORPIONL2_STREX_FAIL = 0xe6,
	SCORPIONL2_CPREAD = 0xe7,
	SCORPIONL2_CPWRITE = 0xe8,
	SCORPIONL2_BARRIER_REQ = 0xe9,
	SCORPIONL2_AXI_READ_SLVPORT = 0xea,
	SCORPIONL2_AXI_WRITE_SLVPORT = 0xeb,
	SCORPIONL2_AXI_READ_SLVPORT_DATABEAT = 0xec,
	SCORPIONL2_AXI_WRITE_SLVPORT_DATABEAT = 0xed,
	SCORPIONL2_SNOOPKILL_PREFILTER = 0xee,
	SCORPIONL2_SNOOPKILL_FILTEROUT = 0xef,
	SCORPIONL2_SNOOPED_IC = 0xf0,
	SCORPIONL2_SNOOPED_BP = 0xf1,
	SCORPIONL2_SNOOPED_BARRIERS = 0xf2,
	SCORPIONL2_SNOOPED_TLB = 0xf3,
	BB_L2_MAX_EVT,
};

static const struct bb_l2_scorp_evt sc_evt[] = {
	{SCORPIONL2_TOTAL_BANK_REQ, 0x80000001, 0, 0x00},
	{SCORPIONL2_DSIDE_READ, 0x80000100, 0, 0x01},
	{SCORPIONL2_DSIDE_WRITE, 0x80010000, 0, 0x02},
	{SCORPIONL2_ISIDE_READ, 0x81000000, 0, 0x03},
	{SCORPIONL2_L2CACHE_ISIDE_READ, 0x80000002, 0, 0x00},
	{SCORPIONL2_L2CACHE_BANK_REQ, 0x80000200, 0, 0x01},
	{SCORPIONL2_L2CACHE_DSIDE_READ, 0x80020000, 0, 0x02},
	{SCORPIONL2_L2CACHE_DSIDE_WRITE, 0x82000000, 0, 0x03},
	{SCORPIONL2_L2NOCACHE_DSIDE_WRITE, 0x80000003, 0, 0x00},
	{SCORPIONL2_L2NOCACHE_ISIDE_READ, 0x80000300, 0, 0x01},
	{SCORPIONL2_L2NOCACHE_TOTAL_REQ, 0x80030000, 0, 0x02},
	{SCORPIONL2_L2NOCACHE_DSIDE_READ, 0x83000000, 0, 0x03},
	{SCORPIONL2_DSIDE_READ_NOL1, 0x80000004, 0, 0x00},
	{SCORPIONL2_L2CACHE_WRITETHROUGH, 0x80000400, 0, 0x01},
	{SCORPIONL2_BARRIERS, 0x84000000, 0, 0x03},
	{SCORPIONL2_HARDWARE_TABLE_WALKS, 0x80000005, 0, 0x00},
	{SCORPIONL2_MVA_POC, 0x80000500, 0, 0x01},
	{SCORPIONL2_L2CACHE_HW_TABLE_WALKS, 0x80050000, 0, 0x02},
	{SCORPIONL2_SETWAY_CACHE_OPS, 0x85000000, 0, 0x03},
	{SCORPIONL2_DSIDE_WRITE_HITS, 0x80000006, 0, 0x00},
	{SCORPIONL2_ISIDE_READ_HITS, 0x80000600, 0, 0x01},
	{SCORPIONL2_CACHE_DSIDE_READ_NOL1, 0x80060000, 0, 0x02},
	{SCORPIONL2_TOTAL_CACHE_HITS, 0x86000000, 0, 0x03},
	{SCORPIONL2_CACHE_MATCH_MISS, 0x80000007, 0, 0x00},
	{SCORPIONL2_DREAD_HIT_L1_DATA, 0x87000000, 0, 0x03},
	{SCORPIONL2_L2LINE_LOCKED, 0x80000008, 0, 0x00},
	{SCORPIONL2_HW_TABLE_WALK_HIT, 0x80000800, 0, 0x01},
	{SCORPIONL2_CACHE_MVA_POC, 0x80080000, 0, 0x02},
	{SCORPIONL2_L2ALLOC_DWRITE_MISS, 0x88000000, 0, 0x03},
	{SCORPIONL2_CORRECTED_TAG_ARRAY, 0x80001A00, 0, 0x01},
	{SCORPIONL2_CORRECTED_DATA_ARRAY, 0x801A0000, 0, 0x02},
	{SCORPIONL2_CORRECTED_REPLACEMENT_ARRAY, 0x9A000000, 0, 0x03},
	{SCORPIONL2_PMBUS_MPAAF, 0x80001C00, 0, 0x01},
	{SCORPIONL2_PMBUS_MPWDAF, 0x801C0000, 0, 0x02},
	{SCORPIONL2_PMBUS_MPBRT, 0x9C000000, 0, 0x03},

	{SCORPIONL2_CPU0_GRANT, 0x80000001, 1, 0x04},
	{SCORPIONL2_CPU1_GRANT, 0x80000100, 1, 0x05},
	{SCORPIONL2_CPU0_NOGRANT, 0x80020000, 1, 0x06},
	{SCORPIONL2_CPU1_NOGRANT, 0x82000000, 1, 0x07},
	{SCORPIONL2_CPU0_LOSING_ARB, 0x80040000, 1, 0x06},
	{SCORPIONL2_CPU1_LOSING_ARB, 0x84000000, 1, 0x07},
	{SCORPIONL2_SLAVEPORT_NOGRANT, 0x80000007, 1, 0x04},
	{SCORPIONL2_SLAVEPORT_BPQ_FULL, 0x80000700, 1, 0x05},
	{SCORPIONL2_SLAVEPORT_LOSING_ARB, 0x80070000, 1, 0x06},
	{SCORPIONL2_SLAVEPORT_GRANT, 0x87000000, 1, 0x07},
	{SCORPIONL2_SLAVEPORT_GRANTLOCK, 0x80000008, 1, 0x04},
	{SCORPIONL2_L2EM_STREX_PASS, 0x80000009, 1, 0x04},
	{SCORPIONL2_L2EM_STREX_FAIL, 0x80000900, 1, 0x05},
	{SCORPIONL2_LDREX_RESERVE_L2EM, 0x80090000, 1, 0x06},
	{SCORPIONL2_SLAVEPORT_LDREX, 0x89000000, 1, 0x07},
	{SCORPIONL2_CPU0_L2EM_CLEARED, 0x800A0000, 1, 0x06},
	{SCORPIONL2_CPU1_L2EM_CLEARED, 0x8A000000, 1, 0x07},
	{SCORPIONL2_SLAVEPORT_L2EM_CLEARED, 0x80000B00, 1, 0x05},
	{SCORPIONL2_CPU0_CLAMPED, 0x8000000E, 1, 0x04},
	{SCORPIONL2_CPU1_CLAMPED, 0x80000E00, 1, 0x05},
	{SCORPIONL2_CPU0_WAIT, 0x800F0000, 1, 0x06},
	{SCORPIONL2_CPU1_WAIT, 0x8F000000, 1, 0x07},
	{SCORPIONL2_CPU0_NONAMBAS_WAIT, 0x80000010, 1, 0x04},
	{SCORPIONL2_CPU1_NONAMBAS_WAIT, 0x80001000, 1, 0x05},
	{SCORPIONL2_CPU0_DSB_WAIT, 0x80000014, 1, 0x04},
	{SCORPIONL2_CPU1_DSB_WAIT, 0x80001400, 1, 0x05},

	{SCORPIONL2_AXI_READ, 0x80000001, 2, 0x08},
	{SCORPIONL2_AXI_WRITE, 0x80000100, 2, 0x09},
	{SCORPIONL2_1BEAT_WRITE, 0x80010000, 2, 0x0a},
	{SCORPIONL2_2BEAT_WRITE, 0x80010000, 2, 0x0b},
	{SCORPIONL2_4BEAT_WRITE, 0x80000002, 2, 0x08},
	{SCORPIONL2_8BEAT_WRITE, 0x80000200, 2, 0x09},
	{SCORPIONL2_12BEAT_WRITE, 0x80020000, 2, 0x0a},
	{SCORPIONL2_16BEAT_WRITE, 0x82000000, 2, 0x0b},
	{SCORPIONL2_1BEAT_DSIDE_READ, 0x80000003, 2, 0x08},
	{SCORPIONL2_2BEAT_DSIDE_READ, 0x80000300, 2, 0x09},
	{SCORPIONL2_4BEAT_DSIDE_READ, 0x80030000, 2, 0x0a},
	{SCORPIONL2_8BEAT_DSIDE_READ, 0x83000000, 2, 0x0b},
	{SCORPIONL2_CSYS_READ_1BEAT, 0x80000004, 2, 0x08},
	{SCORPIONL2_CSYS_READ_2BEAT, 0x80000400, 2, 0x09},
	{SCORPIONL2_CSYS_READ_4BEAT, 0x80040000, 2, 0x0a},
	{SCORPIONL2_CSYS_READ_8BEAT, 0x84000000, 2, 0x0b},
	{SCORPIONL2_4BEAT_IFETCH_READ, 0x80000005, 2, 0x08},
	{SCORPIONL2_8BEAT_IFETCH_READ, 0x80000500, 2, 0x09},
	{SCORPIONL2_CSYS_WRITE_1BEAT, 0x80050000, 2, 0x0a},
	{SCORPIONL2_CSYS_WRITE_2BEAT, 0x85000000, 2, 0x0b},
	{SCORPIONL2_AXI_READ_DATA_BEAT, 0x80000600, 2, 0x09},
	{SCORPIONL2_AXI_WRITE_EVT1, 0x80060000, 2, 0x0a},
	{SCORPIONL2_AXI_WRITE_EVT2, 0x86000000, 2, 0x0b},
	{SCORPIONL2_LDREX_REQ, 0x80000007, 2, 0x08},
	{SCORPIONL2_STREX_PASS, 0x80000700, 2, 0x09},
	{SCORPIONL2_STREX_FAIL, 0x80070000, 2, 0x0a},
	{SCORPIONL2_CPREAD, 0x80000008, 2, 0x08},
	{SCORPIONL2_CPWRITE, 0x80000800, 2, 0x09},
	{SCORPIONL2_BARRIER_REQ, 0x88000000, 2, 0x0b},

	{SCORPIONL2_AXI_READ_SLVPORT, 0x80000001, 3, 0x0c},
	{SCORPIONL2_AXI_WRITE_SLVPORT, 0x80000100, 3, 0x0d},
	{SCORPIONL2_AXI_READ_SLVPORT_DATABEAT, 0x80010000, 3, 0x0e},
	{SCORPIONL2_AXI_WRITE_SLVPORT_DATABEAT, 0x81000000, 3, 0x0f},

	{SCORPIONL2_SNOOPKILL_PREFILTER, 0x80000001, 4, 0x10},
	{SCORPIONL2_SNOOPKILL_FILTEROUT, 0x80000100, 4, 0x11},
	{SCORPIONL2_SNOOPED_IC, 0x80000002, 4, 0x10},
	{SCORPIONL2_SNOOPED_BP, 0x80000200, 4, 0x11},
	{SCORPIONL2_SNOOPED_BARRIERS, 0x80020000, 4, 0x12},
	{SCORPIONL2_SNOOPED_TLB, 0x82000000, 4, 0x13},
};

static u32 bb_l2_read_l2pm0(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 0" : "=r" (val));
	return val;
}

static void bb_l2_write_l2pm0(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 0" : : "r" (val));
}

static u32 bb_l2_read_l2pm1(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 1" : "=r" (val));
	return val;
}

static void bb_l2_write_l2pm1(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 1" : : "r" (val));
}

static u32 bb_l2_read_l2pm2(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 2" : "=r" (val));
	return val;
}

static void bb_l2_write_l2pm2(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 2" : : "r" (val));
}

static u32 bb_l2_read_l2pm3(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 3" : "=r" (val));
	return val;
}

static void bb_l2_write_l2pm3(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 3" : : "r" (val));
}

static u32 bb_l2_read_l2pm4(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 4" : "=r" (val));
	return val;
}

static void bb_l2_write_l2pm4(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 4" : : "r" (val));
}

struct bb_scorpion_access_funcs {
	u32(*read) (void);
	void (*write) (u32);
	void (*pre) (void);
	void (*post) (void);
};

struct bb_scorpion_access_funcs bb_l2_func[] = {
	{bb_l2_read_l2pm0, bb_l2_write_l2pm0, NULL, NULL},
	{bb_l2_read_l2pm1, bb_l2_write_l2pm1, NULL, NULL},
	{bb_l2_read_l2pm2, bb_l2_write_l2pm2, NULL, NULL},
	{bb_l2_read_l2pm3, bb_l2_write_l2pm3, NULL, NULL},
	{bb_l2_read_l2pm4, bb_l2_write_l2pm4, NULL, NULL},
};

#define COLMN0MASK 0x000000ff
#define COLMN1MASK 0x0000ff00
#define COLMN2MASK 0x00ff0000

static u32 bb_l2_get_columnmask(u32 setval)
{
	if (setval & COLMN0MASK)
		return 0xffffff00;
	else if (setval & COLMN1MASK)
		return 0xffff00ff;
	else if (setval & COLMN2MASK)
		return 0xff00ffff;
	else
		return 0x80ffffff;
}

static void bb_l2_evt_setup(u32 gr, u32 setval)
{
	u32 val;
	if (bb_l2_func[gr].pre)
		bb_l2_func[gr].pre();
	val = bb_l2_get_columnmask(setval) & bb_l2_func[gr].read();
	val = val | setval;
	bb_l2_func[gr].write(val);
	if (bb_l2_func[gr].post)
		bb_l2_func[gr].post();
}

#define BB_L2_EVT_START_IDX 0x90
#define BB_L2_INV_EVTYPE 0

static unsigned int get_bb_l2_evtinfo(unsigned int evt_type,
				      struct bb_l2_scorp_evt *evtinfo)
{
	u32 idx;
	u8 prefix;
	u8 reg;
	u8 code;
	u8 group;

	prefix = (evt_type & 0xF0000) >> 16;
	if (prefix == SCORPION_L2_EVT_PREFIX) {
		reg   = (evt_type & 0x0F000) >> 12;
		code  = (evt_type & 0x00FF0) >> 4;
		group =  evt_type & 0x0000F;

		if ((group > 3) || (reg > SCORPION_MAX_L2_REG))
			return BB_L2_INV_EVTYPE;

		evtinfo->val = 0x80000000 | (code << (group * 8));
		evtinfo->grp = reg;
		evtinfo->evt_type_act = group | (reg << 2);
		return evtinfo->evt_type_act;
	}

	if (evt_type < BB_L2_EVT_START_IDX || evt_type >= BB_L2_MAX_EVT)
		return BB_L2_INV_EVTYPE;
	idx = evt_type - BB_L2_EVT_START_IDX;
	if (sc_evt[idx].evt_type == evt_type) {
		evtinfo->val = sc_evt[idx].val;
		evtinfo->grp = sc_evt[idx].grp;
		evtinfo->evt_type_act = sc_evt[idx].evt_type_act;
		return sc_evt[idx].evt_type_act;
	}
	return BB_L2_INV_EVTYPE;
}

static inline void bb_l2_pmnc_write(unsigned long val)
{
	val &= 0xff;
	asm volatile ("mcr p15, 3, %0, c15, c4, 0" : : "r" (val));
}

static inline unsigned long bb_l2_pmnc_read(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c4, 0" : "=r" (val));
	return val;
}

static void bb_l2_set_evcntcr(void)
{
	u32 val = 0x0;
	asm volatile ("mcr p15, 3, %0, c15, c6, 4" : : "r" (val));
}

static inline void bb_l2_set_evtyper(int ctr, int val)
{
	/* select ctr */
	asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (ctr));

	/* write into EVTYPER */
	asm volatile ("mcr p15, 3, %0, c15, c6, 7" : : "r" (val));
}

static void bb_l2_set_evfilter_task_mode(void)
{
	u32 filter_val = 0x000f0030 | 1 << smp_processor_id();

	asm volatile ("mcr p15, 3, %0, c15, c6, 3" : : "r" (filter_val));
}

static void bb_l2_set_evfilter_sys_mode(void)
{
	u32 filter_val = 0x000f003f;

	asm volatile ("mcr p15, 3, %0, c15, c6, 3" : : "r" (filter_val));
}

static void bb_l2_enable_intenset(u32 idx)
{
	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mcr p15, 3, %0, c15, c5, 1" : : "r"
			      (1 << BB_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c5, 1" : : "r" (1 << idx));
	}
}

static void bb_l2_disable_intenclr(u32 idx)
{
	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mcr p15, 3, %0, c15, c5, 0" : : "r"
			      (1 << BB_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c5, 0" : : "r" (1 << idx));
	}
}

static void bb_l2_enable_counter(u32 idx)
{
	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 3" : : "r"
			      (1 << BB_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c4, 3" : : "r" (1 << idx));
	}
}

static void bb_l2_disable_counter(u32 idx)
{
	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 2" : : "r"
			      (1 << BB_L2CYCLE_CTR_BIT));

	} else {
		asm volatile ("mcr p15, 3, %0, c15, c4, 2" : : "r" (1 << idx));
	}
}

static u64 bb_l2_read_counter(u32 idx)
{
	u32 val;
	unsigned long flags;

	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mrc p15, 3, %0, c15, c4, 5" : "=r" (val));
	} else {
		raw_spin_lock_irqsave(&bb_l2_pmu_lock, flags);
		asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (idx));

		/* read val from counter */
		asm volatile ("mrc p15, 3, %0, c15, c6, 5" : "=r" (val));
		raw_spin_unlock_irqrestore(&bb_l2_pmu_lock, flags);
	}

	return val;
}

static void bb_l2_write_counter(u32 idx, u32 val)
{
	unsigned long flags;

	if (idx == BB_L2CYCLE_CTR_EVENT_IDX) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 5" : : "r" (val));
	} else {
		raw_spin_lock_irqsave(&bb_l2_pmu_lock, flags);
		/* select counter */
		asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (idx));

		/* write val into counter */
		asm volatile ("mcr p15, 3, %0, c15, c6, 5" : : "r" (val));
		raw_spin_unlock_irqrestore(&bb_l2_pmu_lock, flags);
	}
}

static int
bb_pmu_event_set_period(struct perf_event *event,
			struct hw_perf_event *hwc, int idx)
{
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int ret = 0;

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (left > (s64) MAX_BB_L2_PERIOD)
		left = MAX_BB_L2_PERIOD;

	local64_set(&hwc->prev_count, (u64)-left);

	bb_l2_write_counter(idx, (u64) (-left) & 0xffffffff);

	perf_event_update_userpage(event);

	return ret;
}

static u64
bb_pmu_event_update(struct perf_event *event, struct hw_perf_event *hwc,
		    int idx, int overflow)
{
	u64 prev_raw_count, new_raw_count;
	u64 delta;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = bb_l2_read_counter(idx);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			    new_raw_count) != prev_raw_count)
		goto again;

	new_raw_count &= MAX_BB_L2_PERIOD;
	prev_raw_count &= MAX_BB_L2_PERIOD;

	if (overflow) {
		delta = MAX_BB_L2_PERIOD - prev_raw_count + new_raw_count;
		pr_err("%s: delta: %lld\n", __func__, delta);
	} else
		delta = new_raw_count - prev_raw_count;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	pr_debug("%s: new: %lld, prev: %lld, event: %ld count: %lld\n",
		 __func__, new_raw_count, prev_raw_count,
		 hwc->config_base, local64_read(&event->count));

	return new_raw_count;
}

static void bb_l2_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	bb_pmu_event_update(event, hwc, hwc->idx, 0);
}

static void bb_l2_stop_counter(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (!(hwc->state & PERF_HES_STOPPED)) {
		bb_l2_disable_intenclr(idx);
		bb_l2_disable_counter(idx);

		bb_pmu_event_update(event, hwc, idx, 0);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}

	pr_debug("%s: event: %ld ctr: %d stopped\n", __func__, hwc->config_base,
		 idx);
}

static void bb_l2_start_counter(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	struct bb_l2_scorp_evt evtinfo;
	int evtype = hwc->config_base;
	int ev_typer;
	unsigned long iflags;
	int cpu_id = smp_processor_id();

	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;

	bb_pmu_event_set_period(event, hwc, idx);

	if (hwc->config_base == BB_L2CYCLE_CTR_RAW_CODE)
		goto out;

	memset(&evtinfo, 0, sizeof(evtinfo));

	ev_typer = get_bb_l2_evtinfo(evtype, &evtinfo);

	raw_spin_lock_irqsave(&bb_l2_pmu_lock, iflags);

	bb_l2_set_evtyper(idx, ev_typer);

	bb_l2_set_evcntcr();

	if (event->cpu < 0)
		bb_l2_set_evfilter_task_mode();
	else
		bb_l2_set_evfilter_sys_mode();

	bb_l2_evt_setup(evtinfo.grp, evtinfo.val);

	raw_spin_unlock_irqrestore(&bb_l2_pmu_lock, iflags);

out:

	bb_l2_enable_intenset(idx);

	bb_l2_enable_counter(idx);

	pr_debug("%s: idx: %d, event: %d, val: %x, cpu: %d\n",
		 __func__, idx, evtype, evtinfo.val, cpu_id);
}

static void bb_l2_del_event(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	unsigned long iflags;

	raw_spin_lock_irqsave(&hw_bb_l2_pmu.lock, iflags);

	clear_bit(idx, (long unsigned int *)(&hw_bb_l2_pmu.active_mask));

	bb_l2_stop_counter(event, PERF_EF_UPDATE);
	hw_bb_l2_pmu.events[idx] = NULL;
	hwc->idx = -1;

	raw_spin_unlock_irqrestore(&hw_bb_l2_pmu.lock, iflags);

	pr_debug("%s: event: %ld deleted\n", __func__, hwc->config_base);

	perf_event_update_userpage(event);
}

static int bb_l2_add_event(struct perf_event *event, int flags)
{
	int ctr = 0;
	struct hw_perf_event *hwc = &event->hw;
	unsigned long iflags;
	int err = 0;

	perf_pmu_disable(event->pmu);

	raw_spin_lock_irqsave(&hw_bb_l2_pmu.lock, iflags);

	/* Cycle counter has a resrvd index */
	if (hwc->config_base == BB_L2CYCLE_CTR_RAW_CODE) {
		if (hw_bb_l2_pmu.events[BB_L2CYCLE_CTR_EVENT_IDX]) {
			pr_err("%s: Stale cycle ctr event ptr !\n", __func__);
			err = -EINVAL;
			goto out;
		}
		hwc->idx = BB_L2CYCLE_CTR_EVENT_IDX;
		hw_bb_l2_pmu.events[BB_L2CYCLE_CTR_EVENT_IDX] = event;
		set_bit(BB_L2CYCLE_CTR_EVENT_IDX,
			(long unsigned int *)&hw_bb_l2_pmu.active_mask);
		goto skip_ctr_loop;
	}

	for (ctr = 0; ctr < MAX_BB_L2_CTRS - 1; ctr++) {
		if (!hw_bb_l2_pmu.events[ctr]) {
			hwc->idx = ctr;
			hw_bb_l2_pmu.events[ctr] = event;
			set_bit(ctr, (long unsigned int *)
				&hw_bb_l2_pmu.active_mask);
			break;
		}
	}

	if (hwc->idx < 0) {
		err = -ENOSPC;
		pr_err("%s: No space for event: %llx!!\n", __func__,
		       event->attr.config);
		goto out;
	}

skip_ctr_loop:

	bb_l2_disable_counter(hwc->idx);

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;

	if (flags & PERF_EF_START)
		bb_l2_start_counter(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	pr_debug("%s: event: %ld, ctr: %d added from cpu:%d\n",
		 __func__, hwc->config_base, hwc->idx, smp_processor_id());
out:
	raw_spin_unlock_irqrestore(&hw_bb_l2_pmu.lock, iflags);

	/* Resume the PMU even if this event could not be added */
	perf_pmu_enable(event->pmu);

	return err;
}

static void bb_l2_pmu_enable(struct pmu *pmu)
{
	unsigned long flags;
	isb();
	raw_spin_lock_irqsave(&bb_l2_pmu_lock, flags);
	/* Enable all counters */
	bb_l2_pmnc_write(bb_l2_pmnc_read() | SCORPIONL2_PMNC_E);
	raw_spin_unlock_irqrestore(&bb_l2_pmu_lock, flags);
}

static void bb_l2_pmu_disable(struct pmu *pmu)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&bb_l2_pmu_lock, flags);
	/* Disable all counters */
	bb_l2_pmnc_write(bb_l2_pmnc_read() & ~SCORPIONL2_PMNC_E);
	raw_spin_unlock_irqrestore(&bb_l2_pmu_lock, flags);
	isb();
}

static inline u32 bb_l2_get_reset_pmovsr(void)
{
	u32 val;

	/* Read */
	asm volatile ("mrc p15, 3, %0, c15, c4, 1" : "=r" (val));

	/* Write to clear flags */
	val &= 0xffffffff;
	asm volatile ("mcr p15, 3, %0, c15, c4, 1" : : "r" (val));

	return val;
}

static irqreturn_t bb_l2_handle_irq(int irq_num, void *dev)
{
	unsigned long pmovsr;
	struct perf_sample_data data;
	struct pt_regs *regs;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int bitp;
	int idx = 0;

	pmovsr = bb_l2_get_reset_pmovsr();

	if (!(pmovsr & 0xffffffff))
		return IRQ_NONE;

	regs = get_irq_regs();

	perf_sample_data_init(&data, 0);

	raw_spin_lock(&hw_bb_l2_pmu.lock);

	while (pmovsr) {
		bitp = __ffs(pmovsr);

		if (bitp == BB_L2CYCLE_CTR_BIT)
			idx = BB_L2CYCLE_CTR_EVENT_IDX;
		else
			idx = bitp;

		event = hw_bb_l2_pmu.events[idx];

		if (!event)
			goto next;

		if (!test_bit(idx, hw_bb_l2_pmu.active_mask))
			goto next;

		hwc = &event->hw;
		bb_pmu_event_update(event, hwc, idx, 1);
		data.period = event->hw.last_period;

		if (!bb_pmu_event_set_period(event, hwc, idx))
			goto next;

		if (perf_event_overflow(event, 0, &data, regs))
			bb_l2_disable_counter(hwc->idx);
next:
		pmovsr &= (pmovsr - 1);
	}

	raw_spin_unlock(&hw_bb_l2_pmu.lock);

	irq_work_run();

	return IRQ_HANDLED;
}

static atomic_t active_bb_l2_events = ATOMIC_INIT(0);
static DEFINE_MUTEX(bb_pmu_reserve_mutex);

static int bb_pmu_reserve_hardware(void)
{
	int i, err = -ENODEV, irq;

	bb_l2_pmu_device = reserve_pmu(ARM_PMU_DEVICE_L2);

	if (IS_ERR(bb_l2_pmu_device)) {
		pr_warning("unable to reserve pmu\n");
		return PTR_ERR(bb_l2_pmu_device);
	}

	if (bb_l2_pmu_device->num_resources < 1) {
		pr_err("no irqs for PMUs defined\n");
		return -ENODEV;
	}

	if (strncmp(bb_l2_pmu_device->name, "l2-arm-pmu", 6)) {
		pr_err("Incorrect pdev reserved !\n");
		return -EINVAL;
	}

	for (i = 0; i < bb_l2_pmu_device->num_resources; ++i) {
		irq = platform_get_irq(bb_l2_pmu_device, i);
		if (irq < 0)
			continue;

		err = request_irq(irq, bb_l2_handle_irq,
				  IRQF_DISABLED | IRQF_NOBALANCING,
				  "bb-l2-pmu", NULL);
		if (err) {
			pr_warning("unable to request IRQ%d for Krait L2 perf "
				   "counters\n", irq);
			break;
		}

		irq_get_chip(irq)->irq_unmask(irq_get_irq_data(irq));
	}

	if (err) {
		for (i = i - 1; i >= 0; --i) {
			irq = platform_get_irq(bb_l2_pmu_device, i);
			if (irq >= 0)
				free_irq(irq, NULL);
		}
		release_pmu(bb_l2_pmu_device);
		bb_l2_pmu_device = NULL;
	}

	return err;
}

static void bb_pmu_release_hardware(void)
{
	int i, irq;

	for (i = bb_l2_pmu_device->num_resources - 1; i >= 0; --i) {
		irq = platform_get_irq(bb_l2_pmu_device, i);
		if (irq >= 0)
			free_irq(irq, NULL);
	}

	bb_l2_pmu_disable(NULL);

	release_pmu(bb_l2_pmu_device);
	bb_l2_pmu_device = NULL;
}

static void bb_pmu_perf_event_destroy(struct perf_event *event)
{
	if (atomic_dec_and_mutex_lock
	    (&active_bb_l2_events, &bb_pmu_reserve_mutex)) {
		bb_pmu_release_hardware();
		mutex_unlock(&bb_pmu_reserve_mutex);
	}
}

static int bb_l2_event_init(struct perf_event *event)
{
	int err = 0;
	struct hw_perf_event *hwc = &event->hw;
	int status = 0;

	switch (event->attr.type) {
	case PERF_TYPE_SHARED:
		break;

	default:
		return -ENOENT;
	}

	hwc->idx = -1;

	event->destroy = bb_pmu_perf_event_destroy;

	if (!atomic_inc_not_zero(&active_bb_l2_events)) {
		/* 0 active events */
		mutex_lock(&bb_pmu_reserve_mutex);
		err = bb_pmu_reserve_hardware();
		mutex_unlock(&bb_pmu_reserve_mutex);
		if (!err)
			atomic_inc(&active_bb_l2_events);
		else
			return err;
	}

	hwc->config = 0;
	hwc->event_base = 0;

	/* Check if we came via perf default syms */
	if (event->attr.config == PERF_COUNT_HW_L2_CYCLES)
		hwc->config_base = BB_L2CYCLE_CTR_RAW_CODE;
	else
		hwc->config_base = event->attr.config;

	/* Only one CPU can control the cycle counter */
	if (hwc->config_base == BB_L2CYCLE_CTR_RAW_CODE) {
		/* Check if its already running */
		asm volatile ("mrc p15, 3, %0, c15, c4, 6" : "=r" (status));
		if (status == 0x2) {
			err = -ENOSPC;
			goto out;
		}
	}

	if (!hwc->sample_period) {
		hwc->sample_period = MAX_BB_L2_PERIOD;
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	pr_debug("%s: event: %lld init'd\n", __func__, event->attr.config);

out:
	if (err < 0)
		bb_pmu_perf_event_destroy(event);

	return err;
}

static struct pmu bb_l2_pmu = {
	.pmu_enable = bb_l2_pmu_enable,
	.pmu_disable = bb_l2_pmu_disable,
	.event_init = bb_l2_event_init,
	.add = bb_l2_add_event,
	.del = bb_l2_del_event,
	.start = bb_l2_start_counter,
	.stop = bb_l2_stop_counter,
	.read = bb_l2_read,
};

static const struct arm_pmu *__init scorpionmp_l2_pmu_init(void)
{
	/* Register our own PMU here */
	perf_pmu_register(&bb_l2_pmu, "BB L2", PERF_TYPE_SHARED);

	memset(&hw_bb_l2_pmu, 0, sizeof(hw_bb_l2_pmu));

	/* Avoid spurious interrupts at startup */
	bb_l2_get_reset_pmovsr();

	raw_spin_lock_init(&hw_bb_l2_pmu.lock);

	/* Don't return an arm_pmu here */
	return NULL;
}
#else

static const struct arm_pmu *__init scorpionmp_l2_pmu_init(void)
{
	return NULL;
}

#endif
