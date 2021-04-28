/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "hal_api.h"

VAL_RESULT_T eHalInit(void **a_phHalHandle)
{
	return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eHalDeInit(void **a_phHalHandle)
{

	return VAL_RESULT_NO_ERROR;
}


VAL_RESULT_T eHalCmdProc(void **a_hHalHandle,
			 HAL_CMD_T a_eHalCmd,
			 void *a_pvInParam,
			 void *a_pvOutParam)
{
	switch (a_eHalCmd) {
	case HAL_CMD_SET_CMD_QUEUE:
		break;
	case HAL_CMD_SET_POWER:
		break;
	case HAL_CMD_SET_ISR:
		break;
	default:
		return VAL_RESULT_INVALID_PARAMETER;
	}

	return VAL_RESULT_NO_ERROR;
}
