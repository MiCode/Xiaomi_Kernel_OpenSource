// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "governor.h"

#include "apusys_power_user.h"
#include "apusys_power.h"
#include "apu_common.h"
#include "apu_log.h"
#include "apu_of.h"
#include "apu_plat.h"
#include "apu_dbg.h"

int apu_device_power_suspend(enum DVFS_USER user, int is_suspend)
{
	int ret = 0;
	struct apu_dev *ad = NULL;
	enum DVFS_USER ori_usr = user;

	if (user == EDMA || user == EDMA2 || user == REVISER)
		user = APUCB;

	ad = apu_find_device(user);
	if (IS_ERR_OR_NULL(ad)) {
		apower_err(ad->dev, "[%s] called by no-exist user (%d)\n",
				__func__, ori_usr);
		ret = -EFAULT;
		goto out;
	}
	apower_info(ad->dev, "[%s] called by %s\n",
		__func__, apu_dev_string(ori_usr));

	ret = pm_runtime_put_sync(ad->dev);
	if (ret)
		apower_err(ad->dev, "[%s] suspend fail, ret %d\n", __func__, ret);

out:
	return ret;
}
EXPORT_SYMBOL(apu_device_power_suspend);

int apu_device_power_on(enum DVFS_USER user)
{
	int ret = 0;
	struct apu_dev *ad = NULL;
	enum DVFS_USER ori_usr = user;

	if (user == EDMA || user == EDMA2 || user == REVISER)
		user = APUCB;

	ad = apu_find_device(user);
	if (IS_ERR_OR_NULL(ad)) {
		apower_err(ad->dev, "[%s] called by no-exist user (%d)\n",
				__func__, ori_usr);
		ret = -EFAULT;
		goto out;
	}
	apower_info(ad->dev, "[%s] called by %s\n", __func__, apu_dev_string(ori_usr));
	ret = pm_runtime_get_sync(ad->dev);
	/* Ignore suppliers with disabled runtime PM. */
	if (ret < 0 && ret != -EACCES) {
		apower_err(ad->dev, "[%s] fail, ret %d\n", __func__, ret);
		pm_runtime_put_noidle(ad->dev);
		goto out;
	}

	return 0;
out:
	return ret;
}
EXPORT_SYMBOL(apu_device_power_on);

int apu_device_power_off(enum DVFS_USER user)
{
	return apu_device_power_suspend(user, 0);
}
EXPORT_SYMBOL(apu_device_power_off);

void apu_device_set_opp(enum DVFS_USER user, uint8_t opp)
{
	struct apu_dev *ad;
	int freq, n_freq;
	struct apu_gov_data *pgov_data;

	ad = apu_find_device(user);
	if (IS_ERR(ad)) {
		pr_info("[%s] %s not support\n", __func__, apu_dev_string(user));
		return;
	}

	if (!pm_runtime_active(ad->dev)) {
		apower_warn(ad->dev, "[%s] already power off\n", __func__);
		return;
	}

	if (apupw_dbg_get_fixopp())
		return;

	/* restrict opp in current spec */
	if (opp < 0)
		opp = 0;
	if (opp > ad->df->profile->max_state - 1)
		opp = ad->df->profile->max_state - 1;

	/* calculate current freq and freq of boost user wants */
	freq = ad->aclk->ops->get_rate(ad->aclk);
	n_freq = apu_opp2freq(ad, opp);
	if (round_Mhz(freq, n_freq))
		return;
	apower_info(ad->dev, "[%s] set opp %d, cur/next %dMhz/%dMhz",
		    __func__, opp, TOMHZ(freq), TOMHZ(n_freq));

	/* update opp in governor data */
	pgov_data = ad->df->data;
	mutex_lock_nested(&ad->df->lock, pgov_data->depth);
	pgov_data->req.value = opp;
	list_sort(NULL, &pgov_data->head, apu_cmp);
	mutex_unlock(&ad->df->lock);
}
EXPORT_SYMBOL(apu_device_set_opp);


