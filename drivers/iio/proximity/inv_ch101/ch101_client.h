/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DRIVERS_IIO_PROXIMITY_INV_CH101_CH101_CLIENT_H_
#define DRIVERS_IIO_PROXIMITY_INV_CH101_CH101_CLIENT_H_

#include <linux/regmap.h>
#include "src/chbsp_init.h"


#define TAG "ch101: "

#define MAX_SAMPLES     450
#define MAX_DEVICES     3
#define MAX_BUSES	2
#define MAX_DEV_BUSES	(MAX_DEVICES * MAX_BUSES)

typedef u32 ioport_pin_t;

enum ioport_direction {
	IOPORT_DIR_INPUT, /*!< IOPORT input direction */
	IOPORT_DIR_OUTPUT, /*!< IOPORT output direction */
};

enum ioport_value {
	IOPORT_PIN_LEVEL_LOW, /*!< IOPORT pin value low */
	IOPORT_PIN_LEVEL_HIGH, /*!< IOPORT pin value high */
};

struct ch101_iq_data {
	s16 I;
	s16 Q;
};

struct ch101_buffer {
	u16 distance[MAX_DEV_BUSES];
	u16 amplitude[MAX_DEV_BUSES];
	u16 nb_samples[MAX_DEV_BUSES];
	struct ch101_iq_data iq_data[MAX_DEV_BUSES][MAX_SAMPLES];
	u8 mode[MAX_DEV_BUSES];
};

struct ch101_client;
struct ch101_callbacks {
	int (*write_reg)(void *cl, u16 i2c_addr, u8 reg, u16 length, u8 *data);
	int (*read_reg)(void *cl, u16 i2c_addr, u8 reg, u16 length, u8 *data);
	int (*write_sync)(void *cl, u16 i2c_addr, u16 length, u8 *data);
	int (*read_sync)(void *cl, u16 i2c_addr, u16 length, u8 *data);
	void (*set_pin_level)(ioport_pin_t pin, int value);
	void (*set_pin_dir)(ioport_pin_t pin, enum ioport_direction dir);
	void (*data_complete)(struct ch101_client *data);
	int (*setup_int_gpio)(struct ch101_client *data, uint32_t pin);
	int (*free_int_gpio)(struct ch101_client *data, uint32_t pin);
};

struct ch101_i2c_bus {
	struct i2c_client *i2c_client;
	struct gpio_desc *gpiod_int[MAX_DEVICES];
	u32 gpio_exp_prog_pin[MAX_DEVICES];
};

struct ch101_gpios {
	struct gpio_desc *gpiod_rst;
	struct gpio_desc *gpiod_rst_pulse;
};

struct ch101_client {
	struct ch101_i2c_bus bus[MAX_BUSES];
	struct ch101_gpios gpios;
	struct ch101_callbacks *cbk;
};

int ch101_core_probe(struct i2c_client *client, struct regmap *regmap,
			struct ch101_callbacks *cbk, const char *name);
int ch101_core_remove(struct i2c_client *client);

#endif /* DRIVERS_IIO_PROXIMITY_INV_CH101_CH101_CLIENT_H_ */
