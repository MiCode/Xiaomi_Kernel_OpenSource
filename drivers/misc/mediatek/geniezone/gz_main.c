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
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

#include <linux/trusty/trusty_ipc.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include <tz_cross/ta_fbc.h> /* FPS GO cmd header */
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include "unittest.h"

#include "gz_main.h"
#include "gz_ut.h"
#include "gz_chmem_ut.h"

#if defined(GZ_SEC_STORAGE_UT)
#include "gz_sec_storage_ut.h"
#endif

/* GP memory parameter max len. Needs to be synced with GZ */
#define GP_MEM_MAX_LEN 1024

/* for stress test */
#define stresstest_secmem 1
#define stresstest_shdmem 2
#define stresstest_shdmem_case1 21
#define stresstest_shdmem_case2 22
#define stresstest_shdmem_case3 23
#define stresstest_ipc 3
#define stresstest_session 5
#define stresstest_chunkmem 6

static int _stresstest_total;
static int _stresstest_failed;

#define KREE_TESTSHOW(fmt...) pr_info("[KREE]" fmt)
#define set_stresstest_total(val) (_stresstest_total = val)
#define set_stresstest_failed(val) (_stresstest_failed = val)
#define add_stresstest_total(val) (_stresstest_total += val)
#define add_stresstest_failed(val) (_stresstest_failed += val)
#define get_stresstest_total() (_stresstest_total)
#define get_stresstest_failed() (_stresstest_failed)

#define stress_test_init(void)                                                 \
	do {                                                                   \
		set_stresstest_total(0);                                       \
		set_stresstest_failed(0);                                      \
	} while (0)

#define stress_test_prt_result(str)                                            \
	{                                                                      \
		if (get_stresstest_failed() > 0)                               \
			KREE_TESTSHOW("[GZTEST] Stress test: %s: FAILED\n",    \
				      str);                                    \
		else                                                           \
			KREE_TESTSHOW("[GZTEST] Stress test: %s: PASSED\n",    \
				      str);                                    \
		KREE_TESTSHOW("======== UT report =======\n");                 \
		KREE_TESTSHOW("total test cases: %u\n",                        \
			      get_stresstest_total());                         \
		KREE_TESTSHOW("FAILED test cases: %u\n",                       \
			      get_stresstest_failed());                        \
		if (get_stresstest_failed() > 0)                               \
			KREE_TESTSHOW("===========> UT FAILED\n");             \
		else                                                           \
			KREE_TESTSHOW("===========> UT SUCCESSED\n");          \
	}

static int stress_test_main(void *argv);
static int stress_test_main_infinite(void *args);
static uint64_t init_shm_test(char **buf_p, int size);
static void verify_chm_result(char *buf, int size);
static int _chunk_memory_test_body(union MTEEC_PARAM *param);
static int test_MD_appendSecureChmem(void *args);

/*end for stress test */

#define KREE_DEBUG(fmt...) pr_debug("[KREE]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE][ERR]" fmt)

//#define GZTEST_DEBUG 1
#define GZ_CHMEM_TEST 0

static const struct file_operations fops = {.owner = THIS_MODULE,
					    .open = gz_dev_open,
					    .release = gz_dev_release,
					    .unlocked_ioctl = gz_ioctl,
#if defined(CONFIG_COMPAT)
					    .compat_ioctl = gz_compat_ioctl,
#endif
					    .write = gz_dev_write,
					    .read = gz_dev_read};

static struct miscdevice gz_device = {.minor = MISC_DYNAMIC_MINOR,
				      .name = "gz_kree",
				      .fops = &fops};

static int get_gz_version(void *args);
static int gz_abort_test(void *args);
static int gz_tipc_test(void *args);
static int gz_test(void *arg);
static int gz_test_shm(void *arg);
static int gz_abort_test(void *arg);

static const char *cases =
	"0: TIPC\n 1: General KREE API\n"
	"2: Shared memory API\n 3: GZ ramdump\n"
	"4: GZ internal API\n 5: FPS GO";

DEFINE_MUTEX(ut_mutex);

/************* sysfs operations ****************/
static ssize_t show_testcases(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", cases);
}

static ssize_t run_gz_case(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t n)
{
	char tmp[50];
	char c;
	struct task_struct *th;

	strcpy(tmp, buf);
	tmp[n - 1] = '\0';
	c = tmp[0];

	KREE_DEBUG("GZ KREE test: %c\n", c);
	switch (c) {
	case '0':
		KREE_DEBUG("get gz version\n");
		th = kthread_run(get_gz_version, NULL, "GZ version");
		break;
	case '1':
		KREE_DEBUG("test tipc\n");
		th = kthread_run(gz_tipc_test, NULL, "GZ tipc test");
		break;

	case '2':
		KREE_DEBUG("test general functions\n");
		th = kthread_run(gz_test, NULL, "GZ KREE test");

		break;
	case '3':
		KREE_DEBUG("test shared memory functions\n");
		th = kthread_run(gz_test_shm, NULL, "GZ KREE test");

		break;
	case '4':
		KREE_DEBUG("test GenieZone abort\n");
		th = kthread_run(gz_abort_test, NULL, "GZ KREE test");

		break;
	case '5':
		KREE_DEBUG("test stress test function\n");
		th = kthread_run(stress_test_main, NULL, "GZ KREE stress test");
		break;
	case '6':
		KREE_DEBUG("test stress test function infinite\n");
		th = kthread_run(stress_test_main_infinite, NULL,
				 "GZ KREE infinite stress test");
		break;
	case '7':
		KREE_DEBUG("test chunk memory for ION_simple\n");
		th = kthread_run(chunk_memory_test_ION_simple, NULL,
			"MCM test*1");
		break;
	case '8':
		KREE_DEBUG("test chunk memory for ION_multiple\n");
		th = kthread_run(chunk_memory_test_ION_Multiple, NULL,
			"MCM test*N");
		break;
	case '9':
		KREE_DEBUG("test chunk memory for ION_simple in MTEE\n");
		th = kthread_run(chunk_memory_test_by_MTEE_TA, NULL,
			"Chmem UT@MTEE");
		break;
	case 'A':
		KREE_DEBUG("test Basic chunk memory\n");
		th = kthread_run(chunk_memory_test, NULL, "Basic Chmem UT");
		break;
	case 'B':
		KREE_DEBUG("test MD Append Chmem\n");
		th = kthread_run(test_MD_appendSecureChmem, NULL,
				 "MD Chmem test");
		break;
#if defined(GZ_SEC_STORAGE_UT)
	case 'C':
		KREE_DEBUG("test GZ Secure Storage\n");
		th = kthread_run(test_SecureStorageBasic, NULL,
				 "sec_storage_ut");
		break;
#endif

	default:
		KREE_DEBUG("err: unknown test case\n");

		break;
	}
	return n;
}
DEVICE_ATTR(gz_test, 0644, show_testcases, run_gz_case);

static int create_files(void)
{
	int res;

	res = misc_register(&gz_device);

	if (res != 0) {
		KREE_DEBUG("ERR: misc register failed.");
		return res;
	}
	res = device_create_file(gz_device.this_device, &dev_attr_gz_test);
	if (res != 0) {
		KREE_DEBUG("ERR: create sysfs do_info failed.");
		return res;
	}
	return 0;
}

/*********** test case implementations *************/

static const char mem_srv_name[] = "com.mediatek.geniezone.srv.mem";
static const char echo_srv_name[] = "com.mediatek.geniezone.srv.echo";
#define APP_NAME2 "com.mediatek.gz.srv.sync-ut"

#define TEST_STR_SIZE 512
static char buf1[TEST_STR_SIZE];
static char buf2[TEST_STR_SIZE];

INIT_UNITTESTS;

static int get_gz_version(void *args)
{
	int ret;
	int i;
	int version_str_len;
	char *version_str;

	ret = trusty_fast_call32(gz_device.this_device, SMC_FC_GET_VERSION_STR,
				 -1, 0, 0);
	if (ret <= 0)
		goto err_get_size;

	version_str_len = ret;

	version_str = kmalloc(version_str_len + 1, GFP_KERNEL);
	for (i = 0; i < version_str_len; i++) {
		ret = trusty_fast_call32(gz_device.this_device,
					 SMC_FC_GET_VERSION_STR, i, 0, 0);
		if (ret < 0)
			goto err_get_char;
		version_str[i] = ret;
	}
	version_str[i] = '\0';

	dev_info(gz_device.this_device, "GZ version: %s\n", version_str);
	KREE_DEBUG("GZ version is : %s.....\n", version_str);
	return 0;

err_get_char:
	kfree(version_str);
	version_str = NULL;
err_get_size:
	dev_info(gz_device.this_device, "failed to get version: %d\n", ret);
	return 0;
}

static void test_hw_cnt(void)
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

static int check_gp_inout_mem(char *buffer)
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


static int tipc_test_send(struct tipc_k_handle *handle, void *param,
	int param_size)
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

static int tipc_test_rcv(struct tipc_k_handle *handle, void *data, size_t len)
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

#define TIPC_TEST_SRV "com.android.ipc-unittest.srv.echo"
int gz_tipc_test(void *args)
{
	int i, rc;
	struct tipc_k_handle h;

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
	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);

	return 0;
}

static void test_gz_syscall(void)
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

	/* connect to the same server multiple times*/
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
			   (uint32_t)session, ret);
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

static void test_gz_mem_api(void)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE mem_session;
	KREE_SECUREMEM_HANDLE mem_handle[4];

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

static int gz_test(void *arg)
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


#define TEST_STR_LEN 512

int _getRandomValue(int maxValue)
{
	int randVal = 0;

	while (1) {
		get_random_bytes(&randVal, sizeof(randVal));
		randVal = abs(randVal % maxValue);
		if (randVal == 0)
			continue;
		break;
	}
	KREE_DEBUG("==> randVal=%d\n", randVal);
	return randVal;
}

static int _test_alloc_securemem(KREE_SESSION_HANDLE mem_session_hdl,
				 KREE_SESSION_HANDLE echo_session_hdl,
				 KREE_SECUREMEM_HANDLE *mem_handle, int reqsize)
{
	int alignment = 0;
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret = 0;

	/* Request secure memory from gz memory service */
	ret = KREE_AllocSecuremem(mem_session_hdl, mem_handle, alignment,
				  reqsize);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] KREE_AllocSecuremem() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	if ((*mem_handle) == (0)) {
		KREE_ERR("[%s] KREE_AllocSecuremem() Fail. mem_handle= %d\n",
			 __func__, *mem_handle);
		return -1;
	}
	KREE_DEBUG(
		"get secure memory handle = %d. (alignment=%d, size=%d). ret=%d\n",
		*mem_handle, alignment, reqsize, ret);

	param[0].value.a = *mem_handle; /* secure memory handle */
	param[0].value.b = reqsize;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	KREE_TeeServiceCall(echo_session_hdl, 0x6001, paramTypes, param);

	if (param[1].value.a != 0) {
		KREE_DEBUG(
			"TZCMD_MEM_STRESS_TEST sec memory: memory handle = %d (alignment=%d, size=%d) [FAIL].\n",
			*mem_handle, alignment, reqsize);
		return -1; /* fail */
	}

	return TZ_RESULT_SUCCESS;
}

static int _test_reg_sharedmem(KREE_SESSION_HANDLE mem_session_hdl,
			       KREE_SESSION_HANDLE echo_session_hdl,
			       KREE_SECUREMEM_HANDLE *mem_handle,
			       KREE_SHAREDMEM_PARAM *shm_param, int numOfPA)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret = 0;

	ret = KREE_RegisterSharedmem(mem_session_hdl, mem_handle, shm_param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] KREE_RegisterSharedmem() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	param[0].value.a = *mem_handle;
	param[1].value.a = numOfPA;
	param[1].value.b = shm_param->size;
	paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
				    TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(echo_session_hdl, 0x5588, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] KREE_TeeServiceCall():echo_session[0x5588] returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

