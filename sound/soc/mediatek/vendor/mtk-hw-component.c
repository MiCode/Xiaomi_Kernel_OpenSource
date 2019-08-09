/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk-hw-compoent.c
 *
 * Project:
 * --------
 *   Audio soc machine vendor ops
 *
 * Description:
 * ------------
 *   Audio machine driver
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

#include "mtk-hw-component.h"
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>

#if (defined(CONFIG_SND_SOC_CS35L35) || defined(CONFIG_SND_SOC_CS43130))
#include <mach/gpio_const.h>
#include <mt-plat/mtk_gpio.h>
#endif

#define GPIO_AP_VERSION0_PIN (GPIO164 | 0x80000000)
#define GPIO_AP_VERSION1_PIN (GPIO165 | 0x80000000)
#define GPIO_AP_VERSION2_PIN (GPIO166 | 0x80000000)
#define GPIO_AP_VERSION3_PIN (GPIO167 | 0x80000000)

#ifdef CONFIG_SND_SOC_CS43130
const char cs43130_ver0_name[] = "cs43130.2-0030";
const char cs43130_ver1_name[] = "cs43130.0-0030";
#endif

#ifdef CONFIG_SND_SOC_CS35L35
const char cs35l35_ver0_name[] = "cs35l35.2-0040";
const char cs35l35_ver1_name[] = "cs35l35.0-0040";
#endif

#if (defined(CONFIG_SND_SOC_CS35L35) || defined(CONFIG_SND_SOC_CS43130))
unsigned int get_hw_version(void)
{
	static unsigned int hw_version, flag;
	unsigned int version0 = 0;
	unsigned int version1 = 0;
	unsigned int version2 = 0;
	unsigned int version3 = 0;

	if (flag)
		return hw_version;
	flag = 1;
	version0 = mt_get_gpio_in(GPIO_AP_VERSION0_PIN);
	version1 = mt_get_gpio_in(GPIO_AP_VERSION1_PIN);
	version2 = mt_get_gpio_in(GPIO_AP_VERSION2_PIN);
	version3 = mt_get_gpio_in(GPIO_AP_VERSION3_PIN);

	hw_version = ((version3 << 3) | (version2 << 2) | (version1 << 1) |
		      version0);
	pr_debug("%s hw_version = 0x%x\n", __func__, hw_version);
	return hw_version;
}
#endif

/* vendor implement to get dai name*/
int get_exthp_dai_codec_name(struct snd_soc_dai_link *mt_soc_exthp_dai)
{

#if (defined(CONFIG_SND_SOC_CS43130))
	int hw_version = -1;

	hw_version = get_hw_version();
	switch (hw_version) {
	case 0:
	case 1:
	case 2:
		mt_soc_exthp_dai[0].codec_name = cs43130_ver0_name;
		break;
	case 3:
		mt_soc_exthp_dai[0].codec_name = cs43130_ver1_name;
		break;
	default:
		pr_debug("hw_version %0x, use default ver1_name\n", hw_version);
		mt_soc_exthp_dai[0].codec_name = cs43130_ver1_name;
	}
#endif

	return 0;
}

/* vendor implement to get dai name*/
int get_extspk_dai_codec_name(struct snd_soc_dai_link *mt_soc_extspk_dai)
{

#if (defined(CONFIG_SND_SOC_CS35L35))
	int hw_version = -1;

	hw_version = get_hw_version();
	switch (hw_version) {
	case 0:
	case 1:
	case 2:
		mt_soc_extspk_dai[0].codec_name = cs35l35_ver0_name;
		break;
	case 3:
		mt_soc_extspk_dai[0].codec_name = cs35l35_ver1_name;
		break;
	default:
		pr_debug("hw_version %0x, use default ver1_name\n", hw_version);
		mt_soc_extspk_dai[0].codec_name = cs35l35_ver1_name;
	}
#endif

	return 0;
}
