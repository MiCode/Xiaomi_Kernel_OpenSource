// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/clk.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "ccci_md_clk"

struct md_ao_clk {
	struct clk *clk_ref;
	unsigned char *clk_name;
};

static struct md_ao_clk md_ao_clk_tbl[] = {
	{NULL, "infra-aes-bclk-md"},
	{NULL, "infra-temp-share-md"},
};

static int md_clk_probe(struct platform_device *pdev)
{
	int ret, i;
	struct clk *clk;
	unsigned char *name;

	for (i = 0; i < ARRAY_SIZE(md_ao_clk_tbl); i++) {
		name = md_ao_clk_tbl[i].clk_name;
		clk = devm_clk_get(&pdev->dev, name);
		if (IS_ERR(clk)) {
			CCCI_ERROR_LOG(-1, TAG, "%s n/a\r\n", name);
			continue;
		}
		md_ao_clk_tbl[i].clk_ref = clk;
	}

	for (i = 0; i < ARRAY_SIZE(md_ao_clk_tbl); i++) {
		if (!md_ao_clk_tbl[i].clk_ref) {
			CCCI_ERROR_LOG(-1, TAG, "%s skip\r\n", name);
			continue;
		}

		ret = clk_prepare_enable(md_ao_clk_tbl[i].clk_ref);
		if (ret) {
			name = md_ao_clk_tbl[i].clk_name;
			CCCI_ERROR_LOG(-1, TAG, "%s fail:%d\r\n", name, ret);
		} else {
			name = md_ao_clk_tbl[i].clk_name;
			CCCI_ERROR_LOG(-1, TAG, "en %s success\r\n", name);
		}

		return ret;
	}

	return 0;
}


static const struct of_device_id md_clk_of_ids[] = {
	{.compatible = "mediatek,ccci_md_clk"},
	{}
};


static struct platform_driver md_clk_driver = {

	.driver = {
			.name = "ccci_md_clk",
			.of_match_table = md_clk_of_ids,
	},

	.probe = md_clk_probe,
};

static int __init md_clk_init(void)
{
	int ret;

	ret = platform_driver_register(&md_clk_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "%s fail %d", __func__, ret);
		return ret;
	}
	return 0;
}

module_init(md_clk_init);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci auxadc driver");
MODULE_LICENSE("GPL");
