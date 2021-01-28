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
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/sizes.h>

#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include <gz-trusty/trusty_ipc.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include "unittest.h"
#include "gz_chmem_ut.h"
/***************************************************/
/*you can update the following #define option*/
#define enbFg 1
#if enbFg
/*this case can test only one chmem: MTEE/TEE Protect-shared */
#define ssmr_ready_for_mcm 0
#else
  /* use this setting that can test multi-chmem. but, you need to
   * (1) update File: drivers\misc\mediatek\trusted_mem\Kconfig
   * (2) add '#' to line: default MTK_SDSP_SHARED_PERM_VPU_TEE if (MACH_MT6779)
   * --> # default MTK_SDSP_SHARED_PERM_VPU_TEE if (MACH_MT6779)
   */
#if IS_ENABLED(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)
#define ssmr_ready_for_mcm 1
#else
#define ssmr_ready_for_mcm 0
#endif
#endif

#define KREE_DEBUG(fmt...) pr_debug("[CM_kUT]" fmt)
#define KREE_INFO(fmt...) pr_info("[CM_kUT]" fmt)
#define KREE_ERR(fmt...) pr_info("[CM_kUT][ERR]" fmt)


#if ssmr_ready_for_mcm
#include "memory_ssmr.h"
#include "mtee_regions.h"
#endif

/***************************************************/
/*don't update the following #define option*/
#define general_chmem 0
#define ION_chmem 1
#define _t_test_type ION_chmem
#define countof(a)   (sizeof(a)/sizeof(*(a)))
#define KREE_SESSION uint32_t
#define KREE_HANDLE  uint32_t
#define mem_srv_name  "com.mediatek.geniezone.srv.mem"
#define echo_srv_name  "com.mediatek.geniezone.srv.echo"

/*only for code separation, DON'T update*/
#define test_data 1
#define ssmr_fun 1
#define common_fun 1
#define SSMR_fun 1
#define test_body_fun 1
#define test_main_entry 1

//extern int ssmr_online(unsigned int feat);
#if test_data

#define _flag_align_y 1
#define _flag_align_n 0
#define _flag_zalloc_y 1
#define _flag_zalloc_n 0

struct _test_chmem_regions {
	uint64_t pa;
	uint32_t size;
	uint32_t feat;
	uint32_t region_id;
	uint32_t _t_sz_beg;
	uint32_t _t_sz_end;
};

/*limit: PA & Size needs 2MB   alignment (p80)*/
/*limit: PA & Size needs 64KB alignment (p60)*/
/*test data will be replaced with SSMR setting*/
struct _test_chmem_regions _t_CM_ary[] = {
	{0x0, 0x0, 0x0, SZ_1M, SZ_1M},	/* MTEE/TEE Protect-shared    */
	{0x0, 0x0, 0x1, SZ_1M, SZ_1M},	/* HA ELF                     */
	{0x0, 0x0, 0x2, SZ_1M, SZ_1M},	/* HA Stack/Heap              */
	{0x0, 0x0, 0x3, SZ_1M, SZ_1M},	/* SDSP Firmware              */
	{0x0, 0x0, 0x4, SZ_1M, SZ_1M},	/* SDSP Shared (VPU/TEE)      */
	{0x0, 0x0, 0x5, SZ_1M, SZ_1M},	/* SDSP Shared (MTEE/TEE)     */
	{0x0, 0x0, 0x6, SZ_1M, SZ_1M}	/* SDSP Shared (VPU/MTEE/TEE) */
};

struct _ssmr_chmem_regions {
	uint32_t feat;
	uint32_t region_id;
};

struct _ssmr_chmem_regions _ssmr_CM_ary[] = {
#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
	/*MTEE/TEE Protect-shared */
	{SSMR_FEAT_PROT_SHAREDMEM, MTEE_MCHUNKS_PROT},
#endif
#if ssmr_ready_for_mcm
#if IS_ENABLED(CONFIG_MTK_HAPP_MEM_SUPPORT)
	/*HA ELF */
	{SSMR_FEAT_TA_ELF, MTEE_MCHUNKS_HAPP},
	/*HA Stack/Heap */
	{SSMR_FEAT_TA_STACK_HEAP, MTEE_MCHUNKS_HAPP_EXTRA},
#endif
#if IS_ENABLED(CONFIG_MTK_SDSP_MEM_SUPPORT)
	/*SDSP Firmware */
	{SSMR_FEAT_SDSP_FIRMWARE, MTEE_MCHUNKS_SDSP},
#endif
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)
	/*SDSP/TEE Shared */
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)
	{SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE},
