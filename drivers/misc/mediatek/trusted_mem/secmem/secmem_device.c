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
#include "secmem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
#include "tee_impl/tee_priv.h"
#include "tee_impl/tee_common.h"

#define SECMEM_SVP_DEVICE_NAME "SECMEM_SVP"
#define SECMEM_2DFR_DEVICE_NAME "SECMEM_2DFR"

static struct trusted_mem_configs secmem_configs = {
#if defined(SECMEM_TEE_SESSION_KEEP_ALIVE)
	.session_keep_alive_enable = true,
#endif
	.minimal_chunk_size = SECMEM_MIN_ALLOC_CHUNK_SIZE,
	.phys_mem_shift_bits = SECMEM_64BIT_PHYS_SHIFT,
	.phys_limit_min_alloc_size = (1 << SECMEM_64BIT_PHYS_SHIFT),
#if defined(SECMEM_MIN_SIZE_CHECK)
	.min_size_check_enable = true,
#endif
#if defined(SECMEM_ALIGNMENT_CHECK)
	.alignment_check_enable = true,
#endif
	.caps = 0,
};

static int __init secmem_init(void)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *t_device;
#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
	struct trusted_mem_device *t_shared_device;
#endif /* end of CONFIG_MTK_CAM_SECURITY_SUPPORT */

	pr_info("%s:%d\n", __func__, __LINE__);

	t_device = create_trusted_mem_device(TRUSTED_MEM_SVP, &secmem_configs);
	if (INVALID(t_device)) {
		pr_err("create SECMEM device failed\n");
		return TMEM_CREATE_DEVICE_FAILED;
	}

	get_tee_peer_ops(&t_device->peer_ops);
	get_tee_peer_priv_data(TEE_MEM_SVP, &t_device->peer_priv);

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s",
		 SECMEM_SVP_DEVICE_NAME);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_device->ssmr_feature_id = SSMR_FEAT_SVP;
#endif
	t_device->mem_type = TRUSTED_MEM_SVP;
	t_device->shared_trusted_mem_device = NULL;

	ret = register_trusted_mem_device(TRUSTED_MEM_SVP, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register SECMEM device failed\n");
		return ret;
	}

#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
	t_shared_device = create_and_register_shared_trusted_mem_device(
		TRUSTED_MEM_SVP_VIRT_2D_FR, t_device, SECMEM_2DFR_DEVICE_NAME);
	if (INVALID(t_shared_device)) {
		destroy_trusted_mem_device(t_device);
		pr_err("create SECMEM share device failed\n");
		return TMEM_CREATE_SHARED_DEVICE_FAILED;
	}

#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_shared_device->ssmr_feature_id = SSMR_FEAT_2D_FR;
#endif
	t_shared_device->mem_type = TRUSTED_MEM_SVP_VIRT_2D_FR;
#endif /* end of CONFIG_MTK_CAM_SECURITY_SUPPORT */

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit secmem_exit(void)
{
}

module_init(secmem_init);
module_exit(secmem_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SVP Secure Memory Driver");
