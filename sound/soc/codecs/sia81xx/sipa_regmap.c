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
#define LOG_FLAG	"sipa_regmap"

#include <linux/regmap.h>
#include <linux/device.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>

#include "sipa_common.h"
#include "sipa_parameter.h"
#include "sipa_regmap.h"
#include "sia91xx_common.h"
#include "sipa_tuning_cmd.h"


struct reg_map_info {
	const uint32_t chip_type;
	const uint32_t reg_addr_width;
	const uint32_t reg_val_width;
	const uint32_t chip_id_addr;
	const SIPA_VAL_RANGE chip_id_ranges[4];
	const uint32_t chip_id_range_num;
};

static const struct reg_map_info reg_map_info_table[] = {
	[CHIP_TYPE_SIA8101] = {
		.chip_type = CHIP_TYPE_SIA8101,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x00,
		.chip_id_ranges = {{0x80, 0x8f}},
		.chip_id_range_num = 1
	},
	[CHIP_TYPE_SIA8109] = {
		.chip_type = CHIP_TYPE_SIA8109,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x41,
		.chip_id_ranges = {{0x09, 0x09}, {0x9B, 0x9B}},
		.chip_id_range_num = 2
	},
	[CHIP_TYPE_SIA8152] = {
		.chip_type = CHIP_TYPE_SIA8152,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x00,
		.chip_id_ranges = {{0x52, 0x57}},
		.chip_id_range_num = 1
	},
	[CHIP_TYPE_SIA8152S] = {
		.chip_type = CHIP_TYPE_SIA8152S,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x00,
		.chip_id_ranges = {{0x5C, 0x5F}},
		.chip_id_range_num = 1
	},
	[CHIP_TYPE_SIA8159] = {
		.chip_type = CHIP_TYPE_SIA8159,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x00,
		.chip_id_ranges = {{0x60, 0x68}, {0x6A, 0x6F}},
		.chip_id_range_num = 2
	},
	[CHIP_TYPE_SIA8159A] = {
		.chip_type = CHIP_TYPE_SIA8159A,
		.reg_addr_width = 8,
		.reg_val_width = 8,
		.chip_id_addr = 0x00,
		.chip_id_ranges = {{0x58, 0x58}},
		.chip_id_range_num = 1
	},
	[CHIP_TYPE_SIA9175] = {
		.chip_type = CHIP_TYPE_SIA9175,
		.reg_addr_width = 8,
		.reg_val_width = 16,
		.chip_id_addr = 0x07,
		.chip_id_ranges = {{9175, 9175}},
		.chip_id_range_num = 1
	},
	[CHIP_TYPE_SIA9195] = {
		.chip_type = CHIP_TYPE_SIA9195,
		.reg_addr_width = 8,
		.reg_val_width = 16,
		.chip_id_addr = 0x06,
		.chip_id_ranges = {{195<<7, (195<<7) | 0x7f}},
		.chip_id_range_num = 1
	},
};

int sia91xx_read_reg16(
	sipa_dev_t *si_pa,
	unsigned char subaddress,
	unsigned short *val)
{
	unsigned int value;
	int ret;
	int retries = I2C_RETRIES;

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: si_pa == NULL !!! \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

retry:
	ret = regmap_read(si_pa->regmap, subaddress, &value);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return -SIPA_ERROR_I2C;
	}
	*val = value & 0xffff;

	return SIPA_ERROR_OK;
}

int sia91xx_write_reg16(
	sipa_dev_t *si_pa,
	unsigned char subaddr,
	unsigned short val)
{
	int ret;
	int retries = I2C_RETRIES;

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: si_pa == NULL !!! \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

retry:
	ret = regmap_write(si_pa->regmap, subaddr, val);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return -SIPA_ERROR_I2C;
	}

	return SIPA_ERROR_OK;
}

