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
#include <linux/printk.h>
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
/* #include <gz/tz_cross/ta_fbc.h> */ /* FPS GO cmd header */
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include "unittest.h"

/* #include "gz_main.h" */
/* #include "gz_ut.h" */
#include "ion_test.h"
#include "memory_ssmr.h"
#include "mtee_regions.h"
#include "gz_chmem_ut.h"
/***************************************************/
#if defined(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)
#define ssmr_ready_for_mcm 1
#else
#define ssmr_ready_for_mcm 0
#endif

#define general_chmem 0
#define ION_chmem 1

#define _test_seq 1
#define _test_parall 2
#define _test_all 3
#define _test_by_TA 4

#define countof(a)   (sizeof(a)/sizeof(*(a)))
#define KREE_SESSION uint32_t
#define KREE_HANDLE  uint32_t
/***************************************************/
#define KREE_DEBUG(fmt...) pr_debug("[%d][KREE]", __LINE__, fmt)
#define KREE_INFO(fmt...) pr_info("[%d][KREE]", __LINE__, fmt)
#define KREE_ERR(fmt...) pr_info("[%d][KREE][ERR]", __LINE__, fmt)

static const char mem_srv_name[] = "com.mediatek.geniezone.srv.mem";
static const char echo_srv_name[] = "com.mediatek.geniezone.srv.echo";


struct _ssmr_chmem_regions {
	uint32_t feat;
	uint32_t region_id;
};

struct _ssmr_chmem_regions _ssmr_CM_ary[] = {
#if ssmr_ready_for_mcm
	/*MTEE/TEE Protect-shared*/
	{SSMR_FEAT_PROT_SHAREDMEM, MTEE_MCHUNKS_PROT},
	/*HA ELF*/
	{SSMR_FEAT_TA_ELF, MTEE_MCHUNKS_HAPP},
	/*HA Stack/Heap*/
	{SSMR_FEAT_TA_STACK_HEAP, MTEE_MCHUNKS_HAPP_EXTRA},
	/*SDSP Firmware*/
	{SSMR_FEAT_SDSP_FIRMWARE, MTEE_MCHUNKS_SDSP},
	/*SDSP/TEE Shared*/
	{SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED},
#else
	/*MTEE/TEE Protect-shared*/
	{SSMR_FEAT_PROT_SHAREDMEM, 0}
#endif
};


INIT_UNITTESTS;

typedef int (*_test_chmem_alloc_func)(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags);

int _test_CM_num; /*global var.*/
int _test_CM_idx; /*global var.*/
int _testParams_begIdx; /*global var.*/
int _testParams_endIdx; /*global var.*/

void _set_test_CM_ary(int idx, uint64_t pa, uint32_t size, int region_id)
{
	_test_CM_ary[idx].pa = pa;
	_test_CM_ary[idx].size = size;
	_test_CM_ary[idx].region_id = region_id;
}

void _prt_test_CM_ary(void)
{
	int i;

	KREE_DEBUG("==================TEST CHMEM INFO=================\n");
	for (i = 0; i < countof(_test_CM_ary); i++) {
		KREE_DEBUG("CM[%d] region_id=%d, pa=0x%llx, size=0x%x",
			i, _test_CM_ary[i].region_id, _test_CM_ary[i].pa,
			_test_CM_ary[i].size);
		switch (_test_CM_ary[i].region_id) {
#if ssmr_ready_for_mcm
		case MTEE_MCHUNKS_PROT:
			KREE_INFO(", PROT_SHAREDMEM\n");
			break;
		case MTEE_MCHUNKS_SDSP_SHARED:
			KREE_INFO(", SDSP_TEE_SHAREDMEM\n");
			break;
		case MTEE_MCHUNKS_SDSP:
			KREE_INFO(", SDSP_FIRMWARE\n");
			break;
		case MTEE_MCHUNKS_HAPP:
			KREE_INFO(", TA_ELF\n");
			break;
		case MTEE_MCHUNKS_HAPP_EXTRA:
			KREE_INFO(", PROT_SHAREDMEM\n");
			break;
#endif
		default:
			KREE_INFO("\n");
			break;
		}
	}
	KREE_DEBUG("==================================================\n");
}

