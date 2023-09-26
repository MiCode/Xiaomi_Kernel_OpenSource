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

#include "gz_chmem_ut.h"

#include <linux/string.h>
#include <linux/slab.h>

#include <kree/system.h>
#include <kree/mem.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include "unittest.h"

//#define KREE_DEBUG(fmt...) pr_debug("[Chmem UT]" fmt)
#define KREE_DEBUG(fmt...) pr_info("[Chmem UT]" fmt)
#define KREE_INFO(fmt...) pr_info("[Chmem UT]" fmt)
#define KREE_ERR(fmt...) pr_info("[Chmem UT][ERR]" fmt)

#include "memory_ssmr.h"
#include "mtee_regions.h"

#define KREE_SESSION uint32_t
#define KREE_HANDLE  uint32_t
#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define echo_srv_name "com.mediatek.geniezone.srv.echo"

INIT_UNITTESTS;

/*get region from SSMR*/
static int ssmr_get(u64 *pa, u32 *size, u32 feat)
{
	phys_addr_t ssmr_pa;
	unsigned long ssmr_size;

	if (ssmr_offline(&ssmr_pa, &ssmr_size, true, feat)) {
		KREE_ERR("[%s]ssmr offline failed (feat:%d)!\n", __func__, feat);
		return TZ_RESULT_ERROR_GENERIC;
	}

	*pa = (u64) ssmr_pa;
	*size = (u32) ssmr_size;
	if (!(*pa) || !(*size)) {
		KREE_ERR("[%s]ssmr pa(0x%llx) & size(0x%x) is invalid\n",
			 __func__, *pa, *size);
		return TZ_RESULT_ERROR_GENERIC;
	}

	KREE_DEBUG("[%s]ssmr offline passed! feat:%d, pa: 0x%llx, sz: 0x%x\n",
		__func__, feat, *pa, *size);

	return TZ_RESULT_SUCCESS;
}

/*release region to SSMR*/
static int ssmr_put(u32 feat)
{
	if (ssmr_online(feat)) {
		KREE_ERR("[%s]ssmr online failed (feat:%d)!\n", __func__, feat);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[%s]ssmr online passed!\n", __func__);

	return TZ_RESULT_SUCCESS;
}

int test_all_funs(KREE_HANDLE append_hd, int chmem_sz, int align_fg)
{
	int alloc_num = 0x20; /*can update, test loop times*/

	int alloc_size = 0;
	TZ_RESULT ret = TZ_RESULT_SUCCESS, ret1 = TZ_RESULT_SUCCESS;
	KREE_SESSION alloc_sn = 0; /*for allocate chm */
	KREE_SESSION echo_sn = 0;  /*for query chm    */
	KREE_HANDLE alloc_hd = 0;
	int alignment;
	int i;

	if ((append_hd <= 0) || (chmem_sz <= 0) || (alloc_num <= 0)) {
		KREE_DEBUG("[%s]Invalid param: append_hd=0x%x, chmem_sz=0x%x, alloc_num=0x%x\n",
			__func__, append_hd, chmem_sz, alloc_num);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	alloc_size = chmem_sz / alloc_num;
	alloc_size = (alloc_size < PAGE_SIZE) ? PAGE_SIZE : alloc_size;
	alloc_num = chmem_sz / alloc_size;
	alignment = align_fg ? (alloc_size) : 0;
	KREE_DEBUG("[%s] test chmem sz=0x%x, alloc sz=0x%x, alloc num=%d, alignment=0x%x\n",
		__func__, chmem_sz, alloc_size, alloc_num, alignment);

	if ((alloc_num <= 0) || (alloc_size <= 0)) {
		KREE_DEBUG("[%s]Invalid param: alloc_num=%d, alloc_size=0x%x\n",
			__func__, alloc_num, alloc_size);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	/*session: mem svr */
	ret = KREE_CreateSession(mem_srv_name, &alloc_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]CreateSession alloc_sn fail(%d)\n", __func__, ret);
		return ret;
	}

	/*session: echo svr */
	ret = KREE_CreateSession(echo_srv_name, &echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]CreateSession echo_sn fail(%d)\n", __func__, ret);
		goto out_alloc_sn;
	}

	for (i = 0; i < alloc_num; i++) {
		//KREE_DEBUG("==> test loop %d/%d\n", i, (alloc_num-1));

		/*alloc */
		if ((i % 2) == 0) {
			ret = KREE_ION_AllocChunkmem(alloc_sn, append_hd, &alloc_hd,
				alignment, alloc_size);
		} else {
			ret = KREE_ION_ZallocChunkmem(alloc_sn, append_hd, &alloc_hd,
				alignment, alloc_size);
		}
		if ((ret != TZ_RESULT_SUCCESS) || (alloc_hd == 0)) {
			KREE_ERR("[%s]alloc[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd, ret);
			goto out_echo_sn;
		}

		/*ref */
		ret = KREE_ION_ReferenceChunkmem(alloc_sn, alloc_hd);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]ref[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd, ret);
			goto out_echo_sn;
		}

		/*query */
		ret = KREE_ION_QueryChunkmem_TEST(echo_sn, alloc_hd, 0x9995);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]query[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd, ret);
			goto out_echo_sn;
		}

		/*unref */
		ret = KREE_ION_UnreferenceChunkmem(alloc_sn, alloc_hd);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]unref[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd, ret);
			goto out_echo_sn;
		}

		/*free */
		ret = KREE_ION_UnreferenceChunkmem(alloc_sn, alloc_hd);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]free[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd, ret);
			goto out_echo_sn;
		}
	}

