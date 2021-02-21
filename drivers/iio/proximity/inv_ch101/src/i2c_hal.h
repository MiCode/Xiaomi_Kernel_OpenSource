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

#ifndef I2CHAL_H
#define I2CHAL_H

unsigned long i2c_master_read_register0(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);
unsigned long i2c_master_read_register1(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);
unsigned long i2c_master_read_register2(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);

unsigned long i2c_master_read_register0_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_read_register1_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_read_register2_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);

unsigned long i2c_master_read_register0_nb(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_read_register1_nb(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_read_register2_nb(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);

unsigned long i2c_master_write_register0(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);
unsigned long i2c_master_write_register1(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);
unsigned long i2c_master_write_register2(unsigned char Address,
	unsigned char RegisterAddr, unsigned short RegisterLen,
	unsigned char *RegisterValue);

unsigned long i2c_master_write_register0_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_write_register1_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);
unsigned long i2c_master_write_register2_sync(unsigned char Address,
	unsigned short RegisterLen, unsigned char *RegisterValue);

void i2c_master_initialize0(void);
void i2c_master_initialize1(void);
void i2c_master_initialize2(void);
void i2c_master_init(void);

void ext_int_init(void);

#endif /* I2CHAL_H */
