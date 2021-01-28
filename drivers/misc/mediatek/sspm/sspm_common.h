/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSPM_COMMON_H__
#define __SSPM_COMMON_H__

#include <linux/types.h>

struct sspm_regs {
	void __iomem *cfg;
	unsigned int cfgregsize;
	int irq;
};

struct sspm_work_struct {
	struct work_struct work;
	unsigned int flags;
};


extern struct sspm_regs sspmreg;

extern int sspm_plt_init(void);

extern void memcpy_to_sspm(void __iomem *trg, const void *src, int size);
extern void memcpy_from_sspm(void *trg, const void __iomem *src, int size);
extern unsigned int is_sspm_ready(void);
#endif
