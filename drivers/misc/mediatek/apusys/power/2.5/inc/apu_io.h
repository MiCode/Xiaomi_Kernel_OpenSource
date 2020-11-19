/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/bitops.h>

#define POLL_INTERVAL		10 /* us */
#define POLL_TIMEOUT		50 /* us */

#define	MASK(x)	(x - 1)

static inline void apu_writel(const unsigned int val,
			      void __force __iomem *regs)
{
	writel(val, regs);
	/* make sure all the write instructions are done */
	wmb();
}

static inline u32 apu_readl(void __force __iomem *regs)
{
	return readl(regs);
}

static inline void apu_setl(const unsigned int val,
			    void __force __iomem *regs)
{
	apu_writel((readl(regs) | val), regs);
}

static inline void apu_clearl(const unsigned int val,
			      void __force __iomem *regs)
{
	apu_writel((readl(regs) & ~val), regs);
}

