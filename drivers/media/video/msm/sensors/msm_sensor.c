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

#include "msm_sensor.h"

/*=============================================================*/

int32_t msm_sensor_write_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc;
	rc = msm_sensor_write_all_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->init_settings,
		s_ctrl->msm_sensor_reg->init_size);
	return rc;
}

int32_t msm_sensor_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc;
	rc = msm_sensor_write_conf_array(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->mode_settings, res);
	if (rc < 0)
		return rc;

	rc = msm_sensor_write_output_settings(s_ctrl, res);
	return rc;
}

int32_t msm_sensor_write_output_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc = -EFAULT;
	struct msm_camera_i2c_reg_conf dim_settings[] = {
		{s_ctrl->sensor_output_reg_addr->x_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].x_output},
		{s_ctrl->sensor_output_reg_addr->y_output,
			s_ctrl->msm_sensor_reg->
			output_settings[res].y_output},
		{s_ctrl->sensor_output_reg_addr->line_length_pclk,
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk},
		{s_ctrl->sensor_output_reg_addr->frame_length_lines,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines},
	};

	rc = msm_camera_i2c_write_tbl(s_ctrl->sensor_i2c_client, dim_settings,
		ARRAY_SIZE(dim_settings), MSM_CAMERA_I2C_WORD_DATA);
	return rc;
}

void msm_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->start_stream_conf,
		s_ctrl->msm_sensor_reg->start_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->stop_stream_conf,
		s_ctrl->msm_sensor_reg->stop_stream_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_on_conf,
		s_ctrl->msm_sensor_reg->group_hold_on_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

void msm_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_camera_i2c_write_tbl(
		s_ctrl->sensor_i2c_client,
		s_ctrl->msm_sensor_reg->group_hold_off_conf,
		s_ctrl->msm_sensor_reg->group_hold_off_conf_size,
		s_ctrl->msm_sensor_reg->default_data_type);
}

uint16_t msm_sensor_get_prev_lines_pf(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->prev_res].frame_length_lines;
}

uint16_t msm_sensor_get_prev_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->prev_res].line_length_pclk;
}

uint16_t msm_sensor_get_pict_lines_pf(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->pict_res].frame_length_lines;
}

uint16_t msm_sensor_get_pict_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->pict_res].line_length_pclk;
}

uint32_t msm_sensor_get_pict_max_exp_lc(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->prev_res].frame_length_lines * 24;
}

void msm_sensor_get_pict_fps(struct msm_sensor_ctrl_t *s_ctrl,
			uint16_t fps, uint16_t *pfps)
{
	uint32_t divider, d1, d2;
	d1 = s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->prev_res].frame_length_lines * Q10 /
		s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->pict_res].frame_length_lines;

	d2 = s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->prev_res].line_length_pclk * Q10 /
		s_ctrl->msm_sensor_reg->
		output_settings[s_ctrl->pict_res].line_length_pclk;

	divider = d1 * d2 / Q10;
	*pfps = (uint16_t) (fps * divider / Q10);
}

int32_t msm_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	s_ctrl->fps_divider = fps->fps_div;

	total_lines_per_frame = (uint16_t)
		((s_ctrl->curr_frame_length_lines) *
		s_ctrl->fps_divider/Q10);

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			total_lines_per_frame, MSM_CAMERA_I2C_WORD_DATA);
	return rc;
}

int32_t msm_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, ll_pclk, ll_ratio;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pclk = s_ctrl->curr_line_length_pclk;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset)) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pclk = ll_pclk * ll_ratio / Q10;
		line = fl_lines - offset;
	}

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk, ll_pclk,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		msm_sensor_write_res_settings(s_ctrl, res);
		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
			rc = msm_camio_csid_config(
				&s_ctrl->curr_csi_params->csid_params);
			v4l2_subdev_notify(s_ctrl->sensor_v4l2_subdev,
						NOTIFY_CID_CHANGE, NULL);
			mb();
			rc = msm_camio_csiphy_config(
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(30);
	}
	return rc;
}

int32_t msm_sensor_set_sensor_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int mode, int res)
{
	int32_t rc = 0;
	if (s_ctrl->curr_res != res) {
		switch (mode) {
		case SENSOR_PREVIEW_MODE:
			s_ctrl->prev_res = res;
			break;
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:
			s_ctrl->pict_res = res;
			break;
		}
		s_ctrl->curr_frame_length_lines =
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines;

		s_ctrl->curr_line_length_pclk =
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk;

		if (s_ctrl->func_tbl->sensor_setting
			(s_ctrl, MSM_SENSOR_UPDATE_PERIODIC, res) < 0)
			return rc;
		s_ctrl->curr_res = res;
	}

	return rc;
}

