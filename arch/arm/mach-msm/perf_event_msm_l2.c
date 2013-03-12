/*
 * Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
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
#include <linux/irq.h>
#include <asm/pmu.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>


#define MAX_SCORPION_L2_CTRS 10

#define SCORPION_L2CYCLE_CTR_BIT 31
#define SCORPION_L2CYCLE_CTR_RAW_CODE 0xfe
#define SCORPIONL2_PMNC_E       (1 << 0)	/* Enable all counters */
#define SCORPION_L2_EVT_PREFIX 3
#define SCORPION_MAX_L2_REG 4

#define L2_EVT_MASK 0xfffff
#define L2_EVT_PREFIX_MASK 0xf0000
#define L2_EVT_PREFIX_SHIFT 16
#define L2_SLAVE_EVT_PREFIX 4

#define PMCR_NUM_EV_SHIFT 11
#define PMCR_NUM_EV_MASK 0x1f

/*
 * The L2 PMU is shared between all CPU's, so protect
 * its bitmap access.
 */
struct pmu_constraints {
	u64 pmu_bitmap;
	u8 codes[64];
	raw_spinlock_t lock;
} l2_pmu_constraints = {
	.pmu_bitmap = 0,
	.codes = {-1},
	.lock = __RAW_SPIN_LOCK_UNLOCKED(l2_pmu_constraints.lock),
};

/* NRCCG format for perf RAW codes. */
PMU_FORMAT_ATTR(l2_prefix,	"config:16-19");
PMU_FORMAT_ATTR(l2_reg,		"config:12-15");
PMU_FORMAT_ATTR(l2_code,	"config:4-11");
PMU_FORMAT_ATTR(l2_grp,		"config:0-3");

static struct attribute *msm_l2_ev_formats[] = {
	&format_attr_l2_prefix.attr,
	&format_attr_l2_reg.attr,
	&format_attr_l2_code.attr,
	&format_attr_l2_grp.attr,
	NULL,
};

/*
 * Format group is essential to access PMU's from userspace
 * via their .name field.
 */
static struct attribute_group msm_l2_pmu_format_group = {
	.name = "format",
	.attrs = msm_l2_ev_formats,
};

static const struct attribute_group *msm_l2_pmu_attr_grps[] = {
	&msm_l2_pmu_format_group,
	NULL,
};

static u32 total_l2_ctrs;
static u32 l2_cycle_ctr_idx;

static u32 pmu_type;

static struct arm_pmu scorpion_l2_pmu;

static u32 l2_orig_filter_prefix = 0x000f0030;

/* L2 slave port traffic filtering */
static u32 l2_slv_filter_prefix = 0x000f0010;

static struct perf_event *l2_events[MAX_SCORPION_L2_CTRS];
static unsigned long l2_used_mask[BITS_TO_LONGS(MAX_SCORPION_L2_CTRS)];

static struct pmu_hw_events scorpion_l2_pmu_hw_events = {
	.events = l2_events,
	.used_mask = l2_used_mask,
	.pmu_lock =
		__RAW_SPIN_LOCK_UNLOCKED(scorpion_l2_pmu_hw_events.pmu_lock),
};

struct scorpion_l2_scorp_evt {
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
	SCORPION_L2_MAX_EVT,
};

