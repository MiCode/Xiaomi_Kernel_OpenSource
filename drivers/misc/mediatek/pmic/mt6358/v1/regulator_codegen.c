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

#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"

static const int vdram2_voltages[] = {
	600000,
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
	2800000,
	3000000,
	3300000,
};

static const int vrf12_voltages[] = {
	1200000,
};

static const int vio18_voltages[] = {
	1800000,
};

static const int vusb_voltages[] = {
	3000000,
	3100000,
};

static const int vcamio_voltages[] = {
	1800000,
};

static const int vcamd_voltages[] = {
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
	1500000,
	1800000,
};

static const int vcn18_voltages[] = {
	1800000,
};

static const int vfe28_voltages[] = {
	2800000,
};

static const int vcn28_voltages[] = {
	2800000,
};

static const int vxo22_voltages[] = {
	2200000,
};

static const int vefuse_voltages[] = {
	1700000,
	1800000,
	1900000,
};

static const int vaux18_voltages[] = {
	1800000,
};

static const int vmch_voltages[] = {
	2900000,
	3000000,
	3300000,
};

static const int vbif28_voltages[] = {
	2800000,
};

static const int vcama1_voltages[] = {
	1800000,
	2500000,
	2700000,
	2800000,
	2900000,
	3000000,
};

static const int vemc_voltages[] = {
	2900000,
	3000000,
	3300000,
};

static const int vio28_voltages[] = {
	2800000,
};

static const int va12_voltages[] = {
	1200000,
};

static const int vrf18_voltages[] = {
	1800000,
};

static const int vcn33_bt_voltages[] = {
	3300000,
	3400000,
	3500000,
};

static const int vcn33_wifi_voltages[] = {
	3300000,
	3400000,
	3500000,
};

static const int vcama2_voltages[] = {
	1800000,
	2500000,
	2700000,
	2800000,
	2900000,
	3000000,
};

static const int vmc_voltages[] = {
	1800000,
	2900000,
	3000000,
	3300000,
};

static const int vldo28_voltages[] = {
	2800000,
	3000000,
};

static const int vaud28_voltages[] = {
	2800000,
};

static const int vsim2_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};



static const int vdram2_idx[] = {
	0, 12,
};

static const int vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const int vibr_idx[] = {
	0, 1, 2, 4, 5, 9, 11, 13,
};

static const int vusb_idx[] = {
	3, 4,
};

static const int vcamd_idx[] = {
	3, 4, 5, 6, 7, 9, 12,
};

static const int vefuse_idx[] = {
	11, 12, 13,
};

static const int vmch_idx[] = {
	2, 3, 5,
};

static const int vcama1_idx[] = {
	0, 7, 9, 10, 11, 12,
};

static const int vemc_idx[] = {
	2, 3, 5,
};

static const int vcn33_bt_idx[] = {
	1, 2, 3,
};

static const int vcn33_wifi_idx[] = {
	1, 2, 3,
};

static const int vcama2_idx[] = {
	0, 7, 9, 10, 11, 12,
};

static const int vmc_idx[] = {
	4, 10, 11, 13,
};

static const int vldo28_idx[] = {
	1, 3,
};

static const int vsim2_idx[] = {
	3, 4, 8, 11, 12,
};



