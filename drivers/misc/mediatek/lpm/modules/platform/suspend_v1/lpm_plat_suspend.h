/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_PLAT_SUSPEND_H__
#define __LPM_PLAT_SUSPEND_H__

int lpm_model_suspend_init(void);

extern void gpio_dump_regs(void);
extern void pll_if_on(void);
extern void subsys_if_on(void);

#endif
