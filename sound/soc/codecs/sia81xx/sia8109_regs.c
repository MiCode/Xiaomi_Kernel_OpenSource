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
#define LOG_FLAG	"sia8109_regs"

 
#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia8109_regs.h"

#define SIA8108_WRITEABLE_REG_NUM			(14)

static const char sia8109_palyback_defaults[][SIA8108_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x07,		//SIA8109_REG_SYSCTRL
				0x80,		//SIA8109_REG_AGCCTRL
				0xCE,		//SIA8109_REG_BOOST_CFG
				0x09,		//SIA8109_REG_CLSD_CFG1
				0x26,		//SIA8109_REG_CLSD_CFG2
				0x0A,		//SIA8109_REG_BSG_CFG
				0x02,		//SIA8109_REG_SML_CFG1
				0x28,		//SIA8109_REG_SML_CFG2
				0x05,		//SIA8109_REG_SML_CFG3
				0x8A,		//SIA8109_REG_SML_CFG4
				0x00,		//SIA8109_REG_Excur_CTRL_1
				0x00,		//SIA8109_REG_Excur_CTRL_2
				0x00,		//SIA8109_REG_Excur_CTRL_3
				0x00		//SIA8109_REG_Excur_CTRL_4
	},
	[SIA81XX_CHANNEL_R] = {
				0x07,		//SIA8109_REG_SYSCTRL
				0x80,		//SIA8109_REG_AGCCTRL
				0xCE,		//SIA8109_REG_BOOST_CFG
				0x09,		//SIA8109_REG_CLSD_CFG1
				0x26,		//SIA8109_REG_CLSD_CFG2
				0x0A,		//SIA8109_REG_BSG_CFG
				0x02,		//SIA8109_REG_SML_CFG1
				0x28,		//SIA8109_REG_SML_CFG2
				0x05,		//SIA8109_REG_SML_CFG3
				0x8A,		//SIA8109_REG_SML_CFG4
				0x00,		//SIA8109_REG_Excur_CTRL_1
				0x00,		//SIA8109_REG_Excur_CTRL_2
				0x00,		//SIA8109_REG_Excur_CTRL_3
				0x00		//SIA8109_REG_Excur_CTRL_4
	}
};

static const char sia8109_voice_defaults[][SIA8108_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x07,		//SIA8109_REG_SYSCTRL
				0x80,		//SIA8109_REG_AGCCTRL
				0xCE,		//SIA8109_REG_BOOST_CFG
				0x09,		//SIA8109_REG_CLSD_CFG1
				0x26,		//SIA8109_REG_CLSD_CFG2
				0x0A,		//SIA8109_REG_BSG_CFG
				0x02,		//SIA8109_REG_SML_CFG1
				0x28,		//SIA8109_REG_SML_CFG2
				0x05,		//SIA8109_REG_SML_CFG3
				0x8A,		//SIA8109_REG_SML_CFG4
				0x00,		//SIA8109_REG_Excur_CTRL_1
				0x00,		//SIA8109_REG_Excur_CTRL_2
				0x00,		//SIA8109_REG_Excur_CTRL_3
				0x00		//SIA8109_REG_Excur_CTRL_4
	},
	[SIA81XX_CHANNEL_R] = {
				0x07,		//SIA8109_REG_SYSCTRL
				0x80,		//SIA8109_REG_AGCCTRL
				0xCE,		//SIA8109_REG_BOOST_CFG
				0x09,		//SIA8109_REG_CLSD_CFG1
				0x26,		//SIA8109_REG_CLSD_CFG2
				0x0A,		//SIA8109_REG_BSG_CFG
				0x02,		//SIA8109_REG_SML_CFG1
				0x28,		//SIA8109_REG_SML_CFG2
				0x05,		//SIA8109_REG_SML_CFG3
				0x8A,		//SIA8109_REG_SML_CFG4
				0x00,		//SIA8109_REG_Excur_CTRL_1
				0x00,		//SIA8109_REG_Excur_CTRL_2
				0x00,		//SIA8109_REG_Excur_CTRL_3
				0x00		//SIA8109_REG_Excur_CTRL_4
	}
};

const struct sia81xx_reg_default_val sia8109_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA8109, 
	.offset = SIA8109_REG_SYSCTRL,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia8109_palyback_defaults,
		.num = ARRAY_SIZE(sia8109_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia8109_voice_defaults,
		.num = ARRAY_SIZE(sia8109_voice_defaults[0])
	}
};

static bool sia8109_writeable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8109_REG_SYSCTRL ... SIA8109_REG_Excur_CTRL_4 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8109_readable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8109_REG_SYSCTRL ... SIA8109_REG_Excur_CTRL_4 :
		case SIA8109_REG_CHIP_ID : 
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8109_volatile_register(
	struct device *dev, 
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia8109_regmap_config = {
	.name = "sia8109",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia8109_writeable_register,
	.readable_reg = sia8109_readable_register,
	.volatile_reg = sia8109_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int sia8109_check_chip_id(
	struct regmap *regmap) 
{
	int val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8109_REG_CHIP_ID, 1, &val))
		return -1;

	if(SIA8109_CHIP_ID_V0_1 != val)
		return -1;

	return 0;
}

static void sia8109_set_xfilter(
	struct regmap *regmap, 
	unsigned int val)
{
	const char buf = 0x10;
	
	if(1 == val) {
		sia81xx_regmap_write(regmap, SIA8109_REG_Excur_CTRL_2, 1, &buf);
	}

	pr_debug("[debug][%s] %s: val = %u \r\n", LOG_FLAG, __func__, val);
}

const struct sia81xx_opt_if sia8109_opt_if = {
	.check_chip_id = sia8109_check_chip_id,
	.set_xfilter = sia8109_set_xfilter,
};



