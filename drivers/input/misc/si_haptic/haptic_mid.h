/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic adapter header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _HAPTIC_MID_H_
#define _HAPTIC_MID_H_

#include <linux/kernel.h>
#include <linux/cdev.h>
#include "haptic.h"

#define hp_err(format, ...) \
	pr_err("[sih_haptic]" format, ##__VA_ARGS__)

#define hp_info(format, ...) \
	pr_info("[sih_haptic]" format, ##__VA_ARGS__)

#define hp_dbg(format, ...) \
	pr_debug("[sih_haptic]" format, ##__VA_ARGS__)

#define null_pointer_err_check(a) \
	do { \
		if (!(a)) \
			return -1; \
	} while (0)

#define SIH_I2C_OPERA_BYTE_ONE                      1
#define SIH_I2C_OPERA_BYTE_TWO                      2
#define SIH_I2C_OPERA_BYTE_THREE                    3
#define SIH_I2C_OPERA_BYTE_FOUR                     4
#define SIH_I2C_OPERA_BYTE_FIVE                     5
#define SIH_I2C_OPERA_BYTE_SIX                      6
#define SIH_I2C_OPERA_BYTE_SEVEN                    7
#define SIH_I2C_OPERA_BYTE_EIGHT                    8

#define SIH_PRIORITY_ACQUIRE_DTS

#define SIH_HAPTIC_COMPAT_688X						"silicon,sih_haptic_688X"
#define SIH_HAPTIC_NAME_688X						"sih_haptic_688X"
#define SIH_HAPTIC_NAME								"sih_vibrator"

extern haptic_func_t sih_688x_func_list;
extern const struct regmap_config sih688x_regmap_config;
extern haptic_stream_func_t stream_play_func;

/*********************************************************
 *
 * Function Call
 *
 *********************************************************/
int i2c_read_bytes(sih_haptic_t *, uint8_t, uint8_t *, uint32_t);
int i2c_write_bytes(sih_haptic_t *, uint8_t, uint8_t *, uint32_t);
int i2c_write_bits(sih_haptic_t *, uint8_t, uint32_t, uint8_t);
int sih_register_func(sih_haptic_t *);
uint8_t crc4_itu(uint8_t *data, uint8_t length);
#endif

