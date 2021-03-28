/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT6781_SUSPEND_H__
#define __MT6781_SUSPEND_H__

int mt6781_model_suspend_init(void);

extern void gpio_dump_regs(void);
extern void pll_if_on(void);
extern void subsys_if_on(void);

#endif
