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
#if IS_ENABLED(CONFIG_MTK_GZ_KREE)
#include <tz_cross/ta_mem.h>
#endif

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_dev_desc.h"
#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
#include "tests/ut_common.h"
#endif
#include "public/mtee_regions.h"
/* clang-format off */
#include "mtee_impl/mtee_ops.h"
/* clang-format on */
#include "tee_impl/tee_ops.h"

#define MTEE_CMD_LOCK() mutex_lock(&mtee_lock)
#define MTEE_CMD_UNLOCK() mutex_unlock(&mtee_lock)

static DEFINE_MUTEX(mtee_lock);
static struct trusted_driver_operations *mtee_ops;
static void *mtee_session_data;

/* clang-format off */
static struct tmem_device_description mtee_dev_desc = {
	.u_ops_data.mtee = {
		.mem_type = TRUSTED_MEM_INVALID,
		.service_name = NULL,
	}
};
/* clang-format on */

static inline int
mtee_directly_invoke_cmd_locked(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	if (unlikely(INVALID(invoke_params)))
		return TMEM_PARAMETER_ERROR;

	if (unlikely(INVALID(mtee_ops)))
		get_mtee_peer_ops(&mtee_ops);

	if (mtee_ops->session_open(&mtee_session_data, &mtee_dev_desc)) {
		pr_err("%s:%d mtee open session failed!\n", __func__, __LINE__);
		return TMEM_MTEE_CREATE_SESSION_FAILED;
	}

	ret = mtee_ops->invoke_cmd(invoke_params, mtee_session_data,
				   &mtee_dev_desc);

	if (mtee_ops->session_close(mtee_session_data, &mtee_dev_desc))
		pr_err("%s:%d mtee close session failed!\n", __func__,
		       __LINE__);

	return ret;
}

int mtee_directly_invoke_cmd(struct trusted_driver_cmd_params *invoke_params)
{
	int ret = TMEM_OK;

	MTEE_CMD_LOCK();
	ret = mtee_directly_invoke_cmd_locked(invoke_params);
	MTEE_CMD_UNLOCK();

	return ret;
}

#define TEE_NOTIFY_MTEE_CHUNK_REGION_INFO_SUPPORT (1)
int mtee_set_mchunks_region(u64 pa, u32 size, int remote_region_type)
{
	struct trusted_driver_cmd_params cmd_params = {0};

	cmd_params.cmd = TZCMD_MEM_CONFIG_CHUNKMEM_INFO_ION;
	cmd_params.param0 = pa;
	cmd_params.param1 = size;
	cmd_params.param2 = remote_region_type;

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	if (is_multi_type_alloc_multithread_test_locked()) {
		pr_debug("%s:%d return for UT purpose!\n", __func__, __LINE__);
		return TMEM_OK;
	}
#endif

#if TEE_NOTIFY_MTEE_CHUNK_REGION_INFO_SUPPORT
	return mtee_directly_invoke_cmd(&cmd_params);
#else
	pr_info("TEE notify reg mem to MTEE is not supported, mchunk=%d\n",
		(u32)cmd_params.param2);
	return TMEM_OK;
#endif
}
