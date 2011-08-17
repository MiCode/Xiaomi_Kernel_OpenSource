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

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <media/msm_camera.h>
#include "msm_sensor.h"

/*=============================================================*/

int32_t msm_sensor_i2c_rxdata(struct msm_sensor_ctrl_t *s_ctrl,
	unsigned char *rxdata, int length)
{
	uint16_t saddr = s_ctrl->msm_sensor_client->addr >> 1;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(s_ctrl->msm_sensor_client->adapter, msgs, 2) < 0) {
		CDBG("msm_sensor_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}

int32_t msm_sensor_i2c_txdata(struct msm_sensor_ctrl_t *s_ctrl,
				unsigned char *txdata, int length)
{
	uint16_t saddr = s_ctrl->msm_sensor_client->addr >> 1;
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	if (i2c_transfer(s_ctrl->msm_sensor_client->adapter, msg, 1) < 0) {
		CDBG("msm_sensor_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}

int32_t msm_sensor_i2c_waddr_write_b(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
	CDBG("%s waddr = 0x%x, wdata = 0x%x\n", __func__, waddr, bdata);
	rc = msm_sensor_i2c_txdata(s_ctrl, buf, 3);
	if (rc < 0)
		CDBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_sensor_i2c_waddr_write_w(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint16_t wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00) >> 8;
	buf[3] = (wdata & 0x00FF);
	CDBG("%s waddr = 0x%x, wdata = 0x%x\n", __func__, waddr, wdata);
	rc = msm_sensor_i2c_txdata(s_ctrl, buf, 4);
	if (rc < 0)
		CDBG("%s fail\n", __func__);
	return rc;
}

int32_t msm_sensor_i2c_waddr_write_b_tbl(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_i2c_reg_conf const *reg_conf_tbl, uint8_t size)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < size; i++) {
		rc = msm_sensor_i2c_waddr_write_b(
			s_ctrl,
			reg_conf_tbl->reg_addr,
			reg_conf_tbl->reg_data);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

int32_t msm_sensor_i2c_waddr_write_w_tbl(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_i2c_reg_conf const *reg_conf_tbl, uint8_t size)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < size; i++) {
		rc = msm_sensor_i2c_waddr_write_w(
			s_ctrl,
			reg_conf_tbl->reg_addr,
			reg_conf_tbl->reg_data);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

int32_t msm_sensor_i2c_waddr_read_w(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t waddr, uint16_t *data)
{
	int32_t rc = 0;
	unsigned char buf[2];
	if (!data)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	rc = msm_sensor_i2c_rxdata(s_ctrl, buf, 2);
	if (rc < 0) {
		CDBG("%s fail\n", __func__);
		return rc;
	}
	*data = (buf[0] << 8 | buf[1]);
	CDBG("%s waddr = 0x%x val = 0x%x!\n", __func__,
	waddr, *data);
	return rc;
}

int msm_sensor_write_b_conf_array(struct msm_sensor_ctrl_t *s_ctrl,
			struct msm_sensor_i2c_conf_array *array, uint16_t index)
{
	return msm_sensor_i2c_waddr_write_b_tbl(
		s_ctrl, array[index].conf, array[index].size);
}

int msm_sensor_write_w_conf_array(struct msm_sensor_ctrl_t *s_ctrl,
			struct msm_sensor_i2c_conf_array *array, uint16_t index)
{
	return msm_sensor_i2c_waddr_write_w_tbl(
		s_ctrl, array[index].conf, array[index].size);
}

int msm_sensor_write_b_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	for (i = 0; i < s_ctrl->msm_sensor_reg.init_size; i++) {
		rc = msm_sensor_write_b_conf_array(
			s_ctrl, s_ctrl->msm_sensor_reg.init_settings, i);
		msleep(s_ctrl->msm_sensor_reg.init_settings[i].delay);
		if (rc < 0)
			break;
	}
	return rc;
}

int msm_sensor_write_w_init_settings(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	for (i = 0; i < s_ctrl->msm_sensor_reg.init_size; i++) {
		rc = msm_sensor_write_w_conf_array(
			s_ctrl, s_ctrl->msm_sensor_reg.init_settings, i);
		msleep(s_ctrl->msm_sensor_reg.init_settings[i].delay);
		if (rc < 0)
			break;
	}
	return rc;
}

int msm_sensor_write_b_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int rc = 0;
	rc = msm_sensor_write_b_conf_array(
		s_ctrl, s_ctrl->msm_sensor_reg.res_settings, res);
	msleep(s_ctrl->msm_sensor_reg.res_settings[res].delay);
	return rc;
}

int msm_sensor_write_w_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int rc = 0;
	rc = msm_sensor_write_w_conf_array(
		s_ctrl, s_ctrl->msm_sensor_reg.res_settings, res);
	msleep(s_ctrl->msm_sensor_reg.res_settings[res].delay);
	return rc;
}

uint16_t msm_sensor_read_b_conf_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			enum msm_sensor_resolution_t res, int8_t array_addr)
{
	return
	s_ctrl->msm_sensor_reg.res_settings[res].
	conf[array_addr].reg_data << 8 |
	s_ctrl->msm_sensor_reg.res_settings[res].
	conf[array_addr+1].reg_data;
}

uint16_t msm_sensor_read_w_conf_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			enum msm_sensor_resolution_t res, int8_t array_addr)
{
	return
	s_ctrl->msm_sensor_reg.res_settings[res].
	conf[array_addr].reg_data;
}