static int _test_alloc_chunkmem(KREE_SESSION_HANDLE mem_session_hdl,
				KREE_SESSION_HANDLE echo_session_hdl,
				KREE_SECUREMEM_HANDLE *mem_handle,
				KREE_SHAREDMEM_PARAM *shm_param, int numOfPA)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret = 0;

	KREE_AppendSecureMultichunkmem(mem_session_hdl, mem_handle, shm_param);
	/*
	 * ret = KREE_AllocChunkmem(mem_session_hdl, mem_handle, shm_param);
	 */
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] KREE_AllocChunkmem() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		return ret;
	}


	param[0].value.a = *mem_handle;
	param[1].value.a = numOfPA;
	param[1].value.b = shm_param->size;
	paramTypes = TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
				    TZPT_VALUE_OUTPUT);
	/* //to-be-del
	 * ret = KREE_TeeServiceCall(echo_session_hdl, 0x9997, paramTypes,
	 * param);
	 * if (ret != TZ_RESULT_SUCCESS) {
	 *     KREE_ERR("[%s] KREE_TeeServiceCall():echo_session[0x9997] returns
	 * Fail. [Stop!]. ret=0x%x\n"
	 *         , __func__, ret);
	 *     return ret;
	 * }
	 */
	return TZ_RESULT_SUCCESS;
}
static int _test_release_securemem(int numOfMemHandler,
				   KREE_SESSION_HANDLE mem_session_hdl,
				   KREE_SECUREMEM_HANDLE *mem_handle)
{
	int i, ret;
	/* Start to unreference secure memory */
	for (i = 0; i < numOfMemHandler; i++) {
		KREE_DEBUG(
			"KREE_UnreferenceSecuremem: mem_handle[%d] =0x%x, mem_session=%d\n",
			i, (uint32_t)(*(mem_handle + i)), mem_session_hdl);
		ret = KREE_UnreferenceSecuremem(mem_session_hdl,
						*(mem_handle + i));
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR(
				"KREE_UnreferenceSecuremem Error: mem_handle[%d] =0x%x, ret=%d\n",
				i, (uint32_t)(*(mem_handle + i)), ret);
			return -1;
		}
	}

	return TZ_RESULT_SUCCESS;
}

static int _test_release_sharedmem(int numOfMemHandler,
				   KREE_SESSION_HANDLE mem_session_hdl,
				   KREE_SECUREMEM_HANDLE *mem_handle)
{
	int i, ret;
	/* Start to unreference secure memory */
	for (i = 0; i < numOfMemHandler; i++) {
		KREE_DEBUG(
			"KREE_UnregisterSharedmem: mem_handle[%d] =0x%x, mem_session=%d\n",
			i, (uint32_t)(*(mem_handle + i)), mem_session_hdl);
		ret = KREE_UnregisterSharedmem(mem_session_hdl,
					       *(mem_handle + i));
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR(
				"KREE_UnregisterSharedmem Error: mem_handle[%d] =0x%x, ret=%d\n",
				i, (uint32_t)(*(mem_handle + i)), ret);
			return -1;
		}
	}

	return TZ_RESULT_SUCCESS;
}

static int _test_release_chunkmem(int numOfMemHandler,
				  KREE_SESSION_HANDLE mem_session_hdl,
				  KREE_SECUREMEM_HANDLE *mem_handle)
{
	int i, ret;
	/* Start to unreference chunk memory */
	for (i = 0; i < numOfMemHandler; i++) {
		KREE_DEBUG(
			"KREE_UnreferenceChunkmem: mem_handle[%d] =0x%x, mem_session=%d\n",
			i, (uint32_t)(*(mem_handle + i)), mem_session_hdl);
		/*
		 * ret = KREE_UnregisterChunkmem(mem_session_hdl,
		 * *(mem_handle+i));
		 */
		ret = KREE_ReleaseSecureMultichunkmem(mem_session_hdl,
						      *(mem_handle + i));
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR(
				"KREE_UnreferenceChunkmem Error: mem_handle[%d] =0x%x, ret=%d\n",
				i, (uint32_t)(*(mem_handle + i)), ret);
			return -1;
		}
	}
	return TZ_RESULT_SUCCESS;
}
static int _stress_test_Memory_body(int test_type, union MTEEC_PARAM *param,
				    KREE_SHAREDMEM_PARAM *shm_param)
{
	int ret, i, m;
	KREE_SESSION_HANDLE *mem_session_hdl = NULL;
	KREE_SESSION_HANDLE *echo_session_hdl = NULL;

	/* KREE_SECUREMEM_HANDLE, KREE_SHAREDMEM_HANDLE are uint32_t */
	KREE_SECUREMEM_HANDLE *mem_handle = NULL;

	int reqsize = shm_param->size;
	int hasSessCreated = 0;

	int isRandomTest = 0;    /* 0: fixed size (default); 1: random size */
	int numOfSession = 1;    /* default */
	int numOfMemHandler = 1; /* default */
	int enableUnreg = 1;     /* 1: run lease mem(default); 0: don't run */
	int numOfPA = 0;	 /* default */
	int enableRegMem = 1; /* 1: run alloc/reg mem(default); 0: don't run */

	char **buf = NULL;
	uint64_t *pa = NULL;

	TEST_BEGIN("memory stress test");
	RESET_UNITTESTS;

	if (param != NULL) { /* get parameters */
		isRandomTest = param[0].value.b;
		numOfSession = param[1].value.a;
		numOfMemHandler = param[1].value.b;
		enableUnreg = param[2].value.a;
		numOfPA = param[2].value.b;
		enableRegMem = param[3].value.a;
	}

	mem_session_hdl = kmalloc_array(numOfSession,
				sizeof(KREE_SESSION_HANDLE), GFP_KERNEL);
	echo_session_hdl = kmalloc_array(numOfSession,
				sizeof(KREE_SESSION_HANDLE), GFP_KERNEL);
	mem_handle = kmalloc_array(numOfMemHandler,
				sizeof(KREE_SECUREMEM_HANDLE), GFP_KERNEL);

	if (!mem_session_hdl) {
		KREE_ERR("==> mem_session_hdl kmalloc fail. Stop.\n");
		goto _clean_mem;
	}

	if (!echo_session_hdl) {
		KREE_ERR("==> echo_session_hdl kmalloc fail. Stop.\n");
		goto _clean_mem;
	}

	if (!mem_handle) {
		KREE_ERR("==> mem_handle kmalloc fail. Stop.\n");
		goto _clean_mem;
	}

	for (i = 0; i < numOfSession; i++) {
		KREE_DEBUG(
			"[Gz_main.c]:[%s] ====> create session times = %d/%d\n",
			__func__, i, numOfSession);

		/* Connect to mem service */
		ret = KREE_CreateSession(mem_srv_name, &mem_session_hdl[i]);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create mem srv session");
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("mem_srv CreateSession Error: 0x%x, %d\n",
				 (uint32_t)mem_session_hdl[i], ret);
			goto _clean_mem;
		}
		KREE_DEBUG("get mem_session_hdl = 0x%x.\n", mem_session_hdl[i]);

		/* Connect to echo service */
		ret = KREE_CreateSession(echo_srv_name, &echo_session_hdl[i]);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo srv session");
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("echo_srv CreateSession Error: 0x%x, %d\n",
				 (uint32_t)echo_session_hdl[i], ret);
			goto _clean_mem;
		}
		KREE_DEBUG("get echo_session_hdl = 0x%x\n",
			   echo_session_hdl[i]);
	}

	hasSessCreated = 1;

	if (!enableRegMem)
		goto _clean_mem;

	/* Start to allocate secure memory/register shared memory */
	for (i = 0; i < numOfMemHandler; i++) {
		if (isRandomTest == 1) { /*random size */
			reqsize = _getRandomValue(shm_param->size);
		}

		switch (test_type) {
		case stresstest_secmem:
			ret = _test_alloc_securemem(mem_session_hdl[0],
						    echo_session_hdl[0],
						    mem_handle + i, reqsize);
			break;
		case stresstest_shdmem:
			ret = _test_reg_sharedmem(
				mem_session_hdl[0], echo_session_hdl[0],
				mem_handle + i, shm_param, numOfPA);
			break;
		case stresstest_chunkmem:
			if (i == 0) {
				buf = kmalloc(
					(numOfMemHandler * sizeof(char *)),
					GFP_KERNEL);
				pa = kmalloc(
					(numOfMemHandler * sizeof(uint64_t)),
					GFP_KERNEL);
			}
			pa[i] = init_shm_test(buf + i, shm_param->size);
			if (!buf[i]) {
				KREE_ERR(
					"[%s] ====> kmalloc Fail! [ buf[%d], Return null. Stop!].\n",
					__func__, i);
				continue;
			}
			shm_param->buffer = (void *)pa[i];
			ret = _test_alloc_chunkmem(
				mem_session_hdl[0], echo_session_hdl[0],
				mem_handle + i, shm_param, numOfPA);
			break;
		}
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "test alloc memory");
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR(
				"test alloc memory Error: mem_handle[%d] =%d, ret=%d [Stop]\n",
				i, (uint32_t)mem_handle[i], ret);
			goto _clean_mem;
		}
	}


	if (!enableUnreg)
		goto _clean_mem;

	/* release memory */
	switch (test_type) {
	case stresstest_secmem:
		ret = _test_release_securemem(numOfMemHandler,
					      mem_session_hdl[0], mem_handle);
		break;
	case stresstest_shdmem:
		ret = _test_release_sharedmem(numOfMemHandler,
					      mem_session_hdl[0], mem_handle);
		break;
	case stresstest_chunkmem:
		ret = _test_release_chunkmem(numOfMemHandler,
					     mem_session_hdl[0], mem_handle);
		/* If don't unreg chmem, check memory cannot read the memory
		 * space anymore. Phone will reboot.
		 * If unreg chmem, the memory can be read
		 */
		for (m = 0; m < numOfMemHandler; m++)
			verify_chm_result(*(buf + m), shm_param->size);

		if (buf != NULL) {
			for (i = 0; i < numOfMemHandler; i++)
				kfree(buf[i]);
			kfree(buf);
		}

		if (pa != NULL)
			kfree(pa);
		break;
	}

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "release memory");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("release memory Error: ret=%d [Stop]\n", ret);
		goto _clean_mem;
	}

_clean_mem:

	if (hasSessCreated) {
		for (i = 0; i < numOfSession; i++) {
			ret = KREE_CloseSession(echo_session_hdl[i]);
			CHECK_EQ(TZ_RESULT_SUCCESS, ret,
				 "close echo srv session");
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR(
					"KREE_CloseSession: echo_session Error:echo_session_hdl[%d] =0x%x, ret=%d\n",
					i, (uint32_t)echo_session_hdl[i], ret);
			}

			ret = KREE_CloseSession(mem_session_hdl[i]);
			CHECK_EQ(TZ_RESULT_SUCCESS, ret,
				 "close mem srv session");
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR(
					"KREE_CloseSession: mem_session Error:	mem_session_hdl[%d] =0x%x, ret=%d\n",
					i, (uint32_t)mem_session_hdl[i], ret);
			}
		}
	}

	kfree(mem_session_hdl);
	kfree(echo_session_hdl);
	kfree(mem_handle);

	TEST_END;
	REPORT_UNITTESTS;

	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);

	return 0;
}

