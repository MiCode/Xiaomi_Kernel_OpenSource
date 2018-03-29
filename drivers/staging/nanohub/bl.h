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
	uint8_t cmd_erase;
	uint8_t cmd_read_memory;
	uint8_t cmd_write_memory;

	int (*open)(const void *);
	void (*close)(const void *);
	uint8_t (*sync)(const void *);
	uint8_t (*write_data)(const void *, uint8_t *, int);
	uint8_t (*write_cmd)(const void *, uint8_t);
	uint8_t (*read_data)(const void *, uint8_t *, int);
	uint8_t (*read_ack)(const void *);

	uint8_t *tx_buffer;
	uint8_t *rx_buffer;
};

int nanohub_bl_open(struct nanohub_data *);
uint8_t nanohub_bl_sync(struct nanohub_data *);
void nanohub_bl_close(struct nanohub_data *);
uint8_t nanohub_bl_download(struct nanohub_data *, uint32_t addr,
			    const uint8_t *data, size_t length);
uint8_t nanohub_bl_erase_shared(struct nanohub_data *);
uint8_t nanohub_bl_erase_sector(struct nanohub_data *, uint16_t);
uint8_t nanohub_bl_read_memory(struct nanohub_data *, uint32_t, uint32_t,
			       uint8_t *);
uint8_t nanohub_bl_write_memory(struct nanohub_data *, uint32_t, uint32_t,
				const uint8_t *);

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
