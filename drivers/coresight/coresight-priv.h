/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

/* Coresight management registers (0xF00-0xFCC)
 * 0xFA0 - 0xFA4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 */
#define CORESIGHT_ITCTRL	(0xF00)
#define CORESIGHT_CLAIMSET	(0xFA0)
#define CORESIGHT_CLAIMCLR	(0xFA4)
#define CORESIGHT_LAR		(0xFB0)
#define CORESIGHT_LSR		(0xFB4)
#define CORESIGHT_AUTHSTATUS	(0xFB8)
#define CORESIGHT_DEVID		(0xFC8)
#define CORESIGHT_DEVTYPE	(0xFCC)

#define CORESIGHT_UNLOCK	(0xC5ACCE55)

#define TIMEOUT_US		(100)

#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & BM(lsb, msb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

#ifdef CONFIG_MSM_QDSS
extern void msm_qdss_csr_enable_bam_to_usb(void);
extern void msm_qdss_csr_disable_bam_to_usb(void);
#else
static inline void msm_qdss_csr_enable_bam_to_usb(void) {}
static inline void msm_qdss_csr_disable_bam_to_usb(void) {}
#endif

#endif
