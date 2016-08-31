/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef _RMI_F34_H
#define _RMI_F34_H

/* F34 image file offsets. */
#define F34_FW_IMAGE_OFFSET 0x100

/* F34 register offsets. */
#define F34_BLOCK_DATA_OFFSET	2

/* F34 commands */
#define F34_WRITE_FW_BLOCK        0x2
#define F34_ERASE_ALL             0x3
#define F34_READ_CONFIG_BLOCK     0x5
#define F34_WRITE_CONFIG_BLOCK    0x6
#define F34_ERASE_CONFIG          0x7
#define F34_ENABLE_FLASH_PROG     0xf

#define F34_STATUS_IN_PROGRESS    0xff
#define F34_STATUS_IDLE		  0x80

#define F34_IDLE_WAIT_MS 500
#define F34_ENABLE_WAIT_MS 300
#define F34_ERASE_WAIT_MS (5 * 1000)

union f34_query_regs {
	struct {
		u16 reg_map:1;
		u16 unlocked:1;
		u16 has_config_id:1;
		u16 reserved:5;
		u16 block_size:16;
		u16 fw_block_count:16;
		u16 config_block_count:16;
	} __attribute__ ((__packed__));
	struct {
		u8 regs[7];
		u16 address;
	} __attribute__((__packed__));
};

union f34_control_status {
	struct {
		u8 command:4;
		u8 status:3;
		u8 program_enabled:1;
	} __attribute__ ((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

#define IS_IDLE(ctl_ptr) ((!ctl_ptr->status) && (!ctl_ptr->command))

#endif