static int ssmr_get(u64 *pa, u32 *size, u32 feat)
{
	phys_addr_t ssmr_pa;
	unsigned long ssmr_size;

	if (ssmr_offline(&ssmr_pa, &ssmr_size, true, feat)) {
		KREE_ERR("ssmr offline falied (feat:%d)!\n", feat);
		return TZ_RESULT_ERROR_GENERIC;
	}

	*pa = (u64)ssmr_pa;
	*size = (u32)ssmr_size;
	if (((*pa) == 0) || ((*size) == 0)) {
		KREE_ERR("ssmr pa(0x%llx) & size(0x%x) is invalid\n",
			*pa, *size);
		return TZ_RESULT_ERROR_GENERIC;
	}

	KREE_DEBUG("ssmr offline passed! feat:%d, pa: 0x%llx, sz: 0x%x\n", feat,
		 *pa, *size);
	return TZ_RESULT_SUCCESS;
}

static int ssmr_put(u32 feat)
{
	if (ssmr_online(feat)) {
		KREE_ERR("ssmr online failed (feat:%d)!\n", feat);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("ssmr online passed!\n");
	return TZ_RESULT_SUCCESS;
}



void _set_test_CM_info(void)
{
	uint64_t pa;
	uint32_t size;
	int i, test_mcm_num;

#if ssmr_ready_for_mcm
	test_mcm_num = countof(_ssmr_CM_ary);
#else
	test_mcm_num = 1;
#endif

	for (i = 0; i < test_mcm_num; i++) {
		ssmr_get(&pa, &size, _ssmr_CM_ary[i].feat);
		_set_test_CM_ary(i, pa, size, _ssmr_CM_ary[i].region_id);
	}

	_prt_test_CM_ary(); /*print and verify*/
}
void _end_test_ssmr_region(void)
{
	int i, test_mcm_num;

#if ssmr_ready_for_mcm
	test_mcm_num = countof(_ssmr_CM_ary);
#else
	test_mcm_num = 1;
#endif

	for (i = 0; i < test_mcm_num; i++)
		ssmr_put(_ssmr_CM_ary[i].feat);
}

void _assign_test_CM_info(int _test_chmems, int ass_id)
{
	_test_CM_num = _test_chmems;
	_test_CM_idx = ass_id;

	KREE_DEBUG("_test_CM_num=%d, idx=%d\n", _test_CM_num, _test_CM_idx);
}

/*common function*/

/*create session*/
int _create_session(const char *srv_name, KREE_SESSION *sn)
{
	int ret;

	ret = KREE_CreateSession(srv_name, sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Create session(%s) Fail(0x%x)\n", srv_name, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[OK]Create session=0x%x\n", *sn);
	return TZ_RESULT_SUCCESS;
}

/*close session*/
int _close_session(KREE_SESSION sn)
{
	int ret;

	ret = KREE_CloseSession(sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Close session=0x%x Fail(0x%x)\n", sn, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	KREE_DEBUG("[OK]Close session=0x%x\n", sn);
	return TZ_RESULT_SUCCESS;
}

/*append multi-chunk memory*/
int _append_chmem(uint32_t test_type, KREE_SESSION append_sn,
	KREE_HANDLE *append_hd, uint64_t pa, uint32_t size, uint32_t region_id)
{
	int ret;
	KREE_SHAREDMEM_PARAM shm_param;

	shm_param.buffer = (void *) pa;
	shm_param.size = size;
	shm_param.mapAry = NULL;
	shm_param.region_id = region_id;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_AppendSecureMultichunkmem(append_sn, append_hd,
			&shm_param);
	else	/*default case*/
		ret = KREE_AppendSecureMultichunkmem_basic(append_sn,
			append_hd, &shm_param);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Append chmem Fail(0x%x)\n", ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[OK] append_hd=0x%x\n", *append_hd);
	return TZ_RESULT_SUCCESS;
}

/*release multi-chunk memory*/
int _release_chmem(uint32_t test_type, KREE_SESSION append_sn
	, KREE_HANDLE append_hd)
{
	int ret;

	/*release chunk memory*/
	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ReleaseSecureMultichunkmem(append_sn, append_hd);
	else	/*default case*/
		ret = KREE_ReleaseSecureMultichunkmem_basic(append_sn,
			append_hd);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Release chmem(hd=0x%x) Fail(0x%x)\n", append_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[OK]Release chm_hd=0x%x\n", append_hd);
	return TZ_RESULT_SUCCESS;
}

