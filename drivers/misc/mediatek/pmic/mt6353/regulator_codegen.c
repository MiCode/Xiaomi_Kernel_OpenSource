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
#include "include/regulator_codegen.h"
#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_ldo.h"
#include "include/pmic_api_buck.h"

static const int vtcxo24_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vxo22_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vcn33_bt_voltages[] = {
	3300000,
	3400000,
	3500000,
	3600000,

};

static const int vcn33_wifi_voltages[] = {
	3300000,
	3400000,
	3500000,
	3600000,

};

static const int vsram_proc_voltages[] = {
	1100000,
};

static const int vldo28_0_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2500000,
	2800000,
	3000000,
	3300000,

};

static const int vldo28_1_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2500000,
	2800000,
	3000000,
	3300000,

};

static const int vtcxo28_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vrf18_voltages[] = {
	1800000,

};

static const int vrf12_voltages[] = {
	900000,
	950000,
	1000000,
	1050000,
	1200000,
	1500000,
	1800000,

};

static const int vcn28_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vcn18_voltages[] = {
	1800000,

};

static const int vcama_voltages[] = {
	1500000,
	1800000,
	2500000,
	2800000,

};

static const int vcamio_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,

};

static const int vcamd_voltages[] = {
	900000,
	1000000,
	1100000,
	1220000,
	1300000,
	1500000,
	1800000,

};

static const int vaux18_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vaud28_voltages[] = {
	1800000,
	2200000,
	2375000,
	2800000,

};

static const int vdram_voltages[] = {
	1240000,
	1390000,
	1540000,
	1540000,

};

static const int vsim1_voltages[] = {
	1700000,
	1700000,
	1800000,
	1860000,
	2760000,
	2760000,
	3000000,
	3100000,

};

static const int vsim2_voltages[] = {
	1700000,
	1700000,
	1800000,
	1860000,
	2760000,
	2760000,
	3000000,
	3100000,

};

static const int vio28_voltages[] = {
	2800000,

};

static const int vmc_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2500000,
	2900000,
	3000000,
	3300000,

};

static const int vmch_voltages[] = {
	2900000,
	3000000,
	3300000,

};

static const int vusb33_voltages[] = {
	3300000,

};

static const int vemc33_voltages[] = {
	2900000,
	3000000,
	3300000,

};

static const int vio18_voltages[] = {
	1800000,

};

static const int vibr_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2500000,
	2800000,
	3000000,
	3300000,

};

/* Regulator vtcxo24 enable */
static int pmic_ldo_vtcxo24_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo24 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vtcxo24_sw_en(1);
	return 0;
}

/* Regulator vtcxo24 disable */
static int pmic_ldo_vtcxo24_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vtcxo24 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vtcxo24_sw_en(0);
	return ret;
}

/* Regulator vtcxo24 is_enabled */
static int pmic_ldo_vtcxo24_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo24 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vtcxo24_en();
}

/* Regulator vtcxo24 set_voltage_sel */
static int pmic_ldo_vtcxo24_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vtcxo24 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vtcxo24_vosel(selector);
	return 0;
}

/* Regulator vtcxo24 get_voltage_sel */
static int pmic_ldo_vtcxo24_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo24 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vtcxo24_vosel();
}

/* Regulator vtcxo24 list_voltage */
static int pmic_ldo_vtcxo24_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vtcxo24_voltages[selector];
	PMICLOG("ldo vtcxo24 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vxo22 enable */
static int pmic_ldo_vxo22_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vxo22 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vxo22_sw_en(1);
	return 0;
}

/* Regulator vxo22 disable */
static int pmic_ldo_vxo22_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vxo22 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vxo22_sw_en(0);
	return ret;
}

/* Regulator vxo22 is_enabled */
static int pmic_ldo_vxo22_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vxo22 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vxo22_en();
}

/* Regulator vxo22 set_voltage_sel */
static int pmic_ldo_vxo22_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vxo22 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vxo22_vosel(selector);
	return 0;
}

/* Regulator vxo22 get_voltage_sel */
static int pmic_ldo_vxo22_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vxo22 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vxo22_vosel();
}

/* Regulator vxo22 list_voltage */
static int pmic_ldo_vxo22_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vxo22_voltages[selector];
	PMICLOG("ldo vxo22 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn33_bt enable */
static int pmic_ldo_vcn33_bt_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_bt enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcn33_bt_sw_en(1);
	return 0;
}

/* Regulator vcn33_bt disable */
static int pmic_ldo_vcn33_bt_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcn33_bt disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcn33_bt_sw_en(0);
	return ret;
}

/* Regulator vcn33_bt is_enabled */
static int pmic_ldo_vcn33_bt_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_bt is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	/*return mt6353_upmu_get_da_qi_vcn33_bt_en();*/
	return mt6353_upmu_get_ldo_vcn33_en_bt();
}

/* Regulator vcn33_bt set_voltage_sel */
static int pmic_ldo_vcn33_bt_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcn33_bt set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcn33_vosel(selector);
	return 0;
}

/* Regulator vcn33_bt get_voltage_sel */
static int pmic_ldo_vcn33_bt_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_bt get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcn33_vosel();
}

/* Regulator vcn33_bt list_voltage */
static int pmic_ldo_vcn33_bt_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcn33_bt_voltages[selector];
	PMICLOG("ldo vcn33_bt list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn33_wifi enable */
static int pmic_ldo_vcn33_wifi_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_wifi enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcn33_wifi_sw_en(1);
	return 0;
}

/* Regulator vcn33_wifi disable */
static int pmic_ldo_vcn33_wifi_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcn33_wifi disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcn33_wifi_sw_en(0);
	return ret;
}

/* Regulator vcn33_wifi is_enabled */
static int pmic_ldo_vcn33_wifi_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_wifi is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	/*return mt6353_upmu_get_da_qi_vcn33_wifi_en();*/
	return mt6353_upmu_get_ldo_vcn33_en_wifi();
}

/* Regulator vcn33_wifi set_voltage_sel */
static int pmic_ldo_vcn33_wifi_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcn33_wifi set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcn33_vosel(selector);
	return 0;
}

/* Regulator vcn33_wifi get_voltage_sel */
static int pmic_ldo_vcn33_wifi_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn33_wifi get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcn33_vosel();
}

