/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#define INIT_SETTING_VERIFIED	1

/*
 * PMIC debug level
 */
unsigned int pmic_dbg_level_set(unsigned int level)
{
	unsigned char Dlevel = (level >> 0) & 0xF;
	unsigned char HKlevel = (level >> 4) & 0xF;
	unsigned char IRQlevel = (level >> 8) & 0xF;
	unsigned char REGlevel = (level >> 12) & 0xF;

	gPMICDbgLvl = Dlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : Dlevel;
	gPMICHKDbgLvl = HKlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : HKlevel;
	gPMICIRQDbgLvl = IRQlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : IRQlevel;
	gPMICREGDbgLvl = REGlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : REGlevel;
	return 0;
}

/*
 * PMIC reg cmp log
 */
struct pmic_setting {
	unsigned short addr;
	unsigned short val;
	unsigned short mask;
	unsigned char shift;
};

static struct pmic_setting init_setting[] = {
	{0x20, 0xA, 0xA, 0},
	{0x24, 0x1F00, 0x1F00, 0},
	{0x30, 0x1, 0x1, 0},
	{0x32, 0x1, 0x1, 0},
	{0x94, 0x0, 0xFFFF, 0},
	{0x10C, 0x10, 0x10, 0},
	{0x112, 0x4, 0x4, 0},
	{0x118, 0x8, 0x8, 0},
	{0x14A, 0x20, 0x20, 0},
	{0x198, 0x0, 0x1FF, 0},
	{0x1B2, 0x3, 0x3, 0},
	{0x3B0, 0x0, 0x300, 0},
	{0x7A6, 0xE000, 0xE000, 0},
	{0x7A8, 0x0, 0x200, 0},
	{0x98A, 0x80, 0x80, 0},
	{0x992, 0xF00, 0xF00, 0},
	{0xA08, 0x1, 0x1, 0},
	{0xA12, 0x1E0, 0x41E0, 0},
	{0xA24, 0xFFFF, 0xFFFF, 0},
	{0xA26, 0xFFC0, 0xFFC0, 0},
	{0xA2C, 0xC0DF, 0xC0DF, 0},
	{0xA2E, 0xEBC0, 0xEBC0, 0},
	{0xA34, 0x8000, 0x8000, 0},
	{0xA40, 0x1400, 0x7C00, 0},
	{0xA9A, 0x2E11, 0xFF11, 0},
	{0xA9E, 0x4000, 0x4000, 0},
	{0xF8C, 0x115, 0x115, 0},
	{0x1188, 0x0, 0x8000, 0},
	{0x1198, 0x13, 0x3FF, 0},
	{0x119E, 0x6000, 0x7000, 0},
	{0x11D4, 0x0, 0x2, 0},
	{0x1212, 0x0, 0x2, 0},
	{0x1224, 0x0, 0x2, 0},
	{0x1238, 0x0, 0x2, 0},
	{0x124A, 0x0, 0x2, 0},
	{0x125C, 0x0, 0x2, 0},
	{0x125E, 0x8000, 0x8000, 0},
	{0x1260, 0x1, 0xFFF, 0},
	{0x1262, 0x4, 0x4, 0},
	{0x1412, 0x8, 0x8, 0},
	{0x1440, 0x200, 0x200, 0},
	{0x148E, 0x18, 0x7F, 0},
	{0x150E, 0x20, 0x7F, 0},
	{0x168E, 0x18, 0x7F, 0},
	{0x170E, 0x18, 0x7F, 0},
	{0x178E, 0x18, 0x7F, 0},
	{0x18B0, 0x2C, 0x7F, 0},
	{0x1918, 0x2810, 0x3F3F, 0},
	{0x191A, 0x800, 0x3F00, 0},
	{0x191E, 0x1, 0x1, 0},
	{0x198A, 0x5004, 0x502C, 0},
	{0x198C, 0x11, 0x3F, 0},
	{0x198E, 0x1E0, 0x1E0, 0},
	{0x1990, 0xFB, 0xFF, 0},
	{0x1994, 0x10, 0x38, 0},
	{0x1996, 0x2004, 0xA02C, 0},
	{0x1998, 0x11, 0x3F, 0},
	{0x199A, 0xFB78, 0xFF78, 0},
	{0x199E, 0x2, 0x7, 0},
	{0x19A0, 0x1050, 0x50F1, 0},
	{0x19A2, 0x5535, 0xFF3F, 0},
	{0x19A4, 0xF, 0xF, 0},
	{0x19A6, 0x20, 0xFF, 0},
	{0x19A8, 0x200, 0x200, 0},
	{0x19AC, 0x4208, 0x46D8, 0},
	{0x19AE, 0x2ADC, 0x7FFE, 0},
	{0x19B0, 0x3C00, 0x3C00, 0},
	{0x19B4, 0x20FD, 0xFFFF, 0},
	{0x19B6, 0x200, 0x200, 0},
	{0x19BC, 0xC00, 0xE00, 0},
	{0x19C0, 0x10, 0x30, 0},
	{0x19C2, 0x10, 0x30, 0},
	{0x1A08, 0x4200, 0x4680, 0},
	{0x1A0A, 0x2ADC, 0x7FFE, 0},
	{0x1A0C, 0x3C00, 0x3C00, 0},
	{0x1A10, 0x20FD, 0xFFFF, 0},
	{0x1A14, 0x4208, 0x46D8, 0},
	{0x1A16, 0x2DC, 0x7FFE, 0},
	{0x1A18, 0x3C00, 0x3C00, 0},
	{0x1A1C, 0x2000, 0xFF00, 0},
	{0x1A1E, 0x200, 0x200, 0},
	{0x1A20, 0x4200, 0x4680, 0},
	{0x1A22, 0x2ACA, 0x7FFE, 0},
	{0x1A24, 0x3C00, 0x3C00, 0},
	{0x1A28, 0x2000, 0xFF00, 0},
	{0x1A2C, 0x20, 0x74, 0},
	{0x1A2E, 0x1E, 0x1E, 0},
	{0x1A30, 0x42, 0xFF, 0},
	{0x1A32, 0x480, 0x7E0, 0},
	{0x1A34, 0x20, 0x74, 0},
	{0x1A36, 0x1E, 0x1E, 0},
	{0x1A38, 0x42, 0xFF, 0},
	{0x1A3A, 0x480, 0x7E0, 0},
	{0x1A3C, 0x14C, 0x3CC, 0},
	{0x1A3E, 0x23C, 0x3FC, 0},
	{0x1A40, 0xC400, 0xFF00, 0},
	{0x1A42, 0x80, 0xFF, 0},
	{0x1A44, 0x702C, 0xFF2C, 0},
	{0x1A66, 0x1C0, 0x3C0, 0},
	{0x1B0E, 0xF, 0xF, 0},
	{0x1B10, 0x1, 0x1, 0},
	{0x1B88, 0x20, 0x8020, 0},
	{0x1B98, 0x20, 0x8020, 0},
	{0x1B9C, 0x4000, 0x4000, 0},
	{0x1BA8, 0x21, 0x8021, 0},
	{0x1BB8, 0x1420, 0x9C20, 0},
	{0x1BBC, 0x2, 0x2, 0},
	{0x1BC8, 0x20, 0x8020, 0},
	{0x1BD8, 0x20, 0x8020, 0},
	{0x1C08, 0x20, 0x8020, 0},
	{0x1C1A, 0x20, 0x8020, 0},
	{0x1C2A, 0x20, 0x8020, 0},
	{0x1C3A, 0x20, 0x8020, 0},
	{0x1C4A, 0x20, 0x8020, 0},
	{0x1C5A, 0x20, 0x8020, 0},
	{0x1C88, 0x20, 0x8020, 0},
	{0x1C8C, 0x4000, 0x4000, 0},
	{0x1C98, 0x20, 0x8020, 0},
	{0x1CA8, 0x20, 0x8020, 0},
	{0x1CB8, 0x20, 0x8020, 0},
	{0x1CC8, 0x20, 0x8020, 0},
	{0x1CD8, 0x20, 0x8020, 0},
	{0x1D08, 0x20, 0x8020, 0},
	{0x1D1A, 0x20, 0x8020, 0},
	{0x1D2A, 0x20, 0x8020, 0},
	{0x1D3A, 0x20, 0x8020, 0},
	{0x1D3E, 0x4000, 0x4000, 0},
	{0x1D4A, 0x20, 0x8020, 0},
	{0x1D5A, 0x20, 0x8020, 0},
	{0x1D88, 0x20, 0x8020, 0},
	{0x1D98, 0x20, 0x8020, 0},
	{0x1E88, 0x20, 0x8020, 0},
	{0x1E8C, 0x10, 0x7F, 0},
	{0x1EA6, 0x20, 0x8020, 0},
	{0x1EAA, 0x10, 0x7F, 0},
	{0x1F08, 0x20, 0x8020, 0},
	{0x1F0C, 0x3C, 0x7F, 0},
	{0x1F2C, 0x20, 0x8020, 0},
	{0x200A, 0x8, 0xC, 0},
	{0x202C, 0x8, 0xC, 0},
};

