/* Quanta EC driver for the Winbond Embedded Controller
 *
 * Copyright (C) 2009 Quanta Computer Inc.
 * 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef WPCE775X_DRV_H
#define WPCE775X_DRV_H

#include <linux/i2c.h>

struct i2c_client *wpce_get_i2c_client(void);
int wpce_smbus_write_word_data(u8 command, u16 value);
struct i2c_client *wpce_get_i2c_client(void);
void wpce_poweroff(void);
void wpce_restart(void);
int wpce_i2c_transfer(struct i2c_msg *msg);
int wpce_smbus_write_word_data(u8 command, u16 value);
int wpce_smbus_write_byte_data(u8 command, u8 value);

#endif