/* Regulator vcn33_wifi list_voltage */
static int pmic_ldo_vcn33_wifi_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcn33_wifi_voltages[selector];
	PMICLOG("ldo vcn33_wifi list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsram_proc enable */
static int pmic_ldo_vsram_proc_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsram_proc enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vsram_proc_sw_en(1);
	return 0;
}

/* Regulator vsram_proc disable */
static int pmic_ldo_vsram_proc_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vsram_proc disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vsram_proc_sw_en(0);
	return ret;
}

/* Regulator vsram_proc is_enabled */
static int pmic_ldo_vsram_proc_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsram_proc is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vsram_proc_en();
}

/* Regulator vsram_proc set_voltage_sel */
/* mt6353 only */
static int pmic_ldo_vsram_proc_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vsram_proc set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_ldo_vsram_proc_vosel(selector);
	return 0;
}

/* Regulator vsram_proc get_voltage_sel */
/* mt6353 only */
static int pmic_ldo_vsram_proc_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsram_proc get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_ldo_vsram_proc_vosel();
}

/* Regulator vsram_proc list_voltage */
static int pmic_ldo_vsram_proc_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	/*---special case---*/
	voltage = rdev->desc->min_uV + rdev->desc->uV_step * selector;
	PMICLOG("ldo vsram_proc list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vldo28_0 enable */
static int pmic_ldo_vldo28_0_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_0 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vldo28_0_sw_en(1);
	return 0;
}

/* Regulator vldo28_0 disable */
static int pmic_ldo_vldo28_0_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vldo28_0 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vldo28_0_sw_en(0);
	return ret;
}

/* Regulator vldo28_0 is_enabled */
static int pmic_ldo_vldo28_0_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_0 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	/*return mt6353_upmu_get_da_qi_vldo28_0_en();*/
	return mt6353_upmu_get_ldo_vldo28_en_0();
}

/* Regulator vldo28_0 set_voltage_sel */
static int pmic_ldo_vldo28_0_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vldo28_0 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vldo28_vosel(selector);
	return 0;
}

/* Regulator vldo28_0 get_voltage_sel */
static int pmic_ldo_vldo28_0_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_0 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vldo28_vosel();
}

/* Regulator vldo28_0 list_voltage */
static int pmic_ldo_vldo28_0_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vldo28_0_voltages[selector];
	PMICLOG("ldo vldo28_0 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vldo28_1 enable */
static int pmic_ldo_vldo28_1_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_1 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vldo28_1_sw_en(1);
	return 0;
}

/* Regulator vldo28_1 disable */
static int pmic_ldo_vldo28_1_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vldo28_1 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vldo28_1_sw_en(0);
	return ret;
}

/* Regulator vldo28_1 is_enabled */
static int pmic_ldo_vldo28_1_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_1 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	/*return mt6353_upmu_get_da_qi_vldo28_1_en();*/
	return mt6353_upmu_get_ldo_vldo28_en_1();
}

/* Regulator vldo28_1 set_voltage_sel */
static int pmic_ldo_vldo28_1_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vldo28_1 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vldo28_vosel(selector);
	return 0;
}

/* Regulator vldo28_1 get_voltage_sel */
static int pmic_ldo_vldo28_1_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vldo28_1 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vldo28_vosel();
}

/* Regulator vldo28_1 list_voltage */
static int pmic_ldo_vldo28_1_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vldo28_1_voltages[selector];
	PMICLOG("ldo vldo28_1 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vtcxo28 enable */
static int pmic_ldo_vtcxo28_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo28 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vtcxo28_sw_en(1);
	return 0;
}

/* Regulator vtcxo28 disable */
static int pmic_ldo_vtcxo28_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vtcxo28 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vtcxo28_sw_en(0);
	return ret;
}

/* Regulator vtcxo28 is_enabled */
static int pmic_ldo_vtcxo28_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo28 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vtcxo28_en();
}

/* Regulator vtcxo28 set_voltage_sel */
static int pmic_ldo_vtcxo28_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vtcxo28 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vtcxo28_vosel(selector);
	return 0;
}

/* Regulator vtcxo28 get_voltage_sel */
static int pmic_ldo_vtcxo28_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vtcxo28 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vtcxo28_vosel();
}

/* Regulator vtcxo28 list_voltage */
static int pmic_ldo_vtcxo28_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vtcxo28_voltages[selector];
	PMICLOG("ldo vtcxo28 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vrf18 enable */
static int pmic_ldo_vrf18_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf18 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vrf18_sw_en(1);
	return 0;
}

/* Regulator vrf18 disable */
static int pmic_ldo_vrf18_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vrf18 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vrf18_sw_en(0);
	return ret;
}

/* Regulator vrf18 is_enabled */
static int pmic_ldo_vrf18_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf18 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vrf18_en();
}

/* Regulator vrf18 set_voltage_sel */
static int pmic_ldo_vrf18_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vrf18 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	/*mt6353_upmu_set_rg_vrf18_vosel(selector);*/
	return 0;
}

/* Regulator vrf18 get_voltage_sel */
static int pmic_ldo_vrf18_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf18 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	/*return mt6353_upmu_get_rg_vrf18_vosel();*/
	return 0;
}

/* Regulator vrf18 list_voltage */
static int pmic_ldo_vrf18_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vrf18_voltages[selector];
	PMICLOG("ldo vrf18 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vrf12 enable */
static int pmic_ldo_vrf12_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf12 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vrf12_sw_en(1);
	return 0;
}

/* Regulator vrf12 disable */
static int pmic_ldo_vrf12_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vrf12 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vrf12_sw_en(0);
	return ret;
}

/* Regulator vrf12 is_enabled */
static int pmic_ldo_vrf12_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf12 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vrf12_en();
}

/* Regulator vrf12 set_voltage_sel */
static int pmic_ldo_vrf12_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vrf12 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vrf12_vosel(selector);
	return 0;
}

/* Regulator vrf12 get_voltage_sel */
static int pmic_ldo_vrf12_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vrf12 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vrf12_vosel();
}

/* Regulator vrf12 list_voltage */
static int pmic_ldo_vrf12_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vrf12_voltages[selector];
	PMICLOG("ldo vrf12 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn28 enable */
static int pmic_ldo_vcn28_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn28 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcn28_sw_en(1);
	return 0;
}

