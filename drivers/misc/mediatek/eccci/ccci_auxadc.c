// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_debug.h"

#define TAG "ccci_adc"

static struct device *md_adc_pdev;
static int adc_num;
static int adc_val;
static int adc_mV;

#ifdef CCCI_KMODULE_ENABLE
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
#endif

static int ccci_get_adc_info(struct device *dev)
{
	int ret, val, mV;
	struct iio_channel *md_channel;

	adc_val = -1;
	adc_mV = -1;
	md_channel = iio_channel_get(dev, "md-channel");

	ret = IS_ERR(md_channel);
	if (ret) {
		if (PTR_ERR(md_channel) == -EPROBE_DEFER) {
			CCCI_ERROR_LOG(-1, TAG, "%s EPROBE_DEFER\r\n",
					__func__);
			return -EPROBE_DEFER;
		}
		CCCI_ERROR_LOG(-1, TAG, "fail to get iio channel (%d)", ret);
		goto Fail;
	}
	adc_num = md_channel->channel->channel;

	ret = iio_read_channel_raw(md_channel, &val);
	if (ret < 0) {
		iio_channel_release(md_channel);
		CCCI_ERROR_LOG(-1, TAG, "iio_read_channel_raw fail");
		goto Fail;
	}
	adc_val = val;

	ret = iio_read_channel_processed(md_channel, &mV);
	iio_channel_release(md_channel);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "iio_read_channel_processed fail");
		goto Fail;
	}
	adc_mV = mV;

	CCCI_NORMAL_LOG(0, TAG, "md_ch = %d, raw_val = %d[%dmV]\n",
		adc_num, val, mV);
	return ret;

Fail:
	return -1;
}

int ccci_get_adc_num(void)
{
	return adc_num;
}
EXPORT_SYMBOL(ccci_get_adc_num);

int ccci_get_adc_val(void)
{
	return adc_val;
}
EXPORT_SYMBOL(ccci_get_adc_val);

int ccci_get_adc_mV(void)
{
	return adc_mV;
}
EXPORT_SYMBOL(ccci_get_adc_mV);

int get_auxadc_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_get_adc_info(&pdev->dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci get adc info fail");
		return ret;
	}
	md_adc_pdev = &pdev->dev;
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
