/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/coresight.h>

struct coresight_platform_data *of_get_coresight_platform_data(
				struct device *dev, struct device_node *node)
{
	int i, ret = 0;
	uint32_t outports_len = 0;
	struct device_node *child_node;
	struct coresight_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32(node, "coresight-id", &pdata->id);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_string(node, "coresight-name", &pdata->name);
	if (ret)
		return ERR_PTR(ret);

	ret = of_property_read_u32(node, "coresight-nr-inports",
				   &pdata->nr_inports);
	if (ret)
		return ERR_PTR(ret);

	pdata->nr_outports = 0;
	if (of_get_property(node, "coresight-outports", &outports_len))
		pdata->nr_outports = outports_len/sizeof(uint32_t);

	if (pdata->nr_outports) {
		pdata->outports = devm_kzalloc(dev, pdata->nr_outports *
					       sizeof(*pdata->outports),
					       GFP_KERNEL);
		if (!pdata->outports)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(node, "coresight-outports",
						 (u32 *)pdata->outports,
						 pdata->nr_outports);
		if (ret)
			return ERR_PTR(ret);

		pdata->child_ids = devm_kzalloc(dev, pdata->nr_outports *
						sizeof(*pdata->child_ids),
						GFP_KERNEL);
		if (!pdata->child_ids)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < pdata->nr_outports; i++) {
			child_node = of_parse_phandle(node,
						      "coresight-child-list",
						      i);
			if (!child_node)
				return ERR_PTR(-EINVAL);

			ret = of_property_read_u32(child_node, "coresight-id",
						   (u32 *)&pdata->child_ids[i]);
			of_node_put(child_node);
			if (ret)
				return ERR_PTR(ret);
		}

		pdata->child_ports = devm_kzalloc(dev, pdata->nr_outports *
						  sizeof(*pdata->child_ports),
						  GFP_KERNEL);
		if (!pdata->child_ports)
			return ERR_PTR(-ENOMEM);

		ret = of_property_read_u32_array(node, "coresight-child-ports",
						 (u32 *)pdata->child_ports,
						 pdata->nr_outports);
		if (ret)
			return ERR_PTR(ret);
	}

	pdata->default_sink = of_property_read_bool(node,
						    "coresight-default-sink");
	return pdata;
}
EXPORT_SYMBOL_GPL(of_get_coresight_platform_data);
