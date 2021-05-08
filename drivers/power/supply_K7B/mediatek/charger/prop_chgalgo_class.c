/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <mt-plat/prop_chgalgo_class.h>

#define PROP_CHGALGO_CLASS_VERSION	"1.0.7_G"

#define to_pca_device(obj) container_of(obj, struct prop_chgalgo_device, dev)

static const char *pca_notify_evt_name[PCA_NOTIEVT_MAX] = {
	"PCA_NOTIEVT_DETACH",
	"PCA_NOTIEVT_HARDRESET",
	"PCA_NOTIEVT_VBUSOVP",
	"PCA_NOTIEVT_IBUSOCP",
	"PCA_NOTIEVT_IBUSUCP_FALL",
	"PCA_NOTIEVT_VBATOVP",
	"PCA_NOTIEVT_IBATOCP",
	"PCA_NOTIEVT_VOUTOVP",
	"PCA_NOTIEVT_VDROVP",
	"PCA_NOTIEVT_VBATOVP_ALARM",
	"PCA_NOTIEVT_VBUSOVP_ALARM",
	"PCA_NOTIEVT_ALGO_STOP",
};

static inline int pca_check_devtype(struct prop_chgalgo_device *pca,
				    enum prop_chgalgo_dev_type type)
{
	if (!pca)
		return -ENODEV;
	if (prop_chgalgo_get_devtype(pca) != type) {
		PCA_ERR("%s type error\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static inline bool pca_check_devtype_bool(struct prop_chgalgo_device *pca,
					  enum prop_chgalgo_dev_type type)
{
	if (!pca)
		return false;
	if (prop_chgalgo_get_devtype(pca) != type) {
		PCA_ERR("%s type error\n", __func__);
		return false;
	}
	return true;
}

static struct class *pca_class;
static struct device_type pca_dev_type;

static ssize_t pca_show_property(struct device *dev,
				 struct device_attribute *attr, char *buf);
static ssize_t pca_store_property(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

#define PCA_DEVICE_ATTR(_name, _mode)					\
{									\
	.attr = { .name = #_name, .mode = _mode },			\
	.show = pca_show_property,					\
	.store = pca_store_property,					\
}

static struct device_attribute pca_device_attributes[] = {
	PCA_DEVICE_ATTR(name, 0444),
	PCA_DEVICE_ATTR(type, 0444),
};

enum {
	PCA_DESC_NAME,
	PCA_DESC_TYPE,
};

static struct attribute *__pca_attrs[ARRAY_SIZE(pca_device_attributes) + 1];
static struct attribute_group pca_attr_group = {
	.attrs = __pca_attrs,
};

static const struct attribute_group *pca_attr_groups[] = {
	&pca_attr_group,
	NULL,
};

static const char *pca_devtype_name[PCA_DEVTYPE_MAX] = {
	"TA", "Charger", "Algorithm",
};

static ssize_t pca_show_property(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct prop_chgalgo_device *pca = to_pca_device(dev);
	const ptrdiff_t offset = attr - pca_device_attributes;

	dev_info(dev, "%s\n", __func__);
	switch (offset) {
	case PCA_DESC_NAME:
		snprintf(buf + strlen(buf), PAGE_SIZE, "%s\n", pca->desc->name);
		break;
	case PCA_DESC_TYPE:
		if (pca->desc->type > PCA_DEVTYPE_MAX) {
			snprintf(buf + strlen(buf), PAGE_SIZE, "Error Type\n");
			break;
		}
		snprintf(buf + strlen(buf), PAGE_SIZE, "%s\n",
			 pca_devtype_name[pca->desc->type]);
		break;
	default:
		break;
	}
	return strlen(buf);
}

static ssize_t pca_store_property(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	dev_info(dev, "%s\n", __func__);
	return count;
}

static void pca_device_release(struct device *dev)
{
	struct prop_chgalgo_device *pca = to_pca_device(dev);

	dev_info(dev, "%s\n", __func__);
	devm_kfree(dev, pca);
}

/*
 * TA released interface
 */
int prop_chgalgo_enable_ta_charging(struct prop_chgalgo_device *pca, bool en,
				    u32 mV, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->enable_charging)
		return -ENOTSUPP;
	return pca->ta_ops->enable_charging(pca, en, mV, mA);
}
EXPORT_SYMBOL(prop_chgalgo_enable_ta_charging);

int prop_chgalgo_set_ta_cap(struct prop_chgalgo_device *pca, u32 mV, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->set_cap)
		return -ENOTSUPP;
	return pca->ta_ops->set_cap(pca, mV, mA);
}
EXPORT_SYMBOL(prop_chgalgo_set_ta_cap);

int prop_chgalgo_get_ta_measure_cap(struct prop_chgalgo_device *pca, u32 *mV,
				    u32 *mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->get_measure_cap)
		return -ENOTSUPP;
	return pca->ta_ops->get_measure_cap(pca, mV, mA);
}
EXPORT_SYMBOL(prop_chgalgo_get_ta_measure_cap);

int prop_chgalgo_get_ta_temperature(struct prop_chgalgo_device *pca, int *temp)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->get_temperature)
		return -ENOTSUPP;
	return pca->ta_ops->get_temperature(pca, temp);
}
EXPORT_SYMBOL(prop_chgalgo_get_ta_temperature);

