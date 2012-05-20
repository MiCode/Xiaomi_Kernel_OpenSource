/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_CS_H
#define _LINUX_CS_H

/* Peripheral id registers (0xFD0-0xFEC) */
#define CS_PIDR4		(0xFD0)
#define CS_PIDR5		(0xFD4)
#define CS_PIDR6		(0xFD8)
#define CS_PIDR7		(0xFDC)
#define CS_PIDR0		(0xFE0)
#define CS_PIDR1		(0xFE4)
#define CS_PIDR2		(0xFE8)
#define CS_PIDR3		(0xFEC)
/* Component id registers (0xFF0-0xFFC) */
#define CS_CIDR0		(0xFF0)
#define CS_CIDR1		(0xFF4)
#define CS_CIDR2		(0xFF8)
#define CS_CIDR3		(0xFFC)

/* DBGv7 with baseline CP14 registers implemented */
#define ARM_DEBUG_ARCH_V7B	(0x3)
/* DBGv7 with all CP14 registers implemented */
#define ARM_DEBUG_ARCH_V7	(0x4)
#define ARM_DEBUG_ARCH_V7_1	(0x5)
#define ETM_ARCH_V3_3		(0x23)
#define PFT_ARCH_V1_1		(0x31)

struct qdss_source {
	struct list_head link;
	const char *name;
	uint32_t fport_mask;
};

struct msm_qdss_platform_data {
	struct qdss_source *src_table;
	size_t size;
	uint8_t afamily;
};

#ifdef CONFIG_MSM_QDSS
extern struct qdss_source *qdss_get(const char *name);
extern void qdss_put(struct qdss_source *src);
extern int qdss_enable(struct qdss_source *src);
extern void qdss_disable(struct qdss_source *src);
extern void qdss_disable_sink(void);
extern int qdss_clk_enable(void);
extern void qdss_clk_disable(void);
#else
static inline struct qdss_source *qdss_get(const char *name) { return NULL; }
static inline void qdss_put(struct qdss_source *src) {}
static inline int qdss_enable(struct qdss_source *src) { return -ENOSYS; }
static inline void qdss_disable(struct qdss_source *src) {}
static inline void qdss_disable_sink(void) {}
static inline int qdss_clk_enable(void) { return -ENOSYS; }
static inline void qdss_clk_disable(void) {}
#endif

#ifdef CONFIG_MSM_JTAG
extern void msm_jtag_save_state(void);
extern void msm_jtag_restore_state(void);
#else
static inline void msm_jtag_save_state(void) {}
static inline void msm_jtag_restore_state(void) {}
#endif

#endif