static int _registerSharedmem_body(KREE_SESSION_HANDLE shm_session,
				   KREE_SHAREDMEM_PARAM *shm_param, int numOfPA,
				   KREE_SHAREDMEM_HANDLE *shm_handle)
{
	TZ_RESULT ret;

	if (numOfPA <= 0) {
		KREE_DEBUG("[%s]====> numOfPA <= 0 ==>[stop!].\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	KREE_DEBUG("[%s] ====> Call KREE_RegisterSharedmem().\n", __func__);
	ret = KREE_RegisterSharedmem(shm_session, shm_handle, shm_param);
	KREE_DEBUG("[%s] ====> shm_handle=%d\n", __func__, *shm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] KREE_RegisterSharedmem() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

static uint64_t init_shm_test(char **buf_p, int size)
{
	int i;
	uint64_t pa = 0;
	char *buf;
	int stat[2]; /* 0: for a; 1: for b; for test stat */

	TEST_BEGIN("init shared memory test");

	*buf_p = kmalloc(size, GFP_KERNEL);
	buf = *buf_p;
	if (!buf) {
		KREE_ERR("[%s] ====> kmalloc Fail.\n", __func__);
		return 0;
	}

	for (i = 0; i < (size - 1); i++)
		buf[i] = 'a'; /* set string:a */
	buf[i] = '\0';

	/* init */
	for (i = 0; i < 2; i++)
		stat[i] = 0;

	/* calculate stat info: char a, b */
	for (i = 0; i < (size); i++) {
		if (buf[i] == 'a')
			stat[0]++;
		else if (buf[i] == 'b')
			stat[1]++;
	}

	pa = (uint64_t)virt_to_phys((void *)buf);

	/* test string at Linux kernel */
	KREE_DEBUG("[%s] ====> buf(%llx) kmalloc size=%d, PA=%llx\n", __func__,
		   (uint64_t)buf, size, pa);
	CHECK_EQ((size - 1), stat[0], "input string a");
	CHECK_EQ(0, stat[1], "input string b");
	TEST_END;
	return pa;
}

static void verify_shm_result(char *buf, int size)
{
	int i;
	int stat[2]; /* 0: for a; 1: for b; for test stat */

	TEST_BEGIN("verify shared memory");

	KREE_DEBUG("[%s] ====> begin to verify shared memory results!\n",
		   __func__);

	if (buf == NULL) {
		KREE_DEBUG("[%s] ====> buf cannot access!\n", __func__);
		goto verify_shm_out;
	} else
		KREE_DEBUG("[%s] ====> buf can access!\n", __func__);
	/* == verify results == */
	for (i = 0; i < 2; i++)
		stat[i] = 0;

	/* calculate stat info: char a, b */
	for (i = 0; i < (size); i++) {
		if (buf[i] == 'a')
			stat[0]++;
		else if (buf[i] == 'b')
			stat[1]++;
	}
	KREE_DEBUG("==>gz_main.c: # of a= %d, # of b=%d\n", stat[0], stat[1]);
	CHECK_EQ(0, stat[0], "input string a");
	CHECK_EQ((size - 1), stat[1], "input string b");

verify_shm_out:
	TEST_END;
}

static void verify_chm_result(char *buf, int size)
{
	int i, count0 = 0;

	TEST_BEGIN("verify chunk memory");

	count0 = 0;

	for (i = 0; i < size - 1; i++)
		buf[i] = 'c';

	buf[i] = '\0';

	for (i = 0; i < size; i++) {
		if (buf[i] == 'c')
			count0++;
	}
	KREE_DEBUG("==>gz_main.c:verify chm result: # of c= %d\n", count0);
	CHECK_EQ(size - 1, count0, "char c");

	TEST_END;
}

static void fill_map_arr(uint64_t *mapAry, int *mapAry_idx, uint64_t pa,
			 int testArySize)
{
	int idx;

	for (idx = 0; idx < testArySize; idx++) {
		mapAry[*mapAry_idx] =
			(uint64_t)((uint64_t)pa + (uint64_t)(idx)*PAGE_SIZE);
		(*mapAry_idx)++;
	}
}

static int gz_test_shm_case1(union MTEEC_PARAM *param)
{
	uint64_t pa;
	char *buf = NULL;
	int numOfPA = 0;
	int size = 4272;
	TZ_RESULT ret;
	KREE_SHAREDMEM_PARAM shm_param;

	TEST_BEGIN("====> shared memory test [Test Case 1]. ");
	RESET_UNITTESTS;

	KREE_DEBUG("[%s] ====> GenieZone KREE test shm 1(continuous pages)\n",
		   __func__);
	KREE_DEBUG("[%s] ====> countinuous pages is processing.\n", __func__);

	if (param != NULL) {
		size = param[0].value.a;
		KREE_DEBUG("size resets to %d\n", size);
	}
	numOfPA = size / PAGE_SIZE;
	if ((size % PAGE_SIZE) != 0)
		numOfPA++;

	pa = init_shm_test(&buf, size);
	if (!buf) {
		KREE_ERR(
			"[%s] ====> kmalloc Fail! [ buf, Return null. Stop!].\n",
			__func__);
		goto shm_out;
	}

	KREE_DEBUG(
		"[%s] ====> kmalloc size=%d, &buf=%llx, PA=%llx, numOfPA=%d\n",
		__func__, size, (uint64_t)buf, pa, numOfPA);

	shm_param.buffer = (void *)pa;
	shm_param.size = size;
	shm_param.region_id = 0;
	/* case 1: continuous pages */
	shm_param.mapAry = NULL;

	if (param != NULL)
		param[2].value.b = numOfPA;

	ret = _stress_test_Memory_body(stresstest_shdmem, param, &shm_param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "shared memory process sys call. ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] ====> _stress_test_Memory_body() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		goto shm_out;
	}

	verify_shm_result(buf, size);

shm_out:
	KREE_DEBUG(
		"[%s] ====> test shared memory ends (test case:1[continuous pages]) ======.\n",
		__func__);

	kfree(buf);

	TEST_END;
	REPORT_UNITTESTS;
	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);
	return TZ_RESULT_SUCCESS;
}

static int gz_test_shm_case2(union MTEEC_PARAM *param)
{
	TZ_RESULT ret;
	KREE_SHAREDMEM_PARAM shm_param;
	int size = 2 * 4 * 1024; /* test str1: 8KB */
	char *buf = NULL;
	uint64_t pa;

	int size2 = 1 * 4 * 1024; /* test str2: 4KB */
	char *buf2 = NULL;
	uint64_t pa2;

	int size3 = 3 * 4 * 1024; /* test str3: 12KB */
	char *buf3 = NULL;
	uint64_t pa3;

	int mapAry_idx = 0;

	int testArySize = size / PAGE_SIZE;
	int testArySize2 = size2 / PAGE_SIZE;
	int testArySize3 = size3 / PAGE_SIZE;

	int numOfPA = (testArySize + testArySize2 + testArySize3);
	uint64_t mapAry[testArySize + testArySize2 + testArySize3 + 1];

	TEST_BEGIN("====> shared memory test [Test Case 2]. ");
	RESET_UNITTESTS;

	KREE_DEBUG("[%s] ====> GenieZone Linux kernel test shm starts.\n",
		   __func__);

	KREE_DEBUG("[%s]=====> discountinuous pages is processing.\n",
		   __func__);

	/* init test string 1 */
	pa = init_shm_test(&buf, size);
	if (!buf) {
		KREE_ERR(
			"[%s] ====> kmalloc Fail! [ buf, Return null. Stop!].\n",
			__func__);
		goto shm_out;
	}

	/* init test string 2 */
	pa2 = init_shm_test(&buf2, size2);
	if (!buf2) {
		KREE_ERR(
			"[%s] ====> kmalloc Fail! [ buf2, Return null. Stop!].\n",
			__func__);
		goto shm_out;
	}

	/* init test string 3 */
	pa3 = init_shm_test(&buf3, size3);
	if (!buf3) {
		KREE_ERR(
			"[%s] ====> kmalloc Fail! [ buf3, Return null. Stop!].\n",
			__func__);
		goto shm_out;
	}

	/* put PA lists in mapAry: mapAry[0]: # of PAs, mapAry[1]=PA1,
	 * mapAry[2]= PA2, ...
	 */
	mapAry_idx = 0;
	mapAry[mapAry_idx++] = (uint64_t)(numOfPA); /* # of PAs */
	fill_map_arr(mapAry, &mapAry_idx, pa, testArySize);
	fill_map_arr(mapAry, &mapAry_idx, pa2, testArySize2);
	fill_map_arr(mapAry, &mapAry_idx, pa3, testArySize3);

	/* case 2: discountinuous pages */
	shm_param.buffer = NULL;
	shm_param.size = size + size2 + size3;
	shm_param.mapAry = (void *)mapAry;

	if (param != NULL)
		param[2].value.b = numOfPA;

	ret = _stress_test_Memory_body(stresstest_shdmem, param, &shm_param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "shared memory process sys call. ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] ====> _stress_test_Memory_body() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		goto shm_out;
	}

	/* Input strings are updated in GZ echo server. show the updated string
	 * stat info
	 * verify update data  (all 'a' are replaced with 'b')
	 */
	verify_shm_result(buf, size);
	verify_shm_result(buf2, size2);
	verify_shm_result(buf3, size3);

shm_out:

	KREE_DEBUG("[%s] ====> test [case 2] shared memory ends ======.\n",
		   __func__);

	kfree(buf);
	kfree(buf2);
	kfree(buf3);

	TEST_END;
	REPORT_UNITTESTS;
	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);
	return TZ_RESULT_SUCCESS;
}

static int gz_test_shm_case3(union MTEEC_PARAM *param)
{
	int numOfCPGroup = 10;     /* default: 10 pages (PAs) */
	int numOfPageInAGroup = 1; /* 1, 2 (all OK) */

	uint64_t startPA = 0x0000000;
	int numOfChar = 0;

	int m = 0, n = 0;
	int i = 0;
	int str_cnt = 0;
	int stat[2];   /* 0: for a; 1: for b; */
	int total_PAs; /* 70*2 = 140 */

	char **testAry;
	uint64_t *paAryOftestAry;
	uint64_t *mapAry;

	TZ_RESULT ret;
	KREE_SHAREDMEM_PARAM shm_param;
	int mapAry_idx = 0;

	TEST_BEGIN("====> shared memory test [Test Case 3].");

	if (param != NULL) {
		numOfCPGroup = param[0].value.a;
		KREE_DEBUG("numOfCPGroup resets to %d\n", numOfCPGroup);
	}

	total_PAs = numOfCPGroup * numOfPageInAGroup; /* 70*2 = 140 */
	mapAry = kmalloc((total_PAs + 1) * sizeof(uint64_t), GFP_KERNEL);

	if (!mapAry) {
		KREE_DEBUG("==> mapAry kmalloc fail. Stop.\n");
		return -1;
	}

	paAryOftestAry = kmalloc((numOfCPGroup) * sizeof(uint64_t), GFP_KERNEL);
	if (!paAryOftestAry) {
		KREE_DEBUG("==> paAryOftestAry kmalloc fail. Stop.\n");
		return -1;
	}

	numOfChar = (int)((numOfCPGroup * numOfPageInAGroup) * PAGE_SIZE
			  - numOfCPGroup);

	RESET_UNITTESTS;
	KREE_DEBUG("==> numOfCPGroup = %d, following call kmalloc().\n",
		   numOfCPGroup);
	testAry = kmalloc((numOfCPGroup * sizeof(char *)), GFP_KERNEL);

	for (m = 0; m < numOfCPGroup; m++) {
		testAry[m] =
			kmalloc((numOfPageInAGroup * PAGE_SIZE), GFP_KERNEL);
		if (!testAry[m])
			KREE_ERR(
				"[%s] ====> kmalloc Fail! [ buf, Return null. Stop!].\n",
				__func__);
		paAryOftestAry[m] = (uint64_t)virt_to_phys((void *)testAry[m]);
	}

	mapAry[mapAry_idx++] = total_PAs;
	KREE_DEBUG("===> test total # of PAs: %d\n", total_PAs);

	for (m = 1; m < (numOfCPGroup + 1); m++) {	/* 1 to 70 groups */
		for (n = 0; n < numOfPageInAGroup; n++) { /* 2 PAs in a group */
			startPA = paAryOftestAry[m - 1];
			mapAry[mapAry_idx++] = (uint64_t)(
				(uint64_t)startPA + (uint64_t)(n)*PAGE_SIZE);
		}
	}

	KREE_DEBUG("===> mapAry[0] = numOfPA= 0x%x\n", (uint32_t)mapAry[0]);

	/* init string value */
	for (m = 0; m < numOfCPGroup; m++) {
		for (i = 0; i < ((numOfPageInAGroup * PAGE_SIZE) - 1); i++) {
			testAry[m][i] = 'a';
			str_cnt++;
		}
		testAry[m][i] = '\0';
	}

	KREE_DEBUG("[%s]====> input string in Linux #of [a] (str_cnt) = %5d\n",
		   __func__, str_cnt);

	/* verify input data */
	for (i = 0; i < 2; i++)
		stat[i] = 0;

	for (m = 0; m < numOfCPGroup; m++) {
		for (i = 0; i < (numOfPageInAGroup * PAGE_SIZE); i++) {
			if (testAry[m][i] == 'a')
				stat[0]++;
			else if (testAry[m][i] == 'b')
				stat[1]++;
		}
	}

	KREE_DEBUG("[%s]======> input string: [original]'s char stat:\n",
		   __func__);
	KREE_DEBUG(
		"[%s]====> input string in Linux #of [a] = %5d  (=%5d ?) -> [%s]\n",
		__func__, stat[0], numOfChar,
		(stat[0] == numOfChar) ? "Successful" : "Fail");
	KREE_DEBUG(
		"[%s]====> input string in Linux #of [b] = %5d  (=%5d ?) -> [%s]\n",
		__func__, stat[1], 0, (stat[1] == 0) ? "Successful" : "Fail");

	CHECK_EQ(numOfChar, stat[0], "input string a");
	CHECK_EQ(0, stat[1], "input string b");

	/* case 3: discountinuous pages */
	shm_param.buffer = NULL;
	shm_param.size = total_PAs * 4096;
	shm_param.mapAry = (void *)mapAry;

	if (param != NULL)
		param[2].value.b = total_PAs;

	ret = _stress_test_Memory_body(stresstest_shdmem, param, &shm_param);

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "shared memory process sys call. ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] ====> _stress_test_Memory_body() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		goto shm_out;
	}

	/* Input strings are updated in GZ echo server. show the updated string
	 * stat info
	 * verify update data  (all 'a' are replaced with 'b')
	 */
	for (i = 0; i < 2; i++)
		stat[i] = 0;

	for (m = 0; m < numOfCPGroup; m++) {
		for (i = 0; i < (numOfPageInAGroup * PAGE_SIZE); i++) {
			if (testAry[m][i] == 'a')
				stat[0]++;
			else if (testAry[m][i] == 'b')
				stat[1]++;
		}
	}

	KREE_DEBUG(
		"[%s]====> updated string from GZ's echo server [upated: a-->b]:\n",
		__func__);
	KREE_DEBUG(
		"[%s]====> input string in Linux #of [a] = %5d  (=%5d ?) -> [%s]\n",
		__func__, stat[0], 0, (stat[0] == 0) ? "Successful" : "Fail");
	KREE_DEBUG(
		"[%s]====> input string in Linux #of [b] = %5d  (=%5d ?) -> [%s]\n",
		__func__, stat[1], numOfChar,
		(stat[1] == ((numOfCPGroup * numOfPageInAGroup) * PAGE_SIZE
			     - numOfCPGroup))
			? "Successful"
			: "Fail");

	CHECK_EQ(0, stat[0], "input string a");
	CHECK_EQ(numOfChar, stat[1], "input string b");

shm_out:
	KREE_DEBUG("[%s] ====> test [case 3] shared memory ends ======.\n",
		   __func__);

	for (m = 0; m < numOfCPGroup; m++)
		kfree(testAry[m]);
	kfree(testAry);

	kfree(mapAry);
	kfree(paAryOftestAry);

	TEST_END;
	REPORT_UNITTESTS;

	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);

	return TZ_RESULT_SUCCESS;
}