int prop_chgalgo_get_ta_status(struct prop_chgalgo_device *pca,
			       struct prop_chgalgo_ta_status *status)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->get_status)
		return -ENOTSUPP;
	return pca->ta_ops->get_status(pca, status);
}
EXPORT_SYMBOL(prop_chgalgo_get_ta_status);

int prop_chgalgo_is_ta_cc(struct prop_chgalgo_device *pca, bool *cc)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->is_cc)
		return -ENOTSUPP;
	return pca->ta_ops->is_cc(pca, cc);
}
EXPORT_SYMBOL(prop_chgalgo_is_ta_cc);

int prop_chgalgo_send_ta_hardreset(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->send_hardreset)
		return -ENOTSUPP;
	return pca->ta_ops->send_hardreset(pca);
}
EXPORT_SYMBOL(prop_chgalgo_send_ta_hardreset);

int prop_chgalgo_authenticate_ta(struct prop_chgalgo_device *pca,
				 struct prop_chgalgo_ta_auth_data *data)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->authenticate_ta)
		return -ENOTSUPP;
	return pca->ta_ops->authenticate_ta(pca, data);
}
EXPORT_SYMBOL(prop_chgalgo_authenticate_ta);

int prop_chgalgo_enable_ta_wdt(struct prop_chgalgo_device *pca, bool en)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->enable_wdt)
		return -ENOTSUPP;
	return pca->ta_ops->enable_wdt(pca, en);
}
EXPORT_SYMBOL(prop_chgalgo_enable_ta_wdt);

int prop_chgalgo_set_ta_wdt(struct prop_chgalgo_device *pca, u32 ms)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->set_wdt)
		return -ENOTSUPP;
	return pca->ta_ops->set_wdt(pca, ms);
}
EXPORT_SYMBOL(prop_chgalgo_set_ta_wdt);

int prop_chgalgo_sync_ta_volt(struct prop_chgalgo_device *pca, u32 vta)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_TA) < 0)
		return -EINVAL;
	if (!pca->ta_ops->sync_vta)
		return -ENOTSUPP;
	return pca->ta_ops->sync_vta(pca, vta);
}
EXPORT_SYMBOL(prop_chgalgo_sync_ta_volt);

/*
 * Charger released interface
 */
int prop_chgalgo_enable_power_path(struct prop_chgalgo_device *pca, bool en)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->enable_power_path)
		return -ENOTSUPP;
	return pca->chg_ops->enable_power_path(pca, en);
}
EXPORT_SYMBOL(prop_chgalgo_enable_power_path);

int prop_chgalgo_enable_charging(struct prop_chgalgo_device *pca, bool en)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->enable_charging)
		return -ENOTSUPP;
	return pca->chg_ops->enable_charging(pca, en);
}
EXPORT_SYMBOL(prop_chgalgo_enable_charging);

int prop_chgalgo_enable_chip(struct prop_chgalgo_device *pca, bool en)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->enable_chip)
		return -ENOTSUPP;
	return pca->chg_ops->enable_chip(pca, en);
}
EXPORT_SYMBOL(prop_chgalgo_enable_chip);