/*alloc from a chunk memory*/
int _alloc_chmem(uint32_t test_type, KREE_SESSION alloc_sn,
	KREE_HANDLE *alloc_hd, KREE_HANDLE append_hd,
	int alloc_size, int alignment, int flags)
{
	int ret;

	if (flags == 0)
		if (test_type == (uint32_t) ION_chmem)
			ret = KREE_ION_AllocChunkmem(alloc_sn, append_hd,
				alloc_hd, alignment, alloc_size);
		else	/*default case*/
			ret = KREE_AllocSecureMultichunkmem(alloc_sn, append_hd,
				alloc_hd, alignment, alloc_size);
	else
		if (test_type == (uint32_t) ION_chmem)
			ret = KREE_ION_ZallocChunkmem(alloc_sn, append_hd,
				alloc_hd, alignment, alloc_size);
		else	/*default case*/
			ret = KREE_ZallocSecureMultichunkmem(alloc_sn,
				append_hd, alloc_hd, alignment, alloc_size);

	if ((ret != TZ_RESULT_SUCCESS) || (*alloc_hd == 0)) {
		KREE_ERR("Alloc Chmem Fail(0x%x)\n", ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	/*
	 * KREE_DEBUG("[OK]Alloc chmem [handle=0x%x, mem_handle=0x%x
	 *		(align=0x%x, size=0x%x)]\n",
	 *		append_hd, alloc_hd[i], alignment, alloc_size);
	 */

	return TZ_RESULT_SUCCESS;
}

/* allocate chm */
int _alloc_chmem_and_check(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE append_hd,
		int alloc_size, int alignment, int flags,
		KREE_HANDLE *alloc_hd)
{
	int i;
	int ret;

	TEST_BEGIN("====> call alloc_chmem_and_check");
	/*RESET_UNITTESTS;*/

	/*KREE_DEBUG("====> call :%s\n", __func__);*/


	ret = _alloc_chmem(test_type, alloc_sn, alloc_hd,
			append_hd, alloc_size, alignment, flags);

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "alloc chmem");
	CHECK_NEQ(0, *alloc_hd, "alloc chmem_handle");

	if ((ret != TZ_RESULT_SUCCESS) || (*alloc_hd == 0)) {
		KREE_ERR("alloc fail: i=%d\n", i);
		ret = TZ_RESULT_ERROR_GENERIC;
	}

	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/

	return ret;
}

/*unref from a chunk memory*/
int _free_chmem(uint32_t test_type, KREE_SESSION alloc_sn,
			KREE_HANDLE alloc_hd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_UnreferenceChunkmem(alloc_sn, alloc_hd);
	else	/*default case*/
		ret = KREE_UnreferenceSecureMultichunkmem(alloc_sn, alloc_hd);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Unref Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	/*KREE_DEBUG("[OK]Unref chunk memory OK.\n");*/

	return TZ_RESULT_SUCCESS;
}

int _free_chmem_and_check(uint32_t test_type,
	KREE_SESSION alloc_sn, KREE_HANDLE alloc_hd)
{

	int i;
	int ret;

	TEST_BEGIN("====> call free_chmem_and_check");
	/*RESET_UNITTESTS;*/

	/*KREE_DEBUG("====> call :%s\n", __func__);*/

	ret = _free_chmem(test_type, alloc_sn, alloc_hd);

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "unref chmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("free fail: i=%d\n", i);


	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/

	return ret;

}

/*reference an allocated chunk memory*/
int _ref_chmem(uint32_t test_type, KREE_SESSION alloc_sn, KREE_HANDLE alloc_hd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_ReferenceChunkmem(alloc_sn, alloc_hd);
	else	/*default case*/
		ret = KREE_ReferenceSecureMultichunkmem(alloc_sn, alloc_hd);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Ref. Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	return TZ_RESULT_SUCCESS;
}

int _ref_chmem_and_check(uint32_t test_type, KREE_SESSION alloc_sn,
	KREE_HANDLE alloc_hd)
{
	int ret;

	TEST_BEGIN("====> call ref_chmem_and_check");
	/*RESET_UNITTESTS;*/

	/*KREE_DEBUG("====> call :%s\n", __func__);*/

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_ReferenceChunkmem(alloc_sn, alloc_hd);
	else	/*default case*/
		ret = KREE_ReferenceSecureMultichunkmem(alloc_sn, alloc_hd);

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "Ref. chmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("Ref. Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);

	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/

	return ret;
}


/*Unreference an allocated chunk memory*/
/*cmd: 0x9995*/
int _query_chmem(uint32_t test_type, KREE_SESSION echo_sn,
		KREE_HANDLE alloc_hd, uint32_t cmd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);
	else	/*default case*/
		ret = KREE_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Query Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	return TZ_RESULT_SUCCESS;
}

