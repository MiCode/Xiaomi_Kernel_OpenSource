// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 */


#include <linux/device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>


#define VIB_DEVICE "regulator_vibrator"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

#define DEFAULT_MIN_LIMIT 15

struct reg_vibr_config {
	unsigned int min_limit;
	unsigned int max_limit;
	unsigned int min_volt;
	unsigned int max_volt;
	struct regulator *reg;
};

struct reg_vibr {
	struct workqueue_struct *vibr_queue;
	struct work_struct vibr_work;
	struct hrtimer vibr_timer;
	spinlock_t vibr_lock;
	unsigned int vibr_dur;
	bool vibr_active;
	bool vibr_state;
	bool reg_status;
	bool vibr_shutdown;
	struct reg_vibr_config vibr_conf;
	struct notifier_block oc_handle;
};

static int mt_vibra_parse_dt(struct device *dev,
		struct reg_vibr_config *vibr_conf)
{
	int ret;

	ret = of_property_read_u32(dev->of_node, "min-limit",
		&(vibr_conf->min_limit));
	if (ret)
		vibr_conf->min_limit = DEFAULT_MIN_LIMIT;
	vibr_conf->min_limit = max_t(unsigned int,
		vibr_conf->min_limit, DEFAULT_MIN_LIMIT);

	ret = of_property_read_u32(dev->of_node, "max-limit",
		&(vibr_conf->max_limit));
	if (ret)
		vibr_conf->max_limit = 0;

	if (!vibr_conf->max_limit &&
		vibr_conf->max_limit < vibr_conf->min_limit) {
		pr_notice("Error load dts: get error limitation(min > max)!\n");
		ret = -EINVAL;
		return ret;
	}

	vibr_conf->reg = devm_regulator_get(dev, "vib");
	if (IS_ERR(vibr_conf->reg)) {
		ret = PTR_ERR(vibr_conf->reg);
		pr_notice("Error load dts: get regulator return %d\n", ret);
		vibr_conf->reg = NULL;
		//return ret;
	}

	ret = of_property_read_u32(dev->of_node, "max-volt",
		&vibr_conf->max_volt);
	if (ret) {
		pr_notice("Error load dts: get max-volt failed!\n");
		ret = -EINVAL;
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "min-volt",
		&vibr_conf->min_volt);
	if (ret) {
		pr_notice("Error load dts: get min-volt failed!\n");
		ret = -EINVAL;
		return ret;
	}

	if (vibr_conf->min_volt > vibr_conf->max_volt) {
		pr_notice("Error load dts: get error voltage(min > max)!\n");
		ret = -EINVAL;
		return ret;
	}

	pr_info("vibr_conf = %u, %u, %u-%u\n",
		vibr_conf->min_limit, vibr_conf->max_limit,
		vibr_conf->min_volt, vibr_conf->max_volt);

	return ret;

}

static int vibr_power_set(struct reg_vibr *vibr)
{
	int ret = 0;

	if (vibr->vibr_conf.reg == NULL)
		return ret;

	pr_info("set voltage = %u-%u\n",
		vibr->vibr_conf.min_volt, vibr->vibr_conf.max_volt);
	ret = regulator_set_voltage(vibr->vibr_conf.reg,
		vibr->vibr_conf.min_volt, vibr->vibr_conf.max_volt);
	if (ret < 0)
		pr_notice("set voltage fail, ret = %d\n", ret);

	return ret;
}

static void vibr_enable(struct reg_vibr *vibr)
{
	if (vibr->vibr_conf.reg == NULL)
		return;

	if (!vibr->reg_status) {
		if (regulator_enable(vibr->vibr_conf.reg))
			pr_notice("set vibr_reg enable failed!\n");
		else
			vibr->reg_status = 1;
	} else {
		pr_notice("vibr_reg already enabled.\n");
	}
}

