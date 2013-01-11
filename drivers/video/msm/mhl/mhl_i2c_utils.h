/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MHL_I2C_UTILS_H__
#define __MHL_I2C_UTILS_H__

#include <linux/i2c.h>
#include <linux/types.h>

#include "mhl_defs.h"

/*
 * I2C command to the adapter to append
 * the buffer from next msg to this one.
 */
#define I2C_M_APPND_NXT_WR          0x0002

extern uint8_t slave_addrs[MAX_PAGES];
extern struct mhl_msm_state_t *mhl_msm_state;

int mhl_i2c_reg_read(uint8_t slave_addr_index, uint8_t reg_offset);
int mhl_i2c_reg_write(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t value);
int mhl_i2c_reg_write_cmds(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t *value, uint16_t count);
void mhl_i2c_reg_modify(uint8_t slave_addr_index, uint8_t reg_offset,
	uint8_t mask, uint8_t val);

#endif /* __MHL_I2C_UTILS_H__ */
