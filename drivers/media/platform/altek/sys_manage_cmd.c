/*
 * File:  sys_managec_md.c
 * Description: System manage command
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/10/14; Aaron Chuang; Initial version
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



/* Standard C*/

/* Global Header Files*/
/*#include <osshell.h>*/

#include "include/isp_camera_cmd.h"
/* ISP Ctrl IF slave*/
#include "include/ispctrl_if_master.h"
/* ISP Ctrl IF slave error*/
#include "include/error/ispctrl_if_master_err.h"

/* Local Header Files*/
#include "include/ispctrl_if_master_local.h"



/******Include File******/



/******Private Constant Definition******/


#define MINI_ISP_LOG_TAG "[sys_manage_cmd]"


/******Private Type Declaration******/



/******Private Function Prototype******/

/******Private Global Variable******/



/******Public Global Variable******/

/******Public Function******/

/**
 *\brief Get Status of Last Executed Command
 *\param  devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_get_status_of_last_exec_command(
			void *devdata, u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Parameter size*/
	u32 para_size = sizeof(struct system_cmd_status_of_last_command);



	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode, param, 0);
	if (err  != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_status_of_last_exec_command_end;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata, param,
						para_size, true);
mast_sys_manage_cmd_get_status_of_last_exec_command_end:

	return err;


}

/**
 *\brief Get Error Code Command
 *\param  devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_get_error_code_command(void *devdata,
							u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Parameter size*/
	/*get last ten error code and error status*/
	u32 para_size = (sizeof(errcode))*10;

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode, param, 0);
	if (err  != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_error_code_command_end;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata, param,
							para_size, false);
	if (err  != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_error_code_command_end;

	misp_err("%s last error code %x %x %x %x", __func__, *(param),
		*(param+1), *(param+2), *(param+3));
mast_sys_manage_cmd_get_error_code_command_end:

	return err;
}

/**
 *\brief Set ISP register
 *\param devdata [In], misp_data
 *\param opCode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_set_isp_register(void *devdata,
						u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Parameter size*/
	u32 para_size = sizeof(struct system_cmd_isp_register);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
						param, para_size);
	return err;
}

/**
 *\brief Get ISP register
 *\param  devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_get_isp_register(void *devdata,
						u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Parameter size*/
	u32 para_size = sizeof(struct system_cmd_isp_register);
	/* Response size*/
	u32 *reg_count = (u32 *)&param[4];



	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
						param, para_size);
	if (err  != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_isp_register_end;

	/* Update response size*/
	para_size = sizeof(struct system_cmd_isp_register) + *reg_count*4;

	/* Get data from slave*/
	err = ispctrl_if_mast_recv_isp_register_response_from_slave(
								devdata,
								param,
								&para_size,
								*reg_count);
mast_sys_manage_cmd_get_isp_register_end:

	return err;
}

/**
 *\brief Set common log level
 *\param  devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_set_comomn_log_level(void *devdata,
						u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(struct system_cmd_common_log_level);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
					param, para_size);
	return err;
}

/**
 *\brief Get chip test report
 *\param  devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_get_chip_test_report(void *devdata,
						u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size = 0;

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode,
						param, para_size);
	if (err  != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_chip_test_report_end;

	/* Update response size*/
	para_size = ReportRegCount;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata, param,
						para_size, true);
mast_sys_manage_cmd_get_chip_test_report_end:

	return err;
}

/*
 *\brief Get isp thermal
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_sys_manage_cmd_get_chip_thermal(
						void *devdata,
						u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 para_size = sizeof(u16);

	misp_info("%s - enter", __func__);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(devdata, opcode, NULL, 0);
	if (err != ERR_SUCCESS)
		goto mast_camera_profile_cmd_get_isp_thermal_end;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata,
			param, para_size, true);
	if (err != ERR_SUCCESS) {
		misp_info("%s - err 0x%x", __func__, err);
		goto mast_camera_profile_cmd_get_isp_thermal_end;
	}

	misp_info("Get thermal value 0x%x", *(u16 *)param);

mast_camera_profile_cmd_get_isp_thermal_end:
	return err;
}

/******End Of File******/