static void vibr_disable(struct reg_vibr *vibr)
{
	if (vibr->vibr_conf.reg == NULL)
		return;

	if (vibr->reg_status) {
		if (regulator_disable(vibr->vibr_conf.reg))
			pr_notice("set vibr_reg disable failed!\n");
		else
			vibr->reg_status = 0;
	} else {
		pr_notice("vibr_reg already disabled.\n");
	}
}

static void update_vibrator(struct work_struct *work)
{

	struct reg_vibr *vibr = container_of(work, struct reg_vibr, vibr_work);

	pr_info("vibr_state = %d\n", vibr->vibr_state);

	if (!vibr->vibr_state)
		vibr_disable(vibr);
	else
		vibr_enable(vibr);
}

static void vibrator_enable(struct reg_vibr *vibr,
	unsigned int dur, unsigned int activate)
{
	unsigned long flags;

	pr_info("cancel hrtimer, cust:%u-%u, dur:%u, shutdown:%d\n",
		vibr->vibr_conf.min_limit, vibr->vibr_conf.max_limit,
		dur, vibr->vibr_shutdown);
	spin_lock_irqsave(&vibr->vibr_lock, flags);
	hrtimer_cancel(&vibr->vibr_timer);

	if (!activate || vibr->vibr_shutdown || !dur) {
		vibr->vibr_state = 0;
	} else {
		dur = max(vibr->vibr_conf.min_limit, dur);
		if (vibr->vibr_conf.max_limit)
			dur = min(dur, vibr->vibr_conf.max_limit);
		vibr->vibr_state = 1;
		hrtimer_start(&vibr->vibr_timer,
			      ktime_set(dur / 1000, (dur % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&vibr->vibr_lock, flags);
	queue_work(vibr->vibr_queue, &vibr->vibr_work);
}

static enum hrtimer_restart mtk_vibrator_timer_func(struct hrtimer *timer)
{
	struct reg_vibr *vibr = container_of(timer,
		struct reg_vibr, vibr_timer);

	vibr->vibr_state = 0;
	queue_work(vibr->vibr_queue, &vibr->vibr_work);
	return HRTIMER_NORESTART;
}

static const struct of_device_id vibr_of_ids[] = {
	{ .compatible = "regulator-vibrator", },
	{}
};

static ssize_t activate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct reg_vibr *vibr = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n", vibr->vibr_active);
}

static ssize_t activate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int activate, duration;
	ssize_t ret;
	struct reg_vibr *vibr = dev_get_drvdata(dev->parent);

	ret = kstrtouint(buf, 10, &activate);
	if (ret) {
		pr_notice("set activate fail\n");
		return ret;
	}
	duration = vibr->vibr_dur;
	pr_info("set activate duration = %u, %u\n",
		activate, duration);
	vibrator_enable(vibr, duration, activate);

	ret = size;
	return ret;
}

static ssize_t state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct reg_vibr *vibr = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n", vibr->vibr_state);
}

static ssize_t state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int state;
	ssize_t ret;
	struct reg_vibr *vibr = dev_get_drvdata(dev->parent);

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		pr_notice("set state fail\n");
		return ret;
	}

	vibr->vibr_state = state;
	ret = size;
	return ret;
}

static ssize_t duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct reg_vibr *vibr = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%u\n", vibr->vibr_dur);
}

static ssize_t duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int activate, duration;
	ssize_t ret;
	struct reg_vibr *vibr;

	ret = kstrtouint(buf, 10, &duration);
	if (ret) {
		pr_notice("set duration fail!\n");
		return ret;
	}
	vibr = dev_get_drvdata(dev->parent);
	vibr->vibr_dur = duration;
	activate = vibr->vibr_active;
	pr_debug("set activate duration = %u, %u\n",
		activate, duration);

	ret = size;
	return ret;
}

static DEVICE_ATTR_RW(activate);
static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(duration);

static struct attribute *activate_attrs[] = {
	&dev_attr_activate.attr,
	NULL,
};

static struct attribute *state_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};

static struct attribute *duration_attrs[] = {
	&dev_attr_duration.attr,
	NULL,
};

