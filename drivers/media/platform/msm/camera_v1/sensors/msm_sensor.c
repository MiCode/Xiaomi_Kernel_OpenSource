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

#include "msm_sensor.h"
#include "msm.h"
#include "msm_ispif.h"
#include "msm_camera_i2c_mux.h"

/*=============================================================*/
int32_t msm_sensor_adjust_frame_lines1(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	uint16_t cur_line = 0;
	uint16_t exp_fl_lines = 0;
	if (s_ctrl->sensor_exp_gain_info) {
		if (s_ctrl->prev_gain && s_ctrl->prev_line &&
			s_ctrl->func_tbl->sensor_write_exp_gain)
			s_ctrl->func_tbl->sensor_write_exp_gain(
				s_ctrl,
				s_ctrl->prev_gain,
				s_ctrl->prev_line);

		msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			&cur_line,
			MSM_CAMERA_I2C_WORD_DATA);
		exp_fl_lines = cur_line +
			s_ctrl->sensor_exp_gain_info->vert_offset;
		if (exp_fl_lines > s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->
				frame_length_lines,
				exp_fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);
		CDBG("%s cur_fl_lines %d, exp_fl_lines %d\n", __func__,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines,
			exp_fl_lines);
	}
	return 0;
}

int32_t msm_sensor_adjust_frame_lines2(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	uint16_t cur_line = 0;
	uint16_t exp_fl_lines = 0;
	uint8_t int_time[3];
	if (s_ctrl->sensor_exp_gain_info) {
		if (s_ctrl->prev_gain && s_ctrl->prev_line &&
			s_ctrl->func_tbl->sensor_write_exp_gain)
			s_ctrl->func_tbl->sensor_write_exp_gain(
				s_ctrl,
				s_ctrl->prev_gain,
				s_ctrl->prev_line);

		msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr-1,
			&int_time[0], 3);
		cur_line |= int_time[0] << 12;
		cur_line |= int_time[1] << 4;
		cur_line |= int_time[2] >> 4;
		exp_fl_lines = cur_line +
			s_ctrl->sensor_exp_gain_info->vert_offset;
		if (exp_fl_lines > s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines)
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_output_reg_addr->
				frame_length_lines,
				exp_fl_lines,
				MSM_CAMERA_I2C_WORD_DATA);
		CDBG("%s cur_line %x cur_fl_lines %x, exp_fl_lines %x\n",
			__func__,
			cur_line,
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines,
			exp_fl_lines);
	}
	return 0;
}

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
	if (rc < 0)
		return rc;

	if (s_ctrl->func_tbl->sensor_adjust_frame_lines)
		rc = s_ctrl->func_tbl->sensor_adjust_frame_lines(s_ctrl, res);

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

int32_t msm_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
	s_ctrl->fps_divider = fps->fps_div;

	return 0;
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

int32_t msm_sensor_setting1(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
		msleep(30);
		if (!csi_config) {
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			CDBG("CSI config in progress\n");
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
				s_ctrl->curr_csic_params);
			CDBG("CSI config is done\n");
			mb();
			msleep(30);
			csi_config = 1;
		}
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE,
			&s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);

		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
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
			s_ctrl->curr_csi_params->csid_params.lane_assign =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_assign;
			s_ctrl->curr_csi_params->csiphy_params.lane_mask =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_mask;
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
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
		s_ctrl->curr_frame_length_lines =
			s_ctrl->msm_sensor_reg->
			output_settings[res].frame_length_lines;

		s_ctrl->curr_line_length_pclk =
			s_ctrl->msm_sensor_reg->
			output_settings[res].line_length_pclk;

		if (s_ctrl->is_csic ||
			!s_ctrl->sensordata->csi_if)
			rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
				MSM_SENSOR_UPDATE_PERIODIC, res);
		else
			rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
				MSM_SENSOR_UPDATE_PERIODIC, res);
		if (rc < 0)
			return rc;
		s_ctrl->curr_res = res;
	}

	return rc;
}

