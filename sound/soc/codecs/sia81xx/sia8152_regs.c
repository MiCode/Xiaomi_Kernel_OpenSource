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
#define LOG_FLAG	"sia8152_regs"


#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia8152_regs.h"

#define SIA8152_WRITEABLE_REG_NUM			(5)

static const char sia8152_palyback_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x93,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x93,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_voice_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x93,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x93,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_receiver_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x43,		//SIA8152_REG_MOD_CFG
				0xD4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x43,		//SIA8152_REG_MOD_CFG
				0xD4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

static const char sia8152_factory_defaults[][SIA8152_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x13,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x13,		//SIA8152_REG_MOD_CFG
				0xE4,		//SIA8152_REG_SYS_EN
				0x80,		//SIA8152_REG_OVP_CFG
				0x00,		//SIA8152_REG_OPC_HCFG
				0x00		//SIA8152_REG_TEST_CFG
	}
};

const struct sia81xx_reg_default_val sia8152_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA8152, 
	.offset = SIA8152_REG_MOD_CFG,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia8152_palyback_defaults,
		.num = ARRAY_SIZE(sia8152_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia8152_voice_defaults,
		.num = ARRAY_SIZE(sia8152_voice_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_RECEIVER] = {
		.vals = (char *)sia8152_receiver_defaults,
		.num = ARRAY_SIZE(sia8152_receiver_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_FACTORY] = {
		.vals = (char *)sia8152_factory_defaults,
		.num = ARRAY_SIZE(sia8152_factory_defaults[0])
	}
};


static const SIA_CHIP_ID_RANGE chip_id_ranges[] = {
	{0x52, 0x57},
	{0x5C, 0x5F}
};

static const SIA_REGS trimming_regs[] = {
	{0x09, 0x86},
	{0x10, 0x00},
	{0x11, 0x60}
};

static bool sia8152_writeable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8152_REG_MOD_CFG ... 0x15:	// SIA8152_REG_TEST_CFG :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8152_readable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8152_REG_CHIP_ID ... 0x15:	// SIA8152_REG_STATE_FLAG2 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8152_volatile_register(
	struct device *dev, 
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia8152_regmap_config = {
	.name = "sia8152",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia8152_writeable_register,
	.readable_reg = sia8152_readable_register,
	.volatile_reg = sia8152_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static int sia8152_check_chip_id(
	struct regmap *regmap) 
{
	int i = 0;
	char val = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA8152_REG_CHIP_ID, 1, &val))
		return -1;

	for (i = 0; i < ARRAY_SIZE(chip_id_ranges); i++) {
		if (val >= chip_id_ranges[i].begin && val <= chip_id_ranges[i].end)
			return 0;
	}

	return -1;
}

static void sia8152_chip_on(
	struct regmap *regmap, 
	unsigned int scene,
	unsigned int channel_num)
{
	char val = 0;

	if (AUDIO_SCENE_NUM <= scene || SIA81XX_CHANNEL_NUM<= channel_num)
		return;

	if (0 != sia81xx_regmap_read(regmap, SIA8152_REG_MOD_CFG, 1, &val))
		return;

	val |= 0x02;
	if (0 != sia81xx_regmap_write(regmap, SIA8152_REG_MOD_CFG, 1, &val))
		return;

	if (0 != sia81xx_regmap_read(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;

	val |= 0x01;
	if (0 != sia81xx_regmap_write(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;

	/* SIA8152_REG_OVP_CFG must be set after chip_en */
	if (0 != sia81xx_regmap_write(regmap, SIA8152_REG_OVP_CFG, 1, 
		&(sia8152_reg_default_val.reg_defaults[scene].vals + 
			channel_num * SIA8152_WRITEABLE_REG_NUM)
			[SIA8152_REG_OVP_CFG - sia8152_reg_default_val.offset]))
		return;
}

static void sia8152_chip_off(
	struct regmap *regmap)
{
	char val = 0;

	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return;

	/* wait chip power down */
	udelay(500);

	val = 0x10;
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_MOD_CFG, 1, &val))
		return;

	val = 0x80;
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_SYS_EN, 1, &val))
		return;

	mdelay(1);	// wait chip power off
}

static bool sia8152_get_chip_en(
	struct regmap *regmap)
{
	char val = 0;

	if (0 != sia81xx_regmap_read(regmap, SIA8152_REG_OPC_HCFG, 1, &val))
		return false;

	if (val & 0x01)
		return true;

	return false;
}

static void sia8152_set_pvdd_limit(
	struct regmap *regmap, unsigned int vol)
{
	char val = 0;
	int8_t cp_ovp = 0;

	if (3200000 > vol || 4500000 < vol) {
		pr_err("[  err][%s] %s: voltage = %u out of range !!! \r\n", 
			LOG_FLAG, __func__, vol);
		return;
	}

	cp_ovp = (int8_t)((vol << 3) / 1000000 - 24 - 1);

	if (0 > cp_ovp)
		cp_ovp = 0;

	if (8 < cp_ovp)	// pvdd <= 8.0V
		cp_ovp = 8;

	if(0 != sia81xx_regmap_read(regmap, SIA8152_REG_OVP_CFG, 1, &val))
		return;

	val = (val & 0x0f) | (cp_ovp << 4);
	if(0 != sia81xx_regmap_write(regmap, SIA8152_REG_OVP_CFG, 1, &val))
		return;
}

extern int sia8152s_check_chip_id(struct regmap *regmap);
extern void sia8152s_check_trimming(struct regmap *regmap);
static void sia8152_check_trimming(
	struct regmap *regmap)
{
	int i = 0;
	const uint32_t reg_num = ARRAY_SIZE(trimming_regs);
	uint8_t vals[reg_num] = {0};
	uint8_t crc = 0;

	if (0 == sia8152s_check_chip_id(regmap))
		sia8152s_check_trimming(regmap);

	/* wait reading trimming data to reg */
	mdelay(1);

	for (i = 0; i < reg_num; i++) {
		if (0 != sia81xx_regmap_read(regmap, 
			trimming_regs[i].addr, 1, (char *)&vals[i]))
			return ;
	}

	crc = vals[reg_num - 1] & 0x0F;
	vals[reg_num - 1] &= 0xF0;

	if (crc != crc4_itu(vals, reg_num)) {
		pr_warn("[ warn][%s] %s: trimming failed !! \r\n", 
			LOG_FLAG, __func__);

		if (0 != sia81xx_regmap_read(regmap, SIA8152_REG_OPC_HCFG, 1, (char *)vals))
			return;

		*vals |= 0x02;
		if (0 != sia81xx_regmap_write(regmap, SIA8152_REG_OPC_HCFG, 1, (char *)vals))
			return;

		for (i = 0; i < reg_num; i++)
			sia81xx_regmap_write(regmap, 
				trimming_regs[i].addr, 1, (char *)&(trimming_regs[i].val));
	}
}

const struct sia81xx_opt_if sia8152_opt_if = {
	.check_chip_id = sia8152_check_chip_id,
	.set_xfilter = NULL,
	.chip_on = sia8152_chip_on,
	.chip_off = sia8152_chip_off,
	.get_chip_en = sia8152_get_chip_en,
	.set_pvdd_limit = sia8152_set_pvdd_limit,
	.check_trimming = sia8152_check_trimming,
};

