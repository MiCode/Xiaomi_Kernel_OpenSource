/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_SLEEP_INTERNAL_H__
#define __MTK_SLEEP_INTERNAL_H__

#include <linux/kernel.h>

#define WAKE_SRC_CFG_KEY            (1U << 31)

#define slp_read(addr)              __raw_readl((void __force __iomem *)(addr))
#define slp_write(addr, val)        mt65xx_reg_sync_writel(val, addr)

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     1
#define SLP_SUSPEND_LOG_EN          1
#else
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     0
#define SLP_SUSPEND_LOG_EN          1
#endif

/**************************************
 * SW code for sleep
 **************************************/
int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on);
unsigned int slp_get_wake_reason(void);
void slp_module_init(void);

/**************************************
 * External functions
 **************************************/
extern void subsys_if_on(void);
extern void pll_if_on(void);
extern void gpio_dump_regs(void);

#endif  /* __MTK_SLEEP_INTERNAL_H__ */