int32_t msm_sensor_mode_init(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	s_ctrl->fps_divider = Q10;
	s_ctrl->cam_mode = MSM_SENSOR_MODE_INVALID;

	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != s_ctrl->cam_mode) {
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		if (s_ctrl->is_csic ||
			!s_ctrl->sensordata->csi_if)
			rc = s_ctrl->func_tbl->sensor_csi_setting(s_ctrl,
				MSM_SENSOR_REG_INIT, 0);
		else
			rc = s_ctrl->func_tbl->sensor_setting(s_ctrl,
				MSM_SENSOR_REG_INIT, 0);
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

int32_t msm_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	long fps = 0;
	uint32_t delay = 0;
	CDBG("%s called\n", __func__);
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	if (s_ctrl->curr_res != MSM_SENSOR_INVALID_RES) {
		fps = s_ctrl->msm_sensor_reg->
			output_settings[s_ctrl->curr_res].vt_pixel_clk /
			s_ctrl->curr_frame_length_lines /
			s_ctrl->curr_line_length_pclk;
		delay = 1000 / fps;
		CDBG("%s fps = %ld, delay = %d\n", __func__, fps, delay);
		msleep(delay);
	}
	return 0;
}

long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_DOWN)
		return -EINVAL;
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
		return msm_sensor_release(s_ctrl);
	case VIDIOC_MSM_SENSOR_CSID_INFO: {
		struct msm_sensor_csi_info *csi_info =
			(struct msm_sensor_csi_info *)arg;
		s_ctrl->csid_version = csi_info->csid_version;
		s_ctrl->is_csic = csi_info->is_csic;
		return 0;
	}
	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_sensor_get_csi_params(struct msm_sensor_ctrl_t *s_ctrl,
		struct csi_lane_params_t *sensor_output_info)
{
	sensor_output_info->csi_lane_assign = s_ctrl->sensordata->
		sensor_platform_info->csi_lane_params->csi_lane_assign;
	sensor_output_info->csi_lane_mask = s_ctrl->sensordata->
		sensor_platform_info->csi_lane_params->csi_lane_mask;
	sensor_output_info->csi_if = s_ctrl->sensordata->csi_if;
	sensor_output_info->csid_core = s_ctrl->sensordata->
			pdata[0].csid_core;
	sensor_output_info->csid_version = s_ctrl->csid_version;
	return 0;
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
			s_ctrl->prev_gain = cdata.cfg.exp_gain.gain;
			s_ctrl->prev_line = cdata.cfg.exp_gain.line;
			break;

		case CFG_SET_PICT_EXP_GAIN:
			if (s_ctrl->func_tbl->
			sensor_write_snapshot_exp_gain == NULL) {
				rc = -EFAULT;
				break;
			}
			rc =
				s_ctrl->func_tbl->
				sensor_write_snapshot_exp_gain(
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

		case CFG_SET_EFFECT:
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

		case CFG_START_STREAM:
			if (s_ctrl->func_tbl->sensor_start_stream == NULL) {
				rc = -EFAULT;
				break;
			}
			s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
			break;

		case CFG_STOP_STREAM:
			if (s_ctrl->func_tbl->sensor_stop_stream == NULL) {
				rc = -EFAULT;
				break;
			}
			s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
			break;

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

int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
				VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

static int32_t msm_sensor_init_flash_data(struct device_node *of_node,
	struct  msm_camera_sensor_info *sensordata)
{
	int32_t rc = 0;
	uint32_t val = 0;

	sensordata->flash_data = kzalloc(sizeof(
		struct msm_camera_sensor_flash_data), GFP_KERNEL);
	if (!sensordata->flash_data) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	rc = of_property_read_u32(of_node, "flash_type", &val);
	CDBG("%s flash_type %d, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	sensordata->flash_data->flash_type = val;
	return rc;
ERROR:
	kfree(sensordata->flash_data);
	return rc;
}

static int32_t msm_sensor_init_vreg_data(struct device_node *of_node,
	struct msm_camera_sensor_platform_info *pinfo)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	count = of_property_count_strings(of_node, "cam_vreg_name");
	CDBG("%s cam_vreg_name count %d\n", __func__, count);

	if (!count)
		return 0;

	pinfo->cam_vreg = kzalloc(sizeof(struct camera_vreg_t) * count,
		GFP_KERNEL);
	if (!pinfo->cam_vreg) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	pinfo->num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node, "cam_vreg_name", i,
			&pinfo->cam_vreg[i].reg_name);
		CDBG("%s reg_name[%d] = %s\n", __func__, i,
			pinfo->cam_vreg[i].reg_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "cam_vreg_type", val_array,
		count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		pinfo->cam_vreg[i].type = val_array[i];
		CDBG("%s cam_vreg[%d].type = %d\n", __func__, i,
			pinfo->cam_vreg[i].type);
	}

	rc = of_property_read_u32_array(of_node, "cam_vreg_min_voltage",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		pinfo->cam_vreg[i].min_voltage = val_array[i];
		CDBG("%s cam_vreg[%d].min_voltage = %d\n", __func__,
			i, pinfo->cam_vreg[i].min_voltage);
	}

	rc = of_property_read_u32_array(of_node, "cam_vreg_max_voltage",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		pinfo->cam_vreg[i].max_voltage = val_array[i];
		CDBG("%s cam_vreg[%d].max_voltage = %d\n", __func__,
			i, pinfo->cam_vreg[i].max_voltage);
	}

	rc = of_property_read_u32_array(of_node, "cam_vreg_op_mode", val_array,
		count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		pinfo->cam_vreg[i].op_mode = val_array[i];
		CDBG("%s cam_vreg[%d].op_mode = %d\n", __func__, i,
			pinfo->cam_vreg[i].op_mode);
	}

	kfree(val_array);
	return rc;
ERROR2:
	kfree(val_array);
ERROR1:
	kfree(pinfo->cam_vreg);
	pinfo->num_vreg = 0;
	return rc;
}

static int32_t msm_sensor_init_gpio_common_tbl_data(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "gpio_common_tbl_num", &count))
		return 0;

	count /= sizeof(uint32_t);

	if (!count)
		return 0;

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_common_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_common_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_common_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio_common_tbl_num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_common_tbl[i].gpio = val_array[i];
		CDBG("%s cam_gpio_common_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_common_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio_common_tbl_flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_common_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_common_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_common_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"gpio_common_tbl_label", i,
			&gconf->cam_gpio_common_tbl[i].label);
		CDBG("%s cam_gpio_common_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_common_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_common_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_common_tbl_size = 0;
	return rc;
}