int prop_chgalgo_set_vbusovp(struct prop_chgalgo_device *pca, u32 mV)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_vbusovp)
		return -ENOTSUPP;
	return pca->chg_ops->set_vbusovp(pca, mV);
}
EXPORT_SYMBOL(prop_chgalgo_set_vbusovp);

int prop_chgalgo_set_ibusocp(struct prop_chgalgo_device *pca, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_ibusocp)
		return -ENOTSUPP;
	return pca->chg_ops->set_ibusocp(pca, mA);
}
EXPORT_SYMBOL(prop_chgalgo_set_ibusocp);

int prop_chgalgo_set_vbatovp(struct prop_chgalgo_device *pca, u32 mV)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_vbatovp)
		return -ENOTSUPP;
	return pca->chg_ops->set_vbatovp(pca, mV);
}
EXPORT_SYMBOL(prop_chgalgo_set_vbatovp);

int prop_chgalgo_set_vbatovp_alarm(struct prop_chgalgo_device *pca, u32 mV)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_vbatovp_alarm)
		return -ENOTSUPP;
	return pca->chg_ops->set_vbatovp_alarm(pca, mV);
}
EXPORT_SYMBOL(prop_chgalgo_set_vbatovp_alarm);

int prop_chgalgo_reset_vbatovp_alarm(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->reset_vbatovp_alarm)
		return -ENOTSUPP;
	return pca->chg_ops->reset_vbatovp_alarm(pca);
}
EXPORT_SYMBOL(prop_chgalgo_reset_vbatovp_alarm);

int prop_chgalgo_set_vbusovp_alarm(struct prop_chgalgo_device *pca, u32 mV)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_vbusovp_alarm)
		return -ENOTSUPP;
	return pca->chg_ops->set_vbusovp_alarm(pca, mV);
}
EXPORT_SYMBOL(prop_chgalgo_set_vbusovp_alarm);

int prop_chgalgo_reset_vbusovp_alarm(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->reset_vbusovp_alarm)
		return -ENOTSUPP;
	return pca->chg_ops->reset_vbusovp_alarm(pca);
}
EXPORT_SYMBOL(prop_chgalgo_reset_vbusovp_alarm);

int prop_chgalgo_set_ibatocp(struct prop_chgalgo_device *pca, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_ibatocp)
		return -ENOTSUPP;
	return pca->chg_ops->set_ibatocp(pca, mA);
}
EXPORT_SYMBOL(prop_chgalgo_set_ibatocp);

int prop_chgalgo_enable_hz(struct prop_chgalgo_device *pca, bool en)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->enable_hz)
		return -ENOTSUPP;
	return pca->chg_ops->enable_hz(pca, en);
}
EXPORT_SYMBOL(prop_chgalgo_enable_hz);

int prop_chgalgo_get_adc(struct prop_chgalgo_device *pca,
			 enum prop_chgalgo_adc_channel chan, int *min, int *max)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->get_adc)
		return -ENOTSUPP;
	return pca->chg_ops->get_adc(pca, chan, min, max);
}
EXPORT_SYMBOL(prop_chgalgo_get_adc);

int prop_chgalgo_get_soc(struct prop_chgalgo_device *pca, u32 *soc)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->get_soc)
		return -ENOTSUPP;
	return pca->chg_ops->get_soc(pca, soc);
}
EXPORT_SYMBOL(prop_chgalgo_get_soc);

int prop_chgalgo_set_ichg(struct prop_chgalgo_device *pca, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_ichg)
		return -ENOTSUPP;
	return pca->chg_ops->set_ichg(pca, mA);
}
EXPORT_SYMBOL(prop_chgalgo_set_ichg);

int prop_chgalgo_set_aicr(struct prop_chgalgo_device *pca, u32 mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->set_aicr)
		return -ENOTSUPP;
	return pca->chg_ops->set_aicr(pca, mA);
}
EXPORT_SYMBOL(prop_chgalgo_set_aicr);

int prop_chgalgo_is_vbuslowerr(struct prop_chgalgo_device *pca, bool *err)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->is_vbuslowerr)
		return -ENOTSUPP;
	return pca->chg_ops->is_vbuslowerr(pca, err);
}
EXPORT_SYMBOL(prop_chgalgo_is_vbuslowerr);

