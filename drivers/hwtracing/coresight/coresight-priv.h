/* Copyright (c) 2011-2012, 2016 The Linux Foundation. All rights reserved.
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

#ifndef _CORESIGHT_PRIV_H
#define _CORESIGHT_PRIV_H

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/coresight.h>

/*
 * Coresight management registers (0xf00-0xfcc)
 * 0xfa0 - 0xfa4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 */
#define CORESIGHT_ITCTRL	0xf00
#define CORESIGHT_CLAIMSET	0xfa0
#define CORESIGHT_CLAIMCLR	0xfa4
#define CORESIGHT_LAR		0xfb0
#define CORESIGHT_LSR		0xfb4
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		0xfc8
#define CORESIGHT_DEVTYPE	0xfcc

#define TIMEOUT_US		100
#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & GENMASK(msb, lsb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

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

static inline bool coresight_authstatus_enabled(void __iomem *addr)
{
	int ret;
	unsigned auth_val;

	if (!addr)
		return false;

	auth_val = readl_relaxed(addr + CORESIGHT_AUTHSTATUS);

	if ((0x2 == BMVAL(auth_val, 0, 1)) ||
	    (0x2 == BMVAL(auth_val, 2, 3)) ||
	    (0x2 == BMVAL(auth_val, 4, 5)) ||
	    (0x2 == BMVAL(auth_val, 6, 7)))
		ret = false;
	else
		ret = true;

	return ret;
}

#ifdef CONFIG_CORESIGHT_SOURCE_ETM3X
extern int etm_readl_cp14(u32 off, unsigned int *val);
extern int etm_writel_cp14(u32 off, u32 val);
#else
static inline int etm_readl_cp14(u32 off, unsigned int *val) { return 0; }
static inline int etm_writel_cp14(u32 off, u32 val) { return 0; }
#endif

#ifdef CONFIG_CORESIGHT_CSR
extern void msm_qdss_csr_enable_bam_to_usb(void);
extern void msm_qdss_csr_disable_bam_to_usb(void);
extern void msm_qdss_csr_disable_flush(void);
extern int coresight_csr_hwctrl_set(uint64_t addr, uint32_t val);
#else
static inline void msm_qdss_csr_enable_bam_to_usb(void) {}
static inline void msm_qdss_csr_disable_bam_to_usb(void) {}
static inline void msm_qdss_csr_disable_flush(void) {}
static inline int coresight_csr_hwctrl_set(uint64_t addr,
					   uint32_t val) { return -EINVAL; }
#endif

#endif
