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
#include "mtk_pd.h"
#include "charger_class.h"
#include "adapter_class.h"

/* dependent on platform */
#include "mtk_charger.h"

struct pd_hal {
	struct charger_device *chg1_dev;
	struct charger_device *chg2_dev;
	struct adapter_device *adapter;
};

int pd_hal_init_hardware(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	struct pd_hal *hal;


	pr_notice("%s\n", __func__);
	if (alg == NULL) {
		pr_notice("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	pd = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (hal == NULL) {
		hal = devm_kzalloc(&pd->pdev->dev, sizeof(*hal), GFP_KERNEL);
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

	hal->adapter = get_adapter_by_name("pd_adapter");
	if (hal->adapter)
		pr_notice("%s: Found pd adapter\n", __func__);
	else {
		pr_notice("%s: Error : can't find pd adapter\n",
			__func__);
		return -ENODEV;
	}

	return 0;
}

int pd_hal_is_pd_adapter_ready(struct chg_alg_device *alg)
{
	struct mtk_pd *pd;
	struct pd_hal *hal;
	int type;

	if (alg == NULL) {
		pr_notice("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	pd = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	type = adapter_dev_get_property(hal->adapter, PD_TYPE);

	pr_notice("%s type:%d\n", __func__, type);

	if (type == MTK_PD_CONNECT_PE_READY_SNK ||
		type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
		type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		return ALG_READY;
	else if (type == MTK_PD_CONNECT_TYPEC_ONLY_SNK)
		return ALG_TA_NOT_SUPPORT;
	return ALG_TA_CHECKING;
}

int pd_hal_get_adapter_cap(struct chg_alg_device *alg, struct pd_power_cap *cap)
{
	struct mtk_pd *pd;
	struct pd_hal *hal;
	struct adapter_power_cap acap = {0};
	int i, ret;

	if (alg == NULL) {
		pr_notice("%s: alg is null\n", __func__);
		return -EINVAL;
	}

	pd = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);

	ret = adapter_dev_get_cap(hal->adapter, MTK_PD, &acap);
	cap->selected_cap_idx = acap.selected_cap_idx;
	cap->nr = acap.nr;
	cap->pdp = acap.pdp;
	for (i = 0; i < 10; i++) {
		cap->pwr_limit[i] = acap.pwr_limit[i];
		cap->min_mv[i] = acap.min_mv[i];
		cap->max_mv[i] = acap.max_mv[i];
		cap->ma[i] = acap.ma[i];
		cap->maxwatt[i] = acap.maxwatt[i];
		cap->minwatt[i] = acap.minwatt[i];
		cap->type[i] = acap.type[i];
		cap->info[i] = acap.info[i];
	}
	return ret;
}

static int get_pmic_vbus(int *vchr)
{
	union power_supply_propval prop = {0};
	static struct power_supply *chg_psy;
	int ret;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk_charger_type");
	if (IS_ERR_OR_NULL(chg_psy)) {
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

int pd_hal_get_vbus(struct chg_alg_device *alg)
{
	int ret = 0;
	int vchr = 0;
	struct pd_hal *hal;

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

int pd_hal_get_ibus(struct chg_alg_device *alg, int *ibus)
{
	int ret = 0;
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;
	hal = chg_alg_dev_get_drv_hal_data(alg);
	ret = charger_dev_get_ibus(hal->chg1_dev, ibus);
	if (ret < 0)
		pr_notice("%s: get vbus failed: %d\n", __func__, ret);

	return ret;
}

int pd_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_get_mivr_state(hal->chg1_dev, in_loop);
	else if (chgidx == CHG2)
		charger_dev_get_mivr_state(hal->chg2_dev, in_loop);
	return 0;
}

int pd_hal_get_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int *mivr1)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	charger_dev_get_mivr(hal->chg1_dev, mivr1);

	if (chgidx == CHG1)
		charger_dev_get_mivr(hal->chg1_dev, mivr1);
	else if (chgidx == CHG2)
		charger_dev_get_mivr(hal->chg2_dev, mivr1);

	return 0;
}

int pd_hal_get_charger_cnt(struct chg_alg_device *alg)
{
	struct pd_hal *hal;
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

bool pd_hal_is_chip_enable(struct chg_alg_device *alg, enum chg_idx chgidx)
{
	struct pd_hal *hal;
	bool is_chip_enable = false;

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

int pd_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable)
{
	//wy fix me
	mtk_chg_enable_vbus_ovp(enable);

	return 0;
}

int pd_hal_set_mivr(struct chg_alg_device *alg, enum chg_idx chgidx, int uV)
{
	int ret = 0;
	bool chg2_chip_enabled = false;
	struct mtk_pd *pd;
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pd = dev_get_drvdata(&alg->dev);
	hal = chg_alg_dev_get_drv_hal_data(alg);

	pr_notice("%s: %d %d\n", __func__, chgidx, uV);

	ret = charger_dev_set_mivr(hal->chg1_dev, uV);
	if (ret < 0)
		pr_notice("%s: failed, ret = %d\n", __func__, ret);

	if (hal->chg2_dev) {
		charger_dev_is_chip_enabled(hal->chg2_dev,
			&chg2_chip_enabled);
		if (chg2_chip_enabled) {
			ret = charger_dev_set_mivr(hal->chg2_dev,
				uV + pd->slave_mivr_diff);
			if (ret < 0)
				pr_info("%s: chg2 failed, ret = %d\n", __func__,
					ret);
		}
	}
	return ret;
}

int pd_hal_get_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	if (chgidx == CHG1)
		charger_dev_get_input_current(hal->chg1_dev,
		ua);
	else if (chgidx == CHG2)
		charger_dev_get_input_current(hal->chg2_dev,
		ua);

	return 0;
}

int pd_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_set_input_current(hal->chg1_dev, ua);

	if (chgidx == CHG1)
		charger_dev_set_input_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2)
		charger_dev_set_input_current(hal->chg2_dev, ua);

	return 0;
}

int pd_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);

	if (chgidx == CHG1)
		charger_dev_set_constant_voltage(hal->chg1_dev,
			uv);
	else if (chgidx == CHG2)
		charger_dev_set_constant_voltage(hal->chg2_dev,
			uv);

	return 0;
}

