// SPDX-License-Identifier: GPL-2.0
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

#include <linux/gpio/driver.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/export.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/time.h>
#include <asm/div64.h>

#include "chirp_hal.h"
#include "chbsp_init.h"

static struct ch101_client *_ch101_client;


void set_chirp_gpios(struct ch101_client *client)
{
	_ch101_client = client;
	printf("%s: data: %p\n", __func__, _ch101_client);
}

struct ch101_client *get_chirp_gpios(void)
{
	return _ch101_client;
}

struct gpio_desc *os_get_pin_desc(ioport_pin_t pin)
{
	if (!_ch101_client)
		return 0;
	if (pin == CHIRP_RST)
		return _ch101_client->gpios.gpiod_rst;
	if (pin == CHIRP_RST_PS)
		return _ch101_client->gpios.gpiod_rst_pulse;
	if (pin == CHIRP0_INT_0)
		return _ch101_client->bus[0].gpiod_int[0];
	if (pin == CHIRP0_INT_1)
		return _ch101_client->bus[0].gpiod_int[1];
	if (pin == CHIRP0_INT_2)
		return _ch101_client->bus[0].gpiod_int[2];
	if (pin == CHIRP1_INT_0)
		return _ch101_client->bus[1].gpiod_int[0];
	if (pin == CHIRP1_INT_1)
		return _ch101_client->bus[1].gpiod_int[1];
	if (pin == CHIRP1_INT_2)
		return _ch101_client->bus[1].gpiod_int[2];
	if (pin >= CHIRP0_PROG_0 && pin <= CHIRP2_PROG_2)
		return 0;

	printf("%s: pin: %d undefined\n", __func__, (u32)pin);

	return 0;
}

u32 os_get_pin_prog(ioport_pin_t pin)
{
	if (!_ch101_client)
		return 0;
	if (pin == CHIRP0_PROG_0)
		return _ch101_client->bus[0].gpio_exp_prog_pin[0];
	if (pin == CHIRP0_PROG_1)
		return _ch101_client->bus[0].gpio_exp_prog_pin[1];
	if (pin == CHIRP0_PROG_2)
		return _ch101_client->bus[0].gpio_exp_prog_pin[2];
	if (pin == CHIRP1_PROG_0)
		return _ch101_client->bus[1].gpio_exp_prog_pin[0];
	if (pin == CHIRP1_PROG_1)
		return _ch101_client->bus[1].gpio_exp_prog_pin[1];
	if (pin == CHIRP1_PROG_2)
		return _ch101_client->bus[1].gpio_exp_prog_pin[2];

	printf("%s: pin: %d undefined\n", __func__, (u32)pin);

	return 0;
}

void prog_set_dir(ioport_pin_t pin, enum ioport_direction dir)
{
	struct ch101_client *data = get_chirp_data();

	if (data && data->cbk->set_pin_dir)
		data->cbk->set_pin_dir(pin, dir);
}

void prog_set_level(ioport_pin_t pin, bool level)
{
	struct ch101_client *data = get_chirp_data();

	if (data && data->cbk->set_pin_level)
		data->cbk->set_pin_level(pin, level);
}

void ioport_set_pin_dir(ioport_pin_t pin, enum ioport_direction dir)
{
	if (pin >= CHIRP0_PROG_0 && pin <= CHIRP2_PROG_2) {
		u32 prog_pin;

		prog_pin = os_get_pin_prog(pin);
//		printf("%s: prog_pin: %d dir: %d\n",
//			__func__, prog_pin, dir);
		prog_set_dir(prog_pin, dir);
	} else {
		struct gpio_desc *desc;

		desc = os_get_pin_desc(pin);
//		printf("%s: pin: %d(%p) dir: %d\n",
//			__func__, pin, desc, dir);
		if (!desc) {
			printf("%s: pin: %d(%p) NULL\n",
				__func__, pin, desc);
			return;
		}
		if (dir == IOPORT_DIR_INPUT)
			gpiod_direction_input(desc);
		else
			gpiod_direction_output(desc, 0);
	}
}

void ioport_set_pin_level(ioport_pin_t pin, bool level)
{
	if (pin >= CHIRP0_PROG_0 && pin <= CHIRP2_PROG_2) {
		u32 prog_pin;

		prog_pin = os_get_pin_prog(pin);
//		printf("%s: prog_pin: %d level: %d\n",
//			__func__, prog_pin, level);
		prog_set_level(prog_pin, level);
	} else {
		struct gpio_desc *desc;

		desc = os_get_pin_desc(pin);
//		printf("%s: pin: %d(%p) level: %d\n",
//			__func__, pin, desc, level);
		if (!desc) {
			printf("%s: pin: %d(%p) NULL\n",
				__func__, pin, desc);
			return;
		}
		gpiod_set_value_cansleep(desc, level);
	}

//	ioport_get_pin_level(pin);
}

enum ioport_direction ioport_get_pin_dir(ioport_pin_t pin)
{
	enum ioport_direction dir = IOPORT_DIR_INPUT;

	if (pin >= CHIRP0_PROG_0 && pin <= CHIRP2_PROG_2) {
		//u32 prog_pin;
		//prog_pin = os_get_pin_prog(pin);
		//dir = prog_get_dir(prog_pin);
	} else {
		struct gpio_desc *desc;

		desc = os_get_pin_desc(pin);
		if (!desc) {
			printf("%s: pin: %d(%p) NULL\n",
				__func__, pin, desc);
			return dir;
		}
		dir = gpiod_get_direction(desc);
//		printf("%s: pin: %d(%p) dir: %d\n", __func__, pin, desc, dir);
	}

	return dir;
}

bool ioport_get_pin_level(ioport_pin_t pin)
{
	bool level = false;

	if (pin >= CHIRP0_PROG_0 && pin <= CHIRP2_PROG_2) {
		//u32 prog_pin;
		//prog_pin = os_get_pin_prog(pin);
		//level = prog_set_level(prog_pin, level);
	} else {
		struct gpio_desc *desc;

		desc = os_get_pin_desc(pin);
		if (!desc) {
			printf("%s: pin: %d(%p) NULL\n",
				__func__, pin, desc);
			return level;
		}
		level = gpiod_get_value_cansleep(desc);
	}

	return level;
}

int32_t os_enable_interrupt(const uint32_t pin)
{
	struct gpio_desc *desc = os_get_pin_desc(pin);
	unsigned int irq = gpiod_to_irq(desc);
	struct ch101_client *data = get_chirp_data();

	printf("%s: irq: %d\n", __func__, irq);

	if (data && data->cbk->setup_int_gpio)
		data->cbk->setup_int_gpio(data, pin);

	return 0;
}

int32_t os_disable_interrupt(const uint32_t pin)
{
	struct gpio_desc *desc = os_get_pin_desc(pin);
	unsigned int irq = gpiod_to_irq(desc);
	struct ch101_client *data = get_chirp_data();

	printf("%s: irq: %d\n", __func__, irq);

	if (data && data->cbk->free_int_gpio)
		data->cbk->free_int_gpio(data, pin);

	return 0;
}

void os_clear_interrupt(const uint32_t pin)
{
}

// delay in microseconds
void os_delay_us(const uint16_t us)
{
	usleep_range(us, us + 1);
}

// delay in miliseconds
void os_delay_ms(const uint16_t ms)
{
	msleep(ms);
}

u64 os_timestamp_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	return timespec_to_ns(&ts);
}

u64 os_timestamp_ms(void)
{
	u64 ns = os_timestamp_ns();
	u32 div = 1e6;

	return div64_ul(ns, div);
}

void os_print_str(char *str)
{
	printf("%s", str);
}