static int _gz_test_shm_body(union MTEEC_PARAM *param)
{
	gz_test_shm_case1(param);
	gz_test_shm_case2(param);
	gz_test_shm_case3(param);

	return TZ_RESULT_SUCCESS;
}

static int gz_test_shm(void *arg)
{
	_gz_test_shm_body(NULL);

	return TZ_RESULT_SUCCESS;
}


static int gz_abort_test(void *args)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE session;
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
			   (uint32_t)session, ret);
	ret = KREE_CloseSession(session);

	return 0;
}


static int stress_loop_test(int test_type, int loopno, union MTEEC_PARAM *param)
{
	int i;
	char *str;
	int STRESS_times = loopno;
	KREE_SHAREDMEM_PARAM shm_param;

	shm_param.buffer = NULL;
	shm_param.mapAry = NULL;
	shm_param.size = 0;

	str = kmalloc(20, GFP_KERNEL);

	strcpy(str, "NON");

	stress_test_init();
	switch (test_type) {

	case stresstest_secmem: /* secure memory test */
		strcpy(str, "secure memory");
		KREE_TESTSHOW("==> stresstest_secmem run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			shm_param.size = param[0].value.a;
			_stress_test_Memory_body(stresstest_secmem, param,
						 &shm_param);
		}
		break;
	case stresstest_shdmem: /* shared memory test */
	case stresstest_shdmem_case1:
		strcpy(str, "shared memory");
		KREE_TESTSHOW("==> stresstest_shdmem case 1 run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			_gz_test_shm_body(param);
		}
		break;
	case stresstest_shdmem_case2:
		/* shared memory test: input numOfCPGroup */
		strcpy(str, "shared memory");
		KREE_TESTSHOW("==> stresstest_shdmem case 2 run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			gz_test_shm_case3(param);
		}
		break;
	case stresstest_shdmem_case3:
		/* shared memory test: radom size+multi-handler */
		strcpy(str, "shared memory");
		KREE_DEBUG("==> stresstest_shdmem case 3 run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			gz_test_shm_case1(param);
		}
		break;
	case stresstest_ipc: /* ipc-test */
		strcpy(str, "ipc test");
		KREE_TESTSHOW("==> stresstest_ipc run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			gz_tipc_test(NULL);
		}
		break;
	case stresstest_session: /* create/close session test */
		strcpy(str, "create/close session");
		KREE_TESTSHOW("==> stresstest_session run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			_stress_test_Memory_body(stresstest_session, param,
						 &shm_param);
		}
		break;
	case stresstest_chunkmem: /*chunk memory test*/
		strcpy(str, "chunkmem");
		KREE_TESTSHOW("==> stresstest_chunkmem run.\n");
		for (i = 1; i <= STRESS_times; i++) {
			KREE_DEBUG(
				"[Gz_main.c]:[%s] ====> test times = %d/%d\n",
				__func__, i, STRESS_times);
			_chunk_memory_test_body(param);
		}
		break;
	}
	stress_test_prt_result(str);

	kfree(str);
	return TZ_RESULT_SUCCESS;
}

static void init_test_param(union MTEEC_PARAM *param)
{
	param[0].value.a = 0; /*size*/
	param[0].value.b = 0; /*isRandom*/

	param[1].value.a = 1; /*numOfSession*/
	param[1].value.b = 1; /*numOfHandler*/

	param[2].value.a = 1; /*enableUnregMem*/
	param[2].value.b = 0; /*numOfPA*/

	param[3].value.a = 1; /*enableRegMem*/
	param[3].value.b = 0; /*undefined*/

	/*
	 * KREE_DEBUG("==> [0].a=%d, [0].b=%d, [1].a=%d, [1].b=%d, [2].a=%d,
	 *[2].b=%d\n"
	 *	, param[0].value.a, param[0].value.b, param[1].value.a,
	 *param[1].value.b
	 *	, param[2].value.a, param[2].value.b);
	 */
}

struct gz_stress_setup {
	char *str;
	uint32_t cmd;
	uint32_t runs;
	union MTEEC_PARAM param[4];
};

/* clang-format off */
#define stress_test_runs 1000
#define max_stress_test_param 4
static const struct gz_stress_setup stress_param[] = {
	[0] = {.str = "ipc test case1: 100 times",
	       .cmd = (uint32_t)stresstest_ipc,
	       .runs = 100},
	[1] = {.str = "ipc test case1: 1000 times",
	       .cmd = (uint32_t)stresstest_ipc,
	       .runs = 1000},
	[2] = {.str = "ipc test case1: 10000 times",
	       .cmd = (uint32_t)stresstest_ipc,
	       .runs = 10000},
	[3] = {.str = "secure mem: 128KB+1 mem_handlers",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 131072,
				.value.b = 0xffffffff} } },
	[4] = {.str = "secure mem: 256KB+1 mem_handlers",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 262144,
				.value.b = 0xffffffff} } },
	[5] = {.str = "secure mem: 1 mem_handlers+radom size (<=200KB)",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 200000, .value.b = 1} } },
	[6] = {.str = "secure mem: 1KB+5 mem_handlers",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 1024, .value.b = 0xffffffff},
			 [1] = {.value.b = 5, .value.a = 0xffffffff} } },
	[7] = {.str = "secure mem: 1KB+10 mem_handlers",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 1024, .value.b = 0xffffffff},
			 [1] = {.value.b = 10, .value.a = 0xffffffff} } },
	[8] = {.str = "secure mem: 1KB+20 mem_handlers",
	       .cmd = (uint32_t)stresstest_secmem,
	       .param = {[0 ... 3].value.a = 0xffffffff,
			 [0 ... 3].value.b = 0xffffffff,
			 [0] = {.value.a = 1024, .value.b = 0xffffffff},
			 [1] = {.value.b = 20, .value.a = 0xffffffff} } },
	[9] = {.str = "shared mem case 1: (3 cases)",
	       .cmd = (uint32_t)stresstest_shdmem},
	[10] = {.str = "shared mem case 2: in_numOfCPGroup=50(200KB)",
		.cmd = (uint32_t)stresstest_shdmem_case2,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 50,
				 .value.b = 0xffffffff} } },
	[11] = {.str = "shared mem case 2: in_numOfCPGroup=150(600KB)",
		.cmd = (uint32_t)stresstest_shdmem_case2,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 150,
				 .value.b = 0xffffffff} } },
	[12] = {.str = "shared mem case 2: in_numOfCPGroup=250(1MB)",
		.cmd = (uint32_t)stresstest_shdmem_case2,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 250,
				 .value.b = 0xffffffff} } },
	[13] = {.str = "shared mem case 2: in_numOfCPGroup=500(2MB)",
		.cmd = (uint32_t)stresstest_shdmem_case2,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 500,
				 .value.b = 0xffffffff} } },
	[14] = {.str = "shared mem case 3: 20 handler+random size (<20KB)",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 20480, .value.b = 1},
			  [1] = {.value.b = 20,
				 .value.a = 0xffffffff} } },
	[15] = {.str = "shared mem case 3: 512 handler+random size (<20KB)",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 20480, .value.b = 1},
			  [1] = {.value.b = 512,
				 .value.a = 0xffffffff} } },
	[16] = {.str = "shared mem case 3: 2 handler+size 40KB",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 40960,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 2, .value.a = 0xffffffff} } },
	[17] = {.str = "shared mem case 3: 2 handler+size 1MB",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 1024000,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 2, .value.a = 0xffffffff} } },
	[18] = {.str = "shared mem case 3: 10 handler+size 1MB",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 1024000,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 10,
				 .value.a = 0xffffffff} } },
	[19] = {.str = "shared mem case 3: 20 handler+size 1MB",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 1024000,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 20,
				 .value.a = 0xffffffff} } },
	[20] = {.str = "shared mem case 3: 2 handler+size 5MB",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 5120000,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 2, .value.a = 0xffffffff} } },
	[21] = {.str =
		"shared mem case 3: 20 handler+size 1MB+disable release mem",
		.cmd = (uint32_t)stresstest_shdmem_case3,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [0] = {.value.a = 1024000,
				 .value.b = 0xffffffff},
			  [1] = {.value.b = 20, .value.a = 0xffffffff},
			  [2] = {.value.a = 0, .value.b = 0xffffffff} } },
	[22] = {.str = "create session: 3 mem+echo session (total:6)",
		.cmd = (uint32_t)stresstest_session,
		.param = {[0 ... 3].value.a = 0xffffffff,
			  [0 ... 3].value.b = 0xffffffff,
			  [1] = {.value.a = 3, .value.b = 0xffffffff},
			  [2] = {.value.a = 0, .value.b = 0xffffffff},
			  [3] = {.value.a = 0, .value.b = 0xffffffff} } },
	{.str = "EOF"}
};
/* clang-format off */