void msm_sensor_start_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_sensor_i2c_waddr_write_b_tbl(s_ctrl,
		s_ctrl->msm_sensor_reg.start_stream_conf,
		s_ctrl->msm_sensor_reg.start_stream_conf_size);
}

void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_sensor_i2c_waddr_write_b_tbl(s_ctrl,
		s_ctrl->msm_sensor_reg.stop_stream_conf,
		s_ctrl->msm_sensor_reg.stop_stream_conf_size);
}

void msm_sensor_group_hold_on(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_sensor_i2c_waddr_write_b_tbl(s_ctrl,
		s_ctrl->msm_sensor_reg.group_hold_on_conf,
		s_ctrl->msm_sensor_reg.group_hold_on_conf_size);
}

void msm_sensor_group_hold_off(struct msm_sensor_ctrl_t *s_ctrl)
{
	msm_sensor_i2c_waddr_write_b_tbl(s_ctrl,
		s_ctrl->msm_sensor_reg.group_hold_off_conf,
		s_ctrl->msm_sensor_reg.group_hold_off_conf_size);
}

uint16_t msm_sensor_get_prev_lines_pf(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->prev_frame_length_lines;
}

uint16_t msm_sensor_get_prev_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->prev_line_length_pck;
}

uint16_t msm_sensor_get_pict_lines_pf(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->snap_frame_length_lines;
}

uint16_t msm_sensor_get_pict_pixels_pl(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->snap_line_length_pck;
}

uint32_t msm_sensor_get_pict_max_exp_lc(struct msm_sensor_ctrl_t *s_ctrl)
{
	return s_ctrl->snap_frame_length_lines  * 24;
}

void msm_sensor_get_pict_fps(struct msm_sensor_ctrl_t *s_ctrl,
			uint16_t fps, uint16_t *pfps)
{
	uint32_t divider, d1, d2;
	d1 = s_ctrl->prev_frame_length_lines * Q10 /
		s_ctrl->snap_frame_length_lines;
	d2 = s_ctrl->prev_line_length_pck * Q10 /
		s_ctrl->snap_line_length_pck;
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
		((s_ctrl->prev_frame_length_lines) *
		s_ctrl->fps_divider/Q10);

	rc = msm_sensor_i2c_waddr_write_w(s_ctrl,
				s_ctrl->frame_length_lines_addr,
				total_lines_per_frame);
	return rc;
}

int32_t msm_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	line = (line * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl.sensor_group_hold_on(s_ctrl);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->frame_length_lines_addr, fl_lines);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->coarse_int_time_addr, line);
	msm_sensor_i2c_waddr_write_w(s_ctrl, s_ctrl->global_gain_addr, gain);
	s_ctrl->func_tbl.sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_write_exp_gain2(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, ll_pclk, ll_ratio;
	uint8_t offset;
	fl_lines = s_ctrl->curr_frame_length_lines;
	ll_pclk = s_ctrl->curr_line_length_pck;
	line = (line * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->vert_offset;
	if (line > (fl_lines - offset)) {
		ll_ratio = (line * Q10) / (fl_lines - offset);
		ll_pclk = ll_pclk * ll_ratio;
		line = fl_lines - offset;
	}

	s_ctrl->func_tbl.sensor_group_hold_on(s_ctrl);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->line_length_pck_addr, ll_pclk);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->coarse_int_time_addr, line);
	msm_sensor_i2c_waddr_write_w(s_ctrl, s_ctrl->global_gain_addr, gain);
	s_ctrl->func_tbl.sensor_group_hold_off(s_ctrl);
	return 0;
}

