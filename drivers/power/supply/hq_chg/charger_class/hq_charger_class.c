// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#include "hq_charger_class.h"

static struct class *charger_class;
static ATOMIC_NOTIFIER_HEAD(charger_notifier);

int charger_get_adc(struct charger_dev *charger, 
				enum sc_adc_channel channel, uint32_t *value)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_adc == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_adc(charger, channel, value);
}
EXPORT_SYMBOL(charger_get_adc);

int charger_get_vbus_type(struct charger_dev *charger, 
							enum vbus_type *vbus_type)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_vbus_type == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_vbus_type(charger, vbus_type);
}
EXPORT_SYMBOL(charger_get_vbus_type);

int charger_get_online(struct charger_dev * charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_online == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_online(charger, en);
}
EXPORT_SYMBOL(charger_get_online);

int charger_is_charge_done(struct charger_dev * charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->is_charge_done == NULL)
		return -EOPNOTSUPP;
	return charger->ops->is_charge_done(charger, en);
}
EXPORT_SYMBOL(charger_is_charge_done);

int charger_get_hiz_status(struct charger_dev * charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_hiz_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_hiz_status(charger, en);
}

int charger_get_input_volt_lmt(struct charger_dev * charger,
		uint32_t *mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_input_volt_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_input_volt_lmt(charger, mv);
}
EXPORT_SYMBOL(charger_get_input_volt_lmt);

int charger_get_input_curr_lmt(struct charger_dev * charger,
		uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_input_curr_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_input_curr_lmt(charger, ma);
}
EXPORT_SYMBOL(charger_get_input_curr_lmt);

int charger_get_chg_status(struct charger_dev * charger,
		uint32_t *chg_state, uint32_t *chg_status)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_chg_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_chg_status(charger, chg_state, chg_status);
}
EXPORT_SYMBOL(charger_get_chg_status);

int charger_get_otg_status(struct charger_dev * charger, bool *en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_otg_status == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_otg_status(charger, en);
}
EXPORT_SYMBOL(charger_get_otg_status);

int charger_get_ichg(struct charger_dev * charger,
        uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_ichg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_ichg(charger, ma);
}
EXPORT_SYMBOL(charger_get_ichg);

int charger_get_term_curr(struct charger_dev * charger,
		uint32_t *ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_term_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_term_curr(charger, ma);
}
EXPORT_SYMBOL(charger_get_term_curr);

int charger_get_term_volt(struct charger_dev * charger,
		uint32_t *mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_term_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_term_volt(charger, mv);
}
EXPORT_SYMBOL(charger_get_term_volt);

int charger_set_hiz(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_hiz == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_hiz(charger, en);
}
EXPORT_SYMBOL(charger_set_hiz);

int charger_set_input_curr_lmt(struct charger_dev * charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_input_curr_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_input_curr_lmt(charger, ma);
}
EXPORT_SYMBOL(charger_set_input_curr_lmt);

int charger_disable_power_path(struct charger_dev * charger, bool ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->disable_power_path == NULL)
		return -EOPNOTSUPP;
	return charger->ops->disable_power_path(charger, ma);
}
EXPORT_SYMBOL(charger_disable_power_path);

int charger_set_input_volt_lmt(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_input_volt_lmt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_input_volt_lmt(charger, mv);
}
EXPORT_SYMBOL(charger_set_input_volt_lmt);

int charger_set_ichg(struct charger_dev * charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_ichg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_ichg(charger, ma);
}
EXPORT_SYMBOL(charger_set_ichg);

int charger_set_chg(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_chg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_chg(charger, en);
}
EXPORT_SYMBOL(charger_set_chg);

int charger_get_chg(struct charger_dev * charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->get_chg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->get_chg(charger);
}
EXPORT_SYMBOL(charger_get_chg);

int charger_set_otg(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg(charger, en);
}
EXPORT_SYMBOL(charger_set_otg);

int charger_set_otg_curr(struct charger_dev * charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg_curr(charger, ma);
}

int charger_set_otg_volt(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_otg_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_otg_volt(charger, mv);
}

int charger_set_term(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term(charger, en);
}
EXPORT_SYMBOL(charger_set_term);

int charger_set_term_curr(struct charger_dev * charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term_curr(charger, ma);
}
EXPORT_SYMBOL(charger_set_term_curr);

int charger_set_term_volt(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_term_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_term_volt(charger, mv);
}
EXPORT_SYMBOL(charger_set_term_volt);

int charger_set_rechg_volt(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_rechg_vol == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_rechg_vol(charger, mv);
}
EXPORT_SYMBOL(charger_set_rechg_volt);

int charger_adc_enable(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->adc_enable == NULL)
		return -EOPNOTSUPP;
	return charger->ops->adc_enable(charger, en);
}
EXPORT_SYMBOL(charger_adc_enable);

int charger_set_shipmode(struct charger_dev * charger, bool en)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_shipmode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_shipmode(charger, en);
}
EXPORT_SYMBOL(charger_set_shipmode);

