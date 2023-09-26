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

#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include <kree/system.h>
#include <kree/mem.h>
#include "unittest.h"
#include "gz_ut.h"

#include <linux/ktime.h>

/*define*/
#define KREE_DEBUG(fmt...) pr_info("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)

#define echo_srv_name  "com.mediatek.geniezone.srv.echo"

/*declaration fun*/
#define TEST_STR_SIZE 512
static char buf1[TEST_STR_SIZE];
static char buf2[TEST_STR_SIZE];

INIT_UNITTESTS;

DEFINE_MUTEX(ut_mutex);
int check_gp_inout_mem(char *buffer)
{
	int i;

	for (i = 0; i < TEST_STR_SIZE; i++) {
		if (i % 3) {
			if (buffer[i] != 'c')
				return 1;
		} else {
			if (buffer[i] != 'd')
				return 1;
		}
	}
	return 0;
}

/*test ReeServiceCall*/
int test_gz_syscall(void *arg)
{
	int i, tmp;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE session = 0, session2 = 0;
	union MTEEC_PARAM param[4];
	uint32_t types;

	TEST_BEGIN("basic session & syscall");
	RESET_UNITTESTS;

	mutex_lock(&ut_mutex);

	ret = KREE_CreateSession(echo_srv_name, &session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo srv session");

	/* connect to unknown server */
	ret = KREE_CreateSession("unknown.server", &session2);
	CHECK_EQ(TZ_RESULT_ERROR_COMMUNICATION, ret,
		 "connect to unknown server");

	/* null checking */
	ret = KREE_CreateSession(echo_srv_name, NULL);
	CHECK_EQ(TZ_RESULT_ERROR_BAD_PARAMETERS, ret,
		 "create session null checking");

	/* connect to the same server multiple times */
	ret = KREE_CreateSession(echo_srv_name, &session2);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo srv session 2");

	/**** Service call test ****/
	for (i = 0; i < TEST_STR_SIZE; i++)
		buf2[i] = 'c';

	param[0].value.a = 0x1230;
	param[1].mem.buffer = (void *)buf1;
	param[1].mem.size = TEST_STR_SIZE;
	param[2].mem.buffer = (void *)buf2;
	param[2].mem.size = TEST_STR_SIZE;

	/* memory boundary case parameters */
	types = TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_MEM_OUTPUT,
			       TZPT_MEM_INOUT, TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(session, TZCMD_TEST_SYSCALL, types, param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "test TA syscall");

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("KREE_TeeServiceCall Error: handle 0x%x, ret %d\n",
			   (uint32_t) session, ret);
	} else {
		tmp = strcmp((char *)param[1].mem.buffer, "sample data 1!!");
		CHECK_EQ(0, tmp, "check gp param: mem output");
		tmp = check_gp_inout_mem(buf2);
		CHECK_EQ(0, tmp, "check gp param: mem inout");
		CHECK_EQ(99, param[3].value.a, "check gp param: value output");
	}
	ret = KREE_CloseSession(session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo srv session");
	ret = KREE_CloseSession(session2);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo srv session 2");

	mutex_unlock(&ut_mutex);
	REPORT_UNITTESTS;
	TEST_END;
	return 0;
}

/*simple TeeServiceCall:add*/
DEFINE_MUTEX(simple_ut_mutex);
int simple_ut(void *args)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE session = 0;
	union MTEEC_PARAM param[4];
	uint32_t types;
	uint32_t val =  0x1230;

	ktime_t start, end;

	TEST_BEGIN("Simple test");
	RESET_UNITTESTS;

	mutex_lock(&simple_ut_mutex);

	/* Connect to echo service */
	start = ktime_get();
	ret = KREE_CreateSession(echo_srv_name, &session);
	end = ktime_get();

	KREE_DEBUG("CreateSession time: %lld (ns)\n",
		ktime_to_ns(ktime_sub(end, start)));

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "CreateSession:echo");

	/**** simple test:add ****/
	param[0].value.a = val;
	param[0].value.b = val;

	types = TZ_ParamTypes1(TZPT_VALUE_INOUT);
	start = ktime_get();
	ret = KREE_TeeServiceCall(session, TZCMD_TEST_ADD, types, param);
	end = ktime_get();
	KREE_DEBUG("TeeServiceCall time(simple ADD): %lld (ns)\n",
		ktime_to_ns(ktime_sub(end, start)));

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "TeeServiceCall");
	CHECK_EQ(param[0].value.b, (val + val), "TeeServiceCall");

	start = ktime_get();
	ret = KREE_CloseSession(session);
	end = ktime_get();
	KREE_DEBUG("CloseSession time(simple ADD): %lld (ns)\n",
		ktime_to_ns(ktime_sub(end, start)));

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "CreateSession:echo");

	mutex_unlock(&simple_ut_mutex);

	REPORT_UNITTESTS;
	TEST_END;
	return 0;
}

