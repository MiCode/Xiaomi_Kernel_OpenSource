/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irqchip.h>
#include "pdc.h"

static struct pdc_pin *pdc_pins_dt;

static int pdc_read_pin_mapping_from_dt(struct device_node *node)
{
	int ret = 0, n;
	int pdc_pin_count;

	n = of_property_count_elems_of_size(node, "qcom,pdc-pins", sizeof(u32));
	if (n <= 0 || n % 2)
		return -EINVAL;

	pdc_pin_count = n / 2;

	pdc_pins_dt = kcalloc(pdc_pin_count + 1, sizeof(struct pdc_pin),
				GFP_KERNEL);
	if (!pdc_pins_dt)
		return -ENOMEM;

	for (n = 0; n < pdc_pin_count; n++) {
		ret = of_property_read_u32_index(node, "qcom,pdc-pins",
			n * 2,
			&pdc_pins_dt[n].pin);
		if (ret)
			goto err;

		ret = of_property_read_u32_index(node, "qcom,pdc-pins",
			(n * 2) + 1,
			(u32 *)&pdc_pins_dt[n].hwirq);
		if (ret)
			goto err;
	}

	pdc_pins_dt[pdc_pin_count].pin = -1;

	return ret;

err:
	kfree(pdc_pins_dt);
	return ret;
}

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	int ret;

	ret = pdc_read_pin_mapping_from_dt(node);
	if (ret) {
		pr_err("%s: Error reading PDC pin mapping: %d\n",
			 __func__, ret);
		return ret;
	}

	ret = qcom_pdc_init(node, parent, pdc_pins_dt);

	pr_info("PDC virt initialized\n");

	return ret;
}

IRQCHIP_DECLARE(pdc_virt, "qcom,pdc-virt", qcom_pdc_gic_init);
