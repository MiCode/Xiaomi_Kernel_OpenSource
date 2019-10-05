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
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
#include <memory_ssmr.h>
#endif
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>

#define PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS
#include "pmem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "pmem/pmem_mock.h"
#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#include "private/secmem_ext.h"
#endif

#define PMEM_DEVICE_NAME "PMEM"

static const char mem_srv_name[] = "com.mediatek.geniezone.srv.mem";

#define LOCK_BY_CALLEE (0)
#if LOCK_BY_CALLEE
#define GZ_SESSION_LOCK_INIT() mutex_init(&sess_data->lock)
#define GZ_SESSION_LOCK() mutex_lock(&sess_data->lock)
#define GZ_SESSION_UNLOCK() mutex_unlock(&sess_data->lock)
#else
#define GZ_SESSION_LOCK_INIT()
#define GZ_SESSION_LOCK()
#define GZ_SESSION_UNLOCK()
#endif

struct GZ_SESSION_DATA {
	KREE_SESSION_HANDLE session_handle;
	KREE_SECUREMEM_HANDLE append_mem_handle;
#if LOCK_BY_CALLEE
	struct mutex lock;
#endif
};

static struct GZ_SESSION_DATA *gz_create_session_data(void)
{
	struct GZ_SESSION_DATA *sess_data;

	sess_data = mld_kmalloc(sizeof(struct GZ_SESSION_DATA), GFP_KERNEL);
	if (INVALID(sess_data)) {
		pr_err("%s:%d out of memory!\n", __func__, __LINE__);
		return NULL;
	}

	memset(sess_data, 0x0, sizeof(struct GZ_SESSION_DATA));

	GZ_SESSION_LOCK_INIT();
	return sess_data;
}

static void gz_destroy_session_data(struct GZ_SESSION_DATA *sess_data)
{
	if (VALID(sess_data))
		mld_kfree(sess_data);
}

