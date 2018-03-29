/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include "mt_afe_def.h"
#include <linux/module.h>
#include <sound/soc.h>

static struct snd_soc_dai_driver mt_dai_stub_dai[] = {
	{
	 .playback = {
		      .stream_name = MT_SOC_DL1_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		      .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		      },
	 .name = MT_SOC_DL1_CPU_DAI_NAME,
	 },
	{
	 .capture = {
		     .stream_name = MT_SOC_UL1_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_UL1_CPU_DAI_NAME,
	 },
	{
	 .playback = {
		      .stream_name = MT_SOC_BTSCO_DL_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = BTSCO_OUT_CHANNELS_MIN,
		      .channels_max = BTSCO_OUT_CHANNELS_MAX,
		      },
	 .capture = {
		     .stream_name = MT_SOC_BTSCO_UL_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = BTSCO_IN_CHANNELS_MIN,
		     .channels_max = BTSCO_IN_CHANNELS_MAX,
		     },
	 .name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 },
	{
	 .capture = {
		     .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_DL1_AWB_CPU_DAI_NAME,
	 },
	{
	 .capture = {
		     .stream_name = MT_SOC_UL2_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_UL2_CPU_DAI_NAME,
	 },
	{
	 .playback = {
		      .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = HDMI_CHANNELS_MIN,
		      .channels_max = HDMI_CHANNELS_MAX,
		      .rate_min = HDMI_RATE_MIN,
		      .rate_max = HDMI_RATE_MAX,
		      },
	 .name = MT_SOC_HDMI_CPU_DAI_NAME,
	 },
	{
	 .playback = {
		      .stream_name = MT_SOC_HDMI_RAW_PLAYBACK_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SPDIF_CHANNELS_MIN,
		      .channels_max = SPDIF_CHANNELS_MAX,
		      },
	 .name = MT_SOC_HDMI_RAW_CPU_DAI_NAME,
	 },
	{
	 .playback = {
		      .stream_name = MT_SOC_SPDIF_PLAYBACK_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SPDIF_CHANNELS_MIN,
		      .channels_max = SPDIF_CHANNELS_MAX,
		      },
	 .name = MT_SOC_SPDIF_CPU_DAI_NAME,
	 },
	 {
	 .capture = {
		     .stream_name = MT_SOC_I2S0_AWB_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_I2S0_AWB_CPU_DAI_NAME,
	 },
	{
	 .playback = {
		      .stream_name = MT_SOC_MRGRX_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		      .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		      },
	 .name = MT_SOC_MRGRX_CPU_DAI_NAME,
	 },
	{
	 .capture = {
		     .stream_name = MT_SOC_MRGRX_AWB_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_MRGRX_AWB_CPU_DAI_NAME,
	 },
	 {
	 .playback = {
		      .stream_name = MT_SOC_DL2_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		      .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		      },
	 .name = MT_SOC_DL2_CPU_DAI_NAME,
	 },
	 {
	 .playback = {
		      .stream_name = MT_SOC_BTSCO2_DL_STREAM_NAME,
		      .rates = STUB_RATES,
		      .formats = STUB_FORMATS,
		      .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		      .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		      },
	 .capture = {
		     .stream_name = MT_SOC_BTSCO2_UL_STREAM_NAME,
		     .rates = STUB_RATES,
		     .formats = STUB_FORMATS,
		     .channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
		     .channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
		     },
	 .name = MT_SOC_BTSCO2_CPU_DAI_NAME,
	 },
};

static const struct snd_soc_component_driver mt_dai_component = {
	.name = MT_SOC_STUB_CPU_DAI
};

static int mt_dai_stub_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_STUB_CPU_DAI);
		pr_debug("%s set dev name %s\n", __func__, dev_name(&pdev->dev));
	}

	rc = snd_soc_register_component(&pdev->dev, &mt_dai_component, mt_dai_stub_dai,
					ARRAY_SIZE(mt_dai_stub_dai));

	return rc;
}

static int mt_dai_stub_dev_remove(struct platform_device *pdev)
{

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_dai_stub_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_STUB_CPU_DAI,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_dai_stub_dt_match);

static struct platform_driver mt_dai_stub_driver = {
	.probe = mt_dai_stub_dev_probe,
	.remove = mt_dai_stub_dev_remove,
	.driver = {
		   .name = MT_SOC_STUB_CPU_DAI,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_dai_stub_dt_match,
		   },
};

module_platform_driver(mt_dai_stub_driver);

/* Module information */
MODULE_DESCRIPTION("MTK SOC DAI driver");
MODULE_LICENSE("GPL v2");