static const struct scorpion_l2_scorp_evt sc_evt[] = {
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

static struct pmu_hw_events *scorpion_l2_get_hw_events(void)
{
	return &scorpion_l2_pmu_hw_events;
}
static u32 scorpion_l2_read_l2pm0(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 0" : "=r" (val));
	return val;
}

static void scorpion_l2_write_l2pm0(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 0" : : "r" (val));
}

static u32 scorpion_l2_read_l2pm1(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 1" : "=r" (val));
	return val;
}

static void scorpion_l2_write_l2pm1(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 1" : : "r" (val));
}

static u32 scorpion_l2_read_l2pm2(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 2" : "=r" (val));
	return val;
}

static void scorpion_l2_write_l2pm2(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 2" : : "r" (val));
}

static u32 scorpion_l2_read_l2pm3(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 3" : "=r" (val));
	return val;
}

static void scorpion_l2_write_l2pm3(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 3" : : "r" (val));
}

static u32 scorpion_l2_read_l2pm4(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c7, 4" : "=r" (val));
	return val;
}

static void scorpion_l2_write_l2pm4(u32 val)
{
	asm volatile ("mcr p15, 3, %0, c15, c7, 4" : : "r" (val));
}

struct scorpion_scorpion_access_funcs {
	u32(*read) (void);
	void (*write) (u32);
	void (*pre) (void);
	void (*post) (void);
};

struct scorpion_scorpion_access_funcs scorpion_l2_func[] = {
	{scorpion_l2_read_l2pm0, scorpion_l2_write_l2pm0, NULL, NULL},
	{scorpion_l2_read_l2pm1, scorpion_l2_write_l2pm1, NULL, NULL},
	{scorpion_l2_read_l2pm2, scorpion_l2_write_l2pm2, NULL, NULL},
	{scorpion_l2_read_l2pm3, scorpion_l2_write_l2pm3, NULL, NULL},
	{scorpion_l2_read_l2pm4, scorpion_l2_write_l2pm4, NULL, NULL},
};

#define COLMN0MASK 0x000000ff
#define COLMN1MASK 0x0000ff00
#define COLMN2MASK 0x00ff0000

static u32 scorpion_l2_get_columnmask(u32 setval)
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

static void scorpion_l2_evt_setup(u32 gr, u32 setval)
{
	u32 val;
	if (scorpion_l2_func[gr].pre)
		scorpion_l2_func[gr].pre();
	val = scorpion_l2_get_columnmask(setval) & scorpion_l2_func[gr].read();
	val = val | setval;
	scorpion_l2_func[gr].write(val);
	if (scorpion_l2_func[gr].post)
		scorpion_l2_func[gr].post();
}

#define SCORPION_L2_EVT_START_IDX 0x90
#define SCORPION_L2_INV_EVTYPE 0

static unsigned int get_scorpion_l2_evtinfo(unsigned int evt_type,
				      struct scorpion_l2_scorp_evt *evtinfo)
{
	u32 idx;
	u8 prefix;
	u8 reg;
	u8 code;
	u8 group;

	prefix = (evt_type & 0xF0000) >> 16;
	if (prefix == SCORPION_L2_EVT_PREFIX ||
			prefix == L2_SLAVE_EVT_PREFIX) {
		reg   = (evt_type & 0x0F000) >> 12;
		code  = (evt_type & 0x00FF0) >> 4;
		group =  evt_type & 0x0000F;

		if ((group > 3) || (reg > SCORPION_MAX_L2_REG))
			return SCORPION_L2_INV_EVTYPE;

		evtinfo->val = 0x80000000 | (code << (group * 8));
		evtinfo->grp = reg;
		evtinfo->evt_type_act = group | (reg << 2);
		return evtinfo->evt_type_act;
	}

	if (evt_type < SCORPION_L2_EVT_START_IDX
			|| evt_type >= SCORPION_L2_MAX_EVT)
		return SCORPION_L2_INV_EVTYPE;

	idx = evt_type - SCORPION_L2_EVT_START_IDX;

	if (sc_evt[idx].evt_type == evt_type) {
		evtinfo->val = sc_evt[idx].val;
		evtinfo->grp = sc_evt[idx].grp;
		evtinfo->evt_type_act = sc_evt[idx].evt_type_act;
		return sc_evt[idx].evt_type_act;
	}
	return SCORPION_L2_INV_EVTYPE;
}

static inline void scorpion_l2_pmnc_write(unsigned long val)
{
	val &= 0xff;
	asm volatile ("mcr p15, 3, %0, c15, c4, 0" : : "r" (val));
}

static inline unsigned long scorpion_l2_pmnc_read(void)
{
	u32 val;
	asm volatile ("mrc p15, 3, %0, c15, c4, 0" : "=r" (val));
	return val;
}

static void scorpion_l2_set_evcntcr(void)
{
	u32 val = 0x0;
	asm volatile ("mcr p15, 3, %0, c15, c6, 4" : : "r" (val));
}

static inline void scorpion_l2_set_evtyper(int ctr, int val)
{
	/* select ctr */
	asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (ctr));

	/* write into EVTYPER */
	asm volatile ("mcr p15, 3, %0, c15, c6, 7" : : "r" (val));
}

static void scorpion_l2_set_evfilter_task_mode(unsigned int is_slv)
{
	u32 filter_val = l2_orig_filter_prefix | 1 << smp_processor_id();

	if (is_slv)
		filter_val = l2_slv_filter_prefix;

	asm volatile ("mcr p15, 3, %0, c15, c6, 3" : : "r" (filter_val));
}

static void scorpion_l2_set_evfilter_sys_mode(unsigned int is_slv)
{
	u32 filter_val = l2_orig_filter_prefix | 0xf;

	if (is_slv)
		filter_val = l2_slv_filter_prefix;

	asm volatile ("mcr p15, 3, %0, c15, c6, 3" : : "r" (filter_val));
}

