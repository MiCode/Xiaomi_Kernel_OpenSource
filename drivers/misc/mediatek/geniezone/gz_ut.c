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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include <gz-trusty/trusty_ipc.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include <kree/system.h>
#include <kree/mem.h>
#include "unittest.h"
#include "gz_ut.h"

/*global variable*/
DEFINE_MUTEX(ut_mutex);

/*define*/
#define TIPC_TEST_SRV "com.android.ipc-unittest.srv.echo"
#define APP_NAME2 "com.mediatek.gz.srv.sync-ut"

#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define echo_srv_name  "com.mediatek.geniezone.srv.echo"

#define KREE_DEBUG(fmt...) pr_debug("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)

/*declaration fun*/
#define TEST_STR_SIZE 512
static char buf1[TEST_STR_SIZE];
static char buf2[TEST_STR_SIZE];

INIT_UNITTESTS;

/*UT fun*/
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

	ret = KREE_CreateSession(echo_srv_name, &ut_session_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] KREE_CreateSession() Fail. ret=0x%x\n", __func__,
			 ret);
		return ret;
	}

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	param[0].value.a = (uint32_t) stress;
	param[1].value.a = (uint32_t) repeat;
	param[1].value.b = (uint32_t) page_num;
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

int tipc_test_send(struct tipc_k_handle *handle, void *param, int param_size)
{
	ssize_t rc;

	if (!handle || !param) {
		KREE_DEBUG("%s: invalid param\n", __func__);
		return -1;
	}

	KREE_DEBUG(" ===> %s: param_size = %d.\n", __func__, param_size);
	rc = tipc_k_write(handle, param, param_size, O_RDWR);
	KREE_DEBUG(" ===> %s: tipc_k_write rc = %d.\n", __func__, (int)rc);

	return rc;
}

int tipc_test_rcv(struct tipc_k_handle *handle, void *data, size_t len)
{
	ssize_t rc;

	if (!handle || !data) {
		KREE_DEBUG("%s: invalid param\n", __func__);
		return -1;
	}

	rc = tipc_k_read(handle, (void *)data, len, O_RDWR);
	KREE_DEBUG(" ===> %s: tipc_k_read(1) rc = %d.\n", __func__, (int)rc);

	return rc;
}

int gz_tipc_test(void *args)
{
	int i, rc;
	struct tipc_k_handle h = {0};

	TEST_BEGIN("tipc basic test");
	RESET_UNITTESTS;

	mutex_lock(&ut_mutex);
	KREE_DEBUG(" ===> %s: test begin\n", __func__);
	/* init test data */
	buf1[0] = buf1[1] = 'Q';
	buf1[2] = '\0';
	for (i = 0; i < TEST_STR_SIZE; i++)
		buf2[i] = 'c';

	KREE_DEBUG(" ===> %s: %s\n", __func__, TIPC_TEST_SRV);
	rc = tipc_k_connect(&h, TIPC_TEST_SRV);
	CHECK_EQ(0, rc, "connect");

	rc = tipc_test_send(&h, buf1, sizeof(buf1));
	CHECK_GT_ZERO(rc, "send 1");
	rc = tipc_test_rcv(&h, buf1, sizeof(buf1));
	CHECK_GT_ZERO(rc, "rcv 1");

	rc = tipc_test_send(&h, buf2, sizeof(buf2));
	CHECK_GT_ZERO(rc, "send 2");
	rc = tipc_test_rcv(&h, buf1, sizeof(buf2));
	CHECK_GT_ZERO(rc, "rcv 2");

	if (h.dn)
		rc = tipc_k_disconnect(&h);
	CHECK_EQ(0, rc, "disconnect");

	mutex_unlock(&ut_mutex);
	TEST_END;
	REPORT_UNITTESTS;

	return 0;
}


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

void test_gz_syscall(void)
{
	int i, tmp;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE session, session2;
	union MTEEC_PARAM param[4];
	uint32_t types;

	TEST_BEGIN("basic session & syscall");
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
	TEST_END;
}


