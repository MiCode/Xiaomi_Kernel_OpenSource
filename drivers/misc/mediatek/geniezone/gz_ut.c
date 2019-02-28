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

#include <linux/kernel.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <kree/system.h>

#include "gz_ut.h"

#define KREE_DEBUG(fmt...) pr_debug("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_debug("[KREE][ERR]" fmt)

int dma_test(void *args)
{
	KREE_SESSION_HANDLE ut_session_handle;
	TZ_RESULT ret;
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int stress = 0, repeat = 0, page_num = 0;
	char c;
	int rv = 0;

	KREE_DEBUG("[%s] start\n", __func__);

	rv = sscanf((char *)args, "%c %d %d %d", &c, &stress, &repeat,
		    &page_num);
	if (rv != 4) {
		KREE_ERR("[%s] ===> sscanf Fail.\n", __func__);
		return TZ_RESULT_ERROR_GENERIC;
	}

	ret = KREE_CreateSession(GZ_ECHO_SRV_NAME, &ut_session_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] KREE_CreateSession() Fail. ret=0x%x\n", __func__,
			 ret);
		return ret;
	}

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	param[0].value.a = (uint32_t)stress;
	param[1].value.a = (uint32_t)repeat;
	param[1].value.b = (uint32_t)page_num;
	ret = KREE_TeeServiceCall(ut_session_handle, TZCMD_DMA_TEST, paramTypes,
				  param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] ====> KREE_TeeServiceCall() Fail. ret=0x%x\n",
			 __func__, ret);
		return ret;
	}

	ret = KREE_CloseSession(ut_session_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] ====> KREE_CloseSession() Fail. ret=0x%x\n",
			 __func__, ret);
		return ret;
	}

	return 0;
}
