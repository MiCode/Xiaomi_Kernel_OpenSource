/*
 * aw_spin.c spin_module
 *
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 *
 *  Author: Yuhui Zhao <zhaoyuhui@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/input.h>

#include "aw882xx_log.h"
#include "aw882xx.h"
#include "aw882xx_spin.h"

static unsigned int g_spin_mode = 0;
static unsigned int g_spin_value = 0;

static DEFINE_MUTEX(g_spin_lock);

int aw_dev_set_channal_mode(struct aw_device *aw_dev,
			struct aw_spin_desc spin_desc, uint32_t spin_val)
{
	int ret;
	struct aw_reg_ch *rx_desc = &spin_desc.rx_desc;
	struct aw_reg_ch *tx_desc = &spin_desc.tx_desc;

	ret = aw_dev->ops.aw_i2c_write_bits(aw_dev, rx_desc->reg, rx_desc->mask,
					spin_desc.spin_table[spin_val].rx_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set rx failed");
		return ret;
	}

	ret = aw_dev->ops.aw_i2c_write_bits(aw_dev, tx_desc->reg, tx_desc->mask,
					spin_desc.spin_table[spin_val].tx_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set tx failed");
		return ret;
	}

	aw_dev_dbg(aw_dev->dev, "set channel mode done!");
	return 0;
}

static int aw_reg_write_spin(struct aw_device *aw_dev,
				uint32_t spin_val, bool mixer_en)
{
	int ret;
	struct aw_device *local_dev = NULL;
	struct list_head *pos = NULL;
	struct list_head *dev_list = NULL;

	if ((g_spin_mode == AW_REG_MIXER_SPIN_MODE) && (mixer_en)) {
		ret = aw882xx_dsp_set_mixer_en(aw_dev, AW_AUDIO_MIX_ENABLE);
		if (ret)
			return ret;

		usleep_range(AW_100000_US, AW_100000_US + 10);
	}

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_dev_set_channal_mode(local_dev, local_dev->spin_desc, spin_val);
		if (ret < 0) {
			aw_dev_err(local_dev->dev, "set channal mode failed");
			return ret;
		}
	}

	if ((g_spin_mode == AW_REG_MIXER_SPIN_MODE) && (mixer_en)) {
		ret = aw882xx_dsp_set_mixer_en(aw_dev, AW_AUDIO_MIX_DSIABLE);
		if (ret)
			return ret;
	}

	return 0;
}

int aw882xx_spin_set_record_val(struct aw_device *aw_dev)
{
	int ret = -1;

	mutex_lock(&g_spin_lock);
	if (g_spin_mode == AW_ADSP_SPIN_MODE) {
		ret = aw882xx_dsp_write_spin(g_spin_value);
		if (ret) {
			mutex_unlock(&g_spin_lock);
			return ret;
		}
	} else if ((g_spin_mode == AW_REG_SPIN_MODE) ||
				(g_spin_mode == AW_REG_MIXER_SPIN_MODE)) {
		ret = aw_dev_set_channal_mode(aw_dev, aw_dev->spin_desc, g_spin_value);
		if (ret) {
			mutex_unlock(&g_spin_lock);
			return ret;
		}
	} else {
		aw_dev_info(aw_dev->dev, "do nothing");
	}
	mutex_unlock(&g_spin_lock);

	aw_dev_info(aw_dev->dev, "set record spin val done");
	return 0;
}

int aw882xx_spin_value_get(struct aw_device *aw_dev,
				uint32_t *spin_val, bool pstream)
{
	int ret = 0;

	if (((g_spin_mode == AW_REG_SPIN_MODE) ||
			(g_spin_mode == AW_REG_MIXER_SPIN_MODE)) || (!pstream))
		*spin_val = g_spin_value;
	else if (g_spin_mode == AW_ADSP_SPIN_MODE)
		ret = aw882xx_dsp_read_spin(spin_val);

	return ret;
}

int aw882xx_spin_value_set(struct aw_device *aw_dev, uint32_t spin_val, bool pstream)
{
	int ret = 0;

	mutex_lock(&g_spin_lock);
	if (pstream) {
		if ((g_spin_mode == AW_REG_SPIN_MODE) ||
					(g_spin_mode == AW_REG_MIXER_SPIN_MODE))
			ret = aw_reg_write_spin(aw_dev, spin_val, true);
		else if (g_spin_mode == AW_ADSP_SPIN_MODE)
			ret = aw882xx_dsp_write_spin(spin_val);
		else
			aw_dev_info(aw_dev->dev, "can't set spin value");
	} else {
		if ((g_spin_mode == AW_REG_SPIN_MODE) ||
					(g_spin_mode == AW_REG_MIXER_SPIN_MODE))
			ret = aw_reg_write_spin(aw_dev, spin_val, false);
		else
			aw_dev_info(aw_dev->dev, "stream no start only record spin angle");
	}
	g_spin_value = spin_val;
	mutex_unlock(&g_spin_lock);

	return ret;
}

static int aw_parse_spin_table_dt(struct aw_device *aw_dev)
{
	int ret = -1;
	const char *str_data = NULL;
	char spin_table_str[AW_SPIN_MAX] = { 0 };
	struct aw_spin_desc *spin_desc = &aw_dev->spin_desc;
	int i;

	ret = of_property_read_string(aw_dev->dev->of_node, "spin-data", &str_data);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get spin_data failed, close spin function");
		return ret;
	}

	ret = sscanf(str_data, "%c %c %c %c",
				&spin_table_str[AW_SPIN_0], &spin_table_str[AW_SPIN_90],
				&spin_table_str[AW_SPIN_180], &spin_table_str[AW_SPIN_270]);
	if  (ret != AW_SPIN_MAX) {
		aw_dev_err(aw_dev->dev, "unsupported str:%s, close spin function", str_data);
		return -EINVAL;
	}

	for (i = 0; i < AW_SPIN_MAX; i++) {
		if (spin_table_str[i] == 'l' || spin_table_str[i] == 'L') {
			spin_desc->spin_table[i].rx_val = spin_desc->rx_desc.left_val;
			spin_desc->spin_table[i].tx_val = spin_desc->tx_desc.left_val;
		} else if (spin_table_str[i] == 'r' || spin_table_str[i] == 'R') {
			spin_desc->spin_table[i].rx_val = spin_desc->rx_desc.right_val;
			spin_desc->spin_table[i].tx_val = spin_desc->tx_desc.right_val;
		} else {
			aw_dev_err(aw_dev->dev, "unsupported str:%s, close spin function", str_data);
			return -EINVAL;
		}
	}

	return 0;
}

static int aw_parse_spin_dts(struct aw_device *aw_dev)
{
	int ret = -1;
	const char *spin_str = NULL;

	ret = of_property_read_string(aw_dev->dev->of_node, "spin-mode", &spin_str);
	if (ret < 0) {
		g_spin_mode = AW_SPIN_OFF_MODE;
		aw_dev_info(aw_dev->dev, "spin-mode get failed, spin switch off, spin_mode:%d", g_spin_mode);
		return 0;
	}

	if (!strcmp(spin_str, "dsp_spin"))
		g_spin_mode = AW_ADSP_SPIN_MODE;
	else if (!strcmp(spin_str, "reg_spin"))
		g_spin_mode = AW_REG_SPIN_MODE;
	else if (!strcmp(spin_str, "reg_mixer_spin"))
		g_spin_mode = AW_REG_MIXER_SPIN_MODE;
	else
		g_spin_mode = AW_SPIN_OFF_MODE;

	if ((g_spin_mode == AW_REG_SPIN_MODE) ||
				(g_spin_mode == AW_REG_MIXER_SPIN_MODE)) {
		ret = aw_parse_spin_table_dt(aw_dev);
		if (ret < 0)
			return ret;
	}

	aw_dev_info(aw_dev->dev, "spin mode is %d",  g_spin_mode);
	return 0;
}

int aw882xx_spin_init(struct aw_spin_desc *spin_desc)
{
	int ret = 0;
	struct aw_device *aw_dev =
		container_of(spin_desc, struct aw_device, spin_desc);

	ret = aw_parse_spin_dts(aw_dev);

	if (g_spin_mode == AW_SPIN_OFF_MODE)
		spin_desc->aw_spin_kcontrol_st = AW_SPIN_KCONTROL_DISABLE;
	else
		spin_desc->aw_spin_kcontrol_st = AW_SPIN_KCONTROL_ENABLE;

	aw_dev_info(aw_dev->dev, "aw_spin_kcontrol_st:%d", spin_desc->aw_spin_kcontrol_st);

	return ret;
}