/* Regulator vcn28 disable */
static int pmic_ldo_vcn28_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcn28 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcn28_sw_en(0);
	return ret;
}

/* Regulator vcn28 is_enabled */
static int pmic_ldo_vcn28_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn28 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vcn28_en();
}

/* Regulator vcn28 set_voltage_sel */
static int pmic_ldo_vcn28_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcn28 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcn28_vosel(selector);
	return 0;
}

/* Regulator vcn28 get_voltage_sel */
static int pmic_ldo_vcn28_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn28 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcn28_vosel();
}

/* Regulator vcn28 list_voltage */
static int pmic_ldo_vcn28_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcn28_voltages[selector];
	PMICLOG("ldo vcn28 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn18 enable */
static int pmic_ldo_vcn18_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn18 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcn18_sw_en(1);
	return 0;
}

/* Regulator vcn18 disable */
static int pmic_ldo_vcn18_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcn18 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcn18_sw_en(0);
	return ret;
}

/* Regulator vcn18 is_enabled */
static int pmic_ldo_vcn18_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn18 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vcn18_en();
}

/* Regulator vcn18 set_voltage_sel */
static int pmic_ldo_vcn18_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcn18 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	/*mt6353_upmu_set_rg_vcn18_vosel(selector);*/
	return 0;
}

/* Regulator vcn18 get_voltage_sel */
static int pmic_ldo_vcn18_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcn18 get_voltage_sel\n");
	/*return mtk_regulator_get_voltage_sel(rdev);*/
	/*return mt6353_upmu_get_rg_vcn18_vosel();*/
	return 0;
}

/* Regulator vcn18 list_voltage */
static int pmic_ldo_vcn18_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcn18_voltages[selector];
	PMICLOG("ldo vcn18 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcama enable */
static int pmic_ldo_vcama_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcama enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcama_sw_en(1);
	return 0;
}

/* Regulator vcama disable */
static int pmic_ldo_vcama_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcama disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcama_sw_en(0);
	return ret;
}

/* Regulator vcama is_enabled */
static int pmic_ldo_vcama_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcama is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vcama_en();
}

/* Regulator vcama set_voltage_sel */
static int pmic_ldo_vcama_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcama set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcama_vosel(selector);
	return 0;
}

/* Regulator vcama get_voltage_sel */
static int pmic_ldo_vcama_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcama get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcama_vosel();
}

/* Regulator vcama list_voltage */
static int pmic_ldo_vcama_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcama_voltages[selector];
	PMICLOG("ldo vcama list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcamio enable */
static int pmic_ldo_vcamio_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamio enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcamio_sw_en(1);
	return 0;
}

/* Regulator vcamio disable */
static int pmic_ldo_vcamio_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcamio disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcamio_sw_en(0);
	return ret;
}

/* Regulator vcamio is_enabled */
static int pmic_ldo_vcamio_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamio is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vcamio_en();
}

/* Regulator vcamio set_voltage_sel */
static int pmic_ldo_vcamio_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcamio set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcamio_vosel(selector);
	return 0;
}

/* Regulator vcamio get_voltage_sel */
static int pmic_ldo_vcamio_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamio get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcamio_vosel();
}

/* Regulator vcamio list_voltage */
static int pmic_ldo_vcamio_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcamio_voltages[selector];
	PMICLOG("ldo vcamio list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcamd enable */
static int pmic_ldo_vcamd_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamd enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vcamd_sw_en(1);
	return 0;
}

/* Regulator vcamd disable */
static int pmic_ldo_vcamd_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vcamd disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vcamd_sw_en(0);
	return ret;
}

/* Regulator vcamd is_enabled */
static int pmic_ldo_vcamd_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamd is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vcamd_en();
}

/* Regulator vcamd set_voltage_sel */
static int pmic_ldo_vcamd_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vcamd set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vcamd_vosel(selector);
	return 0;
}

/* Regulator vcamd get_voltage_sel */
static int pmic_ldo_vcamd_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vcamd get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vcamd_vosel();
}

/* Regulator vcamd list_voltage */
static int pmic_ldo_vcamd_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vcamd_voltages[selector];
	PMICLOG("ldo vcamd list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vaux18 enable */
static int pmic_ldo_vaux18_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaux18 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vaux18_sw_en(1);
	return 0;
}

/* Regulator vaux18 disable */
static int pmic_ldo_vaux18_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vaux18 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vaux18_sw_en(0);
	return ret;
}

/* Regulator vaux18 is_enabled */
static int pmic_ldo_vaux18_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaux18 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vaux18_en();
}

/* Regulator vaux18 set_voltage_sel */
static int pmic_ldo_vaux18_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vaux18 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vaux18_vosel(selector);
	return 0;
}

/* Regulator vaux18 get_voltage_sel */
static int pmic_ldo_vaux18_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaux18 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vaux18_vosel();
}

/* Regulator vaux18 list_voltage */
static int pmic_ldo_vaux18_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vaux18_voltages[selector];
	PMICLOG("ldo vaux18 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vaud28 enable */
static int pmic_ldo_vaud28_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaud28 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vaud28_sw_en(1);
	return 0;
}

/* Regulator vaud28 disable */
static int pmic_ldo_vaud28_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vaud28 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vaud28_sw_en(0);
	return ret;
}

/* Regulator vaud28 is_enabled */
static int pmic_ldo_vaud28_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaud28 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vaud28_en();
}

/* Regulator vaud28 set_voltage_sel */
static int pmic_ldo_vaud28_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vaud28 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vaud28_vosel(selector);
	return 0;
}

/* Regulator vaud28 get_voltage_sel */
static int pmic_ldo_vaud28_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vaud28 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vaud28_vosel();
}

/* Regulator vaud28 list_voltage */
static int pmic_ldo_vaud28_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vaud28_voltages[selector];
	PMICLOG("ldo vaud28 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vdram enable */
static int pmic_ldo_vdram_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vdram enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vdram_sw_en(1);
	return 0;
}

/* Regulator vdram disable */
static int pmic_ldo_vdram_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vdram disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vdram_sw_en(0);
	return ret;
}

