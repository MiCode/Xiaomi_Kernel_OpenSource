/* Copyright (c) 2011-2014, 2017 The Linux Foundation. All rights reserved.
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
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <linux/qdsp6v2/apr.h>

struct hostless_pdata {
	struct work_struct msm_test_add_child_dev_work;
	struct device *dev;
};

#define AUDIO_TEST_MOD_STRING_LEN 30

static void msm_test_add_child_dev(struct work_struct *work)
{
	struct hostless_pdata *pdata;
	struct platform_device *pdev;
	struct device_node *node;
	int ret;
	char plat_dev_name[AUDIO_TEST_MOD_STRING_LEN];
	int adsp_state;

	pdata = container_of(work, struct hostless_pdata,
			     msm_test_add_child_dev_work);
	if (!pdata) {
		pr_err("%s: Memory for pdata does not exist\n",
			__func__);
		return;
	}
	if (!pdata->dev) {
		pr_err("%s: pdata dev is not initialized\n", __func__);
		return;
	}
	if (!pdata->dev->of_node) {
		dev_err(pdata->dev,
			"%s: DT node for pdata does not exist\n", __func__);
		return;
	}

	adsp_state = apr_get_subsys_state();
	while (adsp_state != APR_SUBSYS_LOADED) {
		dev_dbg(pdata->dev, "Adsp is not loaded yet %d\n",
			adsp_state);
		msleep(500);
		adsp_state = apr_get_subsys_state();
	}
	msleep(1000);
	for_each_child_of_node(pdata->dev->of_node, node) {
		if (!strcmp(node->name, "audio_test_mod"))
			strlcpy(plat_dev_name, "audio_test_mod",
				(AUDIO_TEST_MOD_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(pdata->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = pdata->dev;
		pdev->dev.of_node = node;

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}
	}
	return;
fail_pdev_add:
	platform_device_put(pdev);
err:
	return;
}

static int msm_pcm_hostless_prepare(struct snd_pcm_substream *substream)
{
	if (!substream) {
		pr_err("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (pm_qos_request_active(&substream->latency_pm_qos_req))
		pm_qos_remove_request(&substream->latency_pm_qos_req);

	return 0;
}

static struct snd_pcm_ops msm_pcm_hostless_ops = {
	.prepare = msm_pcm_hostless_prepare
};

static struct snd_soc_platform_driver msm_soc_hostless_platform = {
	.ops		= &msm_pcm_hostless_ops,
};

static int msm_pcm_hostless_probe(struct platform_device *pdev)
{
	struct hostless_pdata *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	pdata->dev = &pdev->dev;
	INIT_WORK(&pdata->msm_test_add_child_dev_work,
		  msm_test_add_child_dev);
	schedule_work(&pdata->msm_test_add_child_dev_work);
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_hostless_platform);
}

static int msm_pcm_hostless_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
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
	},
	.probe = msm_pcm_hostless_probe,
	.remove = msm_pcm_hostless_remove,
};

static int __init msm_soc_platform_init(void)
{
	return platform_driver_register(&msm_pcm_hostless_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_hostless_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("Hostless platform driver");
MODULE_LICENSE("GPL v2");
