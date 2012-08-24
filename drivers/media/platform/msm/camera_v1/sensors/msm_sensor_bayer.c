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

#include "msm_sensor_bayer.h"
#include "msm.h"
#include "msm_ispif.h"
#include "msm_camera_i2c_mux.h"
/*=============================================================*/

long msm_sensor_bayer_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
		return 0;
	case VIDIOC_MSM_SENSOR_CSID_INFO: {
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		s_ctrl->is_csic = csi_info->is_csic;
		return 0;
	}
	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_sensor_bayer_get_csi_params(struct msm_sensor_ctrl_t *s_ctrl,
		struct csi_lane_params_t *sensor_output_info)
{
	uint8_t index;
	struct msm_camera_csi_lane_params *csi_lane_params =
		s_ctrl->sensordata->sensor_platform_info->csi_lane_params;
	if (csi_lane_params) {
		sensor_output_info->csi_lane_assign = csi_lane_params->
			csi_lane_assign;
		sensor_output_info->csi_lane_mask = csi_lane_params->
			csi_lane_mask;
	}
	sensor_output_info->csi_if = s_ctrl->sensordata->csi_if;
	for (index = 0; index < sensor_output_info->csi_if; index++)
		sensor_output_info->csid_core[index] = s_ctrl->sensordata->
			pdata[index].csid_core;

	return 0;
}

