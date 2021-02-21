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

#ifndef CHIRPHAL_H
#define CHIRPHAL_H

#include "system.h"
#include "init_driver.h"
#include "chirp_hal.h"
#include "../ch101_client.h"


void ioport_set_pin_dir(ioport_pin_t pin, enum ioport_direction dir);
void ioport_set_pin_level(ioport_pin_t pin, bool level);

enum ioport_direction ioport_get_pin_dir(ioport_pin_t pin);
bool ioport_get_pin_level(ioport_pin_t pin);

int32_t os_enable_interrupt(const u32 pin);
int32_t os_disable_interrupt(const u32 pin);
void os_clear_interrupt(const u32 pin);

void os_delay_us(const u16 us);
void os_delay_ms(const u16 ms);

uint64_t os_timestamp_ms(void);

void set_chirp_gpios(struct ch101_client *client);

void os_print_str(char *str);

#endif /* CHIRPHAL_H */
