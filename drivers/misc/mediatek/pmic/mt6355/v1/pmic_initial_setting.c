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

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_chip.h>
#include <mt-plat/mtk_rtc.h>

#include <linux/io.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_buck.h"
#if defined(CONFIG_MACH_MT6757)
#include <mtk_clkbuf_ctl.h>
#endif

#define PMIC_32K_LESS_DETECT_V1      0
#define PMIC_CO_TSX_V1               1

#define PMIC_DRV_Reg32(addr)             readl(addr)
#define PMIC_DRV_WriteReg32(addr, data)  writel(data, addr)

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
		/* VCTCXO on MT6176, OFF XO on MT6353 */
		/* HW control, use srclken_0 */
		ret = pmic_config_interface(0xA04, 0x0, 0x7, 11);
		pr_info("[PMIC] VCTCXO on MT6176 , OFF XO on MT6353\n");
	} else {
		/*  HW control, use srclken_1, for LP */
		ret = pmic_config_interface(0xA04, 0x1, 0x1, 4);
		ret = pmic_config_interface(0xA04, 0x1, 0x7, 11);
		pr_info("[PMIC] VCTCXO 0x7000=0x%x\n", pmic_reg);
	}
#endif

#if PMIC_CO_TSX_V1
	modem_temp_node = of_find_compatible_node(NULL
				, NULL, "mediatek,MODEM_TEMP_SHARE");

	if (modem_temp_node == NULL) {
		pr_info("PMIC get modem_temp_node failed\n");
		return ret;
	}

	modem_temp_base = of_iomap(modem_temp_node, 0);

#if defined(CONFIG_MACH_MT6757)
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x033f);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base));
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base + 0x04, 0x013f);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x0);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x _RATIO:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base)
		, PMIC_DRV_Reg32(modem_temp_base + 0x04));
#endif

#if defined(CONFIG_MACH_MT6759)
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x033f);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base));
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base + 0x04, 0x013f);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x0);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x _RATIO:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base)
		, PMIC_DRV_Reg32(modem_temp_base + 0x04));
#endif

#if defined(CONFIG_MACH_MT6758)
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x033f);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base));
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base + 0x04, 0x013f);
	/* modem temp */
	PMIC_DRV_WriteReg32(modem_temp_base, 0x0);
	pr_info("[PMIC] TEMP_SHARE_CTRL:0x%x _RATIO:0x%x\n"
		, PMIC_DRV_Reg32(modem_temp_base)
		, PMIC_DRV_Reg32(modem_temp_base + 0x04));
#endif
#endif
	return ret;
}

int PMIC_check_wdt_status(void)
{
	unsigned int ret = 0;

	is_wdt_reboot_pmic = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x8);
	is_wdt_reboot_pmic_chk = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_CLR, 0x8);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC_SET, 0x1);
	ret = pmic_get_register_value(PMIC_RG_WDTRSTB_EN);
	return ret;
}

int PMIC_check_pwrhold_status(void)
{
	unsigned int val = 0;

	pmic_read_interface(PMIC_RG_PWRHOLD_ADDR, &val, PMIC_RG_PWRHOLD_MASK,
			      PMIC_RG_PWRHOLD_SHIFT);
	return val;
}

int PMIC_check_battery(void)
{
	unsigned int val = 0;

	/* ask shin-shyu programming guide */
	mt6355_upmu_set_rg_baton_en(1);
	/*PMIC_upmu_set_baton_tdet_en(1);*/
	val = mt6355_upmu_get_rgs_baton_undet();
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
		pr_info("[PMIC_KERNEL] %s hold = %d only 0 or 1\n"
			, __func__
			, hold);
		return -1;
	}

	if (hold)
		PMICLOG("[PMIC_KERNEL] %s ON\n", __func__);
	else
		PMICLOG("[PMIC_KERNEL] %s OFF\n", __func__);

	/* MT6355 must keep power hold */
	pmic_config_interface_nolock(PMIC_RG_PWRHOLD_ADDR, hold
		, PMIC_RG_PWRHOLD_MASK, PMIC_RG_PWRHOLD_SHIFT);
	PMICLOG("[PMIC_KERNEL] MT6355 PowerHold = 0x%x\n"
		, upmu_get_reg_value(MT6355_PPCCTL0));

	return 0;
}

