// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
/* necessary header */
#include "mtk_pe2.h"
#include "charger_class.h"

/* dependent on platform */
#include "mtk_charger.h"

struct pe20_hal {
	struct charger_device *chg1_dev;
	struct charger_device *chg2_dev;
};

int pe2_hal_init_hardware(struct chg_alg_device *alg)
{
	struct mtk_pe20 *pe2;
	struct pe20_hal *hal;

	pr_notice("%s\n", __func__);
	if (alg == NULL) {
		pr_notice("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	pe2 = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (hal == NULL) {
		hal = devm_kzalloc(&pe2->pdev->dev, sizeof(*hal), GFP_KERNEL);
		if (!hal)
			return -ENOMEM;
		chg_alg_dev_set_drv_hal_data(alg, hal);
	}

	hal->chg1_dev = get_charger_by_name("primary_chg");
	if (hal->chg1_dev)
		pr_notice("%s: Found primary charger\n", __func__);
	else {
		pr_notice("%s: Error : can't find primary charger\n",
			__func__);
		return -ENODEV;
	}

	hal->chg2_dev = get_charger_by_name("secondary_chg");
	if (hal->chg2_dev)
		pr_notice("%s: Found secondary charger\n", __func__);
	else
		pr_notice("%s: Error : can't find secondary charger\n",
			__func__);

	return 0;
}

int pe2_hal_set_efficiency_table(struct chg_alg_device *alg)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_set_pe20_efficiency_table(hal->chg1_dev);
	return 0;
}

int pe2_hal_get_uisoc(struct chg_alg_device *alg)
{
	union power_supply_propval prop;
	struct power_supply *bat_psy = NULL;
	int ret;
	struct mtk_pe20 *pe2;

	if (alg == NULL)
		return -EINVAL;

	pe2 = dev_get_drvdata(&alg->dev);
	bat_psy = devm_power_supply_get_by_phandle(&pe2->pdev->dev,
						       "gauge");
	if (IS_ERR(bat_psy)) {
		pr_notice("%s Couldn't get bat_psy\n", __func__);
		ret = 50;
	} else {
		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
		ret = prop.intval;
	}

	pr_notice("%s:%d\n", __func__,
		ret);
	return ret;
}

int pe2_hal_get_charger_type(struct chg_alg_device *alg)
{
	struct mtk_charger *info;
	struct power_supply *chg_psy = NULL;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
	} else {
		info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
		ret = info->chr_type;
	}

	pr_notice("%s type:%d\n", __func__, ret);
	return info->chr_type;
}

int pe2_hal_set_mivr(struct chg_alg_device *alg, enum chg_idx chgidx, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;
	struct mtk_pe20 *pe2;
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pe2 = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = charger_dev_set_mivr(hal->chg1_dev, uV);
	if (ret < 0)
		pr_notice("%s: failed, ret = %d\n", __func__, ret);

	if (hal->chg2_dev) {
		charger_dev_is_chip_enabled(hal->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(hal->chg2_dev,
				uV + pe2->pe2_slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}

	return ret;
}

int pe2_hal_get_charger_cnt(struct chg_alg_device *alg)
{
	struct pe20_hal *hal;
	int cnt = 0;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (hal->chg1_dev != NULL)
		cnt++;
	if (hal->chg2_dev != NULL)
		cnt++;

	return cnt;
}

bool pe2_hal_is_chip_enable(struct chg_alg_device *alg, enum chg_idx chgidx)
{
	struct pe20_hal *hal;
	bool is_chip_enable;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_is_chip_enabled(hal->chg1_dev,
		&is_chip_enable);
	else if (chgidx == CHG2)
		charger_dev_is_chip_enabled(hal->chg2_dev,
		&is_chip_enable);

	return is_chip_enable;
}

int pe2_hal_enable_charger(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1  && hal->chg1_dev != NULL)
		ret = charger_dev_enable(hal->chg1_dev, en);
	else if (chgidx == CHG2  && hal->chg2_dev != NULL)
		ret = charger_dev_enable(hal->chg2_dev, en);

	pr_notice("%s idx:%d %d\n", __func__, chgidx, en);
	return 0;
}

int pe2_hal_is_charger_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *en)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1  && hal->chg1_dev != NULL)
		ret = charger_dev_is_enabled(hal->chg1_dev, en);
	else if (chgidx == CHG2  && hal->chg2_dev != NULL)
		ret = charger_dev_is_enabled(hal->chg2_dev, en);

	pr_notice("%s idx:%d %d\n", __func__, chgidx, *en);
	return 0;
}

int pe2_hal_reset_ta(struct chg_alg_device *alg, enum chg_idx chgidx)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = charger_dev_reset_ta(hal->chg1_dev);
	if (ret != 0) {
		pr_notice("%s: fail ,ret=%d\n", __func__, ret);
		return -1;
	}
	return 0;
}

static int get_pmic_vbus(int *vchr)
{
	union power_supply_propval prop;
	static struct power_supply *chg_psy;
	int ret;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk_charger_type");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		ret = -1;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	}
	*vchr = prop.intval * 1000;

	pr_notice("%s vbus:%d\n", __func__,
		prop.intval);
	return ret;
}

