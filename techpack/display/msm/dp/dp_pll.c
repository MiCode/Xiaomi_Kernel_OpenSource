// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/of_device.h>
#include "dp_debug.h"
#include "dp_pll.h"

static int dp_pll_fill_io(struct dp_pll *pll)
{
	struct dp_parser *parser = pll->parser;

	pll->io.dp_phy = parser->get_io(parser, "dp_phy");
	if (!pll->io.dp_phy) {
		DP_ERR("Invalid dp_phy resource\n");
		return -ENOMEM;
	}

	pll->io.dp_pll = parser->get_io(parser, "dp_pll");
	if (!pll->io.dp_pll) {
		DP_ERR("Invalid dp_pll resource\n");
		return -ENOMEM;
	}

	pll->io.dp_ln_tx0 = parser->get_io(parser, "dp_ln_tx0");
	if (!pll->io.dp_ln_tx0) {
		DP_ERR("Invalid dp_ln_tx1 resource\n");
		return -ENOMEM;
	}

	pll->io.dp_ln_tx1 = parser->get_io(parser, "dp_ln_tx1");
	if (!pll->io.dp_ln_tx1) {
		DP_ERR("Invalid dp_ln_tx1 resource\n");
		return -ENOMEM;
	}

	pll->io.gdsc = parser->get_io(parser, "gdsc");
	if (!pll->io.gdsc) {
		DP_ERR("Invalid gdsc resource\n");
		return -ENOMEM;
	}

	return 0;
}

static int dp_pll_clock_register(struct dp_pll *pll)
{
	int rc;

	switch (pll->revision) {
	case DP_PLL_5NM_V1:
	case DP_PLL_5NM_V2:
	case DP_PLL_7NM:
		rc = dp_pll_clock_register_5nm(pll);
		break;
	default:
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

static void dp_pll_clock_unregister(struct dp_pll *pll)
{
	switch (pll->revision) {
	case DP_PLL_5NM_V1:
	case DP_PLL_5NM_V2:
	case DP_PLL_7NM:
		dp_pll_clock_unregister_5nm(pll);
		break;
	default:
		break;
	}
}

struct dp_pll *dp_pll_get(struct dp_pll_in *in)
{
	int rc = 0;
	struct dp_pll *pll;
	struct dp_parser *parser;
	const char *label = NULL;
	struct platform_device *pdev;

	if (!in || !in->pdev || !in->pdev->dev.of_node || !in->parser) {
		DP_ERR("Invalid resource pointers\n");
		return ERR_PTR(-EINVAL);
	}

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);
	pll->pdev = in->pdev;
	pll->parser = in->parser;
	pll->aux = in->aux;
	parser = pll->parser;
	pdev = pll->pdev;

	label = of_get_property(pdev->dev.of_node, "qcom,pll-revision", NULL);
	if (label) {
		if (!strcmp(label, "5nm-v1")) {
			pll->revision = DP_PLL_5NM_V1;
		} else if (!strcmp(label, "5nm-v2")) {
			pll->revision = DP_PLL_5NM_V2;
		} else if (!strcmp(label, "7nm")) {
			pll->revision = DP_PLL_7NM;
		} else {
			DP_ERR("Unsupported pll revision\n");
			rc = -ENOTSUPP;
			goto error;
		}
	} else {
		DP_ERR("pll revision not specified\n");
		rc = -EINVAL;
		goto error;
	}

	pll->ssc_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,ssc-feature-enable");
	pll->bonding_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,bonding-feature-enable");

	rc = dp_pll_fill_io(pll);
	if (rc)
		goto error;

	rc = dp_pll_clock_register(pll);
	if (rc)
		goto error;

	DP_INFO("revision=%s, ssc_en=%d, bonding_en=%d\n",
			dp_pll_get_revision(pll->revision), pll->ssc_en,
			pll->bonding_en);

	return pll;
error:
	kfree(pll);
	return ERR_PTR(rc);
}

void dp_pll_put(struct dp_pll *pll)
{
	dp_pll_clock_unregister(pll);
	kfree(pll);
}
