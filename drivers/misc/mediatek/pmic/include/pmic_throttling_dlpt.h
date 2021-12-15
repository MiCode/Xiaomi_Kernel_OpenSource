/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _PMIC_THROTTLING_DLPT_H_
#define _PMIC_THROTTLING_DLPT_H_

#include <linux/dcache.h>
#include <linux/platform_device.h>

/* just use in suspend flow for important log due to console suspend */
#ifdef CONFIG_MTK_AEE_FEATURE
#if defined PMIC_DEBUG_PR_DBG
#define pmic_spm_crit2(fmt, args...)	pr_notice("[SPM-PMIC] " fmt, ##args)
#else
#define pmic_spm_crit2(fmt, args...)	pr_info("[SPM-PMIC] " fmt, ##args)
#endif
#else
#define pmic_spm_crit2(fmt, args...)
#endif

extern int pmic_throttling_dlpt_init(struct platform_device *pdev);
extern void pmic_throttling_dlpt_suspend(void);
extern void pmic_throttling_dlpt_resume(void);
extern void pmic_throttling_dlpt_debug_init(
	struct platform_device *dev, struct dentry *debug_dir);
extern int get_rac(void);
extern int get_imix(void);
extern int do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur);

#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
extern void mt6355_auxadc_dump_setting_regs(void);
extern void mt6355_auxadc_dump_clk_regs(void);
extern void mt6355_auxadc_dump_channel_regs(void);
#endif


#endif				/* _PMIC_THROTTLING_DLPT_H_ */
