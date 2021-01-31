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

#include <linux/i2c.h>

#include "i2c_hal.h"
#include "chirp_hal.h"
#include "init_driver.h"
#include "../ch101_client.h"


unsigned long i2c_master_read_register(int bus_index, unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	int res = 0;
	struct ch101_client *data = get_chirp_data();
	const void *client = data->bus[bus_index].i2c_client;

//	printf("%s: Address: %02x, RegAddr: %02x, RegLen: %d\n",
//		__func__, (u16)Address, (u8)RegAddr, (u16)RegLen);

	res = data->cbk->read_reg((void *)client, (u16)Address, (u8)RegAddr,
		(u16)RegLen, (u8 *)RegValue);

	if (res)
		printf("%s: res: %d", __func__, res);

//	{
//	int i;
//	printf("Read Values: ");
//	for (i = 0; i < (RegLen < 3 ? RegLen : 3); i++)
//		printf(" %02x ", *(u8 *)(RegValue + i));
//	printf("\n");
//	}

	return res;
}

unsigned long i2c_master_write_register(int bus_index, unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	int res = 0;
	struct ch101_client *data = get_chirp_data();
	const void *client = data->bus[bus_index].i2c_client;

//	printf("%s: Address: %02x, RegAddr: %02x, RegLen: %d\n",
//		__func__, (u16)Address, (u8)RegAddr, (u16)RegLen);
//
//	{
//	int i;
//	printf("Write Values: ");
//	for (i = 0; i < (RegLen < 3 ? RegLen : 3); i++)
//		printf(" %02x ", *(u8 *)(RegValue + i));
//	printf("\n");
//	}

	res = data->cbk->write_reg((void *)client, (u16)Address, (u8)RegAddr,
		(u16)RegLen, (u8 *)RegValue);

	if (res)
		printf("%s: res: %d", __func__, res);

	return res;
}

unsigned long i2c_master_read_sync(int bus_index, unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	int res = 0;
	struct ch101_client *data = get_chirp_data();
	const void *client = data->bus[bus_index].i2c_client;

//	printf("%s: start\n", __func__);
//
//	printf("%s: Address: %02x, RegAddr: %02x, RegLen: %d\n",
//		__func__, (u16)Address, (u16)RegLen);

	res = data->cbk->read_sync((void *)client, (u16)Address,
		(u16)RegLen, (u8 *)RegValue);

	if (res)
		printf("%s: res: %d", __func__, res);

//	{
//	int i;
//	printf("Read Values: ");
//	for (i = 0; i < (RegLen < 3 ? RegLen : 3); i++)
//		printf(" %02x ", *(u8 *)(RegValue + i));
//	printf("\n");
//	}

	return (res == 0 ? RegLen : res);
}

unsigned long i2c_master_write_sync(int bus_index, unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	int res = 0;
	struct ch101_client *data = get_chirp_data();
	const void *client = data->bus[bus_index].i2c_client;

//	printf("%s: Address: %02x, RegLen: %d\n",
//		__func__, (u16)Address, (u16)RegLen);
//
//	{
//	int i;
//	printf("Write Values: ");
//	for (i = 0; i < (RegLen < 3 ? RegLen : 3); i++)
//		printf(" %02x ", *(u8 *)(RegValue + i));
//	printf("\n");
//	}

	res = data->cbk->write_sync((void *)client, (u16)Address,
		(u16)RegLen, (u8 *)RegValue);

	if (res)
		printf("%s: res: %d", __func__, res);

	return (res == 0 ? RegLen : res);
}

////////////////////////////////////////////////////////////////////////////////
unsigned long i2c_master_read_register0(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_read_register(0, Address, RegAddr, RegLen, RegValue);
}

unsigned long i2c_master_read_register1(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_read_register(1, Address, RegAddr, RegLen, RegValue);
}

unsigned long i2c_master_read_register2(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_read_register(2, Address, RegAddr, RegLen, RegValue);
}

////////////////////////////////////////////////////////////////////////////////
unsigned long i2c_master_write_register0(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_write_register(0, Address, RegAddr, RegLen, RegValue);
}

unsigned long i2c_master_write_register1(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_write_register(1, Address, RegAddr, RegLen, RegValue);
}

unsigned long i2c_master_write_register2(unsigned char Address,
	unsigned char RegAddr, unsigned short RegLen,
	unsigned char *RegValue)
{
	return i2c_master_write_register(2, Address, RegAddr, RegLen, RegValue);
}

////////////////////////////////////////////////////////////////////////////////
unsigned long i2c_master_read_register0_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(0, Address, RegLen, RegValue);
}

unsigned long i2c_master_read_register1_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(1, Address, RegLen, RegValue);
}

unsigned long i2c_master_read_register2_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(2, Address, RegLen, RegValue);
}

////////////////////////////////////////////////////////////////////////////////
unsigned long i2c_master_read_register0_nb(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(0, Address, RegLen, RegValue);
}

unsigned long i2c_master_read_register1_nb(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(1, Address, RegLen, RegValue);
}

unsigned long i2c_master_read_register2_nb(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_read_sync(2, Address, RegLen, RegValue);
}

////////////////////////////////////////////////////////////////////////////////
unsigned long i2c_master_write_register0_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_write_sync(0, Address, RegLen, RegValue);
}

unsigned long i2c_master_write_register1_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_write_sync(1, Address, RegLen, RegValue);
}

unsigned long i2c_master_write_register2_sync(unsigned char Address,
	unsigned short RegLen, unsigned char *RegValue)
{
	return i2c_master_write_sync(2, Address, RegLen, RegValue);
}

void i2c_master_initialize0(void)
{
}

void i2c_master_initialize1(void)
{
}

void i2c_master_initialize2(void)
{
}

void i2c_master_init(void)
{
	i2c_master_initialize0();
	i2c_master_initialize1();
	i2c_master_initialize2();
}

void ext_int_init(void)
{
}