static struct attribute_group activate_group = {
	.attrs = activate_attrs,
};

static struct attribute_group state_group = {
	.attrs = state_attrs,
};

static struct attribute_group duration_group = {
	.attrs = duration_attrs,
};

static const struct attribute_group *vibr_group[] = {
	&activate_group,
	&state_group,
	&duration_group,
	NULL
};

static struct led_classdev led_vibr = {
	.name		= "vibrator",
	.groups		= vibr_group,
};

static int regulator_oc_event(struct notifier_block *nb,
	unsigned long event, void *data)
{
	struct reg_vibr *vibr = container_of(nb, struct reg_vibr, oc_handle);

	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
	case REGULATOR_EVENT_FAIL:
		pr_info("get regulator oc event: %lu", event);
		vibr_disable(vibr);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int vib_probe(struct platform_device *pdev)
{
	int ret;
	struct reg_vibr *m_vibr;

	pr_info("probe start +++");
	m_vibr = devm_kzalloc(&pdev->dev, sizeof(struct reg_vibr), GFP_KERNEL);
	if (!m_vibr) {
		ret = -ENOMEM;
		goto err;
	}
	m_vibr->vibr_queue = create_singlethread_workqueue(VIB_DEVICE);
	if (!m_vibr->vibr_queue) {
		ret = -ENOMEM;
		pr_notice("unable to create workqueue!\n");
		goto err;
	}

	ret = mt_vibra_parse_dt(&(pdev->dev), &(m_vibr->vibr_conf));
	if (ret) {
		pr_notice("failed to parse devicetree(%d)!\n", ret);
		goto err;
	}

	INIT_WORK(&m_vibr->vibr_work, update_vibrator);
	spin_lock_init(&m_vibr->vibr_lock);
	m_vibr->vibr_shutdown = 0;
	if (m_vibr->vibr_conf.reg == NULL)
		m_vibr->reg_status = 0;
	else if (regulator_is_enabled(m_vibr->vibr_conf.reg))
		m_vibr->reg_status = 1;
	else
		m_vibr->reg_status = 0;

	hrtimer_init(&(m_vibr->vibr_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	m_vibr->vibr_timer.function = mtk_vibrator_timer_func;
	ret = devm_led_classdev_register(&pdev->dev, &led_vibr);
	if (ret < 0) {
		pr_info("led class register fail\n");
		goto err;
	}

	/* register oc notification for this regulator */
	m_vibr->oc_handle.notifier_call = regulator_oc_event;
	ret = devm_regulator_register_notifier(m_vibr->vibr_conf.reg,
		&m_vibr->oc_handle);
	if (ret)
		pr_info("regulator notifier request failed\n");

	platform_set_drvdata(pdev, m_vibr);
	ret = vibr_power_set(m_vibr);
	if (ret < 0) {
		pr_info("set voltage for regulator fail\n");
		goto err;
	}
	pr_info("probe success, end ---");
	return 0;

err:
	pr_notice("probe failed(%d), end ---!\n", ret);
	return ret;

}

static int vib_remove(struct platform_device *pdev)
{
	struct reg_vibr *vibr = platform_get_drvdata(pdev);

	cancel_work_sync(&vibr->vibr_work);
	hrtimer_cancel(&vibr->vibr_timer);
	devm_led_classdev_unregister(&pdev->dev, &led_vibr);

	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	struct reg_vibr *vibr = platform_get_drvdata(pdev);

	pr_info("shutdown: enter!\n");
	spin_lock_irqsave(&vibr->vibr_lock, flags);
	vibr->vibr_shutdown = 1;
	if (vibr->vibr_state) {
		vibr->vibr_state = 0;
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
		pr_info("vibrator will disable!\n");
		vibr_disable(vibr);
	} else {
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
	}
}

static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
			.name = VIB_DEVICE,
			.of_match_table = vibr_of_ids,
		   },
};

module_platform_driver(vibrator_driver);
MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
