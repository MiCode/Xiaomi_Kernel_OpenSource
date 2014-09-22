/*
*Copyright (c) 2014, The Linux Foundation. All rights reserved.
*
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 and
*only version 2 as published by the Free Software Foundation.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*/
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/msm-bus.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/qcom/hvc.h>

#include "governor.h"
#include "devfreq_spdm.h"

#define DEVFREQ_SPDM_DEFAULT_WINDOW_MS 100

static int change_bw(struct device *dev, unsigned long *freq, u32 flags)
{
	struct spdm_data *data = 0;
	int i;
	int next_idx;
	int ret = 0;
	struct hvc_desc desc = { { 0 } };
	int hvc_status = 0;

	if (!dev || !freq)
		return -EINVAL;

	data = dev_get_drvdata(dev);
	if (!data)
		return -EINVAL;

	if (data->devfreq->previous_freq == *freq)
		goto update_thresholds;

	next_idx = data->cur_idx + 1;
	next_idx = next_idx % 2;

	for (i = 0; i < data->pdata->usecase[next_idx].num_paths; i++)
		data->pdata->usecase[next_idx].vectors[i].ab = (*freq) << 6;

	data->cur_idx = next_idx;
	ret = msm_bus_scale_client_update_request(data->bus_scale_client_id,
						  data->cur_idx);

update_thresholds:
	desc.arg[0] = SPDM_CMD_ENABLE;
	desc.arg[1] = data->spdm_client;
	desc.arg[2] = clk_get_rate(data->cci_clk);
	hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
	if (hvc_status)
		pr_err("HVC command %u failed with error %u", (int)desc.arg[0],
			hvc_status);
	return ret;
}

static int get_cur_bw(struct device *dev, unsigned long *freq)
{
	struct spdm_data *data = 0;

	if (!dev || !freq)
		return -EINVAL;

	data = dev_get_drvdata(dev);
	if (!data)
		return -EINVAL;

	*freq = data->pdata->usecase[data->cur_idx].vectors[0].ab >> 6;

	return 0;
}

static int get_dev_status(struct device *dev, struct devfreq_dev_status *status)
{
	struct spdm_data *data = 0;
	int ret;

	if (!dev || !status)
		return -EINVAL;

	data = dev_get_drvdata(dev);
	if (!data)
		return -EINVAL;

	/* determine if we want to go up or down based on the notification */
	if (data->action == SPDM_UP)
		status->busy_time = 255;
	else
		status->busy_time = 0;
	status->total_time = 255;
	ret = get_cur_bw(dev, &status->current_frequency);
	if (ret)
		return ret;

	return 0;

}

static int populate_config_data(struct spdm_data *data,
				struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct device_node *node = pdev->dev.of_node;
	struct property *prop = 0;

	ret = of_property_read_u32(node, "qcom,max-vote",
				   &data->config_data.max_vote);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,bw-upstep",
				   &data->config_data.upstep);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,bw-dwnstep",
				   &data->config_data.downstep);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,alpha-up",
				   &data->config_data.aup);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,alpha-down",
				   &data->config_data.adown);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,bucket-size",
				   &data->config_data.bucket_size);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(node, "qcom,pl-freqs",
					 data->config_data.pl_freqs,
					 SPDM_PL_COUNT - 1);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(node, "qcom,reject-rate",
					 data->config_data.reject_rate,
					 SPDM_PL_COUNT * 2);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(node, "qcom,response-time-us",
					 data->config_data.response_time_us,
					 SPDM_PL_COUNT * 2);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(node, "qcom,cci-response-time-us",
					 data->config_data.cci_response_time_us,
					 SPDM_PL_COUNT * 2);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "qcom,max-cci-freq",
				   &data->config_data.max_cci_freq);
	if (ret)
		return ret;
	ret = of_property_read_u32(node, "qcom,up-step-multp",
				   &data->config_data.up_step_multp);
	if (ret)
		return ret;

	prop = of_find_property(node, "qcom,ports", 0);
	if (!prop)
		return -EINVAL;
	data->config_data.num_ports = prop->length / sizeof(u32);
	data->config_data.ports =
	    devm_kzalloc(&pdev->dev, prop->length, GFP_KERNEL);
	if (!data->config_data.ports)
		return -ENOMEM;
	ret = of_property_read_u32_array(node, "qcom,ports",
					 data->config_data.ports,
					 data->config_data.num_ports);
	if (ret) {
		devm_kfree(&pdev->dev, data->config_data.ports);
		return ret;
	}

	return 0;
}