/* Regulator vdram is_enabled */
static int pmic_ldo_vdram_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vdram is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vdram_en();
}

/* Regulator vdram set_voltage_sel */
static int pmic_ldo_vdram_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vdram set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vdram_vosel(selector);
	return 0;
}

/* Regulator vdram get_voltage_sel */
static int pmic_ldo_vdram_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vdram get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vdram_vosel();
}

/* Regulator vdram list_voltage */
static int pmic_ldo_vdram_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vdram_voltages[selector];
	PMICLOG("ldo vdram list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsim1 enable */
static int pmic_ldo_vsim1_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim1 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vsim1_sw_en(1);
	return 0;
}

/* Regulator vsim1 disable */
static int pmic_ldo_vsim1_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vsim1 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vsim1_sw_en(0);
	return ret;
}

/* Regulator vsim1 is_enabled */
static int pmic_ldo_vsim1_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim1 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vsim1_en();
}

/* Regulator vsim1 set_voltage_sel */
static int pmic_ldo_vsim1_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vsim1 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vsim1_vosel(selector);
	return 0;
}

/* Regulator vsim1 get_voltage_sel */
static int pmic_ldo_vsim1_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim1 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vsim1_vosel();
}

/* Regulator vsim1 list_voltage */
static int pmic_ldo_vsim1_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vsim1_voltages[selector];
	PMICLOG("ldo vsim1 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsim2 enable */
static int pmic_ldo_vsim2_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim2 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vsim2_sw_en(1);
	return 0;
}

/* Regulator vsim2 disable */
static int pmic_ldo_vsim2_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vsim2 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vsim2_sw_en(0);
	return ret;
}

/* Regulator vsim2 is_enabled */
static int pmic_ldo_vsim2_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim2 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vsim2_en();
}

/* Regulator vsim2 set_voltage_sel */
static int pmic_ldo_vsim2_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vsim2 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vsim2_vosel(selector);
	return 0;
}

/* Regulator vsim2 get_voltage_sel */
static int pmic_ldo_vsim2_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vsim2 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vsim2_vosel();
}

/* Regulator vsim2 list_voltage */
static int pmic_ldo_vsim2_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vsim2_voltages[selector];
	PMICLOG("ldo vsim2 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vio28 enable */
static int pmic_ldo_vio28_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio28 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vio28_sw_en(1);
	return 0;
}

/* Regulator vio28 disable */
static int pmic_ldo_vio28_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vio28 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vio28_sw_en(0);
	return ret;
}

/* Regulator vio28 is_enabled */
static int pmic_ldo_vio28_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio28 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vio28_en();
}

/* Regulator vio28 set_voltage_sel */
static int pmic_ldo_vio28_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vio28 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	/*mt6353_upmu_set_rg_vio28_vosel(selector);*/
	return 0;
}

/* Regulator vio28 get_voltage_sel */
static int pmic_ldo_vio28_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio28 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	/*return mt6353_upmu_get_rg_vio28_vosel();*/
	return 0;
}

/* Regulator vio28 list_voltage */
static int pmic_ldo_vio28_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vio28_voltages[selector];
	PMICLOG("ldo vio28 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vmc enable */
static int pmic_ldo_vmc_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmc enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vmc_sw_en(1);
	return 0;
}

/* Regulator vmc disable */
static int pmic_ldo_vmc_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vmc disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vmc_sw_en(0);
	return ret;
}

/* Regulator vmc is_enabled */
static int pmic_ldo_vmc_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmc is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vmc_en();
}

/* Regulator vmc set_voltage_sel */
static int pmic_ldo_vmc_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vmc set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vmc_vosel(selector);
	return 0;
}

/* Regulator vmc get_voltage_sel */
static int pmic_ldo_vmc_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmc get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vmc_vosel();
}

/* Regulator vmc list_voltage */
static int pmic_ldo_vmc_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vmc_voltages[selector];
	PMICLOG("ldo vmc list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vmch enable */
static int pmic_ldo_vmch_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmch enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vmch_sw_en(1);
	return 0;
}

/* Regulator vmch disable */
static int pmic_ldo_vmch_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vmch disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vmch_sw_en(0);
	return ret;
}

/* Regulator vmch is_enabled */
static int pmic_ldo_vmch_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmch is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vmch_en();
}

/* Regulator vmch set_voltage_sel */
static int pmic_ldo_vmch_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vmch set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vmch_vosel(selector);
	return 0;
}

/* Regulator vmch get_voltage_sel */
static int pmic_ldo_vmch_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vmch get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vmch_vosel();
}

/* Regulator vmch list_voltage */
static int pmic_ldo_vmch_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vmch_voltages[selector];
	PMICLOG("ldo vmch list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vusb33 enable */
static int pmic_ldo_vusb33_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vusb33 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vusb33_sw_en(1);
	return 0;
}

/* Regulator vusb33 disable */
static int pmic_ldo_vusb33_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vusb33 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vusb33_sw_en(0);
	return ret;
}

/* Regulator vusb33 is_enabled */
static int pmic_ldo_vusb33_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vusb33 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vusb33_en();
}

/* Regulator vusb33 set_voltage_sel */
static int pmic_ldo_vusb33_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vusb33 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	/*mt6353_upmu_set_rg_vusb33_vosel(selector);*/
	return 0;
}

/* Regulator vusb33 get_voltage_sel */
static int pmic_ldo_vusb33_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vusb33 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	/*return mt6353_upmu_get_rg_vusb33_vosel();*/
	return 0;
}

/* Regulator vusb33 list_voltage */
static int pmic_ldo_vusb33_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vusb33_voltages[selector];
	PMICLOG("ldo vusb33 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vemc33 enable */
static int pmic_ldo_vemc33_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vemc33 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vemc33_sw_en(1);
	return 0;
}

/* Regulator vemc33 disable */
static int pmic_ldo_vemc33_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vemc33 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vemc33_sw_en(0);
	return ret;
}

/* Regulator vemc33 is_enabled */
static int pmic_ldo_vemc33_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vemc33 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vemc33_en();
}

/* Regulator vemc33 set_voltage_sel */
static int pmic_ldo_vemc33_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vemc33 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vemc33_vosel(selector);
	return 0;
}

