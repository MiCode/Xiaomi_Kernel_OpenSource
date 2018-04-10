/* Copyright (C) 2012 by Xiang Xiao <xiaoxiang@xiaomi.com>
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
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/gpio-ir.h>
#include <media/rc-core.h>

struct gpio_ir_dev {
	struct mutex            lock;
	struct platform_device *pdev;
	struct rc_dev          *rdev;
	struct regulator       *tx_reg;
	u32                     tx_carrier;
	u32                     tx_duty_cycle;
	struct regulator       *rx_reg;
	struct hrtimer          rx_timer;
	ktime_t                 rx_carrier_last;
	u32                     rx_carrier_count;
	bool                    rx_carrier_report;
};

struct gpio_ir_tx_packet {
	struct completion done;
	struct hrtimer    timer;
	unsigned int      gpio_nr;
	bool              high_active;
	u32               pulse;
	u32               space;
	bool              abort;
	unsigned int     *buffer;
	unsigned int      length;
	unsigned int      next;
	bool              on;
};

#define GPIO_IR_TX_NAME         (GPIO_IR_NAME "-tx")
#define GPIO_IR_RX_NAME         (GPIO_IR_NAME "-rx")

#define GPIO_IR_MIN_CARRIER_HZ  10000
#define GPIO_IR_DEF_CARRIER_HZ  38000

#define GPIO_IR_DEF_CARRIER_NS  (NSEC_PER_SEC / GPIO_IR_DEF_CARRIER_HZ)
#define GPIO_IR_MAX_CARRIER_NS  (NSEC_PER_SEC / GPIO_IR_MIN_CARRIER_HZ)

/* code for open and close */
static int gpio_ir_rx_enable(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	int rc = 0;

	if (gdev->rx_reg)
		rc = regulator_enable(gdev->rx_reg);
	if (rc == 0 && gpio_is_valid(gdata->rx_gpio_nr))
		enable_irq(gpio_to_irq(gdata->rx_gpio_nr));

	return rc;
}

static void gpio_ir_rx_disable(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;

	if (gpio_is_valid(gdata->rx_gpio_nr))
		disable_irq(gpio_to_irq(gdata->rx_gpio_nr));
	hrtimer_cancel(&gdev->rx_timer);
	if (gdev->rx_reg)
		regulator_disable(gdev->rx_reg);
}

static int gpio_ir_open(struct rc_dev *rdev)
{
	struct gpio_ir_dev *gdev = rdev->priv;
	int rc = 0;

	mutex_lock(&gdev->lock);
	rc = gpio_ir_rx_enable(gdev);
	mutex_unlock(&gdev->lock);

	return rc;
}

static void gpio_ir_close(struct rc_dev *rdev)
{
	struct gpio_ir_dev *gdev = rdev->priv;

	mutex_lock(&gdev->lock);
	gpio_ir_rx_disable(gdev);
	mutex_unlock(&gdev->lock);
}

/* code for ir transmit */
static int gpio_ir_tx_carrier(struct rc_dev *rdev, u32 carrier)
{
	struct gpio_ir_dev *gdev = rdev->priv;

	mutex_lock(&gdev->lock);
	gdev->tx_carrier = carrier;
	mutex_unlock(&gdev->lock);

	return 0;
}

static int gpio_ir_tx_duty_cycle(struct rc_dev *rdev, u32 duty_cycle)
{
	struct gpio_ir_dev *gdev = rdev->priv;

	mutex_lock(&gdev->lock);
	gdev->tx_duty_cycle = duty_cycle;
	mutex_unlock(&gdev->lock);

	return 0;
}

static void gpio_ir_tx_set(struct gpio_ir_tx_packet *gpkt, bool on)
{
	if (gpkt->high_active)
		gpio_set_value(gpkt->gpio_nr, on);
	else
		gpio_set_value(gpkt->gpio_nr, !on);
}

static enum hrtimer_restart gpio_ir_tx_timer(struct hrtimer *timer)
{
	struct gpio_ir_tx_packet *gpkt = container_of(timer, struct gpio_ir_tx_packet, timer);
	enum hrtimer_restart restart = HRTIMER_RESTART;

