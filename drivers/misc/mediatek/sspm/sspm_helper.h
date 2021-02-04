/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SSPM_HELPER_H__
#define __SSPM_HELPER_H__

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

enum SEMAPHORE_FLAG {
	NR_FLAG = 0,
};

extern struct sspm_regs sspmreg;

extern int sspm_plt_init(void);

/* @group_id: the group want to swap in tcm and run. */
extern void memcpy_to_sspm(void __iomem *trg, const void *src, int size);
extern void memcpy_from_sspm(void *trg, const void __iomem *src, int size);
extern int get_sspm_semaphore(int flag);
extern int release_sspm_semaphore(int flag);
extern void sspm_schedule_work(struct sspm_work_struct *sspm_ws);
extern unsigned int is_sspm_ready(void);
#endif