#elif IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE)
	{SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE},
#else
	{SSMR_FEAT_SDSP_TEE_SHAREDMEM, MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE},
#endif
#endif
#else
#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
	/*MTEE/TEE Protect-shared */
	{SSMR_FEAT_PROT_SHAREDMEM, 0}
#endif
#endif
};
#endif				//end of #if test_data

INIT_UNITTESTS;

/*global var.*/
int _available_mcm_num;

#if ssmr_fun
void _show_t_CM_ary(void)
{
	int i;
	char mstr[40];

	memset(mstr, '\0', sizeof(mstr));

	KREE_DEBUG("\n==================TEST CHMEM INFO=================\n");
	for (i = 0; i < countof(_t_CM_ary); i++) {
		if (!_t_CM_ary[i].pa)
			continue;

		switch (_t_CM_ary[i].region_id) {
#if ssmr_ready_for_mcm
		case MTEE_MCHUNKS_PROT:
			strncpy(mstr, "PROT_SHAREDMEM", 14);
			break;
		case MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE:
			strncpy(mstr, "SDSP_TEE_SHAREDMEM (VPU_TEE)", 28);
			break;
		case MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE:
			strncpy(mstr, "SDSP_TEE_SHAREDMEM (MTEE_TEE)", 29);
			break;
		case MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE:
			strncpy(mstr, "SDSP_TEE_SHAREDMEM (VPU_MTEE_TEE)", 33);
			break;
		case MTEE_MCHUNKS_SDSP:
			strncpy(mstr, "SDSP_FIRMWARE", 13);
			break;
		case MTEE_MCHUNKS_HAPP:
			strncpy(mstr, "TA_ELF", 6);
			break;
		case MTEE_MCHUNKS_HAPP_EXTRA:
			strncpy(mstr, "PROT_SHAREDMEM", 14);
			break;
#endif
		default:
			strncpy(mstr, "non", 3);
			break;
		}

		KREE_DEBUG
		    ("CM[%d] region_id=%d, pa=0x%llx, size=0x%x, type=%s\n", i,
		     _t_CM_ary[i].region_id, _t_CM_ary[i].pa, _t_CM_ary[i].size,
		     mstr);
	}
	KREE_DEBUG("==================================================\n");
}

/*get region from SSMR*/
static int ssmr_get(u64 *pa, u32 *size, u32 feat)
{
#if ssmr_ready_for_mcm

	phys_addr_t ssmr_pa;
	unsigned long ssmr_size;

	if (ssmr_offline(&ssmr_pa, &ssmr_size, true, feat)) {
		KREE_ERR("ssmr offline failed (feat:%d)!\n", feat);
		return TZ_RESULT_ERROR_GENERIC;
	}

	*pa = (u64) ssmr_pa;
	*size = (u32) ssmr_size;
	if (!(*pa) || !(*size)) {
		KREE_ERR("ssmr pa(0x%llx) & size(0x%x) is invalid\n",
			 *pa, *size);
		return TZ_RESULT_ERROR_GENERIC;
	}

	KREE_DEBUG("ssmr offline passed! feat:%d, pa: 0x%llx, sz: 0x%x\n", feat,
		   *pa, *size);
#endif
	return TZ_RESULT_SUCCESS;
}