	if (!gpkt->abort && gpkt->next < gpkt->length) {
		if (gpkt->next & 0x01) {
			gpio_ir_tx_set(gpkt, false);

			hrtimer_forward_now(&gpkt->timer,
					ns_to_ktime(gpkt->buffer[gpkt->next++]));
		} else if (!gpkt->pulse || !gpkt->space) {
			gpio_ir_tx_set(gpkt, true);

			hrtimer_forward_now(&gpkt->timer,
					ns_to_ktime(gpkt->buffer[gpkt->next++]));
		} else {
			unsigned int nsecs;

			nsecs = gpkt->on ? gpkt->pulse : gpkt->space;
			nsecs = min(nsecs, gpkt->buffer[gpkt->next]);

			gpio_ir_tx_set(gpkt, gpkt->on);
			hrtimer_forward_now(&gpkt->timer, ns_to_ktime(nsecs));

			gpkt->buffer[gpkt->next] -= nsecs;
			gpkt->on = !gpkt->on;

			if (!gpkt->buffer[gpkt->next])
				gpkt->next++;
		}
	} else {
		restart = HRTIMER_NORESTART;
		gpio_ir_tx_set(gpkt, false);
		complete(&gpkt->done);
	}

	return restart;
}

static int gpio_ir_tx_transmit_with_timer(struct gpio_ir_tx_packet *gpkt)
{
	int rc = 0;

	init_completion(&gpkt->done);

	hrtimer_init(&gpkt->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gpkt->timer.function = gpio_ir_tx_timer;

	hrtimer_start(&gpkt->timer, ns_to_ktime(0), HRTIMER_MODE_REL);

	rc = wait_for_completion_interruptible(&gpkt->done);
	if (rc != 0) {
		gpkt->abort = true;
		wait_for_completion(&gpkt->done);
	}

	return gpkt->next ? : -ERESTARTSYS;
}

static void gpio_ir_tx_ndelay(unsigned long nsecs)
{
	unsigned long max_delay_ns = NSEC_PER_MSEC * MAX_UDELAY_MS;

	while (nsecs > max_delay_ns) {
		ndelay(max_delay_ns);
		nsecs -= max_delay_ns;
	}
	ndelay(nsecs);
}

static int gpio_ir_tx_transmit_with_delay(struct gpio_ir_tx_packet *gpkt)
{
	unsigned long flags;

	local_irq_save(flags);

	for (; gpkt->next < gpkt->length; gpkt->next++) {
		if (signal_pending(current))
			break;
		if (gpkt->next & 0x01) {
			gpio_ir_tx_set(gpkt, false);
			gpio_ir_tx_ndelay(gpkt->buffer[gpkt->next]);
		} else if (!gpkt->pulse || !gpkt->space) {
			gpio_ir_tx_set(gpkt, true);
			gpio_ir_tx_ndelay(gpkt->buffer[gpkt->next]);
		} else {
			while (gpkt->buffer[gpkt->next]) {
				unsigned int nsecs;

				nsecs = gpkt->on ? gpkt->pulse : gpkt->space;
				nsecs = min(nsecs, gpkt->buffer[gpkt->next]);

				gpio_ir_tx_set(gpkt, gpkt->on);
				gpio_ir_tx_ndelay(nsecs);

				gpkt->buffer[gpkt->next] -= nsecs;
				gpkt->on = !gpkt->on;
			}
		}
	}

	gpio_ir_tx_set(gpkt, false);
	local_irq_restore(flags);

	return gpkt->next ? : -ERESTARTSYS;
}

static int gpio_ir_tx_transmit(struct rc_dev *rdev, unsigned *txbuf, unsigned n)
{
	struct gpio_ir_dev *gdev = rdev->priv;
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	struct gpio_ir_tx_packet gpkt = {};
	int i, rc = 0;

	for (i = 0; i < n; i++)
		txbuf[i] *= NSEC_PER_USEC;

	mutex_lock(&gdev->lock);

	if (gdata->tx_disable_rx)
		gpio_ir_rx_disable(gdev);

	if (gdev->tx_reg) {
		rc = regulator_enable(gdev->tx_reg);
		if (rc != 0)
			goto err_regulator_enable;
	}

	if (gdata->tx_soft_carrier) {
		u32 carrier = gdev->tx_carrier ?: GPIO_IR_DEF_CARRIER_HZ;
		u32 duty_cycle = gdev->tx_duty_cycle ?: 50;
		u32 period = NSEC_PER_SEC / carrier;

		gpkt.pulse = period * duty_cycle / 100;
		gpkt.space = period - gpkt.pulse;
	}

	gpkt.gpio_nr     = gdata->tx_gpio_nr;
	gpkt.high_active = gdata->tx_high_active;
	gpkt.buffer      = txbuf;
	gpkt.length      = n;

	if (gdata->tx_with_timer)
		rc = gpio_ir_tx_transmit_with_timer(&gpkt);
	else
		rc = gpio_ir_tx_transmit_with_delay(&gpkt);

	if (gdev->tx_reg)
		regulator_disable(gdev->tx_reg);
err_regulator_enable:
	if (gdata->tx_disable_rx)
		gpio_ir_rx_enable(gdev);
	mutex_unlock(&gdev->lock);
	return rc;
}

static int __devinit gpio_ir_tx_probe(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	int rc = 0;

	if (gdata->tx_reg_id) {
		gdev->tx_reg = regulator_get(&gdev->pdev->dev, gdata->tx_reg_id);
		if (IS_ERR(gdev->tx_reg)) {
			dev_err(&gdev->pdev->dev,
					"failed to regulator_get(%s)\n",
					 gdata->tx_reg_id);
			return PTR_ERR(gdev->tx_reg);
		}
	}

	if (gpio_is_valid(gdata->tx_gpio_nr)) {
		rc = gpio_request(gdata->tx_gpio_nr, GPIO_IR_TX_NAME);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
					"failed to gpio_request(%u)\n",
					gdata->tx_gpio_nr);
			goto err_gpio_request;
		}

		rc = gpio_direction_output(
				gdata->tx_gpio_nr,
				!gdata->tx_high_active);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
				"failed to gpio_direction_output(%u)\n",
				gdata->tx_gpio_nr);
			goto err_gpio_direction_output;
		}

		if (gdata->tx_soft_carrier) {
			gdev->rdev->s_tx_carrier     = gpio_ir_tx_carrier;
			gdev->rdev->s_tx_duty_cycle  = gpio_ir_tx_duty_cycle;
		}
		gdev->rdev->tx_ir = gpio_ir_tx_transmit;
	}

	return rc;

