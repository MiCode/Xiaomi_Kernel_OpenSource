// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "governor-static-map: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/devfreq.h>
#include "governor.h"

struct core_dev_map {
	unsigned int core_mhz;
	unsigned int target_freq;
};

struct static_map_gov {
	struct device		*dev;
	struct device_node	*of_node;
	struct clk		*dev_clk;
	unsigned long		dev_clk_cur_freq;
	struct notifier_block	clock_change_nb;
	struct core_dev_map	*freq_map;
	struct devfreq_governor	*gov;
	struct devfreq		*df;
	bool			mon_started;
	struct list_head	list;
	void			*orig_data;
	unsigned long		resume_freq;
};

static LIST_HEAD(static_map_list);
static DEFINE_MUTEX(static_map_lock);
static DEFINE_MUTEX(state_lock);
static int static_use_cnt;

static struct static_map_gov *find_static_map_node(struct devfreq *df)
{
	struct static_map_gov *node, *found = NULL;

	mutex_lock(&static_map_lock);
	list_for_each_entry(node, &static_map_list, list)
		if (node->of_node == df->dev.parent->of_node) {
			found = node;
			break;
		}
	mutex_unlock(&static_map_lock);

	return found;
}

static unsigned long core_to_dev_freq(struct static_map_gov *d,
		unsigned long coref)
{
	struct core_dev_map *map = d->freq_map;
	unsigned long freq = 0;

	if (!map || !coref)
		goto out;

	/* Start with the first non-zero freq map entry */
	map++;
	while (map->core_mhz && map->core_mhz != coref)
		map++;
	if (!map->core_mhz)
		map--;
	freq = map->target_freq;

out:
	pr_debug("core freq: %lu -> target: %lu\n", coref, freq);
	return freq;
}

#define NUM_COLS	2
static struct core_dev_map *init_core_dev_map(struct device *dev,
					struct device_node *of_node,
					char *prop_name)
{
	int len, nf, i, j;
	u32 data;
	struct core_dev_map *tbl;
	int ret;

	if (!of_node)
		of_node = dev->of_node;

	if (!of_find_property(of_node, prop_name, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct core_dev_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, prop_name, j,
				&data);
		if (ret) {
			dev_err(dev,
				"Couldn't read the core-dev freq table %d\n",
									ret);
			return NULL;
		}
		tbl[i].core_mhz = data;

		ret = of_property_read_u32_index(of_node, prop_name, j + 1,
				&data);
		if (ret) {
			dev_err(dev,
				"Couldn't read the core-dev freq table %d\n",
									ret);
			return NULL;
		}
		tbl[i].target_freq = data;
		pr_debug("Entry%d DEV:%u, Target:%u\n", i, tbl[i].core_mhz,
				tbl[i].target_freq);
	}
	tbl[i].core_mhz = 0;

	return tbl;
}
static int devfreq_static_map_get_freq(struct devfreq *df,
					unsigned long *freq)
{
	struct static_map_gov *gov_node = df->data;

	*freq = core_to_dev_freq(gov_node, gov_node->dev_clk_cur_freq);

	return 0;
}
static int devfreq_clock_change_notify_cb(struct notifier_block *nb,
				       unsigned long action, void *ptr)
{
	struct clk_notifier_data *data = ptr;
	struct static_map_gov *d;
	int ret;

	if (action != POST_RATE_CHANGE)
		return NOTIFY_OK;

	mutex_lock(&state_lock);
	d = container_of(nb, struct static_map_gov, clock_change_nb);

	mutex_lock(&d->df->lock);
	d->dev_clk_cur_freq = data->new_rate;
	if (IS_ERR_VALUE(d->dev_clk_cur_freq)) {
		mutex_unlock(&d->df->lock);
		mutex_unlock(&state_lock);
		return d->dev_clk_cur_freq;
	}
	d->dev_clk_cur_freq = d->dev_clk_cur_freq / 1000;

	ret = update_devfreq(d->df);
	if (ret)
		dev_err(d->dev,
			"Unable to update freq on request %d\n", ret);
	mutex_unlock(&d->df->lock);
	mutex_unlock(&state_lock);

	return 0;
}