static void scorpion_l2_enable_intenset(u32 idx)
{
	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mcr p15, 3, %0, c15, c5, 1" : : "r"
			      (1 << SCORPION_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c5, 1" : : "r" (1 << idx));
	}
}

static void scorpion_l2_disable_intenclr(u32 idx)
{
	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mcr p15, 3, %0, c15, c5, 0" : : "r"
			      (1 << SCORPION_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c5, 0" : : "r" (1 << idx));
	}
}

static void scorpion_l2_enable_counter(u32 idx)
{
	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 3" : : "r"
			      (1 << SCORPION_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c4, 3" : : "r" (1 << idx));
	}
}

static void scorpion_l2_disable_counter(u32 idx)
{
	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 2" : : "r"
			      (1 << SCORPION_L2CYCLE_CTR_BIT));
	} else {
		asm volatile ("mcr p15, 3, %0, c15, c4, 2" : : "r" (1 << idx));
	}
}

static u32 scorpion_l2_read_counter(int idx)
{
	u32 val;
	unsigned long iflags;

	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mrc p15, 3, %0, c15, c4, 5" : "=r" (val));
	} else {
		raw_spin_lock_irqsave(&scorpion_l2_pmu_hw_events.pmu_lock,
				iflags);
		asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (idx));

		/* read val from counter */
		asm volatile ("mrc p15, 3, %0, c15, c6, 5" : "=r" (val));
		raw_spin_unlock_irqrestore(&scorpion_l2_pmu_hw_events.pmu_lock,
				iflags);
	}

	return val;
}

static void scorpion_l2_write_counter(int idx, u32 val)
{
	unsigned long iflags;

	if (idx == l2_cycle_ctr_idx) {
		asm volatile ("mcr p15, 3, %0, c15, c4, 5" : : "r" (val));
	} else {
		raw_spin_lock_irqsave(&scorpion_l2_pmu_hw_events.pmu_lock,
				iflags);

		/* select counter */
		asm volatile ("mcr p15, 3, %0, c15, c6, 0" : : "r" (idx));

		/* write val into counter */
		asm volatile ("mcr p15, 3, %0, c15, c6, 5" : : "r" (val));
		raw_spin_unlock_irqrestore(&scorpion_l2_pmu_hw_events.pmu_lock,
				iflags);
	}
}

static void scorpion_l2_stop_counter(struct hw_perf_event *hwc, int idx)
{
	scorpion_l2_disable_intenclr(idx);
	scorpion_l2_disable_counter(idx);
	pr_debug("%s: event: %ld ctr: %d stopped\n", __func__,
			hwc->config_base, idx);
}

static void scorpion_l2_enable(struct hw_perf_event *hwc, int idx, int cpu)
{
	struct scorpion_l2_scorp_evt evtinfo;
	int evtype = hwc->config_base;
	int ev_typer;
	unsigned long iflags;
	unsigned int is_slv = 0;
	unsigned int evt_prefix;

	raw_spin_lock_irqsave(&scorpion_l2_pmu_hw_events.pmu_lock, iflags);

	if (hwc->config_base == SCORPION_L2CYCLE_CTR_RAW_CODE)
		goto out;

	/* Check if user requested any special origin filtering. */
	evt_prefix = (hwc->config_base &
			L2_EVT_PREFIX_MASK) >> L2_EVT_PREFIX_SHIFT;

	if (evt_prefix == L2_SLAVE_EVT_PREFIX)
		is_slv = 1;

	memset(&evtinfo, 0, sizeof(evtinfo));

	ev_typer = get_scorpion_l2_evtinfo(evtype, &evtinfo);

	scorpion_l2_set_evtyper(idx, ev_typer);

	scorpion_l2_set_evcntcr();

	if (cpu < 0)
		scorpion_l2_set_evfilter_task_mode(is_slv);
	else
		scorpion_l2_set_evfilter_sys_mode(is_slv);

	scorpion_l2_evt_setup(evtinfo.grp, evtinfo.val);

out:

	scorpion_l2_enable_intenset(idx);

	scorpion_l2_enable_counter(idx);

	raw_spin_unlock_irqrestore(&scorpion_l2_pmu_hw_events.pmu_lock, iflags);

	pr_debug("%s: ctr: %d group: %ld group_code: %lld started from cpu:%d\n",
	     __func__, idx, hwc->config_base, hwc->config, smp_processor_id());
}

