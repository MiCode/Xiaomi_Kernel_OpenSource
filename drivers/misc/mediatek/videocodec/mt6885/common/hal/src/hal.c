/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
