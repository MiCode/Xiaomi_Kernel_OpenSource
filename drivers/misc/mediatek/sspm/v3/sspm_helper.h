/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __SSPM_HELPER_H__
#define __SSPM_HELPER_H__

#include <linux/types.h>

struct sspm_regs {
	void __iomem *cfg;
	void __iomem *mboxshare;
	unsigned int cfgregsize;
	int irq;
};

struct sspm_work_struct {
	struct work_struct work;
	unsigned int flags;
};

extern struct sspm_regs sspmreg;

extern int sspm_plt_init(void);
extern unsigned int is_sspm_ready(void);
#endif