static void scorpion_l2_disable(struct hw_perf_event *hwc, int idx)
{
	unsigned long iflags;

	raw_spin_lock_irqsave(&scorpion_l2_pmu_hw_events.pmu_lock, iflags);

	scorpion_l2_stop_counter(hwc, idx);

	raw_spin_unlock_irqrestore(&scorpion_l2_pmu_hw_events.pmu_lock, iflags);

	pr_debug("%s: event: %ld deleted\n", __func__, hwc->config_base);
}

static int scorpion_l2_get_event_idx(struct pmu_hw_events *cpuc,
				  struct hw_perf_event *hwc)
{
	int ctr = 0;

	if (hwc->config_base == SCORPION_L2CYCLE_CTR_RAW_CODE) {
		if (test_and_set_bit(l2_cycle_ctr_idx,
					cpuc->used_mask))
			return -EAGAIN;

		return l2_cycle_ctr_idx;
	}

	for (ctr = 0; ctr < total_l2_ctrs - 1; ctr++) {
		if (!test_and_set_bit(ctr, cpuc->used_mask))
			return ctr;
	}

	return -EAGAIN;
}

static void scorpion_l2_start(void)
{
	isb();
	/* Enable all counters */
	scorpion_l2_pmnc_write(scorpion_l2_pmnc_read() | SCORPIONL2_PMNC_E);
}

static void scorpion_l2_stop(void)
{
	/* Disable all counters */
	scorpion_l2_pmnc_write(scorpion_l2_pmnc_read() & ~SCORPIONL2_PMNC_E);
	isb();
}

static inline u32 scorpion_l2_get_reset_pmovsr(void)
{
	u32 val;

	/* Read */
	asm volatile ("mrc p15, 3, %0, c15, c4, 1" : "=r" (val));

	/* Write to clear flags */
	val &= 0xffffffff;
	asm volatile ("mcr p15, 3, %0, c15, c4, 1" : : "r" (val));

	return val;
}

static irqreturn_t scorpion_l2_handle_irq(int irq_num, void *dev)
{
	unsigned long pmovsr;
	struct perf_sample_data data;
	struct pt_regs *regs;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int bitp;
	int idx = 0;

	pmovsr = scorpion_l2_get_reset_pmovsr();

	if (!(pmovsr & 0xffffffff))
		return IRQ_NONE;

	regs = get_irq_regs();

	perf_sample_data_init(&data, 0);

	while (pmovsr) {
		bitp = __ffs(pmovsr);

		if (bitp == SCORPION_L2CYCLE_CTR_BIT)
			idx = l2_cycle_ctr_idx;
		else
			idx = bitp;

		event = scorpion_l2_pmu_hw_events.events[idx];

		if (!event)
			goto next;

		if (!test_bit(idx, scorpion_l2_pmu_hw_events.used_mask))
			goto next;

		hwc = &event->hw;

		armpmu_event_update(event, hwc, idx);

		data.period = event->hw.last_period;

		if (!armpmu_event_set_period(event, hwc, idx))
			goto next;

		if (perf_event_overflow(event, &data, regs))
			scorpion_l2_disable_counter(hwc->idx);
next:
		pmovsr &= (pmovsr - 1);
	}

	irq_work_run();

	return IRQ_HANDLED;
}

static int scorpion_l2_map_event(struct perf_event *event)
{
	if (pmu_type > 0 && pmu_type == event->attr.type)
		return event->attr.config & L2_EVT_MASK;
	else
		return -ENOENT;
}

static int
scorpion_l2_pmu_generic_request_irq(int irq, irq_handler_t *handle_irq)
{
	return request_irq(irq, *handle_irq,
			IRQF_DISABLED | IRQF_NOBALANCING,
			"scorpion-l2-armpmu", NULL);
}

static void
scorpion_l2_pmu_generic_free_irq(int irq)
{
	if (irq >= 0)
		free_irq(irq, NULL);
}

static int msm_l2_test_set_ev_constraint(struct perf_event *event)
{
	u32 evt_type = event->attr.config & L2_EVT_MASK;
	u8 prefix = (evt_type & 0xF0000) >> 16;
	u8 reg   = (evt_type & 0x0F000) >> 12;
	u8 group =  evt_type & 0x0000F;
	u8 code = (evt_type & 0x00FF0) >> 4;
	unsigned long flags;
	u32 err = 0;
	u64 bitmap_t;
	u32 shift_idx;

	if (!prefix)
		return 0;
	/*
	 * Cycle counter collision is detected in
	 * get_event_idx().
	 */
	if (evt_type == SCORPION_L2CYCLE_CTR_RAW_CODE)
		return err;

	raw_spin_lock_irqsave(&l2_pmu_constraints.lock, flags);

	shift_idx = ((reg * 4) + group);

	bitmap_t = 1 << shift_idx;

	if (!(l2_pmu_constraints.pmu_bitmap & bitmap_t)) {
		l2_pmu_constraints.pmu_bitmap |= bitmap_t;
		l2_pmu_constraints.codes[shift_idx] = code;
		goto out;
	} else {
		/*
		 * If NRCCG's are identical,
		 * its not column exclusion.
		 */
		if (l2_pmu_constraints.codes[shift_idx] != code)
			err = -EPERM;
		else
			/*
			 * If the event is counted in syswide mode
			 * then we want to count only on one CPU
			 * and set its filter to count from all.
			 * This sets the event OFF on all but one
			 * CPU.
			 */
			if (!(event->cpu < 0))
				event->state = PERF_EVENT_STATE_OFF;
	}

out:
	raw_spin_unlock_irqrestore(&l2_pmu_constraints.lock, flags);
	return err;
}

