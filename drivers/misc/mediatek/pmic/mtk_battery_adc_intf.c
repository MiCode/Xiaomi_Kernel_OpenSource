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
#include <linux/delay.h>
#include <linux/time.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_battery.h>

#include <mt-plat/mtk_auxadc_intf.h>
#include <mach/mtk_pmic.h>

#include <mtk_battery_internal.h>
#include <mach/mtk_charger_init.h>
#include <mtk_charger.h>
#include "include/pmic_auxadc.h"

int pmic_get_battery_voltage(void)
{
	int bat = 0;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_FPGA_EARLY_PORTING)
	bat = 4201;
#else
	if (is_isense_supported() && is_power_path_supported())
		bat = pmic_get_auxadc_value(AUXADC_LIST_ISENSE);
	else
		bat = pmic_get_auxadc_value(AUXADC_LIST_BATADC);
#endif
	return bat;
}

bool pmic_is_battery_exist(void)
{
	int temp;
	bool is_bat_exist;
	int hw_id = pmic_get_register_value(PMIC_HWCID);

	temp = pmic_get_register_value(PMIC_RGS_BATON_UNDET);

	if (temp == 0)
		is_bat_exist = true;
	else
		is_bat_exist = false;


	bm_debug(
		"[fg_is_battery_exist] PMIC_RGS_BATON_UNDET = %d\n",
		is_bat_exist);

	if (is_bat_exist == 0) {
		if (hw_id == 0x3510) {
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_WAKEUP_CLR, 1);
			mdelay(1);
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_WAKEUP_CLR, 0);
		} else {
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 1);
			mdelay(1);
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 0);
		}
	}
	return is_bat_exist;

}


int pmic_get_vbus(void)
{
	int vchr = 0;
#if defined(CONFIG_POWER_EXT) || (CONFIG_MTK_GAUGE_VERSION != 30)
	vchr = 5001;
#elif defined(CONFIG_FPGA_EARLY_PORTING)
	vchr = 5001;
#else
	vchr = pmic_get_auxadc_value(AUXADC_LIST_VCDT);
	vchr =
		(((fg_cust_data.r_charger_1 +
		fg_cust_data.r_charger_2) * 100 * vchr) /
		fg_cust_data.r_charger_2) / 100;

#endif
	return vchr;
}

int pmic_get_v_bat_temp(void)
{
	int adc;
#if defined(CONFIG_POWER_EXT)
	adc = 0;
#else
#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
	adc = pmic_get_auxadc_value(AUXADC_LIST_BATTEMP_35);
#else
	adc = pmic_get_auxadc_value(AUXADC_LIST_BATTEMP);
#endif
#endif
	return adc;
}

int pmic_get_ibus(void)
{
	return 0;
}

int pmic_get_charging_current(void)
{
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_FPGA_EARLY_PORTING)
	return 0;
#else
	int v_batsns = 0, v_isense = 0;

	if (is_isense_supported() && !is_power_path_supported()) {
		v_isense = pmic_get_auxadc_value(AUXADC_LIST_ISENSE);
		v_batsns = pmic_get_auxadc_value(AUXADC_LIST_BATADC);

		return (v_isense - v_batsns) * 1000 / R_SENSE;
	}

	return 0;
#endif
}

bool __attribute__ ((weak))
	mtk_bif_is_hw_exist(void)
{
	pr_notice_once("%s: do not have bif driver\n", __func__);
	return false;
}

int __attribute__ ((weak))
	mtk_bif_get_vbat(int *vbat)
{
	pr_notice_once("%s: do not have bif driver\n", __func__);
	return -ENOTSUPP;
}

int __attribute__ ((weak))
	mtk_bif_get_tbat(int *tmp)
{
	pr_notice_once("%s: do not have bif driver\n", __func__);
	return -ENOTSUPP;
}

int __attribute__ ((weak))
	mtk_bif_init(void)
{
	pr_notice_once("%s: do not have bif driver\n", __func__);
	return -ENOTSUPP;
}

int pmic_is_bif_exist(void)
{
	return mtk_bif_is_hw_exist();
}

int pmic_get_bif_battery_voltage(int *vbat)
{
	return mtk_bif_get_vbat(vbat);
}

int pmic_get_bif_battery_temperature(int *tmp)
{
	return mtk_bif_get_tbat(tmp);
}

int pmic_bif_init(void)
{
	return mtk_bif_init();
}

int pmic_enable_hw_vbus_ovp(bool enable)
{
	int ret = 0;

	ret = pmic_set_register_value(PMIC_RG_VCDT_HV_EN, enable);
	if (ret != 0)
		pr_notice("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}