static int32_t msm_sensor_init_gpio_req_tbl_data(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "gpio_req_tbl_num", &count))
		return 0;

	count /= sizeof(uint32_t);

	if (!count)
		return 0;

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio_req_tbl_num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].gpio = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio_req_tbl_flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"gpio_req_tbl_label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_req_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;
	return rc;
}

static int32_t msm_sensor_init_gpio_set_tbl_data(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "gpio_set_tbl_num", &count))
		return 0;

	count /= sizeof(uint32_t);

	if (!count)
		return 0;

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_set_tbl = kzalloc(sizeof(struct msm_gpio_set_tbl) *
		count, GFP_KERNEL);
	if (!gconf->cam_gpio_set_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_set_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio_set_tbl_num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_set_tbl[i].gpio = val_array[i];
		CDBG("%s cam_gpio_set_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio_set_tbl_flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_set_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_set_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].flags);
	}

	rc = of_property_read_u32_array(of_node, "gpio_set_tbl_delay",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_set_tbl[i].delay = val_array[i];
		CDBG("%s cam_gpio_set_tbl[%d].delay = %d\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].delay);
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_set_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_set_tbl_size = 0;
	return rc;
}

static int32_t msm_sensor_init_gpio_tlmm_tbl_data(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;
	struct gpio_tlmm_cfg *tlmm_cfg = NULL;

	if (!of_get_property(of_node, "gpio_tlmm_table_num", &count))
		return 0;

	count /= sizeof(uint32_t);

	if (!count)
		return 0;

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	tlmm_cfg = kzalloc(sizeof(struct gpio_tlmm_cfg) * count, GFP_KERNEL);
	if (!tlmm_cfg) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	gconf->camera_off_table = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!gconf->camera_off_table) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR2;
	}
	gconf->camera_off_table_size = count;

	gconf->camera_on_table = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!gconf->camera_on_table) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR3;
	}
	gconf->camera_on_table_size = count;

	rc = of_property_read_u32_array(of_node, "gpio_tlmm_table_num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR4;
	}
	for (i = 0; i < count; i++) {
		tlmm_cfg[i].gpio = val_array[i];
		CDBG("%s tlmm_cfg[%d].gpio = %d\n", __func__, i,
			tlmm_cfg[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio_tlmm_table_dir",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR4;
	}
	for (i = 0; i < count; i++) {
		tlmm_cfg[i].dir = val_array[i];
		CDBG("%s tlmm_cfg[%d].dir = %d\n", __func__, i,
			tlmm_cfg[i].dir);
	}

	rc = of_property_read_u32_array(of_node, "gpio_tlmm_table_pull",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR4;
	}
	for (i = 0; i < count; i++) {
		tlmm_cfg[i].pull = val_array[i];
		CDBG("%s tlmm_cfg[%d].pull = %d\n", __func__, i,
			tlmm_cfg[i].pull);
	}

	rc = of_property_read_u32_array(of_node, "gpio_tlmm_table_drvstr",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR4;
	}
	for (i = 0; i < count; i++) {
		tlmm_cfg[i].drvstr = val_array[i];
		CDBG("%s tlmm_cfg[%d].drvstr = %d\n", __func__, i,
			tlmm_cfg[i].drvstr);
	}

	for (i = 0; i < count; i++) {
		gconf->camera_off_table[i] = GPIO_CFG(tlmm_cfg[i].gpio,
			0, tlmm_cfg[i].dir, tlmm_cfg[i].pull,
			tlmm_cfg[i].drvstr);
		gconf->camera_on_table[i] = GPIO_CFG(tlmm_cfg[i].gpio,
			1, tlmm_cfg[i].dir, tlmm_cfg[i].pull,
			tlmm_cfg[i].drvstr);
	}

	kfree(tlmm_cfg);
	kfree(val_array);
	return rc;

ERROR4:
	kfree(gconf->camera_on_table);
ERROR3:
	kfree(gconf->camera_off_table);
ERROR2:
	kfree(tlmm_cfg);
ERROR1:
	kfree(val_array);
	gconf->camera_off_table_size = 0;
	gconf->camera_on_table_size = 0;
	return rc;
}

static int32_t msm_sensor_init_csi_data(struct device_node *of_node,
	struct  msm_camera_sensor_info *sensordata)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0, val = 0;
	uint32_t *val_array = NULL;
	struct msm_camera_sensor_platform_info *pinfo =
		sensordata->sensor_platform_info;

	rc = of_property_read_u32(of_node, "csi_if", &count);
	CDBG("%s csi_if %d, rc %d\n", __func__, count, rc);
	if (rc < 0 || !count)
		return rc;
	sensordata->csi_if = count;

	sensordata->pdata = kzalloc(sizeof(
		struct msm_camera_device_platform_data) * count, GFP_KERNEL);
	if (!sensordata->pdata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "csid_core", val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->pdata[i].csid_core = val_array[i];
		CDBG("%s csid_core[%d].csid_core = %d\n", __func__, i,
			sensordata->pdata[i].csid_core);
	}

	rc = of_property_read_u32_array(of_node, "is_vpe", val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->pdata[i].is_vpe = val_array[i];
		CDBG("%s csid_core[%d].is_vpe = %d\n", __func__, i,
			sensordata->pdata[i].is_vpe);
	}

	pinfo->csi_lane_params = kzalloc(
		sizeof(struct msm_camera_csi_lane_params), GFP_KERNEL);
	if (!pinfo->csi_lane_params) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR2;
	}

	rc = of_property_read_u32(of_node, "csi_lane_assign", &val);
	CDBG("%s csi_lane_assign %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR3;
	}
	pinfo->csi_lane_params->csi_lane_assign = val;

	rc = of_property_read_u32(of_node, "csi_lane_mask", &val);
	CDBG("%s csi_lane_mask %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR3;
	}
	pinfo->csi_lane_params->csi_lane_mask = val;

	kfree(val_array);
	return rc;