void pmic_cmp_register(struct seq_file *m)
{
#if INIT_SETTING_VERIFIED
	unsigned int i = 0, val = 0;

	seq_puts(m, "cmp: PMIC_addr\tInit_Val\tRead_val\tMASK\tshift\n");

	for (i = 0; i < ARRAY_SIZE(init_setting); i++) {
		pmic_read_interface(
			init_setting[i].addr, &val,
			init_setting[i].mask, init_setting[i].shift);
		if (val != init_setting[i].val) {
			seq_printf(m, "cmp: 0x%x\tval=0x%x\trval=0x%x\t0x%x\t%d\n"
				   , init_setting[i].addr
				   , init_setting[i].val
				   , val
				   , init_setting[i].mask
				   , init_setting[i].shift);
		}
	}
#else
	seq_printf(m, "%s: disable cmp PMIC register with initial setting\n"
		   , __func__);
#endif
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

	for (i = 0; i < pFlag->offset; i = i + 10) {
		pr_notice("Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n"
			  , i, upmu_get_reg_value(i)
			  , i + 2, upmu_get_reg_value(i + 2)
			  , i + 4, upmu_get_reg_value(i + 4)
			  , i + 6, upmu_get_reg_value(i + 6)
			  , i + 8, upmu_get_reg_value(i + 8));
		if (m != NULL) {
			seq_printf(m,
				"Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n"
				, i, upmu_get_reg_value(i)
				, i + 2, upmu_get_reg_value(i + 2)
				, i + 4, upmu_get_reg_value(i + 4)
				, i + 6, upmu_get_reg_value(i + 6)
				, i + 8, upmu_get_reg_value(i + 8));
		}
	}
}

/*
 * PMIC dump exception status
 */

/* Kernel dump log */
void kernel_dump_exception_reg(void)
{
	/* 1.UVLO off */
	kernel_output_reg(MT6359_TOP_RST_STATUS);
	kernel_output_reg(MT6359_PONSTS);
	kernel_output_reg(MT6359_POFFSTS);
	/* 2.thermal shutdown 150 */
	kernel_output_reg(MT6359_THERMALSTATUS);
	/* 3.power not good */
	kernel_output_reg(MT6359_PG_SDN_STS0);
	/* 4.BUCK OC status */
	kernel_output_reg(MT6359_OC_SDN_STS0);
	kernel_output_reg(MT6359_BUCK_TOP_OC_CON0);
	/* 4.5 BUCK OC shutdown control */
	kernel_output_reg(MT6359_BUCK_TOP_ELR0);
	/* 5.long press shutdown */
	kernel_output_reg(MT6359_STRUP_CON4);
	/* 6.WDTRST */
	kernel_output_reg(MT6359_TOP_RST_MISC);
	/* 7.CLK TRIM */
	kernel_output_reg(MT6359_TOP_CLK_TRIM);
}

/* Kernel & UART dump log */
void both_dump_exception_reg(struct seq_file *s)
{
	/* 1.UVLO off */
	both_output_reg(MT6359_TOP_RST_STATUS);
	both_output_reg(MT6359_PONSTS);
	both_output_reg(MT6359_POFFSTS);
	/* 2.thermal shutdown 150 */
	both_output_reg(MT6359_THERMALSTATUS);
	/* 3.power not good */
	both_output_reg(MT6359_PG_SDN_STS0);
	/* 4.BUCK OC */
	both_output_reg(MT6359_OC_SDN_STS0);
	both_output_reg(MT6359_BUCK_TOP_OC_CON0);
	/* 4.5 BUCK OC shutdown control */
	both_output_reg(MT6359_BUCK_TOP_ELR0);
	/* 5.long press shutdown */
	both_output_reg(MT6359_STRUP_CON4);
	/* 6.WDTRST */
	both_output_reg(MT6359_TOP_RST_MISC);
	/* 7.CLK TRIM */
	both_output_reg(MT6359_TOP_CLK_TRIM);
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
	PMICLOG(PMICTAG "[pmic_boot_status] JUST_PWRKEY_RST=0x%x\n",
		pmic_get_register_value(PMIC_JUST_PWRKEY_RST));

	/* clear WDTRSTB_STATUS */
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	udelay(100);
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);

	/* clear BUCK OC */
	ret_val = pmic_config_interface(MT6359_BUCK_TOP_OC_CON0, 0xFF, 0xFF, 0);
	udelay(200);

	/* clear Additional(TBD) */

	/* add mdelay for output the log in buffer */
	mdelay(500);

	return ret_val;
}