int prop_chgalgo_get_adc_accuracy(struct prop_chgalgo_device *pca,
				  enum prop_chgalgo_adc_channel chan, int *min,
				  int *max)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->get_adc_accuracy)
		return -ENOTSUPP;
	return pca->chg_ops->get_adc_accuracy(pca, chan, min, max);
}
EXPORT_SYMBOL(prop_chgalgo_get_adc_accuracy);

int prop_chgalgo_init_chip(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_CHARGER) < 0)
		return -EINVAL;
	if (!pca->chg_ops->init_chip)
		return -ENOTSUPP;
	return pca->chg_ops->init_chip(pca);
}
EXPORT_SYMBOL(prop_chgalgo_init_chip);

/*
 * Algorithm released interface
 */
int prop_chgalgo_init_algo(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->init_algo)
		return -ENOTSUPP;
	return pca->algo_ops->init_algo(pca);
}
EXPORT_SYMBOL(prop_chgalgo_init_algo);

bool prop_chgalgo_is_algo_ready(struct prop_chgalgo_device *pca)
{
	if (!pca_check_devtype_bool(pca, PCA_DEVTYPE_ALGO))
		return false;
	if (!pca->algo_ops->is_algo_ready)
		return false;
	return pca->algo_ops->is_algo_ready(pca);
}
EXPORT_SYMBOL(prop_chgalgo_is_algo_ready);

int prop_chgalgo_start_algo(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->start_algo)
		return -ENOTSUPP;
	return pca->algo_ops->start_algo(pca);
}
EXPORT_SYMBOL(prop_chgalgo_start_algo);

bool prop_chgalgo_is_algo_running(struct prop_chgalgo_device *pca)
{
	if (!pca_check_devtype_bool(pca, PCA_DEVTYPE_ALGO))
		return false;
	if (!pca->algo_ops->is_algo_running)
		return false;
	return pca->algo_ops->is_algo_running(pca);
}
EXPORT_SYMBOL(prop_chgalgo_is_algo_running);

int prop_chgalgo_plugout_reset(struct prop_chgalgo_device *pca)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->plugout_reset)
		return -ENOTSUPP;
	return pca->algo_ops->plugout_reset(pca);
}
EXPORT_SYMBOL(prop_chgalgo_plugout_reset);

int prop_chgalgo_stop_algo(struct prop_chgalgo_device *pca, bool rerun)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->stop_algo)
		return -ENOTSUPP;
	return pca->algo_ops->stop_algo(pca, rerun);
}
EXPORT_SYMBOL(prop_chgalgo_stop_algo);

int prop_chgalgo_notifier_call(struct prop_chgalgo_device *pca,
			       struct prop_chgalgo_notify *notify)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->notifier_call)
		return -ENOTSUPP;
	return pca->algo_ops->notifier_call(pca, notify);
}
EXPORT_SYMBOL(prop_chgalgo_notifier_call);

int prop_chgalgo_thermal_throttling(struct prop_chgalgo_device *pca, int mA)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->thermal_throttling)
		return -ENOTSUPP;
	return pca->algo_ops->thermal_throttling(pca, mA);
}
EXPORT_SYMBOL(prop_chgalgo_thermal_throttling);

int prop_chgalgo_set_jeita_vbat_cv(struct prop_chgalgo_device *pca, int mV)
{
	if (pca_check_devtype(pca, PCA_DEVTYPE_ALGO) < 0)
		return -EINVAL;
	if (!pca->algo_ops->set_jeita_vbat_cv)
		return -ENOTSUPP;
	return pca->algo_ops->set_jeita_vbat_cv(pca, mV);
}
EXPORT_SYMBOL(prop_chgalgo_set_jeita_vbat_cv);

