/*
 * File: ispctrl_if_master.h
 * Description: The structure and API definition ISP Ctrl IF Master
 * It,s a header file that define structure and API for ISP Ctrl IF Master
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/09/18; Aaron Chuang; Initial version
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


/**
 * \file	 ispctrl_if_master.h
 * \brief	ISP Ctrl IF Master and API define
 * \version  0.01
 * \author   Aaron Chuang
 * \date	 2013/09/17
 * \see	  ispctrl_if_master.h
 */

#ifndef _ISPCTRLIF_MASTER_H_
#define _ISPCTRLIF_MASTER_H_

/**
 *@addtogroup ispctrl_if_master
 *@{
 */

/******Include File******/
#include <linux/interrupt.h>

#include "mtype.h"
#include "moduleid_pj.h"
#include "miniisp.h"

/******Public Constant Definition******/

enum firmware_type {
	BOOT_CODE,
	BASIC_CODE,
	ADVANCED_CODE,
	SCENARIO_CODE,
	HDR_CODE,
	IRP0_CODE,
	IRP1_CODE,
	PPMAP_CODE,
	DEPTH_CODE,
	FIRMWARE_MAX
};

extern int g_isMiniISP_sendboot;



/******Public Type Declaration******/


/******Public Function Prototype******/


/**
 *\brief Execute master command
 *\param opcode [In], Op code
 *\param param  [In], CMD param buffer
 *\return Error code
 */
extern errcode ispctrl_if_mast_execute_cmd(u16 opcode, u8 *param);

/**
 *\brief Send command to slave
 *\param devdata [In], misp_data
 *\param opcode  [In], Op code
 *\param param   [In], CMD param buffer
 *\param len	 [In], CMD param size
 *\return Error code
 */
errcode ispctrl_mast_send_cmd_to_slave(void *devdata,
						u16 opcode,
						u8 *param,
						u32 len);

/**
 *\brief Receive response from slave
 *\param devdata [In], misp_data
 *\param param   [Out], Response buffer
 *\param len	 [Out], Response size
 *\return Error code
 */
errcode ispctrl_mast_recv_response_from_slave(void *devdata,
							u8 *param,
							u32 len,
							bool wait_int);

/**
 *\brief Receive Memory data from slave
 *\param devdata [In], misp_data
 *\param response_buf [Out], Response buffer
 *\param response_size [Out], Response size
 *\param wait_int [In], waiting INT flag
 *\return Error code
 */
errcode ispctrl_if_mast_recv_memory_data_from_slave(
							void *devdata,
							u8 *response_buf,
							u32 *response_size,
							u32 block_size,
							bool wait_int);

#if ENABLE_LINUX_FW_LOADER
/** \brief  Master send bulk (large data) to slave
 *\param devdata [In], misp_data
 *\param buffer  [In], Data buffer to be sent, address 8-byte alignment
 *\param filp	[In], file pointer, used to read the file and send the data
 *\param total_size [In], file size
 *\param block_size  [In], transfer buffer block size
 *\param is_raw   [In], true: mini boot code  false: other files
 *\return Error code
 */
errcode ispctrl_if_mast_send_bulk(void *devdata, const u8 *buffer,
					u32 total_size,
					u32 block_size, bool is_raw);

#else
/** \brief  Master send bulk (large data) to slave
 *\param devdata [In], misp_data
 *\param buffer  [In], Data buffer to be sent, address 8-byte alignment
 *\param filp	[In], file pointer, used to read the file and send the data
 *\param total_size [In], file size
 *\param block_size  [In], transfer buffer block size
 *\param is_raw   [In], true: mini boot code  false: other files
 *\return Error code
 */
errcode ispctrl_if_mast_send_bulk(void *devdata, u8 *buffer,
					struct file *filp, u32 total_size,
					u32 block_size, bool is_raw);
#endif
/** \brief  Master open the firmware
 *\param filename [In], file name of the firmware
 *\param firmwaretype [In], firmware type
 *\return Error code
 */
errcode ispctrl_if_mast_request_firmware(u8 *filename, u8 a_firmwaretype);

/** \brief  Master get SPI status bytes
 *\param  n/a
 *\return status bytes(2 bytes)
 */
u16 ispctrl_if_mast_read_spi_status(void);

/**
 *\brief Receive ISP register response from slave
 *\param devdata [In] misp_data
 *\param response_buf [Out], Response buffer
 *\param response_size [Out], Response size
 *\param total_count [In], Total reg count
 *\return Error code
 */
errcode ispctrl_if_mast_recv_isp_register_response_from_slave(
						void *devdata,
						u8 *response_buf,
						u32 *response_size,
						u32 total_count);

/**
 *\brief Initial purpose
 *\param None
 *\return Error code
 */
void isp_mast_camera_profile_para_init(void);

/****** End of File******/

/**
 *@}
 */

#endif /* _ISPCTRLIF_MASTER_H_*/
