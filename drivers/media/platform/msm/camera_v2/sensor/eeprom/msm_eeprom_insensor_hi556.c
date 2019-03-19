/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include "msm_sd.h"
#include "msm_eeprom.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include<linux/kernel.h>

DEFINE_MSM_MUTEX(msm_eeprom_insensor_hi556_mutex);

struct sensor_otp_reg_t {
	uint32_t addr;
	uint32_t data;
};

struct sensor_otp_reg_t hi556_init_otp[] = {
	{0x0e00, 0x0102},
	{0x0e02, 0x0102},
	{0x0e0c, 0x0100},
	{0x27fe, 0xe000},
	{0x0b0e, 0x8600},
	{0x0d04, 0x0100},
	{0x0d02, 0x0707},
	{0x0f30, 0x6e25},
	{0x0f32, 0x7067},
	{0x0f02, 0x0106},
	{0x0a04, 0x0000},
	{0x0e0a, 0x0001},
	{0x004a, 0x0100},
	{0x003e, 0x1000},
	{0x0a00, 0x0100},
};
#define OTP_ALL_SIZE 5388
#define FUSION_ID_SIZE 10

int hi556_insensor_read_otp_info(struct msm_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0;
	uint16_t data = 0;


	uint16_t j;
	uint8_t *memptr;

	if (!e_ctrl) {
		pr_err("%s e_ctrl is NULL\n", __func__);
		return -EINVAL;
	}

	printk("%s : %d E\n", __func__, __LINE__);


	memptr = e_ctrl->cal_data.mapdata;

	e_ctrl->i2c_client.addr_type = 2;

	for (j = 0; j < sizeof(hi556_init_otp)/sizeof(struct sensor_otp_reg_t); j++) {
		rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), hi556_init_otp[j].addr, hi556_init_otp[j].data, 2);
		if (rc < 0) {
			pr_err("%s: hi556 otp init function failed\n", __func__);
			return rc;
		}
	}

	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0a02, 0x01, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0a00, 0x00, 1);
	msleep(10);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0f02, 0x00, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x011a, 0x01, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x011b, 0x09, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0d04, 0x01, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0d00, 0x07, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x003e, 0x10, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0a00, 0x01, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x10a, (0x401>>8) & 0xff, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x10b, 0x401 & 0xff, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x102, 0x01, 1);


	for (j=0; j < OTP_ALL_SIZE; j++)  {
		rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_read(&(e_ctrl->i2c_client), 0x108, &data, 1);
		memptr[j] = data;
	}


	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x10a, 0x00, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x10b, 0x01, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x102, 0x01, 1);

	for (j=OTP_ALL_SIZE; j <OTP_ALL_SIZE + FUSION_ID_SIZE; j++)  {
		rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_read(&(e_ctrl->i2c_client), 0x108, &data, 1);
		memptr[j] = data;
	}



	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0a00, 0x00, 1);
	msleep(10);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x003f, 0x00, 1);
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0a00, 0x01, 1);

	if (rc < 0) {
		pr_err("%s: hi556 read otp info failed\n", __func__);
		pr_err("%s : %d  rc:%d \n", __func__, __LINE__, rc);
		return rc;
	}

	printk("%s : %d \n", __func__, __LINE__);

	return rc;
}