int pe2_hal_get_vbus(struct chg_alg_device *alg)
{
	int ret = 0;
	int vchr = 0;
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = charger_dev_get_vbus(hal->chg1_dev, &vchr);
	if (ret < 0) {
		ret = get_pmic_vbus(&vchr);
		if (ret < 0)
			pr_notice("%s: get vbus failed: %d\n", __func__, ret);
	}

	return vchr;
}

int pe2_hal_get_vbat(struct chg_alg_device *alg)
{
	union power_supply_propval prop;
	struct power_supply *bat_psy = NULL;
	int ret;
	struct mtk_pe20 *pe2;

	if (alg == NULL)
		return -EINVAL;

	pe2 = dev_get_drvdata(&alg->dev);

	bat_psy = devm_power_supply_get_by_phandle(&pe2->pdev->dev,
						       "gauge");
	if (IS_ERR(bat_psy)) {
		pr_notice("%s Couldn't get bat_psy\n", __func__);
		ret = 3999;
	} else {
		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
		ret = prop.intval;
	}

	pr_debug("%s:%d\n", __func__,
		ret);
	return ret;
}

int pe2_hal_get_ibat(struct chg_alg_device *alg)
{
	union power_supply_propval prop;
	struct power_supply *bat_psy = NULL;
	int ret;
	struct mtk_pe20 *pe2;

	if (alg == NULL)
		return -EINVAL;

	pe2 = dev_get_drvdata(&alg->dev);
	bat_psy = devm_power_supply_get_by_phandle(&pe2->pdev->dev,
						       "gauge");
	if (IS_ERR(bat_psy)) {
		pr_notice("%s Couldn't get bat_psy\n", __func__);
		ret = 0;
	} else {
		ret = power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
		ret = prop.intval;
	}

	pr_debug("%s:%d\n", __func__,
		ret);
	return ret;
}


int pe2_hal_enable_cable_drop_comp(struct chg_alg_device *alg,
	bool en)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	return charger_dev_enable_cable_drop_comp(hal->chg1_dev, false);
}

int pe2_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_set_constant_voltage(hal->chg1_dev,
								uv);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_set_constant_voltage(hal->chg2_dev,
								uv);
	return 0;
}

int pe2_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_set_charging_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_set_charging_current(hal->chg2_dev, ua);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, ua);

	return 0;
}

int pe2_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_get_charging_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_get_charging_current(hal->chg2_dev, ua);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, ua);

	return 0;
}

int pe2_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_set_input_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_set_input_current(hal->chg2_dev, ua);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, ua);

	return 0;
}

int pe2_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		charger_dev_get_mivr_state(hal->chg1_dev, in_loop);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL)
		charger_dev_get_mivr_state(hal->chg2_dev, in_loop);

	return 0;
}

int pe2_hal_send_ta20_current_pattern(struct chg_alg_device *alg,
					  u32 uV)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);
	return charger_dev_send_ta20_current_pattern(hal->chg1_dev,
			uV);
}

int pe2_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable)
{
	mtk_chg_enable_vbus_ovp(enable);

	return 0;
}

int pe2_hal_charger_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_enable_chip(hal->chg1_dev, enable);
	else if (chgidx == CHG2 && hal->chg2_dev != NULL) {
		pr_notice("%s idx:%d %d test\n", __func__, chgidx, enable);
		ret = charger_dev_enable_chip(hal->chg2_dev, enable);
	}
	pr_notice("%s idx:%d %d %d\n", __func__, chgidx, enable,
		hal->chg2_dev != NULL);
	return 0;
}

int pe2_hal_set_eoc_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uA)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_set_eoc_current(hal->chg1_dev, uA);
	if (chgidx == CHG2 && hal->chg2_dev != NULL)
		ret = charger_dev_set_eoc_current(hal->chg2_dev, uA);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, uA);
	return 0;
}

int pe2_hal_enable_termination(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_enable_termination(hal->chg1_dev, enable);
	if (chgidx == CHG2 && hal->chg2_dev != NULL)
		ret = charger_dev_enable_termination(hal->chg2_dev, enable);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, enable);
	return 0;
}

int pe2_hal_get_min_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_get_min_charging_current(hal->chg1_dev, uA);
	if (chgidx == CHG2 && hal->chg2_dev != NULL)
		ret = charger_dev_get_min_charging_current(hal->chg2_dev, uA);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, *uA);
	return 0;
}

int pe2_hal_get_min_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pe20_hal *hal;
	int ret;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1 && hal->chg1_dev != NULL)
		ret = charger_dev_get_min_input_current(hal->chg1_dev, uA);
	if (chgidx == CHG2 && hal->chg2_dev != NULL)
		ret = charger_dev_get_min_input_current(hal->chg2_dev, uA);
	pr_notice("%s idx:%d %d\n", __func__, chgidx, *uA);
	return 0;
}

int pe2_hal_safety_check(struct chg_alg_device *alg,
	int ieoc)
{
	struct pe20_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_safety_check(hal->chg1_dev,
				 ieoc);
	return 0;
}