/* Regulator vdram2 enable */
static int pmic_ldo_vdram2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vdram2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vdram2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram2 disable */
static int pmic_ldo_vdram2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vdram2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vdram2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vdram2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vdram2 is_enabled */
static int pmic_ldo_vdram2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vdram2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vdram2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vdram2 set_voltage_sel */
static int pmic_ldo_vdram2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vdram2_idx[selector];

	PMICLOG("ldo vdram2 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vdram2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram2 get_voltage_sel */
static int pmic_ldo_vdram2_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vdram2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vdram2 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vdram2_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vdram2 list_voltage */
static int pmic_ldo_vdram2_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vdram2_voltages[selector];
	PMICLOG("ldo vdram2 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vdram1 enable */
static int pmic_buck_vdram1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vdram1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram1 disable */
static int pmic_buck_vdram1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vdram1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vdram1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vdram1 is_enabled */
static int pmic_buck_vdram1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vdram1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vdram1 set_voltage_sel */
static int pmic_buck_vdram1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vdram1 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vdram1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram1 get_voltage_sel */
static int pmic_buck_vdram1_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vdram1 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
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

/* Regulator vusb set_voltage_sel */
static int pmic_ldo_vusb_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vusb_idx[selector];

	PMICLOG("ldo vusb set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vusb don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vusb get_voltage_sel */
static int pmic_ldo_vusb_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vusb get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vusb don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vusb_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vusb list_voltage */
static int pmic_ldo_vusb_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vusb_voltages[selector];
	PMICLOG("ldo vusb list_voltage: %d\n"
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

/* Regulator vcamd enable */
static int pmic_ldo_vcamd_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcamd don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd disable */
static int pmic_ldo_vcamd_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcamd should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcamd don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcamd is_enabled */
static int pmic_ldo_vcamd_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcamd don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamd set_voltage_sel */
static int pmic_ldo_vcamd_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcamd_idx[selector];

	PMICLOG("ldo vcamd set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcamd don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd get_voltage_sel */
static int pmic_ldo_vcamd_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcamd don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcamd_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcamd list_voltage */
static int pmic_ldo_vcamd_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcamd_voltages[selector];
	PMICLOG("ldo vcamd list_voltage: %d\n"
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

/* Regulator vsram_proc11 enable */
static int pmic_ldo_vsram_proc11_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc11 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_proc11 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc11 disable */
static int pmic_ldo_vsram_proc11_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc11 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_proc11 should not be disable\n");
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_proc11 don't have enable callback\n"
				  );
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_proc11 is_enabled */
static int pmic_ldo_vsram_proc11_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc11 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_proc11 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc11 set_voltage_sel */
static int pmic_ldo_vsram_proc11_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_proc11 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_proc11 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc11 get_voltage_sel */
static int pmic_ldo_vsram_proc11_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc11 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_proc11 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vcn28 enable */
static int pmic_ldo_vcn28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn28 disable */
static int pmic_ldo_vcn28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn28 is_enabled */
static int pmic_ldo_vcn28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc11 enable */
static int pmic_buck_vproc11_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc11 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vproc11 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc11 disable */
static int pmic_buck_vproc11_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc11 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vproc11 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vproc11 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vproc11 is_enabled */
static int pmic_buck_vproc11_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc11 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vproc11 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc11 set_voltage_sel */
static int pmic_buck_vproc11_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vproc11 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vproc11 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc11 get_voltage_sel */
static int pmic_buck_vproc11_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc11 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vproc11 don't have da_vol_cb\n");
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
		PMICLOG("ldo vsram_others should not be disable)\n");
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_others don't have enable callback\n"
				  );
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

/* Regulator vsram_gpu enable */
static int pmic_ldo_vsram_gpu_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_gpu enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_gpu don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_gpu disable */
static int pmic_ldo_vsram_gpu_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_gpu disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_gpu should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_gpu don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_gpu is_enabled */
static int pmic_ldo_vsram_gpu_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_gpu is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_gpu don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_gpu set_voltage_sel */
static int pmic_ldo_vsram_gpu_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_gpu set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_gpu don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_gpu get_voltage_sel */
static int pmic_ldo_vsram_gpu_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_gpu get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_gpu don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vproc12 enable */
static int pmic_buck_vproc12_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc12 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vproc12 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc12 disable */
static int pmic_buck_vproc12_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc12 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vproc12 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vproc12 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vproc12 is_enabled */
static int pmic_buck_vproc12_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc12 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vproc12 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc12 set_voltage_sel */
static int pmic_buck_vproc12_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vproc12 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vproc12 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc12 get_voltage_sel */
static int pmic_buck_vproc12_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vproc12 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vproc12 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
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

/* Regulator vmch enable */
static int pmic_ldo_vmch_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmch enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vmch don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmch disable */
static int pmic_ldo_vmch_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmch disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vmch should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vmch don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vmch is_enabled */
static int pmic_ldo_vmch_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmch is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vmch don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmch set_voltage_sel */
static int pmic_ldo_vmch_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vmch_idx[selector];

	PMICLOG("ldo vmch set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vmch don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmch get_voltage_sel */
static int pmic_ldo_vmch_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmch get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vmch don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vmch_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vmch list_voltage */
static int pmic_ldo_vmch_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vmch_voltages[selector];
	PMICLOG("ldo vmch list_voltage: %d\n"
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

/* Regulator vgpu enable */
static int pmic_buck_vgpu_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vgpu don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgpu disable */
static int pmic_buck_vgpu_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vgpu should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vgpu don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vgpu is_enabled */
static int pmic_buck_vgpu_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vgpu don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vgpu set_voltage_sel */
static int pmic_buck_vgpu_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vgpu set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vgpu don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgpu get_voltage_sel */
static int pmic_buck_vgpu_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vgpu get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vgpu don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vsram_proc12 enable */
static int pmic_ldo_vsram_proc12_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc12 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_proc12 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc12 disable */
static int pmic_ldo_vsram_proc12_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc12 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_proc12 should not be disable\n");
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_proc12 don't have enable callback\n"
				  );
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_proc12 is_enabled */
static int pmic_ldo_vsram_proc12_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc12 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_proc12 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc12 set_voltage_sel */
static int pmic_ldo_vsram_proc12_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_proc12 set_voltage_sel: %d\n"
			, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_proc12 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc12 get_voltage_sel */
static int pmic_ldo_vsram_proc12_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc12 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_proc12 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vcama1 enable */
static int pmic_ldo_vcama1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcama1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama1 disable */
static int pmic_ldo_vcama1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcama1 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcama1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcama1 is_enabled */
static int pmic_ldo_vcama1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcama1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcama1 set_voltage_sel */
static int pmic_ldo_vcama1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcama1_idx[selector];

	PMICLOG("ldo vcama1 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcama1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama1 get_voltage_sel */
static int pmic_ldo_vcama1_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcama1 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcama1_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcama1 list_voltage */
static int pmic_ldo_vcama1_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcama1_voltages[selector];
	PMICLOG("ldo vcama1 list_voltage: %d\n"
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

/* Regulator vcn33_bt enable */
static int pmic_ldo_vcn33_bt_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_bt enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_bt don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_bt disable */
static int pmic_ldo_vcn33_bt_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_bt disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn33_bt should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn33_bt don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_bt is_enabled */
static int pmic_ldo_vcn33_bt_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_bt is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_bt don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_bt set_voltage_sel */
static int pmic_ldo_vcn33_bt_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_bt_idx[selector];

	PMICLOG("ldo vcn33_bt set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_bt don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_bt get_voltage_sel */
static int pmic_ldo_vcn33_bt_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_bt get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_bt don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_bt_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_bt list_voltage */
static int pmic_ldo_vcn33_bt_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_bt_voltages[selector];
	PMICLOG("ldo vcn33_bt list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vcn33_wifi enable */
static int pmic_ldo_vcn33_wifi_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_wifi enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcn33_wifi don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_wifi disable */
static int pmic_ldo_vcn33_wifi_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_wifi disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcn33_wifi should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcn33_wifi don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcn33_wifi is_enabled */
static int pmic_ldo_vcn33_wifi_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_wifi is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcn33_wifi don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_wifi set_voltage_sel */
static int pmic_ldo_vcn33_wifi_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_wifi_idx[selector];

	PMICLOG("ldo vcn33_wifi set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcn33_wifi don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_wifi get_voltage_sel */
static int pmic_ldo_vcn33_wifi_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcn33_wifi get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcn33_wifi don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcn33_wifi_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcn33_wifi list_voltage */
static int pmic_ldo_vcn33_wifi_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcn33_wifi_voltages[selector];
	PMICLOG("ldo vcn33_wifi list_voltage: %d\n"
		 , voltage);
	return voltage;
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

/* Regulator vcama2 enable */
static int pmic_ldo_vcama2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vcama2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama2 disable */
static int pmic_ldo_vcama2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcama2 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vcama2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcama2 is_enabled */
static int pmic_ldo_vcama2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcama2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcama2 set_voltage_sel */
static int pmic_ldo_vcama2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcama2_idx[selector];

	PMICLOG("ldo vcama2 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcama2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama2 get_voltage_sel */
static int pmic_ldo_vcama2_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcama2 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcama2_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcama2 list_voltage */
static int pmic_ldo_vcama2_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcama2_voltages[selector];
	PMICLOG("ldo vcama2 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vmc enable */
static int pmic_ldo_vmc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vmc don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmc disable */
static int pmic_ldo_vmc_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmc disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vmc should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vmc don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vmc is_enabled */
static int pmic_ldo_vmc_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vmc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmc set_voltage_sel */
static int pmic_ldo_vmc_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vmc_idx[selector];

	PMICLOG("ldo vmc set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vmc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmc get_voltage_sel */
static int pmic_ldo_vmc_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vmc don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vmc_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vmc list_voltage */
static int pmic_ldo_vmc_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vmc_voltages[selector];
	PMICLOG("ldo vmc list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vldo28 enable */
static int pmic_ldo_vldo28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vldo28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vldo28 disable */
static int pmic_ldo_vldo28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vldo28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vldo28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vldo28 is_enabled */
static int pmic_ldo_vldo28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vldo28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vldo28 set_voltage_sel */
static int pmic_ldo_vldo28_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vldo28_idx[selector];

	PMICLOG("ldo vldo28 set_voltage_sel: %d\n"
				, selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vldo28 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vldo28 get_voltage_sel */
static int pmic_ldo_vldo28_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vldo28 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vldo28_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vldo28 list_voltage */
static int pmic_ldo_vldo28_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vldo28_voltages[selector];
	PMICLOG("ldo vldo28 list_voltage: %d\n"
		 , voltage);
	return voltage;
}

/* Regulator vaud28 enable */
static int pmic_ldo_vaud28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vaud28 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vaud28 disable */
static int pmic_ldo_vaud28_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud28 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vaud28 should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vaud28 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vaud28 is_enabled */
static int pmic_ldo_vaud28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vaud28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vaud28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
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

/* Regulator EXT_PMIC2 enable */
static int pmic_regulator_ext2_enable(struct regulator_dev *rdev)
{
	unsigned short ext_en = 0, ext_sel = 0;
	int ret = 0;

	pr_info("regulator ext_pmic2 enable\n");
	ext_en = pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
	if ((ext_en & 0x2) != 0x2)
		ret |= pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_EN,
			ext_en | 0x2);
	ext_sel = pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL);
	if ((ext_sel & 0x2) != 0x2)
		ret |= pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL,
			ext_sel | 0x2);

	return ret;
}

/* Regulator EXT_PMIC2 disable */
static int pmic_regulator_ext2_disable(struct regulator_dev *rdev)
{
	unsigned short ext_en = 0, ext_sel = 0;
	int ret = 0;

	pr_info("regulator ext_pmic2 disable\n");
	if (rdev->use_count == 0) {
		pr_notice("regulator ext_pmic2 should not be disable\n");
		return -1;
	}
	ext_sel = pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL);
	if ((ext_sel & 0x2) != 0x2)
		ret |= pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_SEL,
			ext_sel | 0x2);
	ext_en = pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
	if ((ext_en & 0x2) == 0x2)
		ret |= pmic_set_register_value(PMIC_RG_STRUP_EXT_PMIC_EN,
			ext_en & ~0x2);

	return ret;
}

/* Regulator EXT_PMIC2 is_enabled */
static int pmic_regulator_ext2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("regulator ext_pmic2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("regulator ext_pmic2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

static int pmic_buck_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mtk_regulator *mreg = rdev_get_drvdata(rdev);
	unsigned short val;
	int ret = 0;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = 1;
		break;
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	default:
		return -EINVAL;
	}
	pmic_set_register_value(mreg->modeset_reg, val);
	if (val == pmic_get_register_value(mreg->modeset_reg))
		pr_info("BUCK %s set FPWM mode:%d pass\n"
			, mreg->desc.name, val);
	else {
		pr_notice("BUCK %s set FPWM mode:%d fail\n"
			  , mreg->desc.name, val);
		ret = -1;
	}
	return ret;
}

static unsigned int pmic_buck_get_mode(struct regulator_dev *rdev)
{
	struct mtk_regulator *mreg = rdev_get_drvdata(rdev);
	unsigned int mode;

	if (pmic_get_register_value(mreg->modeset_reg) == 1)
		mode = REGULATOR_MODE_FAST;
	else
		mode = REGULATOR_MODE_NORMAL;
	return mode;
}

/* Regulator vdram2 ops */
static struct regulator_ops pmic_ldo_vdram2_ops = {
	.enable = pmic_ldo_vdram2_enable,
	.disable = pmic_ldo_vdram2_disable,
	.is_enabled = pmic_ldo_vdram2_is_enabled,
	.get_voltage_sel = pmic_ldo_vdram2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vdram2_set_voltage_sel,
	.list_voltage = pmic_ldo_vdram2_list_voltage,
	/* .enable_time = pmic_ldo_vdram2_enable_time, */
};

/* Regulator vdram1 ops */
static struct regulator_ops pmic_buck_vdram1_ops = {
	.enable = pmic_buck_vdram1_enable,
	.disable = pmic_buck_vdram1_disable,
	.is_enabled = pmic_buck_vdram1_is_enabled,
	.get_voltage_sel = pmic_buck_vdram1_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vdram1_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vdram1_enable_time, */
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
	/* .enable_time = pmic_ldo_vrf12_enable_time, */
};

/* Regulator vio18 ops */
static struct regulator_ops pmic_ldo_vio18_ops = {
	.enable = pmic_ldo_vio18_enable,
	.disable = pmic_ldo_vio18_disable,
	.is_enabled = pmic_ldo_vio18_is_enabled,
	/* .enable_time = pmic_ldo_vio18_enable_time, */
};

/* Regulator vusb ops */
static struct regulator_ops pmic_ldo_vusb_ops = {
	.enable = pmic_ldo_vusb_enable,
	.disable = pmic_ldo_vusb_disable,
	.is_enabled = pmic_ldo_vusb_is_enabled,
	.get_voltage_sel = pmic_ldo_vusb_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vusb_set_voltage_sel,
	.list_voltage = pmic_ldo_vusb_list_voltage,
	/* .enable_time = pmic_ldo_vusb_enable_time, */
};

/* Regulator vcamio ops */
static struct regulator_ops pmic_ldo_vcamio_ops = {
	.enable = pmic_ldo_vcamio_enable,
	.disable = pmic_ldo_vcamio_disable,
	.is_enabled = pmic_ldo_vcamio_is_enabled,
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

/* Regulator vsram_proc11 ops */
static struct regulator_ops pmic_ldo_vsram_proc11_ops = {
	.enable = pmic_ldo_vsram_proc11_enable,
	.disable = pmic_ldo_vsram_proc11_disable,
	.is_enabled = pmic_ldo_vsram_proc11_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc11_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc11_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_proc11_enable_time, */
};

/* Regulator vcn28 ops */
static struct regulator_ops pmic_ldo_vcn28_ops = {
	.enable = pmic_ldo_vcn28_enable,
	.disable = pmic_ldo_vcn28_disable,
	.is_enabled = pmic_ldo_vcn28_is_enabled,
	/* .enable_time = pmic_ldo_vcn28_enable_time, */
};

/* Regulator vproc11 ops */
static struct regulator_ops pmic_buck_vproc11_ops = {
	.enable = pmic_buck_vproc11_enable,
	.disable = pmic_buck_vproc11_disable,
	.is_enabled = pmic_buck_vproc11_is_enabled,
	.get_voltage_sel = pmic_buck_vproc11_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc11_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vproc11_enable_time, */
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

/* Regulator vsram_gpu ops */
static struct regulator_ops pmic_ldo_vsram_gpu_ops = {
	.enable = pmic_ldo_vsram_gpu_enable,
	.disable = pmic_ldo_vsram_gpu_disable,
	.is_enabled = pmic_ldo_vsram_gpu_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_gpu_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_gpu_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_gpu_enable_time, */
};

/* Regulator vproc12 ops */
static struct regulator_ops pmic_buck_vproc12_ops = {
	.enable = pmic_buck_vproc12_enable,
	.disable = pmic_buck_vproc12_disable,
	.is_enabled = pmic_buck_vproc12_is_enabled,
	.get_voltage_sel = pmic_buck_vproc12_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc12_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vproc12_enable_time, */
};

/* Regulator vxo22 ops */
static struct regulator_ops pmic_ldo_vxo22_ops = {
	.enable = pmic_ldo_vxo22_enable,
	.disable = pmic_ldo_vxo22_disable,
	.is_enabled = pmic_ldo_vxo22_is_enabled,
	/* .enable_time = pmic_ldo_vxo22_enable_time, */
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

/* Regulator vaux18 ops */
static struct regulator_ops pmic_ldo_vaux18_ops = {
	.enable = pmic_ldo_vaux18_enable,
	.disable = pmic_ldo_vaux18_disable,
	.is_enabled = pmic_ldo_vaux18_is_enabled,
	/* .enable_time = pmic_ldo_vaux18_enable_time, */
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

/* Regulator vbif28 ops */
static struct regulator_ops pmic_ldo_vbif28_ops = {
	.enable = pmic_ldo_vbif28_enable,
	.disable = pmic_ldo_vbif28_disable,
	.is_enabled = pmic_ldo_vbif28_is_enabled,
	/* .enable_time = pmic_ldo_vbif28_enable_time, */
};

/* Regulator vgpu ops */
static struct regulator_ops pmic_buck_vgpu_ops = {
	.enable = pmic_buck_vgpu_enable,
	.disable = pmic_buck_vgpu_disable,
	.is_enabled = pmic_buck_vgpu_is_enabled,
	.get_voltage_sel = pmic_buck_vgpu_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vgpu_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vgpu_enable_time, */
};

/* Regulator vsram_proc12 ops */
static struct regulator_ops pmic_ldo_vsram_proc12_ops = {
	.enable = pmic_ldo_vsram_proc12_enable,
	.disable = pmic_ldo_vsram_proc12_disable,
	.is_enabled = pmic_ldo_vsram_proc12_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc12_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc12_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_proc12_enable_time, */
};

/* Regulator vcama1 ops */
static struct regulator_ops pmic_ldo_vcama1_ops = {
	.enable = pmic_ldo_vcama1_enable,
	.disable = pmic_ldo_vcama1_disable,
	.is_enabled = pmic_ldo_vcama1_is_enabled,
	.get_voltage_sel = pmic_ldo_vcama1_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcama1_set_voltage_sel,
	.list_voltage = pmic_ldo_vcama1_list_voltage,
	/* .enable_time = pmic_ldo_vcama1_enable_time, */
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

/* Regulator vio28 ops */
static struct regulator_ops pmic_ldo_vio28_ops = {
	.enable = pmic_ldo_vio28_enable,
	.disable = pmic_ldo_vio28_disable,
	.is_enabled = pmic_ldo_vio28_is_enabled,
	/* .enable_time = pmic_ldo_vio28_enable_time, */
};

/* Regulator va12 ops */
static struct regulator_ops pmic_ldo_va12_ops = {
	.enable = pmic_ldo_va12_enable,
	.disable = pmic_ldo_va12_disable,
	.is_enabled = pmic_ldo_va12_is_enabled,
	/* .enable_time = pmic_ldo_va12_enable_time, */
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

/* Regulator vrf18 ops */
static struct regulator_ops pmic_ldo_vrf18_ops = {
	.enable = pmic_ldo_vrf18_enable,
	.disable = pmic_ldo_vrf18_disable,
	.is_enabled = pmic_ldo_vrf18_is_enabled,
	/* .enable_time = pmic_ldo_vrf18_enable_time, */
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

/* Regulator vcama2 ops */
static struct regulator_ops pmic_ldo_vcama2_ops = {
	.enable = pmic_ldo_vcama2_enable,
	.disable = pmic_ldo_vcama2_disable,
	.is_enabled = pmic_ldo_vcama2_is_enabled,
	.get_voltage_sel = pmic_ldo_vcama2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcama2_set_voltage_sel,
	.list_voltage = pmic_ldo_vcama2_list_voltage,
	/* .enable_time = pmic_ldo_vcama2_enable_time, */
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

/* Regulator vldo28 ops */
static struct regulator_ops pmic_ldo_vldo28_ops = {
	.enable = pmic_ldo_vldo28_enable,
	.disable = pmic_ldo_vldo28_disable,
	.is_enabled = pmic_ldo_vldo28_is_enabled,
	.get_voltage_sel = pmic_ldo_vldo28_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vldo28_set_voltage_sel,
	.list_voltage = pmic_ldo_vldo28_list_voltage,
	/* .enable_time = pmic_ldo_vldo28_enable_time, */
};

/* Regulator vaud28 ops */
static struct regulator_ops pmic_ldo_vaud28_ops = {
	.enable = pmic_ldo_vaud28_enable,
	.disable = pmic_ldo_vaud28_disable,
	.is_enabled = pmic_ldo_vaud28_is_enabled,
	/* .enable_time = pmic_ldo_vaud28_enable_time, */
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

/* Regulator EXT_PMIC2 ops */
static struct regulator_ops pmic_regulator_ext2_ops = {
	.enable = pmic_regulator_ext2_enable,
	.disable = pmic_regulator_ext2_disable,
	.is_enabled = pmic_regulator_ext2_is_enabled,
};

/*------Regulator ATTR------*/
static ssize_t show_regulator_status(
					struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct mtk_regulator *mreg;
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
	struct mtk_regulator *mreg;
	const int *pVoltage;
	const int *pVoltidx;

	unsigned short regVal;
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
#define BUCK_EN REGULATOR_CHANGE_STATUS
#define BUCK_VOL REGULATOR_CHANGE_VOLTAGE
#define BUCK_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
#define BUCK_VOL_EN_MODE \
	(REGULATOR_CHANGE_STATUS | \
	REGULATOR_CHANGE_VOLTAGE | \
	REGULATOR_CHANGE_MODE)
struct mtk_regulator mt_bucks[] = {
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vdram1, buck,
		500000, 2087500, 12500, 0,
		BUCK_VOL_EN, PMIC_RG_VDRAM1_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vcore, buck,
		500000, 1293750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VCORE_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vpa, buck,
		500000, 3650000, 50000, 0,
		BUCK_VOL_EN, PMIC_RG_VPA_MODESET, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vproc11, buck,
		500000, 1293750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VPROC11_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vproc12, buck,
		500000, 1293750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VPROC12_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vgpu, buck,
		500000, 1293750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VGPU_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vs2, buck,
		500000, 2087500, 12500, 0,
		BUCK_VOL_EN, PMIC_RG_VS2_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vmodem, buck,
		500000, 1293750, 6250, 0,
		BUCK_VOL_EN, PMIC_RG_VMODEM_FPWM, 1
	),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vs1, buck,
		1000000, 2587500, 12500, 0,
		BUCK_VOL_EN, PMIC_RG_VS1_FPWM, 1
	),
};


/* Regulator: LDO */
#define LDO_EN	REGULATOR_CHANGE_STATUS
#define LDO_VOL REGULATOR_CHANGE_VOLTAGE
#define LDO_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mt_ldos[] = {
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vdram2, ldo,
			vdram2_voltages, vdram2_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim1, ldo,
			vsim1_voltages, vsim1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vibr, ldo,
			vibr_voltages, vibr_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf12, ldo,
			1200000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio18, ldo,
			1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vusb, ldo,
			vusb_voltages, vusb_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcamio, ldo,
			1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamd, ldo,
			vcamd_voltages, vcamd_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn18, ldo,
			1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vfe28, ldo,
			2800000, LDO_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc11, ldo,
			500000, 1293750, 6250, 0, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn28, ldo,
			2800000, LDO_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_others, ldo,
			500000, 1293750, 6250, 0, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_gpu, ldo,
			500000, 1293750, 6250, 0, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vxo22, ldo,
			2200000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vefuse, ldo,
			vefuse_voltages, vefuse_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaux18, ldo,
			1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmch, ldo,
			vmch_voltages, vmch_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vbif28, ldo,
			2800000, LDO_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc12, ldo,
			500000, 1293750, 6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama1, ldo,
			vcama1_voltages, vcama1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vemc, ldo,
			vemc_voltages, vemc_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio28, ldo,
			2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(va12, ldo,
			1200000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18, ldo,
			1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_bt, ldo,
			vcn33_bt_voltages, vcn33_bt_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_wifi, ldo,
			vcn33_wifi_voltages, vcn33_wifi_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama2, ldo,
			vcama2_voltages, vcama2_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmc, ldo,
			vmc_voltages, vmc_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28, ldo,
			vldo28_voltages, vldo28_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaud28, ldo,
			2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim2, ldo,
			vsim2_voltages, vsim2_idx, LDO_VOL_EN, 1),
	{
		.desc = {
			.name = "va09",
			.n_voltages = 1,
			.ops = &pmic_regulator_ext2_ops,
			.type = REGULATOR_VOLTAGE,
			.fixed_uV = 900000,
		},
		.constraints = {
			.valid_ops_mask = LDO_EN,
		},
		.en_att = __ATTR(ldo_va09_status, 0664,
			show_regulator_status, store_regulator_status),
		.voltage_att = __ATTR(ldo_va09_voltage, 0664,
			show_regulator_voltage, store_regulator_voltage),
		.da_en_cb = mt6358_upmu_get_da_ext_pmic_en2,
		.isUsedable = 1,
	},
};



int mt_ldos_size = ARRAY_SIZE(mt_ldos);
int mt_bucks_size = ARRAY_SIZE(mt_bucks);
/* -------Code Gen End-------*/

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY

#define PMIC_REGULATOR_LDO_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_ldos[MT6358_POWER_LDO_##_id],	\
	}

#define PMIC_REGULATOR_BUCK_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_bucks[MT6358_POWER_BUCK_##_id],	\
	}




struct of_regulator_match pmic_regulator_buck_matches[] = {
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vdram1, VDRAM1),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vcore, VCORE),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vpa, VPA),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc11, VPROC11),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc12, VPROC12),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vgpu, VGPU),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs2, VS2),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vmodem, VMODEM),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs1, VS1),

};

struct of_regulator_match pmic_regulator_ldo_matches[] = {
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vdram2, VDRAM2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim1, VSIM1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vibr, VIBR),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf12, VRF12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio18, VIO18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vusb, VUSB),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamio, VCAMIO),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamd, VCAMD),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn18, VCN18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vfe28, VFE28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc11, VSRAM_PROC11),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn28, VCN28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_others, VSRAM_OTHERS),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_gpu, VSRAM_GPU),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vxo22, VXO22),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vefuse, VEFUSE),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaux18, VAUX18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmch, VMCH),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vbif28, VBIF28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc12, VSRAM_PROC12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcama1, VCAMA1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vemc, VEMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio28, VIO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va12, VA12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf18, VRF18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_bt, VCN33_BT),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_wifi, VCN33_WIFI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcama2, VCAMA2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmc, VMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vldo28, VLDO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaud28, VAUD28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim2, VSIM2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va09, VA09),
};

int pmic_regulator_ldo_matches_size = ARRAY_SIZE(pmic_regulator_ldo_matches);
int pmic_regulator_buck_matches_size = ARRAY_SIZE(pmic_regulator_buck_matches);

#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
