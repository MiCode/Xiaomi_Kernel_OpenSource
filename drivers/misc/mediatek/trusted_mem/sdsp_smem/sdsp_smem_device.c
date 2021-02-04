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
#include "sdsp_smem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
#include "tee_impl/tee_priv.h"
#include "tee_impl/tee_common.h"

#define SECMEM_SDSP_DEVICE_NAME "SECMEM_SDSP"

static struct trusted_mem_configs sdsp_smem_configs = {
#if defined(SDSP_SMEM_TEE_SESSION_KEEP_ALIVE)
	.session_keep_alive_enable = true,
#endif
	.minimal_chunk_size = SDSP_SMEM_MIN_ALLOC_CHUNK_SIZE,
	.phys_mem_shift_bits = SDSP_SMEM_64BIT_PHYS_SHIFT,
	.phys_limit_min_alloc_size = (1 << SDSP_SMEM_64BIT_PHYS_SHIFT),
#if defined(SDSP_SMEM_MIN_SIZE_CHECK)
	.min_size_check_enable = true,
#endif
#if defined(SDSP_SMEM_ALIGNMENT_CHECK)
	.alignment_check_enable = true,
#endif
	.caps = 0,
};

static int __init sdsp_smem_init(void)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *t_device;

	pr_info("%s:%d\n", __func__, __LINE__);

	t_device = create_trusted_mem_device(TRUSTED_MEM_SDSP_SHARED,
					     &sdsp_smem_configs);
	if (INVALID(t_device)) {
		pr_err("create SDSP_SMEM device failed\n");
		return TMEM_CREATE_DEVICE_FAILED;
	}

	get_tee_peer_ops(&t_device->peer_ops);
	get_tee_peer_priv_data(TEE_MEM_SDSP_SHARED, &t_device->peer_priv);

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s",
		 SECMEM_SDSP_DEVICE_NAME);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_device->ssmr_feature_id = SSMR_FEAT_SDSP_TEE_SHAREDMEM;
#endif
	t_device->mem_type = TRUSTED_MEM_SDSP_SHARED;
	t_device->shared_trusted_mem_device = NULL;

	ret = register_trusted_mem_device(TRUSTED_MEM_SDSP_SHARED, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register SDSP_SMEM device failed\n");
		return ret;
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit sdsp_smem_exit(void)
{
}

module_init(sdsp_smem_init);
module_exit(sdsp_smem_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SDSP Secure Memory Driver");
