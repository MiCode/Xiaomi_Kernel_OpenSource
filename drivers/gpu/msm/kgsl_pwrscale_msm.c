/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include "kgsl_trace.h"

struct msm_priv {
	struct kgsl_device		*device;
	int				enabled;
	unsigned int			cur_freq;
	unsigned int			req_level;
	int				floor_level;
	struct msm_dcvs_core_info	*core_info;
	int				gpu_busy;
	int				dcvs_core_id;
};

/* reference to be used in idle and freq callbacks */
static struct msm_priv *the_msm_priv;

static int msm_idle_enable(int type_core_num,
		enum msm_core_control_event event)
{
	struct msm_priv *priv = the_msm_priv;

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
static int msm_set_freq(int core_num, unsigned int freq)
{
	int i, delta = 5000000;
	struct msm_priv *priv = the_msm_priv;
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
	priv->req_level = i;
	if (priv->req_level <= priv->floor_level) {
		kgsl_pwrctrl_pwrlevel_change(device, priv->req_level);
		priv->cur_freq = pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq;
	}
	mutex_unlock(&device->mutex);

	/* return current frequency in kHz */
	return priv->cur_freq / 1000;
}

static int msm_set_min_freq(int core_num, unsigned int freq)
{
	int i, delta = 5000000;
	struct msm_priv *priv = the_msm_priv;
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
	priv->floor_level = i;
	if (priv->floor_level <= priv->req_level)
		kgsl_pwrctrl_pwrlevel_change(device, priv->floor_level);
	else if (priv->floor_level > priv->req_level)
		kgsl_pwrctrl_pwrlevel_change(device, priv->req_level);

	priv->cur_freq = pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq;
	mutex_unlock(&device->mutex);

	/* return current frequency in kHz */
	return priv->cur_freq / 1000;
}

static unsigned int msm_get_freq(int core_num)
{
	struct msm_priv *priv = the_msm_priv;

	/* return current frequency in kHz */
	return priv->cur_freq / 1000;
}

static void msm_busy(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;
	if (priv->enabled && !priv->gpu_busy) {
		msm_dcvs_idle(priv->dcvs_core_id, MSM_DCVS_IDLE_EXIT, 0);
		trace_kgsl_mpdcvs(device, 1);
		priv->gpu_busy = 1;
	}
	return;
}

static void msm_idle(struct kgsl_device *device,
		struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;

	if (priv->enabled && priv->gpu_busy)
		if (device->ftbl->isidle(device)) {
			msm_dcvs_idle(priv->dcvs_core_id,
					MSM_DCVS_IDLE_ENTER, 0);
			trace_kgsl_mpdcvs(device, 0);
			priv->gpu_busy = 0;
		}
	return;
}

static void msm_sleep(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv = pwrscale->priv;

	if (priv->enabled && priv->gpu_busy) {
		msm_dcvs_idle(priv->dcvs_core_id, MSM_DCVS_IDLE_ENTER, 0);
		trace_kgsl_mpdcvs(device, 0);
		priv->gpu_busy = 0;
	}

	return;
}

static void msm_set_io_fraction(struct kgsl_device *device,
				unsigned int value)
{
	int i;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	for (i = 0; i < pwr->num_pwrlevels; i++)
		pwr->pwrlevels[i].io_fraction = value;

}

static void msm_restore_io_fraction(struct kgsl_device *device)
{
	int i;
	struct kgsl_device_platform_data *pdata =
				kgsl_device_get_drvdata(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	for (i = 0; i < pdata->num_levels; i++)
		pwr->pwrlevels[i].io_fraction =
			pdata->pwrlevel[i].io_fraction;
}

static int msm_init(struct kgsl_device *device,
		     struct kgsl_pwrscale *pwrscale)
{
	struct msm_priv *priv;
	struct msm_dcvs_freq_entry *tbl;
	int i, ret = -EINVAL, low_level;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);
	struct kgsl_device_platform_data *pdata = pdev->dev.platform_data;

	if (the_msm_priv) {
		priv = pwrscale->priv = the_msm_priv;
	} else {
		priv = pwrscale->priv = kzalloc(sizeof(struct msm_priv),
			GFP_KERNEL);
		if (pwrscale->priv == NULL)
			return -ENOMEM;

		priv->core_info = pdata->core_info;
		tbl = priv->core_info->freq_tbl;
		priv->floor_level = pwr->num_pwrlevels - 1;
		/* Fill in frequency table from low to high, reversing order. */
		low_level = pwr->num_pwrlevels - KGSL_PWRLEVEL_LAST_OFFSET;
		for (i = 0; i <= low_level; i++)
			tbl[i].freq =
				pwr->pwrlevels[low_level - i].gpu_freq / 1000;
		priv->dcvs_core_id =
				msm_dcvs_register_core(MSM_DCVS_CORE_TYPE_GPU,
				0,
				priv->core_info,
				msm_set_freq, msm_get_freq, msm_idle_enable,
				msm_set_min_freq,
				priv->core_info->sensors[0]);
		if (priv->dcvs_core_id < 0) {
			KGSL_PWR_ERR(device, "msm_dcvs_register_core failed");
			goto err;
		}
		the_msm_priv = priv;
	}
	priv->device = device;
	ret = msm_dcvs_freq_sink_start(priv->dcvs_core_id);
	if (ret >= 0) {
		if (device->ftbl->isidle(device)) {
			priv->gpu_busy = 0;
			msm_dcvs_idle(priv->dcvs_core_id,
					MSM_DCVS_IDLE_ENTER, 0);
		} else {
			priv->gpu_busy = 1;
		}
		msm_set_io_fraction(device, 0);
		return 0;
	}

	KGSL_PWR_ERR(device, "msm_dcvs_freq_sink_register failed\n");

err:
	if (!the_msm_priv)
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
	msm_dcvs_freq_sink_stop(priv->dcvs_core_id);
	pwrscale->priv = NULL;
	msm_restore_io_fraction(device);
}

struct kgsl_pwrscale_policy kgsl_pwrscale_policy_msm = {
	.name = "msm",
	.init = msm_init,
	.idle = msm_idle,
	.busy = msm_busy,
	.sleep = msm_sleep,
	.close = msm_close,
};