/*cmd: 0x9995*/
int _query_chmem_and_check(uint32_t test_type, KREE_SESSION echo_sn,
		KREE_HANDLE alloc_hd, uint32_t cmd)
{
	int ret;

	TEST_BEGIN("====> call query_chmem_and_check");
	/*RESET_UNITTESTS;*/

	/*KREE_DEBUG("====> call :%s\n", __func__);*/

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);
	else	/*default case*/
		ret = KREE_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);

	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "Query chmem");
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("Query Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);

	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/

	return ret;
}


static int _test_saturation_sequential_body(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags,
		int *_alloc_num, KREE_HANDLE **alloc_hd)
{
	TZ_RESULT ret;
	int i, m, n;
	int end_idx = _test_CM_idx + _test_CM_num;
	KREE_HANDLE extra_alloc_hd;


	TEST_BEGIN("====> start to test: test_saturation_sequential_body");
	/*RESET_UNITTESTS;*/

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		//KREE_DEBUG("==>test CHMEM[%d], alloc_size=0x%x\n",
		//	m, alloc_size);

		/*alloc*/
		for (i = 0; i < _alloc_num[m]; i++) {
			//KREE_DEBUG("=====> Allocate %d/%d\n",
			//	i, (_alloc_num[m]-1));
			ret = _alloc_chmem_and_check(test_type, alloc_sn,
				append_hd[m], alloc_size, alignment, flags,
				&alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("alloc fail: i=%d\n", i);
				break;
			}
		}

		/*Extra alloc will fail!(exceed chmem size).*/
		/*but phone cannot restart*/
		KREE_DEBUG("====> extra alloc. fail is right!\n");
		ret = _alloc_chmem(test_type, alloc_sn, &extra_alloc_hd,
			append_hd[m], alloc_size, alignment, flags);
		CHECK_NEQ(TZ_RESULT_SUCCESS, ret, "alloc extra chmem");
		CHECK_EQ(0, extra_alloc_hd, "alloc extra chmem");


		/*free*/
		for (i = 0; i < _alloc_num[m]; i++) {
			//KREE_DEBUG("=====> Free %d/%d\n",
			//	i, (_alloc_num[m]-1));
			ret = _free_chmem_and_check(test_type, alloc_sn,
				alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("free fail: i=%d\n", i);
				break;
			}
		}
	}

	/* KREE_DEBUG("[%s] ====> ends\n", __func__);*/


	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/
	return TZ_RESULT_SUCCESS;
}


static int _test_saturation_parallel_body(uint32_t test_type,
	KREE_SESSION alloc_sn, KREE_HANDLE *append_hd, int alloc_size,
	int alignment, int flags, int *_alloc_num, KREE_HANDLE **alloc_hd)
{
	TZ_RESULT ret;
	int i, m, n;
	int end_idx = _test_CM_idx + _test_CM_num;
	KREE_HANDLE extra_alloc_hd;


	TEST_BEGIN("====> start to test: test_saturation_parallel_body");
	/*RESET_UNITTESTS;*/

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	/*alloc*/
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		//KREE_DEBUG("==>test CHMEM[%d], alloc_size=0x%x\n",
		//	m, alloc_size);
		for (i = 0; i < _alloc_num[m]; i++) {
			//KREE_DEBUG("=====> Allocate %d/%d\n",
			//	i, (_alloc_num[m]-1));
			ret = _alloc_chmem_and_check(test_type, alloc_sn,
				append_hd[m], alloc_size, alignment, flags,
				&alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("alloc fail: i=%d\n", i);
				break;
			}
		}
	}


	/*Extra alloc will fail!(exceed chmem size). but phone cannot restart*/
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		KREE_DEBUG("====> extra alloc. fail is right!\n");
		ret = _alloc_chmem(test_type, alloc_sn, &extra_alloc_hd,
			append_hd[m], alloc_size, alignment, flags);
		CHECK_NEQ(TZ_RESULT_SUCCESS, ret, "alloc extra chmem");
		CHECK_EQ(0, extra_alloc_hd, "alloc extra chmem");
	}


	/*free*/
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		for (i = 0; i < _alloc_num[m]; i++) {
			//KREE_DEBUG("=====> Free %d/%d\n",
			//	i, (_alloc_num[m]-1));
			ret = _free_chmem_and_check(test_type, alloc_sn,
				alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("free fail: i=%d\n", i);
				break;
			}
		}
	}

	/* KREE_DEBUG("[%s] ====> ends\n", __func__);*/

	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/
	return TZ_RESULT_SUCCESS;
}

