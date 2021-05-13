// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
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

#include "gz_shmem_ut.h"


/*define*/

#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define echo_srv_name  "com.mediatek.geniezone.srv.echo"

#define KREE_DEBUG(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_INFO(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_ERR(fmt...) pr_info("[SM_kUT][ERR]" fmt)

#define test_case_continuous 0
#define test_case_discontinuous 1

#define debugFg 0
#define enableFg 1

/*declaration fun*/

INIT_UNITTESTS;

/*global variable*/

/*UT test data: case 1 & case 2*/
/*case 1: prepare test shmem region: continuous region*/
char *con_buf;
int con_size = 4272;		/*can update */

/*case 2:prepare test shmem region: discontinuous region*/
char **discon_buf;
uint64_t *paAry;
int _num_Group = 3;		/*can update */
int _pg_one_Group = 4;		/*can update */


/*UT fun*/
int _shmem_reg(KREE_SESSION_HANDLE mem_sn,
	       KREE_SECUREMEM_HANDLE *mem_hd, KREE_SHAREDMEM_PARAM *shm_param)
{
	int ret;

	ret = KREE_RegisterSharedmem(mem_sn, mem_hd, shm_param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]KREE_RegisterSharedmem fail(0x%x)\n", __func__,
			 ret);
		return ret;
	}
	KREE_DEBUG("[%s] Done. shmem hd=0x%x\n", __func__, *mem_hd);

	return TZ_RESULT_SUCCESS;
}

