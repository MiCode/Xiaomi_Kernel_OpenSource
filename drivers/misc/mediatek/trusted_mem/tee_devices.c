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

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
#include "private/tmem_utils.h"
/* clang-format off */
#include "tee_impl/tee_priv.h"
/* clang-format on */
#include "tee_impl/tee_common.h"

struct tee_secure_memory_configs {
	enum TRUSTED_MEM_TYPE tmem_type;
	enum TEE_MEM_TYPE tee_smem_type;
	u32 ssmr_feature_id;
	struct tee_op_cmd_mappings *priv_data;
	struct trusted_mem_configs *mem_cfg;
	char *dev_name;
};

static struct trusted_mem_configs tee_smem_general_configs = {
	.session_keep_alive_enable = false,
	.minimal_chunk_size = SIZE_64K,
	.phys_mem_shift_bits = 6,
	.phys_limit_min_alloc_size = (1 << 6),
	.min_size_check_enable = false,
	.alignment_check_enable = true,
	.caps = 0,
};

static struct tee_secure_memory_configs tee_smem_devs[] = {
#ifdef CONFIG_MTK_SECURE_MEM_SUPPORT
	{
		.tmem_type = TRUSTED_MEM_SVP,
		.tee_smem_type = TEE_MEM_SVP,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_SVP,
#endif
		.priv_data = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_SVP",
	},
#endif

#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
	{
		.tmem_type = TRUSTED_MEM_2D_FR,
		.tee_smem_type = TEE_MEM_2D_FR,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_2D_FR,
#endif
		.priv_data = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_2DFR",
	},
#endif

#ifdef CONFIG_MTK_WFD_SMEM_SUPPORT
	{
		.tmem_type = TRUSTED_MEM_WFD,
		.tee_smem_type = TEE_MEM_WFD,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_WFD,
#endif
		.priv_data = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_WFD",
	},
#endif

#if defined(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)                                \
	&& (defined(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE))
	{
		.tmem_type = TRUSTED_MEM_SDSP_SHARED,
		.tee_smem_type = TEE_MEM_SDSP_SHARED,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_SDSP_TEE_SHAREDMEM,
#endif
		.priv_data = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_SDSP",
	},
#endif
};

#define TEE_SECURE_MEM_DEVICE_COUNT ARRAY_SIZE(tee_smem_devs)

static struct trusted_mem_device *
create_tee_smem_device(enum TRUSTED_MEM_TYPE mem_type,
		       struct trusted_mem_configs *cfg,
		       struct tee_op_cmd_mappings *priv_data, u32 ssmr_feat_id,
		       const char *dev_name)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *t_device;

	t_device = create_trusted_mem_device(mem_type, cfg);
	if (INVALID(t_device)) {
		pr_err("create device failed: %d:%s\n", mem_type, dev_name);
		return NULL;
	}

	get_tee_peer_ops(&t_device->peer_ops);
	t_device->peer_priv = priv_data;

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s", dev_name);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_device->ssmr_feature_id = ssmr_feat_id;
#endif
	t_device->mem_type = mem_type;

	ret = register_trusted_mem_device(mem_type, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register device failed: %d:%s\n", mem_type, dev_name);
		return NULL;
	}

	return t_device;
}

static int __init tee_smem_devs_init(void)
{
	struct trusted_mem_device *t_device;
	int idx = 0;

	pr_info("%s:%d (%d)\n", __func__, __LINE__,
		TEE_SECURE_MEM_DEVICE_COUNT);

	for (idx = 0; idx < TEE_SECURE_MEM_DEVICE_COUNT; idx++) {
		get_tee_peer_priv_data(tee_smem_devs[idx].tee_smem_type,
				       &tee_smem_devs[idx].priv_data);
		t_device = create_tee_smem_device(
			tee_smem_devs[idx].tmem_type,
			tee_smem_devs[idx].mem_cfg,
			tee_smem_devs[idx].priv_data,
			tee_smem_devs[idx].ssmr_feature_id,
			tee_smem_devs[idx].dev_name);
		if (INVALID(t_device)) {
			pr_err("create tee smem device failed: %d:%s\n",
			       tee_smem_devs[idx].tmem_type,
			       tee_smem_devs[idx].dev_name);
			return TMEM_CREATE_DEVICE_FAILED;
		}
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit tee_smem_devs_exit(void)
{
}

module_init(tee_smem_devs_init);
module_exit(tee_smem_devs_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek TEE Secure Memory Device Driver");