static int _test_saturation_all_body(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags,
		int *_alloc_num, KREE_HANDLE **alloc_hd)
{
	TZ_RESULT ret;
	int i, m, n;
	int end_idx = _test_CM_idx + _test_CM_num;

	KREE_SESSION echo_sn;

	TEST_BEGIN("====> start to test: test_saturation_all_body");
	/*RESET_UNITTESTS;*/

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	/*session: echo svr*/
	ret = _create_session(echo_srv_name, &echo_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo_sn");
	if (ret != TZ_RESULT_SUCCESS)
		return ret;


	/*alloc*/
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		//KREE_DEBUG("==>test CHMEM[%d], alloc_size=0x%x\n",
		//	m, alloc_size);
		for (i = 0; i < _alloc_num[m]; i++) {
			//KREE_DEBUG("=====> Allocate %d/%d\n", i,
			//	(_alloc_num[m]-1));
			/*alloc*/
			ret = _alloc_chmem_and_check(test_type, alloc_sn,
				append_hd[m], alloc_size, alignment, flags,
				&alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("alloc fail: i=%d\n", i);
				break;
			}

			/*ref*/
			ret = _ref_chmem_and_check(test_type, alloc_sn,
				alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("ref fail: i=%d\n", i);
				break;
			}
			/*query*/
			ret = _query_chmem_and_check(test_type, echo_sn,
						alloc_hd[m][i], 0x9995);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("query fail: i=%d\n", i);
				break;
			}
			/*unref*/
			ret = _free_chmem_and_check(test_type, alloc_sn,
				alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("unref fail: i=%d\n", i);
				break;
			}
			/*free*/
			ret = _free_chmem_and_check(test_type, alloc_sn,
				alloc_hd[m][i]);
			if (ret != TZ_RESULT_SUCCESS) {
				KREE_ERR("free fail: i=%d\n", i);
				break;
			}
		}
	}

	/*close session*/
	ret = _close_session(echo_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo_sn");
	if (ret != TZ_RESULT_SUCCESS)
		return ret;


	/* KREE_DEBUG("[%s] ====> ends\n", __func__);*/


	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/
	return TZ_RESULT_SUCCESS;
}

static int _test_by_MTEE_TA_body(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags,
		int *_alloc_num, KREE_HANDLE **alloc_hd)
{
	TZ_RESULT ret;
	int i, m, n;
	int end_idx = _test_CM_idx + _test_CM_num;
	union MTEEC_PARAM param[4];
	uint32_t paramTypes;

	KREE_SESSION echo_sn;

	TEST_BEGIN("====> start to test: test_by_MTEE_TA_body");
	/*RESET_UNITTESTS;*/

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	/*session: echo svr*/
	ret = _create_session(echo_srv_name, &echo_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create echo_sn");
	if (ret != TZ_RESULT_SUCCESS)
		return ret;


	/*alloc*/
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		if (alloc_size > _test_CM_ary[n].size)
			continue;

		//KREE_DEBUG("==>test CHMEM[%d], alloc_size=0x%x\n",
		//	m, alloc_size);
		for (i = 0; i < _alloc_num[m]; i++) {

			/*Test Chm API in M-TEE*/
			param[0].value.a = append_hd[m]; /* sec mem handle */
			param[0].value.b = _test_CM_ary[n].size; /*chmem size*/
			param[1].value.a = alloc_size;	/* alloc size */
			param[1].value.b = alignment; /*alignment*/

			paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT,
				TZPT_VALUE_INOUT);

			/*TZCMD_TEST_QUERYCHM_TA_Normal   0x9990*/
			KREE_TeeServiceCall(echo_sn, 0x9990, paramTypes, param);

			CHECK_EQ(TZ_RESULT_SUCCESS, param[1].value.a,
				"Test by MTEE TA");
			//KREE_DEBUG("==>[OK] Test by MTEE TA(0x%x)\n",
			//	param[1].value.a);
		}
	}


	/*close session*/
	ret = _close_session(echo_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close echo_sn");
	if (ret != TZ_RESULT_SUCCESS)
		return ret;

	/* KREE_DEBUG("[%s] ====> ends\n", __func__);*/


	TEST_END_NO_PRT;
	/*REPORT_UNITTESTS;*/
	return TZ_RESULT_SUCCESS;
}

