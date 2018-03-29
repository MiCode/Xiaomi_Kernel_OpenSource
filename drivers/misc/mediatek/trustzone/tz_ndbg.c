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


#include "tz_ndbg.h"

#ifdef CC_ENABLE_NDBG

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/random.h>
#if 0
#include <mach/battery_meter.h>
#endif
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_test.h"
#include "tz_cross/ta_mem.h"
#include "tz_cross/ta_ndbg.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#include "kree_int.h"

int entropy_thread(void *arg)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE ndbg_session;
	KREE_SESSION_HANDLE mem_session;
	KREE_SHAREDMEM_HANDLE shm_handle;
	KREE_SHAREDMEM_PARAM shm_param;
	MTEEC_PARAM param[4];
	uint8_t *ptr;

	ptr = kmalloc(NDBG_REE_ENTROPY_SZ, GFP_KERNEL);
	memset(ptr, 0, NDBG_REE_ENTROPY_SZ);

	while (!kthread_should_stop()) {
		ret = KREE_CreateSession(TZ_TA_NDBG_UUID, &ndbg_session);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("CreateSession error %d\n", ret);
			return 1;
		}

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("Create memory session error %d\n", ret);
			return 1;
		}

		shm_param.buffer = ptr;
		shm_param.size = NDBG_REE_ENTROPY_SZ;
		ret = KREE_RegisterSharedmem(mem_session, &shm_handle,
						&shm_param);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("KREE_RegisterSharedmem Error: %s\n",
				TZ_GetErrorString(ret));
			return 1;
		}

#if 0
		*((uint32_t *)(ptr + 0)) = battery_meter_get_battery_voltage();
		*((uint32_t *)(ptr + 4)) = battery_meter_get_VSense();
		*((uint32_t *)(ptr + 8)) = battery_meter_get_charger_voltage();
		*((uint32_t *)(ptr + 12)) = battery_meter_get_charger_voltage();
		get_random_bytes(ptr + NDBG_BAT_ST_SIZE, URAN_SIZE);
#else
		get_random_bytes(ptr, NDBG_REE_ENTROPY_SZ);
#endif

		param[0].memref.handle = (uint32_t) shm_handle;
		param[0].memref.offset = 0;
		param[0].memref.size = NDBG_REE_ENTROPY_SZ;
		param[1].value.a = NDBG_REE_ENTROPY_SZ;

		ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE) ndbg_session,
					TZCMD_NDBG_INIT,
					TZ_ParamTypes3(TZPT_MEMREF_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT),
					param);

		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("TZCMD_NDBG_INIT fail, reason:%s\n",
				TZ_GetErrorString(ret));

		pr_debug("Start to wait reseed.\n");
		ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE) ndbg_session,
					TZCMD_NDBG_WAIT_RESEED,
					TZ_ParamTypes3(TZPT_MEMREF_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT),
					param);
		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("TZCMD_NDBG_WAIT_RESEED fail, reason:%s\n",
				TZ_GetErrorString(ret));

		pr_debug("OK to send reseed.\n");

		ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("KREE_UnregisterSharedmem Error: %s\n",
				TZ_GetErrorString(ret));
			return 1;
		}

		ret = KREE_CloseSession(ndbg_session);
		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("CloseSession error %d\n", ret);

		ret = KREE_CloseSession(mem_session);
		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("Close memory session error %d\n", ret);

	}

	kfree(ptr);

	return 0;
}

#ifdef CC_NDBG_TEST_PROGRAM
int test_random_thread(void *arg)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE ndbg_session;
	KREE_SESSION_HANDLE mem_session;
	KREE_SHAREDMEM_HANDLE shm_handle;
	KREE_SHAREDMEM_PARAM shm_param;
	MTEEC_PARAM param[4];
	uint32_t *ptr;
	int size = 32;

	ptr = kmalloc(size, GFP_KERNEL);
	memset(ptr, 0, size);

	while (!kthread_should_stop()) {
		ret = KREE_CreateSession(TZ_TA_NDBG_UUID, &ndbg_session);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("CreateSession error %d\n", ret);
			return 1;
		}

		ret = KREE_CreateSession(TZ_TA_MEM_UUID, &mem_session);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("Create memory session error %d\n", ret);
			return 1;
		}

		shm_param.buffer = ptr;
		shm_param.size = size;
		ret = KREE_RegisterSharedmem(mem_session, &shm_handle,
						&shm_param);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("KREE_RegisterSharedmem Error: %s\n",
				TZ_GetErrorString(ret));
			return 1;
		}

		param[0].memref.handle = (uint32_t) shm_handle;
		param[0].memref.offset = 0;
		param[0].memref.size = size / 4;
		param[1].value.a = size / 4;

		ret = KREE_TeeServiceCall((KREE_SESSION_HANDLE) ndbg_session,
					TZCMD_NDBG_RANDOM,
					TZ_ParamTypes3(TZPT_MEMREF_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT),
					param);

		ret = KREE_UnregisterSharedmem(mem_session, shm_handle);
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("KREE_UnregisterSharedmem Error: %s\n",
				TZ_GetErrorString(ret));
			return 1;
		}

		ret = KREE_CloseSession(ndbg_session);
		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("CloseSession error %d\n", ret);

		ret = KREE_CloseSession(mem_session);
		if (ret != TZ_RESULT_SUCCESS)
			pr_warn("Close memory session error %d\n", ret);

		ssleep(5);
	}

	kfree(ptr);

	return 0;
}
#endif

int __init tz_ndbg_init(void)
{
	kthread_run(entropy_thread, NULL, "entropy_thread");
#ifdef CC_NDBG_TEST_PROGRAM
	kthread_run(test_random_thread, NULL, "test_random_thread");
#endif
	return 0;
}

late_initcall(tz_ndbg_init);
#endif