void test_gz_mem_api(void)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE mem_session = 0;
	KREE_SECUREMEM_HANDLE mem_handle[4] = {0};

	TEST_BEGIN("mem service & mem APIs");

	ret = KREE_CreateSession(mem_srv_name, &mem_session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create mem srv session");

	/**** Memory ****/
	KREE_DEBUG("[GZTEST] memory APIs...\n");

	ret = KREE_AllocSecuremem(mem_session, &mem_handle[0], 0, 128);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "alloc secure mem 128");
	KREE_DEBUG("[GZTEST]KREE_AllocSecuremem handle = %d.\n", mem_handle[0]);

	ret = KREE_AllocSecuremem(mem_session, &mem_handle[1], 0, 512);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "alloc secure mem 512");
	KREE_DEBUG("[GZTEST]KREE_AllocSecuremem handle = %d.\n", mem_handle[1]);

	ret = KREE_AllocSecuremem(mem_session, &mem_handle[2], 0, 1024);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "alloc secure mem 1024");
	KREE_DEBUG("[GZTEST]KREE_AllocSecuremem handle = %d.\n", mem_handle[2]);

	ret = KREE_ZallocSecurememWithTag(mem_session, &mem_handle[3], 0, 2048);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "zero alloc secure mem 2048");
	KREE_DEBUG("[GZTEST]KREE_ZallocSecuremem handle = %d.\n",
		   mem_handle[3]);


	ret = KREE_ReferenceSecuremem(mem_session, mem_handle[0]);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "reference secure mem 1");
	ret = KREE_ReferenceSecuremem(mem_session, mem_handle[0]);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "reference secure mem 2");

	ret = KREE_UnreferenceSecuremem(mem_session, mem_handle[0]);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "unreference secure mem 1");
	ret = KREE_UnreferenceSecuremem(mem_session, mem_handle[0]);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "unreference secure mem 2");
	ret = KREE_UnreferenceSecuremem(mem_session, mem_handle[0]);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "unreference secure mem 3");

	ret = KREE_CloseSession(mem_session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close mem srv session");

	TEST_END;
}

void test_hw_cnt(void)
{
	u32 f;
	u64 cnt;

	TEST_BEGIN("hardware counter");

	f = KREE_GetSystemCntFrq();
	KREE_DEBUG("KREE_GetSystemCntFreq: %u\n", f);
	CHECK_NEQ(0, f, "MTEE_GetSystemCntFreq");

	cnt = KREE_GetSystemCnt();
	KREE_DEBUG("KREE_GetSystemCnt: %llu\n", cnt);
	CHECK_NEQ(0, cnt, "MTEE_GetSystemCnt");

	TEST_END;
}

#ifdef ENABLE_SYNC_TEST
static int sync_test(void)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE sessionHandle, sessionHandle2;

	union MTEEC_PARAM param[4];
	uint32_t types;

	TEST_BEGIN("TA sync test");

	/* Connect to echo service */
	ret = KREE_CreateSession(echo_srv_name, &sessionHandle);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo srv session");
	CHECK_GT_ZERO(sessionHandle, "check echo srv session value");

	/* Connect to sync-ut service */
	ret = KREE_CreateSession(APP_NAME2, &sessionHandle2);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create sync-ut srv session");
	CHECK_GT_ZERO(sessionHandle, "check echo sync-ut session value");

	/* Request mutex handle from TA1 */
	types = TZ_ParamTypes2(TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(sessionHandle, TZCMD_GET_MUTEX, types, param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "get mutex handle from TA1");

	CHECK_GT_ZERO(param[0].value.a, "check mutex value");

	/* Send mutex handle to TA2 */
	types = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT);
	ret = KREE_TeeServiceCall(sessionHandle2, TZCMD_SEND_MUTEX, types,
				  param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "send mutex handle to TA2");

	/* start mutex test */
	ret = KREE_TeeServiceCall(sessionHandle, TZCMD_TEST_MUTEX, types,
				  param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "send start cmd to TA1");
	ret = KREE_TeeServiceCall(sessionHandle2, TZCMD_TEST_MUTEX, types,
				  param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "send start cmd to TA2");

	ret = KREE_CloseSession(sessionHandle);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo srv session");
	ret = KREE_CloseSession(sessionHandle2);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo sync-ut session");

	TEST_END;
	return 0;
}
#endif

int gz_test(void *arg)
{
	KREE_DEBUG("[GZTEST]====> GenieZone Linux kernel test\n");

	RESET_UNITTESTS;

	test_gz_syscall();
	test_gz_mem_api();
#ifdef ENABLE_SYNC_TEST
	sync_test();
#endif
	test_hw_cnt();

	REPORT_UNITTESTS;

	return 0;
}

int gz_abort_test(void *args)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE session = 0;
	union MTEEC_PARAM param[4];
	uint32_t types;

	ret = KREE_CreateSession(echo_srv_name, &session);
	/**** Service call test ****/
	param[0].value.a = 0x1230;
	/* memory boundary case parameters */
	types = TZ_ParamTypes1(TZPT_VALUE_INPUT);
	ret = KREE_TeeServiceCall(session, TZCMD_ABORT_TEST, types, param);

	if (ret != TZ_RESULT_SUCCESS)
		KREE_DEBUG("KREE_TeeServiceCall Error: handle 0x%x, ret %d\n",
			   (uint32_t) session, ret);
	ret = KREE_CloseSession(session);

	return 0;
}