int32_t msm_sensor_bayer_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensor_cfg_data cdata;
	long rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *regs = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata.cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		regs = kzalloc(conf_array.size * sizeof(
			struct msm_camera_i2c_reg_array),
			GFP_KERNEL);
		if (!regs) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(regs, (void *)conf_array.reg_setting,
			conf_array.size * sizeof(
			struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(regs);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = regs;
		rc = msm_camera_i2c_write_bayer_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(regs);
		break;
	}
	case CFG_READ_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *regs;
		int index;

		if (copy_from_user(&conf_array,
				(void *)cdata.cfg.setting,
				sizeof(struct msm_camera_i2c_reg_setting))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
		}

		regs = kzalloc(conf_array.size * sizeof(
				struct msm_camera_i2c_reg_array),
				GFP_KERNEL);
		if (!regs) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			kfree(regs);
			break;
		}

		if (copy_from_user(regs, (void *)conf_array.reg_setting,
				conf_array.size * sizeof(
				struct msm_camera_i2c_reg_array))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				kfree(regs);
				rc = -EFAULT;
				break;
			}

		s_ctrl->sensor_i2c_client->addr_type = conf_array.addr_type;
		for (index = 0; index < conf_array.size; index++) {
			msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
					regs[index].reg_addr,
					&regs[index].reg_data,
				conf_array.data_type
				);
		}

		if (copy_to_user(conf_array.reg_setting,
			regs,
			conf_array.size * sizeof(
			struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(regs);
			rc = -EFAULT;
			break;
		}
		s_ctrl->sensor_i2c_client->addr_type = conf_array.addr_type;
		kfree(regs);
		break;
	}
	case CFG_PCLK_CHANGE: {
		uint32_t pclk = cdata.cfg.pclk;
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &pclk);
		break;
	}
	case CFG_GPIO_OP: {
		struct msm_cam_gpio_operation gop;
		if (copy_from_user(&gop,
			(void *)cdata.cfg.setting,
			sizeof(struct msm_cam_gpio_operation))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
		}
		switch (gop.op_type) {
		case GPIO_GET_VALUE:
			gop.value = gpio_get_value(gop.address);
			if (copy_from_user((void *)cdata.cfg.setting,
				&gop,
				sizeof(struct msm_cam_gpio_operation))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
			break;
		case GPIO_SET_VALUE:
			gpio_set_value(gop.address, gop.value);
			break;
		case GPIO_SET_DIRECTION_INPUT:
			gpio_direction_input(gop.address);
			break;
		case GPIO_SET_DIRECTION_OUTPUT:
			gpio_direction_output(gop.address, gop.value);
			break;
		case GPIO_REQUEST:
			gpio_request(gop.address, gop.tag);
			break;
		case GPIO_FREE:
			gpio_free(gop.address);
			break;
		default:
			break;
		}

		break;
	}
	case CFG_GET_CSI_PARAMS:
		if (s_ctrl->func_tbl->sensor_get_csi_params == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->sensor_get_csi_params(
			s_ctrl,
			&cdata.cfg.csi_lane_params);

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_CONFIG_VREG_ARRAY: {
		struct msm_camera_vreg_setting vreg_setting;
		struct camera_vreg_t *cam_vreg = NULL;

		if (copy_from_user(&vreg_setting,
			(void *)cdata.cfg.setting,
			sizeof(struct msm_camera_vreg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		cam_vreg = kzalloc(vreg_setting.num_vreg * sizeof(
			struct camera_vreg_t),
			GFP_KERNEL);
		if (!cam_vreg) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(cam_vreg, (void *)vreg_setting.cam_vreg,
			vreg_setting.num_vreg * sizeof(
			struct camera_vreg_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(cam_vreg);
			rc = -EFAULT;
			break;
		}
		rc = msm_camera_config_vreg(
			&s_ctrl->sensor_i2c_client->client->dev,
			cam_vreg,
			vreg_setting.num_vreg,
			NULL,
			0,
			s_ctrl->reg_ptr,
			vreg_setting.enable);
		if (rc < 0) {
			kfree(cam_vreg);
			pr_err("%s: regulator on failed\n", __func__);
			break;
		}

		rc = msm_camera_enable_vreg(
			&s_ctrl->sensor_i2c_client->client->dev,
			cam_vreg,
			vreg_setting.num_vreg,
			NULL,
			0,
			s_ctrl->reg_ptr,
			vreg_setting.enable);
		if (rc < 0) {
			kfree(cam_vreg);
			pr_err("%s: enable regulator failed\n", __func__);
			break;
		}
		kfree(cam_vreg);
		break;
	}
	case CFG_CONFIG_CLK_ARRAY: {
		struct msm_cam_clk_setting clk_setting;
		struct msm_cam_clk_info *clk_info = NULL;

		if (copy_from_user(&clk_setting,
			(void *)cdata.cfg.setting,
			sizeof(struct msm_camera_vreg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		clk_info = kzalloc(clk_setting.num_clk_info * sizeof(
			struct msm_cam_clk_info),
			GFP_KERNEL);
		if (!clk_info) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(clk_info, (void *)clk_setting.clk_info,
			clk_setting.num_clk_info * sizeof(
			struct msm_cam_clk_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(clk_info);
			rc = -EFAULT;
			break;
		}
		rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
			clk_info, s_ctrl->cam_clk,
			clk_setting.num_clk_info,
			clk_setting.enable);
		kfree(clk_info);
		break;
	}
	case CFG_GET_EEPROM_DATA: {
		if (copy_to_user((void *)cdata.cfg.eeprom_data.eeprom_data,
			&s_ctrl->eeprom_data, s_ctrl->eeprom_data.length)) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
		}
		cdata.cfg.eeprom_data.index = s_ctrl->eeprom_data.length;
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t msm_sensor_bayer_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

int32_t msm_sensor_bayer_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
				VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

static struct msm_camera_power_seq_t sensor_power_seq[] = {
	{REQUEST_GPIO, 0},
	{REQUEST_VREG, 0},
	{ENABLE_VREG, 0},
	{ENABLE_GPIO, 0},
	{CONFIG_CLK, 0},
	{CONFIG_EXT_POWER_CTRL, 0},
	{CONFIG_I2C_MUX, 0},
};

int32_t msm_sensor_bayer_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, size = 0, index = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct msm_camera_power_seq_t *power_seq = NULL;
	CDBG("%s: %d\n", __func__, __LINE__);
	if (s_ctrl->power_seq) {
		power_seq = s_ctrl->power_seq;
		size = s_ctrl->num_power_seq;
	} else {
		power_seq = &sensor_power_seq[0];
		size = ARRAY_SIZE(sensor_power_seq);
	}

	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	for (index = 0; index < size; index++) {
		switch (power_seq[index].power_config) {
		case REQUEST_GPIO:
			rc = msm_camera_request_gpio_table(data, 1);
			if (rc < 0) {
				pr_err("%s: request gpio failed\n", __func__);
				goto ERROR;
			}
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case REQUEST_VREG:
			rc = msm_camera_config_vreg(
				&s_ctrl->sensor_i2c_client->client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 1);
			if (rc < 0) {
				pr_err("%s: regulator on failed\n", __func__);
				goto ERROR;
			}
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case ENABLE_VREG:
			rc = msm_camera_enable_vreg(
				&s_ctrl->sensor_i2c_client->client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 1);
			if (rc < 0) {
				pr_err("%s: enable regulator failed\n",
					__func__);
				goto ERROR;
			}
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case ENABLE_GPIO:
			rc = msm_camera_config_gpio_table(data, 1);
			if (rc < 0) {
				pr_err("%s: config gpio failed\n", __func__);
				goto ERROR;
			}
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case CONFIG_CLK:
			if (s_ctrl->clk_rate != 0)
				cam_clk_info->clk_rate = s_ctrl->clk_rate;

			rc = msm_cam_clk_enable(
				&s_ctrl->sensor_i2c_client->client->dev,
				cam_clk_info, s_ctrl->cam_clk,
				ARRAY_SIZE(cam_clk_info), 1);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n", __func__);
				goto ERROR;
			}
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case CONFIG_EXT_POWER_CTRL:
			if (data->sensor_platform_info->ext_power_ctrl != NULL)
				data->sensor_platform_info->ext_power_ctrl(1);
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		case CONFIG_I2C_MUX:
			if (data->sensor_platform_info->i2c_conf &&
				data->sensor_platform_info->i2c_conf->
				use_i2c_mux)
				msm_sensor_bayer_enable_i2c_mux(
					data->sensor_platform_info->i2c_conf);
			if (power_seq[index].delay)
				usleep_range(power_seq[index].delay * 1000,
					(power_seq[index].delay * 1000) + 1000);
			break;
		default:
			pr_err("%s error power config %d\n", __func__,
				power_seq[index].power_config);
			rc = -EINVAL;
			break;
		}
	}

	return rc;

ERROR:
	for (index--; index >= 0; index--) {
		switch (power_seq[index].power_config) {
		case CONFIG_I2C_MUX:
			if (data->sensor_platform_info->i2c_conf &&
				data->sensor_platform_info->i2c_conf->
				use_i2c_mux)
				msm_sensor_bayer_disable_i2c_mux(
					data->sensor_platform_info->i2c_conf);
			break;
		case CONFIG_EXT_POWER_CTRL:
			if (data->sensor_platform_info->ext_power_ctrl != NULL)
				data->sensor_platform_info->ext_power_ctrl(0);
			break;
		case CONFIG_CLK:
			msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->
				dev, cam_clk_info, s_ctrl->cam_clk,
				ARRAY_SIZE(cam_clk_info), 0);
			break;
		case ENABLE_GPIO:
			msm_camera_config_gpio_table(data, 0);
			break;
		case ENABLE_VREG:
			msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->
				client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 0);
			break;
		case REQUEST_VREG:
			msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->
				client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 0);
			break;
		case REQUEST_GPIO:
			msm_camera_request_gpio_table(data, 0);
			break;
		default:
			pr_err("%s error power config %d\n", __func__,
				power_seq[index].power_config);
			break;
		}
	}
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int32_t msm_sensor_bayer_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t size = 0, index = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct msm_camera_power_seq_t *power_seq = NULL;
	CDBG("%s\n", __func__);

	if (s_ctrl->power_seq) {
		power_seq = s_ctrl->power_seq;
		size = s_ctrl->num_power_seq;
	} else {
		power_seq = &sensor_power_seq[0];
		size = ARRAY_SIZE(sensor_power_seq);
	}

	for (index = (size - 1); index >= 0; index--) {
		switch (power_seq[index].power_config) {
		case CONFIG_I2C_MUX:
			if (data->sensor_platform_info->i2c_conf &&
				data->sensor_platform_info->i2c_conf->
				use_i2c_mux)
				msm_sensor_bayer_disable_i2c_mux(
					data->sensor_platform_info->i2c_conf);
			break;
		case CONFIG_EXT_POWER_CTRL:
			if (data->sensor_platform_info->ext_power_ctrl != NULL)
				data->sensor_platform_info->ext_power_ctrl(0);
			break;
		case CONFIG_CLK:
			msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->
				dev, cam_clk_info, s_ctrl->cam_clk,
				ARRAY_SIZE(cam_clk_info), 0);
			break;
		case ENABLE_GPIO:
			msm_camera_config_gpio_table(data, 0);
			break;
		case ENABLE_VREG:
			msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->
				client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 0);
			break;
		case REQUEST_VREG:
			msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->
				client->dev,
				s_ctrl->sensordata->sensor_platform_info->
				cam_vreg,
				s_ctrl->sensordata->sensor_platform_info->
				num_vreg,
				s_ctrl->vreg_seq,
				s_ctrl->num_vreg_seq,
				s_ctrl->reg_ptr, 0);
			break;
		case REQUEST_GPIO:
			msm_camera_request_gpio_table(data, 0);
			break;
		default:
			pr_err("%s error power config %d\n", __func__,
				power_seq[index].power_config);
			break;
		}
	}
	kfree(s_ctrl->reg_ptr);
	return 0;
}

int32_t msm_sensor_bayer_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensor_id_info->sensor_id);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static int32_t msm_sensor_bayer_eeprom_read(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint32_t reg_addr = 0;
	uint8_t *data = s_ctrl->eeprom_data.data;
	uint32_t num_byte = 0;
	int rc = 0;
	uint32_t i2c_addr;
	struct msm_camera_sensor_info *sensor_info = s_ctrl->sensordata;
	i2c_addr = sensor_info->eeprom_info->eeprom_i2c_slave_addr;
	num_byte = s_ctrl->eeprom_data.length = sensor_info->eeprom_info->
		eeprom_read_length;
	reg_addr = sensor_info->eeprom_info->eeprom_reg_addr;

	data = kzalloc(num_byte * sizeof(uint8_t), GFP_KERNEL);
	if (!data) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl->sensor_i2c_client->client->addr = i2c_addr;
	CDBG("eeprom read: i2c addr is %x num byte %d  reg addr %x\n",
		i2c_addr, num_byte, reg_addr);
	rc = msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, reg_addr, data,
		num_byte);
	s_ctrl->sensor_i2c_client->client->addr = s_ctrl->sensor_i2c_addr;
	return rc;
}

