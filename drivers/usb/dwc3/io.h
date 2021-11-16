// SPDX-License-Identifier: GPL-2.0
/**
 * io.h - DesignWare USB3 DRD IO Header
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 */

#ifndef __DRIVERS_USB_DWC3_IO_H
#define __DRIVERS_USB_DWC3_IO_H

#include <linux/io.h>
#include "trace.h"
#include "debug.h"
#include "core.h"
#ifndef CONFIG_FACTORY_BUILD
#include <linux/iopoll.h>
#endif

static inline u32 dwc3_readl(void __iomem *base, u32 offset)
{
	u32 value;

	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	value = readl(base + offset - DWC3_GLOBALS_REGS_START);

	/*
	 * When tracing we want to make it easy to find the correct address on
	 * documentation, so we revert it back to the proper addresses, the
	 * same way they are described on SNPS documentation
	 */
	trace_dwc3_readl(base - DWC3_GLOBALS_REGS_START, offset, value);

	return value;
}

static inline void dwc3_writel(void __iomem *base, u32 offset, u32 value)
{
	/*
	 * We requested the mem region starting from the Globals address
	 * space, see dwc3_probe in core.c.
	 * However, the offsets are given starting from xHCI address space.
	 */
	writel(value, base + offset - DWC3_GLOBALS_REGS_START);

	/*
	 * When tracing we want to make it easy to find the correct address on
	 * documentation, so we revert it back to the proper addresses, the
	 * same way they are described on SNPS documentation
	 */
	trace_dwc3_writel(base - DWC3_GLOBALS_REGS_START, offset, value);
}
#ifndef CONFIG_FACTORY_BUILD
static inline int dwc3_poll_read_timeout(void __iomem *base, u32 offset,
		u32 *reg_addr, u32 mask,
		u32 val, u32 timeout)
{
	u32 reg;
	int ret;

	reg = readl(base + offset - DWC3_GLOBALS_REGS_START);
	trace_dwc3_readl(base - DWC3_GLOBALS_REGS_START, offset, reg);

	ret = readl_poll_timeout_atomic(base + offset - DWC3_GLOBALS_REGS_START,
			reg, (reg & mask) == val, 0, timeout);

	*reg_addr = reg;
	trace_dwc3_readl(base - DWC3_GLOBALS_REGS_START, offset, reg);

	return ret;
}
#endif
#endif /* __DRIVERS_USB_DWC3_IO_H */
