// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/msm-bus.h>

struct proxy_client {
	struct msm_bus_scale_pdata *pdata;
	unsigned int client_handle;
};

static struct proxy_client proxy_client_info;

static int msm_bus_device_proxy_client_probe(struct platform_device *pdev)
{
	int ret;

	proxy_client_info.pdata = msm_bus_cl_get_pdata(pdev);

	if (!proxy_client_info.pdata)
		return 0;

	proxy_client_info.client_handle =
		msm_bus_scale_register_client(proxy_client_info.pdata);

	if (!proxy_client_info.client_handle) {
		dev_err(&pdev->dev, "Unable to register bus client\n");
		return -ENODEV;
	}

	ret = msm_bus_scale_client_update_request(
					proxy_client_info.client_handle, 1);
	if (ret)
		dev_err(&pdev->dev, "Bandwidth update failed (%d)\n", ret);

	return ret;
}

static const struct of_device_id proxy_client_match[] = {
	{.compatible = "qcom,bus-proxy-client"},
	{}
};

static struct platform_driver msm_bus_proxy_client_driver = {
	.probe = msm_bus_device_proxy_client_probe,
	.driver = {
		.name = "msm_bus_proxy_client_device",
		.of_match_table = proxy_client_match,
	},
};

static int __init msm_bus_proxy_client_init_driver(void)
{
	int rc;

	rc =  platform_driver_register(&msm_bus_proxy_client_driver);
	if (rc) {
		pr_err("Failed to register proxy client device driver\n");
		return rc;
	}

	return rc;
}

static int __init msm_bus_proxy_client_unvote(void)
{
	int ret;

	if (!proxy_client_info.pdata || !proxy_client_info.client_handle)
		return 0;

	ret = msm_bus_scale_client_update_request(
					proxy_client_info.client_handle, 0);
	if (ret)
		pr_err("%s: bandwidth update request failed (%d)\n",
			__func__, ret);

	msm_bus_scale_unregister_client(proxy_client_info.client_handle);

	return 0;
}

subsys_initcall_sync(msm_bus_proxy_client_init_driver);
late_initcall_sync(msm_bus_proxy_client_unvote);
