/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <mach/msm_dcvs.h>
#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "a2xx_reg.h"

struct msm_priv {
	struct kgsl_device *device;
	int enabled;
	int handle;
	unsigned int cur_freq;
	struct msm_dcvs_idle idle_source;
	struct msm_dcvs_freq freq_sink;
	struct msm_dcvs_core_info *core_info;
	int gpu_busy;
};

static int msm_idle_enable(struct msm_dcvs_idle *self,
					enum msm_core_control_event event)
{
	struct msm_priv *priv = container_of(self, struct msm_priv,
								idle_source);

	switch (event) {
	case MSM_DCVS_ENABLE_IDLE_PULSE:
		priv->enabled = true;
		break;
	case MSM_DCVS_DISABLE_IDLE_PULSE:
		priv->enabled = false;
		break;
	case MSM_DCVS_ENABLE_HIGH_LATENCY_MODES:
	case MSM_DCVS_DISABLE_HIGH_LATENCY_MODES:
		break;
	}
	return 0;
}

/* Set the requested frequency if it is within 5MHz (delta) of a
 * supported frequency.
 */
static int msm_set_freq(struct msm_dcvs_freq *self,
						unsigned int freq)
{
	int i, delta = 5000000;
	struct msm_priv *priv = container_of(self, struct msm_priv,
								freq_sink);
	struct kgsl_device *device = priv->device;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	/* msm_dcvs manager uses frequencies in kHz */
	freq *= 1000;
	for (i = 0; i < pwr->num_pwrlevels; i++)
		if (abs(pwr->pwrlevels[i].gpu_freq - freq) < delta)
			break;
	if (i == pwr->num_pwrlevels)
		return 0;

	mutex_lock(&device->mutex);
	kgsl_pwrctrl_pwrlevel_change(device, i);
	priv->cur_freq = pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq;
	mutex_unlock(&device->mutex);

	/* return current frequency in kHz */
	return priv->cur_freq / 1000;
}

static unsigned int msm_get_freq(struct msm_dcvs_freq *self)
{
	struct msm_priv *priv = container_of(self, struct msm_priv,
								freq_sink);
	/* return current frequency in kHz */
	return priv->cur_freq / 1000;
}

static void msm_busy(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;
	if (priv->enabled && !priv->gpu_busy) {
		msm_dcvs_idle(priv->handle, MSM_DCVS_IDLE_EXIT, 0);
		priv->gpu_busy = 1;
	}
	return;
}

static void msm_idle(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale, unsigned int ignore_idle)
{
	struct msm_priv *priv = pwrscale->priv;

	if (priv->enabled && priv->gpu_busy)
		if (device->ftbl->isidle(device)) {
			msm_dcvs_idle(priv->handle, MSM_DCVS_IDLE_ENTER, 0);
			priv->gpu_busy = 0;
		}
	return;
}

static void msm_sleep(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;

	if (priv->enabled && priv->gpu_busy) {
		msm_dcvs_idle(priv->handle, MSM_DCVS_IDLE_ENTER, 0);
		priv->gpu_busy = 0;
	}

	return;
}

static int msm_init(struct kgsl_device *device,
		     struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv;
	struct msm_dcvs_freq_entry *tbl;
	int i, ret, low_level;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);
	struct kgsl_device_platform_data *pdata = pdev->dev.platform_data;

	priv = pwrscale->priv = kzalloc(sizeof(struct msm_priv),
		GFP_KERNEL);
	if (pwrscale->priv == NULL)
		return -ENOMEM;

	priv->core_info = pdata->core_info;
	tbl = priv->core_info->freq_tbl;
	/* Fill in frequency table from low to high, reversing order. */
	low_level = pwr->num_pwrlevels - KGSL_PWRLEVEL_LAST_OFFSET;
	for (i = 0; i <= low_level; i++)
		tbl[i].freq =
			pwr->pwrlevels[low_level - i].gpu_freq / 1000;
	ret = msm_dcvs_register_core(device->name, 0, priv->core_info);
	if (ret) {
		KGSL_PWR_ERR(device, "msm_dcvs_register_core failed");
		goto err;
	}

	priv->device = device;
	priv->idle_source.enable = msm_idle_enable;
	priv->idle_source.core_name = device->name;
	priv->handle = msm_dcvs_idle_source_register(&priv->idle_source);
	if (priv->handle < 0) {
		ret = priv->handle;
		KGSL_PWR_ERR(device, "msm_dcvs_idle_source_register failed\n");
		goto err;
	}

	priv->freq_sink.core_name = device->name;
	priv->freq_sink.set_frequency = msm_set_freq;
	priv->freq_sink.get_frequency = msm_get_freq;
	ret = msm_dcvs_freq_sink_register(&priv->freq_sink);
	if (ret >= 0) {
		if (device->ftbl->isidle(device)) {
			priv->gpu_busy = 0;
			msm_dcvs_idle(priv->handle, MSM_DCVS_IDLE_ENTER, 0);
		} else {
			priv->gpu_busy = 1;
		}
		return 0;
	}

	KGSL_PWR_ERR(device, "msm_dcvs_freq_sink_register failed\n");
	msm_dcvs_idle_source_unregister(&priv->idle_source);

err:
	kfree(pwrscale->priv);
	pwrscale->priv = NULL;

	return ret;
}

static void msm_close(struct kgsl_device *device,
		      struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;

	if (pwrscale->priv == NULL)
		return;
	msm_dcvs_idle_source_unregister(&priv->idle_source);
	msm_dcvs_freq_sink_unregister(&priv->freq_sink);
	kfree(pwrscale->priv);
	pwrscale->priv = NULL;
}

struct kgsl_pwrscale_policy kgsl_pwrscale_policy_msm = {
	.name = "msm",
	.init = msm_init,
	.idle = msm_idle,
	.busy = msm_busy,
	.sleep = msm_sleep,
	.close = msm_close,
};
