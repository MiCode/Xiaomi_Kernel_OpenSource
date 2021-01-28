/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#include <vibrator.h>
#include <vibrator_hal.h>
#include <mt-plat/upmu_common.h>

#define VIB_DEVICE				"mtk_vibrator"
#define VIB_TAG                                 "[vibrator]"

struct mt_vibr {
	struct workqueue_struct *vibr_queue;
	struct work_struct vibr_work;
	struct hrtimer vibr_timer;
	int ldo_state;
	int shutdown_flag;
	atomic_t vibr_dur;
	spinlock_t vibr_lock;
	atomic_t vibr_state;
};

static struct mt_vibr *g_mt_vib;

static int vibr_Enable(void)
{
	if (!g_mt_vib->ldo_state) {
		vibr_Enable_HW();
		g_mt_vib->ldo_state = 1;
	}
	return 0;
}

static int vibr_Disable(void)
{
	if (g_mt_vib->ldo_state) {
		vibr_Disable_HW();
		g_mt_vib->ldo_state = 0;
	}
	return 0;
}

static void update_vibrator(struct work_struct *work)
{
	struct mt_vibr *vibr = container_of(work, struct mt_vibr, vibr_work);

	if (atomic_read(&vibr->vibr_state) == 0)
		vibr_Disable();
	else
		vibr_Enable();
}

static void vibrator_enable(unsigned int dur, unsigned int activate)
{
	unsigned long flags;
	struct vibrator_hw *hw = mt_get_cust_vibrator_hw();

	spin_lock_irqsave(&g_mt_vib->vibr_lock, flags);
	hrtimer_cancel(&g_mt_vib->vibr_timer);
	pr_debug(VIB_TAG "cancel hrtimer, cust:%dms, value:%u, shutdown:%d\n",
			hw->vib_timer, dur, g_mt_vib->shutdown_flag);

	if (activate == 0 || g_mt_vib->shutdown_flag == 1) {
		atomic_set(&g_mt_vib->vibr_state, 0);
	} else {
#ifdef CUST_VIBR_LIMIT
		if (dur > hw->vib_limit && dur < hw->vib_timer)
#else
		if (dur >= 10 && dur < hw->vib_timer)
#endif
			dur = hw->vib_timer;

		dur = (dur > 15000 ? 15000 : dur);
		atomic_set(&g_mt_vib->vibr_state, 1);
		hrtimer_start(&g_mt_vib->vibr_timer,
			      ktime_set(dur / 1000, (dur % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&g_mt_vib->vibr_lock, flags);
	queue_work(g_mt_vib->vibr_queue, &g_mt_vib->vibr_work);
}

static void vibrator_oc_handler(void)
{
	pr_debug(VIB_TAG "%s: disable vibr for oc intr happened\n", __func__);
	vibrator_enable(0, 0);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct mt_vibr *vibr = container_of(timer, struct mt_vibr, vibr_timer);

	atomic_set(&vibr->vibr_state, 0);
	queue_work(vibr->vibr_queue, &vibr->vibr_work);
	return HRTIMER_NORESTART;
}

static const struct of_device_id vibr_of_ids[] = {
	{ .compatible = "mediatek,vibrator", },
	{}
};

static atomic_t vib_state;

static ssize_t vibr_activate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&g_mt_vib->vibr_state));
}

static ssize_t vibr_activate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int activate = 0, dur = 0;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &activate);
	if (ret) {
		pr_err(VIB_TAG "set activate fail\n");
		return ret;
	}
	dur = atomic_read(&g_mt_vib->vibr_dur);
	vibrator_enable(dur, activate);
	ret = size;
	return ret;
}

static ssize_t vibr_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&vib_state));
}

static ssize_t vibr_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int state = 0;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		pr_err(VIB_TAG "set state fail\n");
		return ret;
	}
	atomic_set(&vib_state, state);

	ret = size;
	return ret;
}
static ssize_t vibr_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int duration;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &duration);
	if (ret) {
		pr_err(VIB_TAG "set duration fail\n");
		return ret;
	}

	atomic_set(&g_mt_vib->vibr_dur, duration);
	ret = size;
	return ret;
}

static DEVICE_ATTR(activate, 0644, vibr_activate_show, vibr_activate_store);
static DEVICE_ATTR(state, 0644, vibr_state_show, vibr_state_store);
static DEVICE_ATTR(duration, 0644, NULL, vibr_duration_store);

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

static int vib_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt_vibr *vibr;

	init_vibr_oc_handler(vibrator_oc_handler);

	vibr = devm_kzalloc(&pdev->dev, sizeof(*vibr), GFP_KERNEL);
	if (!vibr)
		return -ENOMEM;

	ret = devm_led_classdev_register(&pdev->dev, &led_vibr);
	if (ret < 0) {
		pr_err(VIB_TAG "led class register fail\n");
		return ret;
	}

	vibr->vibr_queue = create_singlethread_workqueue(VIB_DEVICE);
	if (!vibr->vibr_queue) {
		pr_err(VIB_TAG "unable to create workqueue\n");
		return -ENODATA;
	}

	INIT_WORK(&vibr->vibr_work, update_vibrator);
	spin_lock_init(&vibr->vibr_lock);
	vibr->shutdown_flag = 0;
	atomic_set(&vibr->vibr_state, 0);
	hrtimer_init(&vibr->vibr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibr->vibr_timer.function = vibrator_timer_func;

	dev_set_drvdata(&pdev->dev, vibr);
	g_mt_vib = vibr;
	init_cust_vibrator_dtsi(pdev);
	vibr_power_set();

	pr_debug(VIB_TAG "probe done\n");

	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	struct mt_vibr *vibr = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&vibr->vibr_work);
	hrtimer_cancel(&vibr->vibr_timer);
	devm_led_classdev_unregister(&pdev->dev, &led_vibr);

	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	struct mt_vibr *vibr = dev_get_drvdata(&pdev->dev);

	pr_debug(VIB_TAG "shutdown: enter!\n");
	spin_lock_irqsave(&vibr->vibr_lock, flags);
	vibr->shutdown_flag = 1;
	if (atomic_read(&vibr->vibr_state)) {
		atomic_set(&vibr->vibr_state, 0);
		spin_unlock_irqrestore(&vibr->vibr_lock, flags);
		pr_debug(VIB_TAG "%s: vibrator will disable\n", __func__);
		vibr_Disable();
		return;
	}
	spin_unlock_irqrestore(&vibr->vibr_lock, flags);
}


static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
			.name = VIB_DEVICE,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = vibr_of_ids,
#endif
		   },
};

static int vib_mod_init(void)
{
	s32 ret;

	ret = platform_driver_register(&vibrator_driver);
	if (ret) {
		pr_err(VIB_TAG "Unable to register driver (%d)\n", ret);
		return ret;
	}
	pr_debug(VIB_TAG "init Done\n");

	return 0;
}

static void vib_mod_exit(void)
{
	pr_debug(VIB_TAG "%s: Done\n", __func__);
}

module_init(vib_mod_init);
module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