int32_t msm_sensor_bayer_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	CDBG("%s %s_i2c_probe called\n", __func__, client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__, client->name);
		return rc;
	}

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_bayer_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

	if (!s_ctrl->wait_num_frames)
		s_ctrl->wait_num_frames = 1;

	pr_err("%s %s probe succeeded\n", __func__, client->name);
	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	s_ctrl->sensor_v4l2_subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->sensor_v4l2_subdev.entity, 0, NULL, 0);
	s_ctrl->sensor_v4l2_subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->sensor_v4l2_subdev.entity.group_id = SENSOR_DEV;
	s_ctrl->sensor_v4l2_subdev.entity.name =
		s_ctrl->sensor_v4l2_subdev.name;
	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	s_ctrl->sensor_v4l2_subdev.entity.revision =
		s_ctrl->sensor_v4l2_subdev.devnode->num;
	msm_sensor_bayer_eeprom_read(s_ctrl);
	goto power_down;
probe_fail:
	pr_err("%s %s_i2c_probe failed\n", __func__, client->name);
power_down:
	if (rc > 0)
		rc = 0;
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;
}

int32_t msm_sensor_delay_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	CDBG("%s %s_delay_i2c_probe called\n", __func__, client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	if (!s_ctrl->wait_num_frames)
		s_ctrl->wait_num_frames = 1;

	pr_err("%s %s probe succeeded\n", __func__, client->name);
	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	s_ctrl->sensor_v4l2_subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->sensor_v4l2_subdev.entity, 0, NULL, 0);
	s_ctrl->sensor_v4l2_subdev.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->sensor_v4l2_subdev.entity.group_id = SENSOR_DEV;
	s_ctrl->sensor_v4l2_subdev.entity.name =
		s_ctrl->sensor_v4l2_subdev.name;
	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	s_ctrl->sensor_v4l2_subdev.entity.revision =
		s_ctrl->sensor_v4l2_subdev.devnode->num;
	if (rc > 0)
		rc = 0;
	return rc;
}