ERROR3:
	kfree(pinfo->csi_lane_params);
ERROR2:
	kfree(val_array);
ERROR1:
	kfree(sensordata->pdata);
	sensordata->csi_if = 0;
	return rc;
}
static int32_t msm_sensor_init_actuator_data(struct device_node *of_node,
	struct  msm_camera_sensor_info *sensordata)
{
	int32_t rc = 0;
	uint32_t val = 0;

	rc = of_property_read_u32(of_node, "actuator_cam_name", &val);
	CDBG("%s actuator_cam_name %d, rc %d\n", __func__, val, rc);
	if (rc < 0)
		return 0;

	sensordata->actuator_info = kzalloc(sizeof(struct msm_actuator_info),
		GFP_KERNEL);
	if (!sensordata->actuator_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}

	sensordata->actuator_info->cam_name = val;

	rc = of_property_read_u32(of_node, "actuator_vcm_pwd", &val);
	CDBG("%s actuator_vcm_pwd %d, rc %d\n", __func__, val, rc);
	if (!rc)
		sensordata->actuator_info->vcm_pwd = val;

	rc = of_property_read_u32(of_node, "actuator_vcm_enable", &val);
	CDBG("%s actuator_vcm_enable %d, rc %d\n", __func__, val, rc);
	if (!rc)
		sensordata->actuator_info->vcm_enable = val;