/*free region to SSMR*/
static int ssmr_put(u32 feat)
{
#if ssmr_ready_for_mcm
	if (ssmr_online(feat)) {
		KREE_ERR("ssmr online failed (feat:%d)!\n", feat);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("ssmr online passed!\n");
#endif

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT _get_MCM_from_SSMR(void)
{
	uint64_t pa = 0x0ULL;
	uint32_t size = 0x0;
	int i, test_mcm_num;

#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
	test_mcm_num = 1;
#else
	test_mcm_num = 0;
#endif

#if ssmr_ready_for_mcm
	test_mcm_num = countof(_ssmr_CM_ary);
#endif

	KREE_DEBUG("[%s][%d] test_mcm_num= %d\n", __func__, __LINE__,
		   test_mcm_num);
	if (test_mcm_num == 0) {
		KREE_DEBUG("[%s][%d] invalid test chmem:test_mcm_num= %d\n",
			__func__, __LINE__, test_mcm_num);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	_available_mcm_num = 0;	//init
	for (i = 0; i < test_mcm_num; i++) {
		ssmr_get(&pa, &size, _ssmr_CM_ary[i].feat);
		_t_CM_ary[i].pa = pa;
		_t_CM_ary[i].size = size;
		_t_CM_ary[i].feat = _ssmr_CM_ary[i].feat;
		_t_CM_ary[i].region_id = _ssmr_CM_ary[i].region_id;
		_t_CM_ary[i]._t_sz_beg = SZ_2M;	/*default: 2MB */
		_t_CM_ary[i]._t_sz_end = SZ_2M;	/*default: 2MB */
		if (pa != 0x0)
			_available_mcm_num++;
	}
	_show_t_CM_ary();	/*print and verify */

	return TZ_RESULT_SUCCESS;
}

void _free_MCM_to_SSMR(void)
{
	int i;

	KREE_DEBUG("[%s][%d] _available_mcm_num=%d\n",
		   __func__, __LINE__, _available_mcm_num);

	for (i = 0; i < _available_mcm_num; i++)
		ssmr_put(_t_CM_ary[i].feat);
}

int _set_test_size(uint32_t region_id, uint32_t _test_sz_beg,
		   uint32_t _test_sz_end)
{
	int i;

	KREE_DEBUG("[%s][%d] _available_mcm_num=%d\n", __func__, __LINE__,
		   _available_mcm_num);

	for (i = 0; i < _available_mcm_num; i++) {
		if (_t_CM_ary[i].region_id == region_id) {
			if ((_test_sz_beg > _t_CM_ary[i].size) ||
			    (_test_sz_end > _t_CM_ary[i].size)) {
				KREE_ERR("test size > chmem size. fail\n");
				KREE_ERR("(0x%x~0x%x) > (0x%x). fail\n",
					 _test_sz_beg, _test_sz_end,
					 _t_CM_ary[i].size);
				return TZ_RESULT_ERROR_BAD_PARAMETERS;
			}
			_t_CM_ary[i]._t_sz_beg = _test_sz_beg;
			_t_CM_ary[i]._t_sz_end = _test_sz_end;
			KREE_DEBUG
			    ("region_id=%d test size updated to 0x%x ~ 0x%x\n",
			     region_id, _t_CM_ary[i]._t_sz_beg,
			     _t_CM_ary[i]._t_sz_end);
			break;
		}
	}
	return TZ_RESULT_SUCCESS;
}
#endif

#if common_fun
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
	//KREE_DEBUG("[OK]Create session=0x%x\n", *sn);
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
	//KREE_DEBUG("[OK]Close session=0x%x\n", sn);
	return TZ_RESULT_SUCCESS;
}

/*append multi-chunk memory*/
int _append_chmem(uint32_t test_type, KREE_SESSION append_sn,
		  KREE_HANDLE *append_hd, uint64_t pa, uint32_t size,
		  uint32_t region_id)
{
	int ret;
	KREE_SHAREDMEM_PARAM shm_param;

	shm_param.buffer = (void *)pa;
	shm_param.size = size;
	shm_param.mapAry = NULL;
	shm_param.region_id = region_id;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_AppendSecureMultichunkmem(append_sn, append_hd,
						     &shm_param);
	else			/*default case */
		ret = KREE_AppendSecureMultichunkmem_basic(append_sn,
							   append_hd,
							   &shm_param);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Append chmem Fail(0x%x)\n", ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[OK] append_hd=0x%x\n", *append_hd);
	return TZ_RESULT_SUCCESS;
}

/*release multi-chunk memory*/
int _release_chmem(uint32_t test_type, KREE_SESSION append_sn,
		   KREE_HANDLE append_hd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ReleaseSecureMultichunkmem(append_sn, append_hd);
	else			/*default case */
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

	if (!flags)
		if (test_type == (uint32_t) ION_chmem)
			ret = KREE_ION_AllocChunkmem(alloc_sn, append_hd,
						     alloc_hd, alignment,
						     alloc_size);
		else		/*default case */
			ret = KREE_AllocSecureMultichunkmem(alloc_sn, append_hd,
							    alloc_hd, alignment,
							    alloc_size);
	else if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_ZallocChunkmem(alloc_sn, append_hd,
					      alloc_hd, alignment, alloc_size);
	else			/*default case */
		ret = KREE_ZallocSecureMultichunkmem(alloc_sn,
						     append_hd, alloc_hd,
						     alignment, alloc_size);

	if ((ret != TZ_RESULT_SUCCESS) || (*alloc_hd == 0)) {
		KREE_ERR("Alloc Chmem Fail(0x%x)\n", ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	/*
	 * KREE_DEBUG("[OK]Alloc chmem [handle=0x%x, mem_handle=0x%x
	 *              (align=0x%x, size=0x%x)]\n",
	 *              append_hd, alloc_hd[i], alignment, alloc_size);
	 */
	return TZ_RESULT_SUCCESS;
}

/*unref from a chunk memory*/
int _free_chmem(uint32_t test_type, KREE_SESSION alloc_sn, KREE_HANDLE alloc_hd)
{
	int ret, count;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_UnreferenceChunkmem(alloc_sn, alloc_hd);
	else			/*default case */
		ret =
		    KREE_UnreferenceSecureMultichunkmem(alloc_sn, alloc_hd,
							&count);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Unref Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	/*KREE_DEBUG("[OK]Unref chunk memory OK.\n"); */
	return TZ_RESULT_SUCCESS;
}

/*reference an allocated chunk memory*/
int _ref_chmem(uint32_t test_type, KREE_SESSION alloc_sn, KREE_HANDLE alloc_hd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_ReferenceChunkmem(alloc_sn, alloc_hd);
	else			/*default case */
		ret = KREE_ReferenceSecureMultichunkmem(alloc_sn, alloc_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Ref. Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	return TZ_RESULT_SUCCESS;
}

/*cmd: echo @ 0x9995*/
int _query_chmem(uint32_t test_type, KREE_SESSION echo_sn,
		 KREE_HANDLE alloc_hd, uint32_t cmd)
{
	int ret;

	if (test_type == (uint32_t) ION_chmem)
		ret = KREE_ION_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);
	else			/*default case */
		ret = KREE_QueryChunkmem_TEST(echo_sn, alloc_hd, cmd);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("Query Chmem(hd=0x%x) Fail(0x%x)\n", alloc_hd, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}
	return TZ_RESULT_SUCCESS;
}
#endif				/*end of #if common_fun */

#if test_body_fun
typedef int (*_ut_func_case) (uint32_t test_type, KREE_SESSION alloc_sn,
			      KREE_HANDLE append_hd, int alloc_size,
			      int alignment, int flags, int _alloc_num,
			      KREE_HANDLE *alloc_hd);

int _mcm_test_body_saturation(uint32_t test_type, KREE_SESSION alloc_sn,
	KREE_HANDLE append_hd, int alloc_size,
	int alignment, int flags, int _alloc_num,
	KREE_HANDLE *alloc_hd)
{
	TZ_RESULT ret;
	int i;
	KREE_HANDLE extra_alloc_hd;


	/*alloc */
	for (i = 0; i < _alloc_num; i++) {
		//KREE_DEBUG("==> alloc %d/%d\n", i, (_alloc_num-1));
		ret = _alloc_chmem(test_type, alloc_sn,
				&alloc_hd[i], append_hd, alloc_size,
				alignment, flags);
		if ((ret != TZ_RESULT_SUCCESS) || (alloc_hd[i] == 0)) {
			KREE_ERR("alloc[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}
	}

	/*Extra alloc will fail!(exceed chmem size). */
	/*but phone cannot restart */
	KREE_DEBUG("====> extra alloc. fail is right!\n");
	ret = _alloc_chmem(test_type, alloc_sn, &extra_alloc_hd,
			   append_hd, alloc_size, alignment, flags);
	if (ret == TZ_RESULT_SUCCESS) {
		KREE_ERR("extra alloc fail(hd=0x%x)\n", extra_alloc_hd);
		return TZ_RESULT_ERROR_GENERIC;
	}

	/*free */
	for (i = 0; i < _alloc_num; i++) {
		//KREE_DEBUG("==> Free %d/%d\n", i, (_alloc_num-1));
		ret = _free_chmem(test_type, alloc_sn, alloc_hd[i]);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("free fail: ret=0x%x,alloc_hd[%d]=0x%x\n",
				ret, i, alloc_hd[i]);
			return TZ_RESULT_ERROR_GENERIC;
		}

	}

	/* KREE_DEBUG("[%s] ====> ends\n", __func__); */

	return TZ_RESULT_SUCCESS;
}


int _mcm_test_body_all_funs(uint32_t test_type, KREE_SESSION alloc_sn,
			    KREE_HANDLE append_hd, int alloc_size,
			    int alignment, int flags, int _alloc_num,
			    KREE_HANDLE *alloc_hd)
{
	TZ_RESULT ret, ret1;
	int i;

	KREE_SESSION echo_sn;

	/*session: echo svr */
	ret1 = _create_session(echo_srv_name, &echo_sn);
	if (ret1 != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn create fail\n");
		return ret1;
	}

	for (i = 0; i < _alloc_num; i++) {
		//KREE_DEBUG("==> test loop %d/%d\n", i, (_alloc_num-1));

		/*alloc */
		ret = _alloc_chmem(test_type, alloc_sn,
				&alloc_hd[i], append_hd, alloc_size,
				alignment, flags);
		if ((ret != TZ_RESULT_SUCCESS) || (alloc_hd[i] == 0)) {
			KREE_ERR("alloc[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}

		/*ref */
		ret = _ref_chmem(test_type, alloc_sn, alloc_hd[i]);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("ref[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}

		/*query */
		ret = _query_chmem(test_type, echo_sn, alloc_hd[i],
				0x9995);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("query[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}

		/*unref */
		ret = _free_chmem(test_type, alloc_sn, alloc_hd[i]);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("unref[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}

		/*free */
		ret = _free_chmem(test_type, alloc_sn, alloc_hd[i]);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("free[%d] fail, hd=0x%x, ret=0x%x\n",
				i, alloc_hd[i], ret);
			return TZ_RESULT_ERROR_GENERIC;
		}
	}

	/*close session */
	ret1 = _close_session(echo_sn);
	if (ret1 != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn close fail\n");
		return ret1;
	}

	return TZ_RESULT_SUCCESS;
}

int _mcm_test_body_main(_ut_func_case _ut_func, uint32_t test_type,
			KREE_SESSION alloc_sn, KREE_HANDLE append_hd,
			int alloc_size, int _alloc_num, KREE_HANDLE *alloc_hd)
{
	int ret;
	/*four test cases with diff. params */

	/*_alloc_size, alignment, zalloc*/
	ret = _ut_func(test_type, alloc_sn, append_hd, alloc_size, 0,
		       _flag_zalloc_y, _alloc_num, alloc_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("_alloc_size, alignment, zalloc fail\n");
		return ret;
	}

	/*_alloc_size, no-alignment, zalloc*/
	ret = _ut_func(test_type, alloc_sn, append_hd, alloc_size, alloc_size,
		       _flag_zalloc_y, _alloc_num, alloc_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("_alloc_size, no-alignment, zalloc fail\n");
		return ret;
	}

	/*_alloc_size, alignment, no-zalloc*/
	ret = _ut_func(test_type, alloc_sn, append_hd, alloc_size, 0,
		       _flag_zalloc_n, _alloc_num, alloc_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("_alloc_size, alignment, no-zalloc fail\n");
		return ret;
	}

	/*_alloc_size, no-alignment, no-zalloc*/
	ret = _ut_func(test_type, alloc_sn, append_hd, alloc_size, alloc_size,
		       _flag_zalloc_n, _alloc_num, alloc_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("_alloc_size, no-alignment, no-zalloc fail\n");
		return ret;
	}

	return ret;
}

TZ_RESULT _mcm_test_main(void)
{
	TZ_RESULT ret;

	KREE_SESSION append_sn;	/*for append chm */
	KREE_SESSION alloc_sn;	/*for allocate chm */
	KREE_HANDLE *append_hd = NULL;
	KREE_HANDLE *alloc_hd = NULL;

	int i, _alloc_size, _alloc_num;

	TEST_BEGIN("==> test chmem");
	KREE_DEBUG("====> %s runs <====\n", __func__);
	RESET_UNITTESTS;

	/*multi-chmem needs multi_hd */
	append_hd = kmalloc_array(_available_mcm_num, sizeof(KREE_HANDLE),
				  GFP_KERNEL);
	if (!append_hd) {
		KREE_ERR("append_hd alloc fail\n");
		goto out_end;
	}

	/*session: append CM */
	ret = _create_session(mem_srv_name, &append_sn);
	if (ret != TZ_RESULT_SUCCESS)
		goto out_free_mem;

	/*session: allocate CM */
	ret = _create_session(mem_srv_name, &alloc_sn);
	if (ret != TZ_RESULT_SUCCESS)
		goto out_create_append_sn;

	/*append chunk memory */
	for (i = 0; i < _available_mcm_num; i++) {
		ret = _append_chmem(_t_test_type, append_sn, &append_hd[i],
				    _t_CM_ary[i].pa, _t_CM_ary[i].size,
				    _t_CM_ary[i].region_id);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "append chmem");
		if (ret != TZ_RESULT_SUCCESS)
			goto out_create_alloc_sn;
	}

	for (i = 0; i < _available_mcm_num; i++) {
		for (_alloc_size = _t_CM_ary[i]._t_sz_beg;
		     _alloc_size <= _t_CM_ary[i]._t_sz_end; _alloc_size *= 2) {

			KREE_DEBUG("===> test cm_hd[%d]=0x%x ,_alloc_size=0x%x",
				   i, append_hd[i], _alloc_size);
			KREE_DEBUG("===> _alloc_size[0x%x~0x%x]",
				   _t_CM_ary[i]._t_sz_beg,
				   _t_CM_ary[i]._t_sz_end);

			_alloc_num = _t_CM_ary[i].size / _alloc_size;
			alloc_hd =
			    kmalloc_array(_alloc_num, sizeof(KREE_HANDLE),
					  GFP_KERNEL);
			if (!alloc_hd) {
				KREE_ERR("kmalloc alloc_hd[%d] Fail\n",
					 _alloc_num);
				goto out_release_cm;
			}

			/*test all chmem APIs */
			ret = _mcm_test_body_main(_mcm_test_body_all_funs,
					    _t_test_type, alloc_sn,
					    append_hd[i], _alloc_size,
					    _alloc_num, alloc_hd);
			CHECK_EQ(TZ_RESULT_SUCCESS, ret, "test_all_funs");

			/*test alloc saturation cases */
			ret = _mcm_test_body_main(_mcm_test_body_saturation,
					    _t_test_type, alloc_sn,
					    append_hd[i], _alloc_size,
					    _alloc_num, alloc_hd);
			CHECK_EQ(TZ_RESULT_SUCCESS, ret, "test_saturation");

			if (alloc_hd != NULL)
				kfree(alloc_hd);
		}
	}

out_release_cm:
	/*release chunk memory */
	for (i = 0; i < _available_mcm_num; i++) {
		ret = _release_chmem(_t_test_type, append_sn, append_hd[i]);
		CHECK_EQ(TZ_RESULT_SUCCESS, ret, "release chmem");
	}

out_create_alloc_sn:
	/*close session */
	ret = _close_session(alloc_sn);

out_create_append_sn:
	ret = _close_session(append_sn);

out_free_mem:
	if (append_hd != NULL)
		kfree(append_hd);

out_end:
	KREE_DEBUG("====> %s ends <====\n", __func__);

	TEST_END;
	REPORT_UNITTESTS;
	return TZ_RESULT_SUCCESS;

}
#endif				//end of #if test_body_fun

#if test_main_entry
/*UT main entry*/
int chunk_memory_ut(void *args)
{

#if !ssmr_ready_for_mcm
	KREE_DEBUG("SSMR not ready. stop.\n");
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
#else
	int ret;

	ret = _get_MCM_from_SSMR();
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("call _get_MCM_from_SSMR fail(ret=0x%x).\n", ret);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

#if enbFg
	/*default test alloc_size is 2MB, you can update by the API */
	ret = _set_test_size(MTEE_MCHUNKS_PROT, SZ_32M, SZ_64M);
	if (ret != TZ_RESULT_SUCCESS)
		goto out;

/*  // another chunk memory
 *	ret =
 *	    _set_test_size(MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE, _sz_4M,
 *			   _sz_4M);
 *	if (ret != TZ_RESULT_SUCCESS)
 *		goto out;
 */
#endif

	ret = _mcm_test_main();

out:
	_free_MCM_to_SSMR();
	return TZ_RESULT_SUCCESS;
#endif
}
#endif				//end of #if test_main_entry

