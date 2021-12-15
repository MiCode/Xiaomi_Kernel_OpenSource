// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2011-2014, 2017-2019 The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>

#define DRV_NAME "msm-pcm-hostless"

static int msm_pcm_hostless_prepare(struct snd_pcm_substream *substream)
{
	if (!substream) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	pm_qos_remove_request(&substream->latency_pm_qos_req);
	return 0;
}

static const struct snd_pcm_ops msm_pcm_hostless_ops = {
	.prepare = msm_pcm_hostless_prepare
};

static struct snd_soc_component_driver msm_soc_hostless_component = {
	.name		= DRV_NAME,
	.ops		= &msm_pcm_hostless_ops,
};

static int msm_pcm_hostless_probe(struct platform_device *pdev)
{

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
				&msm_soc_hostless_component,
				NULL, 0);
}

static int msm_pcm_hostless_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_pcm_hostless_dt_match[] = {
	{.compatible = "qcom,msm-pcm-hostless"},
	{}
};

static struct platform_driver msm_pcm_hostless_driver = {
	.driver = {
		.name = "msm-pcm-hostless",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_hostless_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_pcm_hostless_probe,
	.remove = msm_pcm_hostless_remove,
};

int __init msm_pcm_hostless_init(void)
{
	return platform_driver_register(&msm_pcm_hostless_driver);
}

void msm_pcm_hostless_exit(void)
{
	platform_driver_unregister(&msm_pcm_hostless_driver);
}

MODULE_DESCRIPTION("Hostless platform driver");
MODULE_LICENSE("GPL v2");