static int _test_saturation_main(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags, uint32_t test_fun)
{
	int _alloc_num[_test_CM_num];	/*# of testing alloc chm*/

	TZ_RESULT ret;
	int m, n;
	int end_idx;
	KREE_HANDLE **alloc_hd = NULL;
	int alloc_ary = 0;


	TEST_BEGIN("====> start to test: test_saturation_main");
	/*RESET_UNITTESTS;*/

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	alloc_hd = kmalloc_array(_test_CM_num, sizeof(KREE_HANDLE *),
		GFP_KERNEL);
	if (!alloc_hd) {
		KREE_ERR("kmalloc alloc_hd Fail\n");
		goto out2;
	}

	end_idx = _test_CM_idx + _test_CM_num;
	for (n = _test_CM_idx, m = 0; n < end_idx; n++, m++) {
		_alloc_num[m] = _test_CM_ary[n].size / alloc_size;
		alloc_ary = (_alloc_num[m] == 0) ? 1 : _alloc_num[m];
		alloc_hd[m] = kmalloc_array(alloc_ary, sizeof(KREE_HANDLE),
			GFP_KERNEL);
		if (!alloc_hd[m]) {
			KREE_ERR("kmalloc alloc_hd[] Fail\n");
			goto out;
		}
	}

	/*call test fun.*/
	if (test_fun == (uint32_t) _test_seq)
		ret = _test_saturation_sequential_body(test_type, alloc_sn,
			append_hd, alloc_size, alignment, flags, _alloc_num,
			alloc_hd);
	else if (test_fun == (uint32_t) _test_parall)
		ret = _test_saturation_parallel_body(test_type, alloc_sn,
			append_hd, alloc_size, alignment, flags, _alloc_num,
			alloc_hd);
	else if (test_fun == (uint32_t) _test_all)
		ret = _test_saturation_all_body(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _alloc_num, alloc_hd);
	else if (test_fun == (uint32_t) _test_by_TA)
		ret = _test_by_MTEE_TA_body(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _alloc_num, alloc_hd);


	/* KREE_DEBUG("[%s] ====> ends\n", __func__);*/

out:

	for (m = 0; m < _test_CM_num; m++)
		if (alloc_hd[m] != NULL)
			kfree(alloc_hd[m]);
out2:
	if (alloc_hd != NULL)
		kfree(alloc_hd);

	TEST_END;
	/*REPORT_UNITTESTS;*/
	return TZ_RESULT_SUCCESS;
}



static int _test_saturation_sequential(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags)
{
	_test_saturation_main(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _test_seq);

	return TZ_RESULT_SUCCESS;
}

static int _test_saturation_parallel(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags)
{
	_test_saturation_main(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _test_parall);

	return TZ_RESULT_SUCCESS;
}

static int _test_saturation_all(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags)
{
	_test_saturation_main(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _test_all);

	return TZ_RESULT_SUCCESS;
}

static int _test_by_MTEE_TA(uint32_t test_type,
		KREE_SESSION alloc_sn, KREE_HANDLE *append_hd,
		int alloc_size, int alignment, int flags)
{
	_test_saturation_main(test_type, alloc_sn, append_hd,
			alloc_size, alignment, flags, _test_by_TA);

	return TZ_RESULT_SUCCESS;
}

static void _test_chmem_loop(_test_chmem_alloc_func test_func,
			int test_param_num, struct AllocParameters params[],
			uint32_t test_type,	KREE_SESSION alloc_sn,
			KREE_HANDLE *append_hd)
{
	int i;

	KREE_DEBUG("_testParams_begIdx=%d, _testParams_endIdx=%d\n",
			_testParams_begIdx, _testParams_endIdx);

	if (_testParams_endIdx < _testParams_begIdx) {
		KREE_ERR("_testParams_endIdx < _testParams_begIdx. return.\n");
		return;
	}
	/*test data: in ion_test.h*/
	//for (i = 0; i < test_param_num; i++) {
	for (i = _testParams_begIdx; i <= _testParams_endIdx; i++) {
		KREE_DEBUG("==>params[%d]: size=0x%x, align=0x%x, init0=0x%x\n",
			i, params[i].size, params[i].alignment,
			params[i].flags);

		test_func(test_type, alloc_sn, append_hd, params[i].size,
			params[i].alignment, params[i].flags);	/*zero init?*/
	}
}

