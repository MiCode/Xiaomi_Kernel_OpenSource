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

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_debugfs.h"

/*
 * PMIC debug level
 */
unsigned int pmic_dbg_level_set(unsigned int level)
{
	gPMICDbgLvl = level > PMIC_LOG_DBG ? PMIC_LOG_DBG : level;
	return 0;
}

/*
 * PMIC reg dump log
 */
void pmic_dump_register(struct seq_file *m)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag =
		&pmu_flags_table[PMU_COMMAND_MAX - 1];
	unsigned int i = 0;

	PMICLOG("dump PMIC register\n");

	for (i = 0; i < (pFlag->offset - 10); i = i + 10) {


		PMICLOG(
		"[0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x\n"
			, i, upmu_get_reg_value(i)
			, i + 2, upmu_get_reg_value(i + 2)
			, i + 4, upmu_get_reg_value(i + 4)
			, i + 6, upmu_get_reg_value(i + 6)
			, i + 8, upmu_get_reg_value(i + 8));

		if (m == NULL)
			continue;

		seq_printf(m,
		"R[0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x [0x%x]=0x%x\n"
			, i, upmu_get_reg_value(i)
			, i + 2, upmu_get_reg_value(i + 2)
			, i + 4, upmu_get_reg_value(i + 4)
			, i + 6, upmu_get_reg_value(i + 6)
			, i + 8, upmu_get_reg_value(i + 8));
	}
}

/*
 * PMIC dump exception status
 */

/* Kernel dump log */
void kernel_dump_exception_reg(void)
{
	/* 1.UVLO off */
	kernel_output_reg(MT6355_TOP_RST_STATUS);
	kernel_output_reg(MT6355_PONSTS);
	kernel_output_reg(MT6355_POFFSTS);
	/* 2.thermal shutdown 150 */
	kernel_output_reg(MT6355_THERMALSTATUS);
	/* 3.power not good */
	kernel_output_reg(MT6355_PGSTATUS0);
	kernel_output_reg(MT6355_PGSTATUS1);
	/* 4.BUCK OC */
	kernel_output_reg(MT6355_PSOCSTATUS);
	kernel_output_reg(MT6355_BUCK_OC_CON0);
	kernel_output_reg(MT6355_BUCK_OC_CON1);
	/* 5.long press shutdown */
	kernel_output_reg(MT6355_STRUP_CON4);
	/* 6.WDTRST */
	kernel_output_reg(MT6355_TOP_RST_MISC);
	/* 7.CLK TRIM */
	kernel_output_reg(MT6355_TOP_CLK_TRIM);
}

/* Kernel & UART dump log */
void both_dump_exception_reg(struct seq_file *s)
{
	/* 1.UVLO off */
	both_output_reg(MT6355_TOP_RST_STATUS);
	both_output_reg(MT6355_PONSTS);
	both_output_reg(MT6355_POFFSTS);
	/* 2.thermal shutdown 150 */
	both_output_reg(MT6355_THERMALSTATUS);
	/* 3.power not good */
	both_output_reg(MT6355_PGSTATUS0);
	both_output_reg(MT6355_PGSTATUS1);
	/* 4.BUCK OC */
	both_output_reg(MT6355_PSOCSTATUS);
	both_output_reg(MT6355_BUCK_OC_CON0);
	both_output_reg(MT6355_BUCK_OC_CON1);
	/* 5.long press shutdown */
	both_output_reg(MT6355_STRUP_CON4);
	/* 6.WDTRST */
	both_output_reg(MT6355_TOP_RST_MISC);
	/* 7.CLK TRIM */
	both_output_reg(MT6355_TOP_CLK_TRIM);
}

/* dump exception reg in kernel and clean status */
int pmic_dump_exception_reg(void)
{
	int ret_val = 0;

	kernel_dump_exception_reg();

	/* clear UVLO off */
	ret_val = pmic_set_register_value(PMIC_TOP_RST_STATUS_CLR, 0xFFFF);

	/* clear thermal shutdown 150 */
	ret_val = pmic_set_register_value(PMIC_RG_STRUP_THR_CLR, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_RG_STRUP_THR_CLR, 0x0);

	/* clear power off status(POFFSTS) and PG status and BUCK OC status */
	ret_val = pmic_set_register_value(PMIC_RG_POFFSTS_CLR, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_RG_POFFSTS_CLR, 0x0);

	/* clear Long press shutdown */
	ret_val = pmic_set_register_value(PMIC_CLR_JUST_RST, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_CLR_JUST_RST, 0x0);
	udelay(200);
	pr_info(PMICTAG "[pmic_boot_status] JUST_PWRKEY_RST=0x%x\n",
		pmic_get_register_value(PMIC_JUST_PWRKEY_RST));

	/* clear WDTRSTB_STATUS */
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	udelay(100);
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);

	/* clear BUCK OC */
	ret_val = pmic_config_interface(MT6355_BUCK_OC_CON0, 0xFF, 0xFF, 0);
	udelay(200);

	/* clear Additional(TBD) */

	/* add mdelay for output the log in buffer */
#if 0	/* TBD */
	mdelay(500);
	mdelay(500);
	mdelay(500);
	mdelay(500);
#endif
	mdelay(500);

	return ret_val;
}