int32_t msm_sensor_set_sensor_mode_b(struct msm_sensor_ctrl_t *s_ctrl,
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
		default:
			rc = -EINVAL;
			break;
		}
		s_ctrl->curr_frame_length_lines =
			msm_sensor_read_b_conf_wdata
			(s_ctrl, res, s_ctrl->frame_length_lines_array_addr);
		s_ctrl->curr_line_length_pck =
			msm_sensor_read_b_conf_wdata
			(s_ctrl, res, s_ctrl->line_length_pck_array_addr);

		if (s_ctrl->func_tbl.sensor_setting
			(s_ctrl, MSM_SENSOR_UPDATE_PERIODIC, res) < 0)
			return rc;
	}
	s_ctrl->curr_res = res;
	return rc;
}

int32_t msm_sensor_set_sensor_mode_w(struct msm_sensor_ctrl_t *s_ctrl,
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
		default:
			rc = -EINVAL;
			break;
		}
		s_ctrl->curr_frame_length_lines =
			msm_sensor_read_w_conf_wdata
			(s_ctrl, res, s_ctrl->frame_length_lines_array_addr);
		s_ctrl->curr_line_length_pck =
			msm_sensor_read_w_conf_wdata
			(s_ctrl, res, s_ctrl->line_length_pck_array_addr);

		if (s_ctrl->func_tbl.sensor_setting
			(s_ctrl, MSM_SENSOR_UPDATE_PERIODIC, res) < 0)
			return rc;
	}
	s_ctrl->curr_res = res;
	return rc;
}

int32_t msm_sensor_mode_init_bdata(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != s_ctrl->cam_mode) {
		if (init_info->prev_res >=
			s_ctrl->msm_sensor_reg.num_conf ||
			init_info->pict_res >=
			s_ctrl->msm_sensor_reg.num_conf) {
			CDBG("Resolution does not exist");
			return -EINVAL;
		}

		s_ctrl->prev_res = init_info->prev_res;
		s_ctrl->pict_res = init_info->pict_res;
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		s_ctrl->prev_frame_length_lines =
			msm_sensor_read_b_conf_wdata(s_ctrl,
				s_ctrl->prev_res,
				s_ctrl->frame_length_lines_array_addr);
		s_ctrl->prev_line_length_pck =
			msm_sensor_read_b_conf_wdata(s_ctrl,
				s_ctrl->prev_res,
				s_ctrl->line_length_pck_array_addr);

		s_ctrl->snap_frame_length_lines =
			msm_sensor_read_b_conf_wdata(s_ctrl,
				s_ctrl->pict_res,
				s_ctrl->frame_length_lines_array_addr);

		s_ctrl->snap_line_length_pck =
			msm_sensor_read_b_conf_wdata(s_ctrl,
				s_ctrl->pict_res,
				s_ctrl->line_length_pck_array_addr);


		rc = s_ctrl->func_tbl.sensor_setting(s_ctrl,
			MSM_SENSOR_REG_INIT, s_ctrl->prev_res);
	}
	return rc;
}