static int _test_chmem_main(uint32_t test_type,
		_test_chmem_alloc_func test_func)
{
	TZ_RESULT ret;

	KREE_SESSION append_sn;	/*for append chm*/
	KREE_SESSION alloc_sn;	/*for allocate chm*/
	KREE_HANDLE *append_hd = NULL;

	int i, j, end_idx;

	TEST_BEGIN("====> start to test:_test_chmem");
	RESET_UNITTESTS;

	/*
	 * KREE_DEBUG("====> start to test:%s\n", __func__);
	 */

	/*multi-chmem needs multi_hd*/
	append_hd = kmalloc_array(_test_CM_num, sizeof(KREE_HANDLE),
		GFP_KERNEL);
	if (!append_hd) {
		KREE_ERR("append_hd alloc fail\n");
		goto out2;
	}

	/*session: append CM*/
	ret = _create_session(mem_srv_name, &append_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create append_sn");
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

	/*session: allocate CM*/
	ret = _create_session(mem_srv_name, &alloc_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "create alloc_sn");
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

	/*append chunk memory*/
	end_idx = _test_CM_idx + _test_CM_num;
	for (i = _test_CM_idx, j = 0; i < end_idx; i++, j++) {
		ret = _append_chmem(test_type, append_sn, &append_hd[j],
			_test_CM_ary[i].pa, _test_CM_ary[i].size,
			_test_CM_ary[i].region_id);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "append chmem");
		if (ret != TZ_RESULT_SUCCESS)
			goto out;
	}

	/*diff test params[] in ion_test.h*/
	KREE_DEBUG("====> Case 1: alloc_test_params\n");
	_test_chmem_loop(test_func, countof(alloc_test_params),
	alloc_test_params, test_type, alloc_sn, append_hd);

	KREE_DEBUG("====> Case 2: alloc_zero_test_params\n");
	_test_chmem_loop(test_func, countof(alloc_zero_test_params),
	alloc_zero_test_params, test_type, alloc_sn, append_hd);

	KREE_DEBUG("====> Case 3: alloc_aligned_test_params\n");
	_test_chmem_loop(test_func, countof(alloc_aligned_test_params),
	alloc_aligned_test_params, test_type, alloc_sn, append_hd);

	KREE_DEBUG("====> Case 4: alloc_zero_aligned_test_params\n");
	_test_chmem_loop(test_func, countof(alloc_zero_aligned_test_params),
	alloc_zero_aligned_test_params, test_type, alloc_sn, append_hd);


	/*release chunk memory*/
	for (i = _test_CM_idx, j = 0; i < end_idx; i++, j++) {

		ret = _release_chmem(test_type, append_sn, append_hd[j]);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "release chmem");
		if (ret != TZ_RESULT_SUCCESS)
			goto out;
	}

	/*close session*/
	ret = _close_session(append_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close append_sn");
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

	ret = KREE_CloseSession(alloc_sn);
	CHECK_EQ(TZ_RESULT_SUCCESS, ret, "close alloc_sn");
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

out:
	KREE_DEBUG("====> %s ends <====\n", __func__);

out2:
	if (append_hd != NULL)
		kfree(append_hd);


	TEST_END;
	REPORT_UNITTESTS;
	return TZ_RESULT_SUCCESS;
}


/*chunk memory test for basic API impl.*/
int _test_chmem_fun_list(uint32_t test_type)
{
	/*test Basic Chmem API*/
	KREE_DEBUG("===>test basic chmem API (0x%x)\n", (uint32_t) test_type);

	/*Case 1*/
	KREE_DEBUG("===>test saturation_sequential\n");
	_test_chmem_main(test_type, _test_saturation_sequential);
	KREE_DEBUG("===>test saturation_sequential [done]\n");

	/*Case 2*/
	KREE_DEBUG("===>test saturation_parallel\n");
	_test_chmem_main(test_type, _test_saturation_parallel);
	KREE_DEBUG("===>test saturation_parallel [done]\n");

	/*Case 3*/
	KREE_DEBUG("===>test chmem all APIs\n");
	_test_chmem_main(test_type, _test_saturation_all);
	KREE_DEBUG("===>test chmem all APIs [done]\n");

	return TZ_RESULT_SUCCESS;
}


#if 1
/*test main entry*/

