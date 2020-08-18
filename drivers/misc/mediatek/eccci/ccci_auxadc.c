/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "ccci_adc"

static struct iio_channel *md_channel;
static int adc_num;
static int adc_val;

static int ccci_get_adc_info(struct device *dev)
{
	int ret, val;

	md_channel = devm_kzalloc(dev, sizeof(struct iio_channel), GFP_KERNEL);
	if (!md_channel) {
		CCCI_ERROR_LOG(-1, TAG, "allocate md channel fail");
		return -1;
	}
	md_channel = iio_channel_get(dev, "md-channel");

	ret = IS_ERR(md_channel);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "fail to get iio channel (%d)", ret);
		goto Fail;
	}
	adc_num = md_channel->channel->channel;
	ret = iio_read_channel_processed(md_channel, &val);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "iio_read_channel_processed fail");
		goto Fail;
	}

	adc_val = val;
	CCCI_NORMAL_LOG(0, TAG, "md_ch = %d, val = %d", adc_num, adc_val);
	return ret;
Fail:
	return -1;

}

int ccci_get_adc_num(void)
{
	return adc_num;
}

int ccci_get_adc_val(void)
{
	return adc_val;
}

int get_auxadc_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_get_adc_info(&pdev->dev);
	if (ret < 0)
		CCCI_ERROR_LOG(-1, TAG, "ccci get adc info fail");
	return 0;
}


static const struct of_device_id ccci_auxadc_of_ids[] = {
	{.compatible = "mediatek,md_auxadc"},
	{}
};


static struct platform_driver ccci_auxadc_driver = {

	.driver = {
			.name = "ccci_auxadc",
			.of_match_table = ccci_auxadc_of_ids,
	},

	.probe = get_auxadc_probe,
};

static int __init ccci_auxadc_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_auxadc_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci auxadc driver init fail %d", ret);
		return ret;
	}
	return 0;
}

module_init(ccci_auxadc_init);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci auxadc driver");
MODULE_LICENSE("GPL");
