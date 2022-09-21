/**************************************************************************
 * Copyright (c) 2016, STMicroelectronics - All Rights Reserved

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include "stmvl53l5_i2c.h"

int32_t stmvl53l5_read_multi(struct i2c_client *client,
	uint8_t * i2c_buffer,
	uint16_t reg_index,
	uint8_t *pdata,
	uint32_t count)
{
	int32_t status = 0;
	uint32_t position = 0;
	uint32_t data_size = 0;
	struct i2c_msg message;

	message.addr  = 0x29;

	do {
		data_size = (count - position) > VL53L5_COMMS_CHUNK_SIZE ? VL53L5_COMMS_CHUNK_SIZE : (count - position);

		i2c_buffer[0] = (reg_index + position) >> 8;
		i2c_buffer[1] = (reg_index + position) & 0xFF;

		message.flags = 0;
		message.buf   = i2c_buffer;
		message.len   = 2;

		status = i2c_transfer(client->adapter, &message, 1);
		if (status != 1)
			return -EIO;

		message.flags = 1;
		message.buf   = pdata + position;
		message.len   = data_size;

		status = i2c_transfer(client->adapter, &message, 1);
		if (status != 1)
			return -EIO;

		position += data_size;

	} while (position < count);

	return 0;
}


int32_t stmvl53l5_write_multi(struct i2c_client *client,
		uint8_t *i2c_buffer,
		uint16_t reg_index,
		uint8_t *pdata,
		uint32_t count)
{
	int32_t status = 0;
	uint32_t position = 0;
	int32_t data_size = 0;
	struct i2c_msg message;

	message.addr  = 0x29;

	do {
		data_size = (count - position) > (VL53L5_COMMS_CHUNK_SIZE-2) ? (VL53L5_COMMS_CHUNK_SIZE-2) : (count - position);

		memcpy(&i2c_buffer[2], &pdata[position], data_size);

		i2c_buffer[0] = (reg_index + position) >> 8;
		i2c_buffer[1] = (reg_index + position) & 0xFF;

		message.flags = 0;
		message.len   = data_size + 2;
		message.buf   = i2c_buffer;

		status = i2c_transfer(client->adapter, &message, 1);
		if (status != 1)
			return -EIO;

		position +=  data_size;

	} while (position < count);

	return 0;
}

int32_t stmvl53l5_write_byte(
	struct i2c_client *client, uint8_t * i2c_buffer, uint16_t address, uint8_t value)
{
	return stmvl53l5_write_multi(client, i2c_buffer, address, &value, 1);
}

int32_t stmvl53l5_read_byte(
	struct i2c_client *client, uint8_t * i2c_buffer, uint16_t address, uint8_t *p_value)
{
	return stmvl53l5_read_multi(client, i2c_buffer, address, p_value, 1);
}