static int populate_spdm_data(struct spdm_data *data,
			      struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct device_node *node = pdev->dev.of_node;

	ret = populate_config_data(data, pdev);
	if (ret)
		return ret;

	ret =
	    of_property_read_u32(node, "qcom,spdm-client", &data->spdm_client);
	if (ret)
		goto no_client;

	ret = of_property_read_u32(node, "qcom,spdm-interval", &data->window);
	if (ret)
		data->window = DEVFREQ_SPDM_DEFAULT_WINDOW_MS;

	data->pdata = msm_bus_cl_get_pdata(pdev);
	if (!data->pdata) {
		ret = -EINVAL;
		goto no_pdata;
	}

	return 0;

no_client:
no_pdata:
	devm_kfree(&pdev->dev, data->config_data.ports);
	return ret;
}

static int probe(struct platform_device *pdev)
{
	struct spdm_data *data = 0;
	int ret = -EINVAL;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->action = SPDM_DOWN;

	platform_set_drvdata(pdev, data);

	ret = populate_spdm_data(data, pdev);
	if (ret)
		goto bad_of;

	data->bus_scale_client_id = msm_bus_scale_register_client(data->pdata);
	if (!data->bus_scale_client_id) {
		ret = -EINVAL;
		goto no_bus_scaling;
	}

	data->cci_clk = clk_get(&pdev->dev, "cci_clk");
	if (IS_ERR(data->cci_clk)) {
		ret = PTR_ERR(data->cci_clk);
		goto no_clock;
	}

	data->profile =
	    devm_kzalloc(&pdev->dev, sizeof(*(data->profile)), GFP_KERNEL);
	if (!data->profile) {
		ret = -ENOMEM;
		goto no_profile;
	}
	data->profile->target = change_bw;
	data->profile->get_dev_status = get_dev_status;
	data->profile->get_cur_freq = get_cur_bw;
	data->profile->polling_ms = data->window;

	data->devfreq =
	    devfreq_add_device(&pdev->dev, data->profile, "spdm_bw_hyp", data);
	if (IS_ERR(data->devfreq)) {
		ret = PTR_ERR(data->devfreq);
		goto no_profile;
	}

	return 0;

no_profile:
no_clock:
	msm_bus_scale_unregister_client(data->bus_scale_client_id);
no_bus_scaling:
	devm_kfree(&pdev->dev, data->config_data.ports);
bad_of:
	devm_kfree(&pdev->dev, data);
	return ret;
}

static int remove(struct platform_device *pdev)
{
	struct spdm_data *data = 0;

	data = platform_get_drvdata(pdev);

	if (data->devfreq)
		devfreq_remove_device(data->devfreq);

	if (data->profile)
		devm_kfree(&pdev->dev, data->profile);

	if (data->bus_scale_client_id)
		msm_bus_scale_unregister_client(data->bus_scale_client_id);

	if (data->config_data.ports)
		devm_kfree(&pdev->dev, data->config_data.ports);

	devm_kfree(&pdev->dev, data);

	return 0;
}

static const struct of_device_id devfreq_spdm_match[] = {
	{.compatible = "qcom,devfreq_spdm"},
	{}
};

static struct platform_driver devfreq_spdm_drvr = {
	.driver = {
		   .name = "devfreq_spdm",
		   .owner = THIS_MODULE,
		   .of_match_table = devfreq_spdm_match,
		   },
	.probe = probe,
	.remove = remove,
};

static int __init devfreq_spdm_init(void)
{
	return platform_driver_register(&devfreq_spdm_drvr);
}

module_init(devfreq_spdm_init);

MODULE_LICENSE("GPL v2");
