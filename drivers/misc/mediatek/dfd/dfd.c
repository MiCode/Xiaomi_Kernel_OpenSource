/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>

#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/mtk_wd_api.h>
#include <mt-plat/mtk_platform_debug.h>
#include "dfd.h"

static struct dfd_drv *drv;

/* return -1 for error indication */
int dfd_setup(void)
{
	int ret;
	struct wd_api *wd_api = NULL;

	if (drv && (drv->enabled == 1) && (drv->base_addr > 0)) {
		/* check support or not first */
		if (check_dfd_support() == 0)
			return -1;

		/* get watchdog api */
		ret = get_wd_api(&wd_api);
		if (ret < 0) {
			//pr_notice("[dfd] get_wd_api error\n");
			return ret;
		}
		wd_api->wd_dfd_count_en(1);
		wd_api->wd_dfd_thermal1_dis(1);
		wd_api->wd_dfd_thermal2_dis(0);
		wd_api->wd_dfd_timeout(drv->rg_dfd_timeout);

		ret = mt_secure_call(MTK_SIP_KERNEL_DFD, DFD_SMC_MAGIC_SETUP,
				(u64) drv->base_addr, drv->chain_length, 0);

		if (ret < 0)
			return ret;

		if (set_sram_flag_dfd_valid() < 0)
			return -1;

		return ret;
	} else
		return -1;
}

static int __init fdt_get_chosen(unsigned long node, const char *uname,
		int depth, void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0
				&& strcmp(uname, "chosen@0") != 0))
		return 0;

	*(unsigned long *)data = node;
	return 1;
}

static int __init dfd_init(void)
{
	struct device_node *dev_node, *infra_node;
	unsigned long node;
	const void *prop;
	unsigned int val;

	drv = kzalloc(sizeof(struct dfd_drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	/* get dfd settings */
	dev_node = of_find_compatible_node(NULL, NULL, "mediatek,dfd");
	if (dev_node) {
		if (of_property_read_u32(dev_node, "mediatek,enabled", &val))
			drv->enabled = 0;
		else
			drv->enabled = val;

		if (of_property_read_u32(dev_node,
					"mediatek,chain_length", &val))
			drv->chain_length = 0;
		else
			drv->chain_length = val;

		if (of_property_read_u32(dev_node,
					"mediatek,rg_dfd_timeout", &val))
			drv->rg_dfd_timeout = 0;
		else
			drv->rg_dfd_timeout = val;
	} else
		return -ENODEV;

	if (drv->enabled == 0)
		return 0;

	of_scan_flat_dt(fdt_get_chosen, &node);
	if (node) {
		prop = of_get_flat_dt_prop(node, "dfd,base_addr_msb", NULL);
		drv->base_addr_msb = (prop) ? of_read_number(prop, 1) : 0;
	} else {
		drv->base_addr_msb = 0;
	}

	infra_node = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
	if (infra_node) {
		void __iomem *infra = of_iomap(infra_node, 0);

		if (infra && drv->base_addr_msb) {
			infra += dfd_infra_base();
			writel(readl(infra)
				| (drv->base_addr_msb >> dfd_ap_addr_offset()),
				infra);
		}
	}

	/* get base address if enabled */
	of_scan_flat_dt(fdt_get_chosen, &node);
	if (node) {
		prop = of_get_flat_dt_prop(node, "dfd,base_addr", NULL);
		drv->base_addr = (prop) ? (u64) of_read_number(prop, 2) : 0;
	} else
		return -ENODEV;

	return 0;
}

core_initcall(dfd_init);
