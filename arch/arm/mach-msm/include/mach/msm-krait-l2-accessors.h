/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_MSM_KRAIT_L2_ACCESSORS_H
#define __ASM_ARCH_MSM_MSM_KRAIT_L2_ACCESSORS_H

#define MAX_L2_PERIOD		((1ULL << 32) - 1)
#define MAX_KRAIT_L2_CTRS	10

#define PMCR_NUM_EV_SHIFT	11
#define PMCR_NUM_EV_MASK	0x1f

#define L2_EVT_MASK		0xfffff

#define L2_SLAVE_EV_PREFIX	4
#define L2_TRACECTR_PREFIX	5

#define L2PMCCNTR		0x409
#define L2PMCCNTCR		0x408
#define L2PMCCNTSR		0x40A
#define L2CYCLE_CTR_BIT		31
#define L2CYCLE_CTR_RAW_CODE	0xfe

#define L2PMOVSR	0x406

#define L2PMCR			0x400
#define L2PMCR_RESET_ALL	0x6
#define L2PMCR_GLOBAL_ENABLE	0x1
#define L2PMCR_GLOBAL_DISABLE	0x0

#define L2PMCNTENSET	0x403
#define L2PMCNTENCLR	0x402

#define L2PMINTENSET	0x405
#define L2PMINTENCLR	0x404

#define IA_L2PMXEVCNTCR_BASE	0x420
#define IA_L2PMXEVTYPER_BASE	0x424
#define IA_L2PMRESX_BASE	0x410
#define IA_L2PMXEVFILTER_BASE	0x423
#define IA_L2PMXEVCNTR_BASE	0x421

/* event format is -e rsRCCG See get_event_desc() */

#define EVENT_PREFIX_MASK	0xf0000
#define EVENT_REG_MASK		0x0f000
#define EVENT_GROUPSEL_MASK	0x0000f
#define EVENT_GROUPCODE_MASK	0x00ff0

#define EVENT_PREFIX_SHIFT		16
#define EVENT_REG_SHIFT			12
#define EVENT_GROUPCODE_SHIFT		4

#define RESRX_VALUE_EN	0x80000000

#ifdef CONFIG_ARCH_MSM_KRAIT
extern void set_l2_indirect_reg(u32 reg_addr, u32 val);
extern u32 get_l2_indirect_reg(u32 reg_addr);
#else
static inline void set_l2_indirect_reg(u32 reg_addr, u32 val) {}
static inline u32 get_l2_indirect_reg(u32 reg_addr)
{
	return 0;
}
#endif

#endif