uint64_t apu_get_power_info(int force)
{
	if (!force)
		queue_delayed_work(pm_wq, &pw_info_work, msecs_to_jiffies(1));
	else
		apupw_dbg_power_info(NULL);
	return 0;
}
EXPORT_SYMBOL(apu_get_power_info);

uint8_t apusys_boost_value_to_opp(enum DVFS_USER user, uint8_t boost)
{
	struct apu_dev *ad;

	ad = apu_find_device(user);
	if (IS_ERR(ad))
		return PTR_ERR_OR_ZERO(ad);

	return (uint8_t)apu_boost2opp(ad, boost);
}
EXPORT_SYMBOL(apusys_boost_value_to_opp);

ulong apusys_opp_to_freq(enum DVFS_USER user, uint8_t opp)
{
	struct apu_dev *ad;

	ad = apu_find_device(user);
	if (IS_ERR(ad))
		return PTR_ERR_OR_ZERO(ad);

	return apu_opp2freq(ad, opp);
}
EXPORT_SYMBOL(apusys_opp_to_freq);

int8_t apusys_get_ceiling_opp(enum DVFS_USER user)
{
	struct apu_dev *ad;
	ulong rate = 0;

	ad = apu_find_device(user);
	if (IS_ERR(ad) || IS_ERR_OR_NULL(ad->aclk))
		return PTR_ERR_OR_ZERO(ad);

	rate = ad->aclk->ops->get_rate(ad->aclk);

	return apu_freq2opp(ad, rate);
}
EXPORT_SYMBOL(apusys_get_ceiling_opp);

bool apu_get_power_on_status(enum DVFS_USER user)
{
	struct apu_dev *ad;

	if (user == EDMA || user == EDMA2 || user == REVISER)
		user = APUCB;

	ad = apu_find_device(user);
	if (IS_ERR(ad))
		return PTR_ERR_OR_ZERO(ad);

	return !pm_runtime_status_suspended(ad->dev);
}
EXPORT_SYMBOL(apu_get_power_on_status);

int8_t apusys_get_opp(enum DVFS_USER user)
{
	struct apu_dev *ad;
	int freq = 0;

	ad = apu_find_device(user);
	if (IS_ERR(ad))
		return PTR_ERR_OR_ZERO(ad);

	freq = ad->aclk->ops->get_rate(ad->aclk);
	return apu_freq2opp(ad, freq);
}
EXPORT_SYMBOL(apusys_get_opp);

void apu_power_reg_dump(void)
{
	//TODO
}
EXPORT_SYMBOL(apu_power_reg_dump);

bool apusys_power_check(void)
{
/* TODO
 *	char *pwr_ptr;
 *	bool pwr_status = true;
 *
 *	pwr_ptr = strstr(saved_command_line,
 *				"apusys_status=normal");
 *	if (pwr_ptr == 0) {
 *		pwr_status = false;
 *		pr_info("apusys power disable !!, pwr_status=%d\n",
 *			pwr_status);
 *	}
 *	pr_info("apusys power check, pwr_status=%d\n",
 *			pwr_status);
 *	return pwr_status;
 */
	return true;
}
EXPORT_SYMBOL(apusys_power_check);

void apu_qos_set_vcore(int opp)
{
	struct apu_dev *ad;
	struct apu_gov_data *pgov_data;
	int freq, n_freq;

	/* get APUVCORE devfreq device */
	ad = apu_find_device(APUCORE);
	if (IS_ERR(ad))
		return;

	if (apupw_dbg_get_fixopp())
		return;

	/*
	 * comparing current freq with voted freq,
	 * if the same, just return.
	 */
	freq = ad->aclk->ops->get_rate(ad->aclk);
	n_freq = apu_opp2freq(ad, opp);
	if (round_Mhz(freq, n_freq))
		return;

	apower_info(ad->dev, "[%s] set opp %d, cur/next %dMhz/%dMhz",
		    __func__, opp, TOMHZ(freq), TOMHZ(n_freq));

	/* get governor data and synchronize calling update_devfreq */
	pgov_data = ad->df->data;
	mutex_lock(&ad->df->lock);
	pgov_data->req.value = opp;
	list_sort(NULL, &pgov_data->head, apu_cmp);
	update_devfreq(ad->df);
	mutex_unlock(&ad->df->lock);
}
EXPORT_SYMBOL(apu_qos_set_vcore);


