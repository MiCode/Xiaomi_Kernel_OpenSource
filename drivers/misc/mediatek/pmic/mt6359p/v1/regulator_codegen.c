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
#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"

static const int vaud18_voltages[] = {
	1800000,
};

static const int vsim1_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};

static const int vibr_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2000000,
	2700000,
	2800000,
	3000000,
	3300000,
};

static const int vrf12_voltages[] = {
	1100000,
	1200000,
	1300000,
};

static const int vusb_voltages[] = {
	3000000,
};

static const int vio18_voltages[] = {
	1700000,
	1800000,
	1900000,
};

static const int vcamio_voltages[] = {
	1700000,
	1800000,
	1900000,
};

static const int vcn18_voltages[] = {
	1800000,
};

static const int vfe28_voltages[] = {
	2800000,
};

static const int vcn13_voltages[] = {
	900000,
	1000000,
	1200000,
	1300000,
};

static const int vcn33_1_bt_voltages[] = {
	2800000,
	3300000,
	3400000,
	3500000,
};

static const int vcn33_1_wifi_voltages[] = {
	2800000,
	3300000,
	3400000,
	3500000,
};

static const int vaux18_voltages[] = {
	1800000,
};

static const int vefuse_voltages[] = {
	1700000,
	1800000,
	1900000,
	2000000,
};

static const int vxo22_voltages[] = {
	1800000,
	2200000,
};

static const int vrfck_voltages[] = {
	1500000,
	1600000,
	1700000,
};

static const int vbif28_voltages[] = {
	2800000,
};

static const int vio28_voltages[] = {
	2800000,
	2900000,
	3000000,
	3100000,
	3300000,
};

static const int vemc_voltages[] = {
	2900000,
	3000000,
	3300000,
};

static const int vcn33_2_bt_voltages[] = {
	2800000,
	3300000,
	3400000,
	3500000,
};

static const int vcn33_2_wifi_voltages[] = {
	2800000,
	3300000,
	3400000,
	3500000,
};

static const int va12_voltages[] = {
	1200000,
	1300000,
};

static const int va09_voltages[] = {
	800000,
	900000,
	1200000,
};

static const int vrf18_voltages[] = {
	1700000,
	1800000,
	1810000,
};

static const int vufs_voltages[] = {
	1700000,
	1800000,
	1900000,
};

static const int vm18_voltages[] = {
	1700000,
	1800000,
	1900000,
};

static const int vbbck_voltages[] = {
	1100000,
	1150000,
	1200000,
};

static const int vsim2_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};



static const int vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const int vibr_idx[] = {
	0, 1, 2, 4, 5, 8, 9, 11, 13,
};

static const int vrf12_idx[] = {
	2, 3, 4,
};

static const int vio18_idx[] = {
	11, 12, 13,
};

static const int vcamio_idx[] = {
	11, 12, 13,
};

static const int vcn13_idx[] = {
	0, 1, 3, 4,
};

static const int vcn33_1_bt_idx[] = {
	9, 13, 14, 15,
};

static const int vcn33_1_wifi_idx[] = {
	9, 13, 14, 15,
};

static const int vefuse_idx[] = {
	11, 12, 13, 14,
};

static const int vxo22_idx[] = {
	0, 4,
};

static const int vrfck_idx[] = {
	2, 7, 12,
};

static const int vio28_idx[] = {
	9, 10, 11, 12, 13,
};

static const int vemc_idx[] = {
	10, 11, 13,
};

static const int vcn33_2_bt_idx[] = {
	9, 13, 14, 15,
};

static const int vcn33_2_wifi_idx[] = {
	9, 13, 14, 15,
};

static const int va12_idx[] = {
	6, 7,
};

static const int va09_idx[] = {
	2, 3, 6,
};

static const int vrf18_idx[] = {
	5, 6, 7,
};

static const int vufs_idx[] = {
	11, 12, 13,
};

static const int vm18_idx[] = {
	11, 12, 13,
};

static const int vbbck_idx[] = {
	4, 8, 12,
};

static const int vsim2_idx[] = {
	3, 4, 8, 11, 12,
};



