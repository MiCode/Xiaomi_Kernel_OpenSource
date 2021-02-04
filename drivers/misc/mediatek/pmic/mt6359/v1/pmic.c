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

#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <mt-plat/mtk_devinfo.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"

void record_md_vosel(void)
{
	g_vmodem_vosel = pmic_get_register_value(PMIC_RG_BUCK_VMODEM_VOSEL);
	pr_info("[%s] vmodem_vosel = 0x%x\n", __func__, g_vmodem_vosel);
}

/* [Export API] */
void vmd1_pmic_setting_on(void)
{
	unsigned int vsram_md_vosel = 0x4F; /*993750*/

	/* 1.Call PMIC driver API configure VMODEM voltage */
	if (g_vmodem_vosel != 0) {
		pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL,
			g_vmodem_vosel);
		pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_VOSEL,
			vsram_md_vosel);
	} else {
		pr_notice("[%s] vmodem vosel has not recorded!\n", __func__);
		g_vmodem_vosel =
			pmic_get_register_value(PMIC_RG_BUCK_VMODEM_VOSEL);
		pr_info("[%s] vmodem_vosel = 0x%x\n",
			__func__, g_vmodem_vosel);
	}
	if (pmic_get_register_value(PMIC_DA_VMODEM_VOSEL) != g_vmodem_vosel)
		pr_notice("[%s] vmodem vosel = 0x%x, da_vosel = 0x%x",
			__func__,
			pmic_get_register_value(PMIC_RG_BUCK_VMODEM_VOSEL),
			pmic_get_register_value(PMIC_DA_VMODEM_VOSEL));
}

void vmd1_pmic_setting_off(void)
{
	PMICLOG("%s\n", __func__);
}

void pmic_enable_smart_reset(unsigned char smart_en,
	unsigned char smart_sdn_en)
{
	pmic_set_register_value(PMIC_RG_SMART_RST_MODE, smart_en);
	pmic_set_register_value(PMIC_RG_SMART_RST_SDN_EN, smart_sdn_en);
	pr_info("[%s] smart_en:%d, smart_sdn_en:%d\n",
		__func__, smart_en, smart_sdn_en);
}

void enable_bat_temp_det(bool en)
{
	pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_FROZE_EN, !en);
}

static unsigned int pmic_scp_set_regulator(struct mtk_regulator mt_reg,
	enum PMU_FLAGS_LIST vosel_reg, unsigned int voltage, bool is_sleep_vol)
{
	unsigned int min_uV = mt_reg.desc.min_uV;
	unsigned int uV_step = mt_reg.desc.uV_step;
	unsigned int n_voltages = mt_reg.desc.n_voltages;
	unsigned short set_step = 0;
	unsigned short get_step = 0;

	set_step = (voltage - min_uV) / uV_step;
	if (voltage < min_uV || set_step >= n_voltages) {
		pr_notice("[%s] SSHUB_%s Set Wrong voltage=%duV is unsupportable range %d-%duV\n"
			  , __func__
			  , mt_reg.desc.name
			  , voltage
			  , min_uV
			  , (n_voltages * uV_step + min_uV));
		return voltage;
	}
	pr_info("SSHUB_%s Expected %svolt step = %d\n",
		mt_reg.desc.name, is_sleep_vol?"sleep ":"", set_step);
	pmic_set_register_value(vosel_reg, set_step);
	udelay(220);
	get_step = pmic_get_register_value(vosel_reg);
	if (get_step != set_step) {
		pr_notice("[%s] Set SSHUB_%s Voltage fail with step = %d, read voltage = %duV\n"
			  , __func__, mt_reg.desc.name, set_step
			  , (get_step * uV_step + min_uV));
		return voltage;
	}
	pr_info("Set SSHUB_%s %sVoltage to %duV pass\n",
		mt_reg.desc.name, is_sleep_vol?"sleep ":"", voltage);
	return 0;
}

/*
 * SCP enable VCORE/VSRAM control, return 0 if success
 */
int pmic_scp_ctrl_enable(bool vcore_en, bool vsram_en, bool is_pmrc_mode)
{
	int ret = 0;

	pmic_set_register_value(PMIC_RG_BUCK_VCORE_SSHUB_EN, vcore_en);
	pmic_set_register_value(PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_EN, vsram_en);
	pmic_set_register_value(PMIC_RG_VR_SSHUB_MODE, is_pmrc_mode);
	pr_info("[%s] vcore_en:%d vsram_en:%d is_pmic_mode:%d\n",
		__func__, vcore_en, vsram_en, is_pmrc_mode);
	return ret;
}

/*
 * SCP set VCORE voltage, return 0 if success,
 * otherwise return set voltage(uV)
 */
unsigned int pmic_scp_set_vcore(unsigned int voltage)
{
	return pmic_scp_set_regulator(
		mt_bucks[MT6359_POWER_BUCK_VCORE],
		PMIC_RG_BUCK_VCORE_SSHUB_VOSEL, voltage, false);
}

/*
 * SCP set VSRAM_CORE(VSRAM_OTHERS) voltage, return 0 if success,
 * otherwise return set voltage(uV)
 */
unsigned int pmic_scp_set_vsram_vcore(unsigned int voltage)
{
	return pmic_scp_set_regulator(
		mt_ldos[MT6359_POWER_LDO_VSRAM_OTHERS],
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL, voltage, false);
}

unsigned int pmic_scp_set_vsram_vcore_sleep(unsigned int voltage)
{
	return pmic_scp_set_regulator(
		mt_ldos[MT6359_POWER_LDO_VSRAM_OTHERS],
		PMIC_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP, voltage, true);
}


/*****************************************************************************
 * PMIC charger detection
 ******************************************************************************/
unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int val = 0;

	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	PMICLOG("[%s] CHRDET status = %d\n", __func__, val);

	return val;
}

int is_ext_vbat_boost_exist(void)
{
	return 0;
}

int is_ext_swchr_exist(void)
{
	return 0;
}

/*****************************************************************************
 * Enternal BUCK status
 ******************************************************************************/

int is_ext_buck_gpio_exist(void)
{
	return pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
