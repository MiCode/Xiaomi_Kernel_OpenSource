/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include "dp_bridge_hpd.h"

struct dp_bridge_hpd_private {
	struct device *dev;
	struct dp_hpd base;
	struct msm_dp_aux_bridge *bridge;
	struct delayed_work work;
	struct dp_hpd_cb *cb;
	bool hpd;
	bool hpd_irq;
	struct mutex hpd_lock;
};

static int dp_bridge_hpd_connect(struct dp_bridge_hpd_private *bridge_hpd,
		bool hpd)
{
	int rc = 0;

	if (!bridge_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	bridge_hpd->base.hpd_high = hpd;
	bridge_hpd->base.alt_mode_cfg_done = hpd;
	bridge_hpd->base.hpd_irq = false;

	if (!bridge_hpd->cb ||
		!bridge_hpd->cb->configure ||
		!bridge_hpd->cb->disconnect) {
		pr_err("invalid cb\n");
		rc = -EINVAL;
		goto error;
	}

	if (hpd)
		rc = bridge_hpd->cb->configure(bridge_hpd->dev);
	else
		rc = bridge_hpd->cb->disconnect(bridge_hpd->dev);

error:
	return rc;
}

static int dp_bridge_hpd_attention(struct dp_bridge_hpd_private *bridge_hpd)
{
	int rc = 0;

	if (!bridge_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	bridge_hpd->base.hpd_irq = true;

	if (bridge_hpd->cb && bridge_hpd->cb->attention)
		rc = bridge_hpd->cb->attention(bridge_hpd->dev);

error:
	return rc;
}

static void dp_bridge_hpd_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct dp_bridge_hpd_private *bridge_hpd = container_of(dw,
		struct dp_bridge_hpd_private, work);

	mutex_lock(&bridge_hpd->hpd_lock);

	if (bridge_hpd->hpd_irq)
		dp_bridge_hpd_attention(bridge_hpd);
	else
		dp_bridge_hpd_connect(bridge_hpd, bridge_hpd->hpd);

	mutex_unlock(&bridge_hpd->hpd_lock);
}

static int dp_bridge_hpd_simulate_connect(struct dp_hpd *dp_hpd, bool hpd)
{
	int rc = 0;
	struct dp_bridge_hpd_private *bridge_hpd;

	if (!dp_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	bridge_hpd = container_of(dp_hpd, struct dp_bridge_hpd_private, base);

	dp_bridge_hpd_connect(bridge_hpd, hpd);
error:
	return rc;
}

static int dp_bridge_hpd_simulate_attention(struct dp_hpd *dp_hpd, int vdo)
{
	int rc = 0;
	struct dp_bridge_hpd_private *bridge_hpd;

	if (!dp_hpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	bridge_hpd = container_of(dp_hpd, struct dp_bridge_hpd_private, base);

	dp_bridge_hpd_attention(bridge_hpd);
error:
	return rc;
}

static int dp_bridge_hpd_cb(void *dp_hpd, bool hpd, bool hpd_irq)
{
	struct dp_bridge_hpd_private *bridge_hpd = dp_hpd;

	mutex_lock(&bridge_hpd->hpd_lock);

	bridge_hpd->hpd = hpd;
	bridge_hpd->hpd_irq = hpd_irq;
	queue_delayed_work(system_wq, &bridge_hpd->work, 0);

	mutex_unlock(&bridge_hpd->hpd_lock);

	return 0;
}

static int dp_bridge_hpd_register(struct dp_hpd *dp_hpd)
{
	struct dp_bridge_hpd_private *bridge_hpd;

	if (!dp_hpd)
		return -EINVAL;

	bridge_hpd = container_of(dp_hpd, struct dp_bridge_hpd_private, base);

	return bridge_hpd->bridge->register_hpd(bridge_hpd->bridge,
			dp_bridge_hpd_cb, bridge_hpd);
}

struct dp_hpd *dp_bridge_hpd_get(struct device *dev,
	struct dp_hpd_cb *cb, struct msm_dp_aux_bridge *aux_bridge)
{
	int rc = 0;
	struct dp_bridge_hpd_private *bridge_hpd;

	if (!dev || !cb) {
		pr_err("invalid device\n");
		rc = -EINVAL;
		goto error;
	}

	bridge_hpd = devm_kzalloc(dev, sizeof(*bridge_hpd), GFP_KERNEL);
	if (!bridge_hpd) {
		rc = -ENOMEM;
		goto error;
	}

	bridge_hpd->dev = dev;
	bridge_hpd->cb = cb;
	bridge_hpd->bridge = aux_bridge;
	mutex_init(&bridge_hpd->hpd_lock);
	INIT_DELAYED_WORK(&bridge_hpd->work, dp_bridge_hpd_work);
	bridge_hpd->base.simulate_connect = dp_bridge_hpd_simulate_connect;
	bridge_hpd->base.simulate_attention = dp_bridge_hpd_simulate_attention;
	bridge_hpd->base.register_hpd = dp_bridge_hpd_register;

	return &bridge_hpd->base;
error:
	return ERR_PTR(rc);
}

void dp_bridge_hpd_put(struct dp_hpd *dp_hpd)
{
	struct dp_bridge_hpd_private *bridge_hpd;

	if (!dp_hpd)
		return;

	bridge_hpd = container_of(dp_hpd, struct dp_bridge_hpd_private, base);

	devm_kfree(bridge_hpd->dev, bridge_hpd);
}