static int stress_test_main(void *argv)
{
	int i = 0;
	int j = 0;
	int def_loopno = 0;
	int ret = 0;
	int all_zero_param;
	union MTEEC_PARAM *param;

	param = kmalloc_array((int)max_stress_test_param,
			sizeof(union MTEEC_PARAM), GFP_KERNEL);

	while (1) {
		if (stress_param[i].str == NULL) {
			i++;
			continue;
		}

		if (!strcmp(stress_param[i].str, "EOF"))
			break;

		KREE_TESTSHOW("[%d]==>[stress test] %s\n", i,
			      stress_param[i].str);

		if (stress_param[i].runs == 0)
			def_loopno = (int)stress_test_runs;
		else
			def_loopno = stress_param[i].runs;

		init_test_param(param);
		all_zero_param = 0;
		for (j = 0; j < (int)max_stress_test_param; j++) {
			if (stress_param[i].param[j].value.a != 0xffffffff)
				param[j].value.a =
					stress_param[i].param[j].value.a;

			if (stress_param[i].param[j].value.b != 0xffffffff)
				param[j].value.b =
					stress_param[i].param[j].value.b;

			if (stress_param[i].param[j].value.a == 0)
				all_zero_param++;

			if (stress_param[i].param[j].value.b == 0)
				all_zero_param++;
		}

		if (all_zero_param == ((int)max_stress_test_param * 2))
			ret = stress_loop_test((int)stress_param[i].cmd,
					       def_loopno, NULL);
		else
			ret = stress_loop_test((int)stress_param[i].cmd,
					       def_loopno, param);

		i++;
	}
	kfree(param);

	return TZ_RESULT_SUCCESS;
}

static int stress_test_main_infinite(void *args)
{
	for (;;)
		stress_test_main(args);

	return TZ_RESULT_SUCCESS;
}

static int _chunk_memory_test_body(union MTEEC_PARAM *param)
{
	int numOfPA = 0;

	/*limit: current chmem impl. need continuous pages*/
	int in_size = 24576; /*default test: 6 pages*/
	int size = in_size;
	TZ_RESULT ret;
	KREE_SHAREDMEM_PARAM shm_param;

	TEST_BEGIN("====> chunk memory test");
	RESET_UNITTESTS;

	KREE_DEBUG("[%s] ====> chunk memory test\n", __func__);

	if (param != NULL) {
		size = param[0].value.a;
		KREE_DEBUG("size resets to %d\n", size);
	}
	numOfPA = size / PAGE_SIZE;
	if ((size % PAGE_SIZE) != 0) {
		numOfPA++;
		size = PAGE_SIZE * numOfPA;
	}

	shm_param.size = size;
	shm_param.mapAry = NULL;

	if (param != NULL)
		param[2].value.b = numOfPA;

	ret = _stress_test_Memory_body(stresstest_chunkmem, param, &shm_param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "chunk memory process sys call. ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"[%s] ====> _stress_test_Memory_body() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
		goto chunkmem_out;
	}

chunkmem_out:
	KREE_DEBUG("[%s] ====> test chunk memory ends ======.\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;
	add_stresstest_total(_uts_total);
	add_stresstest_failed(_uts_failed);
	return TZ_RESULT_SUCCESS;
}

#if 0
static int chunk_memory_test1(void)
{
	KREE_DEBUG("==> Run chunk_memory_test1.\n");
	_chunk_memory_test_body(NULL);
	return TZ_RESULT_SUCCESS;
}

static int chunk_memory_test2(void)
{
	union MTEEC_PARAM *param;

	KREE_DEBUG("==> Run chunk_memory_test2.\n");
	param = kmalloc((int) max_stress_test_param *
			sizeof(union MTEEC_PARAM), GFP_KERNEL);
	init_test_param(param);
	param[0].value.a = 40960;	/*size*/
	param[1].value.b = 2;		/*numOfHandler*/

	_chunk_memory_test_body(param);

	kfree(param);
	return TZ_RESULT_SUCCESS;
}
#endif

static int test_MD_appendSecureChmem(void *args)
{
	int in_size = (10 * 4096); /*default test chm size: 10 pages*/
	int size = in_size;

	union MTEEC_PARAM param[4];
	uint32_t paramTypes;

	uint64_t pa;
	char *buf = NULL;

	TZ_RESULT ret;
	KREE_SESSION_HANDLE echo_session;

	TEST_BEGIN("====> start: test MD appendSecureChmem");
	RESET_UNITTESTS;

	KREE_DEBUG("====> start: %s\n", __func__);

	ret = KREE_CreateSession(echo_srv_name, &echo_session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo_session");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("CreateSession(echo_session) Error: 0x%x\n", ret);
		goto md_out;
	}
	KREE_DEBUG("[OK] echo_session=0x%x\n", (uint32_t)echo_session);

	/*create chunk memory space from Linux*/
	pa = init_shm_test(&buf, size);
	if (!buf) {
		KREE_ERR("[%s] ====> kmalloc test buffer Fail!.\n", __func__);
		goto md_out;
	}

	/*high */
	param[0].value.a = (uint32_t)((uint64_t)pa >> 32);
	/*low  */
	param[0].value.b = (uint32_t)((uint64_t)pa & (0x00000000ffffffff));
	param[1].value.a = size; /* chunk memory size */

	KREE_DEBUG("==> input pa=0x%llx (high=0x%x, low=0x%x), size=0x%x\n", pa,
		   param[0].value.a, param[0].value.b, param[1].value.a);

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	/*TZCMD_TEST_MD_APPEND_CHMEM: 9988*/
	KREE_TeeServiceCall(echo_session, 0x9988, paramTypes, param);


	ret = KREE_CloseSession(echo_session);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo_session");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("CloseSession Err:echo_session=0x%x, ret=%d\n",
			 (uint32_t)echo_session, ret);
	}
	KREE_DEBUG("[OK]Close session. echo_session=0x%x\n",
		   (uint32_t)echo_session);

md_out:
	if (buf != NULL)
		kfree(buf);

	KREE_DEBUG("====> %s ends ======.\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;

	return TZ_RESULT_SUCCESS;
}
/************* kernel module file ops (dummy) ****************/
struct UREE_SHAREDMEM_PARAM {
	uint32_t buffer; /* FIXME: userspace void* is 32-bit */
	uint32_t size;
	uint32_t mapAry;
	uint32_t region_id;
};

struct UREE_SHAREDMEM_PARAM_US {
	uint64_t buffer; /* FIXME: userspace void* is 32-bit */
	uint32_t size;
	uint32_t region_id;
};

struct user_shm_param {
	uint32_t session;
	uint32_t shm_handle;
	struct UREE_SHAREDMEM_PARAM_US param;
};

/************ to close session while driver is cancelled *************/
struct session_info {
	int handle_num;		      /*num of session handles*/
	KREE_SESSION_HANDLE *handles; /*session handles*/
	struct mutex mux;
};

#define queue_SessionNum 32
#define non_SessionID (0xffffffff)

static int _init_session_info(struct file *fp)
{
	struct session_info *info;
	int i;

	info = kmalloc(sizeof(struct session_info), GFP_KERNEL);
	if (!info)
		return TZ_RESULT_ERROR_GENERIC;

	info->handles = kmalloc_array(queue_SessionNum,
				sizeof(KREE_SESSION_HANDLE), GFP_KERNEL);
	if (!info->handles) {
		kfree(info);
		KREE_ERR("info->handles malloc fail. stop!\n");
		return TZ_RESULT_ERROR_GENERIC;
	}
	info->handle_num = queue_SessionNum;
	for (i = 0; i < info->handle_num; i++)
		info->handles[i] = non_SessionID;

	mutex_init(&info->mux);
	fp->private_data = info;
	return TZ_RESULT_SUCCESS;
}

static int _free_session_info(struct file *fp)
{
	struct session_info *info;
	int i, num;

	KREE_DEBUG("====> [%d] [start] %s is running.\n", __LINE__, __func__);

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	num = info->handle_num;
	for (i = 0; i < num; i++) {

		if (info->handles[i] == non_SessionID)
			continue;
		KREE_CloseSession(info->handles[i]);
		KREE_DEBUG("=====> session handle[%d] =%d is closed.\n", i,
			   info->handles[i]);

		info->handles[i] = (KREE_SESSION_HANDLE)non_SessionID;
	}

	/* unlock */
	fp->private_data = 0;
	mutex_unlock(&info->mux);

	kfree(info->handles);
	kfree(info);

	KREE_DEBUG("====> [%d] end of %s().\n", __LINE__, __func__);

	return TZ_RESULT_SUCCESS;
}


