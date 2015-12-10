/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include "governor.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_internal.h"
#include "venus_hfi.h"

struct msm_vidc_bus_table_gov {
	struct {
		u32 load, freq, codecs;
	} *bus_table;
	int bus_table_size;
	struct devfreq_governor devfreq_gov;
};

int msm_vidc_table_get_target_freq(struct devfreq *dev, unsigned long *freq,
		u32 *flag)
{
	struct devfreq_dev_status status = {0};
	struct msm_vidc_gov_data *vidc_data = NULL;
	struct msm_vidc_bus_table_gov *gov = NULL;
	enum vidc_vote_data_session sess_type = 0;
	u32 load = 0, i = 0;
	int j = 0;

	if (!dev || !freq || !flag) {
		dprintk(VIDC_ERR, "%s: Invalid params %p, %p, %p\n",
			__func__, dev, freq, flag);
		return -EINVAL;
	}

	gov = container_of(dev->governor,
			struct msm_vidc_bus_table_gov, devfreq_gov);
	dev->profile->get_dev_status(dev->dev.parent, &status);
	vidc_data = (struct msm_vidc_gov_data *)status.private_data;

	*freq = 0;
	for (i = 0; i < vidc_data->data_count; ++i) {
		struct vidc_bus_vote_data *curr = &vidc_data->data[i];
		u32 frequency = 0;

		load = NUM_MBS_PER_SEC(curr->width, curr->height, curr->fps);
		sess_type = VIDC_VOTE_DATA_SESSION_VAL(
			curr->codec, curr->domain);

		if (curr->power_mode == VIDC_POWER_TURBO) {
			dprintk(VIDC_DBG, "found turbo session[%d] %#x\n",
				i, sess_type);
			*freq = INT_MAX;
			goto exit;
		}

		/*
		 * loop over bus table and select frequency of
		 * matching session and appropriate load
		 */
		for (j = gov->bus_table_size - 1; j >= 0; --j) {
			bool matches = venus_hfi_is_session_supported(
					gov->bus_table[j].codecs, sess_type);
			if (!matches)
				continue;

			frequency = gov->bus_table[j].freq;
			if (load <= gov->bus_table[j].load)
				break;
		}
		*freq += frequency;

		dprintk(VIDC_DBG,
			"session[%d] %#x, wxh %dx%d, fps %d, load %d, freq %d, total_freq %ld\n",
			i, sess_type, curr->width, curr->height,
			curr->fps, load, frequency, *freq);
	}
exit:
	return 0;
}

int msm_vidc_table_event_handler(struct devfreq *devfreq,
		unsigned int event, void *data)
{
	int rc = 0;

	if (!devfreq) {
		dprintk(VIDC_ERR, "%s: NULL devfreq\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case DEVFREQ_GOV_START:
	case DEVFREQ_GOV_RESUME:
		mutex_lock(&devfreq->lock);
		rc = update_devfreq(devfreq);
		mutex_unlock(&devfreq->lock);
		break;
	}

	return rc;
}

static int msm_vidc_load_bus_table(struct platform_device *pdev,
		struct msm_vidc_bus_table_gov *data)
{
	int c = 0;
	const char *name = NULL;

	if (!pdev || !data) {
		dprintk(VIDC_ERR, "%s: invalid args %p %p\n",
			__func__, pdev, data);
		return -EINVAL;
	}

	of_property_read_string(pdev->dev.of_node, "name", &name);
	if (strlen(name) > ARRAY_SIZE(data->devfreq_gov.name) - 1) {
		dprintk(VIDC_ERR,
			"%s: name is too long, max should be %zu chars\n",
			__func__, ARRAY_SIZE(data->devfreq_gov.name) - 1);
		return -EINVAL;
	}

	strlcpy((char *)data->devfreq_gov.name, name,
			ARRAY_SIZE(data->devfreq_gov.name));
	data->devfreq_gov.get_target_freq = msm_vidc_table_get_target_freq;
	data->devfreq_gov.event_handler = msm_vidc_table_event_handler;

	data->bus_table_size = of_property_count_elems_of_size(
			pdev->dev.of_node, "qcom,bus-table",
			sizeof(*data->bus_table));
	if (data->bus_table_size <= 0) {
		dprintk(VIDC_ERR, "%s: invalid bus table size %d\n",
			__func__, data->bus_table_size);
		return -EINVAL;
	}

	data->bus_table = devm_kcalloc(&pdev->dev, data->bus_table_size,
			sizeof(*data->bus_table), GFP_KERNEL);
	if (!data->bus_table) {
		dprintk(VIDC_ERR, "%s: allocation failed\n", __func__);
		return -ENOMEM;
	}

	of_property_read_u32_array(pdev->dev.of_node, "qcom,bus-table",
			(u32 *)data->bus_table,
			sizeof(*data->bus_table) /
			sizeof(u32) * data->bus_table_size);

	dprintk(VIDC_DBG, "%s: bus table:\n", __func__);
	for (c = 0; c < data->bus_table_size; ++c)
		dprintk(VIDC_DBG, "%8d %8d %#x\n", data->bus_table[c].load,
			data->bus_table[c].freq, data->bus_table[c].codecs);

	return 0;
}

static int msm_vidc_bus_table_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_bus_table_gov *gov = NULL;

	dprintk(VIDC_DBG, "%s\n", __func__);

	gov = devm_kzalloc(&pdev->dev, sizeof(*gov), GFP_KERNEL);
	if (!gov) {
		dprintk(VIDC_ERR, "%s: allocation failed\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, gov);

	rc = msm_vidc_load_bus_table(pdev, gov);
	if (rc)
		return rc;

	rc = devfreq_add_governor(&gov->devfreq_gov);
	if (rc)
		dprintk(VIDC_ERR, "%s: add governor failed\n", __func__);

	return rc;
}

static int msm_vidc_bus_table_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_bus_table_gov *gov = NULL;

	dprintk(VIDC_DBG, "%s\n", __func__);

	gov = platform_get_drvdata(pdev);
	if (IS_ERR_OR_NULL(gov))
		return PTR_ERR(gov);

	rc = devfreq_remove_governor(&gov->devfreq_gov);

	return rc;
}

static const struct of_device_id device_id[] = {
	{.compatible = "qcom,msm-vidc,governor,table"},
	{}
};

static struct platform_driver msm_vidc_bus_table_driver = {
	.probe = msm_vidc_bus_table_probe,
	.remove = msm_vidc_bus_table_remove,
	.driver = {
		.name = "msm_vidc_bus_table_governor",
		.owner = THIS_MODULE,
		.of_match_table = device_id,
	},
};

static int __init msm_vidc_bus_table_init(void)
{

	dprintk(VIDC_DBG, "%s\n", __func__);

	return platform_driver_register(&msm_vidc_bus_table_driver);
}

module_init(msm_vidc_bus_table_init);

static void __exit msm_vidc_bus_table_exit(void)
{
	dprintk(VIDC_DBG, "%s\n", __func__);
	platform_driver_unregister(&msm_vidc_bus_table_driver);
}

module_exit(msm_vidc_bus_table_exit);
MODULE_LICENSE("GPL v2");