int32_t msm_sensor_bayer_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (!on)
		rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

int32_t msm_sensor_bayer_v4l2_enum_fmt(struct v4l2_subdev *sd,
	unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

int32_t msm_sensor_bayer_v4l2_s_ctrl(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	struct msm_sensor_v4l2_ctrl_info_t *v4l2_ctrl =
		s_ctrl->msm_sensor_v4l2_ctrl_info;

	CDBG("%s\n", __func__);
	CDBG("%d\n", ctrl->id);
	if (v4l2_ctrl == NULL)
		return rc;
	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (v4l2_ctrl[i].ctrl_id == ctrl->id) {
			if (v4l2_ctrl[i].s_v4l2_ctrl != NULL) {
				CDBG("\n calling msm_sensor_s_ctrl_by_enum\n");
				rc = v4l2_ctrl[i].s_v4l2_ctrl(
					s_ctrl,
					&s_ctrl->msm_sensor_v4l2_ctrl_info[i],
					ctrl->value);
			}
			break;
		}
	}

	return rc;
}

int32_t msm_sensor_bayer_v4l2_query_ctrl(
	struct v4l2_subdev *sd, struct v4l2_queryctrl *qctrl)
{
	int rc = -1, i = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *) sd->dev_priv;

	CDBG("%s\n", __func__);
	CDBG("%s id: %d\n", __func__, qctrl->id);

	if (s_ctrl->msm_sensor_v4l2_ctrl_info == NULL)
		return rc;

	for (i = 0; i < s_ctrl->num_v4l2_ctrl; i++) {
		if (s_ctrl->msm_sensor_v4l2_ctrl_info[i].ctrl_id == qctrl->id) {
			qctrl->minimum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].min;
			qctrl->maximum =
				s_ctrl->msm_sensor_v4l2_ctrl_info[i].max;
			qctrl->flags = 1;
			rc = 0;
			break;
		}
	}

	return rc;
}

int msm_sensor_bayer_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("%s enter\n", __func__);
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
}