/* Regulator vaud18 enable */
static int pmic_ldo_vaud18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vaud18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vaud18 disable */
static int pmic_ldo_vaud18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vaud18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vaud18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vaud18 is_enabled */
static int pmic_ldo_vaud18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vaud18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsim1 enable */
static int pmic_ldo_vsim1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsim1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim1 disable */
static int pmic_ldo_vsim1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsim1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsim1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsim1 is_enabled */
static int pmic_ldo_vsim1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsim1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsim1 set_voltage_sel */
static int pmic_ldo_vsim1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vsim1_idx[selector];

	PMICLOG("ldo vsim1 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vsim1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim1 get_voltage_sel */
static int pmic_ldo_vsim1_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsim1 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vsim1_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vsim1 list_voltage */
static int pmic_ldo_vsim1_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vsim1_voltages[selector];
	PMICLOG("ldo vsim1 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vs1 enable */
static int pmic_buck_vs1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vs1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vs1 disable */
static int pmic_buck_vs1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vs1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vs1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vs1 is_enabled */
static int pmic_buck_vs1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vs1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vs1 set_voltage_sel */
static int pmic_buck_vs1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vs1 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vs1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vs1 get_voltage_sel */
static int pmic_buck_vs1_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vs1 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vgpu11 enable */
static int pmic_buck_vgpu11_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu11 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vgpu11 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgpu11 disable */
static int pmic_buck_vgpu11_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu11 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vgpu11 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vgpu11 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vgpu11 is_enabled */
static int pmic_buck_vgpu11_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu11 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vgpu11 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vgpu11 set_voltage_sel */
static int pmic_buck_vgpu11_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vgpu11 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vgpu11 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgpu11 get_voltage_sel */
static int pmic_buck_vgpu11_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu11 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vgpu11 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vibr enable */
static int pmic_ldo_vibr_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vibr enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vibr don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vibr disable */
static int pmic_ldo_vibr_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vibr disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vibr should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vibr don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vibr is_enabled */
static int pmic_ldo_vibr_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vibr is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vibr don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vibr set_voltage_sel */
static int pmic_ldo_vibr_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vibr_idx[selector];

	PMICLOG("ldo vibr set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vibr don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vibr get_voltage_sel */
static int pmic_ldo_vibr_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vibr get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vibr don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vibr_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vibr list_voltage */
static int pmic_ldo_vibr_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vibr_voltages[selector];
	PMICLOG("ldo vibr list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vrf12 enable */
static int pmic_ldo_vrf12_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf12 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vrf12 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf12 disable */
static int pmic_ldo_vrf12_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf12 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vrf12 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vrf12 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vrf12 is_enabled */
static int pmic_ldo_vrf12_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf12 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vrf12 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vrf12 set_voltage_sel */
static int pmic_ldo_vrf12_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vrf12_idx[selector];

	PMICLOG("ldo vrf12 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vrf12 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf12 get_voltage_sel */
static int pmic_ldo_vrf12_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf12 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vrf12 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vrf12_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vrf12 list_voltage */
static int pmic_ldo_vrf12_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vrf12_voltages[selector];
	PMICLOG("ldo vrf12 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vusb enable */
static int pmic_ldo_vusb_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vusb enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vusb don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vusb disable */
static int pmic_ldo_vusb_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vusb disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vusb should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vusb don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vusb is_enabled */
static int pmic_ldo_vusb_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vusb is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vusb don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc2 enable */
static int pmic_ldo_vsram_proc2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_proc2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc2 disable */
static int pmic_ldo_vsram_proc2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_proc2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_proc2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_proc2 is_enabled */
static int pmic_ldo_vsram_proc2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_proc2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc2 set_voltage_sel */
static int pmic_ldo_vsram_proc2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_proc2 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_proc2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc2 get_voltage_sel */
static int pmic_ldo_vsram_proc2_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_proc2 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vio18 enable */
static int pmic_ldo_vio18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vio18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vio18 disable */
static int pmic_ldo_vio18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vio18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vio18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vio18 is_enabled */
static int pmic_ldo_vio18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vio18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vio18 set_voltage_sel */
static int pmic_ldo_vio18_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vio18_idx[selector];

	PMICLOG("ldo vio18 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vio18 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vio18 get_voltage_sel */
static int pmic_ldo_vio18_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio18 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vio18 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vio18_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vio18 list_voltage */
static int pmic_ldo_vio18_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vio18_voltages[selector];
	PMICLOG("ldo vio18 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcamio enable */
static int pmic_ldo_vcamio_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamio enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcamio don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamio disable */
static int pmic_ldo_vcamio_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamio disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcamio should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcamio don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcamio is_enabled */
static int pmic_ldo_vcamio_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamio is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcamio don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamio set_voltage_sel */
static int pmic_ldo_vcamio_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcamio_idx[selector];

	PMICLOG("ldo vcamio set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcamio don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamio get_voltage_sel */
static int pmic_ldo_vcamio_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamio get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcamio don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcamio_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcamio list_voltage */
static int pmic_ldo_vcamio_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcamio_voltages[selector];
	PMICLOG("ldo vcamio list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn18 enable */
static int pmic_ldo_vcn18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn18 disable */
static int pmic_ldo_vcn18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn18 is_enabled */
static int pmic_ldo_vcn18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vfe28 enable */
static int pmic_ldo_vfe28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vfe28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vfe28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vfe28 disable */
static int pmic_ldo_vfe28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vfe28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vfe28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vfe28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vfe28 is_enabled */
static int pmic_ldo_vfe28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vfe28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vfe28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn13 enable */
static int pmic_ldo_vcn13_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn13 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn13 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn13 disable */
static int pmic_ldo_vcn13_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn13 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn13 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn13 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn13 is_enabled */
static int pmic_ldo_vcn13_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn13 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn13 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn13 set_voltage_sel */
static int pmic_ldo_vcn13_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn13_idx[selector];

	PMICLOG("ldo vcn13 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn13 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn13 get_voltage_sel */
static int pmic_ldo_vcn13_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn13 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn13 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn13_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn13 list_voltage */
static int pmic_ldo_vcn13_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn13_voltages[selector];
	PMICLOG("ldo vcn13 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn33_1_bt enable */
static int pmic_ldo_vcn33_1_bt_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_bt enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_1_bt don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_1_bt disable */
static int pmic_ldo_vcn33_1_bt_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_bt disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn33_1_bt should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn33_1_bt don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_1_bt is_enabled */
static int pmic_ldo_vcn33_1_bt_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_bt is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_1_bt don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_1_bt set_voltage_sel */
static int pmic_ldo_vcn33_1_bt_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_1_bt_idx[selector];

	PMICLOG("ldo vcn33_1_bt set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_1_bt don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_1_bt get_voltage_sel */
static int pmic_ldo_vcn33_1_bt_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_bt get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_1_bt don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_1_bt_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_1_bt list_voltage */
static int pmic_ldo_vcn33_1_bt_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_1_bt_voltages[selector];
	PMICLOG("ldo vcn33_1_bt list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn33_1_wifi enable */
static int pmic_ldo_vcn33_1_wifi_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_wifi enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_1_wifi don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_1_wifi disable */
static int pmic_ldo_vcn33_1_wifi_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_wifi disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("vcn33_1_wifi should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("vcn33_1_wifi don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_1_wifi is_enabled */
static int pmic_ldo_vcn33_1_wifi_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_wifi is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_1_wifi don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_1_wifi set_voltage_sel */
static int pmic_ldo_vcn33_1_wifi_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_1_wifi_idx[selector];

	PMICLOG("ldo vcn33_1_wifi set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_1_wifi don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_1_wifi get_voltage_sel */
static int pmic_ldo_vcn33_1_wifi_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_1_wifi get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_1_wifi don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_1_wifi_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_1_wifi list_voltage */
static int pmic_ldo_vcn33_1_wifi_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_1_wifi_voltages[selector];
	PMICLOG("ldo vcn33_1_wifi list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vaux18 enable */
static int pmic_ldo_vaux18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaux18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vaux18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vaux18 disable */
static int pmic_ldo_vaux18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaux18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vaux18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vaux18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vaux18 is_enabled */
static int pmic_ldo_vaux18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaux18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vaux18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmodem enable */
static int pmic_buck_vmodem_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vmodem enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vmodem don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmodem disable */
static int pmic_buck_vmodem_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vmodem disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vmodem should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vmodem don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vmodem is_enabled */
static int pmic_buck_vmodem_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vmodem is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vmodem don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmodem set_voltage_sel */
static int pmic_buck_vmodem_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vmodem set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vmodem don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmodem get_voltage_sel */
static int pmic_buck_vmodem_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vmodem get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vmodem don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vpu enable */
static int pmic_buck_vpu_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpu enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vpu don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vpu disable */
static int pmic_buck_vpu_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpu disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vpu should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vpu don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vpu is_enabled */
static int pmic_buck_vpu_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpu is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vpu don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vpu set_voltage_sel */
static int pmic_buck_vpu_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vpu set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vpu don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vpu get_voltage_sel */
static int pmic_buck_vpu_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpu get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vpu don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vcore enable */
static int pmic_buck_vcore_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vcore enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vcore don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcore disable */
static int pmic_buck_vcore_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vcore disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vcore should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vcore don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcore is_enabled */
static int pmic_buck_vcore_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vcore is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vcore don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcore set_voltage_sel */
static int pmic_buck_vcore_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vcore set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vcore don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcore get_voltage_sel */
static int pmic_buck_vcore_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vcore get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vcore don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vsram_others enable */
static int pmic_ldo_vsram_others_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_others enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_others don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_others disable */
static int pmic_ldo_vsram_others_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_others disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("vsram_others should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("vsram_others don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_others is_enabled */
static int pmic_ldo_vsram_others_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_others is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_others don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_others set_voltage_sel */
static int pmic_ldo_vsram_others_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_others set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_others don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_others get_voltage_sel */
static int pmic_ldo_vsram_others_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_others get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_others don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vs2 enable */
static int pmic_buck_vs2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vs2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vs2 disable */
static int pmic_buck_vs2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vs2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vs2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vs2 is_enabled */
static int pmic_buck_vs2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vs2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vs2 set_voltage_sel */
static int pmic_buck_vs2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vs2 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vs2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vs2 get_voltage_sel */
static int pmic_buck_vs2_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vs2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vs2 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vefuse enable */
static int pmic_ldo_vefuse_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vefuse enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vefuse don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vefuse disable */
static int pmic_ldo_vefuse_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vefuse disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vefuse should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vefuse don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vefuse is_enabled */
static int pmic_ldo_vefuse_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vefuse is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vefuse don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vefuse set_voltage_sel */
static int pmic_ldo_vefuse_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vefuse_idx[selector];

	PMICLOG("ldo vefuse set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vefuse don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vefuse get_voltage_sel */
static int pmic_ldo_vefuse_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vefuse get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vefuse don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vefuse_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vefuse list_voltage */
static int pmic_ldo_vefuse_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vefuse_voltages[selector];
	PMICLOG("ldo vefuse list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vxo22 enable */
static int pmic_ldo_vxo22_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo22 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vxo22 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vxo22 disable */
static int pmic_ldo_vxo22_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo22 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vxo22 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vxo22 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vxo22 is_enabled */
static int pmic_ldo_vxo22_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo22 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vxo22 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vxo22 set_voltage_sel */
static int pmic_ldo_vxo22_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vxo22_idx[selector];

	PMICLOG("ldo vxo22 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vxo22 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vxo22 get_voltage_sel */
static int pmic_ldo_vxo22_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo22 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vxo22 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vxo22_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vxo22 list_voltage */
static int pmic_ldo_vxo22_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vxo22_voltages[selector];
	PMICLOG("ldo vxo22 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vpa enable */
static int pmic_buck_vpa_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpa enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vpa don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vpa disable */
static int pmic_buck_vpa_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpa disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vpa should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vpa don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vpa is_enabled */
static int pmic_buck_vpa_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpa is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vpa don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vpa set_voltage_sel */
static int pmic_buck_vpa_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vpa set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vpa don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vpa get_voltage_sel */
static int pmic_buck_vpa_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vpa get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vpa don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vrfck enable */
static int pmic_ldo_vrfck_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrfck enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vrfck don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrfck disable */
static int pmic_ldo_vrfck_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrfck disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vrfck should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vrfck don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vrfck is_enabled */
static int pmic_ldo_vrfck_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrfck is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vrfck don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vrfck set_voltage_sel */
static int pmic_ldo_vrfck_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vrfck_idx[selector];

	PMICLOG("ldo vrfck set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vrfck don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrfck get_voltage_sel */
static int pmic_ldo_vrfck_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrfck get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vrfck don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vrfck_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vrfck list_voltage */
static int pmic_ldo_vrfck_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vrfck_voltages[selector];
	PMICLOG("ldo vrfck list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vbif28 enable */
static int pmic_ldo_vbif28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbif28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vbif28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vbif28 disable */
static int pmic_ldo_vbif28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbif28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vbif28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vbif28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vbif28 is_enabled */
static int pmic_ldo_vbif28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbif28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vbif28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc2 enable */
static int pmic_buck_vproc2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vproc2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc2 disable */
static int pmic_buck_vproc2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vproc2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vproc2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vproc2 is_enabled */
static int pmic_buck_vproc2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vproc2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc2 set_voltage_sel */
static int pmic_buck_vproc2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vproc2 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vproc2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc2 get_voltage_sel */
static int pmic_buck_vproc2_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vproc2 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vio28 enable */
static int pmic_ldo_vio28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vio28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vio28 disable */
static int pmic_ldo_vio28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vio28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vio28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vio28 is_enabled */
static int pmic_ldo_vio28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vio28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vio28 set_voltage_sel */
static int pmic_ldo_vio28_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vio28_idx[selector];

	PMICLOG("ldo vio28 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vio28 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vio28 get_voltage_sel */
static int pmic_ldo_vio28_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vio28 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vio28 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vio28_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vio28 list_voltage */
static int pmic_ldo_vio28_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vio28_voltages[selector];
	PMICLOG("ldo vio28 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vemc enable */
static int pmic_ldo_vemc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vemc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vemc don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vemc disable */
static int pmic_ldo_vemc_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vemc disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vemc should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vemc don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vemc is_enabled */
static int pmic_ldo_vemc_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vemc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vemc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vemc set_voltage_sel */
static int pmic_ldo_vemc_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vemc_idx[selector];

	PMICLOG("ldo vemc set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vemc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vemc get_voltage_sel */
static int pmic_ldo_vemc_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vemc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vemc don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vemc_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vemc list_voltage */
static int pmic_ldo_vemc_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vemc_voltages[selector];
	PMICLOG("ldo vemc list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn33_2_bt enable */
static int pmic_ldo_vcn33_2_bt_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_bt enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_2_bt don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_2_bt disable */
static int pmic_ldo_vcn33_2_bt_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_bt disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn33_2_bt should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn33_2_bt don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_2_bt is_enabled */
static int pmic_ldo_vcn33_2_bt_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_bt is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_2_bt don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_2_bt set_voltage_sel */
static int pmic_ldo_vcn33_2_bt_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_2_bt_idx[selector];

	PMICLOG("ldo vcn33_2_bt set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_2_bt don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_2_bt get_voltage_sel */
static int pmic_ldo_vcn33_2_bt_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_bt get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_2_bt don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_2_bt_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_2_bt list_voltage */
static int pmic_ldo_vcn33_2_bt_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_2_bt_voltages[selector];
	PMICLOG("ldo vcn33_2_bt list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn33_2_wifi enable */
static int pmic_ldo_vcn33_2_wifi_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_wifi enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_2_wifi don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_2_wifi disable */
static int pmic_ldo_vcn33_2_wifi_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_wifi disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("vcn33_2_wifi should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("vcn33_2_wifi don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_2_wifi is_enabled */
static int pmic_ldo_vcn33_2_wifi_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_wifi is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_2_wifi don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_2_wifi set_voltage_sel */
static int pmic_ldo_vcn33_2_wifi_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_2_wifi_idx[selector];

	PMICLOG("ldo vcn33_2_wifi set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_2_wifi don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_2_wifi get_voltage_sel */
static int pmic_ldo_vcn33_2_wifi_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_2_wifi get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_2_wifi don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_2_wifi_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_2_wifi list_voltage */
static int pmic_ldo_vcn33_2_wifi_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_2_wifi_voltages[selector];
	PMICLOG("ldo vcn33_2_wifi list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator va12 enable */
static int pmic_ldo_va12_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va12 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo va12 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va12 disable */
static int pmic_ldo_va12_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va12 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo va12 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo va12 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator va12 is_enabled */
static int pmic_ldo_va12_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va12 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo va12 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator va12 set_voltage_sel */
static int pmic_ldo_va12_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = va12_idx[selector];

	PMICLOG("ldo va12 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo va12 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va12 get_voltage_sel */
static int pmic_ldo_va12_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va12 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo va12 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (va12_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator va12 list_voltage */
static int pmic_ldo_va12_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = va12_voltages[selector];
	PMICLOG("ldo va12 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vproc1 enable */
static int pmic_buck_vproc1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vproc1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc1 disable */
static int pmic_buck_vproc1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vproc1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vproc1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vproc1 is_enabled */
static int pmic_buck_vproc1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vproc1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc1 set_voltage_sel */
static int pmic_buck_vproc1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vproc1 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vproc1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc1 get_voltage_sel */
static int pmic_buck_vproc1_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vproc1 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator va09 enable */
static int pmic_ldo_va09_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va09 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo va09 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va09 disable */
static int pmic_ldo_va09_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va09 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo va09 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo va09 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator va09 is_enabled */
static int pmic_ldo_va09_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va09 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo va09 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator va09 set_voltage_sel */
static int pmic_ldo_va09_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = va09_idx[selector];

	PMICLOG("ldo va09 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo va09 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va09 get_voltage_sel */
static int pmic_ldo_va09_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va09 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo va09 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (va09_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator va09 list_voltage */
static int pmic_ldo_va09_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = va09_voltages[selector];
	PMICLOG("ldo va09 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vrf18 enable */
static int pmic_ldo_vrf18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vrf18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf18 disable */
static int pmic_ldo_vrf18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vrf18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vrf18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vrf18 is_enabled */
static int pmic_ldo_vrf18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vrf18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vrf18 set_voltage_sel */
static int pmic_ldo_vrf18_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vrf18_idx[selector];

	PMICLOG("ldo vrf18 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vrf18 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf18 get_voltage_sel */
static int pmic_ldo_vrf18_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vrf18 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vrf18_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vrf18 list_voltage */
static int pmic_ldo_vrf18_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vrf18_voltages[selector];
	PMICLOG("ldo vrf18 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vsram_md enable */
static int pmic_ldo_vsram_md_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_md enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_md don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_md disable */
static int pmic_ldo_vsram_md_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_md disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_md should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_md don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_md is_enabled */
static int pmic_ldo_vsram_md_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_md is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_md don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_md set_voltage_sel */
static int pmic_ldo_vsram_md_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_md set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_md don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_md get_voltage_sel */
static int pmic_ldo_vsram_md_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_md get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_md don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vufs enable */
static int pmic_ldo_vufs_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vufs don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vufs disable */
static int pmic_ldo_vufs_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vufs should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vufs don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vufs is_enabled */
static int pmic_ldo_vufs_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vufs don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vufs set_voltage_sel */
static int pmic_ldo_vufs_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vufs_idx[selector];

	PMICLOG("ldo vufs set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vufs don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vufs get_voltage_sel */
static int pmic_ldo_vufs_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vufs don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vufs_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vufs list_voltage */
static int pmic_ldo_vufs_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vufs_voltages[selector];
	PMICLOG("ldo vufs list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vm18 enable */
static int pmic_ldo_vm18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vm18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vm18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vm18 disable */
static int pmic_ldo_vm18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vm18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vm18 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vm18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vm18 is_enabled */
static int pmic_ldo_vm18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vm18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vm18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vm18 set_voltage_sel */
static int pmic_ldo_vm18_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vm18_idx[selector];

	PMICLOG("ldo vm18 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vm18 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vm18 get_voltage_sel */
static int pmic_ldo_vm18_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vm18 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vm18 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vm18_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vm18 list_voltage */
static int pmic_ldo_vm18_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vm18_voltages[selector];
	PMICLOG("ldo vm18 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vbbck enable */
static int pmic_ldo_vbbck_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbbck enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vbbck don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vbbck disable */
static int pmic_ldo_vbbck_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbbck disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vbbck should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vbbck don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vbbck is_enabled */
static int pmic_ldo_vbbck_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbbck is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vbbck don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vbbck set_voltage_sel */
static int pmic_ldo_vbbck_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vbbck_idx[selector];

	PMICLOG("ldo vbbck set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vbbck don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vbbck get_voltage_sel */
static int pmic_ldo_vbbck_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vbbck get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vbbck don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vbbck_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vbbck list_voltage */
static int pmic_ldo_vbbck_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vbbck_voltages[selector];
	PMICLOG("ldo vbbck list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vsram_proc1 enable */
static int pmic_ldo_vsram_proc1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_proc1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc1 disable */
static int pmic_ldo_vsram_proc1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_proc1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_proc1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_proc1 is_enabled */
static int pmic_ldo_vsram_proc1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_proc1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc1 set_voltage_sel */
static int pmic_ldo_vsram_proc1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_proc1 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_proc1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc1 get_voltage_sel */
static int pmic_ldo_vsram_proc1_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_proc1 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vsim2 enable */
static int pmic_ldo_vsim2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsim2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim2 disable */
static int pmic_ldo_vsim2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsim2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsim2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsim2 is_enabled */
static int pmic_ldo_vsim2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsim2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsim2 set_voltage_sel */
static int pmic_ldo_vsim2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vsim2_idx[selector];

	PMICLOG("ldo vsim2 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vsim2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim2 get_voltage_sel */
static int pmic_ldo_vsim2_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsim2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsim2 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vsim2_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vsim2 list_voltage */
static int pmic_ldo_vsim2_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vsim2_voltages[selector];
	PMICLOG("ldo vsim2 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

static unsigned int pmic_buck_get_mode(struct regulator_dev *rdev)
{
	struct mtk_regulator *mreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (pmic_get_register_value(mreg->modeset_reg) == 1)
		mode = REGULATOR_MODE_FAST;
	else if (pmic_get_register_value(mreg->lp_mode_reg) == 1)
		mode = REGULATOR_MODE_IDLE;
	else
		mode = REGULATOR_MODE_NORMAL;
	return mode;
}

static int pmic_buck_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mtk_regulator *mreg = rdev_get_drvdata(rdev);
	int curr_mode;

	curr_mode = pmic_buck_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		if (curr_mode == REGULATOR_MODE_IDLE) {
			WARN_ON(1);
			pr_notice("BUCK %s is LP mode, can't FPWM\n",
				mreg->desc.name);
			return -EIO;
		}
		if (pmic_get_register_value(mreg->modeset_reg) == 0)
			pmic_set_register_value(mreg->modeset_reg, 1);
		PMICLOG("BUCK %s set FPWM mode pass\n",
			mreg->desc.name);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST)
			pmic_set_register_value(mreg->modeset_reg, 0);
		else if (curr_mode == REGULATOR_MODE_IDLE) {
			pmic_set_register_value(mreg->lp_mode_reg, 0);
			udelay(100);
			PMICLOG("BUCK %s leave LP mode pass\n",
				mreg->desc.name);
		}
		break;
	case REGULATOR_MODE_IDLE:
		if (curr_mode == REGULATOR_MODE_FAST) {
			WARN_ON(1);
			pr_notice("BUCK %s is FPWM mode, can't enter LP\n",
				mreg->desc.name);
			return -EIO;
		}
		pmic_set_register_value(mreg->lp_mode_reg, 1);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* Regulator vaud18 ops */
static struct regulator_ops pmic_ldo_vaud18_ops = {
	.enable = pmic_ldo_vaud18_enable,
	.disable = pmic_ldo_vaud18_disable,
	.is_enabled = pmic_ldo_vaud18_is_enabled,
	/* .enable_time = pmic_ldo_vaud18_enable_time, */
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

/* Regulator vs1 ops */
static struct regulator_ops pmic_buck_vs1_ops = {
	.enable = pmic_buck_vs1_enable,
	.disable = pmic_buck_vs1_disable,
	.is_enabled = pmic_buck_vs1_is_enabled,
	.get_voltage_sel = pmic_buck_vs1_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vs1_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vs1_enable_time, */
};

/* Regulator vgpu11 ops */
static struct regulator_ops pmic_buck_vgpu11_ops = {
	.enable = pmic_buck_vgpu11_enable,
	.disable = pmic_buck_vgpu11_disable,
	.is_enabled = pmic_buck_vgpu11_is_enabled,
	.get_voltage_sel = pmic_buck_vgpu11_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vgpu11_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vgpu11_enable_time, */
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

/* Regulator vusb ops */
static struct regulator_ops pmic_ldo_vusb_ops = {
	.enable = pmic_ldo_vusb_enable,
	.disable = pmic_ldo_vusb_disable,
	.is_enabled = pmic_ldo_vusb_is_enabled,
	/* .enable_time = pmic_ldo_vusb_enable_time, */
};

/* Regulator vsram_proc2 ops */
static struct regulator_ops pmic_ldo_vsram_proc2_ops = {
	.enable = pmic_ldo_vsram_proc2_enable,
	.disable = pmic_ldo_vsram_proc2_disable,
	.is_enabled = pmic_ldo_vsram_proc2_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc2_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_proc2_enable_time, */
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

/* Regulator vcn18 ops */
static struct regulator_ops pmic_ldo_vcn18_ops = {
	.enable = pmic_ldo_vcn18_enable,
	.disable = pmic_ldo_vcn18_disable,
	.is_enabled = pmic_ldo_vcn18_is_enabled,
	/* .enable_time = pmic_ldo_vcn18_enable_time, */
};

/* Regulator vfe28 ops */
static struct regulator_ops pmic_ldo_vfe28_ops = {
	.enable = pmic_ldo_vfe28_enable,
	.disable = pmic_ldo_vfe28_disable,
	.is_enabled = pmic_ldo_vfe28_is_enabled,
	/* .enable_time = pmic_ldo_vfe28_enable_time, */
};

/* Regulator vcn13 ops */
static struct regulator_ops pmic_ldo_vcn13_ops = {
	.enable = pmic_ldo_vcn13_enable,
	.disable = pmic_ldo_vcn13_disable,
	.is_enabled = pmic_ldo_vcn13_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn13_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn13_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn13_list_voltage,
	/* .enable_time = pmic_ldo_vcn13_enable_time, */
};

/* Regulator vcn33_1_bt ops */
static struct regulator_ops pmic_ldo_vcn33_1_bt_ops = {
	.enable = pmic_ldo_vcn33_1_bt_enable,
	.disable = pmic_ldo_vcn33_1_bt_disable,
	.is_enabled = pmic_ldo_vcn33_1_bt_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_1_bt_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_1_bt_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_1_bt_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_1_bt_enable_time, */
};

/* Regulator vcn33_1_wifi ops */
static struct regulator_ops pmic_ldo_vcn33_1_wifi_ops = {
	.enable = pmic_ldo_vcn33_1_wifi_enable,
	.disable = pmic_ldo_vcn33_1_wifi_disable,
	.is_enabled = pmic_ldo_vcn33_1_wifi_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_1_wifi_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_1_wifi_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_1_wifi_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_1_wifi_enable_time, */
};

/* Regulator vaux18 ops */
static struct regulator_ops pmic_ldo_vaux18_ops = {
	.enable = pmic_ldo_vaux18_enable,
	.disable = pmic_ldo_vaux18_disable,
	.is_enabled = pmic_ldo_vaux18_is_enabled,
	/* .enable_time = pmic_ldo_vaux18_enable_time, */
};

/* Regulator vmodem ops */
static struct regulator_ops pmic_buck_vmodem_ops = {
	.enable = pmic_buck_vmodem_enable,
	.disable = pmic_buck_vmodem_disable,
	.is_enabled = pmic_buck_vmodem_is_enabled,
	.get_voltage_sel = pmic_buck_vmodem_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vmodem_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vmodem_enable_time, */
};

/* Regulator vpu ops */
static struct regulator_ops pmic_buck_vpu_ops = {
	.enable = pmic_buck_vpu_enable,
	.disable = pmic_buck_vpu_disable,
	.is_enabled = pmic_buck_vpu_is_enabled,
	.get_voltage_sel = pmic_buck_vpu_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vpu_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vpu_enable_time, */
};

/* Regulator vcore ops */
static struct regulator_ops pmic_buck_vcore_ops = {
	.enable = pmic_buck_vcore_enable,
	.disable = pmic_buck_vcore_disable,
	.is_enabled = pmic_buck_vcore_is_enabled,
	.get_voltage_sel = pmic_buck_vcore_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vcore_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vcore_enable_time, */
};

/* Regulator vsram_others ops */
static struct regulator_ops pmic_ldo_vsram_others_ops = {
	.enable = pmic_ldo_vsram_others_enable,
	.disable = pmic_ldo_vsram_others_disable,
	.is_enabled = pmic_ldo_vsram_others_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_others_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_others_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_others_enable_time, */
};

/* Regulator vs2 ops */
static struct regulator_ops pmic_buck_vs2_ops = {
	.enable = pmic_buck_vs2_enable,
	.disable = pmic_buck_vs2_disable,
	.is_enabled = pmic_buck_vs2_is_enabled,
	.get_voltage_sel = pmic_buck_vs2_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vs2_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vs2_enable_time, */
};

/* Regulator vefuse ops */
static struct regulator_ops pmic_ldo_vefuse_ops = {
	.enable = pmic_ldo_vefuse_enable,
	.disable = pmic_ldo_vefuse_disable,
	.is_enabled = pmic_ldo_vefuse_is_enabled,
	.get_voltage_sel = pmic_ldo_vefuse_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vefuse_set_voltage_sel,
	.list_voltage = pmic_ldo_vefuse_list_voltage,
	/* .enable_time = pmic_ldo_vefuse_enable_time, */
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

/* Regulator vpa ops */
static struct regulator_ops pmic_buck_vpa_ops = {
	.enable = pmic_buck_vpa_enable,
	.disable = pmic_buck_vpa_disable,
	.is_enabled = pmic_buck_vpa_is_enabled,
	.get_voltage_sel = pmic_buck_vpa_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vpa_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vpa_enable_time, */
};

/* Regulator vrfck ops */
static struct regulator_ops pmic_ldo_vrfck_ops = {
	.enable = pmic_ldo_vrfck_enable,
	.disable = pmic_ldo_vrfck_disable,
	.is_enabled = pmic_ldo_vrfck_is_enabled,
	.get_voltage_sel = pmic_ldo_vrfck_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vrfck_set_voltage_sel,
	.list_voltage = pmic_ldo_vrfck_list_voltage,
	/* .enable_time = pmic_ldo_vrfck_enable_time, */
};

/* Regulator vbif28 ops */
static struct regulator_ops pmic_ldo_vbif28_ops = {
	.enable = pmic_ldo_vbif28_enable,
	.disable = pmic_ldo_vbif28_disable,
	.is_enabled = pmic_ldo_vbif28_is_enabled,
	/* .enable_time = pmic_ldo_vbif28_enable_time, */
};

/* Regulator vproc2 ops */
static struct regulator_ops pmic_buck_vproc2_ops = {
	.enable = pmic_buck_vproc2_enable,
	.disable = pmic_buck_vproc2_disable,
	.is_enabled = pmic_buck_vproc2_is_enabled,
	.get_voltage_sel = pmic_buck_vproc2_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc2_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vproc2_enable_time, */
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

/* Regulator vemc ops */
static struct regulator_ops pmic_ldo_vemc_ops = {
	.enable = pmic_ldo_vemc_enable,
	.disable = pmic_ldo_vemc_disable,
	.is_enabled = pmic_ldo_vemc_is_enabled,
	.get_voltage_sel = pmic_ldo_vemc_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vemc_set_voltage_sel,
	.list_voltage = pmic_ldo_vemc_list_voltage,
	/* .enable_time = pmic_ldo_vemc_enable_time, */
};

/* Regulator vcn33_2_bt ops */
static struct regulator_ops pmic_ldo_vcn33_2_bt_ops = {
	.enable = pmic_ldo_vcn33_2_bt_enable,
	.disable = pmic_ldo_vcn33_2_bt_disable,
	.is_enabled = pmic_ldo_vcn33_2_bt_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_2_bt_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_2_bt_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_2_bt_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_2_bt_enable_time, */
};

/* Regulator vcn33_2_wifi ops */
static struct regulator_ops pmic_ldo_vcn33_2_wifi_ops = {
	.enable = pmic_ldo_vcn33_2_wifi_enable,
	.disable = pmic_ldo_vcn33_2_wifi_disable,
	.is_enabled = pmic_ldo_vcn33_2_wifi_is_enabled,
	.get_voltage_sel = pmic_ldo_vcn33_2_wifi_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcn33_2_wifi_set_voltage_sel,
	.list_voltage = pmic_ldo_vcn33_2_wifi_list_voltage,
	/* .enable_time = pmic_ldo_vcn33_2_wifi_enable_time, */
};

/* Regulator va12 ops */
static struct regulator_ops pmic_ldo_va12_ops = {
	.enable = pmic_ldo_va12_enable,
	.disable = pmic_ldo_va12_disable,
	.is_enabled = pmic_ldo_va12_is_enabled,
	.get_voltage_sel = pmic_ldo_va12_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_va12_set_voltage_sel,
	.list_voltage = pmic_ldo_va12_list_voltage,
	/* .enable_time = pmic_ldo_va12_enable_time, */
};

/* Regulator vproc1 ops */
static struct regulator_ops pmic_buck_vproc1_ops = {
	.enable = pmic_buck_vproc1_enable,
	.disable = pmic_buck_vproc1_disable,
	.is_enabled = pmic_buck_vproc1_is_enabled,
	.get_voltage_sel = pmic_buck_vproc1_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc1_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vproc1_enable_time, */
};

/* Regulator va09 ops */
static struct regulator_ops pmic_ldo_va09_ops = {
	.enable = pmic_ldo_va09_enable,
	.disable = pmic_ldo_va09_disable,
	.is_enabled = pmic_ldo_va09_is_enabled,
	.get_voltage_sel = pmic_ldo_va09_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_va09_set_voltage_sel,
	.list_voltage = pmic_ldo_va09_list_voltage,
	/* .enable_time = pmic_ldo_va09_enable_time, */
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

/* Regulator vsram_md ops */
static struct regulator_ops pmic_ldo_vsram_md_ops = {
	.enable = pmic_ldo_vsram_md_enable,
	.disable = pmic_ldo_vsram_md_disable,
	.is_enabled = pmic_ldo_vsram_md_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_md_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_md_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_md_enable_time, */
};

/* Regulator vufs ops */
static struct regulator_ops pmic_ldo_vufs_ops = {
	.enable = pmic_ldo_vufs_enable,
	.disable = pmic_ldo_vufs_disable,
	.is_enabled = pmic_ldo_vufs_is_enabled,
	.get_voltage_sel = pmic_ldo_vufs_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vufs_set_voltage_sel,
	.list_voltage = pmic_ldo_vufs_list_voltage,
	/* .enable_time = pmic_ldo_vufs_enable_time, */
};

/* Regulator vm18 ops */
static struct regulator_ops pmic_ldo_vm18_ops = {
	.enable = pmic_ldo_vm18_enable,
	.disable = pmic_ldo_vm18_disable,
	.is_enabled = pmic_ldo_vm18_is_enabled,
	.get_voltage_sel = pmic_ldo_vm18_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vm18_set_voltage_sel,
	.list_voltage = pmic_ldo_vm18_list_voltage,
	/* .enable_time = pmic_ldo_vm18_enable_time, */
};

/* Regulator vbbck ops */
static struct regulator_ops pmic_ldo_vbbck_ops = {
	.enable = pmic_ldo_vbbck_enable,
	.disable = pmic_ldo_vbbck_disable,
	.is_enabled = pmic_ldo_vbbck_is_enabled,
	.get_voltage_sel = pmic_ldo_vbbck_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vbbck_set_voltage_sel,
	.list_voltage = pmic_ldo_vbbck_list_voltage,
	/* .enable_time = pmic_ldo_vbbck_enable_time, */
};

/* Regulator vsram_proc1 ops */
static struct regulator_ops pmic_ldo_vsram_proc1_ops = {
	.enable = pmic_ldo_vsram_proc1_enable,
	.disable = pmic_ldo_vsram_proc1_disable,
	.is_enabled = pmic_ldo_vsram_proc1_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc1_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc1_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_proc1_enable_time, */
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



/*------Regulator ATTR------*/
static ssize_t show_regulator_status(
					struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct mtk_regulator *mreg = NULL;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, en_att);

	if (mreg->da_en_cb != NULL)
		ret_value = (mreg->da_en_cb)();
	else
		ret_value = 9999;

	PMICLOG("[EM] %s_STATUS : %d\n"
		 , mreg->desc.name
		 , ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_regulator_status(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}

static ssize_t show_regulator_voltage(
					struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct mtk_regulator *mreg = NULL;
	const int *pVoltage = NULL;
	const int *pVoltidx = NULL;

	unsigned short regVal = 0;
	unsigned int ret_value = 0, i = 0, ret = 0;

	mreg = container_of(attr, struct mtk_regulator, voltage_att);

	if (mreg->desc.n_voltages != 1) {
		if (mreg->da_vol_cb != NULL) {
			regVal = (mreg->da_vol_cb)();
			if (mreg->pvoltages != NULL) {
				pVoltage = (const int *)mreg->pvoltages;
				pVoltidx = (const int *)mreg->idxs;
				for (i = 0; i < mreg->desc.n_voltages; i++) {
					if (pVoltidx[i] == regVal) {
						ret = i;
						break;
					}
				}
				ret_value = pVoltage[ret];
			} else
				ret_value = mreg->desc.min_uV +
							mreg->desc.uV_step *
							regVal;
		} else
			pr_notice("[EM] %s_VOLTAGE have no da_vol_cb\n",
							mreg->desc.name);
	} else {
		if (mreg->pvoltages != NULL) {
			pVoltage = (const int *)mreg->pvoltages;
			ret_value = pVoltage[0];
		} else if (mreg->desc.fixed_uV)
			ret_value = mreg->desc.fixed_uV;
		else
			pr_notice("[EM] %s_VOLTAGE have no pVolatges\n",
							mreg->desc.name);
	}

	ret_value = ret_value / 1000;

	PMICLOG("[EM] %s_VOLTAGE : %d\n"
		 , mreg->desc.name
		 , ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_regulator_voltage(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t size)
{
	PMICLOG("[EM] Not Support Write Function\n");
	return size;
}


/* Regulator: BUCK */
#define BUCK_EN	REGULATOR_CHANGE_STATUS
#define BUCK_VOL REGULATOR_CHANGE_VOLTAGE
#define BUCK_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
#define BUCK_VOL_EN_MODE \
	(REGULATOR_CHANGE_STATUS | \
	REGULATOR_CHANGE_VOLTAGE | \
	REGULATOR_CHANGE_MODE)
struct mtk_regulator mt_bucks[] = {
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vs1, buck,
			800000, 2200000, 12500, 0,
			BUCK_VOL_EN, PMIC_RG_VS1_FPWM,
			PMIC_RG_BUCK_VS1_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vgpu11, buck,
			400000, 1193750, 6250, 0,
			BUCK_VOL_EN_MODE, PMIC_RG_VGPU11_FCCM,
			PMIC_RG_BUCK_VGPU11_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vmodem, buck,
			400000, 1100000, 6250, 0,
			BUCK_VOL_EN, PMIC_RG_VMODEM_FCCM,
			PMIC_RG_BUCK_VMODEM_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vpu, buck,
			400000, 1193750, 6250, 0,
			BUCK_VOL_EN_MODE, PMIC_RG_VPU_FCCM,
			PMIC_RG_BUCK_VPU_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vcore, buck,
			400000, 1193750, 6250, 0,
			BUCK_VOL_EN_MODE, PMIC_RG_VCORE_FCCM,
			PMIC_RG_BUCK_VCORE_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vs2, buck,
			800000, 1600000, 12500, 0,
			BUCK_VOL_EN, PMIC_RG_VS2_FPWM,
			PMIC_RG_BUCK_VS2_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vpa, buck,
			500000, 3650000, 50000, 0,
			BUCK_VOL_EN, PMIC_RG_VPA_MODESET,
			PMIC_RG_BUCK_VPA_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vproc2, buck,
			400000, 1193750, 6250, 0,
			BUCK_VOL_EN_MODE, PMIC_RG_VPROC2_FCCM,
			PMIC_RG_BUCK_VPROC2_LP, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
			vproc1, buck,
			400000, 1193750, 6250, 0,
			BUCK_VOL_EN_MODE, PMIC_RG_VPROC1_FCCM,
			PMIC_RG_BUCK_VPROC1_LP, 1
	),
};


/* Regulator: LDO */
#define LDO_EN	REGULATOR_CHANGE_STATUS
#define LDO_VOL REGULATOR_CHANGE_VOLTAGE
#define LDO_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mt_ldos[] = {
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaud18, ldo,
			1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim1, ldo,
			vsim1_voltages, vsim1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vibr, ldo,
			vibr_voltages, vibr_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vrf12, ldo,
			vrf12_voltages, vrf12_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vusb, ldo,
			3000000, LDO_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc2, ldo,
			500000, 1193750, 6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vio18, ldo,
			vio18_voltages, vio18_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamio, ldo,
			vcamio_voltages, vcamio_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn18, ldo,
			1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vfe28, ldo,
			2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn13, ldo,
			vcn13_voltages, vcn13_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_1_bt, ldo,
			vcn33_1_bt_voltages, vcn33_1_bt_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_1_wifi, ldo,
			vcn33_1_wifi_voltages, vcn33_1_wifi_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaux18, ldo,
			1800000, LDO_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_others, ldo,
			500000, 1193750, 6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vefuse, ldo,
			vefuse_voltages, vefuse_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vxo22, ldo,
			vxo22_voltages, vxo22_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vrfck, ldo,
			vrfck_voltages, vrfck_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vbif28, ldo,
			2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vio28, ldo,
			vio28_voltages, vio28_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vemc, ldo,
			vemc_voltages, vemc_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_2_bt, ldo,
			vcn33_2_bt_voltages, vcn33_2_bt_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_2_wifi, ldo,
			vcn33_2_wifi_voltages, vcn33_2_wifi_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(va12, ldo,
			va12_voltages, va12_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(va09, ldo,
			va09_voltages, va09_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18, ldo,
			vrf18_voltages, vrf18_idx, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_md, ldo,
			500000, 1100000, 6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vufs, ldo,
			vufs_voltages, vufs_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vm18, ldo,
			vm18_voltages, vm18_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vbbck, ldo,
			vbbck_voltages, vbbck_idx, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc1, ldo,
			500000, 1193750, 6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim2, ldo,
			vsim2_voltages, vsim2_idx, LDO_VOL_EN, 1),

};



int mt_ldos_size = ARRAY_SIZE(mt_ldos);
int mt_bucks_size = ARRAY_SIZE(mt_bucks);
/* -------Code Gen End-------*/

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY

#define PMIC_REGULATOR_LDO_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_ldos[MT6359_POWER_LDO_##_id],	\
	}

#define PMIC_REGULATOR_BUCK_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_bucks[MT6359_POWER_BUCK_##_id],	\
	}




struct of_regulator_match pmic_regulator_buck_matches[] = {
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs1, VS1),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vgpu11, VGPU11),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vmodem, VMODEM),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vpu, VPU),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vcore, VCORE),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs2, VS2),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vpa, VPA),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc2, VPROC2),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc1, VPROC1),

};

struct of_regulator_match pmic_regulator_ldo_matches[] = {
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaud18, VAUD18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim1, VSIM1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vibr, VIBR),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf12, VRF12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vusb, VUSB),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc2, VSRAM_PROC2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio18, VIO18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamio, VCAMIO),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn18, VCN18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vfe28, VFE28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn13, VCN13),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_1_bt, VCN33_1_BT),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_1_wifi, VCN33_1_WIFI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaux18, VAUX18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_others, VSRAM_OTHERS),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vefuse, VEFUSE),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vxo22, VXO22),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrfck, VRFCK),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vbif28, VBIF28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio28, VIO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vemc, VEMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_2_bt, VCN33_2_BT),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_2_wifi, VCN33_2_WIFI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va12, VA12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va09, VA09),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf18, VRF18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_md, VSRAM_MD),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vufs, VUFS),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vm18, VM18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vbbck, VBBCK),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc1, VSRAM_PROC1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim2, VSIM2),

};

int pmic_regulator_ldo_matches_size = ARRAY_SIZE(pmic_regulator_ldo_matches);
int pmic_regulator_buck_matches_size = ARRAY_SIZE(pmic_regulator_buck_matches);

#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
