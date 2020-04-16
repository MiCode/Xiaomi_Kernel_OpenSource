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
#include <linux/kthread.h>
#include <linux/freezer.h>

#include "tz_cross/trustzone.h"
#include "tz_cross/ta_icnt.h"
#include "trustzone/kree/system.h"
#include "kree_int.h"
#include "tz_counter.h"

#ifdef ENABLE_INC_ONLY_COUNTER
uint32_t TEECK_Icnt_Counter(KREE_SESSION_HANDLE session,
				uint32_t *a, uint32_t *b)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(session, TZCMD_ICNT_COUNT, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("ServiceCall error %d\n", ret);

	*a = param[0].value.a;
	*b = param[1].value.a;

	return ret;
}

uint32_t TEECK_Icnt_Rate(KREE_SESSION_HANDLE session, uint32_t *a)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	paramTypes = TZ_ParamTypes1(TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(session, TZCMD_ICNT_RATE, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS)
		pr_warn("ServiceCall error %d\n", ret);

	*a = param[0].value.a;

	return ret;
}

#define THREAD_COUNT_FREQ 10
int update_counter_thread(void *data)
{
	int ret;
	KREE_SESSION_HANDLE icnt_session;
	uint32_t result;
	uint32_t a, b, rate;
	uint32_t nsec = THREAD_COUNT_FREQ;

	ret = KREE_CreateSession(TZ_TA_ICNT_UUID, &icnt_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("CreateSession error %d\n", ret);
		return 1;
	}

	result = TEECK_Icnt_Rate(icnt_session, &rate);
	if (result == TZ_RESULT_SUCCESS) {
		/* pr_debug("(yjdbg) rate: %d\n", rate); */
		nsec = (0xffffffff / rate);
		nsec -= 600;
		/* pr_debug("(yjdbg) rate: %d\n", nsec); */
	}

	set_freezable();

	for (;;) {
		if (kthread_should_stop())
			break;

		if (try_to_freeze())
			continue;

		result = TEECK_Icnt_Counter(icnt_session, &a, &b);
		if (result == TZ_RESULT_SUCCESS) {
			pr_debug("(yjdbg) tz_test TZCMD_ICNT_COUNT: 0x%x, 0x%x\n",
				a, b);
		}

		schedule_timeout_interruptible(HZ * nsec);
	}

	ret = KREE_CloseSession(icnt_session);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("CloseSession error %d\n", ret);
		return 1;
	}

	return 0;
}
#endif