static int devfreq_static_map_ev_handler(struct devfreq *df,
					unsigned int event, void *data)
{
	int ret = 0;
	struct static_map_gov *gov_node;

	mutex_lock(&state_lock);
	gov_node = find_static_map_node(df);
	if (!gov_node) {
		mutex_unlock(&state_lock);
		dev_err(df->dev.parent,
				"Unable to find static map governor!\n");
		return -ENODEV;
	}

	switch (event) {
	case DEVFREQ_GOV_START:
		gov_node->clock_change_nb.notifier_call =
						devfreq_clock_change_notify_cb;
		gov_node->orig_data = df->data;
		gov_node->df = df;
		df->data = gov_node;
		ret = clk_notifier_register(gov_node->dev_clk,
					&gov_node->clock_change_nb);
		if (ret) {
			dev_err(df->dev.parent,
				"Failed to register clock change notifier %d\n",
									ret);
		}
		break;
	case DEVFREQ_GOV_STOP:
		ret = clk_notifier_unregister(gov_node->dev_clk,
						&gov_node->clock_change_nb);
		if (ret) {
			dev_err(df->dev.parent,
				"Failed to register clock change notifier %d\n",
									ret);
		}
		df->data = gov_node->orig_data;
		gov_node->orig_data = NULL;
		break;
	case DEVFREQ_GOV_SUSPEND:
		ret = clk_notifier_unregister(gov_node->dev_clk,
						&gov_node->clock_change_nb);
		if (ret) {
			dev_err(df->dev.parent,
				"Failed to unregister clk notifier %d\n", ret);
		}
		mutex_lock(&df->lock);
		gov_node->resume_freq = gov_node->dev_clk_cur_freq;
		gov_node->dev_clk_cur_freq = 0;
		update_devfreq(df);
		mutex_unlock(&df->lock);
		break;
	case DEVFREQ_GOV_RESUME:
		ret = clk_notifier_register(gov_node->dev_clk,
						&gov_node->clock_change_nb);
		if (ret) {
			dev_err(df->dev.parent,
				"Failed to register clock change notifier %d\n",
									ret);
		}
		mutex_lock(&df->lock);
		gov_node->dev_clk_cur_freq = gov_node->resume_freq;
		update_devfreq(df);
		gov_node->resume_freq = 0;
		mutex_unlock(&df->lock);
		break;
	default:
		break;
	}
	mutex_unlock(&state_lock);
	return ret;
}
static struct devfreq_governor devfreq_gov_static_map = {
	.name = "static_map",
	.get_target_freq = devfreq_static_map_get_freq,
	.event_handler = devfreq_static_map_ev_handler,
};

static int gov_static_map_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct static_map_gov *d;
	int ret;
	const char *dev_clk_name;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	d->dev = dev;

	ret = of_property_read_string(dev->of_node, "qcom,dev_clk",
							&dev_clk_name);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to read device clock name %d\n", ret);
		return ret;
	}
	d->dev_clk = devm_clk_get(dev, dev_clk_name);
	if (IS_ERR(d->dev_clk))
		return PTR_ERR(d->dev_clk);

	d->of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!d->of_node) {
		dev_err(dev, "Couldn't find a target device.\n");
		ret = -ENODEV;
		return ret;
	}

	d->freq_map = init_core_dev_map(dev, NULL, "qcom,core-dev-table");
	if (!d->freq_map) {
		dev_err(dev, "Couldn't find the core-dev freq table!\n");
		return -EINVAL;
	}
	mutex_lock(&static_map_lock);
	list_add_tail(&d->list, &static_map_list);
	mutex_unlock(&static_map_lock);

	mutex_lock(&state_lock);
	d->gov = &devfreq_gov_static_map;
	if (!static_use_cnt)
		ret = devfreq_add_governor(&devfreq_gov_static_map);
	if (ret)
		dev_err(dev, "Failed to add governor %d\n", ret);
	if (!ret)
		static_use_cnt++;
	mutex_unlock(&state_lock);

	return ret;
}

static const struct of_device_id static_map_match_table[] = {
	{ .compatible = "qcom,static-map"},
	{}
};

static struct platform_driver gov_static_map_driver = {
	.probe = gov_static_map_probe,
	.driver = {
		.name = "static-map",
		.of_match_table = static_map_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(gov_static_map_driver);
MODULE_DESCRIPTION("STATIC MAP GOVERNOR FOR DDR");
MODULE_LICENSE("GPL v2");
