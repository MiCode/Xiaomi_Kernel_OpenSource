// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "memory_ssmr.h"
#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
#include "tests/ut_common.h"
#endif
#include "tee_impl/tee_ops.h"
#include "tee_impl/tee_regions.h"
#include "../iommu/iommu_pseudo.h"

#define TEE_CMD_LOCK() mutex_lock(&tee_lock)
#define TEE_CMD_UNLOCK() mutex_unlock(&tee_lock)

static DEFINE_MUTEX(tee_lock);

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
static struct trusted_driver_operations *tee_ops;
static void *tee_session_data;

static inline int
tee_directly_invoke_cmd_locked(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	if (unlikely(INVALID(invoke_params)))
		return TMEM_PARAMETER_ERROR;

	if (unlikely(INVALID(tee_ops)))
		get_tee_peer_ops(&tee_ops);

	if (tee_ops->session_open(&tee_session_data, NULL)) {
		pr_err("%s:%d tee open session failed!\n", __func__, __LINE__);
		return TMEM_TEE_CREATE_SESSION_FAILED;
	}

	ret = tee_ops->invoke_cmd(invoke_params, tee_session_data, NULL);

	if (tee_ops->session_close(tee_session_data, NULL))
		pr_err("%s:%d tee close session failed!\n", __func__, __LINE__);

	return ret;
}
#else
static inline int
tee_directly_invoke_cmd_locked(struct trusted_driver_cmd_params *invoke_params)
{
	return 0;
}
#endif

int tee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	/* if SVP enable, then TEE will be enabled */
	if(!is_svp_enabled())
		return TMEM_OK;

#if IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
	return TMEM_OK;
#endif

	TEE_CMD_LOCK();
	ret = tee_directly_invoke_cmd_locked(invoke_params);
	TEE_CMD_UNLOCK();

	return ret;
}

#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
int secmem_fr_set_svp_region(u64 pa, u32 size, int remote_region_type)
{
	int ret = 0;
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_SET_SVP_REGION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;
	cmd_params.param2 = remote_region_type;

	if (pa == 0 && size == 0)
		return TMEM_OK;

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

	ret = tee_directly_invoke_cmd(&cmd_params);

	mtk_iommu_sec_init(SEC_ID_SVP);

	return ret;
}

int secmem_fr_set_wfd_region(u64 pa, u32 size, int remote_region_type)
{
	int ret = 0;
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_SET_WFD_REGION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;
	cmd_params.param2 = remote_region_type;

	if (pa == 0 && size == 0)
		return TMEM_OK;

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

	ret = tee_directly_invoke_cmd(&cmd_params);

	mtk_iommu_sec_init(SEC_ID_WFD);

	return ret;
}

int secmem_fr_set_prot_shared_region(u64 pa, u32 size, int remote_region_type)
{
	int ret = 0;
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_SET_PROT_REGION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;
	cmd_params.param2 = remote_region_type;

	if (pa == 0 && size == 0)
		return TMEM_OK;

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

	ret = tee_directly_invoke_cmd(&cmd_params);

	mtk_iommu_sec_init(SEC_ID_SEC_CAM);

	return ret;
}

int secmem_fr_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_2D_FR_SMEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}
#endif

#if IS_ENABLED(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)
int secmem_set_mchunks_region(u64 pa, u32 size, int remote_region_type)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_SET_MCHUNKS_REGION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;
	cmd_params.param2 = remote_region_type;

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

	return tee_directly_invoke_cmd(&cmd_params);
}
#endif

#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)
int secmem_svp_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}

int secmem_dynamic_debug_control(bool enable_dbg)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_DYNAMIC_DEBUG_CONFIG;
	cmd_params.param2 = enable_dbg;
	return tee_directly_invoke_cmd(&cmd_params);
}

int secmem_force_hw_protection(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_SEC_MEM_FORCE_HW_PROTECTION;
	return tee_directly_invoke_cmd(&cmd_params);
}
#endif

#if IS_ENABLED(CONFIG_MTK_WFD_SMEM_SUPPORT)
int wfd_smem_dump_info(void)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = CMD_WFD_SMEM_DUMP_MEM_INFO;
	return tee_directly_invoke_cmd(&cmd_params);
}
#endif
