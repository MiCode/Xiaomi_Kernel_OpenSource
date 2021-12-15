// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/nvmem-consumer.h>
#include <linux/types.h>
#include <linux/err.h>

#include "ccci_rtc.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "ccci_rtc"

static int g_ccci_rtc_val;


int mtk_crystal_exist_status(void)
{
	CCCI_ERROR_LOG(-1, TAG, "[%s] g_ccci_rtc_val : %d.",
			__func__, g_ccci_rtc_val);

	return g_ccci_rtc_val;
}
EXPORT_SYMBOL(mtk_crystal_exist_status);

static int ccci_get_rtc_info(struct platform_device *pdev)
{
	struct nvmem_cell *cell = NULL;
	u8 *buf = NULL;
	ssize_t len = 0;

	cell = nvmem_cell_get(&pdev->dev, "external-32k");
	if (!cell) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] nvmem_cell_get fail: %zu",
			__func__, PTR_ERR(cell));
		return -1;
	}
	if (IS_ERR(cell)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] nvmem_cell_get fail: %zu",
				__func__, PTR_ERR(cell));

		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);

		goto fail;
	}

	buf = (u8 *)nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] nvmem_cell_read fail: %zu\n",
				__func__, PTR_ERR(buf));
		goto fail;
	}

	g_ccci_rtc_val = (u8)(*buf);

	kfree(buf);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] g_ccci_rtc_val = %d; len = %zu\n",
			__func__, g_ccci_rtc_val, len);

	return 0;

fail:
	return -1;
}

static int ccci_rtc_probe(struct platform_device *pdev)
{
	return ccci_get_rtc_info(pdev);
}

static const struct of_device_id ccci_rtc_of_ids[] = {
	{.compatible = "mediatek,md_ccci_rtc"},
	{}
};

static struct platform_driver ccci_rtc_driver = {
	.driver = {
		.name = "md_ccci_rtc",
		.of_match_table = ccci_rtc_of_ids,
	},

	.probe = ccci_rtc_probe,
};

static int __init ccci_rtc_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_rtc_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci rtc driver init fail %d", ret);
		return ret;
	}
	return 0;
}

module_init(ccci_rtc_init);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci rtc driver");
MODULE_LICENSE("GPL");
