/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_CORESIGHT_FUSE
extern bool coresight_fuse_access_disabled(void);
extern bool coresight_fuse_apps_access_disabled(void);
extern bool coresight_fuse_qpdi_access_disabled(void);
extern bool coresight_fuse_nidnt_access_disabled(void);
#else
static inline bool coresight_fuse_access_disabled(void) { return false; }
static inline bool coresight_fuse_apps_access_disabled(void) { return false; }
static inline bool coresight_fuse_qpdi_access_disabled(void) { return false; }
static inline bool coresight_fuse_nidnt_access_disabled(void) { return false; }
#endif
#ifdef CONFIG_CORESIGHT_CSR
extern void msm_qdss_csr_enable_bam_to_usb(void);
extern void msm_qdss_csr_disable_bam_to_usb(void);
extern void msm_qdss_csr_disable_flush(void);
extern int coresight_csr_hwctrl_set(uint64_t addr, uint32_t val);
extern void coresight_csr_set_byte_cntr(uint32_t);
#else
static inline void msm_qdss_csr_enable_bam_to_usb(void) {}
static inline void msm_qdss_csr_disable_bam_to_usb(void) {}
static inline void msm_qdss_csr_disable_flush(void) {}
static inline int coresight_csr_hwctrl_set(uint64_t addr,
					   uint32_t val) { return -ENOSYS; }
static inline void coresight_csr_set_byte_cntr(uint32_t val) {}
#endif
#ifdef CONFIG_CORESIGHT_ETM
extern unsigned int etm_readl_cp14(uint32_t off);
extern void etm_writel_cp14(uint32_t val, uint32_t off);
#else
static inline unsigned int etm_readl_cp14(uint32_t off) { return 0; }
static inline void etm_writel_cp14(uint32_t val, uint32_t off) {}
#endif
#if defined(CONFIG_CORESIGHT_ETM) || defined(CONFIG_CORESIGHT_ETMV4)
extern int coresight_etm_get_funnel_port(int cpu);
#else
static inline int coresight_etm_get_funnel_port(int cpu) { return -ENOSYS; }
#endif

#endif
