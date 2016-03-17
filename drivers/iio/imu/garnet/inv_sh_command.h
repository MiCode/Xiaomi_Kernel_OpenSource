/*
* Copyright (C) 2015 InvenSense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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
#ifndef _INV_SH_COMMAND_H
#define _INV_SH_COMMAND_H

#include <linux/types.h>

#include "inv_mpu_iio.h"
#include "inv_sh_data.h"

union inv_sh_command_data {
	uint16_t delay;
	uint16_t timeout;
	uint16_t power;
	int32_t gain[9];
	int32_t offset[3];
	int32_t frame[9];
};

struct inv_sh_command {
	uint8_t id;
	uint8_t command;
	union inv_sh_command_data data;
};

enum inv_sh_command_id {
	INV_SH_COMMAND_SENSOR_OFF			= 0x00,
	INV_SH_COMMAND_SENSOR_ON			= 0x01,
	INV_SH_COMMAND_SET_POWER			= 0x02,
	INV_SH_COMMAND_BATCH_ON				= 0x03,
	INV_SH_COMMAND_FLUSH				= 0x04,
	INV_SH_COMMAND_SET_DELAY			= 0x05,
	INV_SH_COMMAND_SET_CALIB_GAINS			= 0x06,
	INV_SH_COMMAND_GET_CALIB_GAINS			= 0x07,
	INV_SH_COMMAND_SET_CALIB_OFFSETS		= 0x08,
	INV_SH_COMMAND_GET_CALIB_OFFSETS		= 0x09,
	INV_SH_COMMAND_SET_REF_FRAME			= 0x0A,
	INV_SH_COMMAND_GET_FIRMWARE_INFO		= 0x0B,
	INV_SH_COMMAND_GET_DATA				= 0x0C,
	INV_SH_COMMAND_GET_CLOCK_RATE			= 0x0D,
	INV_SH_COMMAND_PING				= 0x0E,
	INV_SH_COMMAND_RESET				= 0x0F,
	INV_SH_COMMAND_MAX,
};

int inv_sh_command_activate(struct inv_sh_command *cmd, int id, int enable);

int inv_sh_command_set_power(struct inv_sh_command *cmd, int power);

int inv_sh_command_batch(struct inv_sh_command *cmd, int id, int timeout_ms);

int inv_sh_command_flush(struct inv_sh_command *cmd, int id);

int inv_sh_command_set_delay(struct inv_sh_command *cmd, int id, int delay_ms);

int inv_sh_command_set_calib_gains(struct inv_sh_command *cmd, int id,
					int32_t gain[9]);

int inv_sh_command_get_calib_gains(struct inv_sh_command *cmd, int id);

int inv_sh_command_set_calib_offsets(struct inv_sh_command *cmd, int id,
					int32_t offset[3]);

int inv_sh_command_get_calib_offsets(struct inv_sh_command *cmd, int id);

int inv_sh_command_set_ref_frame(struct inv_sh_command *cmd, int id,
					int32_t frame[9]);

int inv_sh_command_get_firmware_info(struct inv_sh_command *cmd);

int inv_sh_command_get_data(struct inv_sh_command *cmd, int id);

int inv_sh_command_get_clock_rate(struct inv_sh_command *cmd);

int inv_sh_command_ping(struct inv_sh_command *cmd, int id);

int inv_sh_command_reset(struct inv_sh_command *cmd, int id);

int inv_sh_command_send(struct inv_mpu_state *st, struct inv_sh_command *cmd);

#endif
