/* Copyright (C) 2013 by Xiang Xiao <xiaoxiang@xiaomi.com>
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/pwm-ir.h>
#include <media/rc-core.h>
struct pwm_ir_dev {
	struct mutex            lock;
	struct platform_device *pdev;
	struct rc_dev          *rdev;
	struct regulator       *reg;
	struct pwm_device      *pwm;
	u32                     carrier;
	u32                     duty_cycle;
};

struct pwm_ir_packet {
	struct completion  done;
	struct hrtimer     timer;
	struct pwm_device *pwm;
	bool               abort;
	unsigned int      *buffer;
	unsigned int       length;
	unsigned int       next;
};

#define __devexit
#define __devinitdata
#define __devinit
#define __devexit_p

static int pwm_ir_tx_config(struct pwm_ir_dev *dev, u32 carrier, u32 duty_cycle)
{
	int period_ns, duty_ns, rc;

	period_ns = NSEC_PER_SEC / carrier;
	duty_ns = period_ns * duty_cycle / 100;

	rc = pwm_config(dev->pwm, duty_ns, period_ns);
	if (rc == 0) {
		dev->carrier = carrier;
		dev->duty_cycle = duty_cycle;
	}

	return rc;
}

static int pwm_ir_tx_carrier(struct rc_dev *rdev, u32 carrier)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;

	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, carrier, dev->duty_cycle);
	mutex_unlock(&dev->lock);

	return rc;
}

static int pwm_ir_tx_duty_cycle(struct rc_dev *rdev, u32 duty_cycle)
{
	struct pwm_ir_dev *dev = rdev->priv;
	int rc;

	mutex_lock(&dev->lock);
	rc = pwm_ir_tx_config(dev, dev->carrier, duty_cycle);
	mutex_unlock(&dev->lock);

	return rc;
}

static enum hrtimer_restart pwm_ir_tx_timer(struct hrtimer *timer)
{
	struct pwm_ir_packet *pkt = container_of(timer, struct pwm_ir_packet, timer);
	enum hrtimer_restart restart = HRTIMER_RESTART;

	if (!pkt->abort && pkt->next < pkt->length) {
		u64 orun = hrtimer_forward_now(&pkt->timer,
				ns_to_ktime(pkt->buffer[pkt->next++]));
		if (orun > 1)
			pr_warn("pwm-ir: lost %llu hrtimer callback\n", orun - 1);

		if (pkt->next & 0x01)
			pwm_disable(pkt->pwm);
		else
			pwm_enable(pkt->pwm);
	} else {
		restart = HRTIMER_NORESTART;
		pwm_disable(pkt->pwm);
		complete(&pkt->done);
	}

	return restart;
}

static int pwm_ir_tx_transmit_with_timer(struct pwm_ir_packet *pkt)
{
	int rc = 0;

	init_completion(&pkt->done);

	hrtimer_init(&pkt->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pkt->timer.function = pwm_ir_tx_timer;

	hrtimer_start(&pkt->timer, ns_to_ktime(0), HRTIMER_MODE_REL);

	rc = wait_for_completion_interruptible(&pkt->done);
	if (rc != 0) {
		pkt->abort = true;
		wait_for_completion(&pkt->done);
	}

	return pkt->next ? : -ERESTARTSYS;
}


static long pwm_ir_tx_work(void *arg)
{
	struct pwm_ir_packet *pkt = arg;
	unsigned long flags;

	local_irq_save(flags);

	for (; pkt->next < pkt->length; pkt->next++) {
		if (signal_pending(current))
			break;
		if (pkt->next & 0x01) {
			pwm_disable(pkt->pwm);

		} else {
			pwm_enable(pkt->pwm);

		}

		ndelay(pkt->buffer[pkt->next]%1000);
		udelay(pkt->buffer[pkt->next]/1000);
	}

	pwm_disable(pkt->pwm);
	local_irq_restore(flags);

	return pkt->next ? : -ERESTARTSYS;
}

static int pwm_ir_tx_transmit_with_delay(struct pwm_ir_packet *pkt)
{
	int cpu, rc = -ENODEV;

	for_each_online_cpu(cpu) {

		if (cpu != 0) {
			rc = work_on_cpu(cpu, pwm_ir_tx_work, pkt);
			break;
		}
	}

	if (rc == -ENODEV) {
		pr_warn("pwm-ir: can't run on the auxilliary cpu\n");
		rc = pwm_ir_tx_work(pkt);
	}

	return rc;
}

static int pwm_ir_tx_transmit(struct rc_dev *rdev, unsigned *txbuf, unsigned n)
{
	struct pwm_ir_dev *dev = rdev->priv;
	struct pwm_ir_data *data = dev->pdev->dev.platform_data;
	struct pwm_ir_packet pkt = {};
	int i, rc = 0;

	for (i = 0; i < n; i++)
		txbuf[i] *= NSEC_PER_USEC;

	mutex_lock(&dev->lock);

	if (dev->reg) {
		rc = regulator_enable(dev->reg);
		if (rc != 0)
			goto err_regulator_enable;
	}

	pkt.pwm    = dev->pwm;
	pkt.buffer = txbuf;
	pkt.length = n;

	if (data->use_timer)
		rc = pwm_ir_tx_transmit_with_timer(&pkt);
	else
		rc = pwm_ir_tx_transmit_with_delay(&pkt);

	if (dev->reg)
		regulator_disable(dev->reg);
err_regulator_enable:
	mutex_unlock(&dev->lock);
	return rc;
}

static int __devinit pwm_ir_tx_probe(struct pwm_ir_dev *dev)
{
	struct pwm_ir_data *data = dev->pdev->dev.platform_data;
	struct platform_device *pdev = dev->pdev;
	int rc = 0;

	if (data->reg_id) {
		dev->reg = regulator_get(&dev->pdev->dev, data->reg_id);
		if (IS_ERR(dev->reg)) {
			dev_err(&dev->pdev->dev,
					"failed to regulator_get(%s)\n",
					 data->reg_id);
			return PTR_ERR(dev->reg);
		}
	}


	dev->pwm = of_pwm_get(pdev->dev.of_node, NULL);
	if (IS_ERR(dev->pwm)) {
		dev_err(&dev->pdev->dev,
				"failed to of_pwm_get()\n");
		rc = PTR_ERR(dev->pwm);
		dev_err(&dev->pdev->dev, "Cannot get PWM device rc:(%d)\n", rc);
		dev->pwm = NULL;
		goto err_regulator_put;
	}

	if (data->low_active) {
#if 0 /* need the latest kernel */
		rc = pwm_set_polarity(dev->pwm, PWM_POLARITY_INVERSED);
