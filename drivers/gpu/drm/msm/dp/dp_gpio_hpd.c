/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/sde_io_util.h>
#include <linux/of_gpio.h>
#include "dp_gpio_hpd.h"

struct dp_gpio_hpd_private {
	struct device *dev;
	struct dp_hpd base;
	struct dss_gpio gpio_cfg;
	struct delayed_work work;
	struct dp_hpd_cb *cb;
	int irq;
	bool hpd;
};

static int dp_gpio_hpd_connect(struct dp_gpio_hpd_private *gpio_hpd, bool hpd)
{
	int rc = 0;

	if (!gpio_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	gpio_hpd->base.hpd_high = hpd;
	gpio_hpd->base.alt_mode_cfg_done = hpd;
	gpio_hpd->base.hpd_irq = false;

	if (!gpio_hpd->cb ||
		!gpio_hpd->cb->configure ||
		!gpio_hpd->cb->disconnect) {
		pr_err("invalid cb\n");
		rc = -EINVAL;
		goto error;
	}

	if (hpd)
		rc = gpio_hpd->cb->configure(gpio_hpd->dev);
	else
		rc = gpio_hpd->cb->disconnect(gpio_hpd->dev);

error:
	return rc;
}

static int dp_gpio_hpd_attention(struct dp_gpio_hpd_private *gpio_hpd)
{
	int rc = 0;

	if (!gpio_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	gpio_hpd->base.hpd_irq = true;

	if (gpio_hpd->cb && gpio_hpd->cb->attention)
		rc = gpio_hpd->cb->attention(gpio_hpd->dev);

error:
	return rc;
}

static irqreturn_t dp_gpio_isr(int unused, void *data)
{
	struct dp_gpio_hpd_private *gpio_hpd = data;
	u32 const disconnect_timeout_retry = 50;
	bool hpd;
	int i;

	if (!gpio_hpd)
		return IRQ_NONE;

	hpd = gpio_get_value_cansleep(gpio_hpd->gpio_cfg.gpio);

	if (!gpio_hpd->hpd && hpd) {
		gpio_hpd->hpd = true;
		queue_delayed_work(system_wq, &gpio_hpd->work, 0);
		return IRQ_HANDLED;
	}

	if (!gpio_hpd->hpd)
		return IRQ_HANDLED;

	/* In DP 1.2 spec, 100msec is recommended for the detection
	 * of HPD connect event. Here we'll poll HPD status for
	 * 50x2ms = 100ms and if HPD is always low, we know DP is
	 * disconnected. If HPD is high, HPD_IRQ will be handled
	 */
	for (i = 0; i < disconnect_timeout_retry; i++) {
		if (hpd) {
			dp_gpio_hpd_attention(gpio_hpd);
			return IRQ_HANDLED;
		}
		usleep_range(2000, 2100);
		hpd = gpio_get_value_cansleep(gpio_hpd->gpio_cfg.gpio);
	}

	gpio_hpd->hpd = false;
	queue_delayed_work(system_wq, &gpio_hpd->work, 0);
	return IRQ_HANDLED;
}

static void dp_gpio_hpd_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct dp_gpio_hpd_private *gpio_hpd = container_of(dw,
		struct dp_gpio_hpd_private, work);

	if (gpio_hpd->hpd) {
		devm_free_irq(gpio_hpd->dev,
			gpio_hpd->irq, gpio_hpd);
		devm_request_threaded_irq(gpio_hpd->dev,
			gpio_hpd->irq, NULL,
			dp_gpio_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"dp-gpio-intp", gpio_hpd);
		dp_gpio_hpd_connect(gpio_hpd, true);
	} else {
		devm_free_irq(gpio_hpd->dev,
				gpio_hpd->irq, gpio_hpd);
		devm_request_threaded_irq(gpio_hpd->dev,
			gpio_hpd->irq, NULL,
			dp_gpio_isr,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"dp-gpio-intp", gpio_hpd);
		dp_gpio_hpd_connect(gpio_hpd, false);
	}
}

static int dp_gpio_hpd_simulate_connect(struct dp_hpd *dp_hpd, bool hpd)
{
	int rc = 0;
	struct dp_gpio_hpd_private *gpio_hpd;

	if (!dp_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	gpio_hpd = container_of(dp_hpd, struct dp_gpio_hpd_private, base);

	dp_gpio_hpd_connect(gpio_hpd, hpd);
error:
	return rc;
}

static int dp_gpio_hpd_simulate_attention(struct dp_hpd *dp_hpd, int vdo)
{
	int rc = 0;
	struct dp_gpio_hpd_private *gpio_hpd;

	if (!dp_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	gpio_hpd = container_of(dp_hpd, struct dp_gpio_hpd_private, base);

	dp_gpio_hpd_attention(gpio_hpd);
error:
	return rc;
}

int dp_gpio_hpd_register(struct dp_hpd *dp_hpd)
{
	struct dp_gpio_hpd_private *gpio_hpd;
	int edge;
	int rc = 0;

	if (!dp_hpd)
		return -EINVAL;

	gpio_hpd = container_of(dp_hpd, struct dp_gpio_hpd_private, base);

	gpio_hpd->hpd = gpio_get_value_cansleep(gpio_hpd->gpio_cfg.gpio);

	edge = gpio_hpd->hpd ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	rc = devm_request_threaded_irq(gpio_hpd->dev, gpio_hpd->irq, NULL,
		dp_gpio_isr,
		edge | IRQF_ONESHOT,
		"dp-gpio-intp", gpio_hpd);
	if (rc) {
		pr_err("Failed to request INTP threaded IRQ: %d\n", rc);
		return rc;
	}

	if (gpio_hpd->hpd)
		queue_delayed_work(system_wq, &gpio_hpd->work, 0);

	return rc;
}

struct dp_hpd *dp_gpio_hpd_get(struct device *dev,
	struct dp_hpd_cb *cb)
{
	int rc = 0;
	const char *hpd_gpio_name = "qcom,dp-hpd-gpio";
	struct dp_gpio_hpd_private *gpio_hpd;
	struct dp_pinctrl pinctrl = {0};

	if (!dev || !cb) {
		pr_err("invalid device\n");
		rc = -EINVAL;
		goto error;
	}

	gpio_hpd = devm_kzalloc(dev, sizeof(*gpio_hpd), GFP_KERNEL);
	if (!gpio_hpd) {
		rc = -ENOMEM;
		goto error;
	}

	pinctrl.pin = devm_pinctrl_get(dev);
	if (!IS_ERR_OR_NULL(pinctrl.pin)) {
		pinctrl.state_hpd_active = pinctrl_lookup_state(pinctrl.pin,
						"mdss_dp_hpd_active");
		if (!IS_ERR_OR_NULL(pinctrl.state_hpd_active)) {
			rc = pinctrl_select_state(pinctrl.pin,
					pinctrl.state_hpd_active);
			if (rc) {
				pr_err("failed to set hpd active state\n");
				goto gpio_error;
			}
		}
	}

	gpio_hpd->gpio_cfg.gpio = of_get_named_gpio(dev->of_node,
		hpd_gpio_name, 0);
	if (!gpio_is_valid(gpio_hpd->gpio_cfg.gpio)) {
		pr_err("%s gpio not specified\n", hpd_gpio_name);
		rc = -EINVAL;
		goto gpio_error;
	}

	strlcpy(gpio_hpd->gpio_cfg.gpio_name, hpd_gpio_name,
		sizeof(gpio_hpd->gpio_cfg.gpio_name));
	gpio_hpd->gpio_cfg.value = 0;

	rc = gpio_request(gpio_hpd->gpio_cfg.gpio,
		gpio_hpd->gpio_cfg.gpio_name);
	if (rc) {
		pr_err("%s: failed to request gpio\n", hpd_gpio_name);
		goto gpio_error;
	}
	gpio_direction_input(gpio_hpd->gpio_cfg.gpio);

	gpio_hpd->dev = dev;
	gpio_hpd->cb = cb;
	gpio_hpd->irq = gpio_to_irq(gpio_hpd->gpio_cfg.gpio);
	INIT_DELAYED_WORK(&gpio_hpd->work, dp_gpio_hpd_work);

	gpio_hpd->base.simulate_connect = dp_gpio_hpd_simulate_connect;
	gpio_hpd->base.simulate_attention = dp_gpio_hpd_simulate_attention;
	gpio_hpd->base.register_hpd = dp_gpio_hpd_register;

	return &gpio_hpd->base;

gpio_error:
	devm_kfree(dev, gpio_hpd);
error:
	return ERR_PTR(rc);
}

void dp_gpio_hpd_put(struct dp_hpd *dp_hpd)
{
	struct dp_gpio_hpd_private *gpio_hpd;

	if (!dp_hpd)
		return;

	gpio_hpd = container_of(dp_hpd, struct dp_gpio_hpd_private, base);

	gpio_free(gpio_hpd->gpio_cfg.gpio);
	devm_kfree(gpio_hpd->dev, gpio_hpd);
}