/*ION_chmem: chunk memory is 2MB alignment*/
/*test: multiple chmems*/
int chunk_memory_test_ION_Multiple(void *args)
{
#if ssmr_ready_for_mcm
	int _test_chmems, _test_chmem_Params_Idx;

	_set_test_CM_info();


 #if 0
	_test_chmems = countof(_test_CM_ary) - 1;	/*4 chmem*/
	_test_chmem_Params_Idx = 1;			/*_test_CM_ary[1~4]*/

	/*xx_test_params[9~]:512KB ~ 64MB*/
	_testParams_begIdx = 9;
	_testParams_endIdx = countof(alloc_test_params) - 1;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);
	_test_chmem_fun_list(ION_chmem);


 #else
	/*'HA elf', 'HA extra mem', 'SDSP_TEE_SHAREDMEM' cannot test now!*/
	/*test 1: PROT_SHAREDMEM*/
	_test_chmems = 1;           /*1 chmem*/
	_test_chmem_Params_Idx = 0;	/*_test_CM_ary[0]: PROT_SHAREDMEM*/

	/*xx_test_params[]:64MB*/
	_testParams_begIdx = countof(alloc_test_params) - 1;
	_testParams_endIdx = countof(alloc_test_params) - 1;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);
	_test_chmem_fun_list(ION_chmem);

	/*test 2: SDSP_FIRMWARE*/
	_test_chmems = 1;           /*1 chmem*/
	_test_chmem_Params_Idx = 3;	/*_test_CM_ary[3]: SDSP_FIRMWARE*/

	/*xx_test_params[9~]:512KB ~ 64MB*/
	_testParams_begIdx = 9;
	_testParams_endIdx = countof(alloc_test_params) - 1;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);
	_test_chmem_fun_list(ION_chmem);

 #endif

	_end_test_ssmr_region();

#else
	KREE_DEBUG("[%s]SSMR for mcm is not ready, cannot run mcm UT.\n",
		__func__);
#endif
	return TZ_RESULT_SUCCESS;
}

/*ION_chmem: chunk memory is 2MB alignment*/
/*test: 1 chmem: MTEE/TEE Prot. mem*/
int chunk_memory_test_ION_simple(void *args)
{
	int _test_chmems, _test_chmem_Params_Idx;

	_set_test_CM_info();

	_test_chmems = 1;			/*1 chmem*/
	_test_chmem_Params_Idx = 0;	/*_test_CM_ary[0]:MTEE/TEE Prot. mem*/

	/*xx_test_params[]:64MB*/
	_testParams_begIdx = countof(alloc_test_params) - 1;
	_testParams_endIdx = countof(alloc_test_params) - 1;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);
	_test_chmem_fun_list(ION_chmem);

	_end_test_ssmr_region();
	return TZ_RESULT_SUCCESS;
}

/*simply and quickly test ION_chmem by MTEE TA*/
/*append/release in Linux kernel, alloc/ref/query/unref in MTEE*/
int chunk_memory_test_by_MTEE_TA(void *args)
{
	int _test_chmems, _test_chmem_Params_Idx;

	_set_test_CM_info();

	_test_chmems = 1;			/*1 chmem*/
	_test_chmem_Params_Idx = 2;	/*_test_CM_ary[2]:TA elf*/

	/*xx_test_params[10]:1MB*/
	_testParams_begIdx = 10;
	_testParams_endIdx = 10;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);

	KREE_DEBUG("===>test chmem all APIs:ION_chmem(2MB align)\n");
	_test_chmem_main(ION_chmem, _test_by_MTEE_TA);
	KREE_DEBUG("===>test chmem all APIs:ION_chmem(2MB align) [done]\n");
#if 0
	KREE_DEBUG("===>test chmem all APIs:general_chmem(4KB align)\n");
	_test_chmem_main(general_chmem, _test_by_MTEE_TA);
	KREE_DEBUG("===>test chmem all APIs:ION_chmem(4KB align) [done]\n");
#endif

	_end_test_ssmr_region();

	return TZ_RESULT_SUCCESS;
}

/*general_chmem: chunk memory is 4KB alignment*/
int chunk_memory_test(void *args)
{
#if 0
	int _test_chmems, _test_chmem_Params_Idx;

	_set_test_CM_info();

	_test_chmems = 1;			/*1 chmem*/
	_test_chmem_Params_Idx = 0;	/*_test_CM_ary[0]:MTEE/TEE Prot. mem*/

	/*xx_test_params[]:64MB*/
	_testParams_begIdx = countof(alloc_test_params) - 1;
	_testParams_endIdx = countof(alloc_test_params) - 1;

	_assign_test_CM_info(_test_chmems, _test_chmem_Params_Idx);
	_test_chmem_fun_list(general_chmem);

	_end_test_ssmr_region();
#else
	KREE_DEBUG("P80 not supported (4KB aligned chmem).\n");
#endif

	return TZ_RESULT_SUCCESS;
}

#endif
