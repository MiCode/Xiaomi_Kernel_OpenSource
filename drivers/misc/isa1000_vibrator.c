/* Copyright (C) 2014 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#include "../staging/android/timed_output.h"

#define ISA1000_VIB_DEFAULT_TIMEOUT	15000
#define ISA1000_VIB_DEFAULT_ENGPIO 33
#define ISA1000_VIB_DEFAULT_PWM_CHANNEL 4
#define ISA1000_VIB_DEFAULT_PWM_FREQUENCY 22400

struct isa1000_vib {
	struct spmi_device *spmi;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct regulator * vdd;
	struct pwm_device *pwm;

	int en_gpio;
	int timeout;
	int state;
	int pwm_channel;
	u32 en_gpio_flags;
	struct mutex lock;
};

static struct isa1000_vib *vib_dev;

static int isa1000_vib_set(struct isa1000_vib *vib, int on)
{
	int rc;
	int period_us = USEC_PER_SEC/ISA1000_VIB_DEFAULT_PWM_FREQUENCY;

	if (on) {
		rc = pwm_config(vib->pwm,
						(period_us * 80/100),
						period_us);
		if (rc < 0){
			dev_err(&vib->spmi->dev, "Unable to config pwm\n");
		}

		rc = pwm_enable(vib->pwm);
		if (rc < 0){
			dev_err(&vib->spmi->dev, "Unable to enable pwm\n");
		}
		gpio_set_value_cansleep(vib->en_gpio, 1);
	} else {
		gpio_set_value_cansleep(vib->en_gpio, 0);
		pwm_disable(vib->pwm);
	}

	return rc;
}

static void isa1000_vib_enable(struct timed_output_dev *dev, int value)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
					 timed_dev);

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0)
		vib->state = 0;
	else {
		value = (value > vib->timeout ?
				 vib->timeout : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);
}

static void isa1000_vib_update(struct work_struct *work)
{
	struct isa1000_vib *vib = container_of(work, struct isa1000_vib,
					 work);
	isa1000_vib_set(vib, vib->state);
}

static int isa1000_vib_get_time(struct timed_output_dev *dev)
{
	struct isa1000_vib *vib = container_of(dev, struct isa1000_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart isa1000_vib_timer_func(struct hrtimer *timer)
{
	struct isa1000_vib *vib = container_of(timer, struct isa1000_vib,
							 vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int isa1000_vibrator_suspend(struct device *dev)
{
	struct isa1000_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	isa1000_vib_set(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(isa1000_vibrator_pm_ops, isa1000_vibrator_suspend, NULL);

static int __devinit isa1000_vibrator_probe(struct spmi_device *spmi)
{
	struct isa1000_vib *vib;
	int rc;
	u32 temp_val;

	vib = devm_kzalloc(&spmi->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->spmi = spmi;

	vib->timeout = ISA1000_VIB_DEFAULT_TIMEOUT;
	rc = of_property_read_u32(spmi->dev.of_node,
			"isa1000,vib-timeout-ms", &temp_val);
	if (!rc) {
		vib->timeout = temp_val;
	}

	vib->en_gpio = of_get_named_gpio_flags(spmi->dev.of_node, "isa1000,en-gpio",
				0, &vib->en_gpio_flags);
	if (vib->en_gpio < 0){
		dev_err(&spmi->dev,"please check enable gpio");
		vib->en_gpio = ISA1000_VIB_DEFAULT_ENGPIO;
	}

	rc = of_property_read_u32(spmi->dev.of_node, "isa1000,pwm-channel", &temp_val);
	if (!rc)
		vib->pwm_channel = temp_val;
	else{
		dev_err(&spmi->dev,"please check pwm output channel");
		vib->pwm_channel = ISA1000_VIB_DEFAULT_PWM_CHANNEL;
	}

	vib->pwm = pwm_request(vib->pwm_channel, "isa1000-vibrator");
	if (IS_ERR(vib->pwm)){
		dev_err(&spmi->dev,"pwm request fail\n");
	}

	vib->vdd = regulator_get(&spmi->dev, "vdd");
	if (IS_ERR(vib->vdd)) {
		rc = PTR_ERR(vib->vdd);
		dev_err(&spmi->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}
	rc = regulator_enable(vib->vdd);
	if (rc) {
		dev_err(&spmi->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	if (gpio_is_valid(vib->en_gpio)) {
		rc = gpio_request(vib->en_gpio, "isa1000-en-gpio");
		if (rc < 0) {
			dev_err(&spmi->dev, "en gpio request failed");
		}
		rc = gpio_direction_output(vib->en_gpio, 0);
		if (rc < 0) {
			dev_err(&spmi->dev, "set_direction for irq gpio failed\n");
		}
	}

	mutex_init(&vib->lock);
	INIT_WORK(&vib->work, isa1000_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = isa1000_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = isa1000_vib_get_time;
	vib->timed_dev.enable = isa1000_vib_enable;

	dev_set_drvdata(&spmi->dev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		return rc;

	vib_dev = vib;

	return rc;
}

static int  __devexit isa1000_vibrator_remove(struct spmi_device *spmi)
{
	struct isa1000_vib *vib = dev_get_drvdata(&spmi->dev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	regulator_disable(vib->vdd);
	timed_output_dev_unregister(&vib->timed_dev);
	mutex_destroy(&vib->lock);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "isa1000,vibrator",
	},
	{}
};

static struct spmi_driver isa1000_vibrator_driver = {
	.driver		= {
		.name	= "isa1000,vibrator",
		.of_match_table = spmi_match_table,
		.pm	= &isa1000_vibrator_pm_ops,
	},
	.probe		= isa1000_vibrator_probe,
	.remove		= __devexit_p(isa1000_vibrator_remove),
};

static int __init isa1000_vibrator_init(void)
{
	return spmi_driver_register(&isa1000_vibrator_driver);
}
module_init(isa1000_vibrator_init);

static void __exit isa1000_vibrator_exit(void)
{
	return spmi_driver_unregister(&isa1000_vibrator_driver);
}
module_exit(isa1000_vibrator_exit);

MODULE_DESCRIPTION("isa1000 vibrator driver");
MODULE_LICENSE("GPL v2");
