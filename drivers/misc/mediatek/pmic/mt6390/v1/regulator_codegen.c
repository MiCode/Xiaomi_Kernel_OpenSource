/*
 * Copyright (C) 2020 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/delay.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_ldo.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"

static const int vfe28_voltages[] = {
	2800000,
};

static const int vxo22_voltages[] = {
	2200000,
	2400000,
};

static const int vrf18_voltages[] = {
	1800000,
};

static const int vrf12_voltages[] = {
	1200000,
};

static const int vefuse_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2800000,
	2900000,
	3000000,
	3300000,
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

static const int vcn28_voltages[] = {
	2800000,
};

static const int vcn18_voltages[] = {
	1800000,
};

static const int vcama_voltages[] = {
	2500000,
	2800000,
};

static const int vcamd_voltages[] = {
	1000000,
	1100000,
	1200000,
	1300000,
	1500000,
	1800000,
};

static const int vcamio_voltages[] = {
	1800000,
};

static const int vldo28_voltages[] = {
	2800000,
	3000000,
};

static const int vaux18_voltages[] = {
	1800000,
};

static const int vaud28_voltages[] = {
	2800000,
};

static const int vio28_voltages[] = {
	2800000,
};

static const int vio18_voltages[] = {
	1800000,
};

static const int vmc_voltages[] = {
	1800000,
	2900000,
	3000000,
	3300000,
};

static const int vmch_voltages[] = {
	2900000,
	3000000,
	3300000,
};

static const int vemc_voltages[] = {
	2900000,
	3000000,
	3300000,
};

static const int vsim1_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};

static const int vsim2_voltages[] = {
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

static const int vusb33_voltages[] = {
	3000000,
	3100000,
};



static const int vxo22_idx[] = {
	0, 2,
};

static const int vefuse_idx[] = {
	0, 1, 2, 4, 9, 10, 11, 13,
};

static const int vcn33_bt_idx[] = {
	5, 6, 7,
};

static const int vcn33_wifi_idx[] = {
	5, 6, 7,
};

static const int vcama_idx[] = {
	7, 10,
};

static const int vcamd_idx[] = {
	4, 5, 6, 7, 9, 12,
};

static const int vldo28_idx[] = {
	1, 3,
};

static const int vmc_idx[] = {
	4, 10, 11, 13,
};

static const int vmch_idx[] = {
	2, 3, 5,
};

static const int vemc_idx[] = {
	2, 3, 5,
};

static const int vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const int vsim2_idx[] = {
	3, 4, 8, 11, 12,
};

static const int vibr_idx[] = {
	0, 1, 2, 4, 5, 9, 11, 13,
};

static const int vusb33_idx[] = {
	3, 4,
};



/* Regulator vfe28 enable */
static int pmic_ldo_vfe28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vfe28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vfe28 don't have en_cb\n");
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

	RGLTRLOG("ldo vfe28 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vfe28 don't have enable callback\n");
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

	RGLTRLOG("ldo vfe28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vfe28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vs1 enable */
static int pmic_buck_vs1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vs1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("buck vs1 don't have en_cb\n");
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

	RGLTRLOG("buck vs1 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		RGLTRLOG("buck vs1 don't be disabled\n");
		ret = 0;
	}

	return ret;
}

