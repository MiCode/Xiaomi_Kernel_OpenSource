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
#define LOG_FLAG	"sia8159_regs"

#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia8159_regs.h"

#define SIA8159_WRITEABLE_REG_NUM			(10)

static const char sia8159_palyback_defaults[][SIA8159_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	},
	[SIA81XX_CHANNEL_R] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	}
};

static const char sia8159_voice_defaults[][SIA8159_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	},
	[SIA81XX_CHANNEL_R] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	}
};

static const char sia8159_receiver_defaults[][SIA8159_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0xAB,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	},
	[SIA81XX_CHANNEL_R] = {
				0xAB,		//SIA8159_REG_SYSCTRL
				0x20,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	}
};

static const char sia8159_factory_defaults[][SIA8159_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x27,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	},
	[SIA81XX_CHANNEL_R] = {
				0xBD,		//SIA8159_REG_SYSCTRL
				0x27,		//SIA8159_REG_ALGO_EN
				0xAE,		//SIA8159_REG_BST_CFG
				0xC9,		//SIA8159_REG_CLSD_CFG
				0x00,		//SIA8159_REG_ALGO_CFG1
				0x28,		//SIA8159_REG_ALGO_CFG2
				0x73,		//SIA8159_REG_ALGO_CFG3
				0x88,		//SIA8159_REG_ALGO_CFG4
				0x0D,		//SIA8159_REG_ALGO_CFG5
				0xA4		//SIA8159_REG_CLSD_OCPCFG
	}
};

const struct sia81xx_reg_default_val sia8159_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA8159, 
	.offset = SIA8159_REG_SYSCTRL,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia8159_palyback_defaults,
		.num = ARRAY_SIZE(sia8159_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia8159_voice_defaults,
		.num = ARRAY_SIZE(sia8159_voice_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_RECEIVER] = {
		.vals = (char *)sia8159_receiver_defaults,
		.num = ARRAY_SIZE(sia8159_receiver_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_FACTORY] = {
		.vals = (char *)sia8159_factory_defaults,
		.num = ARRAY_SIZE(sia8159_factory_defaults[0])
	}
};

static const SIA_CHIP_ID_RANGE chip_id_ranges[] = {
	{0x58, 0x58},
	{0x60, 0x68},
	{0x6A, 0x6F}
};

static bool sia8159_writeable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8159_REG_CHIP_ID ... 0x22 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8159_readable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8159_REG_CHIP_ID ... 0x22 :
		case 0x41 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8159_volatile_register(
	struct device *dev, 
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia8159_regmap_config = {
	.name = "sia8159",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia8159_writeable_register,
	.readable_reg = sia8159_readable_register,
	.volatile_reg = sia8159_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int sia8159_check_chip_id(
	struct regmap *regmap) 
{
	char val = 0;
	int i = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA8159_REG_CHIP_ID, 1, &val))
		return -1;

	for (i = 0; i < ARRAY_SIZE(chip_id_ranges); i++) {
		if (val >= chip_id_ranges[i].begin && val <= chip_id_ranges[i].end)
			return 0;
	}

	return -1;
}

static void sia8159_chip_on(
	struct regmap *regmap, unsigned int scene)
{
	char val = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA8159_REG_ALGO_CFG1, 1, &val))
		return;

	val |= 0x01;
	if (0 != sia81xx_regmap_write(regmap, SIA8159_REG_ALGO_CFG1, 1, &val))
		return;
}

static void sia8159_chip_off(
	struct regmap *regmap)
{
	char val = 0;

	if (0 != sia81xx_regmap_write(regmap, SIA8159_REG_ALGO_CFG1, 1, &val))
		return;

	val = 0x41;
	if (0 != sia81xx_regmap_write(regmap, SIA8159_REG_SYSCTRL, 1, &val))
		return;

	val = 0x20;
	if (0 != sia81xx_regmap_write(regmap, SIA8159_REG_ALGO_EN, 1, &val))
		return;

	mdelay(1);	// wait chip power off
}

static void sia8159_check_trimming(
	struct regmap *regmap)
{
	static const uint32_t reg_num = 
		SIA8159_REG_TRIMMING_END - SIA8159_REG_TRIMMING_BEGIN + 1;
	static const char defaults[reg_num] = {0x76, 0x66, 0x70};
	uint8_t vals[reg_num] = {0};
	uint8_t crc = 0;

	/* wait reading trimming data to reg */
	mdelay(1);

	if (0 != sia81xx_regmap_read(regmap, 
		SIA8159_REG_TRIMMING_BEGIN, reg_num, (char *)vals))
		return ;

	crc = vals[reg_num - 1] & 0x0F;
	vals[reg_num - 1] &= 0xF0;

	if (crc != crc4_itu(vals, reg_num)) {
		pr_warn("[ warn][%s] %s: trimming failed !! \r\n", 
			LOG_FLAG, __func__);

		if (0 != sia81xx_regmap_read(regmap, SIA8159_REG_ALGO_CFG2, 1, (char *)vals))
			return;

		*vals |= 0x02;
		if (0 != sia81xx_regmap_write(regmap, SIA8159_REG_ALGO_CFG2, 1, (char *)vals))
			return;

		sia81xx_regmap_write(regmap, 
			SIA8159_REG_TRIMMING_BEGIN, reg_num, defaults);
	}
}

const struct sia81xx_opt_if sia8159_opt_if = {
	.check_chip_id = sia8159_check_chip_id,
	.set_xfilter = NULL,
	.chip_on = sia8159_chip_on,
	.chip_off = sia8159_chip_off,
	.get_chip_en = NULL,
	.set_pvdd_limit = NULL,
	.check_trimming = sia8159_check_trimming,
};

