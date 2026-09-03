/*
 * Copyright (C) 2020 Unisoc Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include "sprd_coresight-priv.h"
#include "sprd_coresight-apetb-ctrl.h"

static void apetb_init(struct apetb_device *dbg)
{
}

static void apetb_exit(struct apetb_device *dbg)
{
}

static struct apetb_ops ops = {
	.init = apetb_init,
	.exit = apetb_exit,
};

static int sprd_apetb_probe(struct platform_device *pdev)
{
	struct apetb_device *dbg;
	static struct device *dev;
	u32 i;
	struct device_node *psub_node;
	struct device_node *pnode = pdev->dev.of_node;
	u32 source_num;

	dev_dbg(&pdev->dev, "%s entry\n", __func__);

	dbg = apetb_device_register(&pdev->dev, &ops, "sprd_apetb");
	if (!dbg) {
		dev_err(&pdev->dev, "%s apetb_device_register failed!!!", __func__);
		return -ENOMEM;
	}

	psub_node = of_parse_phandle(pnode, "apetb-sink", 0);
	if (psub_node) {
		of_node_put(psub_node);
		dev = sprd_of_coresight_get_device_by_node(psub_node);
		if (!dev) {
			dev_err(&pdev->dev, "%s sprd_of_coresight_get_device_by_node sink failed!!!",
				__func__);
			return -ENODEV;
		}
		dbg->apetb_sink = dev;
	} else {
		dev_err(&pdev->dev, "%s of_parse_phandle sink failed!!!", __func__);
		return -ENODEV;
	}

	source_num = of_count_phandle_with_args(pnode, "apetb-source", NULL);

	if (source_num > MAX_ETB_SOURCE_NUM) {
		dev_err(&pdev->dev, "%s source_num %d exceed MAX_ETB_SOURCE_NUM!!!",
			__func__, source_num);
		dbg->source_num = 0;
		return -ENODEV;
	}

	dbg->source_num = source_num;

	for (i = 0; i < source_num; i++) {
		psub_node = of_parse_phandle(pnode, "apetb-source", i);
		if (psub_node) {
			dev = sprd_of_coresight_get_device_by_node(psub_node);
			if (!dev) {
				dev_err(&pdev->dev, "%s sprd_of_coresight_get_device_by_node source(%d) failed!!!",
					__func__, i);
				return -ENODEV;
			}
			dbg->apetb_source[i] = dev;
		} else {
			dev_err(&pdev->dev, "%s of_parse_phandle source %d failed!!!", __func__, i);
			return -ENODEV;
		}
	}
	dev_dbg(&pdev->dev, "%s end\n", __func__);

	return 0;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "sprd,sprd_apetb",},
	{},
};

static struct platform_driver apetb_driver = {
	.probe = sprd_apetb_probe,
	.driver = {
		.name = "sprd_ap-etb",
		.of_match_table = dt_ids,
	},
};

module_platform_driver(apetb_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SoC Coresight APETB_MAIN Driver");
