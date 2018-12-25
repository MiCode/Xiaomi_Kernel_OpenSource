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

#include <linux/delay.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_ldo.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"

static const int vcamd1_voltages[] = {
	900000,
	1000000,
	1100000,
	1200000,
};

static const int vsim1_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};

static const int vgp_voltages[] = {
	1500000,
	1700000,
	1800000,
	2000000,
	2100000,
	2200000,
	2800000,
	3300000,
};

static const int vmipi_voltages[] = {
	1700000,
	1800000,
	2000000,
	2100000,
};

static const int vxo22_voltages[] = {
	2200000,
	2300000,
};

static const int vcamd2_voltages[] = {
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
	1500000,
	1800000,
};

static const int vmch_voltages[] = {
	2900000,
	3000000,
	3300000,
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

static const int va10_voltages[] = {
	600000,
	700000,
	800000,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
	1400000,
	1500000,
	1600000,
	1700000,
	1800000,
	1900000,
	2000000,
	2100000,
};

static const int vgp2_voltages[] = {
	1200000,
	1300000,
	1500000,
	1800000,
	2000000,
	2800000,
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

static const int vldo28_tp_voltages[] = {
	2800000,
	3000000,
};

static const int vsim2_voltages[] = {
	1700000,
	1800000,
	2700000,
	3000000,
	3100000,
};

static const int vcamd1_idx[] = {
	3, 4, 5, 6,
};

static const int vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const int vgp_idx[] = {
	2, 3, 4, 5, 6, 7, 9, 13,
};

static const int vmipi_idx[] = {
	11, 12, 14, 15,
};

static const int vxo22_idx[] = {
	4, 5,
};

static const int vcamd2_idx[] = {
	3, 4, 5, 6, 7, 9, 12,
};

static const int vmch_idx[] = {
	10, 11, 13,
};

static const int vcama1_idx[] = {
	0, 7, 9, 10, 11, 12,
};

static const int vemc_idx[] = {
	10, 11, 13,
};

static const int va10_idx[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static const int vgp2_idx[] = {
	0, 1, 2, 4, 5, 9, 11, 13,
};

static const int vcn33_bt_idx[] = {
	13, 14, 15,
};

static const int vcn33_wifi_idx[] = {
	13, 14, 15,
};

static const int vcama2_idx[] = {
	7, 9, 10, 11, 12,
};

static const int vmc_idx[] = {
	4, 10, 11, 13,
};

static const int vldo28_idx[] = {
	9, 11,
};

static const int vldo28_tp_idx[] = {
	9, 11,
}; /*--ldo_volt_table2--*/

static const int vsim2_idx[] = {
	3, 4, 8, 11, 12,
};

/* Regulator vdram2 enable */
static int pmic_buck_vdram2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("buck vdram2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram2 disable */
static int pmic_buck_vdram2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("buck vdram2 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("buck vdram2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vdram2 is_enabled */
static int pmic_buck_vdram2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("buck vdram2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vdram2 set_voltage_sel */
static int pmic_buck_vdram2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("buck vdram2 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("buck vdram2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vdram2 get_voltage_sel */
static int pmic_buck_vdram2_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("buck vdram2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("buck vdram2 don't have da_vol_cb\n");
		return -1;
	}

	return (mreg->da_vol_cb)();
}

/* Regulator vcamd1 enable */
static int pmic_ldo_vcamd1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd1 enable\n");
	if (mreg->en_cb != NULL) {
		ret = (mreg->en_cb)(1);
		dsb(sy);
		mdelay(1);
		dsb(sy);
		pmic_set_register_value(PMIC_RG_INT_EN_VCAMD1_OC, 1);
	} else {
		pr_notice("ldo vcamd1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd1 disable */
static int pmic_ldo_vcamd1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcamd1 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL) {
			ret = (mreg->en_cb)(0);
			pmic_set_register_value(PMIC_RG_INT_EN_VCAMD1_OC, 0);
		} else {
			pr_notice("ldo vcamd1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcamd1 is_enabled */
static int pmic_ldo_vcamd1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcamd1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamd1 set_voltage_sel */
static int pmic_ldo_vcamd1_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcamd1_idx[selector];

	PMICLOG("ldo vcamd1 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcamd1 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd1 get_voltage_sel */
static int pmic_ldo_vcamd1_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd1 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcamd1 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcamd1_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcamd1 list_voltage */
static int pmic_ldo_vcamd1_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcamd1_voltages[selector];
	PMICLOG("ldo vcamd1 list_voltage: %d\n", voltage);
	return voltage;
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
		PMICLOG("ldo vsim1 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vsim1 set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vsim1 list_voltage: %d\n", voltage);
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
		PMICLOG("buck vs1 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		pr_notice("buck vs1 non-disable\n");
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

	PMICLOG("buck vs1 set_voltage_sel: %d\n", selector);
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

/* Regulator vgp enable */
static int pmic_ldo_vgp_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vgp don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgp disable */
static int pmic_ldo_vgp_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vgp should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vgp don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vgp is_enabled */
static int pmic_ldo_vgp_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vgp don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vgp set_voltage_sel */
static int pmic_ldo_vgp_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vgp_idx[selector];

	PMICLOG("ldo vgp set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vgp don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgp get_voltage_sel */
static int pmic_ldo_vgp_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vgp don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vgp_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vgp list_voltage */
static int pmic_ldo_vgp_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vgp_voltages[selector];
	PMICLOG("ldo vgp list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vusb33 enable */
static int pmic_ldo_vusb33_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vusb33 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vusb33 don't have en_cb\n");
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

	PMICLOG("ldo vusb33 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vusb33 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vusb33 don't have enable callback\n");
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

	PMICLOG("ldo vusb33 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vusb33 don't have da_en_cb\n");
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
		PMICLOG("ldo vrf12 should not be disable (use_count=%d)\n",
			rdev->use_count);
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
		PMICLOG("buck vdram1 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vdram1 set_voltage_sel: %d\n", selector);
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
		PMICLOG("ldo vcamio should not be disable (use_count=%d)\n",
			rdev->use_count);
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
		PMICLOG("ldo vcn18 should not be disable (use_count=%d)\n",
			rdev->use_count);
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
		PMICLOG("ldo vfe28 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

/* Regulator vrf18_2 enable */
static int pmic_ldo_vrf18_2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vrf18_2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf18_2 disable */
static int pmic_ldo_vrf18_2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vrf18_2 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vrf18_2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vrf18_2 is_enabled */
static int pmic_ldo_vrf18_2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vrf18_2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator va18 enable */
static int pmic_ldo_va18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo va18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va18 disable */
static int pmic_ldo_va18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo va18 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo va18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator va18 is_enabled */
static int pmic_ldo_va18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo va18 don't have da_en_cb\n");
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
		PMICLOG("buck vmodem should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vmodem set_voltage_sel: %d\n", selector);
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
		PMICLOG("buck vcore should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vcore set_voltage_sel: %d\n", selector);
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
		PMICLOG("ldo vcn28 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

/* Regulator vmipi enable */
static int pmic_ldo_vmipi_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmipi enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vmipi don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmipi disable */
static int pmic_ldo_vmipi_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmipi disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vmipi should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vmipi don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vmipi is_enabled */
static int pmic_ldo_vmipi_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmipi is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vmipi don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vmipi set_voltage_sel */
static int pmic_ldo_vmipi_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vmipi_idx[selector];

	PMICLOG("ldo vmipi set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vmipi don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vmipi get_voltage_sel */
static int pmic_ldo_vmipi_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vmipi get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vmipi don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vmipi_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vmipi list_voltage */
static int pmic_ldo_vmipi_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vmipi_voltages[selector];
	PMICLOG("ldo vmipi list_voltage: %d\n", voltage);
	return voltage;
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
		PMICLOG("ldo vsram_gpu should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vsram_gpu set_voltage_sel: %d\n", selector);
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

/* Regulator vsram_core enable */
static int pmic_ldo_vsram_core_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_core enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_core don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_core disable */
static int pmic_ldo_vsram_core_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_core disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_core should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_core don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vsram_core is_enabled */
static int pmic_ldo_vsram_core_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_core is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_core don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_core set_voltage_sel */
static int pmic_ldo_vsram_core_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_core set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_core don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vsram_core get_voltage_sel */
static int pmic_ldo_vsram_core_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_core get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_core don't have da_vol_cb\n");
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
		PMICLOG("buck vs2 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vs2 set_voltage_sel: %d\n", selector);
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

/* Regulator vsram_proc enable */
static int pmic_ldo_vsram_proc_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vsram_proc enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vsram_proc don't have en_cb\n");
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

	PMICLOG("ldo vsram_proc disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vsram_proc should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vsram_proc don't have enable callback\n");
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

	PMICLOG("ldo vsram_proc is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vsram_proc don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vsram_proc set_voltage_sel */
static int pmic_ldo_vsram_proc_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	PMICLOG("ldo vsram_proc set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(selector);
	else {
		pr_notice("ldo vsram_proc don't have vol_cb\n");
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

	PMICLOG("ldo vsram_proc get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vsram_proc don't have da_vol_cb\n");
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
		PMICLOG("ldo vxo22 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vxo22 set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vxo22 list_voltage: %d\n", voltage);
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
		PMICLOG("buck vpa should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vpa set_voltage_sel: %d\n", selector);
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

/* Regulator vrf18_1 enable */
static int pmic_ldo_vrf18_1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_1 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vrf18_1 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vrf18_1 disable */
static int pmic_ldo_vrf18_1_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_1 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vrf18_1 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vrf18_1 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vrf18_1 is_enabled */
static int pmic_ldo_vrf18_1_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vrf18_1 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vrf18_1 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamd2 enable */
static int pmic_ldo_vcamd2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd2 enable\n");
	if (mreg->en_cb != NULL) {
		ret = (mreg->en_cb)(1);
		dsb(sy);
		mdelay(2);
		dsb(sy);
		pmic_set_register_value(PMIC_RG_INT_EN_VCAMD2_OC, 1);
	} else {
		pr_notice("ldo vcamd2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd2 disable */
static int pmic_ldo_vcamd2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vcamd2 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL) {
			ret = (mreg->en_cb)(0);
			pmic_set_register_value(PMIC_RG_INT_EN_VCAMD2_OC, 0);
		} else {
			pr_notice("ldo vcamd2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vcamd2 is_enabled */
static int pmic_ldo_vcamd2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vcamd2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcamd2 set_voltage_sel */
static int pmic_ldo_vcamd2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vcamd2_idx[selector];

	PMICLOG("ldo vcamd2 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vcamd2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vcamd2 get_voltage_sel */
static int pmic_ldo_vcamd2_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcamd2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vcamd2 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vcamd2_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vcamd2 list_voltage */
static int pmic_ldo_vcamd2_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vcamd2_voltages[selector];
	PMICLOG("ldo vcamd2 list_voltage: %d\n", voltage);
	return voltage;
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
		PMICLOG("buck vproc12 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vproc12 set_voltage_sel: %d\n", selector);
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
		PMICLOG("buck vgpu should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vgpu set_voltage_sel: %d\n", selector);
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
		PMICLOG("ldo vmch should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vmch set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vmch list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vcama1 enable */
static int pmic_ldo_vcama1_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama1 enable\n");
	if (mreg->en_cb != NULL) {
		ret = (mreg->en_cb)(1);
		dsb(sy);
		mdelay(1);
		dsb(sy);
		pmic_set_register_value(PMIC_RG_INT_EN_VCAMA1_OC, 1);
	} else {
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
		PMICLOG("ldo vcama1 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL) {
			ret = (mreg->en_cb)(0);
			pmic_set_register_value(PMIC_RG_INT_EN_VCAMA1_OC, 0);
		} else {
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

	PMICLOG("ldo vcama1 set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vcama1 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vtcxo24 enable */
static int pmic_ldo_vtcxo24_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vtcxo24 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vtcxo24 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vtcxo24 disable */
static int pmic_ldo_vtcxo24_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vtcxo24 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vtcxo24 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vtcxo24 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vtcxo24 is_enabled */
static int pmic_ldo_vtcxo24_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vtcxo24 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vtcxo24 don't have da_en_cb\n");
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
		PMICLOG("ldo vio28 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		pr_notice("ldo vio28 non-disable\n");
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

	PMICLOG("ldo vio28 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vio28 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
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
		PMICLOG("ldo vemc should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vemc set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vemc list_voltage: %d\n", voltage);
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
		PMICLOG("ldo va12 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

/* Regulator va10 enable */
static int pmic_ldo_va10_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va10 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo va10 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va10 disable */
static int pmic_ldo_va10_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va10 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo va10 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo va10 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator va10 is_enabled */
static int pmic_ldo_va10_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va10 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo va10 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator va10 set_voltage_sel */
static int pmic_ldo_va10_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = va10_idx[selector];

	PMICLOG("ldo va10 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo va10 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator va10 get_voltage_sel */
static int pmic_ldo_va10_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo va10 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo va10 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (va10_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator va10 list_voltage */
static int pmic_ldo_va10_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = va10_voltages[selector];
	PMICLOG("ldo va10 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vgp2 enable */
static int pmic_ldo_vgp2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp2 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vgp2 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgp2 disable */
static int pmic_ldo_vgp2_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp2 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vgp2 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vgp2 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vgp2 is_enabled */
static int pmic_ldo_vgp2_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp2 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vgp2 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vgp2 set_voltage_sel */
static int pmic_ldo_vgp2_set_voltage_sel(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vgp2_idx[selector];

	PMICLOG("ldo vgp2 set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vgp2 don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vgp2 get_voltage_sel */
static int pmic_ldo_vgp2_get_voltage_sel(
					struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vgp2 get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vgp2 don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vgp2_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vgp2 list_voltage */
static int pmic_ldo_vgp2_list_voltage(
					struct regulator_dev *rdev,
					unsigned int selector)
{
	int voltage;

	voltage = vgp2_voltages[selector];
	PMICLOG("ldo vgp2 list_voltage: %d\n", voltage);
	return voltage;
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
		PMICLOG("ldo vio18 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		pr_notice("ldo vio18 non-disable\n");
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

	PMICLOG("ldo vio18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vio18 don't have da_en_cb\n");
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
		PMICLOG("ldo vcn33_bt should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vcn33_bt set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vcn33_bt list_voltage: %d\n", voltage);
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
		PMICLOG("ldo vcn33_wifi should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vcn33_wifi set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vcn33_wifi list_voltage: %d\n", voltage);
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
		PMICLOG("ldo vsram_md should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vsram_md set_voltage_sel: %d\n", selector);
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
		PMICLOG("ldo vbif28 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

/* Regulator vufs18 enable */
static int pmic_ldo_vufs18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vufs18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vufs18 disable */
static int pmic_ldo_vufs18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vufs18 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vufs18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vufs18 is_enabled */
static int pmic_ldo_vufs18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vufs18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vufs18 don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vcama2 enable */
static int pmic_ldo_vcama2_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vcama2 enable\n");
	if (mreg->en_cb != NULL) {
		ret = (mreg->en_cb)(1);
		dsb(sy);
		mdelay(2);
		dsb(sy);
		pmic_set_register_value(PMIC_RG_INT_EN_VCAMA2_OC, 1);
	} else {
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
		PMICLOG("ldo vcama2 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL) {
			ret = (mreg->en_cb)(0);
			pmic_set_register_value(PMIC_RG_INT_EN_VCAMA2_OC, 0);
		} else {
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

	PMICLOG("ldo vcama2 set_voltage_sel: %d\n", selector);
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
				ret = -1;
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
	PMICLOG("ldo vcama2 list_voltage: %d\n", voltage);
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
		PMICLOG("ldo vmc should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vmc set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vmc list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vldo28_tp enable */
static int pmic_ldo_vldo28_tp_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28_tp enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vldo28_tp don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vldo28_tp disable */
static int pmic_ldo_vldo28_tp_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28_tp disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vldo28_tp should not be disable (use_count=%d)\n"
			, rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vldo28_tp don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vldo28_tp is_enabled */
static int pmic_ldo_vldo28_tp_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28_tp is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vldo28_tp don't have da_en_cb\n");
		return -1;
	}

	return (mreg->da_en_cb)();
}

/* Regulator vldo28_tp set_voltage_sel */
static int pmic_ldo_vldo28_tp_set_voltage_sel(
			struct regulator_dev *rdev, unsigned int selector)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int idx = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);
	mreg->vosel.cur_sel = selector;

	idx = vldo28_tp_idx[selector];

	PMICLOG("ldo vldo28_tp set_voltage_sel: %d\n", selector);
	if (mreg->vol_cb != NULL)
		ret = (mreg->vol_cb)(idx);
	else {
		pr_notice("ldo vldo28_tp don't have vol_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vldo28_tp get_voltage_sel */
static int pmic_ldo_vldo28_tp_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	unsigned int i = 0, selector = 0;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vldo28_tp get_voltage_sel\n");
	if (mreg->da_vol_cb == NULL) {
		pr_notice("ldo vldo28_tp don't have da_vol_cb\n");
		ret = -1;
	} else {
		ret = -1;
		selector = (mreg->da_vol_cb)();
		for (i = 0; i < rdesc->n_voltages; i++) {
			if (vldo28_tp_idx[i] == selector) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

/* Regulator vldo28_tp list_voltage */
static int pmic_ldo_vldo28_tp_list_voltage(
			struct regulator_dev *rdev, unsigned int selector)
{
	int voltage;

	voltage = vldo28_tp_voltages[selector];
	PMICLOG("ldo vldo28_tp list_voltage: %d\n", voltage);
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
		PMICLOG("ldo vldo28 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vldo28 set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vldo28 list_voltage: %d\n", voltage);
	return voltage;
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
		PMICLOG("buck vproc11 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("buck vproc11 set_voltage_sel: %d\n", selector);
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

/* Regulator vxo18 enable */
static int pmic_ldo_vxo18_enable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo18 enable\n");
	if (mreg->en_cb != NULL)
		ret = (mreg->en_cb)(1);
	else {
		pr_notice("ldo vxo18 don't have en_cb\n");
		ret = -1;
	}

	return ret;
}

/* Regulator vxo18 disable */
static int pmic_ldo_vxo18_disable(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;
	int ret = 0;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo18 disable\n");
	if (rdev->use_count == 0) {
		PMICLOG("ldo vxo18 should not be disable (use_count=%d)\n",
			rdev->use_count);
		ret = -1;
	} else {
		if (mreg->en_cb != NULL)
			ret = (mreg->en_cb)(0);
		else {
			pr_notice("ldo vxo18 don't have enable callback\n");
			ret = -1;
		}
	}

	return ret;
}

/* Regulator vxo18 is_enabled */
static int pmic_ldo_vxo18_is_enabled(struct regulator_dev *rdev)
{
	const struct regulator_desc *rdesc = rdev->desc;
	struct mtk_regulator *mreg;

	mreg = container_of(rdesc, struct mtk_regulator, desc);

	PMICLOG("ldo vxo18 is_enabled\n");
	if (mreg->da_en_cb == NULL) {
		pr_notice("ldo vxo18 don't have da_en_cb\n");
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
		PMICLOG("ldo vsim2 should not be disable (use_count=%d)\n",
			rdev->use_count);
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

	PMICLOG("ldo vsim2 set_voltage_sel: %d\n", selector);
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
	PMICLOG("ldo vsim2 list_voltage: %d\n", voltage);
	return voltage;
}

/* Regulator vdram2 ops */
static struct regulator_ops pmic_buck_vdram2_ops = {
	.enable = pmic_buck_vdram2_enable,
	.disable = pmic_buck_vdram2_disable,
	.is_enabled = pmic_buck_vdram2_is_enabled,
	.get_voltage_sel = pmic_buck_vdram2_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vdram2_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vdram2_enable_time, */
};

/* Regulator vcamd1 ops */
static struct regulator_ops pmic_ldo_vcamd1_ops = {
	.enable = pmic_ldo_vcamd1_enable,
	.disable = pmic_ldo_vcamd1_disable,
	.is_enabled = pmic_ldo_vcamd1_is_enabled,
	.get_voltage_sel = pmic_ldo_vcamd1_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcamd1_set_voltage_sel,
	.list_voltage = pmic_ldo_vcamd1_list_voltage,
	/* .enable_time = pmic_ldo_vcamd1_enable_time, */
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

/* Regulator vgp ops */
static struct regulator_ops pmic_ldo_vgp_ops = {
	.enable = pmic_ldo_vgp_enable,
	.disable = pmic_ldo_vgp_disable,
	.is_enabled = pmic_ldo_vgp_is_enabled,
	.get_voltage_sel = pmic_ldo_vgp_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vgp_set_voltage_sel,
	.list_voltage = pmic_ldo_vgp_list_voltage,
	/* .enable_time = pmic_ldo_vgp_enable_time, */
};

/* Regulator vusb33 ops */
static struct regulator_ops pmic_ldo_vusb33_ops = {
	.enable = pmic_ldo_vusb33_enable,
	.disable = pmic_ldo_vusb33_disable,
	.is_enabled = pmic_ldo_vusb33_is_enabled,
	/* .enable_time = pmic_ldo_vusb33_enable_time, */
};

/* Regulator vrf12 ops */
static struct regulator_ops pmic_ldo_vrf12_ops = {
	.enable = pmic_ldo_vrf12_enable,
	.disable = pmic_ldo_vrf12_disable,
	.is_enabled = pmic_ldo_vrf12_is_enabled,
	/* .enable_time = pmic_ldo_vrf12_enable_time, */
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

/* Regulator vcamio ops */
static struct regulator_ops pmic_ldo_vcamio_ops = {
	.enable = pmic_ldo_vcamio_enable,
	.disable = pmic_ldo_vcamio_disable,
	.is_enabled = pmic_ldo_vcamio_is_enabled,
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

/* Regulator vrf18_2 ops */
static struct regulator_ops pmic_ldo_vrf18_2_ops = {
	.enable = pmic_ldo_vrf18_2_enable,
	.disable = pmic_ldo_vrf18_2_disable,
	.is_enabled = pmic_ldo_vrf18_2_is_enabled,
	/* .enable_time = pmic_ldo_vrf18_2_enable_time, */
};

/* Regulator va18 ops */
static struct regulator_ops pmic_ldo_va18_ops = {
	.enable = pmic_ldo_va18_enable,
	.disable = pmic_ldo_va18_disable,
	.is_enabled = pmic_ldo_va18_is_enabled,
	/* .enable_time = pmic_ldo_va18_enable_time, */
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

/* Regulator vcore ops */
static struct regulator_ops pmic_buck_vcore_ops = {
	.enable = pmic_buck_vcore_enable,
	.disable = pmic_buck_vcore_disable,
	.is_enabled = pmic_buck_vcore_is_enabled,
	.get_voltage_sel = pmic_buck_vcore_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vcore_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vcore_enable_time, */
};

/* Regulator vcn28 ops */
static struct regulator_ops pmic_ldo_vcn28_ops = {
	.enable = pmic_ldo_vcn28_enable,
	.disable = pmic_ldo_vcn28_disable,
	.is_enabled = pmic_ldo_vcn28_is_enabled,
	/* .enable_time = pmic_ldo_vcn28_enable_time, */
};

/* Regulator vmipi ops */
static struct regulator_ops pmic_ldo_vmipi_ops = {
	.enable = pmic_ldo_vmipi_enable,
	.disable = pmic_ldo_vmipi_disable,
	.is_enabled = pmic_ldo_vmipi_is_enabled,
	.get_voltage_sel = pmic_ldo_vmipi_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vmipi_set_voltage_sel,
	.list_voltage = pmic_ldo_vmipi_list_voltage,
	/* .enable_time = pmic_ldo_vmipi_enable_time, */
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

/* Regulator vsram_core ops */
static struct regulator_ops pmic_ldo_vsram_core_ops = {
	.enable = pmic_ldo_vsram_core_enable,
	.disable = pmic_ldo_vsram_core_disable,
	.is_enabled = pmic_ldo_vsram_core_is_enabled,
	.get_voltage_sel = pmic_ldo_vsram_core_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vsram_core_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_ldo_vsram_core_enable_time, */
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

/* Regulator vrf18_1 ops */
static struct regulator_ops pmic_ldo_vrf18_1_ops = {
	.enable = pmic_ldo_vrf18_1_enable,
	.disable = pmic_ldo_vrf18_1_disable,
	.is_enabled = pmic_ldo_vrf18_1_is_enabled,
	/* .enable_time = pmic_ldo_vrf18_1_enable_time, */
};

/* Regulator vcamd2 ops */
static struct regulator_ops pmic_ldo_vcamd2_ops = {
	.enable = pmic_ldo_vcamd2_enable,
	.disable = pmic_ldo_vcamd2_disable,
	.is_enabled = pmic_ldo_vcamd2_is_enabled,
	.get_voltage_sel = pmic_ldo_vcamd2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vcamd2_set_voltage_sel,
	.list_voltage = pmic_ldo_vcamd2_list_voltage,
	/* .enable_time = pmic_ldo_vcamd2_enable_time, */
};

/* Regulator vproc12 ops */
static struct regulator_ops pmic_buck_vproc12_ops = {
	.enable = pmic_buck_vproc12_enable,
	.disable = pmic_buck_vproc12_disable,
	.is_enabled = pmic_buck_vproc12_is_enabled,
	.get_voltage_sel = pmic_buck_vproc12_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc12_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vproc12_enable_time, */
};

/* Regulator vgpu ops */
static struct regulator_ops pmic_buck_vgpu_ops = {
	.enable = pmic_buck_vgpu_enable,
	.disable = pmic_buck_vgpu_disable,
	.is_enabled = pmic_buck_vgpu_is_enabled,
	.get_voltage_sel = pmic_buck_vgpu_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vgpu_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vgpu_enable_time, */
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

/* Regulator vtcxo24 ops */
static struct regulator_ops pmic_ldo_vtcxo24_ops = {
	.enable = pmic_ldo_vtcxo24_enable,
	.disable = pmic_ldo_vtcxo24_disable,
	.is_enabled = pmic_ldo_vtcxo24_is_enabled,
	/* .enable_time = pmic_ldo_vtcxo24_enable_time, */
};

/* Regulator vio28 ops */
static struct regulator_ops pmic_ldo_vio28_ops = {
	.enable = pmic_ldo_vio28_enable,
	.disable = pmic_ldo_vio28_disable,
	.is_enabled = pmic_ldo_vio28_is_enabled,
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

/* Regulator va12 ops */
static struct regulator_ops pmic_ldo_va12_ops = {
	.enable = pmic_ldo_va12_enable,
	.disable = pmic_ldo_va12_disable,
	.is_enabled = pmic_ldo_va12_is_enabled,
	/* .enable_time = pmic_ldo_va12_enable_time, */
};

/* Regulator va10 ops */
static struct regulator_ops pmic_ldo_va10_ops = {
	.enable = pmic_ldo_va10_enable,
	.disable = pmic_ldo_va10_disable,
	.is_enabled = pmic_ldo_va10_is_enabled,
	.get_voltage_sel = pmic_ldo_va10_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_va10_set_voltage_sel,
	.list_voltage = pmic_ldo_va10_list_voltage,
	/* .enable_time = pmic_ldo_va10_enable_time, */
};

/* Regulator vgp2 ops */
static struct regulator_ops pmic_ldo_vgp2_ops = {
	.enable = pmic_ldo_vgp2_enable,
	.disable = pmic_ldo_vgp2_disable,
	.is_enabled = pmic_ldo_vgp2_is_enabled,
	.get_voltage_sel = pmic_ldo_vgp2_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vgp2_set_voltage_sel,
	.list_voltage = pmic_ldo_vgp2_list_voltage,
	/* .enable_time = pmic_ldo_vgp2_enable_time, */
};

/* Regulator vio18 ops */
static struct regulator_ops pmic_ldo_vio18_ops = {
	.enable = pmic_ldo_vio18_enable,
	.disable = pmic_ldo_vio18_disable,
	.is_enabled = pmic_ldo_vio18_is_enabled,
	/* .enable_time = pmic_ldo_vio18_enable_time, */
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

/* Regulator vbif28 ops */
static struct regulator_ops pmic_ldo_vbif28_ops = {
	.enable = pmic_ldo_vbif28_enable,
	.disable = pmic_ldo_vbif28_disable,
	.is_enabled = pmic_ldo_vbif28_is_enabled,
	/* .enable_time = pmic_ldo_vbif28_enable_time, */
};

/* Regulator vufs18 ops */
static struct regulator_ops pmic_ldo_vufs18_ops = {
	.enable = pmic_ldo_vufs18_enable,
	.disable = pmic_ldo_vufs18_disable,
	.is_enabled = pmic_ldo_vufs18_is_enabled,
	/* .enable_time = pmic_ldo_vufs18_enable_time, */
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

/* Regulator vldo28_tp ops */
static struct regulator_ops pmic_ldo_vldo28_tp_ops = {
	.enable = pmic_ldo_vldo28_tp_enable,
	.disable = pmic_ldo_vldo28_tp_disable,
	.is_enabled = pmic_ldo_vldo28_tp_is_enabled,
	.get_voltage_sel = pmic_ldo_vldo28_tp_get_voltage_sel,
	.set_voltage_sel = pmic_ldo_vldo28_tp_set_voltage_sel,
	.list_voltage = pmic_ldo_vldo28_tp_list_voltage,
	/* .enable_time = pmic_ldo_vldo28_tp_enable_time, */
};

/* Regulator vproc11 ops */
static struct regulator_ops pmic_buck_vproc11_ops = {
	.enable = pmic_buck_vproc11_enable,
	.disable = pmic_buck_vproc11_disable,
	.is_enabled = pmic_buck_vproc11_is_enabled,
	.get_voltage_sel = pmic_buck_vproc11_get_voltage_sel,
	.set_voltage_sel = pmic_buck_vproc11_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	/* .enable_time = pmic_buck_vproc11_enable_time, */
};

/* Regulator vxo18 ops */
static struct regulator_ops pmic_ldo_vxo18_ops = {
	.enable = pmic_ldo_vxo18_enable,
	.disable = pmic_ldo_vxo18_disable,
	.is_enabled = pmic_ldo_vxo18_is_enabled,
	/* .enable_time = pmic_ldo_vxo18_enable_time, */
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
	struct mtk_regulator *mreg;
	unsigned int ret_value = 0;

	mreg = container_of(attr, struct mtk_regulator, en_att);

	if (mreg->da_en_cb != NULL)
		ret_value = (mreg->da_en_cb)();
	else
		ret_value = 9999;

	PMICLOG("[EM] %s_STATUS : %d\n", mreg->desc.name, ret_value);
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

	PMICLOG("[EM] %s_VOLTAGE : %d\n", mreg->desc.name, ret_value);
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
struct mtk_regulator mt_bucks[] = {
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vdram2, buck, 400000, 1193750,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vs1, buck, 1200000, 2787500,
	12500, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vdram1, buck, 518750, 1312500,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vmodem, buck, 400000, 1193750,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vcore, buck, 406250, 1200000,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vs2, buck, 1200000, 2787500,
	12500, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vpa, buck, 500000, 3650000,
	50000, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vproc12, buck, 406250, 1200000,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vgpu, buck, 406250, 1200000,
	6250, 0, BUCK_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(vproc11, buck, 406250, 1200000,
	6250, 0, BUCK_VOL_EN, 1),

};


/* Regulator: LDO */
#define LDO_EN	REGULATOR_CHANGE_STATUS
#define LDO_VOL REGULATOR_CHANGE_VOLTAGE
#define LDO_VOL_EN (REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE)
struct mtk_regulator mt_ldos[] = {
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamd1, ldo, vcamd1_voltages,
	vcamd1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim1, ldo, vsim1_voltages,
	vsim1_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vgp, ldo, vgp_voltages,
	vgp_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vusb33, ldo, 3000000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf12, ldo, 1200000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcamio, ldo, 1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn18, ldo, 1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vfe28, ldo, 2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18_2, ldo, 1810000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(va18, ldo, 1800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vcn28, ldo, 2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmipi, ldo, vmipi_voltages,
	vmipi_idx, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_gpu, ldo, 518750, 1312500,
	6250, 0, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_core, ldo, 518750, 1312500,
	6250, 0, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_proc, ldo, 518750, 1312500,
	6250, 0, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vxo22, ldo, vxo22_voltages,
	vxo22_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vrf18_1, ldo, 1810000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcamd2, ldo, vcamd2_voltages,
	vcamd2_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmch, ldo, vmch_voltages,
	vmch_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama1, ldo, vcama1_voltages,
	vcama1_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vtcxo24, ldo, 2300000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio28, ldo, 2800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vemc, ldo, vemc_voltages,
	vemc_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(va12, ldo, 1200000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(va10, ldo, va10_voltages,
	va10_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vgp2, ldo, vgp2_voltages,
	vgp2_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vio18, ldo, 1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_bt, ldo, vcn33_bt_voltages,
	vcn33_bt_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcn33_wifi, ldo, vcn33_wifi_voltages,
	vcn33_wifi_idx, LDO_VOL_EN, 1),
	REGULAR_VOLTAGE_REGULATOR_LDO_GEN(vsram_md, ldo, 518750, 1312500,
	6250, 0, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vbif28, ldo, 2800000, LDO_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vufs18, ldo, 1800000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vcama2, ldo, vcama2_voltages,
	vcama2_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vmc, ldo, vmc_voltages,
	vmc_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28, ldo, vldo28_voltages,
	vldo28_idx, LDO_VOL_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vldo28_tp, ldo, vldo28_tp_voltages,
	vldo28_tp_idx, LDO_VOL_EN, 1),
	FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(vxo18, ldo, 1810000, LDO_EN, 1),
	NON_REGULAR_VOLTAGE_REGULATOR_GEN(vsim2, ldo, vsim2_voltages,
	vsim2_idx, LDO_VOL_EN, 1),
};



int mt_ldos_size = ARRAY_SIZE(mt_ldos);
int mt_bucks_size = ARRAY_SIZE(mt_bucks);
/* -------Code Gen End-------*/

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY

#define PMIC_REGULATOR_LDO_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_ldos[MT6355_POWER_LDO_##_id],	\
	}

#define PMIC_REGULATOR_BUCK_OF_MATCH(_name, _id)			\
	{						\
		.name = #_name,						\
		.driver_data = &mt_bucks[MT6355_POWER_BUCK_##_id],	\
	}

struct of_regulator_match pmic_regulator_buck_matches[] = {
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vdram2, VDRAM2),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs1, VS1),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vdram1, VDRAM1),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vmodem, VMODEM),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vcore, VCORE),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vs2, VS2),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vpa, VPA),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc12, VPROC12),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vgpu, VGPU),
	PMIC_REGULATOR_BUCK_OF_MATCH(buck_vproc11, VPROC11),

};

struct of_regulator_match pmic_regulator_ldo_matches[] = {
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamd1, VCAMD1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim1, VSIM1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vgp, VGP),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vusb33, VUSB33),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf12, VRF12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamio, VCAMIO),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn18, VCN18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vfe28, VFE28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf18_2, VRF18_2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va18, VA18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn28, VCN28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmipi, VMIPI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_gpu, VSRAM_GPU),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_core, VSRAM_CORE),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_proc, VSRAM_PROC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vxo22, VXO22),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vrf18_1, VRF18_1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcamd2, VCAMD2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmch, VMCH),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcama1, VCAMA1),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vtcxo24, VTCXO24),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio28, VIO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vemc, VEMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va12, VA12),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_va10, VA10),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vgp2, VGP2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vio18, VIO18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_bt, VCN33_BT),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcn33_wifi, VCN33_WIFI),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsram_md, VSRAM_MD),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vbif28, VBIF28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vufs18, VUFS18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vcama2, VCAMA2),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vmc, VMC),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vldo28, VLDO28),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vldo28_tp, VLDO28_TP),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vxo18, VXO18),
	PMIC_REGULATOR_LDO_OF_MATCH(ldo_vsim2, VSIM2),
};

int pmic_regulator_ldo_matches_size = ARRAY_SIZE(pmic_regulator_ldo_matches);
int pmic_regulator_buck_matches_size = ARRAY_SIZE(pmic_regulator_buck_matches);

#endif				/* End of #ifdef CONFIG_OF */
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