static int _register_session_info(struct file *fp, KREE_SESSION_HANDLE handle)
{
	struct session_info *info;
	int i, num, nspace, ret = -1;
	void *ptr;

	KREE_DEBUG(
		"====> [%d] _register_session_info is calling. in_handleID = %d\n",
		__LINE__, handle);
	if (handle < 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	/* find empty space. */
	num = info->handle_num;
	for (i = 0; i < num; i++) {
		if (info->handles[i] == non_SessionID) {
			ret = i;
			break;
		}
	}

	/* Try grow the space */
	if (ret == -1) {

		nspace = num * 2;
		ptr = krealloc(info->handles,
			       nspace * sizeof(KREE_SESSION_HANDLE),
			       GFP_KERNEL);
		if (ptr == 0) {
			mutex_unlock(&info->mux);
			return TZ_RESULT_ERROR_GENERIC;
		}

		ret = num;
		info->handle_num = nspace;
		info->handles = (int *)ptr;
		memset(&info->handles[num], (KREE_SESSION_HANDLE)non_SessionID,
		       (nspace - num) * sizeof(KREE_SESSION_HANDLE));
	}

	if (ret >= 0)
		info->handles[ret] = handle;

	KREE_DEBUG("=====> session handle[%d]=%d is reg.\n", ret, handle);

	/* unlock */
	mutex_unlock(&info->mux);

	return TZ_RESULT_SUCCESS;
}


static int _unregister_session_info(struct file *fp,
				    KREE_SESSION_HANDLE in_handleID)
{
	struct session_info *info;
	int ret = TZ_RESULT_ERROR_GENERIC;
	int i;

	KREE_DEBUG(
		"====> [%d] _unregister_session_info is calling. in_handleID = %d\n",
		__LINE__, in_handleID);
	if (in_handleID < 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	info = (struct session_info *)fp->private_data;

	/* lock */
	mutex_lock(&info->mux);

	for (i = 0; i < info->handle_num; i++) {
		if (info->handles[i] == in_handleID) {
			info->handles[i] = (KREE_SESSION_HANDLE)non_SessionID;
			KREE_DEBUG("=====> session handle[%d]=%d is unreg.\n",
				   i, in_handleID);
			ret = TZ_RESULT_SUCCESS;
			break;
		}
	}

	KREE_DEBUG("====> [%d] [after] _free_session_info is calling. ret=%d\n",
		   __LINE__, ret);

	/* unlock */
	mutex_unlock(&info->mux);

	return ret;
}

static int gz_dev_open(struct inode *inode, struct file *filp)
{
	KREE_DEBUG("====>gz_dev_open & _init_session_info is calling.\n");
	_init_session_info(filp);
	return 0;
}

static int gz_dev_release(struct inode *inode, struct file *filp)
{
	KREE_DEBUG("====>[before] gz_dev_release is calling.\n");
	_free_session_info(filp);
	KREE_DEBUG("====>[after] gz_dev_release is calling.\n");
	return 0;
}


static ssize_t gz_dev_read(struct file *filp, char __user *buf, size_t count,
			   loff_t *pos)
{
	*pos = 0;
	return 0;
}

static ssize_t gz_dev_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *pos)
{
	return count;
}

TZ_RESULT get_US_PAMapAry(struct user_shm_param *shm_data,
			  KREE_SHAREDMEM_PARAM *shm_param, int *numOfPA,
			  struct MTIOMMU_PIN_RANGE_T *pin, uint64_t *map_p)
{
	unsigned long cret;
	struct page **page;
	int i;
	unsigned long *pfns;
	struct page **delpages;

	KREE_DEBUG("====> [%s] get_US_PAMapAry is calling.\n", __func__);
	KREE_DEBUG(
		"====> session: %u, shm_handle: %u, param.size: %u, param.buffer: 0x%llx\n",
		(*shm_data).session, (*shm_data).shm_handle,
		(*shm_data).param.size, (*shm_data).param.buffer);

	if (((*shm_data).param.size <= 0) || (!(*shm_data).param.buffer)) {
		KREE_DEBUG(
			"[%s]====> param.size <= 0 OR !param.buffer ==>[stop!].\n",
			__func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*init value*/
	pin = NULL;
	map_p = NULL;
	(*shm_param).buffer = NULL;
	(*shm_param).size = 0;
	(*shm_param).mapAry = NULL;

	cret = TZ_RESULT_SUCCESS;
	/*
	 * map pages
	 */
	/*
	 * 1. get user pages
	 * note: 'pin' resource need to keep for unregister.
	 * It will be freed after unregisted.
	 */

	pin = kzalloc(sizeof(struct MTIOMMU_PIN_RANGE_T), GFP_KERNEL);
	if (pin == NULL) {
		KREE_DEBUG("[%s]====> pin is null.\n", __func__);
		cret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto us_map_fail;
	}
	pin->pageArray = NULL;
	cret = _map_user_pages(pin, (unsigned long)(*shm_data).param.buffer,
			       (*shm_data).param.size, 0);

	if (cret != 0) {
		pin->pageArray = NULL;
		KREE_DEBUG(
			"[%s]====> _map_user_pages fail. map user pages = 0x%x\n",
			__func__, (uint32_t)cret);
		cret = TZ_RESULT_ERROR_INVALID_HANDLE;
		goto us_map_fail;
	}
	/* 2. build PA table */
	map_p = kzalloc(sizeof(uint64_t) * (pin->nrPages + 1), GFP_KERNEL);
	if (map_p == NULL) {
		KREE_DEBUG("[%s]====> map_p is null.\n", __func__);
		cret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto us_map_fail;
	}
	map_p[0] = pin->nrPages;
	if (pin->isPage) {
		page = (struct page **)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++)
			map_p[1 + i] = (uint64_t)PFN_PHYS(
				page_to_pfn(page[i])); /* PA */
	} else {				       /* pfn */
		pfns = (unsigned long *)pin->pageArray;
		for (i = 0; i < pin->nrPages; i++)
			map_p[1 + i] = (uint64_t)PFN_PHYS(pfns[i]); /* get PA */
	}

	/* init register shared mem params */
	(*shm_param).buffer = NULL;
	(*shm_param).size = 0;
	(*shm_param).mapAry = (void *)map_p;

	*numOfPA = pin->nrPages;


us_map_fail:
	if (pin) {
		if (pin->pageArray) {
			delpages = (struct page **)pin->pageArray;
			if (pin->isPage) {
				for (i = 0; i < pin->nrPages; i++)
					put_page(delpages[i]);
				KREE_DEBUG(
					"===> gz_main.c[%d] after page release\n",
					__LINE__);
			}
			kfree(pin->pageArray);
			KREE_DEBUG(
				"===> gz_main.c[%d] after free pin->pageArray\n",
				__LINE__);
		}
		kfree(pin);
	}

	return cret;
}

/**************************************************************************
 *  DEV DRIVER IOCTL
 *  Ported from trustzone driver
 **************************************************************************/
static long tz_client_open_session(struct file *filep, unsigned long arg)
{
	struct kree_session_cmd_param param;
	unsigned long cret;
	char uuid[40];
	long len;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE handle;

	cret = copy_from_user(&param, (void *)arg, sizeof(param));
	if (cret)
		return -EFAULT;

	/* Check if can we access UUID string. 10 for min uuid len. */
	if (!access_ok(VERIFY_READ, param.data, 10))
		return -EFAULT;

	KREE_DEBUG("%s: uuid addr = 0x%llx\n", __func__, param.data);
	len = strncpy_from_user(uuid, (void *)(unsigned long)param.data,
				sizeof(uuid));
	if (len <= 0)
		return -EFAULT;

	uuid[sizeof(uuid) - 1] = 0;
	ret = KREE_CreateSession(uuid, &handle);
	param.ret = ret;
	param.handle = handle;

	KREE_DEBUG("===> tz_client_open_session: handle =%d\n", handle);
	if (handle >= 0)
		_register_session_info(filep, handle);

	cret = copy_to_user((void *)arg, &param, sizeof(param));
	if (cret)
		return cret;

	return 0;
}

static long tz_client_close_session(struct file *filep, unsigned long arg)
{
	struct kree_session_cmd_param param;
	unsigned long cret;
	TZ_RESULT ret;

	cret = copy_from_user(&param, (void *)arg, sizeof(param));
	if (cret)
		return -EFAULT;

	ret = KREE_CloseSession(param.handle);
	param.ret = ret;
	_unregister_session_info(filep, param.handle);

	cret = copy_to_user((void *)arg, &param, sizeof(param));
	if (cret)
		return -EFAULT;

	return 0;
}

static long tz_client_tee_service(struct file *file, unsigned long arg,
				  unsigned int compat)
{
	struct kree_tee_service_cmd_param cparam;
	unsigned long cret;
	uint32_t tmpTypes;
	union MTEEC_PARAM param[4], oparam[4];
	int i;
	TZ_RESULT ret;
	KREE_SESSION_HANDLE handle;
	void __user *ubuf;
	uint32_t ubuf_sz;

	cret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));
	if (cret) {
		KREE_ERR("%s: copy_from_user(msg) failed\n", __func__);
		return -EFAULT;
	}

	if (cparam.paramTypes != TZPT_NONE || cparam.param) {
		if (!access_ok(VERIFY_READ, cparam.param, sizeof(oparam)))
			return -EFAULT;

		cret = copy_from_user(oparam,
				      (void *)(unsigned long)cparam.param,
				      sizeof(oparam));
		if (cret) {
			KREE_ERR("%s: copy_from_user(param) failed\n",
				 __func__);
			return -EFAULT;
		}
	}

	handle = (KREE_SESSION_HANDLE)cparam.handle;
	KREE_DEBUG("%s: session handle = %u\n", __func__, handle);

	/* Parameter processing. */
	memset(param, 0, sizeof(param));
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_INPUT:
		case TZPT_VALUE_INOUT:
			param[i] = oparam[i];
			break;

		case TZPT_VALUE_OUTPUT:
		case TZPT_NONE:
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#ifdef CONFIG_COMPAT
			if (compat) {
				ubuf = compat_ptr(oparam[i].mem32.buffer);
				ubuf_sz = oparam[i].mem32.size;
			} else
#endif
			{
				ubuf = oparam[i].mem.buffer;
				ubuf_sz = oparam[i].mem.size;
			}

			KREE_DEBUG("%s: ubuf = %p, ubuf_sz = %u\n", __func__,
				   ubuf, ubuf_sz);

			if (type != TZPT_MEM_OUTPUT) {
				if (!access_ok(VERIFY_READ, ubuf, ubuf_sz)) {
					KREE_ERR("%s: cannnot read mem\n",
						 __func__);
					cret = -EFAULT;
					goto error;
				}
			}
			if (type != TZPT_MEM_INPUT) {
				if (!access_ok(VERIFY_WRITE, ubuf, ubuf_sz)) {
					KREE_ERR("%s: cannnot write mem\n",
						 __func__);
					cret = -EFAULT;
					goto error;
				}
			}

			if (ubuf_sz > GP_MEM_MAX_LEN) {
				KREE_ERR("%s: ubuf_sz larger than max(%d)\n",
					 __func__, GP_MEM_MAX_LEN);
				cret = -ENOMEM;
				goto error;
			}

			param[i].mem.size = ubuf_sz;
			param[i].mem.buffer =
				kmalloc(param[i].mem.size, GFP_KERNEL);
			if (!param[i].mem.buffer) {
				KREE_ERR("%s: kmalloc failed\n", __func__);
				cret = -ENOMEM;
				goto error;
			}

			if (type != TZPT_MEM_OUTPUT) {
				cret = copy_from_user(param[i].mem.buffer, ubuf,
						      param[i].mem.size);
				if (cret) {
					KREE_ERR("%s: copy_from_user failed\n",
						 __func__);
					cret = -EFAULT;
					goto error;
				}
			}
			break;

		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
			param[i] = oparam[i];
			break;

		default:
			ret = TZ_RESULT_ERROR_BAD_FORMAT;
			goto error;
		}
	}

	KREE_SESSION_LOCK(handle);
	ret = KREE_TeeServiceCall(handle, cparam.command, cparam.paramTypes,
				  param);
	KREE_SESSION_UNLOCK(handle);

	cparam.ret = ret;
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_VALUE_OUTPUT:
		case TZPT_VALUE_INOUT:
			oparam[i] = param[i];
			break;

		default:
		case TZPT_MEMREF_INPUT:
		case TZPT_MEMREF_OUTPUT:
		case TZPT_MEMREF_INOUT:
		case TZPT_VALUE_INPUT:
		case TZPT_NONE:
			break;

		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
#ifdef CONFIG_COMPAT
			if (compat)
				ubuf = compat_ptr(oparam[i].mem32.buffer);
			else
