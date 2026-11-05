/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic adapter driver file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/i2c.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/errno.h>
#include "haptic_mid.h"
#include "sih688x_reg.h"
#include "haptic_regmap.h"
#include "haptic_misc.h"

typedef struct sih_match_funclist {
	haptic_func_t *haptic_func;
	const struct regmap_config *haptic_regmap_config;
} sih_match_funclist_t;

sih_match_funclist_t sih_match_if[] = {
	{
		.haptic_func = &sih_688x_func_list,
		.haptic_regmap_config = &sih688x_regmap_config,
	}
};

/*********************************************************
 *
 * I2C Read/Write
 *
 *********************************************************/
int i2c_read_bytes(sih_haptic_t *sih_haptic, uint8_t reg_addr,
	uint8_t *buf, uint32_t len)
{
	int ret = -1;

	ret = i2c_master_send(sih_haptic->i2c, &reg_addr, SIH_I2C_OPERA_BYTE_ONE);
	if (ret < 0) {
		hp_err("%s:couldn't send addr:0x%02x, ret=%d\n", __func__,
			reg_addr, ret);
		return ret;
	}
	ret = i2c_master_recv(sih_haptic->i2c, buf, len);
	if (ret != len) {
		hp_err("%s:couldn't read data, ret=%d\n", __func__, ret);
		return ret;
	}
	return ret;
}

int i2c_write_bytes(sih_haptic_t *sih_haptic, uint8_t reg_addr,
	uint8_t *buf, uint32_t len)
{
	uint8_t *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(sih_haptic->i2c, data, len + 1);
	if (ret < 0)
		hp_err("%s:i2c master send 0x%02x err\n", __func__, reg_addr);
	kfree(data);
	return ret;
}

int i2c_write_bits(sih_haptic_t *sih_haptic, uint8_t reg_addr,
	uint32_t mask, uint8_t reg_data)
{
	uint8_t reg_val = 0;
	int ret = -1;

	ret = i2c_read_bytes(sih_haptic, reg_addr, &reg_val,
		SIH_I2C_OPERA_BYTE_ONE);
	if (ret < 0) {
		hp_err("%s:i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = i2c_write_bytes(sih_haptic, reg_addr, &reg_val,
		SIH_I2C_OPERA_BYTE_ONE);
	if (ret < 0)
		hp_err("%s:i2c write error, ret=%d\n", __func__, ret);
	return ret;
}

int sih_register_func(sih_haptic_t *sih_haptic)
{
	int i;
	int ret = -1;
	int array_len = 0;

	array_len = (int)(sizeof(sih_match_if) / sizeof(sih_match_funclist_t));

	sih_haptic->stream_func = &stream_play_func;

	for (i = 0; i < array_len; ++i) {
		ret = sih_match_if[i].haptic_func->probe(sih_haptic);
		if (!ret) {
			sih_haptic->hp_func = sih_match_if[i].haptic_func;
			sih_haptic->regmapp.config = sih_match_if[i].haptic_regmap_config;
			hp_info("%s:match sequence number is %d\n", __func__, i);
			return ret;
		}
	}

	return ret;
}

uint8_t crc4_itu(uint8_t *data, uint8_t length)
{
	uint8_t i;
	uint8_t crc = 0;

	while (length--) {
		crc ^= *data++;
		for (i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x0C;
			else
				crc = (crc >> 1);
		}
	}
	return crc;
}