err_gpio_direction_output:
	gpio_free(gdata->tx_gpio_nr);
err_gpio_request:
	if (gdev->tx_reg)
		regulator_put(gdev->tx_reg);
	return rc;
}

static void gpio_ir_tx_remove(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;

	if (gpio_is_valid(gdata->tx_gpio_nr))
		gpio_free(gdata->tx_gpio_nr);

	if (gdev->tx_reg)
		regulator_put(gdev->tx_reg);
}

static int gpio_ir_rx_carrier_report(struct rc_dev *rdev, int enable)
{
	struct gpio_ir_dev *gdev = rdev->priv;

	mutex_lock(&gdev->lock);
	gdev->rx_carrier_report = enable;
	mutex_unlock(&gdev->lock);

	return 0;
}

static u32 gpio_ir_rx_carrier_period(struct gpio_ir_dev *gdev, s32 adjust)
{
	ktime_t now;
	s64     delta;
	u32     period;

	now    = ktime_add_ns(ktime_get(), adjust);
	delta  = ktime_to_ns(ktime_sub(now, gdev->rx_carrier_last));
	period = (u32)(2 * delta) / gdev->rx_carrier_count;

	return period;
}

static enum hrtimer_restart gpio_ir_rx_timer(struct hrtimer *timer)
{
	struct gpio_ir_dev *gdev = container_of(timer, struct gpio_ir_dev, rx_timer);
	u32 period = gpio_ir_rx_carrier_period(gdev, -GPIO_IR_MAX_CARRIER_NS);
	int rc = -ENOENT;

