/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include "msm_vidc_res_parse.h"
#include "msm_vidc_internal.h"
#include "venus_hfi.h"

enum bus_profile {
	VIDC_BUS_PROFILE_NORMAL			= BIT(0),
	VIDC_BUS_PROFILE_LOW			= BIT(1),
	VIDC_BUS_PROFILE_UBWC			= BIT(2),
};

struct bus_profile_entry {
	struct {
		u32 load, freq;
	} *bus_table;
	u32 bus_table_size;
	u32 codec_mask;
	enum bus_profile profile;
};

struct msm_vidc_bus_table_gov {
	struct bus_profile_entry *bus_prof_entries;
	u32 count;
	struct devfreq_governor devfreq_gov;
};

static int __get_bus_freq(struct msm_vidc_bus_table_gov *gov,
		struct vidc_bus_vote_data *data,
		enum bus_profile profile)
{
	int i = 0, load = 0, freq = 0;
	enum vidc_vote_data_session sess_type = 0;
	struct bus_profile_entry *entry = NULL;
	bool found = false;

	load = NUM_MBS_PER_SEC(data->width, data->height, data->fps);
	sess_type = VIDC_VOTE_DATA_SESSION_VAL(data->codec, data->domain);

	/* check if ubwc bus profile is present */
	for (i = 0; i < gov->count; i++) {
		entry = &gov->bus_prof_entries[i];
		if (!entry->bus_table || !entry->bus_table_size)
			continue;
		if (!venus_hfi_is_session_supported(
				entry->codec_mask, sess_type))
			continue;
		if (entry->profile == profile) {
			found = true;
			break;
		}
	}

	if (found) {
		/* loop over bus table and select frequency */
		for (i = entry->bus_table_size - 1; i >= 0; --i) {
			/* load is arranged in descending order */
			freq = entry->bus_table[i].freq;
			if (load <= entry->bus_table[i].load)
				break;
		}
	}

	return freq;
}

