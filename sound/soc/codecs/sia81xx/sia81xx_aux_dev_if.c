/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#define DEBUG
#define LOG_FLAG	"sia81xx_aux"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/soc.h>

unsigned int soc_sia81xx_get_aux_num(
	struct platform_device *pdev)
{
	int ret = 0;
	u32 max_dev_num = 0, dev_num = 0;

	if(NULL == pdev) {
		pr_err("[  err][%s] : NULL == pdev !!! \r\n", 
			__func__);
		return 0;
	}

	if(NULL == pdev->dev.of_node) {
		pr_err("[  err][%s] : NULL == pdev->dev.of_node !!! \r\n", 
			__func__);
		return 0;
	}
	
	ret = of_property_read_u32(pdev->dev.of_node, 
				"si,sia81xx-max-num", &max_dev_num);
	if (0 != ret) {
		pr_err("[  err][%s] : of_property_read_u32 ret = %d !!! \r\n", 
			__func__, ret);
		return 0;
	}
	
	if (0 == max_dev_num) {
		pr_warn("[ warn][%s] : max_dev_num = %u !!! \r\n", 
			__func__, max_dev_num);
		return 0;
	}

	/* Get count of WSA device phandles for this platform */
	dev_num = of_count_phandle_with_args(pdev->dev.of_node, 
					"si,sia81xx-aux-devs", NULL);
	if(0 >= dev_num) {
		pr_warn("[ warn][%s] : dev_num = %u !!! \r\n", 
			__func__, dev_num);
		return 0;
	}

	if(dev_num > max_dev_num)
		dev_num = max_dev_num;

	return (unsigned int)dev_num;
}
EXPORT_SYMBOL(soc_sia81xx_get_aux_num);

unsigned int soc_sia81xx_get_codec_conf_num(
	struct platform_device *pdev)
{
	return soc_sia81xx_get_aux_num(pdev);
}
EXPORT_SYMBOL(soc_sia81xx_get_codec_conf_num);

int soc_sia81xx_init(
	struct platform_device *pdev, 
	struct snd_soc_aux_dev *aux_dev, 
	u32 aux_num, 
	struct snd_soc_codec_conf *codec_conf, 
	u32 conf_num)
{
	int ret, i;
	u32 dev_num = 0, prefix_num = 0;
	struct device_node *dev_of_node;
	const char *dev_name_prefix = NULL;
	char *aux_dev_name = NULL;

	if((0 == aux_num) || (0 == conf_num)) {
		pr_err("[  err][%s] : aux_num = %u, config_num= %u \r\n", 
			__func__, aux_num, conf_num);
		return -EINVAL;
	}

	dev_num = soc_sia81xx_get_aux_num(pdev);
	if((aux_num != dev_num) || (conf_num != dev_num)) {
		pr_err("[  err][%s] : aux_num = %u, config_num= %u, "
			"dev_num = %u !!! \r\n", 
			__func__, aux_num, conf_num, dev_num);
		return -EINVAL;
	}

	prefix_num = of_property_count_strings(pdev->dev.of_node,
						"si,sia81xx-aux-devs-prefix");
	if(dev_num > prefix_num) {
		pr_err("[  err][%s] : dev_num = %u, prefix_num = %u !!! \r\n", 
			__func__, dev_num, prefix_num);
		return -EINVAL;
	}

	for (i = 0; i < dev_num; i++) {
		dev_of_node = of_parse_phandle(pdev->dev.of_node, 
							"si,sia81xx-aux-devs", i);
		if (unlikely(NULL == dev_of_node)) {
			pr_warn("[ warn][%s]: sia81xx dev %d parse error !!! \r\n", 
				__func__, i);
			return  -EINVAL;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
						    "si,sia81xx-aux-devs-prefix",
						    i, &dev_name_prefix);
		if (0 != ret) {
			pr_warn("[ warn][%s] : sia81xx dev %d "
				"parse prefix ret = %d !!! \r\n", 
				__func__, i, ret);
			return ret;
		}

		aux_dev_name = devm_kzalloc(&pdev->dev, 128, GFP_KERNEL);
		if (NULL == aux_dev_name) { 
			/*
			 * FIXED ME : before i, the aux_dev_pos[i].name's memory
			 * didn't free !!! 
			 */
			return -ENOMEM;
		}
		
		snprintf(aux_dev_name, strlen("sia81xx.%d"), "sia81xx.%d", i);
		aux_dev[i].name = (const char *)aux_dev_name;
		aux_dev[i].codec_name = NULL;
		aux_dev[i].codec_of_node = dev_of_node;
		aux_dev[i].init = NULL;

		codec_conf[i].dev_name = NULL;
		codec_conf[i].name_prefix = dev_name_prefix;
		codec_conf[i].of_node = dev_of_node;

		pr_debug("[debug][%s] : aux_dev = %s \r\n", 
			__func__, aux_dev[i].name);
	}

	return 0;
}
EXPORT_SYMBOL(soc_sia81xx_init);

int soc_aux_init_only_sia81xx(
	struct platform_device *pdev, 
	struct snd_soc_card *card)
{
	int ret = 0;
	u32 aux_num = 0, conf_num = 0;
	struct snd_soc_aux_dev *aux_dev = NULL;
	struct snd_soc_codec_conf *codec_conf = NULL;

	aux_num = soc_sia81xx_get_aux_num(pdev);
	conf_num = soc_sia81xx_get_codec_conf_num(pdev);

	if((aux_num != conf_num) || (0 == aux_num)) {
		pr_err("[  err][%s] : aux_num = %u, conf_num= %u !!! \r\n", 
			__func__, aux_num, conf_num);
		return -EINVAL;
	}

	/* make sure that the "dev_num" must be greater than 0 !!! */
	aux_dev = devm_kzalloc(card->dev, 
					aux_num * sizeof(struct snd_soc_aux_dev),
					GFP_KERNEL);
	if (NULL == aux_dev)
		return -ENOMEM;

	codec_conf = devm_kzalloc(card->dev, 
					conf_num * sizeof(struct snd_soc_codec_conf),
					GFP_KERNEL);
	if (NULL == codec_conf)
		return -ENOMEM;

	ret = soc_sia81xx_init(pdev, aux_dev, aux_num, codec_conf, conf_num);
	if(0 != ret) {
		pr_err("[  err][%s] : soc_sia81xx_init ret = %d !!! \r\n", 
			__func__, ret);
		return ret;
	}

	card->aux_dev = aux_dev;
	card->codec_conf = codec_conf;

	card->num_aux_devs = aux_num;
	card->num_configs = conf_num;

	return 0;
}
EXPORT_SYMBOL(soc_aux_init_only_sia81xx);