out_echo_sn:
	/*close session */
	ret1 = KREE_CloseSession(echo_sn);
	if (ret1 != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s]echo_sn(0x%x) close fail(%d)\n", __func__, echo_sn, ret1);

out_alloc_sn:
	/*close session */
	ret1 = KREE_CloseSession(alloc_sn);
	if (ret1 != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s]alloc_sn(0x%x) close fail(%d)\n", __func__, alloc_sn, ret1);

	return ret;
}

int test_saturation(KREE_HANDLE append_hd, int chmem_sz, int align_fg)
{
	int alloc_num = 0x20; /*can update, test loop times*/

	int alloc_size = 0;
	TZ_RESULT ret = TZ_RESULT_SUCCESS, ret1 = TZ_RESULT_SUCCESS;
	KREE_SESSION alloc_sn = 0; /*for allocate chm */
	KREE_HANDLE *alloc_hd = NULL;
	KREE_HANDLE extra_alloc_hd = 0;
	int alignment;
	int i;

	if ((append_hd <= 0) || (chmem_sz <= 0) || (alloc_num <= 0)) {
		KREE_DEBUG("[%s]Invalid param: append_hd=0x%x, chmem_sz=0x%x, alloc_num=0x%x\n",
			__func__, append_hd, chmem_sz, alloc_num);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	alloc_size = chmem_sz / alloc_num;
	alloc_size = (alloc_size < PAGE_SIZE) ? PAGE_SIZE : alloc_size;
	alloc_num = chmem_sz / alloc_size;
	alignment = align_fg ? (alloc_size) : 0;
	KREE_DEBUG("[%s] test chmem sz=0x%x, alloc sz=0x%x, alloc num=%d, alignment=0x%x\n",
		__func__, chmem_sz, alloc_size, alloc_num, alignment);

	if ((alloc_num <= 0) || (alloc_size <= 0)) {
		KREE_DEBUG("[%s]Invalid param: alloc_num=%d, alloc_size=0x%x\n",
			__func__, alloc_num, alloc_size);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	alloc_hd = kmalloc_array(alloc_num, sizeof(KREE_HANDLE), GFP_KERNEL);
	if (!alloc_hd) {
		KREE_DEBUG("[%s]alloc_hd arrary kmalloc fail.\n", __func__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	/*session: mem svr */
	ret = KREE_CreateSession(mem_srv_name, &alloc_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]CreateSession alloc_sn fail(%d)\n", __func__, ret);
		goto out_free_alloc_hd;
	}

	/*alloc */
	for (i = 0; i < alloc_num; i++) {
		//KREE_DEBUG("==> alloc test loop %d/%d\n", i, (alloc_num-1));

		if ((i % 2) == 0) {
			ret = KREE_ION_AllocChunkmem(alloc_sn, append_hd, &alloc_hd[i],
				alignment, alloc_size);
		} else {
			ret = KREE_ION_ZallocChunkmem(alloc_sn, append_hd, &alloc_hd[i],
				alignment, alloc_size);
		}
		if ((ret != TZ_RESULT_SUCCESS) || (alloc_hd[i] == 0)) {
			KREE_ERR("[%s]alloc[%d] fail, hd=0x%x(%d)\n",
				__func__, i, alloc_hd[i], ret);
			goto out_alloc_sn;
		}
	}

	/*out-bound alloc will fail. phone won't restart*/
	ret = KREE_ION_AllocChunkmem(alloc_sn, append_hd,
		&extra_alloc_hd, alignment, alloc_size);
	if (ret == TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]extra alloc fail(hd=0x%x)\n",
			__func__, extra_alloc_hd);
		ret = TZ_RESULT_ERROR_GENERIC;
		goto out_alloc_sn;
	}
	KREE_DEBUG("[%s] testing out-bound alloc failed is correct!\n", __func__);

	/*free */
	for (i = 0; i < alloc_num; i++) {
		//KREE_DEBUG("==> free test loop %d/%d\n", i, (alloc_num-1));
		ret = KREE_ION_UnreferenceChunkmem(alloc_sn, alloc_hd[i]);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]free[%d] fail, hd=0x%x(%d)\n", __func__, i, alloc_hd[i], ret);
			goto out_alloc_sn;
		}
	}

out_alloc_sn:
	/*close session */
	ret1 = KREE_CloseSession(alloc_sn);
	if (ret1 != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s]alloc_sn(0x%x) close fail(%d)\n", __func__, alloc_sn, ret1);