/* Regulator vs1 is_enabled */
static int pmic_buck_vs1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vs1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("buck vs1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vs1 set_voltage_sel */
static int pmic_buck_vs1_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("buck vs1 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("buck vs1 don't have vol_cb\n");
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

	RGLTRLOG("buck vs1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("buck vs1 don't have da_vol_cb\n");
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

	RGLTRLOG("ldo vxo22 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vxo22 don't have en_cb\n");
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

	RGLTRLOG("ldo vxo22 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vxo22 don't have enable callback\n");
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

	RGLTRLOG("ldo vxo22 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vxo22 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vxo22 set_voltage_sel */
static int pmic_ldo_vxo22_set_voltage_sel(struct regulator_dev *rdev,
					  unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vxo22_idx[selector];

	RGLTRLOG("ldo vxo22 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vxo22 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vxo22 get_voltage_sel */
static int pmic_ldo_vxo22_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vxo22 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vxo22 don't have da_vol_cb\n");
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
static int pmic_ldo_vxo22_list_voltage(struct regulator_dev *rdev,
				       unsigned int selector)
{
	int voltage;

	voltage = vxo22_voltages[selector];
	RGLTRLOG("ldo vxo22 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vrf18 enable */
static int pmic_ldo_vrf18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vrf18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vrf18 don't have en_cb\n");
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

	RGLTRLOG("ldo vrf18 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vrf18 don't have enable callback\n");
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

	RGLTRLOG("ldo vrf18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vrf18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vrf12 enable */
static int pmic_ldo_vrf12_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vrf12 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vrf12 don't have en_cb\n");
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

	RGLTRLOG("ldo vrf12 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vrf12 don't have enable callback\n");
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

	RGLTRLOG("ldo vrf12 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vrf12 don't have da_en_cb\n");
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

	pr_info("ldo vefuse enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vefuse don't have en_cb\n");
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

	pr_info("ldo vefuse disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vefuse don't have enable callback\n");
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

	RGLTRLOG("ldo vefuse is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vefuse don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vefuse set_voltage_sel */
static int pmic_ldo_vefuse_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vefuse_idx[selector];

	RGLTRLOG("ldo vefuse set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vefuse don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vefuse get_voltage_sel */
static int pmic_ldo_vefuse_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vefuse get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vefuse don't have da_vol_cb\n");
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
static int pmic_ldo_vefuse_list_voltage(struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vefuse_voltages[selector];
	RGLTRLOG("ldo vefuse list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn33_bt enable */
static int pmic_ldo_vcn33_bt_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcn33_bt enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcn33_bt don't have en_cb\n");
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

	pr_info("ldo vcn33_bt disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcn33_bt don't have enable callback\n");
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

	RGLTRLOG("ldo vcn33_bt is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcn33_bt don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_bt set_voltage_sel */
static int pmic_ldo_vcn33_bt_set_voltage_sel(struct regulator_dev *rdev,
					     unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_bt_idx[selector];

	RGLTRLOG("ldo vcn33_bt set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vcn33_bt don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_bt get_voltage_sel */
static int pmic_ldo_vcn33_bt_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcn33_bt get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vcn33_bt don't have da_vol_cb\n");
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
static int pmic_ldo_vcn33_bt_list_voltage(struct regulator_dev *rdev,
					  unsigned int selector)
{
	int voltage;

	voltage = vcn33_bt_voltages[selector];
	RGLTRLOG("ldo vcn33_bt list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn33_wifi enable */
static int pmic_ldo_vcn33_wifi_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcn33_wifi enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcn33_wifi don't have en_cb\n");
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

	RGLTRLOG("ldo vcn33_wifi disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcn33_wifi don't have enable callback\n");
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

	RGLTRLOG("ldo vcn33_wifi is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcn33_wifi don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn33_wifi set_voltage_sel */
static int pmic_ldo_vcn33_wifi_set_voltage_sel(struct regulator_dev *rdev,
					       unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcn33_wifi_idx[selector];

	RGLTRLOG("ldo vcn33_wifi set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vcn33_wifi don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcn33_wifi get_voltage_sel */
static int pmic_ldo_vcn33_wifi_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcn33_wifi get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vcn33_wifi don't have da_vol_cb\n");
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
static int pmic_ldo_vcn33_wifi_list_voltage(struct regulator_dev *rdev,
					    unsigned int selector)
{
	int voltage;

	voltage = vcn33_wifi_voltages[selector];
	RGLTRLOG("ldo vcn33_wifi list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcn28 enable */
static int pmic_ldo_vcn28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcn28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcn28 don't have en_cb\n");
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

	RGLTRLOG("ldo vcn28 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcn28 don't have enable callback\n");
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

	RGLTRLOG("ldo vcn28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcn28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcn18 enable */
static int pmic_ldo_vcn18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcn18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcn18 don't have en_cb\n");
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

	pr_info("ldo vcn18 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcn18 don't have enable callback\n");
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

	RGLTRLOG("ldo vcn18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcn18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcama enable */
static int pmic_ldo_vcama_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcama enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcama don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama disable */
static int pmic_ldo_vcama_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcama disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcama don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcama is_enabled */
static int pmic_ldo_vcama_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcama is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcama don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcama set_voltage_sel */
static int pmic_ldo_vcama_set_voltage_sel(struct regulator_dev *rdev,
					  unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcama_idx[selector];

	RGLTRLOG("ldo vcama set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vcama don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcama get_voltage_sel */
static int pmic_ldo_vcama_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcama get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vcama don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcama_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcama list_voltage */
static int pmic_ldo_vcama_list_voltage(struct regulator_dev *rdev,
				       unsigned int selector)
{
	int voltage;

	voltage = vcama_voltages[selector];
	RGLTRLOG("ldo vcama list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcamd enable */
static int pmic_ldo_vcamd_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcamd enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcamd don't have en_cb\n");
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

	pr_info("ldo vcamd disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcamd don't have enable callback\n");
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

	RGLTRLOG("ldo vcamd is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcamd don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamd set_voltage_sel */
static int pmic_ldo_vcamd_set_voltage_sel(struct regulator_dev *rdev,
					  unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcamd_idx[selector];

	RGLTRLOG("ldo vcamd set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vcamd don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd get_voltage_sel */
static int pmic_ldo_vcamd_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vcamd get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vcamd don't have da_vol_cb\n");
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
static int pmic_ldo_vcamd_list_voltage(struct regulator_dev *rdev,
				       unsigned int selector)
{
	int voltage;

	voltage = vcamd_voltages[selector];
	RGLTRLOG("ldo vcamd list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vmodem enable */
static int pmic_buck_vmodem_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vmodem enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("buck vmodem don't have en_cb\n");
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

	RGLTRLOG("buck vmodem disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("buck vmodem don't have enable callback\n");
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

	RGLTRLOG("buck vmodem is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("buck vmodem don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmodem set_voltage_sel */
static int pmic_buck_vmodem_set_voltage_sel(struct regulator_dev *rdev,
					    unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("buck vmodem set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("buck vmodem don't have vol_cb\n");
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

	RGLTRLOG("buck vmodem get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("buck vmodem don't have da_vol_cb\n");
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

	RGLTRLOG("buck vcore enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("buck vcore don't have en_cb\n");
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

	RGLTRLOG("buck vcore disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("buck vcore don't have enable callback\n");
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

	RGLTRLOG("buck vcore is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("buck vcore don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcore set_voltage_sel */
static int pmic_buck_vcore_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("buck vcore set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("buck vcore don't have vol_cb\n");
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

	RGLTRLOG("buck vcore get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("buck vcore don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vcamio enable */
static int pmic_ldo_vcamio_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vcamio enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vcamio don't have en_cb\n");
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

	pr_info("ldo vcamio disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vcamio don't have enable callback\n");
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

	RGLTRLOG("ldo vcamio is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vcamio don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vldo28 enable */
static int pmic_ldo_vldo28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vldo28 enable\n");
	if (mreg->en_cb != NULL) {
		ret = (mreg->en_cb)(1);
		/* this OC interrupt needs to delay 1ms after enable power */
		dsb(sy);
		mdelay(1);
		dsb(sy);
		pmic_enable_interrupt(INT_VLDO28_OC, 1, "PMIC");
	} else {
		RGLTRLOG("ldo vldo28 don't have en_cb\n");
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

	RGLTRLOG("ldo vldo28 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL) {
			ret = (mreg->en_cb)(0);
			/* After disable power, this OC interrupt should be
			 * disabled as well
			 */
			pmic_enable_interrupt(INT_VLDO28_OC, 0, "PMIC");
		} else {
			RGLTRLOG("ldo vldo28 don't have enable callback\n");
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

	RGLTRLOG("ldo vldo28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vldo28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vldo28 set_voltage_sel */
static int pmic_ldo_vldo28_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vldo28_idx[selector];

	RGLTRLOG("ldo vldo28 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vldo28 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vldo28 get_voltage_sel */
static int pmic_ldo_vldo28_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vldo28 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vldo28 don't have da_vol_cb\n");
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
static int pmic_ldo_vldo28_list_voltage(struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vldo28_voltages[selector];
	RGLTRLOG("ldo vldo28 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsram_others enable */
static int pmic_ldo_vsram_others_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsram_others enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vsram_others don't have en_cb\n");
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

	RGLTRLOG("ldo vsram_others disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vsram_others don't have en callback\n");
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

	RGLTRLOG("ldo vsram_others is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vsram_others don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_others set_voltage_sel */
static int pmic_ldo_vsram_others_set_voltage_sel(struct regulator_dev *rdev,
						 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("ldo vsram_others set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("ldo vsram_others don't have vol_cb\n");
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

	RGLTRLOG("ldo vsram_others get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vsram_others don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vproc enable */
static int pmic_buck_vproc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vproc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("buck vproc don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc disable */
static int pmic_buck_vproc_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vproc disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("buck vproc don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vproc is_enabled */
static int pmic_buck_vproc_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vproc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("buck vproc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vproc set_voltage_sel */
static int pmic_buck_vproc_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("buck vproc set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("buck vproc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vproc get_voltage_sel */
static int pmic_buck_vproc_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vproc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("buck vproc don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vsram_proc enable */
static int pmic_ldo_vsram_proc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsram_proc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vsram_proc don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc disable */
static int pmic_ldo_vsram_proc_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsram_proc disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vsram_proc don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_proc is_enabled */
static int pmic_ldo_vsram_proc_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsram_proc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vsram_proc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc set_voltage_sel */
static int pmic_ldo_vsram_proc_set_voltage_sel(struct regulator_dev *rdev,
					       unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("ldo vsram_proc set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("ldo vsram_proc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_proc get_voltage_sel */
static int pmic_ldo_vsram_proc_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsram_proc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vsram_proc don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vaux18 enable */
static int pmic_ldo_vaux18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vaux18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vaux18 don't have en_cb\n");
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

	RGLTRLOG("ldo vaux18 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vaux18 don't have enable callback\n");
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

	RGLTRLOG("ldo vaux18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vaux18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vpa enable */
static int pmic_buck_vpa_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("buck vpa enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("buck vpa don't have en_cb\n");
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

	RGLTRLOG("buck vpa disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("buck vpa don't have enable callback\n");
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

	RGLTRLOG("buck vpa is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("buck vpa don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vpa set_voltage_sel */
static int pmic_buck_vpa_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	RGLTRLOG("buck vpa set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		RGLTRLOG("buck vpa don't have vol_cb\n");
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

	RGLTRLOG("buck vpa get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("buck vpa don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vaud28 enable */
static int pmic_ldo_vaud28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vaud28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vaud28 don't have en_cb\n");
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

	pr_info("ldo vaud28 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vaud28 don't have enable callback\n");
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

	RGLTRLOG("ldo vaud28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vaud28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vio28 enable */
static int pmic_ldo_vio28_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vio28 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vio28 don't have en_cb\n");
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

	RGLTRLOG("ldo vio28 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		RGLTRLOG("ldo vio28 don't be disabled\n");
		ret = 0;
	}

	return ret;
}

/* Regulator vio28 is_enabled */
static int pmic_ldo_vio28_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vio28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vio28 don't have da_en_cb\n");
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

	RGLTRLOG("ldo vio18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vio18 don't have en_cb\n");
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

	RGLTRLOG("ldo vio18 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		RGLTRLOG("ldo vio18 don't be disabled\n");
		ret = 0;
	}

	return ret;
}

/* Regulator vio18 is_enabled */
static int pmic_ldo_vio18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vio18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vio18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmc enable */
static int pmic_ldo_vmc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vmc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vmc don't have en_cb\n");
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

	pr_info("ldo vmc disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vmc don't have enable callback\n");
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

	RGLTRLOG("ldo vmc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vmc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmc set_voltage_sel */
static int pmic_ldo_vmc_set_voltage_sel(struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vmc_idx[selector];

	RGLTRLOG("ldo vmc set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vmc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmc get_voltage_sel */
static int pmic_ldo_vmc_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vmc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vmc don't have da_vol_cb\n");
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
static int pmic_ldo_vmc_list_voltage(struct regulator_dev *rdev,
				     unsigned int selector)
{
	int voltage;

	voltage = vmc_voltages[selector];
	RGLTRLOG("ldo vmc list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vmch enable */
static int pmic_ldo_vmch_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vmch enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vmch don't have en_cb\n");
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

	pr_info("ldo vmch disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vmch don't have enable callback\n");
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

	RGLTRLOG("ldo vmch is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vmch don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmch set_voltage_sel */
static int pmic_ldo_vmch_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vmch_idx[selector];

	RGLTRLOG("ldo vmch set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vmch don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmch get_voltage_sel */
static int pmic_ldo_vmch_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vmch get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vmch don't have da_vol_cb\n");
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
static int pmic_ldo_vmch_list_voltage(struct regulator_dev *rdev,
				      unsigned int selector)
{
	int voltage;

	voltage = vmch_voltages[selector];
	RGLTRLOG("ldo vmch list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vemc enable */
static int pmic_ldo_vemc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vemc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vemc don't have en_cb\n");
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

	pr_info("ldo vemc disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vemc don't have enable callback\n");
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

	RGLTRLOG("ldo vemc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vemc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vemc set_voltage_sel */
static int pmic_ldo_vemc_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vemc_idx[selector];

	RGLTRLOG("ldo vemc set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vemc don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vemc get_voltage_sel */
static int pmic_ldo_vemc_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vemc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vemc don't have da_vol_cb\n");
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
static int pmic_ldo_vemc_list_voltage(struct regulator_dev *rdev,
				      unsigned int selector)
{
	int voltage;

	voltage = vemc_voltages[selector];
	RGLTRLOG("ldo vemc list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsim1 enable */
static int pmic_ldo_vsim1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vsim1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vsim1 don't have en_cb\n");
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

	pr_info("ldo vsim1 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vsim1 don't have enable callback\n");
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

	RGLTRLOG("ldo vsim1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vsim1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsim1 set_voltage_sel */
static int pmic_ldo_vsim1_set_voltage_sel(struct regulator_dev *rdev,
					  unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vsim1_idx[selector];

	RGLTRLOG("ldo vsim1 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vsim1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim1 get_voltage_sel */
static int pmic_ldo_vsim1_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsim1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vsim1 don't have da_vol_cb\n");
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
static int pmic_ldo_vsim1_list_voltage(struct regulator_dev *rdev,
				       unsigned int selector)
{
	int voltage;

	voltage = vsim1_voltages[selector];
	RGLTRLOG("ldo vsim1 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vsim2 enable */
static int pmic_ldo_vsim2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vsim2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vsim2 don't have en_cb\n");
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

	pr_info("ldo vsim2 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vsim2 don't have enable callback\n");
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

	RGLTRLOG("ldo vsim2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vsim2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsim2 set_voltage_sel */
static int pmic_ldo_vsim2_set_voltage_sel(struct regulator_dev *rdev,
					  unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vsim2_idx[selector];

	RGLTRLOG("ldo vsim2 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vsim2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsim2 get_voltage_sel */
static int pmic_ldo_vsim2_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vsim2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vsim2 don't have da_vol_cb\n");
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
static int pmic_ldo_vsim2_list_voltage(struct regulator_dev *rdev,
				       unsigned int selector)
{
	int voltage;

	voltage = vsim2_voltages[selector];
	RGLTRLOG("ldo vsim2 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vibr enable */
static int pmic_ldo_vibr_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	pr_info("ldo vibr enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vibr don't have en_cb\n");
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

	pr_info("ldo vibr disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vibr don't have enable callback\n");
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

	RGLTRLOG("ldo vibr is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vibr don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vibr set_voltage_sel */
static int pmic_ldo_vibr_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vibr_idx[selector];

	RGLTRLOG("ldo vibr set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vibr don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vibr get_voltage_sel */
static int pmic_ldo_vibr_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vibr get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vibr don't have da_vol_cb\n");
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
static int pmic_ldo_vibr_list_voltage(struct regulator_dev *rdev,
				      unsigned int selector)
{
	int voltage;

	voltage = vibr_voltages[selector];
	RGLTRLOG("ldo vibr list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vusb33 enable */
static int pmic_ldo_vusb33_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vusb33 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		RGLTRLOG("ldo vusb33 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vusb33 disable */
static int pmic_ldo_vusb33_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vusb33 disable\n");
	if (rdev->use_count == 0) {
		RGLTRLOG("%s should not be disable. (use_count=0).\n"
			 , __func__);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			RGLTRLOG("ldo vusb33 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vusb33 is_enabled */
static int pmic_ldo_vusb33_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vusb33 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		RGLTRLOG("ldo vusb33 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vusb33 set_voltage_sel */
static int pmic_ldo_vusb33_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vusb33_idx[selector];

	RGLTRLOG("ldo vusb33 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		RGLTRLOG("ldo vusb33 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vusb33 get_voltage_sel */
static int pmic_ldo_vusb33_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	RGLTRLOG("ldo vusb33 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		RGLTRLOG("ldo vusb33 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vusb33_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vusb33 list_voltage */
static int pmic_ldo_vusb33_list_voltage(struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vusb33_voltages[selector];
	RGLTRLOG("ldo vusb33 list_voltage: %d\n", voltage);
	return voltage;
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

/* Regulator vfe28 ops */
static struct regulator_ops pmic_ldo_vfe28_ops = {
	.enable = pmic_ldo_vfe28_enable,
	.disable = pmic_ldo_vfe28_disable,
	.is_enabled = pmic_ldo_vfe28_is_enabled,
	/* .enable_time = pmic_ldo_vfe28_enable_time, */
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

/* Regulator vrf18 ops */
static struct regulator_ops pmic_ldo_vrf18_ops = {
	.enable = pmic_ldo_vrf18_enable,
	.disable = pmic_ldo_vrf18_disable,
	.is_enabled = pmic_ldo_vrf18_is_enabled,
	/* .enable_time = pmic_ldo_vrf18_enable_time, */
};

/* Regulator vrf12 ops */
static struct regulator_ops pmic_ldo_vrf12_ops = {
	.enable = pmic_ldo_vrf12_enable,
	.disable = pmic_ldo_vrf12_disable,
	.is_enabled = pmic_ldo_vrf12_is_enabled,
	/* .enable_time = pmic_ldo_vrf12_enable_time, */
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

/* Regulator vcn28 ops */
static struct regulator_ops pmic_ldo_vcn28_ops = {
	.enable = pmic_ldo_vcn28_enable,
	.disable = pmic_ldo_vcn28_disable,
	.is_enabled = pmic_ldo_vcn28_is_enabled,
	/* .enable_time = pmic_ldo_vcn28_enable_time, */
};

/* Regulator vcn18 ops */
static struct regulator_ops pmic_ldo_vcn18_ops = {
	.enable = pmic_ldo_vcn18_enable,
	.disable = pmic_ldo_vcn18_disable,
	.is_enabled = pmic_ldo_vcn18_is_enabled,
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

/* Regulator vmodem ops */
static struct regulator_ops pmic_buck_vmodem_ops = {
	.enable = pmic_buck_vmodem_enable,
	.disable = pmic_buck_vmodem_disable,
	.is_enabled = pmic_buck_vmodem_is_enabled,
	.get_voltage_sel = pmic_buck_vmodem_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vmodem_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vmodem_enable_time, */
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

/* Regulator vcamio ops */
static struct regulator_ops pmic_ldo_vcamio_ops = {
	.enable = pmic_ldo_vcamio_enable,
	.disable = pmic_ldo_vcamio_disable,
	.is_enabled = pmic_ldo_vcamio_is_enabled,
	/* .enable_time = pmic_ldo_vcamio_enable_time, */
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

/* Regulator vproc ops */
static struct regulator_ops pmic_buck_vproc_ops = {
	.enable = pmic_buck_vproc_enable,
	.disable = pmic_buck_vproc_disable,
	.is_enabled = pmic_buck_vproc_is_enabled,
	.get_voltage_sel = pmic_buck_vproc_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.set_mode = pmic_buck_set_mode,
	.get_mode = pmic_buck_get_mode,
	/* .enable_time = pmic_buck_vproc_enable_time, */
};

/* Regulator vsram_proc ops */
static struct regulator_ops pmic_ldo_vsram_proc_ops = {
	.enable = pmic_ldo_vsram_proc_enable,
	.disable = pmic_ldo_vsram_proc_disable,
	.is_enabled = pmic_ldo_vsram_proc_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_proc_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_proc_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_proc_enable_time, */
};

/* Regulator vaux18 ops */
static struct regulator_ops pmic_ldo_vaux18_ops = {
	.enable = pmic_ldo_vaux18_enable,
	.disable = pmic_ldo_vaux18_disable,
	.is_enabled = pmic_ldo_vaux18_is_enabled,
	/* .enable_time = pmic_ldo_vaux18_enable_time, */
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

/* Regulator vaud28 ops */
static struct regulator_ops pmic_ldo_vaud28_ops = {
	.enable = pmic_ldo_vaud28_enable,
	.disable = pmic_ldo_vaud28_disable,
	.is_enabled = pmic_ldo_vaud28_is_enabled,
	/* .enable_time = pmic_ldo_vaud28_enable_time, */
};

/* Regulator vio28 ops */
static struct regulator_ops pmic_ldo_vio28_ops = {
	.enable = pmic_ldo_vio28_enable,
	.disable = pmic_ldo_vio28_disable,
	.is_enabled = pmic_ldo_vio28_is_enabled,
	/* .enable_time = pmic_ldo_vio28_enable_time, */
};

/* Regulator vio18 ops */
static struct regulator_ops pmic_ldo_vio18_ops = {
	.enable = pmic_ldo_vio18_enable,
	.disable = pmic_ldo_vio18_disable,
	.is_enabled = pmic_ldo_vio18_is_enabled,
	/* .enable_time = pmic_ldo_vio18_enable_time, */
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



/*------Regulator ATTR------*/
static ssize_t show_regulator_status(struct device *dev,
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

	RGLTRLOG("[EM] %s_STATUS : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_regulator_status(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	RGLTRLOG("[EM] Not Support Write Function\n");
	return size;
}

static ssize_t show_regulator_voltage(struct device *dev,
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
					mreg->desc.uV_step * regVal;
		} else
			RGLTRLOG("[EM] %s_VOLTAGE have no da_vol_cb\n"
				 , mreg->desc.name);
	} else {
		if (mreg->pvoltages != NULL) {
			pVoltage = (const int *)mreg->pvoltages;
			ret_value = pVoltage[0];
		} else if (mreg->desc.fixed_uV)
			ret_value = mreg->desc.fixed_uV;
		else
			RGLTRLOG("[EM] %s_VOLTAGE have no pVolatges\n"
				 , mreg->desc.name);
	}

	ret_value = ret_value / 1000;

	RGLTRLOG("[EM] %s_VOLTAGE : %d\n", mreg->desc.name, ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_regulator_voltage(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	RGLTRLOG("[EM] Not Support Write Function\n");
	return size;
}

static unsigned int mt6390_map_mode(unsigned int mode)
{
	return mode == 1 ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

/* Regulator: BUCK */
#define BUCK_EN	REGULATOR_CHANGE_STATUS
#define BUCK_VOL REGULATOR_CHANGE_VOLTAGE
#define BUCK_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
#define BUCK_VOL_EN_MODE (REGULATOR_CHANGE_STATUS | \
	REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mt_bucks[] = {
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vs1, buck, 1200000, 2200000, 12500, 0,
		BUCK_VOL_EN, PMIC_RG_VS1_MODESET, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vmodem, buck, 500000, 1193750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VMODEM_FPWM, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vcore, buck, 518750, 1193750, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VCORE_FPWM, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vproc, buck, 518750, 1312500, 6250, 0,
		BUCK_VOL_EN_MODE, PMIC_RG_VPROC_FPWM, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(
		vpa, buck, 500000, 3650000, 50000, 0,
		BUCK_VOL_EN, PMIC_RG_VPA_MODESET, 1),
};


/* Regulator: LDO */
#define LDO_EN	REGULATOR_CHANGE_STATUS
#define LDO_VOL REGULATOR_CHANGE_VOLTAGE
#define LDO_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mt_ldos[] = {
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vfe28, ldo,
		2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vxo22, ldo,
		vxo22_voltages, vxo22_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18, ldo,
		1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf12, ldo,
		1200000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vefuse, ldo,
		vefuse_voltages, vefuse_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_bt, ldo,
		vcn33_bt_voltages, vcn33_bt_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_wifi, ldo,
		vcn33_wifi_voltages, vcn33_wifi_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn28, ldo,
		2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn18, ldo,
		1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama, ldo,
		vcama_voltages, vcama_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamd, ldo,
		vcamd_voltages, vcamd_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcamio, ldo,
		1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28, ldo,
		vldo28_voltages, vldo28_idx, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_others, ldo,
		518750, 1312500, 6250, 0, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc, ldo,
		518750, 1312500, 6250, 0, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaux18, ldo,
		1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vaud28, ldo,
		2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio28, ldo,
		2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio18, ldo,
		1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmc, ldo,
		vmc_voltages, vmc_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmch, ldo,
		vmch_voltages, vmch_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vemc, ldo,
		vemc_voltages, vemc_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim1, ldo,
		vsim1_voltages, vsim1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim2, ldo,
		vsim2_voltages, vsim2_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vibr, ldo,
		vibr_voltages, vibr_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vusb33, ldo,
		vusb33_voltages, vusb33_idx, LDO_VOL_EN, 1),

};



int mt_ldos_size = ARRAY_SIZE(mt_ldos);
int mt_bucks_size = ARRAY_SIZE(mt_bucks);
/* -------Code Gen End-------*/

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY

#define PMIC_REGULATOR_LDO_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_ldos[MT6390_POWER_LDO_##_id],	\
	}

#define PMIC_REGULATOR_BUCK_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_bucks[MT6390_POWER_BUCK_##_id],	\
	}




struct of_regulator_match pmic_regulator_buck_matches[] = {
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs1, VS1),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vmodem, VMODEM),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vcore, VCORE),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc, VPROC),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vpa, VPA),

};

struct of_regulator_match pmic_regulator_ldo_matches[] = {
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vfe28, VFE28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vxo22, VXO22),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf18, VRF18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf12, VRF12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vefuse, VEFUSE),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_bt, VCN33_BT),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_wifi, VCN33_WIFI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn28, VCN28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn18, VCN18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcama, VCAMA),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamd, VCAMD),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamio, VCAMIO),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vldo28, VLDO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_others, VSRAM_OTHERS),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc, VSRAM_PROC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaux18, VAUX18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vaud28, VAUD28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio28, VIO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio18, VIO18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmc, VMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmch, VMCH),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vemc, VEMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim1, VSIM1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim2, VSIM2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vibr, VIBR),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vusb33, VUSB33),

};

int pmic_regulator_ldo_matches_size = ARRAY_SIZE(pmic_regulator_ldo_matches);
int pmic_regulator_buck_matches_size = ARRAY_SIZE(pmic_regulator_buck_matches);

#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