int32_t msm_sensor_mode_init(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	s_ctrl->fps = 30*Q8;
	s_ctrl->fps_divider = Q10;
	s_ctrl->cam_mode = MSM_SENSOR_MODE_INVALID;

	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != s_ctrl->cam_mode) {
		if (init_info->prev_res >=
			s_ctrl->msm_sensor_reg->num_conf ||
			init_info->pict_res >=
			s_ctrl->msm_sensor_reg->num_conf) {
			CDBG("Resolution does not exist");
			return -EINVAL;
		}

		s_ctrl->prev_res = init_info->prev_res;
		s_ctrl->pict_res = init_info->pict_res;
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
			MSM_SENSOR_REG_INIT, s_ctrl->prev_res);
	}
	return rc;
}

int32_t msm_sensor_get_output_info(struct msm_sensor_ctrl_t *s_ctrl,
		struct sensor_output_info_t *sensor_output_info)
{
	int rc = 0;
	sensor_output_info->num_info = s_ctrl->msm_sensor_reg->num_conf;
	if (copy_to_user((void *)sensor_output_info->output_info,
		s_ctrl->msm_sensor_reg->output_settings,
		sizeof(struct msm_sensor_output_info_t) *
		s_ctrl->msm_sensor_reg->num_conf))
		rc = -EFAULT;

	return rc;
}

