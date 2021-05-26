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
#define LOG_FLAG	"sia81xx_regmap"

 
#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"

#include "sia8101_regs.h"
#include "sia8108_regs.h"
#include "sia8109_regs.h"


struct reg_map_info {
	const struct regmap_config *config;
	const struct sia81xx_reg_default_val *reg_default;
	const struct sia81xx_opt_if *opt;
};

static const struct reg_map_info reg_map_info_table[] = {
	[CHIP_TYPE_SIA8101] = {
		.config = &sia8101_regmap_config,
		.reg_default = &sia8101_reg_default_val,
		.opt = &sia8101_opt_if
	},
	[CHIP_TYPE_SIA8108] = {
		.config = &sia8108_regmap_config,
		.reg_default = &sia8108_reg_default_val,
		.opt = &sia8108_opt_if
	},
	[CHIP_TYPE_SIA8109] = {
		.config = &sia8109_regmap_config,
		.reg_default = &sia8109_reg_default_val,
		.opt = &sia8109_opt_if
	}
};

static int verify_chip_type(
	unsigned int type)
{
	if(type >= ARRAY_SIZE(reg_map_info_table)) {
		pr_err("[  err][%s] %s: chip_type = %u, "
			"ARRAY_SIZE(reg_map_info) = %lu \r\n", 
			LOG_FLAG, __func__, type, ARRAY_SIZE(reg_map_info_table));
		return -ENODEV;
	}

	return 0;
}

// when read one byte, a int(4 byte) buffer is needed.
// or it will cause __stack_chk_fail issue.
int sia81xx_regmap_read(
	struct regmap *regmap, 
	unsigned int start_reg,
	unsigned int reg_num,
	void *buf)
{
	if(NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(NULL == buf) {
		pr_err("[  err][%s] %s: NULL == buf \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	if(1 == reg_num) {
		unsigned int *val = (unsigned int *)buf;
		return regmap_read(regmap, start_reg, val);
	}

	return regmap_bulk_read(regmap, start_reg, buf, reg_num);
}

int sia81xx_regmap_write(
	struct regmap *regmap, 
	unsigned int start_reg,
	unsigned int reg_num,
	const char *buf)
{
	if(NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(NULL == buf) {
		pr_err("[  err][%s] %s: NULL == buf \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	if(1 == reg_num) {
		unsigned int val = (unsigned int)(*buf);
		return regmap_write(regmap, start_reg, val);
	}

	return regmap_bulk_write(regmap, start_reg, buf, reg_num);
}


/*
 * Update regmap default register values based on machine id
 */
void sia81xx_regmap_defaults(
	struct regmap *regmap, 
	unsigned int chip_type, 
	unsigned int scene,
	unsigned int channel_num)
{
	int ret = -ENOENT;
	int i = 0;
	
	pr_debug("[debug][%s] %s: running, chip_type = %u, channel_num = %u \r\n", 
		LOG_FLAG, __func__, chip_type, channel_num);

	if(NULL == regmap) {
		pr_warn("[ warn][%s] %s: NULL == regmap \r\n", 
			LOG_FLAG, __func__);
		return ;
	}

	if(AUDIO_SCENE_NUM <= scene) {
		pr_err("[  err][%s] %s: scene = %u, AUDIO_SCENE_NUM = %u \r\n", 
			LOG_FLAG, __func__, scene, AUDIO_SCENE_NUM);
		return ;
	}

	if(SIA81XX_CHANNEL_NUM <= channel_num) {
		pr_err("[  err][%s] %s: channel_num = %u, SIA81XX_CHANNEL_NUM = %u \r\n", 
			LOG_FLAG, __func__, channel_num, SIA81XX_CHANNEL_NUM);
		return ;
	}

	for(i = 0; i < ARRAY_SIZE(reg_map_info_table); i++) {
		
		if(chip_type == reg_map_info_table[i].reg_default->chip_type) {			
			ret = sia81xx_regmap_write(
				regmap, 
				reg_map_info_table[i].reg_default->offset,
				reg_map_info_table[i].reg_default->reg_defaults[scene].num,
				reg_map_info_table[i].reg_default->reg_defaults[scene].vals + 
					(channel_num * reg_map_info_table[i].reg_default->reg_defaults[scene].num)
				);
			
			break;
		}
	}
	
	if(0 != ret) {
		pr_err("[  err][%s] %s: ret = %d, chip_type = %u, regmap = %p \r\n", 
			LOG_FLAG, __func__, ret, chip_type, regmap);
	}
}

struct regmap *sia81xx_regmap_init(
	struct i2c_client *client, 
	unsigned int chip_type) 
{
	if(NULL == client)
		return NULL;
	
	if(0 != verify_chip_type(chip_type))
		return NULL;

	return regmap_init_i2c(client, reg_map_info_table[chip_type].config);
}

void sia81xx_regmap_remove(
	struct regmap *regmap) 
{
	if(!IS_ERR(regmap))
		regmap_exit(regmap);
}



/********************************************************************
 * sia81xx reg map opt functions
 ********************************************************************/
int sia81xx_regmap_check_chip_id(
	struct regmap *regmap, 
	unsigned int chip_type) 
{
	if(NULL == regmap)
		return -EPERM;
	
	if(0 != verify_chip_type(chip_type))
		return -EPERM;

	if(NULL != reg_map_info_table[chip_type].opt->check_chip_id) {
		return reg_map_info_table[chip_type].opt->check_chip_id(regmap);
	}

	/* if no need to check chip id then return "pass" */
	return 0;
}

void sia81xx_regmap_set_xfilter(
	struct regmap *regmap, 
	unsigned int chip_type, 
	unsigned int val)
{
	if(NULL == regmap)
		return ;
	
	if(0 != verify_chip_type(chip_type))
		return ;

	if(NULL != reg_map_info_table[chip_type].opt->set_xfilter) {
		reg_map_info_table[chip_type].opt->set_xfilter(regmap, val);
	}

	return ;
}

/********************************************************************
 * end - sia81xx reg map opt functions
 ********************************************************************/