int sipa_regmap_read(
	struct regmap *regmap,
	unsigned int chip_type,
	unsigned int start_reg,
	unsigned int reg_num,
	void *buf)
{
	if (NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (NULL == buf) {
		pr_err("[  err][%s] %s: NULL == buf \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (1 == reg_num) {
		unsigned int val = 0;
		int ret = regmap_read(regmap, start_reg, &val);
		if (0 != ret)
			return ret;

		switch (reg_map_info_table[chip_type].reg_val_width) {
		case 8:
			*(unsigned char *)buf = (unsigned char)val;
			break;
		case 16:
			*(unsigned short *)buf = (unsigned short)val;
			break;
		default:
			pr_err("[  err][%s] %s: reg_val_width error \r\n", 
				LOG_FLAG, __func__);
			return -EINVAL;
		}

		return 0;
	}

	return regmap_bulk_read(regmap, start_reg, buf, reg_num);
}
	
int sipa_regmap_write(
	struct regmap *regmap,
	unsigned int chip_type,
	unsigned int start_reg,
	unsigned int reg_num,
	const void *buf)
{
	if (NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (NULL == buf) {
		pr_err("[  err][%s] %s: NULL == buf \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (1 == reg_num) {
		unsigned int val = 0;

		switch (reg_map_info_table[chip_type].reg_val_width) {
		case 8:
			val = (unsigned int)(*(unsigned char *)buf);
			break;
		case 16:
			val = (unsigned int)(*(unsigned short *)buf);
			break;
		default:
			pr_err("[  err][%s] %s: reg_val_width error \r\n", 
				LOG_FLAG, __func__);
			return -EINVAL;
		}

		return regmap_write(regmap, start_reg, val);
	}

	return regmap_bulk_write(regmap, start_reg, buf, reg_num);
}

static bool sipa_writeable_register(
	struct device *dev,
	unsigned int reg)
{
	return true;
}

static bool sipa_readable_register(
	struct device *dev,
	unsigned int reg)
{
	return true;
}

static bool sipa_volatile_register(
	struct device *dev,
	unsigned int reg)
{
	return true;
}

static int verify_chip_type(
	unsigned int ch,
	unsigned int type)
{
	SIPA_CHIP_CFG chip_cfg;

	if (type >= ARRAY_SIZE(reg_map_info_table)) {
		//pr_warn("[ warn][%s] %s: chip_type = %u, "
		//	"ARRAY_SIZE(reg_map_info) = %lu \r\n",
		//	LOG_FLAG, __func__, type, ARRAY_SIZE(reg_map_info_table));
		return -ENODEV;
	}

	if (sipa_param_is_loaded()) {
		if (NULL == sipa_param_read_chip_cfg(ch, type, &chip_cfg)) {
			return -ENODEV;
		}

		if (reg_map_info_table[type].reg_addr_width 
				!= chip_cfg.reg_addr_width ||
			reg_map_info_table[type].reg_val_width
				!= chip_cfg.reg_val_width) {
			pr_err("[  err][%s] %s: "
				"reg_addr_width = %u "
				"chip_cfg.reg_addr_width = %u "
				"reg_val_width = %u "
				"chip_cfg.reg_val_width = %u \r\n",
				LOG_FLAG, __func__,
				reg_map_info_table[type].reg_addr_width,
				chip_cfg.reg_addr_width,
				reg_map_info_table[type].reg_val_width,
				chip_cfg.reg_val_width);
			return -ENODEV;
		}
	}

	return 0;
}

struct regmap *sipa_regmap_init(
	struct i2c_client *client,
	unsigned int ch,
	unsigned int chip_type)
{
	struct regmap_config config = {0};

	if (NULL == client)
		return NULL;

	if (0 != verify_chip_type(ch, chip_type))
		return NULL;

	config.name = "sipa";
	config.reg_bits = reg_map_info_table[chip_type].reg_addr_width;
	config.val_bits = reg_map_info_table[chip_type].reg_val_width;
	config.max_register = SIPA_MAX_REG_ADDR;
	config.writeable_reg = sipa_writeable_register;
	config.readable_reg = sipa_readable_register;
	config.volatile_reg = sipa_volatile_register;

	return devm_regmap_init_i2c(client, &config);
}

void sipa_regmap_remove(
	sipa_dev_t *si_pa)
{
	if (NULL == si_pa)
		return ;

	if (!IS_ERR(si_pa->regmap))
		regmap_exit(si_pa->regmap);
}

/********************************************************************
 * sia81xx reg map opt functions
 ********************************************************************/
int sipa_regmap_check_chip_id(
	struct regmap *regmap,
	unsigned int ch,
	unsigned int chip_type)
{
	int i = 0;
	unsigned int val = 0;

	if (NULL == regmap)
		return -EPERM;

	if (0 != verify_chip_type(ch, chip_type))
		return -EPERM;

	if (0 != regmap_read(regmap, reg_map_info_table[chip_type].chip_id_addr, &val))
		return -1;

	for (i = 0; i < reg_map_info_table[chip_type].chip_id_range_num; i++) {
		if (val >= reg_map_info_table[chip_type].chip_id_ranges[i].begin 
			&& val <= reg_map_info_table[chip_type].chip_id_ranges[i].end)
			return 0;
	}

	pr_err("[  err][%s] %s: chip_id_addr(0x%04x), val(0x%04x) \r\n",
		LOG_FLAG, __func__, reg_map_info_table[chip_type].chip_id_addr, val);
	return -1;
}

/*
 * Update regmap default register values based on machine id
 */
void sipa_regmap_defaults(
    struct regmap *regmap,
	unsigned int chip_type,
	unsigned int scene,
	unsigned int channel_num)
{
	int i = 0;
	int ret = -ENOENT;
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	const SIPA_PARAM_LIST *init_list = NULL;
	const SIPA_REG_COMMON *init_regs = NULL;

	pr_debug("[debug][%s] %s: running, chip_type = %u, channel_num = %u \r\n",
		LOG_FLAG, __func__, chip_type, channel_num);

	if (NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n",
			LOG_FLAG, __func__);
		return;
	}

	if (AUDIO_SCENE_NUM <= scene) {
		pr_err("[  err][%s] %s: scene = %u, AUDIO_SCENE_NUM = %u \r\n",
			LOG_FLAG, __func__, scene, AUDIO_SCENE_NUM);
		return;
	}

	if (SIPA_CHANNEL_NUM <= channel_num) {
		pr_err("[  err][%s] %s: channel_num = %u, SIPA_CHANNEL_NUM = %u \r\n",
			LOG_FLAG, __func__, channel_num, SIPA_CHANNEL_NUM);
		return;
	}

	if (0 != verify_chip_type(channel_num, chip_type))
		return;

	data = sipa_param_read_chip_cfg(channel_num, chip_type, &chip_cfg);
	if (NULL == data) {
		pr_err("[  err][%s] %s: fw unloaded \r\n", 
			LOG_FLAG, __func__);
		return;
	}

	init_list = &chip_cfg.init;
	if (init_list->node_size != sizeof(SIPA_REG_COMMON)) {
		pr_err("[  err][%s] %s: node_size(%u) != sizeof(SIPA_REG_COMMON)(%lu)", 
			LOG_FLAG, __func__, init_list->node_size, sizeof(SIPA_REG_COMMON));
		return;
	}

	init_regs = (SIPA_REG_COMMON *)(data + init_list->offset);
	for (i = 0; i < init_list->num; i++) {
		ret = regmap_write(regmap, init_regs[i].addr, init_regs[i].val[scene]);
		if (0 != ret) {
			pr_warn("[ warn][%s] %s: ret = %d, chip_type = %u, regmap = %p, addr = 0x%x\r\n",
				LOG_FLAG, __func__, ret, chip_type, regmap, init_regs[i].addr);
			break;
		}
	}
}

static int sipa_regmap_proc_1_reg(sipa_dev_t *si_pa, 
	SIPA_REG_PROC *reg, unsigned int val_width)
{
	unsigned int val = 0;
	unsigned int full_mask = 0;

	if (NULL == si_pa || NULL == reg)
		return -EINVAL;

	if (SIPA_MAX_REG_ADDR <= reg->addr)
		return -EINVAL;

	full_mask = (1 << val_width) - 1;

	switch (reg->action) {
	case SIPA_REG_READ:
		if (0 != regmap_read(si_pa->regmap, reg->addr, &val))
			return -EFAULT;

		reg->val[si_pa->scene] = (reg->val[si_pa->scene] & (~reg->mask))
				| (val & reg->mask);

		if (0 != reg->delay)
			udelay(reg->delay);
		break;
	case SIPA_REG_WRITE:
		if (full_mask != (reg->mask & full_mask)) {
			if (0 != regmap_read(si_pa->regmap, reg->addr, &val))
				return -EFAULT;

			val = (val  & (~reg->mask)) | (reg->val[si_pa->scene] & reg->mask);
		} else
			val = reg->val[si_pa->scene];

		if (0 != regmap_write(si_pa->regmap, reg->addr, val))
			return -EFAULT;

		if (0 != reg->delay)
			udelay(reg->delay);
		break;
	case SIPA_REG_CHECK:
		if (0 != regmap_read(si_pa->regmap, reg->addr, &val))
			return -EFAULT;

		if ((val & reg->mask) != (reg->val[si_pa->scene] & reg->mask))
			return -1;

		if (0 != reg->delay)
			udelay(reg->delay);
		break;
	case SIPA_REG_PAD:
		if (0 != reg->delay)
			udelay(reg->delay);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sipa_regmap_proc_1_reg_list(sipa_dev_t *si_pa, 
	const SIPA_PARAM_LIST *list, SIPA_REG_PROC *regs, uint32_t val_width)
{
	int i = 0;
 	int ret = -ENOENT;

	if (NULL == list || NULL == regs)
		return -EINVAL;

	if (list->node_size != sizeof(SIPA_REG_PROC)) {
		pr_err("[  err][%s] %s: node_size(%u) != sizeof(SIPA_REG_PROC)(%lu)", 
			LOG_FLAG, __func__, list->node_size, sizeof(SIPA_REG_PROC));
		return -ENOEXEC;
	}

	for (i = 0; i < list->num; i++) {
		ret = sipa_regmap_proc_1_reg(si_pa, &regs[i], val_width);
		if (0 != ret) {
			pr_err("[  err][%s] %s: ret = %d, chip_type = %u, regmap = %p, addr = 0x%x\r\n",
				LOG_FLAG, __func__, ret, si_pa->chip_type, si_pa->regmap, regs[i].addr);
			return ret;
		}
	}

	return 0;
}

bool sipa_regmap_set_chip_on(
	sipa_dev_t *si_pa)
{
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	const SIPA_PARAM_LIST *startup_list = NULL;
	SIPA_REG_PROC *startup_regs = NULL;

	if (NULL == si_pa)
		return false;

	if (0 != verify_chip_type(si_pa->channel_num, si_pa->chip_type))
		return false;

	data = sipa_param_read_chip_cfg(si_pa->channel_num, si_pa->chip_type, &chip_cfg);
	if (NULL == data) {
		pr_err("[  err][%s] %s: fw unloaded \r\n", 
			LOG_FLAG, __func__);
		return false;
	}

	startup_list = &chip_cfg.startup;
	startup_regs = (SIPA_REG_PROC *)(data + startup_list->offset);
	if (0 != sipa_regmap_proc_1_reg_list(si_pa, 
			startup_list, startup_regs, chip_cfg.reg_val_width))
		return false;

	return true;
}

bool sipa_regmap_set_chip_off(
	sipa_dev_t *si_pa)
{
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	const SIPA_PARAM_LIST *shutdown_list = NULL;
	SIPA_REG_PROC *shutdown_regs = NULL;

	if (NULL == si_pa)
		return false;

	if (0 != verify_chip_type(si_pa->channel_num, si_pa->chip_type))
		return false;

	data = sipa_param_read_chip_cfg(si_pa->channel_num, si_pa->chip_type, &chip_cfg);
	if (NULL == data) {
		pr_err("[  err][%s] %s: fw unloaded \r\n", 
			LOG_FLAG, __func__);
		return false;
	}

	shutdown_list = &chip_cfg.shutdown;
	shutdown_regs = (SIPA_REG_PROC *)(data + shutdown_list->offset);
	if (0 != sipa_regmap_proc_1_reg_list(si_pa, 
			shutdown_list, shutdown_regs, chip_cfg.reg_val_width))
		return false;

	return true;
}

bool sipa_regmap_get_chip_en(
	sipa_dev_t *si_pa)
{
	uint32_t val = 0;
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	const SIPA_REG *reg_en = NULL;

	if (NULL == si_pa)
		return false;

	if (0 != verify_chip_type(si_pa->channel_num, si_pa->chip_type))
		return false;

	data = sipa_param_read_chip_cfg(si_pa->channel_num, si_pa->chip_type, &chip_cfg);
	if (NULL == data)
		return false;

	reg_en = &chip_cfg.chip_en;
	if (SIPA_MAX_REG_ADDR <= reg_en->addr)
		return false;

	// can use SIPA_REG_CHECK action to do this
	if (0 != regmap_read(si_pa->regmap, reg_en->addr, &val))
		return false;
	
	if ((val & reg_en->mask) == (reg_en->val & reg_en->mask))
		return true;

	return false;
}

void sipa_regmap_set_pvdd_limit(
	struct regmap *regmap, uint32_t chip_type, uint32_t ch, unsigned int vol)
{
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	SIPA_FUNC0_TO_REG *pvdd_limit;
	int8_t cp_ovp = 0;

	if (NULL == regmap)
		return;

	if (3200000 > vol || 5000000 < vol) {
		pr_err("[  err][%s] %s: voltage = %u out of range !!! \r\n",
			LOG_FLAG, __func__, vol);
		return;
	}

	if (0 != verify_chip_type(ch, chip_type))
		return;

	data = sipa_param_read_chip_cfg(ch, chip_type, &chip_cfg);
	if (NULL == data) {
		pr_err("[  err][%s] %s: fw unloaded \r\n", 
			LOG_FLAG, __func__);
		return;
	}

	cp_ovp = (int8_t)((vol << 3) / 1000000 - 24 - 1);
	pvdd_limit = &chip_cfg.pvdd_limit;
	if (cp_ovp < pvdd_limit->valid_range.begin)
		cp_ovp = pvdd_limit->valid_range.begin;
	else if (cp_ovp > pvdd_limit->valid_range.end)
		cp_ovp = pvdd_limit->valid_range.end;

	if (0 != regmap_read(regmap, pvdd_limit->reg.addr, &pvdd_limit->reg.val))
		return;

	pvdd_limit->reg.val = (pvdd_limit->reg.val	& (~pvdd_limit->reg.mask)) | 
		((cp_ovp << pvdd_limit->bit_offset) & pvdd_limit->reg.mask);

	if (0 != regmap_write(regmap, pvdd_limit->reg.addr, pvdd_limit->reg.val))
		return;
}

static uint32_t sipa_regmap_bits_pack(sipa_dev_t *si_pa, 
	const SIPA_PARAM_LIST *list, SIPA_REG_PROC *regs, uint32_t val_width,
	uint8_t *buff, uint32_t buff_len)
{
	int ret;
	int i, j;
	uint32_t bits_total = 0;

	if (NULL == si_pa || NULL == list || NULL == regs || 
		NULL == buff || 0 == buff_len)
		return 0;

	if (list->node_size != sizeof(SIPA_REG_PROC)) {
		pr_err("[  err][%s] %s: node_size(%u) != sizeof(SIPA_REG_PROC)(%lu)",
			LOG_FLAG, __func__, list->node_size, sizeof(SIPA_REG_PROC));
		return 0;
	}

	//memset(buff, 0x00, buff_len);
	for (j = 0; j < list->num; j++) {
		ret = sipa_regmap_proc_1_reg(si_pa, &regs[j], val_width);
		if (0 != ret) {
			pr_err("[  err][%s] %s: ret = %d, chip_type = %u, regmap = %p, addr = 0x%x\r\n",
				LOG_FLAG, __func__, ret, si_pa->chip_type, si_pa->regmap, regs[j].addr);
			return 0;
		}

		for (i = val_width - 1; i >= 0; i--) {
			if ((regs[j].mask >> i) & 0x1) {
				buff[bits_total / 8] |= 
					((regs[j].val[si_pa->scene] >> i) & 0x1) << (7 - (bits_total % 8));
				bits_total++;

				if (bits_total > buff_len * 8) {
					pr_err("[  err][%s] %s: bits_total(%u bits) > buff_len (%u bytes) \r\n",
						LOG_FLAG, __func__, bits_total, buff_len);
					return 0;
				}
			}
		}
	}

	return bits_total;
}

void sipa_regmap_check_trimming(
	sipa_dev_t *si_pa)
{
	SIPA_CHIP_CFG chip_cfg;
	uint8_t *data;
	const SIPA_PARAM_LIST *reg_list = NULL;
	SIPA_REG_PROC *regs = NULL;
	uint32_t val_width = 0;
	uint32_t crc_width = 0;
	uint8_t trim[32] = {0}, crc_buff[4] = {0};
	uint32_t trim_bytes = 0;
	uint16_t crc = 0, crc_cal = 0;
	uint32_t ret = -1;

	if (NULL == si_pa)
		return;

	if (0 != verify_chip_type(si_pa->channel_num, si_pa->chip_type))
		return;

	data = sipa_param_read_chip_cfg(si_pa->channel_num, si_pa->chip_type, &chip_cfg);
	if (NULL == data) {
		pr_err("[  err][%s] %s: fw unloaded \r\n",
			LOG_FLAG, __func__);
		return;
	}

	/* wait reading trimming data to reg */
	mdelay(1);

	val_width = chip_cfg.reg_val_width;
	reg_list = &chip_cfg.trim_regs.efuse;
	if (!reg_list->num) {
		pr_debug("[debug][%s] no efuse return\r\n", __func__);
		return;
	}
		
	regs = (SIPA_REG_PROC *)(data + reg_list->offset);
	trim_bytes = sipa_regmap_bits_pack(
		si_pa, reg_list, regs, val_width, trim, sizeof(trim));
	if (0 >= trim_bytes) {
		pr_err("[  err][%s] %s: read trim regs failed! \r\n",
			LOG_FLAG, __func__);
		return;
	} else
		trim_bytes = trim_bytes / 8 + (trim_bytes % 8 ? 1 : 0);

	crc_width = chip_cfg.trim_regs.crc_width;
	if (sizeof(crc) * 8 < crc_width) {
		pr_err("[  err][%s] %s: 16 < crc_width(%u)",
			LOG_FLAG, __func__, crc_width);
		return;
	}

	reg_list = &chip_cfg.trim_regs.crc;
	regs = (SIPA_REG_PROC *)(data + reg_list->offset);

	ret = sipa_regmap_bits_pack(si_pa, reg_list, regs, val_width, crc_buff, sizeof(crc_buff));
	if (crc_width != ret) {
		pr_err("[  err][%s] %s: read trim crc regs failed! crc_width = %u ret = %u\r\n",
			LOG_FLAG, __func__, crc_width, ret);
		return;
	}

	switch (crc_width) {
	case 4:
		crc = crc_buff[0] >> 4;
		crc_cal = crc4_itu(trim, trim_bytes);
		break;
	case 8:
		crc = crc_buff[0];
		crc_cal = crc8_maxim(trim, trim_bytes);
		break;
	case 16:
		crc = (crc_buff[0] << 8) | crc_buff[1];
		crc_cal = crc16_maxim(trim, trim_bytes);
		break;
	default:
		pr_err("[  err][%s] %s: crc_width(%u) unsupported! \r\n",
			LOG_FLAG, __func__, crc_width);
		return;
	}

	if (crc_cal != crc) {
		pr_err("[  err][%s] %s: ch%d check trim crc failed! \r\n", 
			LOG_FLAG, __func__, si_pa->channel_num);

		reg_list = &chip_cfg.trim_regs.default_set;
		regs = (SIPA_REG_PROC *)(data + reg_list->offset);
		sipa_regmap_proc_1_reg_list(si_pa, reg_list, regs, val_width);

#ifdef SIA91XX_TYPE
		sipa_tuning_close_temp_f0_module(
			si_pa->timer_task_hdl,
			si_pa->dyn_ud_vdd_port,
			si_pa->channel_num);
#endif
	}
}
/********************************************************************
 * end - sia81xx reg map opt functions
 ********************************************************************/

