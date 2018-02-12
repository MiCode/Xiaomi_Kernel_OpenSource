/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"

int32_t camera_io_dev_poll(struct camera_io_master *io_master_info,
	uint32_t addr, uint16_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms)
{
	int16_t mask = data_mask & 0xFF;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		return cam_cci_i2c_poll(io_master_info->cci_client,
			addr, data, mask, data_type, addr_type, delay_ms);
	} else if (io_master_info->master_type == I2C_MASTER) {
		return cam_qup_i2c_poll(io_master_info->client,
			addr, data, data_mask, addr_type, data_type,
			delay_ms);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
}

int32_t camera_io_dev_read(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		return cam_cci_i2c_read(io_master_info->cci_client,
			addr, data, addr_type, data_type);
	} else if (io_master_info->master_type == I2C_MASTER) {
		return cam_qup_i2c_read(io_master_info->client,
			addr, data, addr_type, data_type);
	} else if (io_master_info->master_type == SPI_MASTER) {
		return cam_spi_read(io_master_info,
			addr, data, addr_type, data_type);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
	return 0;
}

int32_t camera_io_dev_read_seq(struct camera_io_master *io_master_info,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type, int32_t num_bytes)
{
	if (io_master_info->master_type == CCI_MASTER) {
		return cam_camera_cci_i2c_read_seq(io_master_info->cci_client,
			addr, data, addr_type, num_bytes);
	} else if (io_master_info->master_type == I2C_MASTER) {
		return cam_qup_i2c_read_seq(io_master_info->client,
			addr, data, addr_type, num_bytes);
	} else if (io_master_info->master_type == SPI_MASTER) {
		return cam_spi_read_seq(io_master_info,
			addr, data, addr_type, num_bytes);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
	return 0;
}

int32_t camera_io_dev_write(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		return -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		return cam_cci_i2c_write_table(io_master_info,
			write_setting);
	} else if (io_master_info->master_type == I2C_MASTER) {
		return cam_qup_i2c_write_table(io_master_info,
			write_setting);
	} else if (io_master_info->master_type == SPI_MASTER) {
		return cam_spi_write_table(io_master_info,
			write_setting);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
}

int32_t camera_io_dev_write_continuous(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag)
{
	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		return -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		return cam_cci_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
	} else if (io_master_info->master_type == I2C_MASTER) {
		return cam_qup_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
	} else if (io_master_info->master_type == SPI_MASTER) {
		return cam_spi_write_table(io_master_info,
			write_setting);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
}

int32_t camera_io_init(struct camera_io_master *io_master_info)
{
	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		io_master_info->cci_client->cci_subdev =
			cam_cci_get_subdev();
		return cam_sensor_cci_i2c_util(io_master_info->cci_client,
			MSM_CCI_INIT);
	} else if ((io_master_info->master_type == I2C_MASTER) ||
			(io_master_info->master_type == SPI_MASTER)) {
		return 0;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
}

int32_t camera_io_release(struct camera_io_master *io_master_info)
{
	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		return cam_sensor_cci_i2c_util(io_master_info->cci_client,
			MSM_CCI_RELEASE);
	} else if ((io_master_info->master_type == I2C_MASTER) ||
			(io_master_info->master_type == SPI_MASTER)) {
		return 0;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		return -EINVAL;
	}
}
