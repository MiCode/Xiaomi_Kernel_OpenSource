/*
 * Copyright (C) 2016 Google, Inc.
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

#ifndef _NANOHUB_BL_H
#define _NANOHUB_BL_H

#include <linux/platform_data/nanohub.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>

struct nanohub_data;

struct nanohub_bl {
	u8 cmd_erase;
	u8 cmd_read_memory;
	u8 cmd_write_memory;

	int (*open)(const void *);
	void (*close)(const void *);
	u8 (*sync)(const void *);
	u8 (*write_data)(const void *, u8 *, int);
	u8 (*write_cmd)(const void *, u8);
	u8 (*read_data)(const void *, u8 *, int);
	u8 (*read_ack)(const void *);

	u8 *tx_buffer;
	u8 *rx_buffer;
};

int nanohub_bl_open(struct nanohub_data *data);
u8 nanohub_bl_sync(struct nanohub_data *data);
void nanohub_bl_close(struct nanohub_data *data);
u8 nanohub_bl_download(struct nanohub_data *data,
	u32 addr, const u8 *image, size_t length);
u8 nanohub_bl_erase_shared(struct nanohub_data *data);
u8 nanohub_bl_erase_sector(struct nanohub_data *data, uint16_t sector);
u8 nanohub_bl_read_memory(struct nanohub_data *data, u32 addr,
				u32 length, u8 *buffer);
u8 nanohub_bl_write_memory(struct nanohub_data *data, u32 addr,
				u32 length, const u8 *buffer);

/*
 * Bootloader commands
 * _NS versions are no-stretch. (Only valid on I2C)
 * will return CMD_BUSY instead of stretching the clock
 */

#define CMD_GET				0x00
#define CMD_GET_VERSION			0x01
#define CMD_GET_ID			0x02
#define CMD_READ_MEMORY			0x11
#define CMD_NACK			0x1F
#define CMD_GO				0x21
#define CMD_WRITE_MEMORY		0x31
#define CMD_WRITE_MEMORY_NS		0x32
#define CMD_ERASE			0x44
#define CMD_ERASE_NS			0x45
#define CMD_SOF				0x5A
#define CMD_WRITE_PROTECT		0x63
#define CMD_WRITE_PROTECT_NS		0x64
#define CMD_WRITE_UNPROTECT		0x73
#define CMD_WRITE_UNPROTECT_NS		0x74
#define CMD_BUSY			0x76
#define CMD_ACK				0x79
#define CMD_READOUT_PROTECT		0x82
#define CMD_READOUT_PROTECT_NS		0x83
#define CMD_READOUT_UNPROTECT		0x92
#define CMD_READOUT_UNPROTECT_NS	0x93
#define CMD_SOF_ACK			0xA5

#endif