/* Regulator vemc33 get_voltage_sel */
static int pmic_ldo_vemc33_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vemc33 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vemc33_vosel();
}

/* Regulator vemc33 list_voltage */
static int pmic_ldo_vemc33_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vemc33_voltages[selector];
	PMICLOG("ldo vemc33 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vio18 enable */
static int pmic_ldo_vio18_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio18 enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vio18_sw_en(1);
	return 0;
}

/* Regulator vio18 disable */
static int pmic_ldo_vio18_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vio18 disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vio18_sw_en(0);
	return ret;
}

/* Regulator vio18 is_enabled */
static int pmic_ldo_vio18_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio18 is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vio18_en();
}

/* Regulator vio18 set_voltage_sel */
static int pmic_ldo_vio18_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vio18 set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	/*mt6353_upmu_set_rg_vio18_vosel(selector);*/
	return 0;
}

/* Regulator vio18 get_voltage_sel */
static int pmic_ldo_vio18_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vio18 get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	/*return mt6353_upmu_get_rg_vio18_vosel();*/
	return 0;
}

/* Regulator vio18 list_voltage */
static int pmic_ldo_vio18_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vio18_voltages[selector];
	PMICLOG("ldo vio18 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vibr enable */
static int pmic_ldo_vibr_enable(struct regulator_dev *rdev)
{
	PMICLOG("ldo vibr enable\n");
	/*return mtk_regulator_enable(rdev);*/
	pmic_ldo_vibr_sw_en(1);
	return 0;
}

/* Regulator vibr disable */
static int pmic_ldo_vibr_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	PMICLOG("ldo vibr disable\n");
	/*return mtk_regulator_disable(rdev);*/
	if (rdev->use_count == 0) {
		pr_err("regulator name=%s should not disable( use_count=%d)\n", rdev->desc->name,
			rdev->use_count);
		ret = -1;
	} else
		pmic_ldo_vibr_sw_en(0);
	return ret;
}

/* Regulator vibr is_enabled */
static int pmic_ldo_vibr_is_enabled(struct regulator_dev *rdev)
{
	PMICLOG("ldo vibr is_enabled\n");
	/*return mtk_regulator_is_enabled(rdev);*/
	return mt6353_upmu_get_da_qi_vibr_en();
}

/* Regulator vibr set_voltage_sel */
static int pmic_ldo_vibr_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	mreg->vosel.cur_sel = selector;
	PMICLOG("ldo vibr set_voltage_sel: %d\n", selector);
	/*return mtk_regulator_set_voltage_sel(rdev, selector);*/
	mt6353_upmu_set_rg_vibr_vosel(selector);
	return 0;
}

/* Regulator vibr get_voltage_sel */
static int pmic_ldo_vibr_get_voltage_sel(struct regulator_dev *rdev)
{
	PMICLOG("ldo vibr get_voltage_sel\n");

	/*return mtk_regulator_get_voltage_sel(rdev);*/
	return mt6353_upmu_get_rg_vibr_vosel();
}