int charger_set_prechg_volt(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_prechg_volt == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_prechg_volt(charger, mv);
}
EXPORT_SYMBOL(charger_set_prechg_volt);

int charger_set_prechg_curr(struct charger_dev * charger, int ma)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_prechg_curr == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_prechg_curr(charger, ma);
}
EXPORT_SYMBOL(charger_set_prechg_curr);

int charger_force_dpdm(struct charger_dev * charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->force_dpdm == NULL)
		return -EOPNOTSUPP;
	return charger->ops->force_dpdm(charger);
}
EXPORT_SYMBOL(charger_force_dpdm);

int charger_reset(struct charger_dev * charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->reset == NULL)
		return -EOPNOTSUPP;
	return charger->ops->reset(charger);
}
EXPORT_SYMBOL(charger_reset);

int charger_set_wd_timeout(struct charger_dev * charger, int ms)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->set_wd_timeout == NULL)
		return -EOPNOTSUPP;
	return charger->ops->set_wd_timeout(charger, ms);
}

int charger_kick_wd(struct charger_dev * charger)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->kick_wd == NULL)
		return -EOPNOTSUPP;
	return charger->ops->kick_wd(charger);
}

int charger_qc_identify(struct charger_dev * charger, int qc3_enable)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc_identify == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc_identify(charger, qc3_enable);
}
EXPORT_SYMBOL(charger_qc_identify);

int charger_qc3_vbus_puls(struct charger_dev * charger, bool state, int count)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc3_vbus_puls == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc3_vbus_puls(charger, state, count);
}
EXPORT_SYMBOL(charger_qc3_vbus_puls);

int charger_qc2_vbus_mode(struct charger_dev * charger, int mv)
{
	if (!charger || !charger->ops)
		return -EINVAL;
	if (charger->ops->qc2_vbus_mode == NULL)
		return -EOPNOTSUPP;
	return charger->ops->qc2_vbus_mode(charger, mv);
}
EXPORT_SYMBOL(charger_qc2_vbus_mode);

static void charger_changed_work(struct work_struct *work)
{
	struct charger_dev *charger = container_of(work, struct charger_dev,
								changed_work);

	if (likely(charger->changed)) {
		mutex_lock(&charger->changed_lock);
		charger->changed = false;
		mutex_unlock(&charger->changed_lock);
		atomic_notifier_call_chain(&charger_notifier,
						CHARGER_NOTIFER_INT, charger);
	}
}

void charger_changed(struct charger_dev *charger)
{
	mutex_lock(&charger->changed_lock);
	charger->changed = true;
	mutex_unlock(&charger->changed_lock);
	schedule_work(&charger->changed_work);
}
EXPORT_SYMBOL(charger_changed);

int charger_register_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL(charger_register_notifier);

void charger_unregister_notifier(struct notifier_block *nb)
{
	atomic_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL(charger_unregister_notifier);

static int charger_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct charger_dev *charger = dev_get_drvdata(dev);

	return strcmp(charger->name, name) == 0;
}

struct charger_dev *charger_find_dev_by_name(const char *name)
{
	struct charger_dev *charger = NULL;
	struct device *dev = class_find_device(charger_class, NULL, name,
					charger_match_device_by_name);

	if (dev) {
		charger = dev_get_drvdata(dev);
	}

	return charger;
}
EXPORT_SYMBOL(charger_find_dev_by_name);

struct charger_dev * charger_register(char *name, struct device *parent,
							struct charger_ops *ops, void *private)
{
	struct charger_dev *charger;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return ERR_PTR(-ENOMEM);

	dev = &(charger->dev);

	device_initialize(dev);

	dev->class = charger_class;
	dev->parent = parent;
	dev_set_drvdata(dev, charger);

	charger->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	charger->name = name;
	charger->ops = ops;

	mutex_init(&charger->changed_lock);
	INIT_WORK(&charger->changed_work, charger_changed_work);

	return charger;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(charger_register);

void * charger_get_private(struct charger_dev *charger)
{
	if (!charger)
		return ERR_PTR(-EINVAL);
	return charger->private;
}
EXPORT_SYMBOL(charger_get_private);

int charger_unregister(struct charger_dev *charger)
{
	device_unregister(&charger->dev);
	kfree(charger);
	return 0;
}
EXPORT_SYMBOL(charger_unregister);

static int __init charger_class_init(void)
{
	charger_class = class_create(THIS_MODULE, "charger_class");
	pr_info("%s\n", __func__);
	if (IS_ERR(charger_class)) {
		return PTR_ERR(charger_class);
	}

	charger_class->dev_uevent = NULL;

	return 0;
}

static void __exit charger_class_exit(void)
{
	class_destroy(charger_class);
}

subsys_initcall(charger_class_init);
module_exit(charger_class_exit);

MODULE_DESCRIPTION("Huaqin Charger Class Core");
MODULE_LICENSE("GPL v2");