int32_t msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("msm_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			if (s_ctrl->func_tbl->
			sensor_get_pict_fps == NULL) {
				rc = -EFAULT;
				break;
			}
			s_ctrl->func_tbl->
			sensor_get_pict_fps(
				s_ctrl,
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			if (s_ctrl->func_tbl->
			sensor_get_prev_lines_pf == NULL) {
				rc = -EFAULT;
				break;
			}
			cdata.cfg.prevl_pf =
				s_ctrl->func_tbl->
				sensor_get_prev_lines_pf
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			if (s_ctrl->func_tbl->
			sensor_get_prev_pixels_pl == NULL) {
				rc = -EFAULT;
				break;
			}
			cdata.cfg.prevp_pl =
				s_ctrl->func_tbl->
				sensor_get_prev_pixels_pl
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			if (s_ctrl->func_tbl->
			sensor_get_pict_lines_pf == NULL) {
				rc = -EFAULT;
				break;
			}
			cdata.cfg.pictl_pf =
				s_ctrl->func_tbl->
				sensor_get_pict_lines_pf
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			if (s_ctrl->func_tbl->
			sensor_get_pict_pixels_pl == NULL) {
				rc = -EFAULT;
				break;
			}
			cdata.cfg.pictp_pl =
				s_ctrl->func_tbl->
				sensor_get_pict_pixels_pl
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			if (s_ctrl->func_tbl->
			sensor_get_pict_max_exp_lc == NULL) {
				rc = -EFAULT;
				break;
			}
			cdata.cfg.pict_max_exp_lc =
				s_ctrl->func_tbl->
				sensor_get_pict_max_exp_lc
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			if (s_ctrl->func_tbl->
			sensor_set_fps == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_fps(
				s_ctrl,
				&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_exp_gain == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_exp_gain == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			if (s_ctrl->func_tbl->
			sensor_set_sensor_mode == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_set_sensor_mode(
					s_ctrl,
					cdata.mode,
					cdata.rs);
			break;

		case CFG_PWR_DOWN:
			break;

		case CFG_MOVE_FOCUS:
			break;

		case CFG_SET_DEFAULT_FOCUS:
			break;

		case CFG_GET_AF_MAX_STEPS:
			cdata.max_steps = 32;
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_EFFECT:
			break;


		case CFG_SEND_WB_INFO:
			break;

		case CFG_SENSOR_INIT:
			if (s_ctrl->func_tbl->
			sensor_mode_init == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_mode_init(
				s_ctrl,
				cdata.mode,
				&(cdata.cfg.init_info));
			break;

		case CFG_GET_OUTPUT_INFO:
			if (s_ctrl->func_tbl->
			sensor_get_output_info == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->func_tbl->
				sensor_get_output_info(
				s_ctrl,
				&cdata.cfg.output_info);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_EEPROM_DATA:
			if (s_ctrl->sensor_eeprom_client == NULL ||
				s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data == NULL) {
				rc = -EFAULT;
				break;
			}
			rc = s_ctrl->sensor_eeprom_client->
				func_tbl.eeprom_get_data(
				s_ctrl->sensor_eeprom_client,
				&cdata.cfg.eeprom_data);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_eeprom_data_t)))
				rc = -EFAULT;
			break;

		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int32_t msm_sensor_power_up(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	msm_camio_clk_rate_set(MSM_SENSOR_MCLK_24HZ);
	rc = gpio_request(data->sensor_reset, "SENSOR_NAME");
	if (!rc) {
		CDBG("%s: reset sensor\n", __func__);
		gpio_direction_output(data->sensor_reset, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		usleep_range(1000, 2000);
	} else {
		CDBG("%s: gpio request fail", __func__);
	}
	return rc;
}

int32_t msm_sensor_power_down(const struct msm_camera_sensor_info *data)
{
	CDBG("%s\n", __func__);
	gpio_set_value_cansleep(data->sensor_reset, 0);
	usleep_range(1000, 2000);
	gpio_free(data->sensor_reset);
	return 0;
}


int32_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		CDBG("%s: read id failed\n", __func__);
		return rc;
	}

	CDBG("msm_sensor id: %d\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		CDBG("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

int32_t msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *this_ctrl;
	CDBG("%s_i2c_probe called\n", client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		rc = -EFAULT;
		goto probe_failure;
	}

	this_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (this_ctrl->sensor_i2c_client != NULL) {
		this_ctrl->sensor_i2c_client->client = client;
		if (this_ctrl->sensor_i2c_addr != 0)
			this_ctrl->sensor_i2c_client->client->addr =
				this_ctrl->sensor_i2c_addr;
	} else {
		rc = -EFAULT;
	}

probe_failure:
	CDBG("%s_i2c_probe failed\n", client->name);
	return rc;
}

int32_t msm_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	mutex_lock(s_ctrl->msm_sensor_mutex);
	s_ctrl->func_tbl->sensor_power_down(s_ctrl->sensordata);
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	CDBG("%s completed\n", __func__);
	return 0;
}

int32_t msm_sensor_open_init(struct msm_sensor_ctrl_t *s_ctrl,
				const struct msm_camera_sensor_info *data)
{
	if (data)
		s_ctrl->sensordata = data;

	return s_ctrl->func_tbl->sensor_power_up(data);
}

int32_t msm_sensor_probe(struct msm_sensor_ctrl_t *s_ctrl,
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(s_ctrl->sensor_i2c_driver);
	if (rc < 0 || s_ctrl->sensor_i2c_client->client == NULL) {
		rc = -ENOTSUPP;
		CDBG("I2C add driver failed");
		goto probe_fail;
	}

	rc = s_ctrl->func_tbl->sensor_power_up(info);
	if (rc < 0)
		goto probe_fail;

	rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

	if (s_ctrl->sensor_eeprom_client != NULL) {
		struct msm_camera_eeprom_client *eeprom_client =
			s_ctrl->sensor_eeprom_client;
		if (eeprom_client->func_tbl.eeprom_init != NULL &&
			eeprom_client->func_tbl.eeprom_release != NULL) {
			rc = eeprom_client->func_tbl.eeprom_init(
				eeprom_client,
				s_ctrl->sensor_i2c_client->client->adapter);
			if (rc < 0)
				goto probe_fail;

			rc = msm_camera_eeprom_read_tbl(eeprom_client,
			eeprom_client->read_tbl, eeprom_client->read_tbl_size);
			eeprom_client->func_tbl.eeprom_release(eeprom_client);
			if (rc < 0)
				goto probe_fail;
		}
	}

	s->s_init = s_ctrl->func_tbl->sensor_open_init;
	s->s_release = s_ctrl->func_tbl->sensor_release;
	s->s_config  = s_ctrl->func_tbl->sensor_config;
	s->s_camera_type = s_ctrl->camera_type;
	if (info->sensor_platform_info != NULL)
		s->s_mount_angle = info->sensor_platform_info->mount_angle;
	else
		s->s_mount_angle = 0;

	goto power_down;
probe_fail:
	i2c_del_driver(s_ctrl->sensor_i2c_driver);
power_down:
	s_ctrl->func_tbl->sensor_power_down(info);
	return rc;
}

int32_t msm_sensor_v4l2_probe(struct msm_sensor_ctrl_t *s_ctrl,
	const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	int32_t rc = 0;
	rc = s_ctrl->func_tbl->sensor_probe(s_ctrl, info, s);
	if (rc < 0)
		return rc;

	s_ctrl->sensor_v4l2_subdev = sdev;
	v4l2_i2c_subdev_init(s_ctrl->sensor_v4l2_subdev,
		s_ctrl->sensor_i2c_client->client,
		s_ctrl->sensor_v4l2_subdev_ops);
	s_ctrl->sensor_v4l2_subdev->dev_priv = (void *) s_ctrl;
	return rc;
}

int32_t msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *) sd->dev_priv;
	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

static int msm_sensor_debugfs_stream_s(void *data, u64 val)
{
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *) data;
	if (val)
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	else
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_stream, NULL,
			msm_sensor_debugfs_stream_s, "%llu\n");

static int msm_sensor_debugfs_test_s(void *data, u64 val)
{
	CDBG("val: %llu\n", val);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sensor_debugfs_test, NULL,
			msm_sensor_debugfs_test_s, "%llu\n");

int msm_sensor_enable_debugfs(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct dentry *debugfs_base, *sensor_dir;
	debugfs_base = debugfs_create_dir("msm_sensor", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	sensor_dir = debugfs_create_dir
		(s_ctrl->sensordata->sensor_name, debugfs_base);
	if (!sensor_dir)
		return -ENOMEM;

	if (!debugfs_create_file("stream", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_stream))
		return -ENOMEM;

	if (!debugfs_create_file("test", S_IRUGO | S_IWUSR, sensor_dir,
			(void *) s_ctrl, &sensor_debugfs_test))
		return -ENOMEM;

	return 0;
}
