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

#include "gz_shmem_ut.h"
#include <linux/string.h>
#include <linux/slab.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include "unittest.h"

#define KREE_DEBUG(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_INFO(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_ERR(fmt...) pr_info("[SM_kUT][ERR]" fmt)

INIT_UNITTESTS;

#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define echo_srv_name "com.mediatek.geniezone.srv.echo"

int verify_data(char *buf, int size, char ch)
{
	int i;
	int cnt = 0;

	for (i = 0; i < size; i++) {
		if (buf[i] != ch)
			return TZ_RESULT_ERROR_GENERIC;
		cnt++;
	}

	//KREE_DEBUG("[%s]char:%c @buf(0x%llx,sz=0x%x),cnt=0x%x\n",
	//__func__, ch, (uint64_t) buf, size, cnt);

	return TZ_RESULT_SUCCESS;
}

/*update shmem data in GZ*/
int upt_data_in_GZ(int32_t mem_hd, int shm_size, int numOfPA)
{
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;
	KREE_SESSION_HANDLE echo_sn; /*for echo service */
	int ret = TZ_RESULT_SUCCESS;

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
	ret = KREE_TeeServiceCall(echo_sn,
		TZCMD_SHARED_MEM_TEST, paramTypes, param); /*5588*/
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s]echo[0x5588] fail(0x%x)\n", __func__, ret);

	ret = KREE_CloseSession(echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn close fail\n");
		return ret;
	}

	//KREE_DEBUG("[%s] upt shmem hd=0x%x, sz=0x%x, num_PA=0x%x\n",
	//	   __func__, mem_hd, shm_size, numOfPA);
	return ret;
}

TZ_RESULT shmem_test_continuous(void)
{
	int sz = 4272; /*can update for test*/

	int aligned_sz = (int) PAGE_ALIGN(sz);
	int sz_order = 0;
	char *buf = NULL;
	int num_PA = 0;
	uint64_t pa = 0;
	uint32_t rem = 0;
	KREE_SESSION_HANDLE mem_sn = 0;
	KREE_SHAREDMEM_HANDLE mem_hd;
	KREE_SHAREDMEM_PARAM shm_param;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;

	TEST_BEGIN("==> shmem test continuous case");
	RESET_UNITTESTS;

	num_PA = sz / PAGE_SIZE;
	if ((sz % PAGE_SIZE) != 0)
		num_PA++;

	//buf = kmalloc(sz, GFP_KERNEL);
	sz_order = get_order(aligned_sz);
	buf = (char *)__get_free_pages(GFP_KERNEL, sz_order);
	if (!buf) {
		KREE_ERR("[%s] buf kmalloc fail.\n", __func__);
		ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	pa = (uint64_t) virt_to_phys((void *)buf);

	(void)div_u64_rem(pa, PAGE_SIZE, &rem);
	if (rem != 0)	{
		KREE_ERR("[%s] buf PA(0x%llx) !aligned, stop UT\n",
			__func__, pa);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto out_free_buf;
	}

	KREE_DEBUG("[%s]sz=%d, aligned_sz=%d, PA=%llx, num_PA=%d\n",
	__func__, sz, aligned_sz, pa, num_PA);

	memset(buf, 'a', sz);	/*init data */
	ret = verify_data(buf, sz, 'a');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf (bf)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf (bf) fail(%d)\n", __func__, ret);
		goto out_free_buf;
	}

	shm_param.buffer = (void *)pa;
	shm_param.size = sz;
	shm_param.region_id = 0;
	shm_param.mapAry = NULL; /*continuous pages */

	/*create session*/
	ret = KREE_CreateSession(mem_srv_name, &mem_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "create mem_sn");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("CreateSession fail(%d)\n", mem_sn);
		goto out_free_buf;
	}

	/*reg shared mem */
	ret = KREE_RegisterSharedmem(mem_sn, &mem_hd, &shm_param);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "reg shmem");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]Reg Shmem fail(0x%x)\n", __func__, ret);
		goto out_create_mem_sn;
	}

	KREE_DEBUG("[%s]sz=%d, &buf=%llx, PA=%llx, num_PA=%d, hd=0x%x\n",
	__func__, sz, (uint64_t) buf, pa, num_PA, mem_hd);

	/*update shmem data in GZ side */
	ret = upt_data_in_GZ(mem_hd, sz, num_PA);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "upt shmem in GZ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("upt shmem fail (ret=%d)\n", ret);
		goto out_unreg_shmem;
	}

	/*verify updated data:b (changed in GZ) */
	ret = verify_data(buf, sz, 'b');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf (af)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf (af) fail(%d)\n", __func__, ret);
		goto out_unreg_shmem;
	}