#else
		rc = -ENOSYS;
#endif
		if (rc != 0) {
			dev_err(&dev->pdev->dev, "failed to change polarity\n");
			goto err_pwm_free;
		}
	}

	rc = pwm_ir_tx_config(dev, 38000, 50);
	if (rc != 0) {
		dev_err(&dev->pdev->dev, "failed to change carrier and duty\n");
		goto err_pwm_free;
	}
	dev->rdev->tx_ir           = pwm_ir_tx_transmit;
	dev->rdev->s_tx_carrier    = pwm_ir_tx_carrier;
	dev->rdev->s_tx_duty_cycle = pwm_ir_tx_duty_cycle;

	return rc;

err_pwm_free:
	pwm_free(dev->pwm);
err_regulator_put:
	if (dev->reg)
		regulator_put(dev->reg);
	return rc;
}

static void pwm_ir_tx_remove(struct pwm_ir_dev *dev)
{
	if (dev->reg)
		regulator_put(dev->reg);
	pwm_free(dev->pwm);
}

static int __devinit pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev;
	int rc = -ENOMEM;


	if (!pdev->dev.platform_data) {
		pdev->dev.platform_data = devm_kzalloc(&pdev->dev, sizeof(struct pwm_ir_data), GFP_KERNEL);
		if (pdev->dev.platform_data) {
			if (pdev->dev.of_node) {
				struct pwm_ir_data *data = pdev->dev.platform_data;

				of_property_read_string(pdev->dev.of_node, "reg-id", &data->reg_id);

				data->low_active = of_property_read_bool(pdev->dev.of_node, "low-active");
				data->use_timer = of_property_read_bool(pdev->dev.of_node, "use-timer");

				dev_info(&pdev->dev,
						"reg-id = %s, low-active = %d, use-timer = %d\n",
						data->reg_id,  data->low_active, data->use_timer);
			}
		} else {
			dev_err(&pdev->dev, "failed to alloc platform data\n");
			return -ENOMEM;
		}
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "failed to alloc dev\n");
		return rc;
	}

	mutex_init(&dev->lock);

	dev->pdev = pdev;
	platform_set_drvdata(pdev, dev);

	dev->rdev = rc_allocate_device();
	if (!dev->rdev) {
		dev_err(&pdev->dev, "failed to alloc rdev\n");
		goto err_rc_allocate_device;
	}

	dev->rdev->dev.parent       = &pdev->dev;
	dev->rdev->input_name       = PWM_IR_NAME;
	dev->rdev->input_phys       = PWM_IR_NAME;
	dev->rdev->input_id.bustype = BUS_HOST;
	dev->rdev->driver_name      = PWM_IR_NAME;
	dev->rdev->map_name         = RC_MAP_LIRC;
	dev->rdev->driver_type      = RC_DRIVER_IR_RAW;
	dev->rdev->priv             = dev;

	rc = pwm_ir_tx_probe(dev);
	if (rc != 0)
		goto err_pwm_ir_tx_probe;

	rc = rc_register_device(dev->rdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rdev\n");
		goto err_rc_register_device;
	}

	return rc;

err_rc_register_device:
	pwm_ir_tx_remove(dev);
err_pwm_ir_tx_probe:
	rc_free_device(dev->rdev);
err_rc_allocate_device:
	kfree(dev);
	return rc;
}

static int __devexit pwm_ir_remove(struct platform_device *pdev)
{
	struct pwm_ir_dev *dev = platform_get_drvdata(pdev);

	rc_unregister_device(dev->rdev);
	pwm_ir_tx_remove(dev);
	kfree(dev);

	return 0;
}

static const struct of_device_id of_pwm_ir_match[] = {
	{.compatible = PWM_IR_NAME,},
	{},
};

static struct platform_driver pwm_ir_driver = {
	.probe  = pwm_ir_probe,
	.remove = __devexit_p(pwm_ir_remove),
	.driver = {
		.name  = PWM_IR_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_pwm_ir_match,
	},
};

static int __init pwm_ir_init(void)
{
	return platform_driver_register(&pwm_ir_driver);
}
late_initcall(pwm_ir_init);

static void __exit pwm_ir_exit(void)
{
	platform_driver_unregister(&pwm_ir_driver);
}
module_exit(pwm_ir_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("PWM IR driver");