int pd_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_set_charging_current(hal->chg1_dev, ua);

	if (chgidx == CHG1)
		charger_dev_set_charging_current(hal->chg1_dev, ua);
	else if (chgidx == CHG2)
		charger_dev_set_charging_current(hal->chg2_dev, ua);

	return 0;
}

int pd_hal_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	pr_notice("%s %d %d\n", __func__, mV, mA);
	hal = chg_alg_dev_get_drv_hal_data(alg);
	return adapter_dev_set_cap(hal->adapter, MTK_PD, mV, mA);
}

int pd_hal_is_charger_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *en)
{
	struct pd_hal *hal;
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

int pd_hal_enable_charger(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en)
{
	struct pd_hal *hal;
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


int pd_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua)
{
	struct pd_hal *hal;

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

int pd_hal_get_min_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pd_hal *hal;
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

int pd_hal_get_min_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA)
{
	struct pd_hal *hal;
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

int pd_hal_safety_check(struct chg_alg_device *alg,
	int ieoc)
{
	struct pd_hal *hal;

	if (alg == NULL)
		return -EINVAL;

	hal = chg_alg_dev_get_drv_hal_data(alg);
	charger_dev_safety_check(hal->chg1_dev,
				 ieoc);
	return 0;
}

int pd_hal_set_eoc_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uA)
{
	struct pd_hal *hal;
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

int pd_hal_enable_termination(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pd_hal *hal;
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

int pd_hal_charger_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable)
{
	struct pd_hal *hal;
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

int pd_hal_get_uisoc(struct chg_alg_device *alg)
{
	union power_supply_propval prop = {0};
	struct power_supply *bat_psy = NULL;
	int ret;
	struct mtk_pd *pd;

	if (alg == NULL)
		return -EINVAL;

	pd = dev_get_drvdata(&alg->dev);
	bat_psy = pd->bat_psy;

	if (IS_ERR_OR_NULL(bat_psy)) {
		pr_notice("%s retry to get bat_psy\n", __func__);
		bat_psy = devm_power_supply_get_by_phandle(&pd->pdev->dev, "gauge");
		pd->bat_psy = bat_psy;
	}

	if (IS_ERR_OR_NULL(bat_psy)) {
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


