/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
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
extern void __init mtk_cpuidle_framework_init(void);
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
extern void ccci_set_spm_mdsrc_cb(void (*md_clock_src_cb)(u8 set));
extern void ccci_set_spm_md_sleep_cb(bool (*spm_md_sleep_cb)(void));
#endif

extern u32 spm_vcorefs_get_MD_status(void);
#if IS_ENABLED(CONFIG_MTK_MDPM_LEGACY)
extern void mdpm_register_md_status_cb(u32 (*get_MD_status)(void));
#endif

extern void register_spm_resource_req_func(bool (*spm_resource_req_func)(unsigned int user,
						unsigned int req_mask));
extern bool spm_resource_req(unsigned int user, unsigned int req_mask);

#endif  /* __MTK_SLEEP_INTERNAL_H__ */
