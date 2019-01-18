/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>

#include "atl_qcom.h"
#include "atl_of.h"

static const struct of_device_id aqc_matches[] = {
	{ .compatible = "aquantia,aqc-107" },
	{ .compatible = "aquantia,aqc-108" },
	{ .compatible = "aquantia,aqc-109" },
};

int atl_parse_dt(struct device *dev)
{
	if (!dev->of_node) {
		dev_dbg(dev, "device tree node is not present\n");
		return 0;
	}

	if (!of_match_node(aqc_matches, dev->of_node)) {
		dev_notice(dev, "device tree node is not compatible\n");
		return 0;
	}

	/* Aquantia properties go here */

	/* OEM properties go here */

	return atl_qcom_parse_dt(dev);
}
