/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/delay.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_chip.h>

#include <linux/io.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_buck.h"

#define LP_INIT_SETTING_VERIFIED 1

unsigned int g_pmic_chip_version = 1;

int PMIC_MD_INIT_SETTING_V1(void)
{
	/* No need for PMIC MT6359 */
	return 0;
}

int PMIC_check_wdt_status(void)
{
	unsigned int ret = 0;

	is_wdt_reboot_pmic = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	udelay(50);
	is_wdt_reboot_pmic_chk = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x1);
	ret = pmic_get_register_value(PMIC_RG_WDTRSTB_EN);
	return ret;
}

int PMIC_check_pwrhold_status(void)
{
	unsigned int val = 0;

	pmic_read_interface(PMIC_RG_PWRHOLD_ADDR, &val,
		PMIC_RG_PWRHOLD_MASK,
		PMIC_RG_PWRHOLD_SHIFT);
	return val;
}

int PMIC_check_battery(void)
{
	unsigned int val = 0;

	/* ask shin-shyu programming guide */
	pmic_set_register_value(PMIC_RG_BATON_EN, 1);
	/*PMIC_upmu_set_baton_tdet_en(1);*/
	val = pmic_get_register_value(PMIC_AD_BATON_UNDET);
	if (val == 0) {
		pr_debug("bat is exist.\n");
		is_battery_remove = 0;
		return 1;
	}
	pr_debug("bat NOT exist.\n");
	is_battery_remove = 1;
	return 0;
}

int PMIC_POWER_HOLD(unsigned int hold)
{
	if (hold > 1) {
		pr_notice("[%s] hold = %d only 0 or 1\n", __func__, hold);
		return -1;
	}

	if (hold)
		PMICLOG("[%s] ON\n", __func__);
	else
		PMICLOG("[%s] OFF\n", __func__);

	pmic_config_interface_nolock(PMIC_RG_PWRHOLD_ADDR, hold,
				     PMIC_RG_PWRHOLD_MASK,
				     PMIC_RG_PWRHOLD_SHIFT);
	PMICLOG("[PMIC_KERNEL] PowerHold = 0x%x\n"
		, pmic_get_register_value(PMIC_RG_PWRHOLD));

	return 0;
}

unsigned int PMIC_CHIP_VER(void)
{
	unsigned int ret = 0;
	unsigned short chip_ver = 0;

	chip_ver = pmic_get_register_value(PMIC_SWCID);

	ret = ((chip_ver & 0x00F0) >> 4);

	return ret;
}

#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
void PMIC_LP_INIT_SETTING(void)
{
	g_pmic_chip_version = PMIC_CHIP_VER();
#if LP_INIT_SETTING_VERIFIED
	/*SODI3*/
	pmic_buck_vcore_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu11_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vs1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);

	/*Deepidle*/
	pmic_buck_vcore_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu11_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vs1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	/* SRCLKEN14 HW_ON no need to setting */
	/*pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_ON);*/
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN2, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);

	pr_info("[%s] Chip Ver = %d\n", __func__, g_pmic_chip_version);
#endif /*LP_INIT_SETTING_VERIFIED*/
}
#elif defined(CONFIG_MACH_MT6873)
void PMIC_LP_INIT_SETTING(void)
{
	g_pmic_chip_version = PMIC_CHIP_VER();
#if LP_INIT_SETTING_VERIFIED
	/* For RF setting: If PL set Multi-user mode, need to sync it */
	/*SODI3*/
	pmic_buck_vcore_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu11_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vs1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);

	/*Deepidle*/
	pmic_buck_vcore_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu11_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_buck_vs1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	/* SRCLKEN14 HW_ON no need to setting */
	/*pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_ON);*/
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN2, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);
	pr_info("[%s] Chip Ver = %d\n", __func__, g_pmic_chip_version);
#endif /*LP_INIT_SETTING_VERIFIED*/
}
#elif defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
void PMIC_LP_INIT_SETTING(void)
{
	g_pmic_chip_version = PMIC_CHIP_VER();
#if LP_INIT_SETTING_VERIFIED
	/*SODI3*/
	pmic_buck_vcore_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
#ifdef CONFIG_REGULATOR_MT6315 /* MTK_5G_B_MT6360_MT6315 */
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF); /* for CPU-L */
#else
	pmic_buck_vproc2_lp(SRCLKEN0, 1, 1, HW_LP); /* for APU */
#endif
	pmic_buck_vgpu11_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
#ifdef CONFIG_REGULATOR_MT6315 /* MTK_5G_B_MT6360_MT6315 */
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF); /* for VRF09 */
#else
	pmic_buck_vmodem_lp(SW, 1, 1, SW_OFF); /* for CPU-L */
#endif
	pmic_buck_vs1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);

	/*Deepidle*/
	pmic_buck_vcore_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vpu_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vproc1_lp(SW, 1, 1, SW_OFF);
#ifdef CONFIG_REGULATOR_MT6315 /* MTK_5G_B_MT6360_MT6315 */
	pmic_buck_vproc2_lp(SW, 1, 1, SW_OFF); /* for CPU-L */
#else
	pmic_buck_vproc2_lp(SRCLKEN2, 1, 1, HW_LP); /* for APU */
#endif
	pmic_buck_vgpu11_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vgpu12_lp(SW, 1, 1, SW_OFF);
#ifdef CONFIG_REGULATOR_MT6315 /* MTK_5G_B_MT6360_MT6315 */
	pmic_buck_vmodem_lp(SRCLKEN1, 0, 1, HW_OFF); /* for VRF09 */
#else
	pmic_buck_vmodem_lp(SW, 1, 1, SW_OFF); /* for CPU-L */
#endif
	pmic_buck_vs1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vpa_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_proc2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_others_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vsram_md_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcamio_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vm18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn13_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_va09_lp(SRCLKEN1, 1, 1, HW_LP);
	/* SRCLKEN14 HW_ON no need to setting */
	/*pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_ON);*/
	pmic_ldo_vfe28_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vbif28_lp(SRCLKEN2, 1, 1, HW_OFF);
	pmic_ldo_vaud18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vcn33_1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vcn33_2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vio28_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vufs_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vibr_lp(SW, 1, 1, SW_OFF);
	pr_info("[%s] Chip Ver = %d\n", __func__, g_pmic_chip_version);
#endif /*LP_INIT_SETTING_VERIFIED*/
}
#endif