	if (gdev->rx_carrier_report) {
		DEFINE_IR_RAW_EVENT(ev);

		ev.carrier        = NSEC_PER_SEC / period;
		ev.duty_cycle     = 50;
		ev.carrier_report = 1;

		rc = ir_raw_event_store(gdev->rdev, &ev);
		if (rc != 0) {
			dev_err(&gdev->pdev->dev,
					"failed to ir_raw_event_store(%d)\n", rc);
		}
	} else {
		rc = ir_raw_event_store_edge_with_adjust(gdev->rdev,
				IR_SPACE, period - GPIO_IR_MAX_CARRIER_NS);
		if (rc != 0) {
			dev_err(&gdev->pdev->dev, "failed to "
					"ir_raw_event_store_edge_with_adjust(%d)\n", rc);
		}
	}

	if (rc == 0)
		ir_raw_event_handle(gdev->rdev);

	return HRTIMER_NORESTART;
}

static bool gpio_ir_rx_get(struct gpio_ir_data *gdata)
{
	if (gdata->rx_high_active)
		return gpio_get_value(gdata->rx_gpio_nr);
	else
		return !gpio_get_value(gdata->rx_gpio_nr);
}

static irqreturn_t gpio_ir_rx_irq(int irq, void *dev)
{
	struct gpio_ir_dev *gdev = dev;
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	int rc = -ENOENT;

	if (gdata->rx_soft_carrier) {
		if (!hrtimer_active(&gdev->rx_timer)) {
			gdev->rx_carrier_last  = ktime_get();
			gdev->rx_carrier_count = 0;
			if (!gdev->rx_carrier_report) {
				rc = ir_raw_event_store_edge(gdev->rdev, IR_PULSE);
				if (rc != 0) {
					dev_err(&gdev->pdev->dev, "failed to "
							"ir_raw_event_store_edge(%d, %d)\n",
							IR_PULSE, rc);
				}
			}
		}

		gdev->rx_carrier_count++;
		hrtimer_start(&gdev->rx_timer,
				ns_to_ktime(GPIO_IR_MAX_CARRIER_NS),
				HRTIMER_MODE_REL);
	} else {
		enum raw_event_type type;

		type = gpio_ir_rx_get(gdata) ? IR_PULSE : IR_SPACE;
		rc = ir_raw_event_store_edge(gdev->rdev, type);
		if (rc != 0) {
			dev_err(&gdev->pdev->dev, "failed to "
					"ir_raw_event_store_edge(%d, %d)\n",
					type, rc);
		}
	}

	if (rc == 0)
		ir_raw_event_handle(gdev->rdev);

	return IRQ_HANDLED;
}

static int __devinit gpio_ir_rx_probe(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	unsigned long flags;
	int rc = 0;

	hrtimer_init(&gdev->rx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gdev->rx_timer.function = gpio_ir_rx_timer;

	if (gdata->rx_reg_id) {
		gdev->rx_reg = regulator_get(&gdev->pdev->dev, gdata->rx_reg_id);
		if (IS_ERR(gdev->rx_reg)) {
			dev_err(&gdev->pdev->dev,
					"failed to regulator_get(%s)\n",
					gdata->rx_reg_id);
			return PTR_ERR(gdev->rx_reg);
		}
	}

	if (gpio_is_valid(gdata->rx_gpio_nr)) {
		rc = gpio_request(gdata->rx_gpio_nr, GPIO_IR_RX_NAME);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
					"failed to gpio_request(%u)\n",
					gdata->rx_gpio_nr);
			goto err_gpio_request;
		}

		rc = gpio_direction_input(gdata->rx_gpio_nr);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
					"failed to gpio_direction_input(%u)\n",
					gdata->rx_gpio_nr);
			goto err_gpio_direction_input;
		}

		flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_DISABLED;
		rc = request_irq(gpio_to_irq(gdata->rx_gpio_nr),
				gpio_ir_rx_irq, flags, GPIO_IR_RX_NAME, gdev);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
					"failed to request_irq(%d)\n",
					gpio_to_irq(gdata->rx_gpio_nr));
			goto err_request_irq;
		}

		rc = device_init_wakeup(&gdev->pdev->dev, gdata->rx_can_wakeup);
		if (rc < 0) {
			dev_err(&gdev->pdev->dev,
					"failed to device_init_wakeup(%d)\n",
					gdata->rx_can_wakeup);
			goto err_device_init_wakeup;
		}

		if (gdata->rx_soft_carrier)
			gdev->rdev->s_carrier_report = gpio_ir_rx_carrier_report;
	}

	return rc;