/* Regulator vibr list_voltage */
static int pmic_ldo_vibr_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int voltage;

	voltage = vibr_voltages[selector];
	PMICLOG("ldo vibr list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vtcxo24 ops */
static struct regulator_ops pmic_ldo_vtcxo24_ops = {
	.enable = pmic_ldo_vtcxo24_enable,
	.disable = pmic_ldo_vtcxo24_disable,
	.is_enabled = pmic_ldo_vtcxo24_is_enabled,
	.get_voltage_sel = pmic_ldo_vtcxo24_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vtcxo24_set_voltage_sel,
	.list_voltage = pmic_ldo_vtcxo24_list_voltage,
	/* .enable_time = pmic_ldo_vtcxo24_enable_time, */
};

/* Regulator vxo22 ops */
static struct regulator_ops pmic_ldo_vxo22_ops = {
	.enable = pmic_ldo_vxo22_enable,
	.disable = pmic_ldo_vxo22_disable,
	.is_enabled = pmic_ldo_vxo22_is_enabled,
	.get_voltage_sel = pmic_ldo_vxo22_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vxo22_set_voltage_sel,
	.list_voltage = pmic_ldo_vxo22_list_voltage,
	/* .enable_time = pmic_ldo_vxo22_enable_time, */
};

/* Regulator vcn33_bt ops */
static struct regulator_ops pmic_ldo_vcn33_bt_ops = {
	.enable = pmic_ldo_vcn33_bt_enable,
	.disable = pmic_ldo_vcn33_bt_disable,
	.is_enabled = pmic_ldo_vcn33_bt_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_bt_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_bt_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_bt_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_bt_enable_time, */
};

/* Regulator vcn33_wifi ops */
static struct regulator_ops pmic_ldo_vcn33_wifi_ops = {
	.enable = pmic_ldo_vcn33_wifi_enable,
	.disable = pmic_ldo_vcn33_wifi_disable,
	.is_enabled = pmic_ldo_vcn33_wifi_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_wifi_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_wifi_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_wifi_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_wifi_enable_time, */
};

/* Regulator vsram_proc ops */
static struct regulator_ops pmic_ldo_vsram_proc_ops = {
	.enable = pmic_ldo_vsram_proc_enable,
	.disable = pmic_ldo_vsram_proc_disable,
	.is_enabled = pmic_ldo_vsram_proc_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc_set_voltage_sel,
	.list_voltage = pmic_ldo_vsram_proc_list_voltage,
	/* .enable_time = pmic_ldo_vsram_proc_enable_time, */
};

/* Regulator vldo28_0 ops */
static struct regulator_ops pmic_ldo_vldo28_ops = {
	.enable = pmic_ldo_vldo28_0_enable,
	.disable = pmic_ldo_vldo28_0_disable,
	.is_enabled = pmic_ldo_vldo28_0_is_enabled,
	.get_voltage_sel = pmic_ldo_vldo28_0_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vldo28_0_set_voltage_sel,
	.list_voltage = pmic_ldo_vldo28_0_list_voltage,
	/* .enable_time = pmic_ldo_vldo28_0_enable_time, */
};

/* Regulator vldo28_1 ops */
static struct regulator_ops pmic_ldo_vldo28_1_ops = {
	.enable = pmic_ldo_vldo28_1_enable,
	.disable = pmic_ldo_vldo28_1_disable,
	.is_enabled = pmic_ldo_vldo28_1_is_enabled,
	.get_voltage_sel = pmic_ldo_vldo28_1_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vldo28_1_set_voltage_sel,
	.list_voltage = pmic_ldo_vldo28_1_list_voltage,
	/* .enable_time = pmic_ldo_vldo28_1_enable_time, */
};

/* Regulator vtcxo28 ops */
static struct regulator_ops pmic_ldo_vtcxo28_ops = {
	.enable = pmic_ldo_vtcxo28_enable,
	.disable = pmic_ldo_vtcxo28_disable,
	.is_enabled = pmic_ldo_vtcxo28_is_enabled,
	.get_voltage_sel = pmic_ldo_vtcxo28_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vtcxo28_set_voltage_sel,
	.list_voltage = pmic_ldo_vtcxo28_list_voltage,
	/* .enable_time = pmic_ldo_vtcxo28_enable_time, */
};

/* Regulator vrf18 ops */
static struct regulator_ops pmic_ldo_vrf18_ops = {
	.enable = pmic_ldo_vrf18_enable,
	.disable = pmic_ldo_vrf18_disable,
	.is_enabled = pmic_ldo_vrf18_is_enabled,
	.get_voltage_sel = pmic_ldo_vrf18_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vrf18_set_voltage_sel,
	.list_voltage = pmic_ldo_vrf18_list_voltage,
	/* .enable_time = pmic_ldo_vrf18_enable_time, */
};

/* Regulator vrf12 ops */
static struct regulator_ops pmic_ldo_vrf12_ops = {
	.enable = pmic_ldo_vrf12_enable,
	.disable = pmic_ldo_vrf12_disable,
	.is_enabled = pmic_ldo_vrf12_is_enabled,
	.get_voltage_sel = pmic_ldo_vrf12_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vrf12_set_voltage_sel,
	.list_voltage = pmic_ldo_vrf12_list_voltage,
	/* .enable_time = pmic_ldo_vrf12_enable_time, */
};

/* Regulator vcn28 ops */
static struct regulator_ops pmic_ldo_vcn28_ops = {
	.enable = pmic_ldo_vcn28_enable,
	.disable = pmic_ldo_vcn28_disable,
	.is_enabled = pmic_ldo_vcn28_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn28_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn28_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn28_list_voltage,
	/* .enable_time = pmic_ldo_vcn28_enable_time, */
};

/* Regulator vcn18 ops */
static struct regulator_ops pmic_ldo_vcn18_ops = {
	.enable = pmic_ldo_vcn18_enable,
	.disable = pmic_ldo_vcn18_disable,
	.is_enabled = pmic_ldo_vcn18_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn18_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn18_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn18_list_voltage,
	/* .enable_time = pmic_ldo_vcn18_enable_time, */
};

/* Regulator vcama ops */
static struct regulator_ops pmic_ldo_vcama_ops = {
	.enable = pmic_ldo_vcama_enable,
	.disable = pmic_ldo_vcama_disable,
	.is_enabled = pmic_ldo_vcama_is_enabled,
	.get_voltage_sel = pmic_ldo_vcama_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcama_set_voltage_sel,
	.list_voltage = pmic_ldo_vcama_list_voltage,
	/* .enable_time = pmic_ldo_vcama_enable_time, */
};

/* Regulator vcamio ops */
static struct regulator_ops pmic_ldo_vcamio_ops = {
	.enable = pmic_ldo_vcamio_enable,
	.disable = pmic_ldo_vcamio_disable,
	.is_enabled = pmic_ldo_vcamio_is_enabled,
	.get_voltage_sel = pmic_ldo_vcamio_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcamio_set_voltage_sel,
	.list_voltage = pmic_ldo_vcamio_list_voltage,
	/* .enable_time = pmic_ldo_vcamio_enable_time, */
};

/* Regulator vcamd ops */
static struct regulator_ops pmic_ldo_vcamd_ops = {
	.enable = pmic_ldo_vcamd_enable,
	.disable = pmic_ldo_vcamd_disable,
	.is_enabled = pmic_ldo_vcamd_is_enabled,
	.get_voltage_sel = pmic_ldo_vcamd_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcamd_set_voltage_sel,
	.list_voltage = pmic_ldo_vcamd_list_voltage,
	/* .enable_time = pmic_ldo_vcamd_enable_time, */
};

/* Regulator vaux18 ops */
static struct regulator_ops pmic_ldo_vaux18_ops = {
	.enable = pmic_ldo_vaux18_enable,
	.disable = pmic_ldo_vaux18_disable,
	.is_enabled = pmic_ldo_vaux18_is_enabled,
	.get_voltage_sel = pmic_ldo_vaux18_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vaux18_set_voltage_sel,
	.list_voltage = pmic_ldo_vaux18_list_voltage,
	/* .enable_time = pmic_ldo_vaux18_enable_time, */
};

/* Regulator vaud28 ops */
static struct regulator_ops pmic_ldo_vaud28_ops = {
	.enable = pmic_ldo_vaud28_enable,
	.disable = pmic_ldo_vaud28_disable,
	.is_enabled = pmic_ldo_vaud28_is_enabled,
	.get_voltage_sel = pmic_ldo_vaud28_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vaud28_set_voltage_sel,
	.list_voltage = pmic_ldo_vaud28_list_voltage,
	/* .enable_time = pmic_ldo_vaud28_enable_time, */
};

/* Regulator vdram ops */
static struct regulator_ops pmic_ldo_vdram_ops = {
	.enable = pmic_ldo_vdram_enable,
	.disable = pmic_ldo_vdram_disable,
	.is_enabled = pmic_ldo_vdram_is_enabled,
	.get_voltage_sel = pmic_ldo_vdram_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vdram_set_voltage_sel,
	.list_voltage = pmic_ldo_vdram_list_voltage,
	/* .enable_time = pmic_ldo_vdram_enable_time, */
};

/* Regulator vsim1 ops */
static struct regulator_ops pmic_ldo_vsim1_ops = {
	.enable = pmic_ldo_vsim1_enable,
	.disable = pmic_ldo_vsim1_disable,
	.is_enabled = pmic_ldo_vsim1_is_enabled,
	.get_voltage_sel = pmic_ldo_vsim1_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsim1_set_voltage_sel,
	.list_voltage = pmic_ldo_vsim1_list_voltage,
	/* .enable_time = pmic_ldo_vsim1_enable_time, */
};

/* Regulator vsim2 ops */
static struct regulator_ops pmic_ldo_vsim2_ops = {
	.enable = pmic_ldo_vsim2_enable,
	.disable = pmic_ldo_vsim2_disable,
	.is_enabled = pmic_ldo_vsim2_is_enabled,
	.get_voltage_sel = pmic_ldo_vsim2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsim2_set_voltage_sel,
	.list_voltage = pmic_ldo_vsim2_list_voltage,
	/* .enable_time = pmic_ldo_vsim2_enable_time, */
};

/* Regulator vio28 ops */
static struct regulator_ops pmic_ldo_vio28_ops = {
	.enable = pmic_ldo_vio28_enable,
	.disable = pmic_ldo_vio28_disable,
	.is_enabled = pmic_ldo_vio28_is_enabled,
	.get_voltage_sel = pmic_ldo_vio28_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vio28_set_voltage_sel,
	.list_voltage = pmic_ldo_vio28_list_voltage,
	/* .enable_time = pmic_ldo_vio28_enable_time, */
};

/* Regulator vmc ops */
static struct regulator_ops pmic_ldo_vmc_ops = {
	.enable = pmic_ldo_vmc_enable,
	.disable = pmic_ldo_vmc_disable,
	.is_enabled = pmic_ldo_vmc_is_enabled,
	.get_voltage_sel = pmic_ldo_vmc_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vmc_set_voltage_sel,
	.list_voltage = pmic_ldo_vmc_list_voltage,
	/* .enable_time = pmic_ldo_vmc_enable_time, */
};

/* Regulator vmch ops */
static struct regulator_ops pmic_ldo_vmch_ops = {
	.enable = pmic_ldo_vmch_enable,
	.disable = pmic_ldo_vmch_disable,
	.is_enabled = pmic_ldo_vmch_is_enabled,
	.get_voltage_sel = pmic_ldo_vmch_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vmch_set_voltage_sel,
	.list_voltage = pmic_ldo_vmch_list_voltage,
	/* .enable_time = pmic_ldo_vmch_enable_time, */
};

/* Regulator vusb33 ops */
static struct regulator_ops pmic_ldo_vusb33_ops = {
	.enable = pmic_ldo_vusb33_enable,
	.disable = pmic_ldo_vusb33_disable,
	.is_enabled = pmic_ldo_vusb33_is_enabled,
	.get_voltage_sel = pmic_ldo_vusb33_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vusb33_set_voltage_sel,
	.list_voltage = pmic_ldo_vusb33_list_voltage,
	/* .enable_time = pmic_ldo_vusb33_enable_time, */
};

/* Regulator vemc33 ops */
static struct regulator_ops pmic_ldo_vemc_ops = {
	.enable = pmic_ldo_vemc33_enable,
	.disable = pmic_ldo_vemc33_disable,
	.is_enabled = pmic_ldo_vemc33_is_enabled,
	.get_voltage_sel = pmic_ldo_vemc33_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vemc33_set_voltage_sel,
	.list_voltage = pmic_ldo_vemc33_list_voltage,
	/* .enable_time = pmic_ldo_vemc33_enable_time, */
};

/* Regulator vio18 ops */
static struct regulator_ops pmic_ldo_vio18_ops = {
	.enable = pmic_ldo_vio18_enable,
	.disable = pmic_ldo_vio18_disable,
	.is_enabled = pmic_ldo_vio18_is_enabled,
	.get_voltage_sel = pmic_ldo_vio18_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vio18_set_voltage_sel,
	.list_voltage = pmic_ldo_vio18_list_voltage,
	/* .enable_time = pmic_ldo_vio18_enable_time, */
};

/* Regulator vibr ops */
static struct regulator_ops pmic_ldo_vibr_ops = {
	.enable = pmic_ldo_vibr_enable,
	.disable = pmic_ldo_vibr_disable,
	.is_enabled = pmic_ldo_vibr_is_enabled,
	.get_voltage_sel = pmic_ldo_vibr_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vibr_set_voltage_sel,
	.list_voltage = pmic_ldo_vibr_list_voltage,
	/* .enable_time = pmic_ldo_vibr_enable_time, */
};


static ssize_t show_LDO_STATUS(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_regulator *mreg;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, en_att);

	ret_value = pmic_get_register_value(mreg->en_reg);

	PMICLOG("[EM] LDO_%s_STATUS : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_LDO_STATUS(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

static ssize_t show_LDO_VOLTAGE(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_regulator *mreg;
	const int *pVoltage;

	unsigned short regVal;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, voltage_att);

	if (mreg->desc.n_voltages != 1) {
		if (mreg->vol_reg != 0) {
			regVal = pmic_get_register_value(mreg->vol_reg);
			if (mreg->pvoltages != NULL) {
				pVoltage = (const int *)mreg->pvoltages;
				/*HW LDO sequence issue, we need to change it */
				ret_value = pVoltage[regVal];
			} else {
				ret_value = mreg->desc.min_uV + mreg->desc.uV_step * regVal;
			}
		} else {
			PMICLOG("[EM][ERROR] LDO_%s_VOLTAGE : voltage=0 vol_reg=0\n",
				mreg->desc.name);
		}
	} else {
		pVoltage = (const int *)mreg->pvoltages;
		ret_value = pVoltage[0];
	}

	ret_value = ret_value / 1000;
	pr_err("[EM] LDO_%s_VOLTAGE : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);

}

static ssize_t store_LDO_VOLTAGE(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}


/* Regulator: LDO */
#define LDO_EN	REGULATOR_CHANGE_STATUS
#define LDO_VOL REGULATOR_CHANGE_VOLTAGE
#define LDO_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mtk_ldos[] = {
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vtcxo24, VTCXO24, ldo, vtcxo24_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VTCXO24_EN, PMIC_RG_VTCXO24_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vxo22, VXO22, ldo, vxo22_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VXO22_EN, PMIC_RG_VXO22_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_bt, VCN33_BT, ldo, vcn33_bt_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCN33_EN_BT, PMIC_RG_VCN33_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_wifi, VCN33_WIFI, ldo, vcn33_wifi_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCN33_EN_WIFI, PMIC_RG_VCN33_VOSEL),
	REGULAR_VOLTAGE_REGULATOR_GEN(vsram_proc, VSRAM_PROC, ldo, 600000, 1393750, 6250, 0,
		LDO_VOL_EN, 1, PMIC_LDO_VSRAM_PROC_EN, PMIC_LDO_VSRAM_PROC_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28, VLDO28, ldo, vldo28_0_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VLDO28_EN_0, PMIC_RG_VLDO28_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28_1, VLDO28_1, ldo, vldo28_1_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VLDO28_EN_1, PMIC_RG_VLDO28_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vtcxo28, VTCXO28, ldo, vtcxo28_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VTCXO28_EN, PMIC_RG_VTCXO28_VOSEL),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18, VRF18, ldo, vrf18_voltages,
		LDO_EN, 1, PMIC_LDO_VRF18_EN),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vrf12, VRF12, ldo, vrf12_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VRF12_EN, PMIC_RG_VRF12_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn28, VCN28, ldo, vcn28_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCN28_EN, PMIC_RG_VCN28_VOSEL),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn18, VCN18, ldo, vcn18_voltages,
		LDO_EN, 1, PMIC_LDO_VCN18_EN),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama, VCAMA, ldo, vcama_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCAMA_EN, PMIC_RG_VCAMA_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamio, VCAMIO, ldo, vcamio_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCAMIO_EN, PMIC_RG_VCAMIO_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamd, VCAMD, ldo, vcamd_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VCAMD_EN, PMIC_RG_VCAMD_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vaux18, VAUX18, ldo, vaux18_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VAUX18_EN, PMIC_RG_VAUX18_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vaud28, VAUD28, ldo, vaud28_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VAUD28_EN, PMIC_RG_VAUD28_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vdram, VDRAM, ldo, vdram_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VDRAM_EN, PMIC_RG_VDRAM_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim1, VSIM1, ldo, vsim1_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VSIM1_EN, PMIC_RG_VSIM1_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim2, VSIM2, ldo, vsim2_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VSIM2_EN, PMIC_RG_VSIM2_VOSEL),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio28, VIO28, ldo, vio28_voltages,
		LDO_EN, 1, PMIC_LDO_VIO28_EN),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmc, VMC, ldo, vmc_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VMC_EN, PMIC_RG_VMC_VOSEL),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmch, VMCH, ldo, vmch_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VMCH_EN, PMIC_RG_VMCH_VOSEL),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vusb33, VUSB33, ldo, vusb33_voltages,
		LDO_EN, 1, PMIC_LDO_VUSB33_EN),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vemc, VEMC, ldo, vemc33_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VEMC33_EN, PMIC_RG_VEMC33_VOSEL),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio18, VIO18, ldo, vio18_voltages,
		LDO_EN, 1, PMIC_LDO_VIO18_EN),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vibr, VIBR, ldo, vibr_voltages,
		LDO_VOL_EN, 1, PMIC_LDO_VIBR_EN, PMIC_RG_VIBR_VOSEL),

};

