/*
 * dmic.h  --  OMAP Digital Microphone Controller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_OMAP_DMIC_H
#define __ASM_ARCH_OMAP_DMIC_H

#define OMAP44XX_DMIC_L3_BASE	0x4902e000

#define OMAP_DMIC_REVISION		0x00
#define OMAP_DMIC_SYSCONFIG		0x10
#define OMAP_DMIC_IRQSTATUS_RAW		0x24
#define OMAP_DMIC_IRQSTATUS		0x28
#define OMAP_DMIC_IRQENABLE_SET		0x2C
#define OMAP_DMIC_IRQENABLE_CLR		0x30
#define OMAP_DMIC_IRQWAKE_EN		0x34
#define OMAP_DMIC_DMAENABLE_SET		0x38
#define OMAP_DMIC_DMAENABLE_CLR		0x3C
#define OMAP_DMIC_DMAWAKEEN		0x40
#define OMAP_DMIC_CTRL			0x44
#define OMAP_DMIC_DATA			0x48
#define OMAP_DMIC_FIFO_CTRL		0x4C
#define OMAP_DMIC_FIFO_DMIC1R_DATA	0x50
#define OMAP_DMIC_FIFO_DMIC1L_DATA	0x54
#define OMAP_DMIC_FIFO_DMIC2R_DATA	0x58
#define OMAP_DMIC_FIFO_DMIC2L_DATA	0x5C
#define OMAP_DMIC_FIFO_DMIC3R_DATA	0x60
#define OMAP_DMIC_FIFO_DMIC3L_DATA	0x64

/*
 * DMIC_IRQ bit fields
 * IRQSTATUS_RAW, IRQSTATUS, IRQENABLE_SET, IRQENABLE_CLR
 */

#define OMAP_DMIC_IRQ			(1 << 0)
#define OMAP_DMIC_IRQ_FULL		(1 << 1)
#define OMAP_DMIC_IRQ_ALMST_EMPTY	(1 << 2)
#define OMAP_DMIC_IRQ_EMPTY		(1 << 3)
#define OMAP_DMIC_IRQ_MASK		0x07

/*
 * DMIC_DMAENABLE bit fields
 */

#define OMAP_DMIC_DMA_ENABLE		0x1

/*
 * DMIC_CTRL bit fields
 */

#define OMAP_DMIC_UP1_ENABLE		0x0001
#define OMAP_DMIC_UP2_ENABLE		0x0002
#define OMAP_DMIC_UP3_ENABLE		0x0004
#define OMAP_DMIC_UP_ENABLE_MASK	0x0007
#define OMAP_DMIC_FORMAT		0x0008
#define OMAP_DMIC_POLAR1		0x0010
#define OMAP_DMIC_POLAR2		0x0020
#define OMAP_DMIC_POLAR3		0x0040
#define OMAP_DMIC_POLAR_MASK		0x0070
#define OMAP_DMIC_CLK_DIV_SHIFT		7
#define OMAP_DMIC_CLK_DIV_MASK		0x0380
#define	OMAP_DMIC_RESET			0x0400

#define OMAP_DMIC_ENABLE_MASK		0x007

#define OMAP_DMICOUTFORMAT_LJUST	(0 << 3)
#define OMAP_DMICOUTFORMAT_RJUST	(1 << 3)

/*
 * DMIC_FIFO_CTRL bit fields
 */

#define OMAP_DMIC_THRES_MAX		0xF

#endif
