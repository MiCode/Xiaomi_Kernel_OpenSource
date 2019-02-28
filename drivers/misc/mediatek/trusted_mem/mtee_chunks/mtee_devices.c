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
#include "mtee_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
/* clang-format off */
#include "mtee_impl/mtee_priv.h"
/* clang-format on */

struct mtee_chunk_memory_configs {
	enum TRUSTED_MEM_TYPE mem_type;
	u32 ssmr_feature_id;
	struct mtee_peer_ops_priv_data priv_data;
	struct trusted_mem_configs *mem_cfg;
	char *dev_name;
};

static struct trusted_mem_configs mchunk_general_configs = {
#ifdef MTEE_MCHUNKS_SESSION_KEEP_ALIVE
	.session_keep_alive_enable = true,
#endif
	.minimal_chunk_size = MTEE_MCHUNKS_MIN_ALLOC_CHUNK_SIZE,
	.phys_mem_shift_bits = MTEE_64BIT_PHYS_SHIFT,
	.phys_limit_min_alloc_size = (1 << MTEE_64BIT_PHYS_SHIFT),
#if defined(MTEE_MCHUNKS_MIN_SIZE_CHECK)
	.min_size_check_enable = true,
#endif
#if defined(MTEE_MCHUNKS_ALIGNMENT_CHECK)
	.alignment_check_enable = true,
#endif
	.caps = 0,
};

static struct mtee_chunk_memory_configs mtee_mchunks[] = {
#ifdef CONFIG_MTK_HAPP_MEM_SUPPORT
	{
		.mem_type = TRUSTED_MEM_HAPP,
		.ssmr_feature_id = SSMR_FEAT_TA_ELF,
		.priv_data = {.mem_type = TRUSTED_MEM_HAPP},
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "MTEE_HAPP",
	},
	{
		.mem_type = TRUSTED_MEM_HAPP_EXTRA,
		.ssmr_feature_id = SSMR_FEAT_TA_STACK_HEAP,
		.priv_data = {.mem_type = TRUSTED_MEM_HAPP_EXTRA},
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "MTEE_HAPP_EXTRA",
	},
#endif
#ifdef CONFIG_MTK_SDSP_MEM_SUPPORT
	{
		.mem_type = TRUSTED_MEM_SDSP,
		.ssmr_feature_id = SSMR_FEAT_SDSP_FIRMWARE,
		.priv_data = {.mem_type = TRUSTED_MEM_SDSP},
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "SDSP",
	},
#endif
#if defined(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)                                \
	&& (defined(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)                      \
	    || defined(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE))
	{
		.mem_type = TRUSTED_MEM_SDSP_SHARED,
		.ssmr_feature_id = SSMR_FEAT_SDSP_TEE_SHAREDMEM,
		.priv_data = {.mem_type = TRUSTED_MEM_SDSP_SHARED},
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "SDSP_SHARED",
	},
#endif
};

#define MTEE_MCHUNKS_DEVICE_COUNT ARRAY_SIZE(mtee_mchunks)

static struct trusted_mem_device *
create_mtee_mchunk_device(enum TRUSTED_MEM_TYPE mem_type,
			  struct trusted_mem_configs *cfg,
			  struct mtee_peer_ops_priv_data *priv_data,
			  u32 ssmr_feat_id, const char *dev_name)
{
	int ret = TMEM_OK;
	struct trusted_mem_device *t_device;

	t_device = create_trusted_mem_device(mem_type, cfg);
	if (INVALID(t_device)) {
		pr_err("create device failed: %d:%s\n", mem_type, dev_name);
		return NULL;
	}

	get_mtee_peer_ops(&t_device->peer_ops);
	t_device->peer_priv = priv_data;

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s", dev_name);
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
	t_device->ssmr_feature_id = ssmr_feat_id;
#endif
	t_device->mem_type = mem_type;
	t_device->shared_trusted_mem_device = NULL;

	ret = register_trusted_mem_device(mem_type, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register device failed: %d:%s\n", mem_type, dev_name);
		return NULL;
	}

	return t_device;
}

static int __init mtee_mchunks_init(void)
{
	struct trusted_mem_device *t_device;
	int idx = 0;

	pr_info("%s:%d (%d)\n", __func__, __LINE__, MTEE_MCHUNKS_DEVICE_COUNT);

	for (idx = 0; idx < MTEE_MCHUNKS_DEVICE_COUNT; idx++) {
		t_device = create_mtee_mchunk_device(
			mtee_mchunks[idx].mem_type, mtee_mchunks[idx].mem_cfg,
			&mtee_mchunks[idx].priv_data,
			mtee_mchunks[idx].ssmr_feature_id,
			mtee_mchunks[idx].dev_name);
		if (INVALID(t_device)) {
			pr_err("create mchunk device failed: %d:%s\n",
			       mtee_mchunks[idx].mem_type,
			       mtee_mchunks[idx].dev_name);
			return TMEM_CREATE_DEVICE_FAILED;
		}
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static void __exit mtee_mchunks_exit(void)
{
}

module_init(mtee_mchunks_init);
module_exit(mtee_mchunks_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MTEE Multiple Chunks Memory Driver");
