/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __BUS_TRACER_V1_H__
#define __BUS_TRACER_V1_H__

#define get_bit_at(reg, pos) (((reg) >> (pos)) & 1)

#define NUM_ID_FILTER		2

#define SW_RST_B		(1 << 0)
#define ID_FILTER0_EN		(1 << 1)
#define ID_FILTER1_EN		(1 << 2)
#define ID_FILTER2_EN		(1 << 3)
#define ID_FILTER3_EN		(1 << 4)
#define ID_WATCH_OR_BYPASS	(1 << 5)
#define ID_FILTER_EN		(1 << 6)
#define ADDR_FILTER0_EN		(1 << 7)
#define ADDR_FILTER1_EN		(1 << 8)
#define ADDR_FILTER2_EN		(1 << 9)
#define ADDR_FILTER3_EN		(1 << 10)
#define ADDR_WATCH_OR_BYPASS	(1 << 11)
#define ADDR_FILTER_EN		(1 << 12)
#define ADDR_RANGE0_EN		(1 << 13)
#define ADDR_RANGE1_EN		(1 << 14)
#define ADDR_RANGE_MODE		(1 << 15)
#define BUS_TRACE_EN		(1 << 16)
#define WDT_RST_EN		(1 << 18)
#define AW_DISABLE		(1 << 20)
#define AR_DISABLE		(1 << 21)
#define W_DISABLE		(1 << 22)
#define B_DISABLE		(1 << 23)
#define R_DISABLE		(1 << 24)

#define BYPASS_FILTER_SHIFT	12

#define BUS_TRACE_CON			0x0
#define BUS_TRACE_CON_SYNC_SET		0x4
#define BUS_TRACE_CON_SYNC_STATUS	0x8
#define BUS_TRACE_ADDR_RANGE0_L		0x10
#define BUS_TRACE_ADDR_RANGE0_H		0x14
#define BUS_TRACE_ADDR_RANGE1_L		0x18
#define BUS_TRACE_ADDR_RANGE1_H		0x1c
#define BUS_TRACE_ID_FILTER0		0x20
#define BUS_TRACE_ID_FILTER1		0x24
#define BUS_TRACE_ID_FILTER2		0x28
#define BUS_TRACE_ID_FILTER3		0x2c
#define BUS_TRACE_ID_MASK0		0x30
#define BUS_TRACE_ID_MASK1		0x34
#define BUS_TRACE_ID_MASK2		0x38
#define BUS_TRACE_ID_MASK3		0x3c
#define BUS_TRACE_ADDR_FILTER0_L	0x40
#define BUS_TRACE_ADDR_FILTER0_H	0x44
#define BUS_TRACE_ADDR_FILTER1_L	0x48
#define BUS_TRACE_ADDR_FILTER1_H	0x4c
#define BUS_TRACE_ADDR_FILTER2_L	0x50
#define BUS_TRACE_ADDR_FILTER2_H	0x54
#define BUS_TRACE_ADDR_FILTER3_L	0x58
#define BUS_TRACE_ADDR_FILTER3_H	0x5c
#define BUS_TRACE_ADDR_MASK0_L		0x60
#define BUS_TRACE_ADDR_MASK0_H		0x64
#define BUS_TRACE_ADDR_MASK1_L		0x68
#define BUS_TRACE_ADDR_MASK1_H		0x6c
#define BUS_TRACE_ADDR_MASK2_L		0x70
#define BUS_TRACE_ADDR_MASK2_H		0x74
#define BUS_TRACE_ADDR_MASK3_L		0x78
#define BUS_TRACE_ADDR_MASK3_H		0x7c
#define BUS_TRACE_ATID			0x80
#define BUS_TRACE_BUS_DBG_CON_AO	0xfc

/* ETB registers, "CoreSight Components TRM", 9.3 */
#define ETB_DEPTH		0x04
#define ETB_STATUS		0x0c
#define ETB_READMEM		0x10
#define ETB_READADDR		0x14
#define ETB_WRITEADDR		0x18
#define ETB_TRIGGERCOUNT	0x1c
#define ETB_CTRL		0x20
#define ETB_RWD			0x24
#define ETB_LAR			0xfb0

#define DEM_DBGRST_ALL		0x28
#define DEM_ATB_CLK		0x70
#define INSERT_TS0		0x80
#define ETR_AWID		0x84
#define ETR_AWUSER		0x88
#define DEM_LAR			0xfb0

#define DBG_ERR_FLAG_CON		0x80
#define DBG_ERR_FLAG_WDT_MASK_EN	0x84
#define DBG_ERR_FLAG_STATUS0		0x88
#define DBG_ERR_FLAG_STATUS1		0x8c
#define DBG_ERR_FLAG_SYSTIMER_L		0x90
#define DBG_ERR_FLAG_SYSTIMER_H		0x94
#define DBG_ERR_FLAG_ERR_STAGE		0x98
#define DBG_ERR_FLAG_IRQ_POLARITY	0x9c

#define REPLICATOR1_BASE	0x1000
#define REPLICATOR_LAR		0xfb0
#define REPLICATOR_IDFILTER0	0x0
#define REPLICATOR_IDFILTER1	0x4

#define FUNNEL_CTRL_REG		0x0
#define FUNNEL_LOCKACCESS	0xfb0

#define CORESIGHT_LAR		0xfb0
#define CORESIGHT_UNLOCK        0xc5acce55

static inline void CS_LOCK(void __iomem *addr)
{
	do {
		/* Wait for things to settle */
		mb();
		writel_relaxed(0x0, addr + CORESIGHT_LAR);
	} while (0);
}

static inline void CS_UNLOCK(void __iomem *addr)
{
	do {
		writel_relaxed(CORESIGHT_UNLOCK, addr + CORESIGHT_LAR);
		/* Make sure everyone has seen this */
		mb();
	} while (0);
}

#endif /* end of __BUS_TRACER_V1_H__ */

