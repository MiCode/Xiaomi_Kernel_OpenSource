/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _PMIC_THROTTLING_DLPT_H_
#define _PMIC_THROTTLING_DLPT_H_

#define pmic_emerg(fmt, args...)		pr_emerg("[SPM-PMIC] " fmt, ##args)
#define pmic_alert(fmt, args...)		pr_alert("[SPM-PMIC] " fmt, ##args)
#define pmic_crit(fmt, args...)		pr_crit("[SPM-PMIC] " fmt, ##args)
#define pmic_err(fmt, args...)		pr_err("[SPM-PMIC] " fmt, ##args)
#define pmic_warn(fmt, args...)		pr_warn("[SPM-PMIC] " fmt, ##args)
#define pmic_notice(fmt, args...)	pr_notice("[SPM-PMIC] " fmt, ##args)
#define pmic_info(fmt, args...)		pr_info("[SPM-PMIC] " fmt, ##args)
#define pmic_debug(fmt, args...)		pr_info("[SPM-PMIC] " fmt, ##args)	/* pr_debug show nothing */

/* just use in suspend flow for important log due to console suspend */
#if defined PMIC_DEBUG_PR_DBG
#define pmic_spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	pmic_crit(fmt, ##args);		\
} while (0)
#else
#define pmic_spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	pmic_debug(fmt, ##args);		\
} while (0)
#endif

extern int pmic_throttling_dlpt_init(void);
extern void low_battery_protect_init(void);
extern void battery_oc_protect_init(void);
extern void bat_percent_notify_init(void);
extern void dlpt_notify_init(void);
extern void pmic_throttling_dlpt_suspend(void);
extern void pmic_throttling_dlpt_resume(void);
extern void pmic_throttling_dlpt_debug_init(struct platform_device *dev, struct dentry *debug_dir);
extern void bat_h_int_handler(void);
extern void bat_l_int_handler(void);
extern void fg_cur_h_int_handler(void);
extern void fg_cur_l_int_handler(void);
#endif				/* _PMIC_THROTTLING_DLPT_H_ */
