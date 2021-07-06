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


#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_test.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#include "kree_int.h"


uint32_t TEECK_Test_Add(KREE_SESSION_HANDLE session, uint32_t a, uint32_t b)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	param[0].value.a = a;
	param[1].value.a = b;
	paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
					TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(session, TZCMD_TEST_ADD, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("ServiceCall error %d\n", ret);
		param[2].value.a = 0;
	}

	return param[2].value.a;
}


void tz_test(void)
{
	int ret;
	KREE_SESSION_HANDLE test_session;
	KREE_SESSION_HANDLE mem_session;
	KREE_SECUREMEM_HANDLE mem_handle;
	KREE_SHAREDMEM_HANDLE shm_handle;
	struct KREE_SHAREDMEM_PARAM shm_param;
	uint32_t result;
	struct timespec start, end;
	long long ns;
	int i;
	union MTEEC_PARAM param[4];
	uint32_t *ptr;
	int size;

	ret = KREE_CreateSession(TZ_TA_TEST_UUID, &test_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("CreateSession error %d\n", ret);
		return;
	}

	result = TEECK_Test_Add(test_session, 10, 20);
	pr_debug("%s TZCMD_TEST_ADD %d\n", __func__, result);

	/* / Time test. */
	getnstimeofday(&start);
	for (i = 0; i < 100; i++)
		result = TEECK_Test_Add(test_session, 10, 20);

	getnstimeofday(&end);
	ns = ((long long)end.tv_sec - start.tv_sec) * 1000000000 +
			(end.tv_nsec - start.tv_nsec);
	pr_debug("100 times TEST_ADD %lld ns\n", ns);

	ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Create memory session error %d\n", ret);
		return;
	}

	pr_debug("test A\n");
	size = 4 * 1024;
	ptr = kmalloc(size, GFP_KERNEL);


	shm_param.buffer = ptr;
	shm_param.size = size;
	ret = KREE_RegisterSharedmem(mem_session, &shm_handle, &shm_param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("KREE_RegisterSharedmem Error: %s\n",
			TZ_GetErrorString(ret));
		return;
	}
	pr_debug("shm handle = 0x%x\n", shm_handle);

	for (i = 0; i < 4 * 1024 / 4; i++)
		ptr[i] = i;

	param[0].memref.handle = (uint32_t) shm_handle;
	param[0].memref.offset = 0;
	param[0].memref.size = 4 * 1024;
	param[1].value.a = (4 * 1024) / 4;
	ret = KREE_TeeServiceCall(test_session, TZCMD_TEST_ADD_MEM,
					TZ_ParamTypes3(TZPT_MEMREF_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT),
					param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("TZCMD_TEST_ADD_MEM error %d\n", ret);
		return;
	}
	pr_debug("KREE ADD MEM result = 0x%x\n", param[2].value.a);

	ret = KREE_AllocSecuremem(mem_session, &mem_handle, 0, 1024);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("Secure memory allocate error %d\n", ret);
		return;
	}

	param[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(test_session, TZCMD_TEST_DO_A,
				  TZ_ParamTypes3(TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT,
							TZPT_VALUE_OUTPUT),
					param);
	pr_debug("Do A = 0x%x, 0x%x (%d)\n",
		param[1].value.a, param[2].value.a, ret);

	ret = KREE_ReferenceSecuremem(mem_session, mem_handle);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("KREE_ReferenceSecuremem Error: %d\n", ret);

	param[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(test_session, TZCMD_TEST_DO_B,
					TZ_ParamTypes3(TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT,
							TZPT_VALUE_OUTPUT),
					param);
	pr_debug("Do B = 0x%x, 0x%x (%d)\n",
		param[1].value.a, param[2].value.a, ret);

	/* Free/Unreference secure memory */
	ret = KREE_UnreferenceSecuremem(mem_session, mem_handle);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("KREE_UnReferenceSecureMem Error 1: %d\n", ret);

	ret = KREE_UnreferenceSecuremem(mem_session, mem_handle);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("KREE_UnReferenceSecureMem Error 2: %d\n", ret);

	ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("KREE_UnregisterSharedmem Error: %s\n",
			TZ_GetErrorString(ret));
		return;
	}

	ret = KREE_CloseSession(test_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("CloseSession error %d\n", ret);

	ret = KREE_CloseSession(mem_session);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("Close memory session error %d\n", ret);

	pr_debug("KREE test done!!!!\n");
}
