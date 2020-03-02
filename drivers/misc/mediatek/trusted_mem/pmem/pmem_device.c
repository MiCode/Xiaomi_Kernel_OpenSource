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

#define PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS
#include "pmem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "pmem/pmem_mock.h"
#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
/* clang-format off */
#include "mtee_impl/mtee_priv.h"
/* clang-format on */

#define PMEM_DEVICE_NAME "PMEM"

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

static struct mtee_peer_ops_priv_data pmem_priv_data = {
	.mem_type = TRUSTED_MEM_PROT,
};

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
	get_mtee_peer_ops(&t_device->peer_ops);
	t_device->peer_priv = &pmem_priv_data;

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
