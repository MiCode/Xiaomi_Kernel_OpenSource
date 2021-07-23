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

#ifndef __PMIC_DEBUGFS_H__
#define __PMIC_DEBUGFS_H__

#include <linux/dcache.h>

#define adb_output_reg(reg) \
	seq_printf(s, "[pmic_boot_status] " #reg " Reg[0x%x]=0x%x\n",	\
		   reg, upmu_get_reg_value(reg))
#define kernel_output_reg(reg) \
	pr_notice("[pmic_boot_status] " #reg " Reg[0x%x]=0x%x\n",	\
		  reg, upmu_get_reg_value(reg))
#define both_output_reg(reg) \
	do { \
		seq_printf(s, "[pmic_boot_status] " #reg " Reg[0x%x]=0x%x\n", \
			reg, upmu_get_reg_value(reg)); \
		pr_notice("[pmic_boot_status] " #reg " Reg[0x%x]=0x%x\n", \
			reg, upmu_get_reg_value(reg)); \
	} while (0)

#define PMIC_LOG_DBG     4
#define PMIC_LOG_INFO    3
#define PMIC_LOG_NOT     2
#define PMIC_LOG_WARN    1
#define PMIC_LOG_ERR     0

/* extern variable */
extern struct dentry *mtk_pmic_dir;
extern unsigned int gPMICDbgLvl;

/* extern function */
extern void kernel_dump_exception_reg(void);
#ifdef CONFIG_MTK_PMIC_COMMON
extern int pmic_debug_init(struct platform_device *dev);
extern unsigned int pmic_dbg_level_set(unsigned int level);
extern void pmic_cmp_register(struct seq_file *m);
extern void pmic_dump_register(struct seq_file *m);
extern void both_dump_exception_reg(struct seq_file *s);
extern int pmic_dump_exception_reg(void);
#endif /*--CONFIG_MTK_PMIC_COMMON--*/

#endif	/* __PMIC_DEBUGFS_H__ */
