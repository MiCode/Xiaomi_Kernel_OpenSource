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
#include <linux/kernel.h>
#include <linux/string.h>

#include "inv_mpu_iio.h"
#include "inv_sh_command.h"

int inv_sh_command_activate(struct inv_sh_command *cmd, int id, int enable)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	if (enable)
		cmd->command = INV_SH_COMMAND_SENSOR_ON;
	else
		cmd->command = INV_SH_COMMAND_SENSOR_OFF;

	return 0;
}

int inv_sh_command_set_power(struct inv_sh_command *cmd, int power)
{
	uint16_t val;

	if (cmd == NULL || power < 0)
		return -EINVAL;

	if (power == 0)
		val = 0;
	else
		val = 1;

	cmd->id = 0;
	cmd->command = INV_SH_COMMAND_SET_POWER;
	cmd->data.power = val;

	return 0;
}

int inv_sh_command_batch(struct inv_sh_command *cmd, int id, int timeout_ms)
{
	uint16_t val;

	if (cmd == NULL || id < 0 || timeout_ms < 0)
		return -EINVAL;

	if (timeout_ms > 0xFFFF)
		val = 0xFFFF;
	else
		val = timeout_ms;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_BATCH_ON;
	cmd->data.timeout = val;

	return 0;
}

int inv_sh_command_flush(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_FLUSH;

	return 0;
}

int inv_sh_command_set_delay(struct inv_sh_command *cmd, int id, int delay_ms)
{
	uint16_t val;

	if (cmd == NULL || id < 0 || delay_ms < 0)
		return -EINVAL;

	if (delay_ms > 0xFFFF)
		val = 0xFFFF;
	else
		val = delay_ms;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_SET_DELAY;
	cmd->data.delay = val;

	return 0;
}

int inv_sh_command_set_calib_gains(struct inv_sh_command *cmd, int id,
					int32_t gain[9])
{
	if (cmd == NULL || id < 0 || gain == NULL)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_SET_CALIB_GAINS;
	memcpy(cmd->data.gain, gain, sizeof(cmd->data.gain));

	return 0;
}

int inv_sh_command_get_calib_gains(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_GET_CALIB_GAINS;

	return 0;
}

int inv_sh_command_set_calib_offsets(struct inv_sh_command *cmd, int id,
					int32_t offset[3])
{
	if (cmd == NULL || id < 0 || offset == NULL)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_SET_CALIB_OFFSETS;
	memcpy(cmd->data.offset, offset, sizeof(cmd->data.offset));

	return 0;
}

int inv_sh_command_get_calib_offsets(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_GET_CALIB_OFFSETS;

	return 0;
}

int inv_sh_command_set_ref_frame(struct inv_sh_command *cmd, int id,
					int32_t frame[9])
{
	if (cmd == NULL || id < 0 || frame == NULL)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_SET_REF_FRAME;
	memcpy(cmd->data.frame, frame, sizeof(cmd->data.frame));

	return 0;
}

int inv_sh_command_get_firmware_info(struct inv_sh_command *cmd)
{
	if (cmd == NULL)
		return -EINVAL;

	cmd->id = 0;
	cmd->command = INV_SH_COMMAND_GET_FIRMWARE_INFO;

	return 0;
}

int inv_sh_command_get_data(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_GET_DATA;

	return 0;
}

int inv_sh_command_get_clock_rate(struct inv_sh_command *cmd)
{
	if (cmd == NULL)
		return -EINVAL;

	cmd->id = 0;
	cmd->command = INV_SH_COMMAND_GET_CLOCK_RATE;

	return 0;
}

int inv_sh_command_ping(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_PING;

	return 0;
}

int inv_sh_command_reset(struct inv_sh_command *cmd, int id)
{
	if (cmd == NULL || id < 0)
		return -EINVAL;

	cmd->id = id;
	cmd->command = INV_SH_COMMAND_RESET;

	return 0;
}

static inline void inv_sh_write_le_int16(uint8_t *data, uint16_t value)
{
	data[0] = value & 0xFF;
	data[1] = (value >> 8) & 0xFF;
}

static inline void inv_sh_write_le_int32(uint8_t *data, uint32_t value)
{
	data[0] = value & 0xFF;
	data[1] = (value >> 8) & 0xFF;
	data[2] = (value >> 16) & 0xFF;
	data[3] = (value >> 24) & 0xFF;
}

int inv_sh_command_send(struct inv_mpu_state *st, struct inv_sh_command *cmd)
{
	uint8_t data[64];
	uint8_t *ptr = data;
	size_t size;
	int i;

	/* mandatory fields */
	data[0] = cmd->id;
	data[1] = cmd->command;
	size = 2;

	/* optional data field */
	ptr = &data[2];
	switch (cmd->command) {
	case INV_SH_COMMAND_SET_POWER:
		inv_sh_write_le_int16(ptr, cmd->data.power);
		size += sizeof(cmd->data.power);
		break;
	case INV_SH_COMMAND_BATCH_ON:
		inv_sh_write_le_int16(ptr, cmd->data.timeout);
		size += sizeof(cmd->data.timeout);
		break;
	case INV_SH_COMMAND_SET_DELAY:
		inv_sh_write_le_int16(ptr, cmd->data.delay);
		size += sizeof(cmd->data.delay);
		break;
	case INV_SH_COMMAND_SET_CALIB_GAINS:
		for (i = 0; i < ARRAY_SIZE(cmd->data.gain); i++) {
			inv_sh_write_le_int32(ptr, cmd->data.gain[i]);
			ptr += sizeof(cmd->data.gain[i]);
		}
		size += sizeof(cmd->data.gain);
		break;
	case INV_SH_COMMAND_SET_CALIB_OFFSETS:
		for (i = 0; i < ARRAY_SIZE(cmd->data.offset); i++) {
			inv_sh_write_le_int32(ptr, cmd->data.offset[i]);
			ptr += sizeof(cmd->data.offset[i]);
		}
		size += sizeof(cmd->data.offset);
		break;
	case INV_SH_COMMAND_SET_REF_FRAME:
		for (i = 0; i < ARRAY_SIZE(cmd->data.frame); i++) {
			inv_sh_write_le_int32(ptr, cmd->data.frame[i]);
			ptr += sizeof(cmd->data.frame[i]);
		}
		size += sizeof(cmd->data.frame);
		break;
	default:
		break;
	}

	return inv_send_command_down(st, data, size);
}
