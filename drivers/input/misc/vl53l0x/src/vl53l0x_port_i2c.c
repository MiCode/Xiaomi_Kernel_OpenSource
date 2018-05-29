/*
 *  vl53l0x_port_i2c.c - Linux kernel modules for
 *  STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include "stmvl53l0x-i2c.h"
#include "stmvl53l0x-cci.h"
#include "vl53l0x_platform.h"
#include "vl53l0x_i2c_platform.h"
#include "stmvl53l0x.h"

#define I2C_M_WR			0x00
#define STATUS_OK			0x00
#define STATUS_FAIL			(-1)
/** int  VL_I2CWrite(VL_Dev_t dev, void *buff, uint8_t len);
 * @brief       Write data buffer to VL53L0 device via i2c
 * @param dev   The device to write to
 * @param buff  The data buffer
 * @param len   The length of the transaction in byte
 * @return      0 on success
 */
int VL_I2CWrite(struct vl_data *dev, uint8_t *buff, uint8_t len)
{


	int err = 0;

	if (dev->bus_type == CCI_BUS) {
#ifdef CAMERA_CCI
		uint16_t index;
		struct cci_data *cci_client_obj =
			(struct cci_data *)dev->client_object;
		struct msm_camera_i2c_client *client = cci_client_obj->client;

		index = buff[0];
		/*dbg("%s: index: %d len: %d\n", __func__, index, len); */

		if (len == 2) {
			uint8_t data;

			data = buff[1];
			/* for byte access */
			err = client->i2c_func_tbl->i2c_write(client, index,
					data, MSM_CAMERA_I2C_BYTE_DATA);
			if (err < 0) {
				dbg("%s:%d failed status=%d\n",
					__func__, __LINE__, err);
				return err;
			}
		} else if (len == 3) {
			uint16_t data;

			data = ((uint16_t)buff[1] << 8) | (uint16_t)buff[2];
			err = client->i2c_func_tbl->i2c_write(client, index,
					data, MSM_CAMERA_I2C_WORD_DATA);
			if (err < 0) {
				dbg("%s:%d failed status=%d\n",
					__func__, __LINE__, err);
				return err;
			}
		} else if (len >= 5) {
			err = client->i2c_func_tbl->i2c_write_seq(client,
					index, &buff[1], (len-1));
			if (err < 0) {
				dbg("%s:%d failed status=%d\n",
					__func__, __LINE__, err);
				return err;
			}

		}
#endif
#ifndef CAMERA_CCI
	} else {
		struct i2c_msg msg[1];
		struct i2c_data *i2c_client_obj =
					(struct i2c_data *)dev->client_object;
		struct i2c_client *client =
				(struct i2c_client *)i2c_client_obj->client;

		msg[0].addr = client->addr;
		msg[0].flags = I2C_M_WR;
		msg[0].buf = buff;
		msg[0].len = len;

		err = i2c_transfer(client->adapter, msg, 1);
		/* return the actual messages transfer */
		if (err != 1) {
			dbg("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n",
				__func__, err, client->addr,
				(buff[0] << 8 | buff[1]));
			return STATUS_FAIL;
		}
#endif
	}

	return 0;
}


/** int VL_I2CRead(VL_Dev_t dev, void *buff, uint8_t len);
 * @brief       Read data buffer from VL53L0 device via i2c
 * @param dev   The device to read from
 * @param buff  The data buffer to fill
 * @param len   The length of the transaction in byte
 * @return      transaction status
 */
int VL_I2CRead(struct vl_data *dev, uint8_t *buff, uint8_t len)
{

	int err = 0;

	if (dev->bus_type == CCI_BUS) {
#ifdef CAMERA_CCI
		uint16_t index;
		struct cci_data *cci_client_obj =
				(struct cci_data *)dev->client_object;
		struct msm_camera_i2c_client *client = cci_client_obj->client;

		index = buff[0];
		/* dbg("%s: index: %d\n", __func__, index); */
		err = client->i2c_func_tbl->i2c_read_seq(client,
							index, buff, len);
		if (err < 0) {
			dbg("%s:%d failed status=%d\n",
					__func__, __LINE__, err);
			return err;
		}
#endif
	} else {
#ifndef CAMERA_CCI
		struct i2c_msg msg[1];
		struct i2c_data *i2c_client_obj =
					(struct i2c_data *)dev->client_object;
		struct i2c_client *client =
				(struct i2c_client *) i2c_client_obj->client;

		msg[0].addr = client->addr;
		msg[0].flags = I2C_M_RD|client->flags;
		msg[0].buf = buff;
		msg[0].len = len;

		err = i2c_transfer(client->adapter, &msg[0], 1);
		/* return the actual message transfer */
		if (err != 1) {
			dbg("%s: Read i2c_transfer err:%d, addr:0x%x\n",
				__func__, err, client->addr);
			return STATUS_FAIL;
		}
#endif
	}

	return 0;
}
