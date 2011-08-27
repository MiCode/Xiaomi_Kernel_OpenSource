/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include "msm_camera_eeprom.h"

int32_t msm_camera_eeprom_init(struct msm_camera_eeprom_client *ectrl,
	struct i2c_adapter *adapter)
{
	CDBG("%s: open", __func__);
	ectrl->i2c_client->client =
		i2c_new_dummy(adapter, ectrl->i2c_addr >> 1);
	if (ectrl->i2c_client->client == NULL) {
		CDBG("%s: eeprom i2c get client failed\n", __func__);
		return -EFAULT;
	}
	ectrl->i2c_client->client->addr = ectrl->i2c_addr;
	CDBG("%s: done", __func__);
	return 0;
}

int32_t msm_camera_eeprom_release(struct msm_camera_eeprom_client *ectrl)
{
	if (ectrl->i2c_client->client != NULL) {
		i2c_unregister_device(ectrl->i2c_client->client);
		ectrl->i2c_client->client = NULL;
	}
	return 0;
}

int32_t msm_camera_eeprom_read(struct msm_camera_eeprom_client *ectrl,
	uint16_t reg_addr, void *data, uint32_t num_byte,
	uint16_t convert_endian)
{
	int rc = 0;
	if (ectrl->func_tbl.eeprom_set_dev_addr != NULL)
		ectrl->func_tbl.eeprom_set_dev_addr(ectrl, &reg_addr);

	if (!convert_endian) {
		rc = msm_camera_i2c_read_seq(
			ectrl->i2c_client, reg_addr, data, num_byte);
	} else {
		unsigned char buf[num_byte];
		uint8_t *data_ptr = (uint8_t *) data;
		int i;
		rc = msm_camera_i2c_read_seq(
			ectrl->i2c_client, reg_addr, buf, num_byte);
		for (i = 0; i < num_byte; i++)
			data_ptr[i] = buf[num_byte-i-1];
	}
	return rc;
}

int32_t msm_camera_eeprom_read_tbl(struct msm_camera_eeprom_client *ectrl,
	struct msm_camera_eeprom_read_t *read_tbl, uint16_t tbl_size)
{
	int i, rc = 0;
	CDBG("%s: open", __func__);
	if (read_tbl == NULL)
		return rc;

	for (i = 0; i < tbl_size; i++) {
		rc = msm_camera_eeprom_read
			(ectrl, read_tbl[i].reg_addr,
			read_tbl[i].dest_ptr, read_tbl[i].num_byte,
			read_tbl[i].convert_endian);
		if (rc < 0)	{
			CDBG("%s: read failed\n", __func__);
			return rc;
		}
	}
	CDBG("%s: done", __func__);
	return rc;
}

int32_t msm_camera_eeprom_get_data(struct msm_camera_eeprom_client *ectrl,
	struct sensor_eeprom_data_t *edata)
{
	int rc = 0;
	if (edata->index >= ectrl->data_tbl_size)
		return -EFAULT;
	if (copy_to_user(edata->eeprom_data,
		ectrl->data_tbl[edata->index].data,
		ectrl->data_tbl[edata->index].size))
		rc = -EFAULT;
	return rc;
}