static int msm_vidc_table_get_target_freq(struct devfreq *dev,
		unsigned long *frequency, u32 *flag)
{
	struct devfreq_dev_status status = {0};
	struct msm_vidc_gov_data *vidc_data = NULL;
	struct msm_vidc_bus_table_gov *gov = NULL;
	enum bus_profile profile = 0;
	int i = 0;

	if (!dev || !frequency || !flag) {
		dprintk(VIDC_ERR, "%s: Invalid params %pK, %pK, %pK\n",
			__func__, dev, frequency, flag);
		return -EINVAL;
	}

	gov = container_of(dev->governor,
			struct msm_vidc_bus_table_gov, devfreq_gov);
	if (!gov) {
		dprintk(VIDC_ERR, "%s: governor not found\n", __func__);
		return -EINVAL;
	}

	dev->profile->get_dev_status(dev->dev.parent, &status);
	vidc_data = (struct msm_vidc_gov_data *)status.private_data;

	*frequency = 0;
	for (i = 0; i < vidc_data->data_count; i++) {
		struct vidc_bus_vote_data *data = &vidc_data->data[i];
		int freq = 0;

		if (data->power_mode == VIDC_POWER_TURBO) {
			dprintk(VIDC_DBG, "bus: found turbo session[%d] %#x\n",
				i, VIDC_VOTE_DATA_SESSION_VAL(data->codec,
					data->domain));
			*frequency = INT_MAX;
			goto exit;
		}

		profile = VIDC_BUS_PROFILE_NORMAL;
		if (data->color_formats[0] == HAL_COLOR_FORMAT_NV12_TP10_UBWC ||
			data->color_formats[0] == HAL_COLOR_FORMAT_NV12_UBWC)
			profile = VIDC_BUS_PROFILE_UBWC;

		freq = __get_bus_freq(gov, data, profile);
		/*
		 * chose frequency from normal profile
		 * if specific profile frequency was not found.
		 */
		if (!freq)
			freq = __get_bus_freq(gov, data,
				VIDC_BUS_PROFILE_NORMAL);

		*frequency += (unsigned long)freq;

		dprintk(VIDC_DBG,
			"session[%d] %#x: wxh %dx%d, fps %d, bus_profile %#x, freq %d, total_freq %ld KBps\n",
			i, VIDC_VOTE_DATA_SESSION_VAL(
			data->codec, data->domain), data->width,
			data->height, data->fps, profile,
			freq, *frequency);
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

static int msm_vidc_free_bus_table(struct platform_device *pdev,
		struct msm_vidc_bus_table_gov *data)
{
	int rc = 0, i = 0;

	if (!pdev || !data) {
		dprintk(VIDC_ERR, "%s: invalid args %pK %pK\n",
			__func__, pdev, data);
		return -EINVAL;
	}

	for (i = 0; i < data->count; i++)
		data->bus_prof_entries[i].bus_table = NULL;

	data->bus_prof_entries = NULL;
	data->count = 0;

	return rc;
}

static int msm_vidc_load_bus_table(struct platform_device *pdev,
		struct msm_vidc_bus_table_gov *data)
{
	int rc = 0, i = 0, j = 0;
	const char *name = NULL;
	struct bus_profile_entry *entry = NULL;
	struct device_node *parent_node = NULL;
	struct device_node *child_node = NULL;

	if (!pdev || !data) {
		dprintk(VIDC_ERR, "%s: invalid args %pK %pK\n",
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

	parent_node = of_find_node_by_name(pdev->dev.of_node,
			"qcom,bus-freq-table");
	if (!parent_node) {
		dprintk(VIDC_DBG, "Node qcom,bus-freq-table not found.\n");
		return 0;
	}

	data->count = of_get_child_count(parent_node);
	if (!data->count) {
		dprintk(VIDC_DBG, "No child nodes in qcom,bus-freq-table\n");
		return 0;
	}

	data->bus_prof_entries = devm_kzalloc(&pdev->dev,
			sizeof(*data->bus_prof_entries) * data->count,
			GFP_KERNEL);
	if (!data->bus_prof_entries) {
		dprintk(VIDC_DBG, "no memory to allocate bus_prof_entries\n");
		return -ENOMEM;
	}

	for_each_child_of_node(parent_node, child_node) {

		if (i >= data->count) {
			dprintk(VIDC_ERR,
				"qcom,bus-freq-table: invalid child node %d, max is %d\n",
				i, data->count);
			break;
		}
		entry = &data->bus_prof_entries[i];

		if (of_find_property(child_node, "qcom,codec-mask", NULL)) {
			rc = of_property_read_u32(child_node,
					"qcom,codec-mask", &entry->codec_mask);
			if (rc) {
				dprintk(VIDC_ERR,
					"qcom,codec-mask not found\n");
				break;
			}
		}

		if (of_find_property(child_node, "qcom,low-power-mode", NULL))
			entry->profile = VIDC_BUS_PROFILE_LOW;
		else if (of_find_property(child_node, "qcom,ubwc-mode", NULL))
			entry->profile = VIDC_BUS_PROFILE_UBWC;
		else
			entry->profile = VIDC_BUS_PROFILE_NORMAL;

		if (of_find_property(child_node,
					"qcom,load-busfreq-tbl", NULL)) {
			rc = msm_vidc_load_u32_table(pdev, child_node,
						"qcom,load-busfreq-tbl",
						sizeof(*entry->bus_table),
						(u32 **)&entry->bus_table,
						&entry->bus_table_size);
			if (rc) {
				dprintk(VIDC_ERR,
					"qcom,load-busfreq-tbl failed\n");
				break;
			}
		} else {
			entry->bus_table = NULL;
			entry->bus_table_size = 0;
		}

		dprintk(VIDC_DBG,
			"qcom,load-busfreq-tbl: size %d, codec_mask %#x, profile %#x\n",
			entry->bus_table_size, entry->codec_mask,
			entry->profile);
		for (j = 0; j < entry->bus_table_size; j++)
			dprintk(VIDC_DBG, "   load %8d freq %8d\n",
				entry->bus_table[j].load,
				entry->bus_table[j].freq);

		i++;
	}

	return rc;
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

	rc = msm_vidc_free_bus_table(pdev, gov);
	if (rc)
		dprintk(VIDC_WARN, "%s: free bus table failed\n", __func__);

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