/*update shmem data in GZ*/
int _shmem_upt_data(KREE_SECUREMEM_HANDLE mem_hd, int shm_size, int numOfPA)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	int ret;

	KREE_SESSION_HANDLE echo_sn;	/*for echo service */

	KREE_DEBUG("[%s] upt shmem hd=0x%x, size=0x%x, numOfPA=0x%x\n",
		   __func__, mem_hd, shm_size, numOfPA);

	ret = KREE_CreateSession(echo_srv_name, &echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn create fail\n");
		return ret;
	}

	param[0].value.a = mem_hd;
	param[1].value.a = numOfPA;
	param[1].value.b = shm_size;
	paramTypes = TZ_ParamTypes3
	    (TZPT_VALUE_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);
	ret = KREE_TeeServiceCall(echo_sn, 0x5588, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]echo[0x5588] fail(0x%x)\n", __func__, ret);
		return ret;
	}

	ret = KREE_CloseSession(echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn close fail\n");
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

int _shmem_verify_data_detail(char *buf, int size, char ch)
{
	int i;
	int cnt = 0;

	KREE_DEBUG("[%s]verify char: %c @buf(0x%llx, size=0x%x)\n",
		   __func__, ch, (uint64_t) buf, size);
	for (i = 0; i < size; i++) {
		if (buf[i] != ch)
			return TZ_RESULT_ERROR_GENERIC;
		cnt++;
	}
	KREE_DEBUG
	    ("[%s][done]verify char: %c @buf(0x%llx, size=0x%x), cnt=0x%x\n",
	     __func__, ch, (uint64_t) buf, size, cnt);
	return TZ_RESULT_SUCCESS;
}

int _shmem_verify_data(int test_case, char ch)
{
#if enableFg
	int m, ret;
#endif

	switch (test_case) {
	case test_case_continuous:
		return _shmem_verify_data_detail(con_buf, con_size, ch);

#if enableFg
	case test_case_discontinuous:
		for (m = 0; m < _num_Group; m++) {
			ret =
			    _shmem_verify_data_detail(discon_buf[m],
			      (_pg_one_Group * PAGE_SIZE), ch);
			if (ret != TZ_RESULT_SUCCESS)
				return ret;
		}
		break;
#endif

	default:
		KREE_DEBUG("[%s]Wrong test case:%d\n", __func__, test_case);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	return TZ_RESULT_SUCCESS;
}

int _shmem_unreg(KREE_SESSION_HANDLE mem_sn, KREE_SECUREMEM_HANDLE mem_hd)
{
	int ret;

	KREE_DEBUG("Unreg. Shmem: mem_sn=0x%x, mem_hd=0x%x\n", mem_sn, mem_hd);
	ret = KREE_UnregisterSharedmem(mem_sn, mem_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Unreg. Shmem fail: mem_hd=0x%x(ret=0x%x)\n", mem_hd,
			 ret);
		return ret;
	}
	return TZ_RESULT_SUCCESS;
}


TZ_RESULT _get_region_continuous(KREE_SHAREDMEM_PARAM *shm_param,
				 int *_shm_size, int *_num_PA)
{
	int numOfPA = 0;
	uint64_t pa = 0;

	numOfPA = con_size / PAGE_SIZE;
	if ((con_size % PAGE_SIZE) != 0)
		numOfPA++;

	*_shm_size = con_size;
	*_num_PA = numOfPA;

	con_buf = kmalloc(con_size, GFP_KERNEL);
	if (!con_buf) {
		KREE_ERR("[%s] con_buf kmalloc Fail.\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	memset(con_buf, 'a', con_size);	/*init data */

	pa = (uint64_t) virt_to_phys((void *)con_buf);

	KREE_DEBUG
	    ("[%s]shmem test buf: size=%d, &buf=%llx, PA=%llx, numOfPA=%d\n",
	     __func__, *_shm_size, (uint64_t) con_buf, pa, *_num_PA);

	shm_param->buffer = (void *)pa;
	shm_param->size = *_shm_size;
	shm_param->region_id = 0;
	shm_param->mapAry = NULL;	/*continuous pages */

	return TZ_RESULT_SUCCESS;

}

TZ_RESULT _get_region_discontinuous(KREE_SHAREDMEM_PARAM *shm_param,
				    int *_shm_size, int *_num_PA)
{
	int m = 0, n = 0;

	int _total_PAs = _num_Group * _pg_one_Group;
	uint64_t startPA = 0x0;
	int _idx = 1;

	*_shm_size = _total_PAs * PAGE_SIZE;
	*_num_PA = _total_PAs;

	discon_buf = kmalloc((_num_Group * sizeof(char *)), GFP_KERNEL);
	if (!discon_buf) {
		KREE_ERR("[%s] discon_buf kmalloc Fail.\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	for (m = 0; m < _num_Group; m++) {
		discon_buf[m] =
		    kmalloc((_pg_one_Group * PAGE_SIZE), GFP_KERNEL);
		if (!discon_buf[m]) {
			KREE_ERR("[%s] discon_buf[%d] kmalloc Fail.\n",
				 __func__, m);
			for (n = 0; n < m; n++)
				kfree(discon_buf[n]);
			kfree(discon_buf);
			return TZ_RESULT_ERROR_OUT_OF_MEMORY;
		}
	}


	paAry = kmalloc((_total_PAs + 1) * sizeof(uint64_t), GFP_KERNEL);
	if (!paAry) {
		KREE_DEBUG("[%s]paAry kmalloc fail.\n", __func__);
		goto malloc_fail_out;
	}

	paAry[0] = _total_PAs;
	for (m = 0; m < _num_Group; m++) {
		startPA = (uint64_t) virt_to_phys((void *)discon_buf[m]);
		for (n = 0; n < _pg_one_Group; n++) {	/* 2 PAs in a group */
			paAry[_idx++] = (uint64_t) ((uint64_t) startPA +
						    (uint64_t) (n) * PAGE_SIZE);
		}

		/*init data */
		memset(discon_buf[m], 'a', (_pg_one_Group * PAGE_SIZE));
	}

#if debugFg
	/*for debug */
	for (m = 0; m < _idx; m++)
		KREE_DEBUG("[%s]paAry[%d]=0x%llx\n", __func__, m,
			   (uint64_t) paAry[m]);
#endif
	shm_param->buffer = NULL;
	shm_param->size = _total_PAs * PAGE_SIZE;
	shm_param->mapAry = (void *)paAry;	/*discountinuous pages */

	return TZ_RESULT_SUCCESS;

malloc_fail_out:
	for (n = 0; n < _num_Group; n++)
		kfree(discon_buf[n]);
	kfree(discon_buf);

	return TZ_RESULT_ERROR_OUT_OF_MEMORY;
}

TZ_RESULT _get_region(int test_case, KREE_SHAREDMEM_PARAM *shm_param,
		      int *_shm_size, int *_num_PA)
{
	/*prepare test shmem region */
	switch (test_case) {
	case test_case_continuous:
		return _get_region_continuous(shm_param, _shm_size, _num_PA);

#if enableFg
	case test_case_discontinuous:
		return _get_region_discontinuous(shm_param, _shm_size, _num_PA);
#endif

	default:
		KREE_DEBUG("[%s]Wrong test case:%d\n", __func__, test_case);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	return TZ_RESULT_SUCCESS;
}


TZ_RESULT _free_region_continuous(void)
{
	if (!con_buf)
		kfree(con_buf);
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _free_region_discontinuous(void)
{
	int n;

	for (n = 0; n < _num_Group; n++)
		kfree(discon_buf[n]);
	kfree(discon_buf);

	kfree(paAry);
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _free_region(int test_case)
{
	/*prepare test shmem region */
	switch (test_case) {
	case test_case_continuous:
		return _free_region_continuous();

#if enableFg
	case test_case_discontinuous:
		return _free_region_discontinuous();
#endif

	default:
		KREE_DEBUG("[%s]Wrong test case:%d\n", __func__, test_case);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _shmem_test_main(int test_case)
{
	TZ_RESULT ret;

	KREE_SESSION_HANDLE mem_sn;	/*for mem service */
	KREE_SESSION_HANDLE mem_hd;
	KREE_SHAREDMEM_PARAM shm_param;

	int _shm_size, _num_PA;

	TEST_BEGIN("==> test shmem");
	KREE_DEBUG("====> %s runs <====\n", __func__);
	RESET_UNITTESTS;

	/*prepare test shmem region */
	ret = _get_region(test_case, &shm_param, &_shm_size, &_num_PA);
	CHECK_EQ(ret, 0, "get shmem UT region");
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

	/*verify init data:a */
	ret = _shmem_verify_data(test_case, 'a');
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "init string error");
	if (ret != TZ_RESULT_SUCCESS)
		goto out_free_buf;

	/*session: for shmem */
	ret = KREE_CreateSession(mem_srv_name, &mem_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create mem_sn");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("mem_sn create fail\n");
		goto out_free_buf;
	}

	/*reg shared mem */
	ret = _shmem_reg(mem_sn, &mem_hd, &shm_param);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "reg shmem");
	if (ret != TZ_RESULT_SUCCESS)
		goto out_create_mem_sn;

	/*update shmem data in GZ side */
	ret = _shmem_upt_data(mem_hd, _shm_size, _num_PA);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "upt shmem in GZ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("upt shmem data fail (ret=0x%x)\n", ret);
		goto out_unreg_shmem;
	}

	/*verify updated data:b (changed in GZ) */
	ret = _shmem_verify_data(test_case, 'b');
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "updated string error");
	if (ret != TZ_RESULT_SUCCESS)
		goto out_free_buf;

out_unreg_shmem:
	/*unreg shared mem */
	ret = _shmem_unreg(mem_sn, mem_hd);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "unreg shmem");

out_create_mem_sn:
	/*close session */
	ret = KREE_CloseSession(mem_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close mem_sn");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("mem_sn close fail\n");

out_free_buf:
	/*free test shmem region */
	_free_region(test_case);

out:
	KREE_DEBUG("====> %s ends <====\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;
	return TZ_RESULT_SUCCESS;

}
DEFINE_MUTEX(mutex_shmem_ut);
int gz_test_shm(void *arg)
{
#if enableFg
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&mutex_shmem_ut);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("mutex_c fail(0x%x)\n", locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	/*case 1: continuous region */
	_shmem_test_main(test_case_continuous);


	/*case 2: discontinuous region */
	_shmem_test_main(test_case_discontinuous);

	mutex_unlock(&mutex_shmem_ut);

#endif

	return TZ_RESULT_SUCCESS;
}
