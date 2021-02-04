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

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
/* clang-format off */
#include "mtee_impl/mtee_priv.h"
/* clang-format on */

#define PMEM_DEVICE_NAME "PMEM"

static struct trusted_mem_configs pmem_configs = {
#if defined(PMEM_MTEE_SESSION_KEEP_ALIVE)
	.session_keep_alive_enable = true,
#endif
	.minimal_chunk_size = SIZE_2M,
	.phys_mem_shift_bits = PMEM_64BIT_PHYS_SHIFT,
	.phys_limit_min_alloc_size = (1 << PMEM_64BIT_PHYS_SHIFT),
	.min_size_check_enable = true,
	.alignment_check_enable = true,
	.caps = 0,
};

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
