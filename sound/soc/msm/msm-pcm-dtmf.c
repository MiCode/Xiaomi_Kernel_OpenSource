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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/q6afe.h>

static int msm_dtmf_rx_generate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	uint16_t low_freq = ucontrol->value.integer.value[0];
	uint16_t high_freq = ucontrol->value.integer.value[1];
	int64_t duration = ucontrol->value.integer.value[2];
	uint16_t gain = ucontrol->value.integer.value[3];

	pr_debug("%s: low_freq=%d high_freq=%d duration=%d gain=%d\n",
		 __func__, low_freq, high_freq, (int)duration, gain);
	afe_dtmf_generate_rx(duration, high_freq, low_freq, gain);
	return 0;
}

static int msm_dtmf_rx_generate_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s:\n", __func__);
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static struct snd_kcontrol_new msm_dtmf_controls[] = {
	SOC_SINGLE_MULTI_EXT("DTMF_Generate Rx Low High Duration Gain",
			     SND_SOC_NOPM, 0, 5000, 0, 4,
			     msm_dtmf_rx_generate_get,
			     msm_dtmf_rx_generate_put),
};

static int msm_pcm_dtmf_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, msm_dtmf_controls,
				      ARRAY_SIZE(msm_dtmf_controls));
	return 0;
}

static struct snd_pcm_ops msm_pcm_ops = {};

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.probe		= msm_pcm_dtmf_probe,
};

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-dtmf",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("DTMF platform driver");
MODULE_LICENSE("GPL v2");