out_free_alloc_hd:
	if (alloc_hd != NULL)
		kfree(alloc_hd);

	return ret;
}

int gz_test_chm_main(uint32_t feat, uint32_t region_id)
{
	TZ_RESULT ret = TZ_RESULT_SUCCESS;
	uint64_t pa = 0x0ULL;
	uint32_t size = 0x0;
	KREE_SESSION append_sn = 0;
	KREE_HANDLE append_hd = 0;
	KREE_SHAREDMEM_PARAM shm_param;

	TEST_BEGIN("==> gz_test_chm");
	RESET_UNITTESTS;

	/*get pa, size of PROT chmem*/
	ret = ssmr_get(&pa, &size, feat);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "ssmr_get chmem");
	if ((ret != TZ_RESULT_SUCCESS) || (pa == 0) || (size == 0)) {
		KREE_ERR("[%s]ssmr_get fail(%d):pa=0x%llx, sz=0x%x\n", __func__, ret, pa, size);
		goto out;
	}

	KREE_DEBUG("[%s]SSMR get pa=0x%llx, sz=0x%x\n", __func__, pa, size);

	/*create session*/
	ret = KREE_CreateSession(mem_srv_name, &append_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "create append_sn");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]CreateSession fail(%d)\n", __func__, ret);
		goto out_ssmr_put;
	}

	/*append chmem*/
	shm_param.buffer = (void *)pa;
	shm_param.size = size;
	shm_param.mapAry = NULL;
	shm_param.region_id = region_id;

	ret = KREE_AppendSecureMultichunkmem(append_sn, &append_hd, &shm_param);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "append chmem");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]append chmem fail(%d)\n", __func__, ret);
		goto out_close_sn;
	}
	KREE_DEBUG("[%s] append chmem hd=0x%x ok\n", __func__, append_hd);

	/*test funs*/
	ret = test_all_funs(append_hd, size, 1); /*alignment*/
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "test_all_funs(aligned)");
	KREE_DEBUG("[%s] test_all_funs(aligned) ok\n", __func__);

	ret = test_all_funs(append_hd, size, 0); /*not alignment*/
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "test_all_funs(not_aligned)");
	KREE_DEBUG("[%s] test_all_funs(not_aligned) ok\n", __func__);

	ret = test_saturation(append_hd, size, 1); /*alignment*/
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "test_all_funs(aligned)");
	KREE_DEBUG("[%s] test_saturation(aligned) ok\n", __func__);

	ret = test_saturation(append_hd, size, 0); /*not alignment*/
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "test_all_funs(not_aligned)");
	KREE_DEBUG("[%s] test_saturation(not_aligned) ok\n", __func__);

	/*release chmem*/
	ret = KREE_ReleaseSecureMultichunkmem(append_sn, append_hd);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "append chmem");
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Release chmem(hd=0x%x) fail(%d)\n", __func__, append_hd, ret);
		goto out_close_sn;
	}
	KREE_DEBUG("[%s] release chmem hd=0x%x ok\n", __func__, append_hd);

out_close_sn:
	/*close session */
	ret = KREE_CloseSession(append_sn);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "close append_sn");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] append_sn(0x%x) close fail(%d)\n", __func__, append_sn, ret);

out_ssmr_put:
	/*release chmem*/
	ret = ssmr_put(feat);
	CHECK_EQ(ret, TZ_RESULT_SUCCESS, "ssmr_put chmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] ssmr_put fail(%d)\n", __func__, ret);

out:
	KREE_DEBUG("%s test end\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;

	return ret;
}

/*
 * tested chmem feat/region_id are defined in mtee_regions.h:
 *
 * (1) MTEE/TEE Protect-shared
 *     {SSMR_FEAT_PROT_SHAREDMEM, MTEE_MCHUNKS_PROT}
 * (2) HA ELF
 *     {SSMR_FEAT_TA_ELF, MTEE_MCHUNKS_HAPP}
 * (3) HA Stack/Heap
 *     {SSMR_FEAT_TA_STACK_HEAP, MTEE_MCHUNKS_HAPP_EXTRA}
 * (4) SDSP Firmware
 *     {SSMR_FEAT_SDSP_FIRMWARE, MTEE_MCHUNKS_SDSP}
 * (5) SDSP/TEE Shared
 *    {SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE}
 * (6) VPU/MTEE/TEE Shared
 *    {SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE}
 * (7) VPU/TEE Shared
 *    {SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE}
 */

DEFINE_MUTEX(chmem_ut_mutex);
int gz_test_chm(void *args)
{
	mutex_lock(&chmem_ut_mutex);

#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
	/*simple test target: protected chmem*/
	gz_test_chm_main(SSMR_FEAT_PROT_SHAREDMEM, MTEE_MCHUNKS_PROT);
#else
	KREE_INFO("[%s]CONFIG_MTK_PROT_MEM_SUPPORT disabled\n", __func__);
	KREE_INFO("[%s]This UT is not supported.\n", __func__);
#endif

	mutex_unlock(&chmem_ut_mutex);
	return TZ_RESULT_SUCCESS;
}