err_device_init_wakeup:
	free_irq(gpio_to_irq(gdata->rx_gpio_nr), gdev);
err_request_irq:
err_gpio_direction_input:
	gpio_free(gdata->rx_gpio_nr);
err_gpio_request:
	if (gdev->rx_reg)
		regulator_put(gdev->rx_reg);
	return rc;
}

static void gpio_ir_rx_remove(struct gpio_ir_dev *gdev)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;

	if (gpio_is_valid(gdata->rx_gpio_nr)) {
		free_irq(gpio_to_irq(gdata->rx_gpio_nr), gdev);
		gpio_free(gdata->rx_gpio_nr);
	}

	hrtimer_cancel(&gdev->rx_timer);
	if (gdev->rx_reg)
		regulator_put(gdev->rx_reg);
}

#ifdef CONFIG_PM
static int gpio_ir_rx_wake(struct gpio_ir_dev *gdev, bool enable)
{
	struct gpio_ir_data *gdata = gdev->pdev->dev.platform_data;
	int rc = 0;

	if (gpio_is_valid(gdata->rx_gpio_nr)) {
		int irq = gpio_to_irq(gdata->rx_gpio_nr);

		if (enable)
			rc = enable_irq_wake(irq);
		else
			rc = disable_irq_wake(irq);
	}

	return rc;
}

static int gpio_ir_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_ir_dev *gdev = platform_get_drvdata(pdev);
	int rc = 0;

	mutex_lock(&gdev->lock);
	if (gdev->rdev->open_count) {
		if (device_may_wakeup(dev))
			rc = gpio_ir_rx_wake(gdev, true);
		else
			gpio_ir_rx_disable(gdev);
	}
	mutex_unlock(&gdev->lock);

	return rc;
}

static int gpio_ir_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_ir_dev *gdev = platform_get_drvdata(pdev);
	int rc = 0;

	mutex_lock(&gdev->lock);
	if (gdev->rdev->open_count) {
		if (device_may_wakeup(dev))
			rc = gpio_ir_rx_wake(gdev, false);
		else
			rc = gpio_ir_rx_enable(gdev);
	}
	mutex_unlock(&gdev->lock);

	return rc;
}

static const struct dev_pm_ops gpio_ir_pm_ops = {
	.suspend = gpio_ir_suspend,
	.resume  = gpio_ir_resume,
};
#endif