#endif
				ubuf = oparam[i].mem.buffer;

			if (type != TZPT_MEM_INPUT) {
				cret = copy_to_user(ubuf, param[i].mem.buffer,
						    param[i].mem.size);
				if (cret) {
					cret = -EFAULT;
					goto error;
				}
			}

			kfree(param[i].mem.buffer);
			break;
		}
	}

	/* Copy data back. */
	if (cparam.paramTypes != TZPT_NONE) {
		cret = copy_to_user((void *)(unsigned long)cparam.param, oparam,
				    sizeof(oparam));
		if (cret) {
			KREE_ERR("%s: copy_to_user(param) failed\n", __func__);
			return -EFAULT;
		}
	}


	cret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
	if (cret) {
		KREE_ERR("%s: copy_to_user(msg) failed\n", __func__);
		return -EFAULT;
	}
	return 0;

error:
	tmpTypes = cparam.paramTypes;
	for (i = 0; tmpTypes; i++) {
		enum TZ_PARAM_TYPES type = tmpTypes & 0xff;

		tmpTypes >>= 8;
		switch (type) {
		case TZPT_MEM_INPUT:
		case TZPT_MEM_OUTPUT:
		case TZPT_MEM_INOUT:
			kfree(param[i].mem.buffer);
			break;

		default:
			break;
		}
	}
	return cret;
}

/*used for test secure carema*/
uint64_t _sc_pa;
char *_sc_buf;