int mtk_ldos_size = ARRAY_SIZE(mtk_ldos);


/* -------Code Gen End-------*/

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY

#define PMIC_REGULATOR_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mtk_ldos[MT6353_POWER_LDO_##_id],	\
	}

struct of_regulator_match pmic_regulator_matches[] = {
	PMIC_REGULATOR_OF_MATCH(ldo_vtcxo24, VTCXO24),
	PMIC_REGULATOR_OF_MATCH(ldo_vxo22, VXO22),
	PMIC_REGULATOR_OF_MATCH(ldo_vcn33_bt, VCN33_BT),
	PMIC_REGULATOR_OF_MATCH(ldo_vcn33_wifi, VCN33_WIFI),
	PMIC_REGULATOR_OF_MATCH(ldo_vsram_proc, VSRAM_PROC),
	PMIC_REGULATOR_OF_MATCH(ldo_vldo28, VLDO28), /* for mt_pm6353_ldo.h */
	PMIC_REGULATOR_OF_MATCH(ldo_vldo28_1, VLDO28_1),
	PMIC_REGULATOR_OF_MATCH(ldo_vtcxo28, VTCXO28),
	PMIC_REGULATOR_OF_MATCH(ldo_vrf18, VRF18),
	PMIC_REGULATOR_OF_MATCH(ldo_vrf12, VRF12),
	PMIC_REGULATOR_OF_MATCH(ldo_vcn28, VCN28),
	PMIC_REGULATOR_OF_MATCH(ldo_vcn18, VCN18),
	PMIC_REGULATOR_OF_MATCH(ldo_vcama, VCAMA),
	PMIC_REGULATOR_OF_MATCH(ldo_vcamio, VCAMIO),
	PMIC_REGULATOR_OF_MATCH(ldo_vcamd, VCAMD),
	PMIC_REGULATOR_OF_MATCH(ldo_vaux18, VAUX18),
	PMIC_REGULATOR_OF_MATCH(ldo_vaud28, VAUD28),
	PMIC_REGULATOR_OF_MATCH(ldo_vdram, VDRAM),
	PMIC_REGULATOR_OF_MATCH(ldo_vsim1, VSIM1),
	PMIC_REGULATOR_OF_MATCH(ldo_vsim2, VSIM2),
	PMIC_REGULATOR_OF_MATCH(ldo_vio28, VIO28),
	PMIC_REGULATOR_OF_MATCH(ldo_vmc, VMC),
	PMIC_REGULATOR_OF_MATCH(ldo_vmch, VMCH),
	PMIC_REGULATOR_OF_MATCH(ldo_vusb33, VUSB33),
	PMIC_REGULATOR_OF_MATCH(ldo_vemc, VEMC), /* for mt_pm6353_ldo.h */
	PMIC_REGULATOR_OF_MATCH(ldo_vio18, VIO18),
	PMIC_REGULATOR_OF_MATCH(ldo_vibr, VIBR),
};

int pmic_regulator_matches_size = ARRAY_SIZE(pmic_regulator_matches);

#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */


