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

#include "linux/delay.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_chip.h>
#ifdef CONFIG_OF
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#endif
#define PMIC_32K_LESS_DETECT_V1      0
#define PMIC_CO_TSX_V1               1

#define PMIC_READ_REGISTER_UINT32(reg)	(*(volatile uint32_t *const)(reg))
#define PMIC_INREG32(x)	PMIC_READ_REGISTER_UINT32((uint32_t *)((void *)(x)))
#define PMIC_WRITE_REGISTER_UINT32(reg, val)	((*(volatile uint32_t *const)(reg)) = (val))
#define PMIC_OUTREG32(x, y)	PMIC_WRITE_REGISTER_UINT32((uint32_t *)((void *)(x)), (uint32_t)(y))

#define PMIC_DRV_Reg32(addr)             PMIC_INREG32(addr)
#define PMIC_DRV_WriteReg32(addr, data)  PMIC_OUTREG32(addr, data)

int PMIC_MD_INIT_SETTING_V1(void)
{
	unsigned int ret = 0;
#if PMIC_32K_LESS_DETECT_V1
	unsigned int pmic_reg = 0;
#endif

#if PMIC_CO_TSX_V1
	struct device_node *modem_temp_node = NULL;
	void __iomem *modem_temp_base = NULL;
#endif

#if PMIC_32K_LESS_DETECT_V1

	/* 32k less crystal auto detect start */
	ret |= pmic_config_interface(0x701E, 0x1, 0x1, 0);
	ret |= pmic_config_interface(0x701E, 0x3, 0xF, 1);
	ret = pmic_read_interface(0x7000, &pmic_reg, 0xffff, 0);
	ret |= pmic_config_interface(0x701E, 0x0, 0x1, 0);
	ret = pmic_config_interface(0xA04, 0x1, 0x1, 3);
	if ((pmic_reg & 0x200) == 0x200) {
		/* VCTCXO on MT6176 , OFF XO on MT6353
		   HW control, use srclken_0 */

		ret = pmic_config_interface(0xA04, 0x0, 0x7, 11);
		pr_err("[PMIC] VCTCXO on MT6176 , OFF XO on MT6353\n");
	} else {
		/*  HW control, use srclken_1, for LP */
		ret = pmic_config_interface(0xA04, 0x1, 0x1, 4);
		ret = pmic_config_interface(0xA04, 0x1, 0x7, 11);
		pr_err("[PMIC] VCTCXO 0x7000=0x%x\n", pmic_reg);
	}
#endif

#if PMIC_CO_TSX_V1
	modem_temp_node = of_find_compatible_node(NULL, NULL, "mediatek,MODEM_TEMP_SHARE");

	if (modem_temp_node == NULL) {
		pr_err("PMIC get modem_temp_node failed\n");
		modem_temp_base = 0;
		return ret;
	}
/* -- MT6353 TBD Start -- */
	modem_temp_base = of_iomap(modem_temp_node, 0);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x011f);
	pr_err("[PMIC] TEMP_SHARE_CTRL:0x%x\n", PMIC_DRV_Reg32(modem_temp_base));
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base + 0x04, 0x013f);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x0);
	pr_err("[PMIC] TEMP_SHARE_CTRL:0x%x _RATIO:0x%x\n", PMIC_DRV_Reg32(modem_temp_base),
	       PMIC_DRV_Reg32(modem_temp_base + 0x04));
/* -- MT6353 TBD End -- */

#endif
	return ret;
}

void PMIC_upmu_set_rg_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(PMIC_RG_BATON_EN_ADDR),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_BATON_EN_MASK),
				    (unsigned int)(PMIC_RG_BATON_EN_SHIFT)
	    );
}

void PMIC_upmu_set_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(PMIC_BATON_TDET_EN_ADDR),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_BATON_TDET_EN_MASK),
				    (unsigned int)(PMIC_BATON_TDET_EN_SHIFT)
	    );
}

unsigned int PMIC_upmu_get_rgs_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(PMIC_RGS_BATON_UNDET_ADDR),
				  (&val),
				  (unsigned int)(PMIC_RGS_BATON_UNDET_MASK),
				  (unsigned int)(PMIC_RGS_BATON_UNDET_SHIFT)
	    );
	return val;
}

int PMIC_check_battery(void)
{
	unsigned int val = 0;

	/* ask shin-shyu programming guide */
	PMIC_upmu_set_rg_baton_en(1);
	PMIC_upmu_set_baton_tdet_en(1);
	val = PMIC_upmu_get_rgs_baton_undet();
	if (val == 0) {
		pr_debug("bat is exist.\n");
		return 1;
	}
	pr_debug("bat NOT exist.\n");
	return 0;
}

int PMIC_POWER_HOLD(unsigned int hold)
{
	if (hold > 1) {
		pr_err("[PMIC_KERNEL] PMIC_POWER_HOLD hold = %d only 0 or 1\n", hold);
		return -1;
	}

	if (hold)
		pr_debug("[PMIC_KERNEL] PMIC_POWER_HOLD ON\n");
	else
		pr_debug("[PMIC_KERNEL] PMIC_POWER_HOLD OFF\n");

	/* MT6335 must keep power hold */
	pmic_config_interface(PMIC_RG_PWRHOLD_ADDR, hold, PMIC_RG_PWRHOLD_MASK,
			      PMIC_RG_PWRHOLD_SHIFT);
	pr_debug("[PMIC_KERNEL] MT6335 PowerHold = 0x%x\n", upmu_get_reg_value(MT6335_PPCCTL0));

	return 0;
}

void PMIC_INIT_SETTING_V1(void)
{
	unsigned int chip_version = 0;
	unsigned int ret = 0;

	chip_version = pmic_get_register_value(PMIC_SWCID);

	/*1.UVLO off */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] TOP_RST_STATUS Reg[0x%x]=0x%x\n",
		 MT6335_TOP_RST_STATUS, upmu_get_reg_value(MT6335_TOP_RST_STATUS));
	/*2.thermal shutdown 150 */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] THERMALSTATUS Reg[0x%x]=0x%x\n",
		 MT6335_THERMALSTATUS, upmu_get_reg_value(MT6335_THERMALSTATUS));
	/*3.power not good */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] PGSTATUS0 Reg[0x%x]=0x%x\n",
		 MT6335_PGSTATUS0, upmu_get_reg_value(MT6335_PGSTATUS0));
	pr_debug("[PMIC_KERNEL][pmic_boot_status] PGSTATUS1 Reg[0x%x]=0x%x\n",
		 MT6335_PGSTATUS1, upmu_get_reg_value(MT6335_PGSTATUS1));
	/*4.buck oc */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] PSOCSTATUS Reg[0x%x]=0x%x\n",
		 MT6335_PSOCSTATUS, upmu_get_reg_value(MT6335_PSOCSTATUS));
	/*5.long press shutdown */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] STRUP_CON4 Reg[0x%x]=0x%x\n",
		 MT6335_STRUP_CON4, upmu_get_reg_value(MT6335_STRUP_CON4));
	/*6.WDTRST */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] TOP_RST_MISC Reg[0x%x]=0x%x\n",
		 MT6335_TOP_RST_MISC, upmu_get_reg_value(MT6335_TOP_RST_MISC));
	/*7.CLK TRIM */
	pr_debug("[PMIC_KERNEL][pmic_boot_status] TOP_CLK_TRIM  Reg[0x%x]=0x%x\n",
		 MT6335_TOP_CLK_TRIM, upmu_get_reg_value(MT6335_TOP_CLK_TRIM));

	is_battery_remove = !PMIC_check_battery();
	is_wdt_reboot_pmic = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	is_wdt_reboot_pmic_chk = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);
	/*--------------------------------------------------------*/
	if (!(upmu_get_reg_value(MT6335_PPCCTL0)))
		pmic_set_register_value(PMIC_RG_PWRHOLD, 0x1);
	pr_err("[PMIC] 6335 PowerHold = 0x%x\n", upmu_get_reg_value(MT6335_PPCCTL0));
	pr_err("[PMIC] 6335 PMIC Chip = 0x%x\n", chip_version);
	pr_err("[PMIC] 2016-02-25...\n");
	pr_err("[PMIC] is_battery_remove =%d is_wdt_reboot=%d\n",
	       is_battery_remove, is_wdt_reboot_pmic);
	pr_err("[PMIC] is_wdt_reboot_chk=%d\n", is_wdt_reboot_pmic_chk);

	pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x1);

/* [3:3]: RG_STRUP_AUXADC_RSTB_SEL; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0xC, 0x1, 0x1, 3);
/* [15:15]: RG_STRUP_ENVTEM_CTRL; 16/01/27 , Kim */
	ret = pmic_config_interface(0x18, 0x1, 0x1, 15);
/* [0:0]: RG_STRUP_VSRAM_DVFS2_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 0);
/* [1:1]: RG_STRUP_VSRAM_DVFS1_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 1);
/* [2:2]: RG_STRUP_VSRAM_CORE_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 2);
/* [3:3]: RG_STRUP_VSRAM_GPU_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 3);
/* [4:4]: RG_STRUP_VSRAM_MD_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 4);
/* [5:5]: RG_STRUP_VUFS18_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 5);
/* [6:6]: RG_STRUP_VEMC_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 6);
/* [7:7]: RG_STRUP_VA12_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 7);
/* [8:8]: RG_STRUP_VA10_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 8);
/* [9:9]: RG_STRUP_VA18_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 9);
/* [10:10]: RG_STRUP_VDRAM_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 10);
/* [11:11]: RG_STRUP_VMODEM_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 11);
/* [12:12]: RG_STRUP_VMD1_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 12);
/* [13:13]: RG_STRUP_VS2_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 13);
/* [14:14]: RG_STRUP_VS1_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 14);
/* [15:15]: RG_STRUP_VCORE_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1C, 0x1, 0x1, 15);
/* [14:14]: RG_STRUP_EXT_PMIC_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1E, 0x1, 0x1, 14);
/* [15:15]: RG_STRUP_VUSB33_PG_H2L_EN; 16/01/27 , Kim */
	ret = pmic_config_interface(0x1E, 0x1, 0x1, 15);
/* [8:8]: RG_RTC_SPAR_DEB_EN; Scott (16/01/14) */
	ret = pmic_config_interface(0x28, 0x1, 0x1, 8);
/* [9:9]: RG_RTC_ALARM_DEB_EN; Scott (16/01/14) */
	ret = pmic_config_interface(0x28, 0x1, 0x1, 9);
/* [1:1]: RG_EN_DRVSEL; 16/01/27 , Kim */
	ret = pmic_config_interface(0x2C, 0x1, 0x1, 1);
/* [2:2]: RG_RST_DRVSEL; 16/01/27 , Kim */
	ret = pmic_config_interface(0x2C, 0x1, 0x1, 2);
/* [0:0]: RG_PWRHOLD; 16/01/27 , Kim, INIT_SET */
	ret = pmic_config_interface(0x2E, 0x1, 0x1, 0);
/* [5:5]: RG_SRCLKEN_IN0_HW_MODE; Nick, From MT6351 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 5);
/* [6:6]: RG_SRCLKEN_IN1_HW_MODE; Nick, From MT6351 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 6);
/* [7:7]: RG_OSC_SEL_HW_MODE; Nick, From MT6351 */
	ret = pmic_config_interface(0x204, 0x1, 0x1, 7);
/* [0:0]: RG_SMT_WDTRSTB_IN; Nick, From MT6351 */
	ret = pmic_config_interface(0x220, 0x1, 0x1, 0);
/* [1:1]: RG_SMT_HOMEKEY; 16/01/27 , Kim */
	ret = pmic_config_interface(0x220, 0x1, 0x1, 1);
/* [6:6]: RG_LDO_CALI_75K_CK_PDN; Waverly */
	ret = pmic_config_interface(0x406, 0x1, 0x1, 6);
/* [8:8]: RG_TRIM_75K_CK_PDN; Waverly */
	ret = pmic_config_interface(0x406, 0x1, 0x1, 8);
/* [2:2]: RG_RTCDET_CK_PDN; Waverly, From MT6351 */
	ret = pmic_config_interface(0x40C, 0x1, 0x1, 2);
/* [3:3]: RG_RTC_75K_CK_PDN; Waverly, From MT6351 */
	ret = pmic_config_interface(0x40C, 0x1, 0x1, 3);
/* [13:13]: RG_RTC32K_1V8_1_O_PDN; 5/30, KH, for MT6337 ACCDET function */
	ret = pmic_config_interface(0x40C, 0x0, 0x1, 13);
/* [1:1]: RG_EFUSE_CK_PDN; Waverly, From MT6351 */
	ret = pmic_config_interface(0x412, 0x1, 0x1, 1);
/* [3:3]: RG_75K_32K_SEL; Waverly */
	ret = pmic_config_interface(0x418, 0x1, 0x1, 3);
/* [7:7]: RG_REG_CK_PDN_HWEN; Waverly */
	ret = pmic_config_interface(0x42A, 0x1, 0x1, 7);
/* [6:6]: RG_FGADC_RST_SRC_SEL; GM3.0 */
	ret = pmic_config_interface(0x606, 0x1, 0x1, 6);
/* [0:0]: RG_WDTRSTB_EN; Kim,2016/01/22 */
	ret = pmic_config_interface(0x60E, 0x1, 0x1, 0);
/* [5:5]: RG_WDTRSTB_DEB; Kim,2016/01/22 */
	ret = pmic_config_interface(0x60E, 0x1, 0x1, 5);
/* [0:0]: RG_SLP_RW_EN; Waverly */
	ret = pmic_config_interface(0xC00, 0x1, 0x1, 0);
/* [1:1]: RG_BUCK_DCM_MODE;  */
	ret = pmic_config_interface(0xE00, 0x1, 0x1, 1);
/* [6:0]: RG_BUCK_VCORE_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0xE1C, 0xF, 0x7F, 0);
/* [7:7]: RG_BUCK_VCORE_SFCHG_FEN; Tim */
	ret = pmic_config_interface(0xE1E, 0x0, 0x1, 7);
/* [14:8]: RG_BUCK_VCORE_SFCHG_RRATE; Tim */
	ret = pmic_config_interface(0xE1E, 0x4, 0x7F, 8);
/* [1:1]: RG_BUCK_VCORE_HW0_OP_EN;  */
	ret = pmic_config_interface(0xE22, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VCORE_HW2_OP_EN;  */
	ret = pmic_config_interface(0xE22, 0x1, 0x1, 3);
/* [1:1]: RG_BUCK_VCORE_HW0_OP_CFG;  */
	ret = pmic_config_interface(0xE28, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VCORE_HW2_OP_CFG;  */
	ret = pmic_config_interface(0xE28, 0x1, 0x1, 3);
/* [8:8]: RG_BUCK_VCORE_ON_OP;  */
	ret = pmic_config_interface(0xE28, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VCORE_LP_OP;  */
	ret = pmic_config_interface(0xE28, 0x0, 0x1, 9);
/* [1:1]: RG_BUCK_VDRAM_HW0_OP_EN;  */
	ret = pmic_config_interface(0xE44, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VDRAM_HW2_OP_EN;  */
	ret = pmic_config_interface(0xE44, 0x1, 0x1, 3);
/* [1:1]: RG_BUCK_VDRAM_HW0_OP_CFG;  */
	ret = pmic_config_interface(0xE4A, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VDRAM_HW2_OP_CFG;  */
	ret = pmic_config_interface(0xE4A, 0x1, 0x1, 3);
/* [8:8]: RG_BUCK_VDRAM_ON_OP;  */
	ret = pmic_config_interface(0xE4A, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VDRAM_LP_OP;  */
	ret = pmic_config_interface(0xE4A, 0x0, 0x1, 9);
/* [6:0]: RG_BUCK_VMD1_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0xE60, 0xF, 0x7F, 0);
/* [6:0]: RG_BUCK_VMD1_SFCHG_FRATE; Tim */
	ret = pmic_config_interface(0xE62, 0x4, 0x7F, 0);
/* [14:8]: RG_BUCK_VMD1_SFCHG_RRATE; Tim */
	ret = pmic_config_interface(0xE62, 0x4, 0x7F, 8);
/* [5:4]: RG_BUCK_VMD1_DVS_TRANS_CTRL; Tim */
	ret = pmic_config_interface(0xE64, 0x1, 0x3, 4);
/* [1:1]: RG_BUCK_VMD1_HW0_OP_EN;  */
	ret = pmic_config_interface(0xE66, 0x0, 0x1, 1);
/* [3:3]: RG_BUCK_VMD1_HW2_OP_EN;  */
	ret = pmic_config_interface(0xE66, 0x0, 0x1, 3);
/* [8:8]: RG_BUCK_VMD1_ON_OP;  */
	ret = pmic_config_interface(0xE6C, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VMD1_LP_OP;  */
	ret = pmic_config_interface(0xE6C, 0x0, 0x1, 9);
/* [6:0]: RG_BUCK_VMODEM_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0xE82, 0xF, 0x7F, 0);
/* [6:0]: RG_BUCK_VMODEM_SFCHG_FRATE; Tim */
	ret = pmic_config_interface(0xE84, 0x4, 0x7F, 0);
/* [14:8]: RG_BUCK_VMODEM_SFCHG_RRATE; Tim */
	ret = pmic_config_interface(0xE84, 0x4, 0x7F, 8);
/* [5:4]: RG_BUCK_VMODEM_DVS_TRANS_CTRL; Tim */
	ret = pmic_config_interface(0xE86, 0x1, 0x3, 4);
/* [1:1]: RG_BUCK_VMODEM_HW0_OP_EN;  */
	ret = pmic_config_interface(0xE88, 0x0, 0x1, 1);
/* [3:3]: RG_BUCK_VMODEM_HW2_OP_EN;  */
	ret = pmic_config_interface(0xE88, 0x0, 0x1, 3);
/* [8:8]: RG_BUCK_VMODEM_ON_OP;  */
	ret = pmic_config_interface(0xE8E, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VMODEM_LP_OP;  */
	ret = pmic_config_interface(0xE8E, 0x0, 0x1, 9);
/* [1:1]: RG_BUCK_VS1_HW0_OP_EN;  */
	ret = pmic_config_interface(0xEAA, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VS1_HW2_OP_EN;  */
	ret = pmic_config_interface(0xEAA, 0x1, 0x1, 3);
/* [1:1]: RG_BUCK_VS1_HW0_OP_CFG;  */
	ret = pmic_config_interface(0xEB0, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VS1_HW2_OP_CFG;  */
	ret = pmic_config_interface(0xEB0, 0x1, 0x1, 3);
/* [8:8]: RG_BUCK_VS1_ON_OP;  */
	ret = pmic_config_interface(0xEB0, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VS1_LP_OP;  */
	ret = pmic_config_interface(0xEB0, 0x0, 0x1, 9);
/* [1:1]: RG_BUCK_VS2_HW0_OP_EN;  */
	ret = pmic_config_interface(0xECC, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VS2_HW2_OP_EN;  */
	ret = pmic_config_interface(0xECC, 0x1, 0x1, 3);
/* [1:1]: RG_BUCK_VS2_HW0_OP_CFG;  */
	ret = pmic_config_interface(0xED2, 0x1, 0x1, 1);
/* [3:3]: RG_BUCK_VS2_HW2_OP_CFG;  */
	ret = pmic_config_interface(0xED2, 0x1, 0x1, 3);
/* [8:8]: RG_BUCK_VS2_ON_OP;  */
	ret = pmic_config_interface(0xED2, 0x1, 0x1, 8);
/* [9:9]: RG_BUCK_VS2_LP_OP;  */
	ret = pmic_config_interface(0xED2, 0x0, 0x1, 9);
/* [6:0]: RG_BUCK_VPA1_SFCHG_FRATE; JL */
	ret = pmic_config_interface(0xEE8, 0x1, 0x7F, 0);
/* [14:8]: RG_BUCK_VPA1_SFCHG_RRATE; JL */
	ret = pmic_config_interface(0xEE8, 0x1, 0x7F, 8);
/* [1:0]: RG_BUCK_VPA1_DVS_TRANS_TD; JL */
	ret = pmic_config_interface(0xEEA, 0x0, 0x3, 0);
/* [5:4]: RG_BUCK_VPA1_DVS_TRANS_CTRL; JL */
	ret = pmic_config_interface(0xEEA, 0x3, 0x3, 4);
/* [6:6]: RG_BUCK_VPA1_DVS_TRANS_ONCE; JL */
	ret = pmic_config_interface(0xEEA, 0x1, 0x1, 6);
/* [6:6]: RG_BUCK_VPA1_DVS_BW_ONCE; JL */
	ret = pmic_config_interface(0xEEC, 0x1, 0x1, 6);
/* [3:2]: RG_BUCK_VPA1_OC_WND; 2/24, Spec. from CW Cheng */
	ret = pmic_config_interface(0xEEE, 0x3, 0x3, 2);
/* [5:0]: RG_BUCK_VPA1_VOSEL_DLC011;  */
	ret = pmic_config_interface(0xEF6, 0x10, 0x3F, 0);
/* [13:8]: RG_BUCK_VPA1_VOSEL_DLC111;  */
	ret = pmic_config_interface(0xEF6, 0x28, 0x3F, 8);
/* [13:8]: RG_BUCK_VPA1_VOSEL_DLC001; 2/16, to solve VPA=0.85V current capability issue */
	ret = pmic_config_interface(0xEF8, 0x2, 0x3F, 8);
/* [6:0]: RG_BUCK_VPA2_SFCHG_FRATE;  */
	ret = pmic_config_interface(0xF18, 0x1, 0x7F, 0);
/* [14:8]: RG_BUCK_VPA2_SFCHG_RRATE;  */
	ret = pmic_config_interface(0xF18, 0x1, 0x7F, 8);
/* [1:0]: RG_BUCK_VPA2_DVS_TRANS_TD;  */
	ret = pmic_config_interface(0xF1A, 0x0, 0x3, 0);
/* [5:4]: RG_BUCK_VPA2_DVS_TRANS_CTRL;  */
	ret = pmic_config_interface(0xF1A, 0x3, 0x3, 4);
/* [6:6]: RG_BUCK_VPA2_DVS_TRANS_ONCE;  */
	ret = pmic_config_interface(0xF1A, 0x1, 0x1, 6);
/* [6:6]: RG_BUCK_VPA2_DVS_BW_ONCE;  */
	ret = pmic_config_interface(0xF1C, 0x1, 0x1, 6);
/* [3:2]: RG_BUCK_VPA2_OC_WND; 2/24, Spec. from CW Cheng */
	ret = pmic_config_interface(0xF1E, 0x3, 0x3, 2);
/* [5:0]: RG_BUCK_VPA2_VOSEL_DLC011;  */
	ret = pmic_config_interface(0xF26, 0x10, 0x3F, 0);
/* [13:8]: RG_BUCK_VPA2_VOSEL_DLC111;  */
	ret = pmic_config_interface(0xF26, 0x28, 0x3F, 8);
/* [13:8]: RG_BUCK_VPA2_VOSEL_DLC001; 2/16, to solve VPA=0.85V current capability issue */
	ret = pmic_config_interface(0xF28, 0x2, 0x3F, 8);
/* [1:0]: RG_VCORE_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF5E, 0x2, 0x3, 0);
/* [4:3]: RG_VMODEM_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF5E, 0x2, 0x3, 3);
/* [6:5]: RG_VMD1_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF5E, 0x2, 0x3, 5);
/* [13:11]: RG_VSRAM_DVFS1_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF5E, 0x2, 0x7, 11);
/* [2:0]: RG_VSRAM_DVFS2_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF60, 0x2, 0x7, 0);
/* [5:3]: RG_VSRAM_VCORE_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF60, 0x2, 0x7, 3);
/* [8:6]: RG_VSRAM_VMD_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF60, 0x2, 0x7, 6);
/* [11:9]: RG_VSRAM_VGPU_SLEEP_VOLTAGE; 2/16, Nick for Elbrus */
	ret = pmic_config_interface(0xF60, 0x2, 0x7, 9);
/* [2:0]: RG_VCORE_SLP; 2/15, Improve jitter. Hung-Mu */
	ret = pmic_config_interface(0xF68, 0x5, 0x7, 0);
/* [3:3]: RG_VCORE_ADRC_FEN; For stability improvement, MT6351 issue. Hung-Mu */
	ret = pmic_config_interface(0xF68, 0x0, 0x1, 3);
/* [6:4]: RG_VCORE_PFM_RIP; 2/16, Hung-Mu for AC spec. */
	ret = pmic_config_interface(0xF6E, 0x1, 0x7, 4);
/* [3:3]: RG_VDRAM_ADRC_FEN; For stability improvement, MT6351 issue. Hung-Mu */
	ret = pmic_config_interface(0xF7C, 0x0, 0x1, 3);
/* [9:6]: RG_VDRAM_NLIM_TRIMMING; 2/24, Terry-CJ, No FT on RG_DRAM_NLIM,
it's trimmed with fixed code of 4'h8. So the register must be over-write to default code with 4'h0 */
	ret = pmic_config_interface(0xF80, 0x0, 0xF, 6);
/* [6:3]: RG_VMODEM_RZSEL1; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xF8E, 0x7, 0xF, 3);
/* [14:11]: RG_VMODEM_CSL; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xF8E, 0x8, 0xF, 11);
/* [2:0]: RG_VMODEM_SLP; 2/15, Improve jitter. Hung-Mu */
	ret = pmic_config_interface(0xF90, 0x5, 0x7, 0);
/* [3:3]: RG_VMODEM_ADRC_FEN; For stability improvement, MT6351 issue. Hung-Mu */
	ret = pmic_config_interface(0xF90, 0x0, 0x1, 3);
/* [5:5]: RG_VMODEM_VC_CLAMP_FEN; 2/14, Hung-Mu */
	ret = pmic_config_interface(0xF90, 0x0, 0x1, 5);
/* [6:4]: RG_VMODEM_PFM_RIP; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xF96, 0x2, 0x7, 4);
/* [6:3]: RG_VMD1_RZSEL1; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xFA2, 0x7, 0xF, 3);
/* [14:11]: RG_VMD1_CSL; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xFA2, 0x8, 0xF, 11);
/* [2:0]: RG_VMD1_SLP; 2/15, Improve jitter. Hung-Mu */
	ret = pmic_config_interface(0xFA4, 0x5, 0x7, 0);
/* [3:3]: RG_VMD1_ADRC_FEN; For stability improvement, MT6351 issue. Hung-Mu */
	ret = pmic_config_interface(0xFA4, 0x0, 0x1, 3);
/* [6:4]: RG_VMD1_PFM_RIP; 2/13, Hung-Mu */
	ret = pmic_config_interface(0xFAA, 0x2, 0x7, 4);
/* [14:11]: RG_VS1_CSL; 2/15, Cindy, for VS1 capability */
	ret = pmic_config_interface(0xFB6, 0x0, 0xF, 11);
/* [8:6]: RG_VS1_PFMOC; 2/15, Cindy */
	ret = pmic_config_interface(0xFB8, 0x7, 0x7, 6);
/* [14:11]: RG_VS2_CSL; 2/15, Cindy, for VS2 capability */
	ret = pmic_config_interface(0xFCA, 0x0, 0xF, 11);
/* [8:6]: RG_VS2_PFMOC; 2/15, Cindy */
	ret = pmic_config_interface(0xFCC, 0x7, 0x7, 6);
/* [5:4]: RG_VPA1_CSMIR; Mason,Corner stability */
	ret = pmic_config_interface(0xFDE, 0x3, 0x3, 4);
/* [7:6]: RG_VPA1_CSL; Mason,Corner current limit */
	ret = pmic_config_interface(0xFDE, 0x1, 0x3, 6);
/* [9:8]: RG_VPA1_SLP; Mason,Corner stability */
	ret = pmic_config_interface(0xFDE, 0x2, 0x3, 8);
/* [10:10]: RG_VPA1_AZC_EN; JL */
	ret = pmic_config_interface(0xFDE, 0x0, 0x1, 10);
/* [15:14]: RG_VPA1_RZSEL; 2/15, JL, for VBAT = 3.0V */
	ret = pmic_config_interface(0xFDE, 0x1, 0x3, 14);
/* [11:8]: RG_VPA1_NLIM_SEL; JL */
	ret = pmic_config_interface(0xFE0, 0x2, 0xF, 8);
/* [3:2]: RG_VPA1_MIN_ON; Mason, Corner Accuracy */
	ret = pmic_config_interface(0xFE2, 0x1, 0x3, 2);
/* [15:14]: RG_VPA1_MIN_PK; JL */
	ret = pmic_config_interface(0xFE2, 0x0, 0x3, 14);
/* [7:0]: RG_VPA1_RSV1; JL */
	ret = pmic_config_interface(0xFE4, 0x2, 0xFF, 0);
/* [15:8]: RG_VPA1_RSV2; JL */
	ret = pmic_config_interface(0xFE4, 0x88, 0xFF, 8);
/* [5:4]: RG_VPA2_CSMIR; JL */
	ret = pmic_config_interface(0xFEA, 0x1, 0x3, 4);
/* [10:10]: RG_VPA2_AZC_EN; JL */
	ret = pmic_config_interface(0xFEA, 0x0, 0x1, 10);
/* [15:14]: RG_VPA2_RZSEL; 2/15, JL, for VBAT = 3.0V */
	ret = pmic_config_interface(0xFEA, 0x1, 0x3, 14);
/* [11:8]: RG_VPA2_NLIM_SEL; JL */
	ret = pmic_config_interface(0xFEC, 0x2, 0xF, 8);
/* [3:2]: RG_VPA2_MIN_ON; JL */
	ret = pmic_config_interface(0xFEE, 0x0, 0x3, 2);
/* [15:14]: RG_VPA2_MIN_PK; JL */
	ret = pmic_config_interface(0xFEE, 0x0, 0x3, 14);
/* [7:0]: RG_VPA2_RSV1; JL */
	ret = pmic_config_interface(0xFF0, 0x2, 0xFF, 0);
/* [15:8]: RG_VPA2_RSV2; JL */
	ret = pmic_config_interface(0xFF0, 0x88, 0xFF, 8);
/* [1:1]: RG_VIO28_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1402, 0x1, 0x1, 1);
/* [3:3]: RG_VIO28_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1402, 0x1, 0x1, 3);
/* [1:1]: RG_VIO28_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x1408, 0x1, 0x1, 1);
/* [3:3]: RG_VIO28_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x1408, 0x1, 0x1, 3);
/* [8:8]: RG_VIO28_GO_ON_OP;  */
	ret = pmic_config_interface(0x1408, 0x1, 0x1, 8);
/* [9:9]: RG_VIO28_GO_LP_OP;  */
	ret = pmic_config_interface(0x1408, 0x0, 0x1, 9);
/* [1:1]: RG_VIO18_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1416, 0x1, 0x1, 1);
/* [3:3]: RG_VIO18_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1416, 0x1, 0x1, 3);
/* [1:1]: RG_VIO18_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x141C, 0x1, 0x1, 1);
/* [3:3]: RG_VIO18_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x141C, 0x1, 0x1, 3);
/* [8:8]: RG_VIO18_GO_ON_OP;  */
	ret = pmic_config_interface(0x141C, 0x1, 0x1, 8);
/* [9:9]: RG_VIO18_GO_LP_OP;  */
	ret = pmic_config_interface(0x141C, 0x0, 0x1, 9);
/* [1:1]: RG_VUFS18_HW0_OP_EN;  */
	ret = pmic_config_interface(0x142A, 0x1, 0x1, 1);
/* [3:3]: RG_VUFS18_HW2_OP_EN;  */
	ret = pmic_config_interface(0x142A, 0x1, 0x1, 3);
/* [1:1]: RG_VUFS18_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x1430, 0x1, 0x1, 1);
/* [3:3]: RG_VUFS18_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x1430, 0x1, 0x1, 3);
/* [8:8]: RG_VUFS18_GO_ON_OP;  */
	ret = pmic_config_interface(0x1430, 0x1, 0x1, 8);
/* [9:9]: RG_VUFS18_GO_LP_OP;  */
	ret = pmic_config_interface(0x1430, 0x0, 0x1, 9);
/* [1:1]: RG_VA10_HW0_OP_EN;  */
	ret = pmic_config_interface(0x143E, 0x1, 0x1, 1);
/* [3:3]: RG_VA10_HW2_OP_EN;  */
	ret = pmic_config_interface(0x143E, 0x1, 0x1, 3);
/* [1:1]: RG_VA10_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x1444, 0x1, 0x1, 1);
/* [3:3]: RG_VA10_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x1444, 0x1, 0x1, 3);
/* [8:8]: RG_VA10_GO_ON_OP;  */
	ret = pmic_config_interface(0x1444, 0x1, 0x1, 8);
/* [9:9]: RG_VA10_GO_LP_OP;  */
	ret = pmic_config_interface(0x1444, 0x0, 0x1, 9);
/* [1:1]: RG_VA12_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1452, 0x1, 0x1, 1);
/* [3:3]: RG_VA12_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1452, 0x1, 0x1, 3);
/* [1:1]: RG_VA12_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x1458, 0x1, 0x1, 1);
/* [3:3]: RG_VA12_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x1458, 0x1, 0x1, 3);
/* [8:8]: RG_VA12_GO_ON_OP;  */
	ret = pmic_config_interface(0x1458, 0x1, 0x1, 8);
/* [9:9]: RG_VA12_GO_LP_OP;  */
	ret = pmic_config_interface(0x1458, 0x0, 0x1, 9);
/* [1:1]: RG_VA18_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1466, 0x1, 0x1, 1);
/* [3:3]: RG_VA18_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1466, 0x1, 0x1, 3);
/* [1:1]: RG_VA18_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x146C, 0x1, 0x1, 1);
/* [3:3]: RG_VA18_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x146C, 0x1, 0x1, 3);
/* [8:8]: RG_VA18_GO_ON_OP;  */
	ret = pmic_config_interface(0x146C, 0x1, 0x1, 8);
/* [9:9]: RG_VA18_GO_LP_OP;  */
	ret = pmic_config_interface(0x146C, 0x0, 0x1, 9);
/* [1:1]: RG_VUSB33_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1478, 0x1, 0x1, 1);
/* [3:3]: RG_VUSB33_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1478, 0x1, 0x1, 3);
/* [1:1]: RG_VUSB33_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x147E, 0x1, 0x1, 1);
/* [3:3]: RG_VUSB33_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x147E, 0x1, 0x1, 3);
/* [8:8]: RG_VUSB33_GO_ON_OP;  */
	ret = pmic_config_interface(0x147E, 0x1, 0x1, 8);
/* [9:9]: RG_VUSB33_GO_LP_OP;  */
	ret = pmic_config_interface(0x147E, 0x0, 0x1, 9);
/* [1:1]: RG_VEMC_HW0_OP_EN;  */
	ret = pmic_config_interface(0x148A, 0x0, 0x1, 1);
/* [3:3]: RG_VEMC_HW2_OP_EN;  */
	ret = pmic_config_interface(0x148A, 0x0, 0x1, 3);
/* [8:8]: RG_VEMC_GO_ON_OP;  */
	ret = pmic_config_interface(0x1490, 0x1, 0x1, 8);
/* [9:9]: RG_VEMC_GO_LP_OP;  */
	ret = pmic_config_interface(0x1490, 0x0, 0x1, 9);
/* [1:1]: RG_VXO22_HW0_OP_EN;  */
	ret = pmic_config_interface(0x149E, 0x1, 0x1, 1);
/* [3:3]: RG_VXO22_HW2_OP_EN;  */
	ret = pmic_config_interface(0x149E, 0x1, 0x1, 3);
/* [1:1]: RG_VXO22_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x14A4, 0x1, 0x1, 1);
/* [3:3]: RG_VXO22_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x14A4, 0x1, 0x1, 3);
/* [8:8]: RG_VXO22_GO_ON_OP;  */
	ret = pmic_config_interface(0x14A4, 0x1, 0x1, 8);
/* [9:9]: RG_VXO22_GO_LP_OP;  */
	ret = pmic_config_interface(0x14A4, 0x0, 0x1, 9);
/* [1:1]: RG_VEFUSE_HW0_OP_EN;  */
	ret = pmic_config_interface(0x14B0, 0x0, 0x1, 1);
/* [3:3]: RG_VEFUSE_HW2_OP_EN;  */
	ret = pmic_config_interface(0x14B0, 0x0, 0x1, 3);
/* [8:8]: RG_VEFUSE_GO_ON_OP;  */
	ret = pmic_config_interface(0x14B6, 0x1, 0x1, 8);
/* [9:9]: RG_VEFUSE_GO_LP_OP;  */
	ret = pmic_config_interface(0x14B6, 0x0, 0x1, 9);
/* [1:1]: RG_VSIM1_HW0_OP_EN;  */
	ret = pmic_config_interface(0x14C4, 0x0, 0x1, 1);
/* [3:3]: RG_VSIM1_HW2_OP_EN;  */
	ret = pmic_config_interface(0x14C4, 0x0, 0x1, 3);
/* [8:8]: RG_VSIM1_GO_ON_OP;  */
	ret = pmic_config_interface(0x14CA, 0x1, 0x1, 8);
/* [9:9]: RG_VSIM1_GO_LP_OP;  */
	ret = pmic_config_interface(0x14CA, 0x0, 0x1, 9);
/* [1:1]: RG_VSIM2_HW0_OP_EN;  */
	ret = pmic_config_interface(0x14D8, 0x0, 0x1, 1);
/* [3:3]: RG_VSIM2_HW2_OP_EN;  */
	ret = pmic_config_interface(0x14D8, 0x0, 0x1, 3);
/* [8:8]: RG_VSIM2_GO_ON_OP;  */
	ret = pmic_config_interface(0x14DE, 0x1, 0x1, 8);
/* [9:9]: RG_VSIM2_GO_LP_OP;  */
	ret = pmic_config_interface(0x14DE, 0x0, 0x1, 9);
/* [1:1]: RG_VCAMAF_HW0_OP_EN;  */
	ret = pmic_config_interface(0x14EC, 0x0, 0x1, 1);
/* [3:3]: RG_VCAMAF_HW2_OP_EN;  */
	ret = pmic_config_interface(0x14EC, 0x0, 0x1, 3);
/* [8:8]: RG_VCAMAF_GO_ON_OP;  */
	ret = pmic_config_interface(0x14F2, 0x1, 0x1, 8);
/* [9:9]: RG_VCAMAF_GO_LP_OP;  */
	ret = pmic_config_interface(0x14F2, 0x0, 0x1, 9);
/* [1:1]: RG_VTOUCH_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1500, 0x1, 0x1, 1);
/* [3:3]: RG_VTOUCH_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1500, 0x1, 0x1, 3);
/* [1:1]: RG_VTOUCH_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x1506, 0x1, 0x1, 1);
/* [3:3]: RG_VTOUCH_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x1506, 0x1, 0x1, 3);
/* [8:8]: RG_VTOUCH_GO_ON_OP;  */
	ret = pmic_config_interface(0x1506, 0x1, 0x1, 8);
/* [9:9]: RG_VTOUCH_GO_LP_OP;  */
	ret = pmic_config_interface(0x1506, 0x0, 0x1, 9);
/* [1:1]: RG_VCAMD1_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1514, 0x0, 0x1, 1);
/* [3:3]: RG_VCAMD1_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1514, 0x0, 0x1, 3);
/* [8:8]: RG_VCAMD1_GO_ON_OP;  */
	ret = pmic_config_interface(0x151A, 0x1, 0x1, 8);
/* [9:9]: RG_VCAMD1_GO_LP_OP;  */
	ret = pmic_config_interface(0x151A, 0x0, 0x1, 9);
/* [1:1]: RG_VCAMD2_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1528, 0x0, 0x1, 1);
/* [3:3]: RG_VCAMD2_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1528, 0x0, 0x1, 3);
/* [8:8]: RG_VCAMD2_GO_ON_OP;  */
	ret = pmic_config_interface(0x152E, 0x1, 0x1, 8);
/* [9:9]: RG_VCAMD2_GO_LP_OP;  */
	ret = pmic_config_interface(0x152E, 0x0, 0x1, 9);
/* [1:1]: RG_VCAMIO_HW0_OP_EN;  */
	ret = pmic_config_interface(0x153C, 0x0, 0x1, 1);
/* [3:3]: RG_VCAMIO_HW2_OP_EN;  */
	ret = pmic_config_interface(0x153C, 0x0, 0x1, 3);
/* [8:8]: RG_VCAMIO_GO_ON_OP;  */
	ret = pmic_config_interface(0x1542, 0x1, 0x1, 8);
/* [9:9]: RG_VCAMIO_GO_LP_OP;  */
	ret = pmic_config_interface(0x1542, 0x0, 0x1, 9);
/* [0:0]: RG_VMIPI_SW_OP_EN;  */
	ret = pmic_config_interface(0x1550, 0x0, 0x1, 0);
/* [2:2]: RG_VMIPI_HW1_OP_EN;  */
	ret = pmic_config_interface(0x1550, 0x1, 0x1, 2);
/* [2:2]: RG_VMIPI_HW1_OP_CFG;  */
	ret = pmic_config_interface(0x1556, 0x0, 0x1, 2);
/* [8:8]: RG_VMIPI_GO_ON_OP;  */
	ret = pmic_config_interface(0x1556, 0x1, 0x1, 8);
/* [9:9]: RG_VMIPI_GO_LP_OP;  */
	ret = pmic_config_interface(0x1556, 0x0, 0x1, 9);
/* [1:1]: RG_VGP3_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1564, 0x0, 0x1, 1);
/* [3:3]: RG_VGP3_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1564, 0x0, 0x1, 3);
/* [8:8]: RG_VGP3_GO_ON_OP;  */
	ret = pmic_config_interface(0x156A, 0x1, 0x1, 8);
/* [9:9]: RG_VGP3_GO_LP_OP;  */
	ret = pmic_config_interface(0x156A, 0x0, 0x1, 9);
/* [1:1]: RG_VCN33_HW0_OP_EN_BT;  */
	ret = pmic_config_interface(0x1578, 0x0, 0x1, 1);
/* [3:3]: RG_VCN33_HW2_OP_EN_BT;  */
	ret = pmic_config_interface(0x1578, 0x0, 0x1, 3);
/* [8:8]: RG_VCN33_GO_ON_OP_BT;  */
	ret = pmic_config_interface(0x157E, 0x1, 0x1, 8);
/* [9:9]: RG_VCN33_GO_LP_OP_BT;  */
	ret = pmic_config_interface(0x157E, 0x0, 0x1, 9);
/* [1:1]: RG_VCN33_HW0_OP_EN_WIFI;  */
	ret = pmic_config_interface(0x1586, 0x0, 0x1, 1);
/* [3:3]: RG_VCN33_HW2_OP_EN_WIFI;  */
	ret = pmic_config_interface(0x1586, 0x0, 0x1, 3);
/* [8:8]: RG_VCN33_GO_ON_OP_WIFI;  */
	ret = pmic_config_interface(0x158C, 0x1, 0x1, 8);
/* [9:9]: RG_VCN33_GO_LP_OP_WIFI;  */
	ret = pmic_config_interface(0x158C, 0x0, 0x1, 9);
/* [1:1]: RG_VCN18_HW0_OP_EN_BT;  */
	ret = pmic_config_interface(0x159A, 0x0, 0x1, 1);
/* [3:3]: RG_VCN18_HW2_OP_EN_BT;  */
	ret = pmic_config_interface(0x159A, 0x0, 0x1, 3);
/* [8:8]: RG_VCN18_GO_ON_OP_BT;  */
	ret = pmic_config_interface(0x15A0, 0x1, 0x1, 8);
/* [9:9]: RG_VCN18_GO_LP_OP_BT;  */
	ret = pmic_config_interface(0x15A0, 0x0, 0x1, 9);
/* [1:1]: RG_VCN18_HW0_OP_EN_WIFI;  */
	ret = pmic_config_interface(0x15A8, 0x0, 0x1, 1);
/* [3:3]: RG_VCN18_HW2_OP_EN_WIFI;  */
	ret = pmic_config_interface(0x15A8, 0x0, 0x1, 3);
/* [8:8]: RG_VCN18_GO_ON_OP_WIFI;  */
	ret = pmic_config_interface(0x15AE, 0x1, 0x1, 8);
/* [9:9]: RG_VCN18_GO_LP_OP_WIFI;  */
	ret = pmic_config_interface(0x15AE, 0x0, 0x1, 9);
/* [1:1]: RG_VCN28_HW0_OP_EN;  */
	ret = pmic_config_interface(0x15BC, 0x0, 0x1, 1);
/* [3:3]: RG_VCN28_HW2_OP_EN;  */
	ret = pmic_config_interface(0x15BC, 0x0, 0x1, 3);
/* [8:8]: RG_VCN28_GO_ON_OP;  */
	ret = pmic_config_interface(0x15C2, 0x1, 0x1, 8);
/* [9:9]: RG_VCN28_GO_LP_OP;  */
	ret = pmic_config_interface(0x15C2, 0x0, 0x1, 9);
/* [1:1]: RG_VIBR_HW0_OP_EN;  */
	ret = pmic_config_interface(0x15CE, 0x0, 0x1, 1);
/* [3:3]: RG_VIBR_HW2_OP_EN;  */
	ret = pmic_config_interface(0x15CE, 0x0, 0x1, 3);
/* [8:8]: RG_VIBR_GO_ON_OP;  */
	ret = pmic_config_interface(0x15D4, 0x1, 0x1, 8);
/* [9:9]: RG_VIBR_GO_LP_OP;  */
	ret = pmic_config_interface(0x15D4, 0x0, 0x1, 9);
/* [1:1]: RG_VBIF28_HW0_OP_EN;  */
	ret = pmic_config_interface(0x15E0, 0x0, 0x1, 1);
/* [3:3]: RG_VBIF28_HW2_OP_EN;  */
	ret = pmic_config_interface(0x15E0, 0x0, 0x1, 3);
/* [8:8]: RG_VBIF28_GO_ON_OP;  */
	ret = pmic_config_interface(0x15E6, 0x1, 0x1, 8);
/* [9:9]: RG_VBIF28_GO_LP_OP;  */
	ret = pmic_config_interface(0x15E6, 0x0, 0x1, 9);
/* [0:0]: RG_VFE28_SW_OP_EN;  */
	ret = pmic_config_interface(0x1602, 0x0, 0x1, 0);
/* [2:2]: RG_VFE28_HW1_OP_EN;  */
	ret = pmic_config_interface(0x1602, 0x1, 0x1, 2);
/* [2:2]: RG_VFE28_HW1_OP_CFG;  */
	ret = pmic_config_interface(0x1608, 0x0, 0x1, 2);
/* [8:8]: RG_VFE28_GO_ON_OP;  */
	ret = pmic_config_interface(0x1608, 0x1, 0x1, 8);
/* [9:9]: RG_VFE28_GO_LP_OP;  */
	ret = pmic_config_interface(0x1608, 0x0, 0x1, 9);
/* [1:1]: RG_VMCH_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1614, 0x0, 0x1, 1);
/* [3:3]: RG_VMCH_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1614, 0x0, 0x1, 3);
/* [8:8]: RG_VMCH_GO_ON_OP;  */
	ret = pmic_config_interface(0x161A, 0x1, 0x1, 8);
/* [9:9]: RG_VMCH_GO_LP_OP;  */
	ret = pmic_config_interface(0x161A, 0x0, 0x1, 9);
/* [1:1]: RG_VMC_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1628, 0x0, 0x1, 1);
/* [3:3]: RG_VMC_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1628, 0x0, 0x1, 3);
/* [8:8]: RG_VMC_GO_ON_OP;  */
	ret = pmic_config_interface(0x162E, 0x1, 0x1, 8);
/* [9:9]: RG_VMC_GO_LP_OP;  */
	ret = pmic_config_interface(0x162E, 0x0, 0x1, 9);
/* [0:0]: RG_VRF18_1_SW_OP_EN;  */
	ret = pmic_config_interface(0x163C, 0x0, 0x1, 0);
/* [2:2]: RG_VRF18_1_HW1_OP_EN;  */
	ret = pmic_config_interface(0x163C, 0x1, 0x1, 2);
/* [2:2]: RG_VRF18_1_HW1_OP_CFG;  */
	ret = pmic_config_interface(0x1642, 0x0, 0x1, 2);
/* [8:8]: RG_VRF18_1_GO_ON_OP;  */
	ret = pmic_config_interface(0x1642, 0x1, 0x1, 8);
/* [9:9]: RG_VRF18_1_GO_LP_OP;  */
	ret = pmic_config_interface(0x1642, 0x0, 0x1, 9);
/* [0:0]: RG_VRF18_2_SW_OP_EN;  */
	ret = pmic_config_interface(0x1650, 0x0, 0x1, 0);
/* [2:2]: RG_VRF18_2_HW1_OP_EN;  */
	ret = pmic_config_interface(0x1650, 0x1, 0x1, 2);
/* [2:2]: RG_VRF18_2_HW1_OP_CFG;  */
	ret = pmic_config_interface(0x1656, 0x0, 0x1, 2);
/* [8:8]: RG_VRF18_2_GO_ON_OP;  */
	ret = pmic_config_interface(0x1656, 0x1, 0x1, 8);
/* [9:9]: RG_VRF18_2_GO_LP_OP;  */
	ret = pmic_config_interface(0x1656, 0x0, 0x1, 9);
/* [0:0]: RG_VRF12_SW_OP_EN;  */
	ret = pmic_config_interface(0x1664, 0x0, 0x1, 0);
/* [2:2]: RG_VRF12_HW1_OP_EN;  */
	ret = pmic_config_interface(0x1664, 0x1, 0x1, 2);
/* [2:2]: RG_VRF12_HW1_OP_CFG;  */
	ret = pmic_config_interface(0x166A, 0x0, 0x1, 2);
/* [8:8]: RG_VRF12_GO_ON_OP;  */
	ret = pmic_config_interface(0x166A, 0x1, 0x1, 8);
/* [9:9]: RG_VRF12_GO_LP_OP;  */
	ret = pmic_config_interface(0x166A, 0x0, 0x1, 9);
/* [6:0]: RG_VSRAM_DVFS1_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0x169C, 0xF, 0x7F, 0);
/* [1:1]: RG_VSRAM_DVFS1_HW0_OP_EN;  */
	ret = pmic_config_interface(0x16A2, 0x0, 0x1, 1);
/* [3:3]: RG_VSRAM_DVFS1_HW2_OP_EN;  */
	ret = pmic_config_interface(0x16A2, 0x1, 0x1, 3);
/* [1:1]: RG_VSRAM_DVFS1_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x16A8, 0x1, 0x1, 1);
/* [3:3]: RG_VSRAM_DVFS1_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x16A8, 0x1, 0x1, 3);
/* [8:8]: RG_VSRAM_DVFS1_GO_ON_OP;  */
	ret = pmic_config_interface(0x16A8, 0x1, 0x1, 8);
/* [9:9]: RG_VSRAM_DVFS1_GO_LP_OP;  */
	ret = pmic_config_interface(0x16A8, 0x0, 0x1, 9);
/* [6:0]: RG_VSRAM_DVFS2_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0x16BC, 0xF, 0x7F, 0);
/* [1:1]: RG_VSRAM_DVFS2_HW0_OP_EN;  */
	ret = pmic_config_interface(0x16C2, 0x0, 0x1, 1);
/* [3:3]: RG_VSRAM_DVFS2_HW2_OP_EN;  */
	ret = pmic_config_interface(0x16C2, 0x0, 0x1, 3);
/* [8:8]: RG_VSRAM_DVFS2_GO_ON_OP;  */
	ret = pmic_config_interface(0x16C8, 0x1, 0x1, 8);
/* [9:9]: RG_VSRAM_DVFS2_GO_LP_OP;  */
	ret = pmic_config_interface(0x16C8, 0x0, 0x1, 9);
/* [6:0]: RG_VSRAM_VCORE_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0x16DC, 0xF, 0x7F, 0);
/* [1:1]: RG_VSRAM_VCORE_HW0_OP_EN;  */
	ret = pmic_config_interface(0x16E2, 0x1, 0x1, 1);
/* [3:3]: RG_VSRAM_VCORE_HW2_OP_EN;  */
	ret = pmic_config_interface(0x16E2, 0x1, 0x1, 3);
/* [1:1]: RG_VSRAM_VCORE_HW0_OP_CFG;  */
	ret = pmic_config_interface(0x16E8, 0x1, 0x1, 1);
/* [3:3]: RG_VSRAM_VCORE_HW2_OP_CFG;  */
	ret = pmic_config_interface(0x16E8, 0x1, 0x1, 3);
/* [8:8]: RG_VSRAM_VCORE_GO_ON_OP;  */
	ret = pmic_config_interface(0x16E8, 0x1, 0x1, 8);
/* [9:9]: RG_VSRAM_VCORE_GO_LP_OP;  */
	ret = pmic_config_interface(0x16E8, 0x0, 0x1, 9);
/* [6:0]: RG_VSRAM_VGPU_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0x16FC, 0xF, 0x7F, 0);
/* [1:1]: RG_VSRAM_VGPU_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1702, 0x0, 0x1, 1);
/* [3:3]: RG_VSRAM_VGPU_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1702, 0x0, 0x1, 3);
/* [8:8]: RG_VSRAM_VGPU_GO_ON_OP;  */
	ret = pmic_config_interface(0x1708, 0x1, 0x1, 8);
/* [9:9]: RG_VSRAM_VGPU_GO_LP_OP;  */
	ret = pmic_config_interface(0x1708, 0x0, 0x1, 9);
/* [6:0]: RG_VSRAM_VMD_VOSEL_SLEEP; 2/16, Nick, for Elbrus, 0.5V */
	ret = pmic_config_interface(0x171C, 0xF, 0x7F, 0);
/* [1:1]: RG_VSRAM_VMD_HW0_OP_EN;  */
	ret = pmic_config_interface(0x1722, 0x0, 0x1, 1);
/* [3:3]: RG_VSRAM_VMD_HW2_OP_EN;  */
	ret = pmic_config_interface(0x1722, 0x0, 0x1, 3);
/* [8:8]: RG_VSRAM_VMD_GO_ON_OP;  */
	ret = pmic_config_interface(0x1728, 0x1, 0x1, 8);
/* [9:9]: RG_VSRAM_VMD_GO_LP_OP;  */
	ret = pmic_config_interface(0x1728, 0x0, 0x1, 9);
/* [0:0]: RG_DCM_MODE;  */
	ret = pmic_config_interface(0x1740, 0x1, 0x1, 0);
/* [0:0]: RG_VIO28_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1742, 0x0, 0x1, 0);
/* [0:0]: RG_VIO18_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1744, 0x0, 0x1, 0);
/* [0:0]: RG_VUFS18_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1746, 0x0, 0x1, 0);
/* [0:0]: RG_VA10_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1748, 0x0, 0x1, 0);
/* [0:0]: RG_VA12_CK_SW_MODE;  */
	ret = pmic_config_interface(0x174A, 0x0, 0x1, 0);
/* [0:0]: RG_VSRAM_DVFS1_CK_SW_MODE;  */
	ret = pmic_config_interface(0x174C, 0x0, 0x1, 0);
/* [0:0]: RG_VSRAM_DVFS2_CK_SW_MODE;  */
	ret = pmic_config_interface(0x174E, 0x0, 0x1, 0);
/* [0:0]: RG_VSRAM_VCORE_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1750, 0x0, 0x1, 0);
/* [0:0]: RG_VSRAM_VGPU_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1752, 0x0, 0x1, 0);
/* [0:0]: RG_VSRAM_VMD_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1754, 0x0, 0x1, 0);
/* [0:0]: RG_VA18_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1756, 0x0, 0x1, 0);
/* [0:0]: RG_VUSB33_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1758, 0x0, 0x1, 0);
/* [0:0]: RG_VEMC_CK_SW_MODE;  */
	ret = pmic_config_interface(0x175A, 0x0, 0x1, 0);
/* [0:0]: RG_VXO22_CK_SW_MODE;  */
	ret = pmic_config_interface(0x175C, 0x0, 0x1, 0);
/* [0:0]: RG_VEFUSE_CK_SW_MODE;  */
	ret = pmic_config_interface(0x175E, 0x0, 0x1, 0);
/* [0:0]: RG_VSIM1_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1760, 0x0, 0x1, 0);
/* [0:0]: RG_VSIM2_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1762, 0x0, 0x1, 0);
/* [0:0]: RG_VCAMAF_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1764, 0x0, 0x1, 0);
/* [0:0]: RG_VTOUCH_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1766, 0x0, 0x1, 0);
/* [0:0]: RG_VCAMD1_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1768, 0x0, 0x1, 0);
/* [0:0]: RG_VCAMD2_CK_SW_MODE;  */
	ret = pmic_config_interface(0x176A, 0x0, 0x1, 0);
/* [0:0]: RG_VCAMIO_CK_SW_MODE;  */
	ret = pmic_config_interface(0x176C, 0x0, 0x1, 0);
/* [0:0]: RG_VMIPI_CK_SW_MODE;  */
	ret = pmic_config_interface(0x176E, 0x0, 0x1, 0);
/* [0:0]: RG_VGP3_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1770, 0x0, 0x1, 0);
/* [0:0]: RG_VCN33_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1772, 0x0, 0x1, 0);
/* [0:0]: RG_VCN18_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1774, 0x0, 0x1, 0);
/* [0:0]: RG_VCN28_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1776, 0x0, 0x1, 0);
/* [0:0]: RG_VIBR_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1778, 0x0, 0x1, 0);
/* [0:0]: RG_VBIF28_CK_SW_MODE;  */
	ret = pmic_config_interface(0x177A, 0x0, 0x1, 0);
/* [0:0]: RG_VFE28_CK_SW_MODE;  */
	ret = pmic_config_interface(0x177C, 0x0, 0x1, 0);
/* [0:0]: RG_VMCH_CK_SW_MODE;  */
	ret = pmic_config_interface(0x177E, 0x0, 0x1, 0);
/* [0:0]: RG_VMC_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1780, 0x0, 0x1, 0);
/* [0:0]: RG_VRF18_1_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1782, 0x0, 0x1, 0);
/* [0:0]: RG_VRF18_2_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1784, 0x0, 0x1, 0);
/* [0:0]: RG_VRF12_CK_SW_MODE;  */
	ret = pmic_config_interface(0x1786, 0x0, 0x1, 0);
/* [7:7]: RG_VSRAM_DVFS1_PLCUR_EN; 5/19, issue of VSRAM instability triggered by WDTRSTB_IN on Elbrus EVB.
Default value ECO to 1'h0, and set to 1'h1 by PMIC init. */
	ret = pmic_config_interface(0x183A, 0x1, 0x1, 7);
/* [7:7]: RG_VSRAM_DVFS2_PLCUR_EN; 5/19, issue of VSRAM instability triggered by WDTRSTB_IN on Elbrus EVB.
Default value ECO to 1'h0, and set to 1'h1 by PMIC init. */
	ret = pmic_config_interface(0x183C, 0x1, 0x1, 7);
/* [7:7]: RG_VSRAM_VCORE_PLCUR_EN; 5/19, issue of VSRAM instability triggered by WDTRSTB_IN on Elbrus EVB.
Default value ECO to 1'h0, and set to 1'h1 by PMIC init. */
	ret = pmic_config_interface(0x183E, 0x1, 0x1, 7);
/* [7:7]: RG_VSRAM_VGPU_PLCUR_EN; 5/19, issue of VSRAM instability triggered by WDTRSTB_IN on Elbrus EVB.
Default value ECO to 1'h0, and set to 1'h1 by PMIC init. */
	ret = pmic_config_interface(0x1840, 0x1, 0x1, 7);
/* [7:7]: RG_VSRAM_VMD_PLCUR_EN; 5/19, issue of VSRAM instability triggered by WDTRSTB_IN on Elbrus EVB.
Default value ECO to 1'h0, and set to 1'h1 by PMIC init. */
	ret = pmic_config_interface(0x1842, 0x1, 0x1, 7);
/* [15:15]: RG_VIO18_FT_DNMC_EN;  */
	ret = pmic_config_interface(0x1862, 0x1, 0x1, 15);
/* [15:15]: AUXADC_CK_AON; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0x24A8, 0x0, 0x1, 15);
/* [11:0]: AUXADC_AVG_NUM_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24B0, 0x83, 0xFFF, 0);
/* [15:15]: AUXADC_AVG_NUM_SEL_WAKEUP; 20160129 by wlchen */
	ret = pmic_config_interface(0x24B0, 0x1, 0x1, 15);
/* [9:0]: AUXADC_SPL_NUM_LARGE; 3/7, Wei-Lin, to fix BATID low V accuracy */
	ret = pmic_config_interface(0x24B2, 0x20, 0x3FF, 0);
/* [14:14]: AUXADC_SPL_NUM_SEL_BAT_TEMP; 3/7, Wei-Lin, to fix BATID low V accuracy */
	ret = pmic_config_interface(0x24B6, 0x1, 0x1, 14);
/* [9:0]: AUXADC_SPL_NUM_CH0; 20160129 by Wei-Lin */
	ret = pmic_config_interface(0x24B8, 0x7, 0x3FF, 0);
/* [9:0]: AUXADC_SPL_NUM_CH3; 20160213 by Wei-Lin */
	ret = pmic_config_interface(0x24BA, 0x20, 0x3FF, 0);
/* [9:0]: AUXADC_SPL_NUM_CH7; 20160129 by Wei-Lin */
	ret = pmic_config_interface(0x24BC, 0x7, 0x3FF, 0);
/* [14:12]: AUXADC_AVG_NUM_CH0; 20160129 by wlchen */
	ret = pmic_config_interface(0x24BE, 0x6, 0x7, 12);
/* [5:4]: AUXADC_TRIM_CH2_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24C0, 0x1, 0x3, 4);
/* [7:6]: AUXADC_TRIM_CH3_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24C0, 0x3, 0x3, 6);
/* [9:8]: AUXADC_TRIM_CH4_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24C0, 0x1, 0x3, 8);
/* [15:14]: AUXADC_TRIM_CH7_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24C0, 0x2, 0x3, 14);
/* [7:6]: AUXADC_TRIM_CH11_SEL; 20160129 by wlchen */
	ret = pmic_config_interface(0x24C2, 0x3, 0x3, 6);
/* [14:14]: AUXADC_START_SHADE_EN; 20160129 by wlchen */
	ret = pmic_config_interface(0x24D4, 0x1, 0x1, 14);
/* [1:0]: AUXADC_DATA_REUSE_SEL; 2/26, ZF */
	ret = pmic_config_interface(0x24D8, 0x0, 0x3, 0);
/* [8:8]: AUXADC_DATA_REUSE_EN; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0x24D8, 0x1, 0x1, 8);
/* [9:0]: AUXADC_MDRT_DET_PRD; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0x2530, 0x40, 0x3FF, 0);
/* [15:15]: AUXADC_MDRT_DET_EN; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0x2530, 0x1, 0x1, 15);
/* [2:2]: AUXADC_MDRT_DET_WKUP_EN; From ZF's golden setting 20160129 */
	ret = pmic_config_interface(0x2534, 0x1, 0x1, 2);
/* [0:0]: AUXADC_MDRT_DET_START_SEL; From L1's request for AUXADC,My Tu,4/25 */
	ret = pmic_config_interface(0x2538, 0x1, 0x1, 0);
/* [4:0]: RG_LBAT_INT_VTH; Ricky: E1 only */
	ret = pmic_config_interface(0x2616, 0x2, 0x1F, 0);

/*****************************************************
 * below programming is used for MD setting
 *****************************************************/
	PMIC_MD_INIT_SETTING_V1();
}
