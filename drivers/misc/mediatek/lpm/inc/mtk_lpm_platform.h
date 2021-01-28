/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_PLATFORM_H__
#define __MTK_LPM_PLATFORM_H__

#include <linux/types.h>
#include <linux/kernel.h>

enum MT_LPM_PLAT_TRACE_TYPE {
	MT_LPM_PLAT_TRACE_SYSRAM,
};

struct MTK_LPM_PLAT_TRACE {
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

struct mtk_lpm_irqremain {
	size_t count;
	unsigned int *irqs;
	unsigned int *wakeup_src_cat;
	unsigned int *wakeup_src;
};

extern int mtk_lpm_irqremain_get(struct mtk_lpm_irqremain **irqs);
extern void mtk_lpm_irqremain_put(struct mtk_lpm_irqremain *irqs);
extern void mtk_lpm_irqremain_list_release(void);

#endif