int32_t msm_sensor_mode_init_wdata(struct msm_sensor_ctrl_t *s_ctrl,
			int mode, struct sensor_init_cfg *init_info)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	if (mode != s_ctrl->cam_mode) {
		if (init_info->prev_res >=
			s_ctrl->msm_sensor_reg.num_conf ||
			init_info->pict_res >=
			s_ctrl->msm_sensor_reg.num_conf) {
			CDBG("Resolution does not exist");
			return -EINVAL;
		}

		s_ctrl->prev_res = init_info->prev_res;
		s_ctrl->pict_res = init_info->pict_res;
		s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
		s_ctrl->cam_mode = mode;

		s_ctrl->prev_frame_length_lines =
			msm_sensor_read_w_conf_wdata(s_ctrl,
				s_ctrl->prev_res,
				s_ctrl->frame_length_lines_array_addr);
		s_ctrl->prev_line_length_pck =
			msm_sensor_read_w_conf_wdata(s_ctrl,
				s_ctrl->prev_res,
				s_ctrl->line_length_pck_array_addr);

		s_ctrl->snap_frame_length_lines =
			msm_sensor_read_w_conf_wdata(s_ctrl,
				s_ctrl->pict_res,
				s_ctrl->frame_length_lines_array_addr);

		s_ctrl->snap_line_length_pck =
			msm_sensor_read_w_conf_wdata(s_ctrl,
				s_ctrl->pict_res,
				s_ctrl->line_length_pck_array_addr);


		rc = s_ctrl->func_tbl.sensor_setting(s_ctrl,
			MSM_SENSOR_REG_INIT, s_ctrl->prev_res);
	}
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
			s_ctrl->func_tbl.
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
			cdata.cfg.prevl_pf =
				s_ctrl->func_tbl.
				sensor_get_prev_lines_pf
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				s_ctrl->func_tbl.
				sensor_get_prev_pixels_pl
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				s_ctrl->func_tbl.
				sensor_get_pict_lines_pf
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				s_ctrl->func_tbl.
				sensor_get_pict_pixels_pl
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				s_ctrl->func_tbl.
				sensor_get_pict_max_exp_lc
				(s_ctrl);

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = s_ctrl->func_tbl.
				sensor_set_fps(
				s_ctrl,
				&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				s_ctrl->func_tbl.
				sensor_write_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				s_ctrl->func_tbl.
				sensor_write_exp_gain(
					s_ctrl,
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = s_ctrl->func_tbl.
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
			rc = s_ctrl->func_tbl.
				sensor_mode_init(
				s_ctrl,
				cdata.mode,
				&(cdata.cfg.init_info));
			break;

		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int16_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	rc = msm_sensor_i2c_waddr_read_w(s_ctrl,
			s_ctrl->sensor_id_addr, &chipid);
	CDBG("msm_sensor id: %d\n", chipid);
	if (chipid != s_ctrl->sensor_id) {
		CDBG("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

int msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *this_ctrl;
	CDBG("%s_i2c_probe called\n", client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	this_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	this_ctrl->msm_sensor_client = client;
	return 0;

probe_failure:
	CDBG("%s_i2c_probe failed\n", client->name);
	return rc;
}

int msm_sensor_probe(struct msm_sensor_ctrl_t *s_ctrl,
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(s_ctrl->msm_sensor_i2c_driver);
	if (rc < 0 || s_ctrl->msm_sensor_client == NULL) {
		rc = -ENOTSUPP;
		CDBG("I2C add driver failed");
		goto probe_fail;
	}

	rc = s_ctrl->func_tbl.sensor_power_up(info);
	if (rc < 0)
		goto probe_fail;
	s->s_init = s_ctrl->func_tbl.sensor_open_init;
	s->s_release = s_ctrl->func_tbl.sensor_release;
	s->s_config  = s_ctrl->func_tbl.sensor_config;
	s->s_camera_type = s_ctrl->camera_type;
	s->s_mount_angle = 0;
	s_ctrl->func_tbl.sensor_power_down(info);
	return rc;
probe_fail:
	return rc;
}

int msm_sensor_v4l2_probe(struct msm_sensor_ctrl_t *s_ctrl,
	const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = s_ctrl->func_tbl.sensor_probe(s_ctrl, info, s);
	if (rc < 0)
		return rc;

	s_ctrl->sensor_v4l2_subdev = sdev;
	v4l2_i2c_subdev_init(s_ctrl->sensor_v4l2_subdev,
		s_ctrl->msm_sensor_client, s_ctrl->sensor_v4l2_subdev_ops);
	s_ctrl->sensor_v4l2_subdev->dev_priv = (void *) s_ctrl;
	return rc;
}

int msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
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
		s_ctrl->func_tbl.sensor_start_stream(s_ctrl);
	else
		s_ctrl->func_tbl.sensor_stop_stream(s_ctrl);
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
