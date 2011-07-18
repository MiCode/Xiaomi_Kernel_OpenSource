/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>

#include <mach/msm_iomap.h>
#include <mach/scm.h>
#include <mach/scm-io.h>

#define SCM_IO_READ	0x1
#define SCM_IO_WRITE	0x2

#define BETWEEN(p, st, sz) ((p) >= (void __iomem *)(st) && \
				(p) < ((void __iomem *)(st) + (sz)))
#define XLATE(p, pst, vst) ((u32)((p) - (vst)) + (pst))

static u32 __secure_readl(u32 addr)
{
	u32 r;
	r = scm_call_atomic1(SCM_SVC_IO, SCM_IO_READ, addr);
	__iormb();
	return r;
}

u32 secure_readl(void __iomem *c)
{
	if (BETWEEN(c, MSM_MMSS_CLK_CTL_BASE, MSM_MMSS_CLK_CTL_SIZE))
		return __secure_readl(XLATE(c, MSM_MMSS_CLK_CTL_PHYS,
					MSM_MMSS_CLK_CTL_BASE));
	else if (BETWEEN(c, MSM_TCSR_BASE, MSM_TCSR_SIZE))
		return __secure_readl(XLATE(c, MSM_TCSR_PHYS, MSM_TCSR_BASE));
	return readl(c);
}
EXPORT_SYMBOL(secure_readl);

static void __secure_writel(u32 v, u32 addr)
{
	__iowmb();
	scm_call_atomic2(SCM_SVC_IO, SCM_IO_WRITE, addr, v);
}

void secure_writel(u32 v, void __iomem *c)
{
	if (BETWEEN(c, MSM_MMSS_CLK_CTL_BASE, MSM_MMSS_CLK_CTL_SIZE))
		__secure_writel(v, XLATE(c, MSM_MMSS_CLK_CTL_PHYS,
					MSM_MMSS_CLK_CTL_BASE));
	else if (BETWEEN(c, MSM_TCSR_BASE, MSM_TCSR_SIZE))
		__secure_writel(v, XLATE(c, MSM_TCSR_PHYS, MSM_TCSR_BASE));
	else
		writel(v, c);
}
EXPORT_SYMBOL(secure_writel);
