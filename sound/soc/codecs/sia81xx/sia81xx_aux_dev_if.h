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


#ifndef _SIA81XX_AUX_DEV_IF_H
#define _SIA81XX_AUX_DEV_IF_H

unsigned int soc_sia81xx_get_aux_num(
	struct platform_device *pdev);

unsigned int soc_sia81xx_get_codec_conf_num(
	struct platform_device *pdev);

int soc_sia81xx_init(
	struct platform_device *pdev, 
	struct snd_soc_aux_dev *aux_dev, 
	u32 aux_num, 
	struct snd_soc_codec_conf *codec_conf, 
	u32 conf_num);

int soc_aux_init_only_sia81xx(
	struct platform_device *pdev, 
	struct snd_soc_card *card);
int soc_aux_deinit_only_sia81xx(
	struct platform_device *pdev, 
	struct snd_soc_card *card);

#endif /* _SIA81XX_AUX_DEV_IF_H */

