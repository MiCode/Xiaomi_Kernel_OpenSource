// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 * Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include "hal_api.h"

VAL_RESULT_T eHalInit(VAL_HANDLE_T *a_phHalHandle)
{
	return VAL_RESULT_NO_ERROR;
}

VAL_RESULT_T eHalDeInit(VAL_HANDLE_T *a_phHalHandle)
{

	return VAL_RESULT_NO_ERROR;
}


VAL_RESULT_T eHalCmdProc(VAL_HANDLE_T *a_hHalHandle,
			HAL_CMD_T a_eHalCmd, VAL_VOID_T *a_pvInParam,
			VAL_VOID_T *a_pvOutParam)
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