	return 0;
ERROR:
	return rc;
}

static int32_t msm_sensor_init_sensor_data(struct platform_device *pdev,
	struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t val = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct msm_camera_sensor_platform_info *pinfo = NULL;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct msm_camera_sensor_info *sensordata = NULL;

	s_ctrl->sensordata = kzalloc(sizeof(struct msm_camera_sensor_info),
		GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;
	rc = of_property_read_string(of_node, "sensor_name",
		&sensordata->sensor_name);
	CDBG("%s sensor_name %s, rc %d\n", __func__,
		sensordata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "camera_type", &val);
	CDBG("%s camera_type %d, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	sensordata->camera_type = val;

	rc = of_property_read_u32(of_node, "sensor_type", &val);
	CDBG("%s sensor_type %d, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	sensordata->sensor_type = val;

	rc = msm_sensor_init_flash_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	sensordata->sensor_platform_info = kzalloc(sizeof(
		struct msm_camera_sensor_platform_info), GFP_KERNEL);
	if (!sensordata->sensor_platform_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	pinfo = sensordata->sensor_platform_info;

	rc = of_property_read_u32(of_node, "mount_angle", &pinfo->mount_angle);
	CDBG("%s mount_angle %d, rc %d\n", __func__, pinfo->mount_angle, rc);
	if (rc < 0) {
		/* Set default mount angle */
		pinfo->mount_angle = 0;
		rc = 0;
	}

	rc = msm_sensor_init_csi_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}

	rc = msm_sensor_init_vreg_data(of_node, pinfo);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR3;
	}

	pinfo->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
		GFP_KERNEL);
	if (!pinfo->gpio_conf) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR4;
	}
	gconf = pinfo->gpio_conf;
	rc = of_property_read_u32(of_node, "gpio_no_mux", &gconf->gpio_no_mux);
	CDBG("%s gconf->gpio_no_mux %d, rc %d\n", __func__,
		gconf->gpio_no_mux, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR5;
	}

	rc = msm_sensor_init_gpio_common_tbl_data(of_node, gconf);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR5;
	}

	rc = msm_sensor_init_gpio_req_tbl_data(of_node, gconf);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR6;
	}

	rc = msm_sensor_init_gpio_set_tbl_data(of_node, gconf);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR7;
	}

	rc = msm_sensor_init_gpio_tlmm_tbl_data(of_node, gconf);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR8;
	}

	rc = msm_sensor_init_actuator_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR9;
	}

	return rc;

ERROR9:
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		camera_on_table);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		camera_off_table);
ERROR8:
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_set_tbl);
ERROR7:
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_req_tbl);
ERROR6:
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_common_tbl);
ERROR5:
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf);
ERROR4:
	kfree(s_ctrl->sensordata->sensor_platform_info->cam_vreg);
ERROR3:
	kfree(s_ctrl->sensordata->sensor_platform_info->csi_lane_params);
	kfree(s_ctrl->sensordata->pdata);
ERROR2:
	kfree(s_ctrl->sensordata->sensor_platform_info);
	kfree(s_ctrl->sensordata->flash_data);
ERROR1:
	kfree(s_ctrl->sensordata);
	return rc;
}

int32_t msm_sensor_free_sensor_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (!s_ctrl->pdev)
		return 0;
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		camera_on_table);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		camera_off_table);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_set_tbl);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf->
		cam_gpio_common_tbl);
	kfree(s_ctrl->sensordata->sensor_platform_info->gpio_conf);
	kfree(s_ctrl->sensordata->sensor_platform_info->cam_vreg);
	kfree(s_ctrl->sensordata->sensor_platform_info->csi_lane_params);
	kfree(s_ctrl->sensordata->pdata);
	kfree(s_ctrl->sensordata->sensor_platform_info);
	kfree(s_ctrl->sensordata->flash_data);
	kfree(s_ctrl->sensordata);
	return 0;
}