/* Class released interface */
struct prop_chgalgo_device *
prop_chgalgo_device_register(struct device *parent,
			     struct prop_chgalgo_desc *pca_desc,
			     struct prop_chgalgo_ta_ops *ta_ops,
			     struct prop_chgalgo_chg_ops *chg_ops,
			     struct prop_chgalgo_algo_ops *algo_ops,
			     void *drv_data)
{
	int ret;
	struct prop_chgalgo_device *pca;

	dev_info(parent, "%s (%s)\n", __func__, pca_desc->name);
	pca = devm_kzalloc(parent, sizeof(*pca), GFP_KERNEL);
	if (!pca)
		return ERR_PTR(-ENOMEM);

	pca->dev.class = pca_class;
	pca->dev.type = &pca_dev_type;
	pca->dev.parent = parent;
	pca->dev.release = pca_device_release;
	dev_set_drvdata(&pca->dev, pca);
	pca->drv_data = drv_data;
	dev_set_name(&pca->dev, "%s", pca_desc->name);
	pca->desc = pca_desc;
	pca->ta_ops = ta_ops;
	pca->chg_ops = chg_ops;
	pca->algo_ops = algo_ops;
	srcu_init_notifier_head(&pca_desc->nh);

	ret = device_register(&pca->dev);
	if (ret) {
		kfree(pca);
		return ERR_PTR(ret);
	}

	dev_info(parent, "%s (%s) successfully\n", __func__, pca->desc->name);
	return pca;
}
EXPORT_SYMBOL(prop_chgalgo_device_register);

void prop_chgalgo_device_unregister(struct prop_chgalgo_device *pca)
{
	if (!pca)
		return;
	device_unregister(&pca->dev);
}
EXPORT_SYMBOL(prop_chgalgo_device_unregister);

static int pca_match_dev_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct prop_chgalgo_device *pca = dev_get_drvdata(dev);

	return strcmp(pca->desc->name, name) == 0;
}

struct prop_chgalgo_device *prop_chgalgo_dev_get_by_name(const char *name)
{
	struct device *dev = class_find_device(pca_class, NULL, name,
					       pca_match_dev_by_name);

	return dev ? dev_get_drvdata(dev) : NULL;
}
EXPORT_SYMBOL(prop_chgalgo_dev_get_by_name);

const char *prop_chgalgo_notify_evt_tostring(enum prop_chgalgo_notify_evt evt)
{
	return pca_notify_evt_name[evt];
}
EXPORT_SYMBOL(prop_chgalgo_notify_evt_tostring);

static void pca_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = pca_attr_groups;
	for (i = 0; i < ARRAY_SIZE(pca_device_attributes); i++)
		__pca_attrs[i] = &pca_device_attributes[i].attr;
}

static int __init pca_class_init(void)
{
	pr_info("%s (%s)\n", __func__, PROP_CHGALGO_CLASS_VERSION);

	pca_class = class_create(THIS_MODULE, "prop_chgalgo_class");
	if (IS_ERR(pca_class)) {
		pr_info("%s fail(%ld)\n", __func__, PTR_ERR(pca_class));
		return PTR_ERR(pca_class);
	}
	pca_init_attrs(&pca_dev_type);
	pca_class->suspend = NULL;
	pca_class->resume = NULL;

	pr_info("%s successfully\n", __func__);
	return 0;
}

static void __exit pca_class_exit(void)
{
	pr_info("%s\n", __func__);
	class_destroy(pca_class);
}

subsys_initcall(pca_class_init);
module_exit(pca_class_exit);

MODULE_DESCRIPTION("Proprietary Charging Algorithm Class");
MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_VERSION(PROP_CHGALGO_CLASS_VERSION);
MODULE_LICENSE("GPL");

/*
 * Revision Note
 * 1.0.7
 * (1) Add init_chip ops
 *
 * 1.0.6
 * (1) Fix is_algo_running & is_algo_ready build error
 *
 * 1.0.5
 * (1) Add thermal throttling ops
 * (2) Add is_ta_cc ops
 * (3) Remove BIF support
 * (4) Add jeita cv ops
 *
 * 1.0.4
 * (1) Add get_adc_accuracy ops
 *
 * 1.0.3
 * (1) Add IBUSUCP_FALL notification
 * (2) Add checking vbuslowerr/sync vta ops
 *
 * 1.0.2
 * (1) Add ta_istep/ta_vstep/support_cc/support_status in authentication data
 *
 * 1.0.1
 * (1) Add 3 operations
 *   - set_vbatovp_alarm
 *   - reset_vbatovp_alarm
 *   - notifier_call
 *
 * 1.0.0
 * Initial release
 */