static int msm_l2_clear_ev_constraint(struct perf_event *event)
{
	u32 evt_type = event->attr.config & L2_EVT_MASK;
	u8 prefix = (evt_type & 0xF0000) >> 16;
	u8 reg   = (evt_type & 0x0F000) >> 12;
	u8 group =  evt_type & 0x0000F;
	unsigned long flags;
	u64 bitmap_t;
	u32 shift_idx;

	if (!prefix)
		return 0;

	raw_spin_lock_irqsave(&l2_pmu_constraints.lock, flags);

	shift_idx = ((reg * 4) + group);

	bitmap_t = 1 << shift_idx;

	/* Clear constraint bit. */
	l2_pmu_constraints.pmu_bitmap &= ~bitmap_t;

	raw_spin_unlock_irqrestore(&l2_pmu_constraints.lock, flags);
	return 1;
}

static int get_num_events(void)
{
	int val;

	val = scorpion_l2_pmnc_read();
	/*
	 * Read bits 15:11 of the L2PMCR and add 1
	 * for the cycle counter.
	 */
	return ((val >> PMCR_NUM_EV_SHIFT) & PMCR_NUM_EV_MASK) + 1;
}

static struct arm_pmu scorpion_l2_pmu = {
	.id		=	ARM_PERF_PMU_ID_SCORPIONMP_L2,
	.type		=	ARM_PMU_DEVICE_L2CC,
	.name		=	"Scorpion L2CC PMU",
	.start		=	scorpion_l2_start,
	.stop		=	scorpion_l2_stop,
	.handle_irq	=	scorpion_l2_handle_irq,
	.request_pmu_irq	= scorpion_l2_pmu_generic_request_irq,
	.free_pmu_irq		= scorpion_l2_pmu_generic_free_irq,
	.enable		=	scorpion_l2_enable,
	.disable	=	scorpion_l2_disable,
	.read_counter	=	scorpion_l2_read_counter,
	.get_event_idx	=	scorpion_l2_get_event_idx,
	.write_counter	=	scorpion_l2_write_counter,
	.map_event	=	scorpion_l2_map_event,
	.max_period	=	(1LLU << 32) - 1,
	.get_hw_events	=	scorpion_l2_get_hw_events,
	.test_set_event_constraints	= msm_l2_test_set_ev_constraint,
	.clear_event_constraints	= msm_l2_clear_ev_constraint,
	.pmu.attr_groups		= msm_l2_pmu_attr_grps,
};

static int __devinit scorpion_l2_pmu_device_probe(struct platform_device *pdev)
{
	scorpion_l2_pmu.plat_device = pdev;

	if (!armpmu_register(&scorpion_l2_pmu, "msm-l2", -1))
		pmu_type = scorpion_l2_pmu.pmu.type;

	return 0;
}

static struct platform_driver scorpion_l2_pmu_driver = {
	.driver		= {
		.name	= "l2-pmu",
	},
	.probe		= scorpion_l2_pmu_device_probe,
};

static int __init register_scorpion_l2_pmu_driver(void)
{
	/* Avoid spurious interrupt if any */
	scorpion_l2_get_reset_pmovsr();

	total_l2_ctrs = get_num_events();
	scorpion_l2_pmu.num_events = total_l2_ctrs;

	pr_info("Detected %d counters on the L2CC PMU.\n",
			total_l2_ctrs);

	/*
	 * The L2 cycle counter index in the used_mask
	 * bit stream is always after the other counters.
	 * Counter indexes begin from 0 to keep it consistent
	 * with the h/w.
	 */
	l2_cycle_ctr_idx = total_l2_ctrs - 1;

	return platform_driver_register(&scorpion_l2_pmu_driver);
}
device_initcall(register_scorpion_l2_pmu_driver);
