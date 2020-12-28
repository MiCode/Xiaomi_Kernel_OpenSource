// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include "dsi_pll.h"

static int dsi_pll_clock_register(struct platform_device *pdev,
				struct dsi_pll_resource *pll_res)
{
	int rc;

	switch (pll_res->pll_revision) {
	case DSI_PLL_5NM:
		rc = dsi_pll_clock_register_5nm(pdev, pll_res);
		break;
	case DSI_PLL_10NM:
		rc = dsi_pll_clock_register_10nm(pdev, pll_res);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		DSI_PLL_ERR(pll_res, "clock register failed rc=%d\n", rc);

	return rc;
}

static inline int dsi_pll_get_ioresources(struct platform_device *pdev,
				void __iomem **regmap, char *resource_name)
{
	int rc = 0;
	struct resource *rsc = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, resource_name);
	if (rsc) {
		if (!regmap)
			return -ENOMEM;

		*regmap = devm_ioremap(&pdev->dev,
					rsc->start, resource_size(rsc));
		if (!*regmap)
			return -ENOMEM;
	}
	return rc;
}

static void dsi_pll_free_bootmem(u32 mem_addr, u32 size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + size) >> PAGE_SHIFT;
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

static void dsi_pll_parse_dfps(struct platform_device *pdev,
				struct dsi_pll_resource *pll_res)
{
	struct device_node *pnode = NULL;
	const u32 *addr;
	void *trim_codes = NULL;
	u64 size;
	u32 offsets[2];

	pnode = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (IS_ERR_OR_NULL(pnode)) {
		DSI_PLL_INFO(pll_res, "of_parse_phandle failed\n");
		goto node_err;
	}

	addr = of_get_address(pnode, 0, &size, NULL);
	if (!addr) {
		DSI_PLL_ERR(pll_res,
			"failed to parse the dfps memory address\n");
		goto node_err;
	}

	/* maintain compatibility for 32/64 bit */
	offsets[0] = (u32) of_read_ulong(addr, 2);
	offsets[1] = (u32) size;

	trim_codes = memremap(offsets[0], offsets[1], MEMREMAP_WB);
	if (!trim_codes)
		goto mem_err;

	pll_res->dfps = kzalloc(sizeof(struct dfps_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pll_res->dfps)) {
		DSI_PLL_ERR(pll_res, "pll_res->dfps allocate failed\n");
		goto mem_err;
	}

	/* memcopy complete dfps structure from kernel virtual memory */
	memcpy_fromio(pll_res->dfps, trim_codes, sizeof(struct dfps_info));

mem_err:
	if (trim_codes)
		memunmap(trim_codes);

	/* free the dfps memory here */
	dsi_pll_free_bootmem(offsets[0], offsets[1]);

node_err:
	if (pnode)
		of_node_put(pnode);
}

int dsi_pll_init(struct platform_device *pdev, struct dsi_pll_resource **pll)
{
	int rc = 0;
	const char *label;
	struct dsi_pll_resource *pll_res = NULL;
	bool in_trusted_vm = false;

	if (!pdev->dev.of_node) {
		pr_err("Invalid DSI PHY node\n");
		return -ENOTSUPP;
	}

	pll_res = devm_kzalloc(&pdev->dev, sizeof(struct dsi_pll_resource),
								GFP_KERNEL);
	if (!pll_res)
		return -ENOMEM;

	*pll = pll_res;

	label = of_get_property(pdev->dev.of_node, "pll-label", NULL);
	if (!label) {
		DSI_PLL_ERR(pll_res, "DSI pll label not specified\n");
		return 0;
	}

	DSI_PLL_INFO(pll_res, "DSI pll label = %s\n", label);

	/**
	  * Currently, Only supports 5nm and 10nm PLL version. Will add
	  * support for other versions as needed.
	  */

	if (!strcmp(label, "dsi_pll_5nm"))
		pll_res->pll_revision = DSI_PLL_5NM;
	else if (!strcmp(label, "dsi_pll_10nm"))
		pll_res->pll_revision = DSI_PLL_10NM;
	else
		return -ENOTSUPP;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index",
			&pll_res->index);
	if (rc) {
		DSI_PLL_ERR(pll_res, "Unable to get the cell-index rc=%d\n", rc);
		pll_res->index = 0;
	}

	pll_res->ssc_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,dsi-pll-ssc-en");

	if (pll_res->ssc_en) {
		DSI_PLL_INFO(pll_res, "PLL SSC enabled\n");

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ssc-frequency-hz", &pll_res->ssc_freq);

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ssc-ppm", &pll_res->ssc_ppm);

		pll_res->ssc_center = false;

		label = of_get_property(pdev->dev.of_node,
			"qcom,dsi-pll-ssc-mode", NULL);

		if (label && !strcmp(label, "center-spread"))
			pll_res->ssc_center = true;
	}


	if (dsi_pll_get_ioresources(pdev, &pll_res->pll_base, "pll_base")) {
		DSI_PLL_ERR(pll_res, "Unable to remap pll base resources\n");
		return -ENOMEM;
	}

	pr_info("PLL base=%p\n", pll_res->pll_base);

	if (dsi_pll_get_ioresources(pdev, &pll_res->phy_base, "dsi_phy")) {
		DSI_PLL_ERR(pll_res, "Unable to remap pll phy base resources\n");
		return -ENOMEM;
	}

	if (dsi_pll_get_ioresources(pdev, &pll_res->dyn_pll_base,
							"dyn_refresh_base")) {
		DSI_PLL_ERR(pll_res, "Unable to remap dynamic pll base resources\n");
		return -ENOMEM;
	}

	if (dsi_pll_get_ioresources(pdev, &pll_res->gdsc_base, "gdsc_base")) {
		DSI_PLL_ERR(pll_res, "Unable to remap gdsc base resources\n");
		return -ENOMEM;
	}

	in_trusted_vm = of_property_read_bool(pdev->dev.of_node,
						"qcom,dsi-pll-in-trusted-vm");
	if (in_trusted_vm) {
		DSI_PLL_INFO(pll_res,
			"Bypassing PLL clock register for Trusted VM\n");
		return rc;
	}

	rc = dsi_pll_clock_register(pdev, pll_res);
	if (rc) {
		DSI_PLL_ERR(pll_res, "clock register failed rc=%d\n", rc);
		return -EINVAL;
	}

	if (!(pll_res->index))
		dsi_pll_parse_dfps(pdev, pll_res);

	return rc;

}
