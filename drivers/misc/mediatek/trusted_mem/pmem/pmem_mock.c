/*
 * Copyright (C) 2018 MediaTek Inc.
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

#define TMEM_MOCK_FMT
#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/mutex.h>

#define PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS
#include "pmem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "pmem/pmem_mock.h"
#include "pmem/memmgr_buddy.h"
#include "private/tmem_error.h"
#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#include "private/secmem_ext.h"
#endif

#define LOCK_BY_CALLEE (0)
#if LOCK_BY_CALLEE
static DEFINE_MUTEX(mock_mem_lock);
#define MOCK_SESSION_LOCK() mutex_lock(&mock_mem_lock)
#define MOCK_SESSION_UNLOCK() mutex_unlock(&mock_mem_lock)
#else
#define MOCK_SESSION_LOCK()
#define MOCK_SESSION_UNLOCK()
#endif

#define MOCK_POOL_PA_ADDR64_START (0x180000000ULL)
#define MOCK_POOL_SIZE SIZE_128M

static int mock_ssmr_get(u64 *pa, u32 *size, u32 feat, void *priv)
{
	UNUSED(feat);
	UNUSED(priv);

	pr_info("%s:%d\n", __func__, __LINE__);

	*pa = MOCK_POOL_PA_ADDR64_START;
	*size = MOCK_POOL_SIZE;

	pr_debug("pa address is 0x%llx, size is 0x%x\n", *pa, *size);
	return TMEM_OK;
}

static int mock_ssmr_put(u32 feat, void *priv)
{
	UNUSED(feat);
	UNUSED(priv);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static int mock_mtee_chunk_alloc(u32 alignment, u32 size, u32 *refcount,
				 u32 *sec_handle, u8 *owner, u32 id, u32 clean,
				 void *peer_data, void *priv)
{
	u32 adjust_alignment = alignment;

	UNUSED(peer_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(priv);

	MOCK_SESSION_LOCK();

	if (memmgr_alloc(adjust_alignment, size, refcount, sec_handle, clean)
	    != TMEM_OK) {
		MOCK_SESSION_UNLOCK();
		return TMEM_MOCK_ALLOC_FAILED;
	}

	MOCK_SESSION_UNLOCK();
	return TMEM_OK;
}

static int mock_mtee_chunk_free(u32 sec_handle, u8 *owner, u32 id,
				void *peer_data, void *priv)
{
	UNUSED(peer_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(priv);

	MOCK_SESSION_LOCK();
	memmgr_free(sec_handle);
	MOCK_SESSION_UNLOCK();

	return TMEM_OK;
}

static int mock_mem_reg_cfg_notify_tee(u64 pa, u32 size)
{
#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	return secmem_fr_set_prot_shared_region(pa, size);
#else
	return TMEM_OK;
#endif
}

static int mock_mtee_mem_add(u64 pa, u32 size, void *peer_data, void *priv)
{
	int ret = TMEM_OK;
	u64 mock_size = size;

	UNUSED(peer_data);
	UNUSED(priv);

	if (mock_size > MOCK_POOL_SIZE)
		mock_size = MOCK_POOL_SIZE;

	pr_debug("add mem pa is 0x%llx, size is 0x%x, adjust to 0x%llx\n", pa,
		 size, mock_size);

	MOCK_SESSION_LOCK();

	ret = memmgr_add_region(pa, mock_size);
	if (ret != 0) {
		pr_err("notify reg mem add to TEE failed:%d\n", ret);
		MOCK_SESSION_UNLOCK();
		return TMEM_MOCK_NOTIFY_MEM_ADD_CFG_TO_TEE_FAILED;
	}

	ret = mock_mem_reg_cfg_notify_tee(pa, size);
	if (ret != 0) {
		pr_err("notify reg mem add to TEE failed:%d\n", ret);
		MOCK_SESSION_UNLOCK();
		return TMEM_MOCK_NOTIFY_MEM_ADD_CFG_TO_TEE_FAILED;
	}

	MOCK_SESSION_UNLOCK();
	return ret;
}

static int mock_mtee_mem_remove(void *peer_data, void *priv)
{
	int ret = TMEM_OK;

	UNUSED(peer_data);
	UNUSED(priv);

	pr_info("%s:%d\n", __func__, __LINE__);

	MOCK_SESSION_LOCK();

	ret = memmgr_remove_region();
	if (ret != 0) {
		pr_err("release reg mem failed:%d\n", ret);
		MOCK_SESSION_UNLOCK();
		return TMEM_MOCK_RELEASE_MEMORY_FAILED;
	}

	ret = mock_mem_reg_cfg_notify_tee(0x0ULL, 0x0);
	if (ret != 0) {
		pr_err("notify reg mem remove to TEE failed:%d\n", ret);
		MOCK_SESSION_UNLOCK();
		return TMEM_MOCK_NOTIFY_MEM_REMOVE_CFG_TO_TEE_FAILED;
	}

	MOCK_SESSION_UNLOCK();
	return ret;
}

static int mock_mtee_open(void **peer_data, void *priv)
{
	UNUSED(peer_data);
	UNUSED(priv);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static int mock_mtee_close(void *peer_data, void *priv)
{
	UNUSED(peer_data);
	UNUSED(priv);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static int mock_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			       void *peer_data, void *priv)
{
	UNUSED(invoke_params);
	UNUSED(peer_data);
	UNUSED(priv);

	pr_err("%s:%d operation is not implemented yet!\n", __func__, __LINE__);
	return TMEM_OPERATION_NOT_IMPLEMENTED;
}

static struct trusted_driver_operations pmem_mock_peer_ops = {
	.session_open = mock_mtee_open,
	.session_close = mock_mtee_close,
	.memory_alloc = mock_mtee_chunk_alloc,
	.memory_free = mock_mtee_chunk_free,
	.memory_grant = mock_mtee_mem_add,
	.memory_reclaim = mock_mtee_mem_remove,
	.invoke_cmd = mock_invoke_command,
};

static struct ssmr_operations pmem_mock_ssmr_ops = {
	.offline = mock_ssmr_get,
	.online = mock_ssmr_put,
};

void get_mocked_peer_ops(struct trusted_driver_operations **ops)
{
	*ops = &pmem_mock_peer_ops;
}

void get_mocked_ssmr_ops(struct ssmr_operations **ops)
{
	*ops = &pmem_mock_ssmr_ops;
}