out_unreg_shmem:
	/*unreg shared mem */
	ret = KREE_UnregisterSharedmem(mem_sn, mem_hd);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "unreg shmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("Unreg Shmem fail: hd=0x%x(%d)\n", mem_hd, ret);

out_create_mem_sn:
	/*close session */
	ret = KREE_CloseSession(mem_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "close mem_sn");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("mem_sn close fail\n");

out_free_buf:
	/*free test shmem region */
	if (buf)
		free_pages((unsigned long)buf, sz_order);
		//kfree(buf); /*w kmalloc()*/

out:
	KREE_DEBUG("[%s] test end\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;
	return ret;
}

TZ_RESULT shmem_test_discontinuous(void)
{
	int sz1 = 8192, sz2 = 12288; /*can update for test*/

	int aligned_sz1 = (int) PAGE_ALIGN(sz1);
	int aligned_sz2 = (int) PAGE_ALIGN(sz2);
	int sz1_order = 0, sz2_order = 0;
	char *buf1 = NULL, *buf2 = NULL;
	int num_PA1 = 0, num_PA2 = 0, num_PA;
	int num_PA1_div = 0, num_PA2_div = 0;
	int num_PA1_mod = 0, num_PA2_mod = 0;

	uint64_t pa1 = 0, pa2 = 0;
	uint32_t rem = 0;
	uint64_t *paAry = NULL;
	KREE_SESSION_HANDLE mem_sn = 0;
	KREE_SHAREDMEM_HANDLE mem_hd;
	KREE_SHAREDMEM_PARAM shm_param;
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	int m = 0, idx = 1;

	TEST_BEGIN("==> shmem test discontinuous case");
	RESET_UNITTESTS;

	num_PA1_div = sz1 / ((int) PAGE_SIZE);
	num_PA1_mod = sz1 % ((int) PAGE_SIZE);
	num_PA1 = (num_PA1_mod != 0) ? (num_PA1_div + 1) : num_PA1_div;

	num_PA2_div = sz2 / ((int) PAGE_SIZE);
	num_PA2_mod = sz2 % ((int) PAGE_SIZE);
	num_PA2 = (num_PA2_mod != 0) ? (num_PA2_div + 1) : num_PA2_div;

	num_PA = (num_PA1 + num_PA2);

	//buf1 = kmalloc(sz1, GFP_KERNEL);
	sz1_order = get_order(aligned_sz1);
	buf1 = (char *)__get_free_pages(GFP_KERNEL, sz1_order);
	if (!buf1) {
		KREE_ERR("[%s] buf1 kmalloc fail.\n", __func__);
		ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	memset(buf1, 'a', sz1);	/*init data */
	ret = verify_data(buf1, sz1, 'a');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf1 (bf)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf1 (bf) fail(%d)\n", __func__, ret);
		goto out_free_buf1;
	}

	//buf2 = kmalloc(sz2, GFP_KERNEL);
	sz2_order = get_order(aligned_sz2);
	buf2 = (char *)__get_free_pages(GFP_KERNEL, sz2_order);
	if (!buf2) {
		KREE_ERR("[%s] buf2 kmalloc fail.\n", __func__);
		ret = TZ_RESULT_ERROR_OUT_OF_MEMORY;
		goto out_free_buf1;
	}

	memset(buf2, 'a', sz2);	/*init data */
	ret = verify_data(buf2, sz2, 'a');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf2 (bf)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf2 (bf) fail(%d)\n", __func__, ret);
		goto out_free_buf2;
	}

	pa1 = (uint64_t) virt_to_phys((void *)buf1);
	pa2 = (uint64_t) virt_to_phys((void *)buf2);

	(void)div_u64_rem(pa1, PAGE_SIZE, &rem);
	if (rem != 0)	{
		KREE_ERR("[%s] buf1 PA(0x%llx) !aligned, stop UT\n",
			__func__, pa1);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto out_free_buf2;
	}

	(void)div_u64_rem(pa2, PAGE_SIZE, &rem);
	if (rem != 0)	{
		KREE_ERR("[%s] buf2 PA(0x%llx) !aligned, stop UT\n",
			__func__, pa2);
		ret = TZ_RESULT_ERROR_BAD_PARAMETERS;
		goto out_free_buf2;
	}

	paAry = kmalloc((num_PA + 1) * sizeof(uint64_t), GFP_KERNEL);
	if (!paAry) {
		KREE_DEBUG("[%s]paAry kmalloc fail.\n", __func__);
		goto out_free_buf2;
	}
	paAry[0] = num_PA;

	for (m = 0; m < num_PA1; m++)
		paAry[idx++] = pa1 + (uint64_t) (m) * (uint64_t) PAGE_SIZE;

	for (m = 0; m < num_PA2; m++)
		paAry[idx++] = pa2 + (uint64_t) (m) * (uint64_t) PAGE_SIZE;

	/*for debug */
	//for (m = 0; m < idx; m++)
	//	KREE_DEBUG("paAry[%d]=0x%llx\n", m, paAry[m]);

	shm_param.buffer = NULL;
	shm_param.size = (num_PA1 + num_PA2) * (uint32_t) PAGE_SIZE;
	shm_param.region_id = 0;
	shm_param.mapAry = (void *)paAry; /*discountinuous pages*/

	/*create session*/
	ret = KREE_CreateSession(mem_srv_name, &mem_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "create mem_sn");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("CreateSession fail(%d)\n", mem_sn);
		goto out_free_paAry;
	}

	/*reg shared mem */
	ret = KREE_RegisterSharedmem(mem_sn, &mem_hd, &shm_param);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "reg shmem");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]Reg Shmem fail(0x%x)\n", __func__, ret);
		goto out_create_mem_sn;
	}

	KREE_DEBUG("[%s]sz1=%d, &buf1=%llx, PA1=%llx, num_PA1=%d, hd=0x%x\n",
	__func__, sz1, (uint64_t) buf1, pa1, num_PA1, mem_hd);

	KREE_DEBUG("[%s]sz2=%d, &buf2=%llx, PA2=%llx, num_PA2=%d, hd=0x%x\n",
	__func__, sz2, (uint64_t) buf2, pa2, num_PA2, mem_hd);

	/*update shmem data in GZ side */
	ret = upt_data_in_GZ(mem_hd, (sz1 + sz2), num_PA);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "upt shmem in GZ");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("upt shmem fail (ret=%d)\n", ret);
		goto out_unreg_shmem;
	}

	/*verify updated data:b (changed in GZ) */
	ret = verify_data(buf1, sz1, 'b');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf1 (af)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf1 (af) fail(%d)\n", __func__, ret);
		goto out_unreg_shmem;
	}

	ret = verify_data(buf2, sz2, 'b');
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "verify buf2 (af)");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]buf2 (af) fail(%d)\n", __func__, ret);
		goto out_unreg_shmem;
	}

out_unreg_shmem:
	/*unreg shared mem */
	ret = KREE_UnregisterSharedmem(mem_sn, mem_hd);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "unreg shmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("Unreg Shmem fail: hd=0x%x(%d)\n", mem_hd, ret);

out_create_mem_sn:
	/*close session */
	ret = KREE_CloseSession(mem_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "close mem_sn");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("mem_sn close fail\n");

out_free_paAry:
	if (paAry)
		kfree(paAry);

out_free_buf2:
	/*free test shmem region */
	if (buf2)
		free_pages((unsigned long)buf2, sz2_order);
		//kfree(buf2);

out_free_buf1:
	/*free test shmem region */
	if (buf1)
		free_pages((unsigned long)buf1, sz1_order);
		//kfree(buf1);

out:
	KREE_DEBUG("[%s] test end\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;
	return ret;
}

DEFINE_MUTEX(shmem_ut_mutex);
int gz_test_shm(void *arg)
{
	mutex_lock(&shmem_ut_mutex);

	/*case1: test continuous region*/
	shmem_test_continuous();

	/*case2: test discontinuous region*/
	shmem_test_discontinuous();

	mutex_unlock(&shmem_ut_mutex);

	return TZ_RESULT_SUCCESS;
}

