/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_MTK_LDVT)
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     1
#define SLP_SUSPEND_LOG_EN          1
#else
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     0
#define SLP_SUSPEND_LOG_EN          1
#endif

#define	SPM_SUSPEND_PLAT_SLP_DP		(1<<0u)
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

enum gs_flag {
	GS_PMIC = (0x1 << 0),
	GS_PMIC_6315 = (0x1 << 1),
	GS_CG   = (0x1 << 2),
	GS_DCM  = (0x1 << 3),
	/* GS_ALL will need to be modified, if the gs_dump_flag is changed */
	GS_ALL  = (GS_PMIC | GS_PMIC_6315 | GS_CG | GS_DCM),
};

#endif  /* __MTK_SLEEP_INTERNAL_H__ */