static int __devinit gpio_ir_probe(struct platform_device *pdev)
{
	struct gpio_ir_data *gdata;
	struct gpio_ir_dev *gdev;
	int rc = -ENOMEM;

	gdata = pdev->dev.platform_data;
	if (!gdata) {
		gdata = devm_kzalloc(&pdev->dev, sizeof(*gdata), GFP_KERNEL);
		if (gdata) {
			pdev->dev.platform_data = gdata;
			if (pdev->dev.of_node) {
				of_property_read_string(pdev->dev.of_node, "tx-reg-id", &gdata->tx_reg_id);
				of_property_read_u32(pdev->dev.of_node, "tx-gpio-nr", (u32 *)&gdata->tx_gpio_nr);
				gdata->tx_high_active = of_property_read_bool(pdev->dev.of_node, "tx-high-active");
				gdata->tx_soft_carrier = of_property_read_bool(pdev->dev.of_node, "tx-soft-carrier");
				gdata->tx_disable_rx = of_property_read_bool(pdev->dev.of_node, "tx-disable-rx");
				gdata->tx_with_timer = of_property_read_bool(pdev->dev.of_node, "tx-with-timer");

				of_property_read_string(pdev->dev.of_node, "rx-reg-id", &gdata->rx_reg_id);
				of_property_read_u32(pdev->dev.of_node, "rx-gpio-nr", (u32 *)&gdata->rx_gpio_nr);
				gdata->rx_high_active = of_property_read_bool(pdev->dev.of_node, "rx-high-active");
				gdata->rx_soft_carrier = of_property_read_bool(pdev->dev.of_node, "rx-soft-carrier");
				of_property_read_u64(pdev->dev.of_node, "rx-init-protos", &gdata->rx_init_protos);
				gdata->rx_can_wakeup = of_property_read_bool(pdev->dev.of_node, "rx-can-wakeup");
				of_property_read_string(pdev->dev.of_node, "rx-map-name", &gdata->rx_map_name);

				dev_info(&pdev->dev,
						"tx-reg-id = %s, tx-gpio-nr = %d, tx-high-active = %d, "
						"tx-soft-carrier = %d, tx-disable-rx = %d, tx-with-timer = %d\n",
						gdata->tx_reg_id, gdata->tx_gpio_nr, gdata->tx_high_active,
						gdata->tx_soft_carrier, gdata->tx_disable_rx, gdata->tx_with_timer);

				dev_info(&pdev->dev,
						"rx-reg-id = %s, rx-gpio-nr = %d, rx-high-active = %d, rx-soft-carrier = %d, "
						"rx-init-protos = %d, rx-can-wakeup = %d, rx-map-name = %d\n",
						gdata->rx_reg_id, gdata->rx_gpio_nr, gdata->rx_high_active, gdata->rx_soft_carrier,
						gdata->rx_init_protos, gdata->rx_can_wakeup, gdata->rx_map_name);
			}
		} else {
			dev_err(&pdev->dev, "failed to alloc platform data\n");
			return -ENOMEM;
		}
	}

	gdev = kzalloc(sizeof(*gdev), GFP_KERNEL);
	if (!gdev) {
		dev_err(&pdev->dev, "failed to alloc gdev\n");
		return rc;
	}

	mutex_init(&gdev->lock);

	gdev->pdev = pdev;
	platform_set_drvdata(pdev, gdev);

	gdev->rdev = rc_allocate_device();
	if (!gdev->rdev) {
		dev_err(&pdev->dev, "failed to alloc rdev\n");
		goto err_rc_allocate_device;
	}

	gdev->rdev->dev.parent       = &pdev->dev;
	gdev->rdev->input_name       = GPIO_IR_NAME;
	gdev->rdev->input_phys       = GPIO_IR_NAME;
	gdev->rdev->input_id.bustype = BUS_HOST;
	gdev->rdev->driver_name      = GPIO_IR_NAME;
	gdev->rdev->map_name         = gdata->rx_map_name;
	gdev->rdev->driver_type      = RC_DRIVER_IR_RAW;
	gdev->rdev->allowed_protos   = gdata->rx_init_protos;
	gdev->rdev->priv             = gdev;
	gdev->rdev->open             = gpio_ir_open;
	gdev->rdev->close            = gpio_ir_close;

	rc = gpio_ir_tx_probe(gdev);
	if (rc != 0)
		goto err_gpio_ir_tx_probe;

	rc = gpio_ir_rx_probe(gdev);
	if (rc != 0)
		goto err_gpio_ir_rx_probe;

	rc = rc_register_device(gdev->rdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "failed to register rdev\n");
		goto err_rc_register_device;
	}

	return rc;

err_rc_register_device:
	gpio_ir_rx_remove(gdev);
err_gpio_ir_rx_probe:
	gpio_ir_tx_remove(gdev);
err_gpio_ir_tx_probe:
	rc_free_device(gdev->rdev);
err_rc_allocate_device:
	kfree(gdev);
	return rc;
}

static int __devexit gpio_ir_remove(struct platform_device *pdev)
{
	struct gpio_ir_dev *gdev = platform_get_drvdata(pdev);

	rc_unregister_device(gdev->rdev);
	gpio_ir_rx_remove(gdev);
	gpio_ir_tx_remove(gdev);
	kfree(gdev);

	return 0;
}

static const struct of_device_id of_gpio_ir_match[] = {
	{.compatible = GPIO_IR_NAME,},
	{},
};

static struct platform_driver gpio_ir_driver = {
	.probe  = gpio_ir_probe,
	.remove = __devexit_p(gpio_ir_remove),
	.driver = {
		.name  = GPIO_IR_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_gpio_ir_match,
#ifdef CONFIG_PM
		.pm    = &gpio_ir_pm_ops,
#endif
	},
};

static int __init gpio_ir_init(void)
{
	return platform_driver_register(&gpio_ir_driver);
}
module_init(gpio_ir_init);

static void __exit gpio_ir_exit(void)
{
	platform_driver_unregister(&gpio_ir_driver);
}
module_exit(gpio_ir_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("GPIO IR driver");
