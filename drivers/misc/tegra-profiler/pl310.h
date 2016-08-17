/*
 * drivers/misc/tegra-profiler/pl310.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __QUADD_PL310_H
#define __QUADD_PL310_H

/*
 * l2x0 event type
 */
enum quadd_l2x0_event_type {
	QUADD_L2X0_TYPE_DATA_READ_MISSES	= 0,
	QUADD_L2X0_TYPE_DATA_WRITE_MISSES	= 1,
	QUADD_L2X0_TYPE_INSTRUCTION_MISSES	= 2,
};

#ifdef __KERNEL__

#include <linux/io.h>

#define L2X0_EVENT_CNT_ENABLE		(1 << 0)
#define L2X0_EVENT_CNT_RESET_CNT0	(1 << 1)
#define L2X0_EVENT_CNT_RESET_CNT1	(2 << 1)


#define L2X0_EVENT_CNT_CFG_DRHIT	(2 << 2)
#define L2X0_EVENT_CNT_CFG_DRREQ	(3 << 2)

#define L2X0_EVENT_CNT_CFG_DWHIT	(4 << 2)
#define L2X0_EVENT_CNT_CFG_DWREQ	(5 << 2)

#define L2X0_EVENT_CNT_CFG_IRHIT	(7 << 2)
#define L2X0_EVENT_CNT_CFG_IRREQ	(8 << 2)

/*
 * l2x0 counters
 */
enum quadd_l2x0_counter {
	QUADD_L2X0_COUNTER1 = 0,
	QUADD_L2X0_COUNTER0 = 1,
};

struct l2x0_context {
	int l2x0_event_type;
	int event_id;

	void __iomem *l2x0_base;
	spinlock_t lock;
};

struct quadd_event_source_interface;

struct quadd_event_source_interface *quadd_l2x0_events_init(void);

static inline unsigned long quadd_get_pl310_phys_addr(void)
{
	unsigned long phys_addr = 0;

#if defined(CONFIG_ARCH_TEGRA)
	phys_addr = 0x50043000;
#endif
	return phys_addr;
}

#endif  /* __KERNEL__ */

#endif	/* __QUADD_PL310_H */
