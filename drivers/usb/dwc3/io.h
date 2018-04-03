/**
 * io.h - DesignWare USB3 DRD IO Header
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_USB_DWC3_IO_H
#define __DRIVERS_USB_DWC3_IO_H

#include <linux/io.h>
#include "trace.h"
#include "debug.h"
#include "core.h"

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
	dwc3_trace(trace_dwc3_readl, "addr %pK value %08x",
			base - DWC3_GLOBALS_REGS_START + offset, value);

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
	dwc3_trace(trace_dwc3_writel, "addr %pK value %08x",
			base - DWC3_GLOBALS_REGS_START + offset, value);
}

static inline void dwc3_masked_write_readback(void __iomem *base,
	u32 offset, const u32 mask, u32 value)
{
	u32 write_val, tmp;

	tmp = readl_relaxed(base + offset - DWC3_GLOBALS_REGS_START);
	tmp &= ~mask;		/* retain other bits */
	write_val = tmp | value;

	writel_relaxed(write_val, base + offset - DWC3_GLOBALS_REGS_START);

	/* Read back to see if value was written */
	tmp = readl_relaxed(base + offset - DWC3_GLOBALS_REGS_START);

	dwc3_trace(trace_dwc3_masked_write_readback,
			"addr %p readback val %08x",
			base - DWC3_GLOBALS_REGS_START + offset, tmp);

	tmp &= mask;		/* clear other bits */
	if (tmp != value)
		pr_err("%s: write: %x to %x FAILED\n",
			__func__, value, offset);
}

#endif /* __DRIVERS_USB_DWC3_IO_H */
