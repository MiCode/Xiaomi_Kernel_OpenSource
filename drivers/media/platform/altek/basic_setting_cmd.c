/*
 * File: basic_setting_cmd.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/10/14; Aaron Chuang; Initial version
 *  2013/12/05; Bruce Chung; 2nd version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */


/******Include File******/
#include "include/isp_camera_cmd.h"
#include "include/ispctrl_if_master.h"
#include "include/error/ispctrl_if_master_err.h"
#include "include/miniisp.h"
#include "include/ispctrl_if_master_local.h"

/******Private Constant Definition******/
#define MINI_ISP_LOG_TAG "[[miniisp]Basic_setting_cmd]"

/******Private Type Declaration******/

/******Private Function Prototype******/

/******Private Global Variable******/

/******Public Global Variable******/

/******Public Function******/

/*
 *\brief Set Depth 3A Info
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_basic_setting_cmd_set_depth_3a_info(void *devdata,
							u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_depth_3a_info);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Set Depth auto interleave mode
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode
mast_basic_setting_cmd_set_depth_auto_interleave_mode(
	void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_depth_auto_interleave_param);
	struct isp_cmd_depth_auto_interleave_param *depth_auto_interleave_param;

	depth_auto_interleave_param =
		(struct isp_cmd_depth_auto_interleave_param *)param;

	misp_info("%s - enter", __func__);
	misp_info("[onoff]: %d, [skip_frame]: %d, ",
	depth_auto_interleave_param->depth_interleave_mode_on_off,
	depth_auto_interleave_param->skip_frame_num_after_illuminator_pulse);

	misp_info("[projector_Lv]: %d, [flood_Lv]: %d",
		depth_auto_interleave_param->projector_power_level,
		depth_auto_interleave_param->illuminator_power_level);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Set projector interleave mode with depth type
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode
mast_basic_setting_cmd_set_interleave_mode_depth_type(
	void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_interleave_mode_depth_type);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Set depth polish level
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_basic_setting_cmd_set_depth_polish_level(
	void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_depth_polish_level);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Set exposure param
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_basic_setting_cmd_set_exposure_param(
	void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_exposure_param);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/*
 *\brief Set depth stream size
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_basic_setting_cmd_set_depth_stream_size(
	void *devdata, u16 opcode, u8 *param)
{
	/*Error Code */
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct isp_cmd_depth_stream_size);

	/*Send command to slave */
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}
/******End Of File******/