int32_t msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s: %d\n", __func__, __LINE__);
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	usleep_range(1000, 2000);
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	if (s_ctrl->sensor_i2c_client->cci_client) {
		rc = msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto cci_init_failed;
		}
	}
	return rc;

cci_init_failed:
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
}

int32_t msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s\n", __func__);
	if (s_ctrl->sensor_i2c_client->cci_client) {
		msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_RELEASE);
	}

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
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
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s msm_sensor id: %x, exp id: %x\n", __func__, chipid,
		s_ctrl->sensor_id_info->sensor_id);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(sd, struct msm_sensor_ctrl_t, sensor_v4l2_subdev);
}

int32_t msm_sensor_i2c_probe(struct i2c_client *client,
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
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

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
	goto power_down;
probe_fail:
	pr_err("%s %s_i2c_probe failed\n", __func__, client->name);
power_down:
	if (rc > 0)
		rc = 0;
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	return rc;
}

static int msm_sensor_subdev_match_core(struct device *dev, void *data)
{
	int core_index = (int)data;
	struct platform_device *pdev = to_platform_device(dev);
	CDBG("%s cci pdev %p\n", __func__, pdev);
	if (pdev->id == core_index)
		return 1;
	else
		return 0;
}

int32_t msm_sensor_platform_probe(struct platform_device *pdev, void *data)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *)data;
	struct device_driver *driver;
	struct device *dev;
	s_ctrl->pdev = pdev;
	CDBG("%s called data %p\n", __func__, data);
	if (pdev->dev.of_node) {
		rc = msm_sensor_init_sensor_data(pdev, s_ctrl);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
	}
	s_ctrl->sensor_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client->cci_client) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}
	driver = driver_find(MSM_CCI_DRV_NAME, &platform_bus_type);
	if (!driver) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}

	dev = driver_find_device(driver, NULL, 0,
				msm_sensor_subdev_match_core);
	if (!dev) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}
	s_ctrl->sensor_i2c_client->cci_client->cci_subdev =
		dev_get_drvdata(dev);
	CDBG("%s sd %p\n", __func__,
		s_ctrl->sensor_i2c_client->cci_client->cci_subdev);
	s_ctrl->sensor_i2c_client->cci_client->cci_i2c_master = MASTER_0;
	s_ctrl->sensor_i2c_client->cci_client->sid =
		s_ctrl->sensor_i2c_addr >> 1;
	s_ctrl->sensor_i2c_client->cci_client->retries = 0;
	s_ctrl->sensor_i2c_client->cci_client->id_map = 0;

	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__,
			pdev->id_entry->name);
		return rc;
	}

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

	v4l2_subdev_init(&s_ctrl->sensor_v4l2_subdev,
		s_ctrl->sensor_v4l2_subdev_ops);
	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_set_subdevdata(&s_ctrl->sensor_v4l2_subdev, pdev);
	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	goto power_down;
probe_fail:
	pr_err("%s %s probe failed\n", __func__, pdev->id_entry->name);
power_down:
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;
}

int32_t msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (on) {
		rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		if (rc < 0) {
			pr_err("%s: %s power_up failed rc = %d\n", __func__,
				s_ctrl->sensordata->sensor_name, rc);
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
		} else {
			if (s_ctrl->func_tbl->sensor_match_id)
				rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
			else
				rc = msm_sensor_match_id(s_ctrl);
			if (rc < 0) {
				pr_err("%s: %s match_id failed  rc=%d\n",
					__func__,
					s_ctrl->sensordata->sensor_name, rc);
				if (s_ctrl->func_tbl->sensor_power_down(s_ctrl)
					< 0)
					pr_err("%s: %s power_down failed\n",
					__func__,
					s_ctrl->sensordata->sensor_name);
				s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
		}
	} else {
		rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

int32_t msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

int32_t msm_sensor_v4l2_s_ctrl(struct v4l2_subdev *sd,
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

int32_t msm_sensor_v4l2_query_ctrl(
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

int msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	CDBG("%s enter\n", __func__);
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	return rc;
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
