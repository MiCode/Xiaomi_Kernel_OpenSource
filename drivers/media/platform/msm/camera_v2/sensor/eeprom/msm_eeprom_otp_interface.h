/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_EEPROM_INSENSOR_H
#define MSM_EEPROM_INSENSOR_H

static uint16_t eeprom_sensor_readreg(
	struct msm_eeprom_ctrl_t *s_ctrl, uint32_t reg_addr)
{
	uint16_t reg_value = 0;
	s_ctrl->i2c_client.i2c_func_tbl->i2c_read(
				&(s_ctrl->i2c_client),
				reg_addr,
				&reg_value, MSM_CAMERA_I2C_BYTE_DATA);
	return reg_value ;
}

static int eeprom_sensor_writereg(
	struct msm_eeprom_ctrl_t *s_ctrl, uint32_t reg_addr, uint32_t reg_value, uint32_t delay)
{
	int rc = 0;
	rc = s_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&(s_ctrl->i2c_client), reg_addr, reg_value, MSM_CAMERA_I2C_BYTE_DATA);
	msleep(delay);
	return rc;
}

static int eeprom_init_ov5675_reg_otp(struct msm_eeprom_ctrl_t *e_ctrl, uint16_t addr)
{
	int rc = 0;
	if (!e_ctrl) {
		pr_err("%s e_ctrl is NULL\n", __func__);
		return -EINVAL;
	}

	printk("%s %d E\n", __func__, __LINE__);

	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	e_ctrl->i2c_client.cci_client->sid = addr >> 1;
	rc = eeprom_sensor_writereg(e_ctrl, 0x3d88, 0x7010, 1);
	if (rc < 0) {
		pr_err("i2c write faild\n");
		return rc;
	}
	rc = eeprom_sensor_writereg(e_ctrl, 0x3d8a, 0x70a4, 1);
	if (rc < 0) {
		pr_err("i2c write faild\n");
		return rc;
	}
	rc = eeprom_sensor_writereg(e_ctrl, 0x3d81, 0x01, 1);
	if (rc < 0) {
		pr_err("i2c write faild\n");
		return rc;
	}
	rc = eeprom_sensor_writereg(e_ctrl, 0x0100, 0x01, 1);
	if (rc < 0) {
		pr_err("i2c write faild\n");
		return rc;
	}

	printk("%s : %d X\n", __func__, __LINE__);

	return 0;
}

static uint16_t sensor_eeprom_match_crc_id(struct msm_eeprom_ctrl_t *e_ctrl, uint32_t addr)
{
	uint16_t data = 0;
	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;

	data = eeprom_sensor_readreg(e_ctrl, addr);

	if (0x01 != data) {
		pr_err("eeprom match otp id failed! map valid data is %x !\n", data);
		return -EPERM;
	}
	data = eeprom_sensor_readreg(e_ctrl, addr+1);

	return data;
}
#endif
