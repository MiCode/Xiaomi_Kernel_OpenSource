/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __MACH_QDSS_H
#define __MACH_QDSS_H

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
