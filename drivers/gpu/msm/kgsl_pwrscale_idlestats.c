/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/idle_stats_device.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"

struct idlestats_priv {
	char name[32];
	struct msm_idle_stats_device idledev;
	struct kgsl_device *device;
	struct msm_idle_pulse pulse;
};

static void idlestats_get_sample(struct msm_idle_stats_device *idledev,
	struct msm_idle_pulse *pulse)
{
	struct kgsl_power_stats stats;
	struct idlestats_priv *priv = container_of(idledev,
		struct idlestats_priv, idledev);
	struct kgsl_device *device = priv->device;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	mutex_lock(&device->mutex);
	/* If the GPU is asleep, don't wake it up - assume that we
	   are idle */

	if (!(device->state & (KGSL_STATE_SLEEP | KGSL_STATE_NAP))) {
		device->ftbl->power_stats(device, &stats);
		pulse->busy_start_time = pwr->time - stats.busy_time;
		pulse->busy_interval = stats.busy_time;
	} else {
		pulse->busy_start_time = pwr->time;
		pulse->busy_interval = 0;
	}
	pulse->wait_interval = 0;
	mutex_unlock(&device->mutex);
}

static void idlestats_busy(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv = pwrscale->priv;
	if (priv->pulse.busy_start_time != 0)
		msm_idle_stats_idle_end(&priv->idledev, &priv->pulse);
	priv->pulse.busy_start_time = ktime_to_us(ktime_get());
}

static void idlestats_idle(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct kgsl_power_stats stats;
	struct idlestats_priv *priv = pwrscale->priv;

	/* This is called from within a mutex protected function, so
	   no additional locking required */
	device->ftbl->power_stats(device, &stats);

	/* If total_time is zero, then we don't have
	   any interesting statistics to store */
	if (stats.total_time == 0) {
		priv->pulse.busy_start_time = 0;
		return;
	}

	priv->pulse.busy_interval   = stats.busy_time;
	priv->pulse.wait_interval   = 0;
	msm_idle_stats_idle_start(&priv->idledev);
}

static int idlestats_init(struct kgsl_device *device,
		     struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv;
	int ret;

	priv = pwrscale->priv = kzalloc(sizeof(struct idlestats_priv),
		GFP_KERNEL);
	if (pwrscale->priv == NULL)
		return -ENOMEM;

	snprintf(priv->name, sizeof(priv->name), "idle_stats_%s",
		 device->name);

	priv->device = device;

	priv->idledev.name = (const char *) priv->name;
	priv->idledev.get_sample = idlestats_get_sample;

	ret = msm_idle_stats_register_device(&priv->idledev);

	if (ret) {
		kfree(pwrscale->priv);
		pwrscale->priv = NULL;
	}

	return ret;
}

static void idlestats_close(struct kgsl_device *device,
		      struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv = pwrscale->priv;

	if (pwrscale->priv == NULL)
		return;

	msm_idle_stats_deregister_device(&priv->idledev);

	kfree(pwrscale->priv);
	pwrscale->priv = NULL;
}

struct kgsl_pwrscale_policy kgsl_pwrscale_policy_idlestats = {
	.name = "idlestats",
	.init = idlestats_init,
	.idle = idlestats_idle,
	.busy = idlestats_busy,
	.close = idlestats_close
};