static int gz_session_open(void **peer_data, void *priv)
{
	int ret = 0;
	struct GZ_SESSION_DATA *sess_data;

	UNUSED(priv);

	sess_data = gz_create_session_data();
	if (INVALID(sess_data)) {
		pr_err("Create session data failed: out of memory!\n");
		return TMEM_GZ_CREATE_SESSION_FAILED;
	}

	GZ_SESSION_LOCK();

	ret = KREE_CreateSession(mem_srv_name, &sess_data->session_handle);
	if (ret != 0) {
		pr_err("GZ open session failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_CREATE_SESSION_FAILED;
	}

	*peer_data = (void *)sess_data;
	GZ_SESSION_UNLOCK();
	return TMEM_OK;
}

static int gz_session_close(void *peer_data, void *priv)
{
	int ret;
	struct GZ_SESSION_DATA *sess_data = (struct GZ_SESSION_DATA *)peer_data;

	UNUSED(priv);
	GZ_SESSION_LOCK();

	ret = KREE_CloseSession(sess_data->session_handle);
	if (ret != 0) {
		pr_err("GZ close session failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_CLOSE_SESSION_FAILED;
	}

	GZ_SESSION_UNLOCK();
	gz_destroy_session_data(sess_data);
	return TMEM_OK;
}

static int gz_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		    u8 *owner, u32 id, u32 clean, void *peer_data, void *priv)
{
	int ret;
	struct GZ_SESSION_DATA *sess_data = (struct GZ_SESSION_DATA *)peer_data;

	UNUSED(priv);
	GZ_SESSION_LOCK();

	if (clean) {
		ret = KREE_ION_ZallocChunkmem(sess_data->session_handle,
					      sess_data->append_mem_handle,
					      sec_handle, alignment, size);
	} else {
		ret = KREE_ION_AllocChunkmem(sess_data->session_handle,
					     sess_data->append_mem_handle,
					     sec_handle, alignment, size);
	}

	if (ret != 0) {
		pr_err("GZ alloc chunk memory failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_ALLOC_CHUNK_FAILED;
	}

	*refcount = 1;
	GZ_SESSION_UNLOCK();
	return TMEM_OK;
}

static int gz_free(u32 sec_handle, u8 *owner, u32 id, void *peer_data,
		   void *priv)
{
	int ret;
	struct GZ_SESSION_DATA *sess_data = (struct GZ_SESSION_DATA *)peer_data;

	UNUSED(priv);
	GZ_SESSION_LOCK();

	ret = KREE_ION_UnreferenceChunkmem(sess_data->session_handle,
					   sec_handle);
	if (ret != 0) {
		pr_err("GZ free chunk memory failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_FREE_CHUNK_FAILED;
	}

	GZ_SESSION_UNLOCK();
	return TMEM_OK;
}

static int gz_mem_reg_cfg_notify_tee(u64 pa, u32 size)
{
#if defined(CONFIG_MTK_SECURE_MEM_SUPPORT)                                     \
	&& defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	return secmem_fr_set_prot_shared_region(pa, size);
#else
	return TMEM_OK;
#endif
}

static int gz_mem_reg_add(u64 pa, u32 size, void *peer_data, void *priv)
{
	int ret;
	struct GZ_SESSION_DATA *sess_data = (struct GZ_SESSION_DATA *)peer_data;
	KREE_SHAREDMEM_PARAM mem_param;

	UNUSED(priv);
	mem_param.buffer = (void *)pa;
	mem_param.size = size;
	mem_param.mapAry = NULL;

	GZ_SESSION_LOCK();

	ret = KREE_AppendSecureMultichunkmem(sess_data->session_handle,
					     &sess_data->append_mem_handle,
					     &mem_param);
	if (ret != 0) {
		pr_err("GZ append reg mem failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_MOCK_APPEND_MEMORY_FAILED;
	}

	ret = gz_mem_reg_cfg_notify_tee(pa, size);
	if (ret != 0) {
		pr_err("GZ notify reg mem add to TEE failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_MOCK_NOTIFY_MEM_ADD_CFG_TO_TEE_FAILED;
	}

	GZ_SESSION_UNLOCK();
	return TMEM_OK;
}

static int gz_mem_reg_remove(void *peer_data, void *priv)
{
	int ret;
	struct GZ_SESSION_DATA *sess_data = (struct GZ_SESSION_DATA *)peer_data;

	UNUSED(priv);
	GZ_SESSION_LOCK();

	ret = KREE_ReleaseSecureMultichunkmem(sess_data->session_handle,
					      sess_data->append_mem_handle);
	if (ret != 0) {
		pr_err("GZ release reg mem failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_RELEASE_MEMORY_FAILED;
	}

	ret = gz_mem_reg_cfg_notify_tee(0x0ULL, 0x0);
	if (ret != 0) {
		pr_err("GZ notify reg mem remove to TEE failed:%d\n", ret);
		GZ_SESSION_UNLOCK();
		return TMEM_GZ_NOTIFY_MEM_REMOVE_CFG_TO_TEE_FAILED;
	}

	GZ_SESSION_UNLOCK();
	return TMEM_OK;
}

static int gz_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			     void *peer_data, void *priv)
{
	UNUSED(invoke_params);
	UNUSED(peer_data);
	UNUSED(priv);

	pr_err("%s:%d operation is not implemented yet!\n", __func__, __LINE__);
	return TMEM_OPERATION_NOT_IMPLEMENTED;
}

static struct trusted_driver_operations pmem_driver_ops = {
	.session_open = gz_session_open,
	.session_close = gz_session_close,
	.memory_alloc = gz_alloc,
	.memory_free = gz_free,
	.memory_grant = gz_mem_reg_add,
	.memory_reclaim = gz_mem_reg_remove,
	.invoke_cmd = gz_invoke_command,
};

static struct trusted_mem_configs pmem_configs = {
#if defined(PMEM_MOCK_MTEE)
	.mock_peer_enable = true,
#endif
#if defined(PMEM_MOCK_SSMR)
	.mock_ssmr_enable = true,
#endif
#if defined(PMEM_MTEE_SESSION_KEEP_ALIVE)
	.session_keep_alive_enable = true,
#endif
	.minimal_chunk_size = PMEM_MIN_ALLOC_CHUNK_SIZE,
	.phys_mem_shift_bits = PMEM_64BIT_PHYS_SHIFT,
	.phys_limit_min_alloc_size = (1 << PMEM_64BIT_PHYS_SHIFT),
#if defined(PMEM_MIN_SIZE_CHECK)
	.min_size_check_enable = true,
#endif
#if defined(PMEM_ALIGNMENT_CHECK)
	.alignment_check_enable = true,
#endif
	.caps = 0,
};

#ifdef PMEM_MOCK_OBJECT_SUPPORT
static int pmem_open(struct inode *inode, struct file *file)
{
	UNUSED(inode);
	UNUSED(file);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static int pmem_release(struct inode *inode, struct file *file)
{
	UNUSED(inode);
	UNUSED(file);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

#define REG_CORE_OPS_STR_LEN (32)
static char register_core_ops_str[REG_CORE_OPS_STR_LEN];
static char *get_registered_core_ops(void)
{
	return register_core_ops_str;
}

static ssize_t pmem_read(struct file *file, char __user *user_buf, size_t count,
			 loff_t *offset)
{
	char *ops_str = get_registered_core_ops();

	return simple_read_from_buffer(user_buf, count, offset, ops_str,
				       strlen(ops_str));
}

static const struct file_operations pmem_proc_fops = {
	.owner = THIS_MODULE,
	.open = pmem_open,
	.release = pmem_release,
	.read = pmem_read,
};

static void pmem_create_proc_entry(void)
{
#if defined(PMEM_MOCK_MTEE) && defined(PMEM_MOCK_SSMR)
	pr_info("PMEM_MOCK_ALL\n");
	snprintf(register_core_ops_str, REG_CORE_OPS_STR_LEN, "PMEM_MOCK_ALL");
#elif defined(PMEM_MOCK_MTEE)
	pr_info("PMEM_MOCK_MTEE\n");
	snprintf(register_core_ops_str, REG_CORE_OPS_STR_LEN, "PMEM_MOCK_MTEE");
#else
	pr_info("PMEM_CORE_OPS\n");
	snprintf(register_core_ops_str, REG_CORE_OPS_STR_LEN, "PMEM_CORE_OPS");
#endif

	proc_create("pmem0", 0664, NULL, &pmem_proc_fops);
}
#endif

static int __init pmem_init(void)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *t_device;

	pr_info("%s:%d\n", __func__, __LINE__);

	t_device = create_trusted_mem_device(TRUSTED_MEM_PROT, &pmem_configs);
	if (INVALID(t_device)) {
		pr_err("create PMEM device failed\n");
		return TMEM_CREATE_DEVICE_FAILED;
	}

#ifdef PMEM_MOCK_OBJECT_SUPPORT
	if (pmem_configs.mock_peer_enable)
		get_mocked_peer_ops(&t_device->mock_peer_ops);
	if (pmem_configs.mock_ssmr_enable)
		get_mocked_ssmr_ops(&t_device->mock_ssmr_ops);
#endif
	t_device->peer_ops = &pmem_driver_ops;

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s", PMEM_DEVICE_NAME);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_device->ssmr_feature_id = SSMR_FEAT_PROT_SHAREDMEM;
#endif
	t_device->mem_type = TRUSTED_MEM_PROT;
	t_device->shared_trusted_mem_device = NULL;

	ret = register_trusted_mem_device(TRUSTED_MEM_PROT, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register PMEM device failed\n");
		return ret;
	}

#ifdef PMEM_MOCK_OBJECT_SUPPORT
	pmem_create_proc_entry();
#endif

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit pmem_exit(void)
{
}

module_init(pmem_init);
module_exit(pmem_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Protect Memory Driver");