static int _sc_test_appendChm_body(KREE_SESSION_HANDLE appendchm_session,
				   KREE_SECUREMEM_HANDLE *appendchm_handle,
				   int in_chm_size)
{
	uint32_t chm_size = in_chm_size; /*test chm size*/
	uint32_t size = chm_size;

	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	KREE_SHAREDMEM_PARAM shm_param;

	KREE_DEBUG("====> %s run.\n", __func__);

	if (size <= 0) {
		KREE_DEBUG("====> chm_size <= 0 [Stop]\n");
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	if (appendchm_session == 0) {
		KREE_DEBUG("%s ====> append chm session= 0[Stop]\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*create chunk memory space from Linux*/
	_sc_buf = NULL;
	_sc_pa = init_shm_test(&_sc_buf, size);
	if (!_sc_buf) {
		KREE_ERR("[%s] ====> kmalloc test buffer _sc_buf Fail!.\n",
			 __func__);
		return TZ_RESULT_ERROR_GENERIC;
	}

	shm_param.buffer = (void *)_sc_pa;
	shm_param.size = size;
	shm_param.mapAry = NULL;

	/*append chunk memory*/
	ret = KREE_AppendSecureMultichunkmem(appendchm_session,
					     appendchm_handle, &shm_param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("KREE_AppendSecureMultichunkmem() Fail. ret=0x%x\n",
			 ret);

		if (_sc_buf != NULL)
			kfree(_sc_buf);

		return ret;
	}

	KREE_DEBUG("[OK] append chm OK. appendchm_handle=0x%x\n",
		   (uint32_t)*appendchm_handle);
	return ret;
}

static long _sc_test_appendChm(struct file *filep, unsigned long arg)
{
	int in_chm_size;

	struct kree_user_sc_param cparam;
	int ret;
	KREE_SESSION_HANDLE append_chm_session;
	KREE_SECUREMEM_HANDLE append_chm_handle;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	append_chm_session = cparam.chmp.append_chm_session;
	in_chm_size = cparam.size;

	if (in_chm_size <= 0) {
		KREE_ERR("%s: BAD_PARAMETERS: in_chm_size=%d\n", __func__,
			 in_chm_size);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	KREE_DEBUG("[%s]input append_chm_session =0x%x\n", __func__,
		   (uint32_t)append_chm_session);

	ret = _sc_test_appendChm_body(append_chm_session, &append_chm_handle,
				      in_chm_size);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: _sc_test_appendChm_body failed(0x%x)\n", __func__,
			 ret);
		return ret;
	}

	cparam.chmp.append_chm_handle = append_chm_handle;

	KREE_DEBUG("[%s]ret append_chm_handle=0x%x\n", __func__,
		   (uint32_t)append_chm_handle);

	/* copy result back to user */
	ret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: copy_to_user failed(0x%x)\n", __func__, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	return ret;
}

static int _sc_test_releaseChm_body(KREE_SESSION_HANDLE appendchm_session,
				    KREE_SECUREMEM_HANDLE appendchm_handle)
{
	TZ_RESULT ret;

	KREE_DEBUG("====> %s run.\n", __func__);

	if ((appendchm_session == 0) || (appendchm_handle == 0)) {
		KREE_DEBUG("%s ====> append chm session/handle = 0[Stop]\n",
			   __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*release chunk memory*/
	ret = KREE_ReleaseSecureMultichunkmem(appendchm_session,
					      appendchm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("KREE_ReleaseSecureMultichunkmem Error: ret=%d\n",
			 ret);
		goto out1;
	}
	KREE_DEBUG("[OK][%d] release chm OK. appendchm_handle=0x%x\n", __LINE__,
		   (uint32_t)appendchm_handle);

out1:

	if (_sc_buf != NULL)
		kfree(_sc_buf);

	KREE_DEBUG("[%s] ====> _sc_test_releaseChm ends ======.\n", __func__);

	return ret;
}

static long _sc_test_releaseChm(struct file *filep, unsigned long arg)
{
	struct kree_user_sc_param cparam;
	int ret;
	KREE_SESSION_HANDLE append_chm_session;
	KREE_SECUREMEM_HANDLE append_chm_handle;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	append_chm_session = cparam.chmp.append_chm_session;
	append_chm_handle = cparam.chmp.append_chm_handle;

	KREE_DEBUG("[%s]append_chm_handle=0x%x, append_chm_session =0x%x\n",
		   __func__, (uint32_t)append_chm_handle,
		   (uint32_t)append_chm_session);

	ret = _sc_test_releaseChm_body(append_chm_session, append_chm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: _sc_test_releaseChm_body failed(0x%x)\n",
			 __func__, ret);
		return ret;
	}

	return ret;
}

static int _sc_test_allocChm_body(KREE_SESSION_HANDLE alloc_chm_session,
				  KREE_SECUREMEM_HANDLE appendchm_handle,
				  KREE_ION_HANDLE *ION_Handle,
				  uint32_t alignment_size,
				  uint32_t allocchm_size, int zalloc)
{
	uint32_t alignment = alignment_size; /*PAGE_SIZE*/
	uint32_t alloc_size = allocchm_size;

	TZ_RESULT ret;

	KREE_SECUREMEM_HANDLE allocatechm_handle;

	KREE_DEBUG("====> %s run.\n", __func__);

	if (alloc_size <= 0) {
		KREE_DEBUG("====> alloc_size <= 0 [Stop]\n");
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	if ((alloc_chm_session == 0) || (appendchm_handle == 0)) {
		KREE_DEBUG(
			"[%s]alloc_chm_session or appendchm_handle=0[Stop]\n",
			__func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	*ION_Handle = 0;

	/* allocate chm */
	if (zalloc)
		ret = KREE_ION_ZallocChunkmem(
			alloc_chm_session, appendchm_handle,
			&allocatechm_handle, alignment, alloc_size);
	else
		ret = KREE_ION_AllocChunkmem(
			alloc_chm_session, appendchm_handle,
			&allocatechm_handle, alignment, alloc_size);
	if ((ret != TZ_RESULT_SUCCESS) || (allocatechm_handle == 0))
		KREE_ERR(
			"[%s] KREE_ION_AllocChunkmem() returns Fail. [Stop!]. ret=0x%x\n",
			__func__, ret);
	else {
		KREE_DEBUG(
			"[OK]Alloc chunk memory [chm_handle = 0x%x, mem_handle = 0x%x (alignment=%d, size=%d)]\n",
			appendchm_handle, allocatechm_handle, alignment,
			alloc_size);
		*ION_Handle = allocatechm_handle;
	}

	return ret;
}

static long _sc_test_allocChm(struct file *filep, unsigned long arg, int zalloc)
{
	struct kree_user_sc_param cparam;
	int ret;
	KREE_ION_HANDLE ION_Handle = 0;
	KREE_SECUREMEM_HANDLE append_chm_handle;
	KREE_SESSION_HANDLE alloc_chm_session;
	uint32_t allocchm_size;
	uint32_t alignment_size = PAGE_SIZE; /*4096*/

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	allocchm_size = cparam.size;
	alloc_chm_session = cparam.chmp.alloc_chm_session;
	append_chm_handle = cparam.chmp.append_chm_handle;
	alignment_size = cparam.alignment;

	KREE_DEBUG("[%s]input allocchm_size = 0x%x,alloc_chm_session=0x%x\n",
		   __func__, (uint32_t)allocchm_size,
		   (uint32_t)alloc_chm_session);

	if (allocchm_size <= 0) {
		KREE_DEBUG("[%s]====> allocchm_size <= 0 [stop!].\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	ret = _sc_test_allocChm_body(alloc_chm_session, append_chm_handle,
				     &ION_Handle, alignment_size, allocchm_size,
				     zalloc);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: _sc_test_allocChm failed(0x%x)\n", __func__, ret);
		return ret;
	}

	cparam.ION_handle = ION_Handle;

	KREE_DEBUG("[%s]ret ION_Handle=0x%x\n", __func__, (uint32_t)ION_Handle);

	/* copy result back to user */
	ret = copy_to_user((void *)arg, &cparam, sizeof(cparam));
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: copy_to_user failed(0x%x)\n", __func__, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	return ret;
}

static int _sc_test_refChm_body(KREE_SESSION_HANDLE alloc_chm_session,
				KREE_ION_HANDLE ION_Handle)
{
	TZ_RESULT ret;

	KREE_DEBUG("====> _sc_test_unrefChm\n");

	if (ION_Handle == 0) {
		KREE_DEBUG("====> ION_Handle = 0 [Stop]\n");
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*reference chm*/
	ret = KREE_ION_ReferenceChunkmem(alloc_chm_session, ION_Handle);
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] Unref Chmem Fail. [Stop!]. ret=0x%x\n", __func__,
			 ret);
	else
		KREE_DEBUG("[%s][OK]Unreference chunk memory OK.\n", __func__);

	return ret;
}

static long _sc_test_refChm(struct file *filep, unsigned long arg)
{

	struct kree_user_sc_param cparam;
	int ret;
	KREE_ION_HANDLE ION_Handle = 0;
	KREE_SESSION_HANDLE alloc_chm_session;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	alloc_chm_session = cparam.chmp.alloc_chm_session;
	ION_Handle = cparam.ION_handle;

	KREE_DEBUG("[%s]ION_Handle = 0x%x,alloc_chm_session=0x%x\n", __func__,
		   (uint32_t)ION_Handle, (uint32_t)alloc_chm_session);

	ret = _sc_test_refChm_body(alloc_chm_session, ION_Handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: _sc_test_allocChm failed(0x%x)\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int _sc_test_unrefChm_body(KREE_SESSION_HANDLE alloc_chm_session,
				  KREE_ION_HANDLE ION_Handle)
{
	TZ_RESULT ret;

	KREE_DEBUG("====> _sc_test_unrefChm\n");

	if (ION_Handle == 0) {
		KREE_DEBUG("====> ION_Handle = 0 [Stop]\n");
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*unreference chm: free*/
	ret = KREE_ION_UnreferenceChunkmem(alloc_chm_session, ION_Handle);
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] Unref Chmem Fail. [Stop!]. ret=0x%x\n", __func__,
			 ret);
	else
		KREE_DEBUG("[%s][OK]Unreference chunk memory OK.\n", __func__);

	return ret;
}

static long _sc_test_unrefChm(struct file *filep, unsigned long arg)
{

	struct kree_user_sc_param cparam;
	int ret;
	KREE_ION_HANDLE ION_Handle = 0;
	KREE_SESSION_HANDLE alloc_chm_session;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	alloc_chm_session = cparam.chmp.alloc_chm_session;
	ION_Handle = cparam.ION_handle;

	KREE_DEBUG("[%s]ION_Handle = 0x%x,alloc_chm_session=0x%x\n", __func__,
		   (uint32_t)ION_Handle, (uint32_t)alloc_chm_session);

	ret = _sc_test_unrefChm_body(alloc_chm_session, ION_Handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("%s: _sc_test_allocChm failed(0x%x)\n", __func__, ret);
		return ret;
	}

	return ret;
}

static long _sc_test_cp_chm2shm(struct file *filep, unsigned long arg)
{

	struct kree_user_sc_param cparam;
	int ret;
	KREE_ION_HANDLE ION_Handle = 0;
	KREE_SECUREMEM_HANDLE shm_handle;
	KREE_SESSION_HANDLE cp_session;
	uint32_t size;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	/*input params*/
	shm_handle = cparam.other_handle;
	ION_Handle = cparam.ION_handle; /*need to transform to mem_handle*/
	size = cparam.size;		/*alloc size*/
	cp_session = cparam.chmp.alloc_chm_session;

	KREE_DEBUG(
		"[%s] input: cp_session=0x%x, shm_handle=0x%x, ION_Handle=0x%x, size=0x%x\n",
		__func__, cp_session, shm_handle, ION_Handle, size);

	ret = KREE_ION_CP_Chm2Shm(cp_session, ION_Handle, shm_handle, size);

	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] KREE_ION_CP_Chm2Shm Fail. ret=0x%x\n", __func__,
			 ret);
	else
		KREE_DEBUG("[OK]KREE_ION_CP_Chm2Shm done\n");

	return ret;
}

static long _sc_test_upt_chmdata(struct file *filep, unsigned long arg)
{
	struct kree_user_sc_param cparam;
	int ret;
	KREE_ION_HANDLE ION_Handle = 0;
	KREE_SECUREMEM_HANDLE shm_handle;
	KREE_SESSION_HANDLE echo_session;
	union MTEEC_PARAM param[4];
	uint32_t size;

	/* copy param from user */
	ret = copy_from_user(&cparam, (void *)arg, sizeof(cparam));

	if (ret < 0) {
		KREE_ERR("%s: copy_from_user failed(%d)\n", __func__, ret);
		return ret;
	}

	/*input params*/
	shm_handle = cparam.other_handle;
	ION_Handle = cparam.ION_handle;
	size = cparam.size;

	/*create session for echo*/
	ret = KREE_CreateSession(echo_srv_name, &echo_session);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR(
			"echo_srv CreateSession (echo_session:0x%x) Error: %d\n",
			(uint32_t)echo_session, ret);
		return ret;
	}
	KREE_DEBUG("[OK] create echo_session=0x%x.\n", (uint32_t)echo_session);

	param[0].value.a = ION_Handle; /*need to transform to mem_handle*/
	param[1].value.a = size;       /*alloc size*/

	KREE_DEBUG("[%s] input: shm_handle=0x%x. ION_Handle=0x%x\n", __func__,
		   param[0].value.b, param[0].value.a);

	/*test: modify chm memory data*/
	ret = KREE_ION_AccessChunkmem(echo_session, param,
				      0x9989); /*TZCMD_TEST_CHM_UPT_DATA*/
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] modify chm memory data Fail. ret=0x%x\n",
			 __func__, ret);
	else
		KREE_DEBUG("[OK]modify chm memory data done\n");

	ret = KREE_CloseSession(echo_session);
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("KREE_CloseSession Error:echo_session=0x%x, ret=%d\n",
			 (uint32_t)echo_session, ret);
	else
		KREE_DEBUG("[OK] close session OK. echo_session=0x%x\n",
			   (uint32_t)echo_session);

	return ret;
}

static long _gz_ioctl(struct file *filep, unsigned int cmd, unsigned long arg,
		      unsigned int compat)

{
	int err;
	TZ_RESULT ret = 0;
	char __user *user_req;
	struct user_shm_param shm_data;
	struct kree_user_sc_param cparam;
	KREE_SHAREDMEM_PARAM shm_param;
	KREE_SHAREDMEM_HANDLE shm_handle;
	struct MTIOMMU_PIN_RANGE_T *pin = NULL;
	uint64_t *map_p = NULL;
	int numOfPA = 0;

	if (_IOC_TYPE(cmd) != MTEE_IOC_MAGIC)
		return -EINVAL;

	if (compat)
		user_req = (char __user *)compat_ptr(arg);
	else
		user_req = (char __user *)arg;

	switch (cmd) {
	case MTEE_CMD_SHM_REG:

		KREE_DEBUG("====> GZ_IOC_REG_SHAREDMEM ====\n");

		/* copy param from user */
		err = copy_from_user(&shm_data, user_req, sizeof(shm_data));
		if (err < 0) {
			KREE_ERR("%s: copy_from_user failed(%d)\n", __func__,
				 err);
			return err;
		}

		if ((shm_data.param.size <= 0) || (!shm_data.param.buffer)) {
			KREE_DEBUG(
				"[%s]====> param.size <= 0 OR !param.buffer ==>[stop!].\n",
				__func__);
			return TZ_RESULT_ERROR_BAD_PARAMETERS;
		}

		KREE_DEBUG(
			"[%s]sizeof(shm_data):0x%x, session:%u, shm_handle:%u, size:%u, &buffer:0x%llx\n",
			__func__, (uint32_t)sizeof(shm_data), shm_data.session,
			shm_data.shm_handle, shm_data.param.size,
			shm_data.param.buffer);
		ret = get_US_PAMapAry(&shm_data, &shm_param, &numOfPA, pin,
				      map_p);
		if (ret != TZ_RESULT_SUCCESS)
			KREE_ERR(
				"[%s] ====> get_us_PAMapAry() returns Fail. [Stop!]. ret=0x%x\n",
				__func__, ret);
		shm_param.region_id = shm_data.param.region_id;
		if (ret == TZ_RESULT_SUCCESS) {
			ret = _registerSharedmem_body(shm_data.session,
						      &shm_param, numOfPA,
						      &shm_handle);
			if (ret != TZ_RESULT_SUCCESS)
				KREE_ERR(
					"[%s] ====> shm_mem_service_process() returns Fail. [Stop!]. ret=0x%x\n",
					__func__, ret);
		}
		if (shm_param.mapAry != NULL)
			kfree(shm_param.mapAry);

		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("%s: KREE API failed(0x%x)\n", __func__, ret);
			return ret;
		}

		shm_data.shm_handle = shm_handle;

		/* copy result back to user */
		shm_data.session = ret;
		err = copy_to_user(user_req, &shm_data, sizeof(shm_data));
		if (err < 0)
			KREE_ERR("%s: copy_to_user failed(%d)\n", __func__,
				 err);

		break;

	case MTEE_CMD_SHM_UNREG:
		/* do nothing */
		break;

	case MTEE_CMD_OPEN_SESSION:
		KREE_DEBUG("====> MTEE_CMD_OPEN_SESSION ====\n");
		ret = tz_client_open_session(filep, arg);

		if (ret != TZ_RESULT_SUCCESS)
			KREE_ERR(
				"MTEE_CMD_OPEN_SESSION: tz_client_open_session() returns fail!\n");
		return ret;

	case MTEE_CMD_CLOSE_SESSION:
		KREE_DEBUG("====> MTEE_CMD_CLOSE_SESSION ====\n");
		ret = tz_client_close_session(filep, arg);
		if (ret != TZ_RESULT_SUCCESS)
			KREE_ERR(
				"MTEE_CMD_CLOSE_SESSION: tz_client_close_session() returns fail!\n");

		return ret;

	case MTEE_CMD_TEE_SERVICE:
		KREE_DEBUG("====> MTEE_CMD_TEE_SERVICE ====\n");
		return tz_client_tee_service(filep, arg, compat);

	case MTEE_CMD_SC_TEST_CP_CHM2SHM: /*Secure Camera Test*/
		KREE_DEBUG("====> MTEE_CMD_SC_TEST_CP_CHM2SHM ====\n");
		return _sc_test_cp_chm2shm(filep, arg);

	case MTEE_CMD_SC_TEST_UPT_CHMDATA: /*Secure Camera Test*/
		KREE_DEBUG("====> MTEE_CMD_SC_TEST_UPT_CHMDATA ====\n");
		return _sc_test_upt_chmdata(filep, arg);

	case MTEE_CMD_APPEND_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_APPEND_CHMEM ====\n");
		return _sc_test_appendChm(filep, arg);

	case MTEE_CMD_RELEASE_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_RELEASE_CHMEM ====\n");
		return _sc_test_releaseChm(filep, arg);

	case MTEE_CMD_ALLOC_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_ALLOC_CHMEM ====\n");
		return _sc_test_allocChm(filep, arg, 0);

	case MTEE_CMD_ZALLOC_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_ZALLOC_CHMEM ====\n");
		return _sc_test_allocChm(filep, arg, 1);

	case MTEE_CMD_REF_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_REF_CHMEM ====\n");
		return _sc_test_refChm(filep, arg);

	case MTEE_CMD_UNREF_CHMEM:
		KREE_DEBUG("====> MTEE_CMD_UNREF_CHMEM ====\n");
		return _sc_test_unrefChm(filep, arg);

	case MTEE_CMD_SC_CHMEM_HANDLE:
		err = copy_from_user(&cparam, user_req, sizeof(cparam));
		if (err < 0) {
			KREE_ERR("%s: copy_from_user failed(%d)\n", __func__,
				 err);
			return err;
		}
		ret = _IONHandle2MemHandle(cparam.ION_handle,
					   &(cparam.other_handle));
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("%s: _IONHandle2MemHandle fail, ret = %d\n",
				 __func__, ret);
			return ret;
		}
		err = copy_to_user(user_req, &cparam, sizeof(cparam));
		if (err < 0)
			KREE_ERR("%s: copy_to_user failed(%d)\n", __func__,
				 err);
		break;

	default:
		KREE_ERR("%s: undefined ioctl cmd 0x%x\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long gz_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	return _gz_ioctl(filep, cmd, arg, 0);
}

#if defined(CONFIG_COMPAT)
static long gz_compat_ioctl(struct file *filep, unsigned int cmd,
			    unsigned long arg)
{
	return _gz_ioctl(filep, cmd, arg, 1);
}
#endif

/************ kernel module init entry ***************/
static int __init gz_init(void)
{
	int res = 0;

	res = create_files();
	if (res) {
		KREE_DEBUG("create sysfs failed: %d\n", res);
	} else {
		struct task_struct *gz_get_cpuinfo_task;

		gz_get_cpuinfo_task = kthread_create(
			gz_get_cpuinfo_thread, NULL, "gz_get_cpuinfo_task");
		if (IS_ERR(gz_get_cpuinfo_task)) {
			KREE_ERR("Unable to start kernel thread %s\n",
				 __func__);
			res = PTR_ERR(gz_get_cpuinfo_task);
		} else
			wake_up_process(gz_get_cpuinfo_task);
	}

	return res;
}

/************ kernel module exit entry ***************/
static void __exit gz_exit(void)
{
	KREE_DEBUG("gz driver exit\n");
}


module_init(gz_init);
module_exit(gz_exit);
