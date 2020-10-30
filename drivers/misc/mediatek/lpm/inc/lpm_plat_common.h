/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_PLAT_COMMON_H__
#define __LPM_PLAT_COMMON_H__

#include <linux/types.h>
#include <linux/kernel.h>

enum LPM_PLAT_TRACE_TYPE {
	LPM_PLAT_TRACE_SYSRAM,
};

struct LPM_PLAT_TRACE {
	size_t (*read)(unsigned long offset, void *buf, size_t sz);
	size_t (*write)(unsigned long offset, const void *buf, size_t sz);
};

/* NOTICE - this enum must synchronize with kernel site */
enum MT_PLAT_DRAM_TYPE {
	SPMFW_LP4_2CH_3200 = 0,
	SPMFW_LP4X_2CH_3600,
	SPMFW_LP3_1CH_1866,
	SPMFW_TYPE_NOT_FOUND,
};

struct lpm_irqremain {
	size_t count;
	unsigned int *irqs;
	unsigned int *wakeup_src_cat;
	unsigned int *wakeup_src;
};

extern int lpm_irqremain_get(struct lpm_irqremain **irqs);
extern void lpm_irqremain_put(struct lpm_irqremain *irqs);
extern void lpm_irqremain_list_release(void);

int lpm_platform_trace_get(int type, struct LPM_PLAT_TRACE *trace);

#endif
