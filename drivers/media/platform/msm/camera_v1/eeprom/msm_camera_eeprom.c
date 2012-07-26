/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

int32_t msm_camera_eeprom_read(struct msm_eeprom_ctrl_t *ectrl,
	uint32_t reg_addr, void *data, uint32_t num_byte,
	uint16_t convert_endian)
{
	int rc = 0;
	if (ectrl->func_tbl.eeprom_set_dev_addr != NULL)
		ectrl->func_tbl.eeprom_set_dev_addr(ectrl, &reg_addr);

	if (!convert_endian) {
		rc = msm_camera_i2c_read_seq(
			&ectrl->i2c_client, reg_addr, data, num_byte);
	} else {
		unsigned char buf[num_byte];
		uint8_t *data_ptr = (uint8_t *) data;
		int i;
		rc = msm_camera_i2c_read_seq(
			&ectrl->i2c_client, reg_addr, buf, num_byte);
		for (i = 0; i < num_byte; i += 2) {
			data_ptr[i] = buf[i+1];
			data_ptr[i+1] = buf[i];
		}
	}
	return rc;
}

int32_t msm_camera_eeprom_read_tbl(struct msm_eeprom_ctrl_t *ectrl,
	struct msm_camera_eeprom_read_t *read_tbl, uint16_t tbl_size)
{
	int i, rc = 0;
	CDBG("%s: open\n", __func__);
	if (read_tbl == NULL)
		return rc;

	for (i = 0; i < tbl_size; i++) {
		rc = msm_camera_eeprom_read
			(ectrl, read_tbl[i].reg_addr,
			read_tbl[i].dest_ptr, read_tbl[i].num_byte,
			read_tbl[i].convert_endian);
		if (rc < 0) {
			pr_err("%s: read failed\n", __func__);
			return rc;
		}
	}
	CDBG("%s: done\n", __func__);
	return rc;
}

int32_t msm_camera_eeprom_get_info(struct msm_eeprom_ctrl_t *ectrl,
	struct msm_camera_eeprom_info_t *einfo)
{
	int rc = 0;
	CDBG("%s: open\n", __func__);
	memcpy(einfo, ectrl->info, ectrl->info_size);
	CDBG("%s: done =%d\n", __func__, rc);
	return rc;
}

int32_t msm_camera_eeprom_get_data(struct msm_eeprom_ctrl_t *ectrl,
	struct msm_eeprom_data_t *edata)
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

int32_t msm_eeprom_config(struct msm_eeprom_ctrl_t *e_ctrl,
	void __user *argp)
{
	struct msm_eeprom_cfg_data cdata;
	int32_t rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct msm_eeprom_cfg_data)))
		return -EFAULT;
	mutex_lock(e_ctrl->eeprom_mutex);

	switch (cdata.cfgtype) {
	case CFG_GET_EEPROM_INFO:
		if (e_ctrl->func_tbl.eeprom_get_info == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = e_ctrl->func_tbl.eeprom_get_info(e_ctrl,
			&cdata.cfg.get_info);
		cdata.is_eeprom_supported = 1;
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct msm_eeprom_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_EEPROM_DATA:
		if (e_ctrl->func_tbl.eeprom_get_data == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = e_ctrl->func_tbl.eeprom_get_data(e_ctrl,
			&cdata.cfg.get_data);

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct msm_eeprom_cfg_data)))
			rc = -EFAULT;
		break;
	default:
		break;
	}
	mutex_unlock(e_ctrl->eeprom_mutex);
	return rc;
}

struct msm_eeprom_ctrl_t *get_ectrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_eeprom_ctrl_t, sdev);
}

long msm_eeprom_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_eeprom_ctrl_t *e_ctrl = get_ectrl(sd);
	void __user *argp = (void __user *)arg;
	switch (cmd) {
	case VIDIOC_MSM_EEPROM_CFG:
		return msm_eeprom_config(e_ctrl, argp);
	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_eeprom_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_eeprom_ctrl_t *e_ctrl_t = NULL;
	CDBG("%s called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	e_ctrl_t = (struct msm_eeprom_ctrl_t *)(id->driver_data);
	e_ctrl_t->i2c_client.client = client;

	if (e_ctrl_t->i2c_addr != 0)
		e_ctrl_t->i2c_client.client->addr = e_ctrl_t->i2c_addr;

	CDBG("%s client = %x\n", __func__, (unsigned int) client);

	/* Assign name for sub device */
	snprintf(e_ctrl_t->sdev.name, sizeof(e_ctrl_t->sdev.name),
		"%s", e_ctrl_t->i2c_driver->driver.name);

	if (e_ctrl_t->func_tbl.eeprom_init != NULL) {
		rc = e_ctrl_t->func_tbl.eeprom_init(e_ctrl_t,
			e_ctrl_t->i2c_client.client->adapter);
	}
	msm_camera_eeprom_read_tbl(e_ctrl_t,
		e_ctrl_t->read_tbl,
		e_ctrl_t->read_tbl_size);

	if (e_ctrl_t->func_tbl.eeprom_format_data != NULL)
		e_ctrl_t->func_tbl.eeprom_format_data();

	if (e_ctrl_t->func_tbl.eeprom_release != NULL)
		rc = e_ctrl_t->func_tbl.eeprom_release(e_ctrl_t);


	/* Initialize sub device */
	v4l2_i2c_subdev_init(&e_ctrl_t->sdev,
		e_ctrl_t->i2c_client.client,
		e_ctrl_t->eeprom_v4l2_subdev_ops);
	CDBG("%s success resut=%d\n", __func__, rc);
	return rc;

probe_failure:
	pr_err("%s failed! rc = %d\n", __func__, rc);
	return rc;
}