int apu_power_device_register(enum DVFS_USER user, struct platform_device *pdev)
{
	return 0;
}
EXPORT_SYMBOL(apu_power_device_register);

void apu_power_device_unregister(enum DVFS_USER user)
{
	pr_debug("[%s] user %d\n", user);
}
EXPORT_SYMBOL(apu_power_device_unregister);

int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
				       void (*power_on_callback)(void *para),
				       void (*power_off_callback)(void *para))
{
	return apu_power_cb_register(user, power_on_callback, power_off_callback);
}
EXPORT_SYMBOL(apu_power_callback_device_register);

void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user)
{
	return apu_power_cb_unregister(user);
}
EXPORT_SYMBOL(apu_power_callback_device_unregister);

static int apusys_power_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	/* initial run time power management */
	pm_runtime_enable(dev);

	/* Enumerate child at last, since child need parent's devfreq */
	err = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (err) {
		dev_info(dev, "%s populate fail\n", __func__);
		return err;
	}
	return err;
}

static int apusys_power_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s %d\n", __func__, __LINE__);
	of_platform_depopulate(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id apusys_power_of_match[] = {
	{ .compatible = "mediatek,apusys_power" },
	{ },
};

MODULE_DEVICE_TABLE(of, apusys_power_of_match);
struct platform_driver apusys_power_driver = {
	.probe	= apusys_power_probe,
	.remove	= apusys_power_remove,
	.driver = {
		.name = "mediatek,apusys_power",
		.of_match_table = apusys_power_of_match,
	},
};

int apu_power_init(void)
{
	int ret = 0;

	ret = devfreq_add_governor(&agov_passive);
	if (ret)
		return ret;
	ret = devfreq_add_governor(&agov_userspace);
	if (ret)
		return ret;
	ret = devfreq_add_governor(&agov_constrain);
	if (ret)
		return ret;
	ret = devfreq_add_governor(&agov_passive_pe);
	if (ret)
		return ret;
	ret = platform_driver_register(&apu_rpc_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&core_devfreq_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&con_devfreq_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&iommu_devfreq_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&apu_cb_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&vpu_devfreq_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&mdla_devfreq_driver);
	if (ret)
		return ret;
	ret = platform_driver_register(&apusys_power_driver);
	if (ret)
		return ret;
	return 0;
}

void apu_power_exit(void)
{
	int ret = 0;

	platform_driver_unregister(&mdla_devfreq_driver);
	platform_driver_unregister(&vpu_devfreq_driver);
	platform_driver_unregister(&apu_cb_driver);
	platform_driver_unregister(&apusys_power_driver);
	platform_driver_unregister(&iommu_devfreq_driver);
	platform_driver_unregister(&con_devfreq_driver);
	platform_driver_unregister(&core_devfreq_driver);
	platform_driver_unregister(&apu_rpc_driver);

	ret = devfreq_remove_governor(&agov_constrain);
	if (ret)
		pr_info("[%s] failed remove gov %s %d\n",
			__func__, agov_constrain.name, ret);

	ret = devfreq_remove_governor(&agov_passive);
	if (ret)
		pr_info("[%s] failed remove gov %s %d\n",
			__func__, agov_passive.name, ret);

	ret = devfreq_remove_governor(&agov_userspace);
	if (ret)
		pr_info("[%s] failed remove gov %s %d\n",
			__func__, agov_userspace.name, ret);

	ret = devfreq_remove_governor(&agov_passive_pe);
	if (ret)
		pr_info("[%s] failed remove gov %s %d\n",
			__func__, agov_passive_pe.name, ret);
}