void PMIC_PWROFF_SEQ_SETTING(void)
{
	int ret = 0;

	ret = pmic_set_register_value(PMIC_RG_CPS_W_KEY, 0x4729);
	ret = pmic_set_register_value(PMIC_RG_VS2_DSA, 0x02);
	ret = pmic_set_register_value(PMIC_RG_VSRAM_CORE_DSA, 0x03);
	ret = pmic_set_register_value(PMIC_RG_VSRAM_MD_DSA, 0x03);
	ret = pmic_set_register_value(PMIC_RG_VSRAM_GPU_DSA, 0x1F);
	ret = pmic_set_register_value(PMIC_RG_VCORE_DSA, 0x04);
	ret = pmic_set_register_value(PMIC_RG_VGPU_DSA, 0x04);
	ret = pmic_set_register_value(PMIC_RG_VMODEM_DSA, 0x06);
	ret = pmic_set_register_value(PMIC_RG_VS1_DSA, 0x06);
	ret = pmic_set_register_value(PMIC_RG_VA10_DSA, 0x07);
	ret = pmic_set_register_value(PMIC_RG_VA12_DSA, 0x1F);
	ret = pmic_set_register_value(PMIC_RG_VIO18_DSA, 0x07);
	ret = pmic_set_register_value(PMIC_RG_VEMC_DSA, 0x07);
	ret = pmic_set_register_value(PMIC_RG_VUFS18_DSA, 0x1F);
	ret = pmic_set_register_value(PMIC_RG_VIO28_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_VSRAM_PROC_DSA, 0x04);
	ret = pmic_set_register_value(PMIC_RG_VPROC11_DSA, 0x06);
	ret = pmic_set_register_value(PMIC_RG_VPROC12_DSA, 0x06);
	ret = pmic_set_register_value(PMIC_RG_EXT_PMIC_DSA, 0x06);
	ret = pmic_set_register_value(PMIC_RG_VDRAM1_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_VDRAM2_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_VUSB33_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_VXO22_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_VXO18_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_BUCK_RSV_DSA, 0x08);
	ret = pmic_set_register_value(PMIC_RG_CPS_W_KEY, 0x0000);
}

#if defined(CONFIG_MACH_MT6757)
void PMIC_LP_INIT_SETTING(void)
{
	int ret = 0;
	/* for VPROC power down issue */
	pmic_config_interface(PMIC_RG_STRUP_VPROC12_PG_ENB_ADDR,
			      0x1,
			      PMIC_RG_STRUP_VPROC12_PG_ENB_MASK,
			      PMIC_RG_STRUP_VPROC12_PG_ENB_SHIFT);
	/*--suspend--*/
	ret = pmic_buck_vproc11_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vproc12_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_buck_vcore_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vdram1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vs1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_gpu_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_md_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsram_core_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SRCLKEN1, 1, HW_OFF);
#if defined(CONFIG_MTK_RTC)
	if (is_clk_buf_from_pmic()) {
		if (crystal_exist_status() == true)
			ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_OFF);
		else
			ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_LP);
	} else {
		ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_OFF);
	}
#else
	ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_LP);
#endif
	ret = pmic_ldo_vxo18_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vusb33_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);
	/*--deepidle--*/
	ret = pmic_buck_vproc11_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vproc12_lp(SRCLKEN2, 1, HW_OFF);
	ret = pmic_buck_vcore_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vdram1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vs1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_gpu_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_md_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsram_core_lp(SPM, 1, SPM_ON);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vxo22_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vxo18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vusb33_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN2, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);

}
#elif defined(CONFIG_MACH_MT6759)
void PMIC_LP_INIT_SETTING(void)
{
	int ret = 0;

	/*--suspend--*/
	ret = pmic_buck_vproc11_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vproc12_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vcore_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vdram1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vs1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsram_gpu_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_md_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_core_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SRCLKEN1, 1, HW_OFF);

#if defined(CONFIG_MTK_RTC)
	if ((crystal_exist_status()) == true)
		ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_OFF);
	else
#endif
		ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_LP);

	ret = pmic_ldo_vxo18_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_va18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vusb33_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);

/*--deep idle--*/
	ret = pmic_buck_vproc11_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vproc12_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vcore_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vdram1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vs1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsram_gpu_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_md_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_core_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vxo22_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vxo18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_va18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vusb33_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN2, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);

}
#elif defined(CONFIG_MACH_MT6758)
void PMIC_LP_INIT_SETTING(void)
{
	int ret = 0;

/*--Suspend--*/
	ret = pmic_buck_vproc11_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vproc12_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vcore_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vdram1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vs1_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsram_gpu_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsram_md_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_core_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SW, 1, SW_OFF);

#if defined(CONFIG_MTK_RTC)
	if ((crystal_exist_status()) == true)
		ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_OFF);
	else
#endif
		ret = pmic_ldo_vxo22_lp(SRCLKEN0, 1, HW_LP);

	ret = pmic_ldo_vxo18_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_va18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vusb33_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN0, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);

/*--Deepidle--*/
	ret = pmic_buck_vproc11_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vproc12_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vcore_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vgpu_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vdram1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vdram2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SW, 1, SW_OFF);
	ret = pmic_buck_vs1_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vs2_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_buck_vpa_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_proc_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsram_gpu_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsram_md_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsram_core_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vfe28_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vtcxo24_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vxo22_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vxo18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vrf18_1_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf18_2_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vrf12_lp(SRCLKEN1, 1, HW_OFF);
	ret = pmic_ldo_vcn33_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn28_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcn18_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcama2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamio_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vcamd2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_va10_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_va12_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_va18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsim1_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vsim2_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vldo28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmipi_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vio28_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vmc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vmch_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vemc_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vufs18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vusb33_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vbif28_lp(SRCLKEN2, 1, HW_OFF);
	ret = pmic_ldo_vio18_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vgp_lp(SW, 1, SW_OFF);
	ret = pmic_ldo_vgp2_lp(SW, 1, SW_OFF);
}
#else
void PMIC_LP_INIT_SETTING(void)
{
	/* nothing */
}
#endif
