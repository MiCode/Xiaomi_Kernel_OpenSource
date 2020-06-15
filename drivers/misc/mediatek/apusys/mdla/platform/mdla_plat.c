// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/of_device.h>

#include <common/mdla_device.h>

#include "mdla_plat_internal.h"

static unsigned int nr_core_ids = DEFAULT_CORE_NUM;

struct mdla_plat_func {
	int (*init)(struct platform_device *pdev);
	void (*deinit)(struct platform_device *pdev);
};

#define MDLA_MATCH_DATA(name, entry, exit)\
static struct mdla_plat_func name = { .init = entry, .deinit = exit}

MDLA_MATCH_DATA(mt6779_data, mdla_mt6779_init, mdla_mt6779_deinit);
MDLA_MATCH_DATA(mt6873_data, mdla_mt6873_init, mdla_mt6873_deinit);
MDLA_MATCH_DATA(mt6885_data, mdla_mt6885_init, mdla_mt6885_deinit);
MDLA_MATCH_DATA(mt8195_data, mdla_mt8195_init, mdla_mt8195_deinit);

static const struct of_device_id mdla_of_match[] = {
	{ .compatible = "mediatek, mt6779-mdla", .data = &mt6779_data},
	{ .compatible = "mediatek, mt6873-mdla", .data = &mt6873_data},
	{ .compatible = "mediatek, mt6885-mdla", .data = &mt6885_data},
	{ .compatible = "mediatek, mt8195-mdla", .data = &mt8195_data},
	{ /* end of list */},
};
MODULE_DEVICE_TABLE(of, mdla_of_match);


unsigned int mdla_plat_get_core_num(void)
{
	return nr_core_ids;
}

const struct of_device_id *mdla_plat_get_device(void)
{
	return mdla_of_match;
}

int mdla_plat_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdla_plat_func *data;

	of_property_read_u32(dev->of_node, "core_num", &nr_core_ids);
	if (nr_core_ids > MAX_CORE_NUM) {
		dev_info(dev, "Invalid core number (%d)\n", nr_core_ids);
		nr_core_ids = DEFAULT_CORE_NUM;
	}

	dev_info(dev, "MDLA core number = %d\n", nr_core_ids);

	data = (struct mdla_plat_func *)of_device_get_match_data(dev);

	if (!data)
		return -1;

	return data->init(pdev);
}

void mdla_plat_deinit(struct platform_device *pdev)
{
	struct mdla_plat_func *data;

	data = (struct mdla_plat_func *)of_device_get_match_data(&pdev->dev);

	if (data)
		data->deinit(pdev);
}

