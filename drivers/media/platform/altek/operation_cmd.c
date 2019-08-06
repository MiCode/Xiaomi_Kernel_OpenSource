/*
 * File: operation_cmd.c
 * Description: operation command
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


#include <linux/string.h>
#include <uapi/asm-generic/errno-base.h>

#include "include/error.h"
#include "include/mtype.h"
#include "include/error.h"
#include "include/isp_camera_cmd.h"
#include "include/miniisp.h"
#include "include/ispctrl_if_master.h"


/******Private Constant Definition******/


#define MINI_ISP_LOG_TAG "[operation_cmd]"

extern struct file *g_filp[FIRMWARE_MAX];

/******Public Function******/

/**
 *\brief Mini ISP open
 *\param devdata [In], CMD param
 *\param opcode [In], CMD param
 *\param param [In], CMD param
 *\return Error code
 */
errcode mast_operation_cmd_miniisp_open(void *devdata,
						u16 opcode, u8 *FileNametbl[])
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	char *pBootpath,  /*boot code*/
		*pBasicpath,  /*basic code*/
		*pAdvancedpath, /*advanced code*/
		*pScenariopath,  /*scenario data*/
		*pHdrpath,  /*hdr qmerge data*/
		*pIrp0path, /*irp0 qmerge data*/
		*pIrp1path, /*irp1 qmerge data*/
		*pPPmappath, /*pp map*/
		*pDepthpath; /*depth qmerge data*/

	pBootpath	= FileNametbl[0];
	pBasicpath	= FileNametbl[1];
	pAdvancedpath	= FileNametbl[2];
	pScenariopath	= FileNametbl[3];
	pHdrpath	= FileNametbl[4];
	pIrp0path	= FileNametbl[5];
	pIrp1path	= FileNametbl[6];
	pPPmappath	= FileNametbl[7];
	pDepthpath	= FileNametbl[8];

	misp_info("%s - start", __func__);

	/*open boot code*/
	if (pBootpath) {
		/*
		 *misp_info("%s - boot code, path: %s",
		 *		__func__, pBootpath);
		 */
		err = ispctrl_if_mast_request_firmware(pBootpath,
						BOOT_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open boot code failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}

	/*open basic code*/
	if (pBasicpath) {
		/*
		 *misp_info("%s - basic code, path: %s",
		 *		__func__, pBasicpath);
		 */
		err = ispctrl_if_mast_request_firmware(pBasicpath,
						BASIC_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open basic code failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}

	/*open advanced code*/
	if (pAdvancedpath) {
		/*
		 *misp_info("%s - advanced code, path: %s",
		 *		__func__, pAdvancedpath);
		 */
		err = ispctrl_if_mast_request_firmware(pAdvancedpath,
						ADVANCED_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open advanced code failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}


	/*open scenario data*/
	if (pScenariopath) {
		/*
		 *misp_info("%s - scenario data, path: %s",
		 *		__func__, pScenariopath);
		 */
		err = ispctrl_if_mast_request_firmware(pScenariopath,
						SCENARIO_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open scenario data failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}

	/*open hdr qmerge data*/
	if (pHdrpath) {

		err = ispctrl_if_mast_request_firmware(pHdrpath,
						HDR_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open hdr qmerge data failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}
	/*open irp0 qmerge data*/
	if (pIrp0path) {

		err = ispctrl_if_mast_request_firmware(pIrp0path,
						IRP0_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open irp0 qmerge data failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}
	/*open irp1 qmerge data*/
	if (pIrp1path) {

		err = ispctrl_if_mast_request_firmware(pIrp1path,
						IRP1_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open irp1 qmerge data failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}

	/*open pp map*/
	if (pPPmappath) {

		err = ispctrl_if_mast_request_firmware(pPPmappath,
						PPMAP_CODE);
		if (err != ERR_SUCCESS) {
			misp_err("%s - open PP map failed", __func__);
			goto mast_operation_cmd_miniisp_open_end;
		}
	}

	/*open depth qmerge data*/
	if (pDepthpath) {

		err = ispctrl_if_mast_request_firmware(pDepthpath,
						DEPTH_CODE);
		if (err != ERR_SUCCESS) {
			/* Ignored error for 'No such file or directory' */
			if ((ENOENT + err) == 0) {
				g_filp[DEPTH_CODE] = NULL;
				err = ERR_SUCCESS;
			}
			misp_info("%s - open depth qmerge data failed", __func__);
			/* goto mast_operation_cmd_miniisp_open_end; */
		}
	}

	misp_info("%s end", __func__);

mast_operation_cmd_miniisp_open_end:

	return err;
}

/******Private Function******/





/****** End Of File******/
