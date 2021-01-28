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
#include <linux/sizes.h>
#include <memory_ssmr.h>

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_priv.h"
#include "private/tmem_utils.h"
#include "private/tmem_dev_desc.h"
/* clang-format off */
#include "mtee_impl/mtee_ops.h"
/* clang-format on */
#include "tee_impl/tee_invoke.h"

static struct trusted_mem_configs mchunk_general_configs = {
	.session_keep_alive_enable = false,
	.minimal_chunk_size = SZ_2M,
	.phys_mem_shift_bits = 10,
	.phys_limit_min_alloc_size = (1 << 10),
	.min_size_check_enable = false,
	.alignment_check_enable = false,
	.caps = 0,
};

static struct tmem_device_description mtee_mchunks[] = {
#if IS_ENABLED(CONFIG_MTK_PROT_MEM_SUPPORT)
	{
		.kern_tmem_type = TRUSTED_MEM_PROT,
		.tee_smem_type = TEE_SMEM_PROT,
		.mtee_chunks_id = MTEE_MCHUNKS_PROT,
		.ssmr_feature_id = SSMR_FEAT_PROT_SHAREDMEM,
		.u_ops_data.mtee = {.mem_type = TRUSTED_MEM_PROT},
#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)                                  \
	&& IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
		.notify_remote = true,
		.notify_remote_fn = secmem_fr_set_prot_shared_region,
#else
		.notify_remote = false,
		.notify_remote_fn = NULL,
#endif
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "PMEM",
	},
#endif

#if IS_ENABLED(CONFIG_MTK_HAPP_MEM_SUPPORT)
	{
		.kern_tmem_type = TRUSTED_MEM_HAPP,
		.tee_smem_type = TEE_SMEM_HAPP_ELF,
		.mtee_chunks_id = MTEE_MCHUNKS_HAPP,
		.ssmr_feature_id = SSMR_FEAT_TA_ELF,
		.u_ops_data.mtee = {.mem_type = TRUSTED_MEM_HAPP},
		.notify_remote = true,
		.notify_remote_fn = secmem_set_mchunks_region,
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "MTEE_HAPP",
	},
	{
		.kern_tmem_type = TRUSTED_MEM_HAPP_EXTRA,
		.tee_smem_type = TEE_SMEM_HAPP_EXTRA,
		.mtee_chunks_id = MTEE_MCHUNKS_HAPP_EXTRA,
		.ssmr_feature_id = SSMR_FEAT_TA_STACK_HEAP,
		.u_ops_data.mtee = {.mem_type = TRUSTED_MEM_HAPP_EXTRA},
		.notify_remote = true,
		.notify_remote_fn = secmem_set_mchunks_region,
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "MTEE_HAPP_EXTRA",
	},
#endif

#if IS_ENABLED(CONFIG_MTK_SDSP_MEM_SUPPORT)
	{
		.kern_tmem_type = TRUSTED_MEM_SDSP,
		.tee_smem_type = TEE_SMEM_SDSP_FIRMWARE,
		.mtee_chunks_id = MTEE_MCHUNKS_SDSP,
		.ssmr_feature_id = SSMR_FEAT_SDSP_FIRMWARE,
		.u_ops_data.mtee = {.mem_type = TRUSTED_MEM_SDSP},
		.notify_remote = true,
		.notify_remote_fn = secmem_set_mchunks_region,
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "SDSP",
	},
#endif

#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)                             \
	&& (IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)                   \
	    || IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE))
	{
		.kern_tmem_type = TRUSTED_MEM_SDSP_SHARED,
		.tee_smem_type = TEE_SMEM_SDSP_SHARED,
#if IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_MTEE_TEE)
		.mtee_chunks_id = MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE,
#elif IS_ENABLED(CONFIG_MTK_SDSP_SHARED_PERM_VPU_MTEE_TEE)
		.mtee_chunks_id = MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE,
#else
		.mtee_chunks_id = MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE,
#endif
		.ssmr_feature_id = SSMR_FEAT_SDSP_TEE_SHAREDMEM,
		.u_ops_data.mtee = {.mem_type = TRUSTED_MEM_SDSP_SHARED},
		.notify_remote = true,
		.notify_remote_fn = secmem_set_mchunks_region,
		.mem_cfg = &mchunk_general_configs,
		.dev_name = "SDSP_SHARED",
	},
#endif
};

#define MTEE_MCHUNKS_DEVICE_COUNT ARRAY_SIZE(mtee_mchunks)

static struct trusted_mem_device *
create_mtee_mchunk_device(enum TRUSTED_MEM_TYPE mem_type,
			  struct trusted_mem_configs *cfg,
			  struct tmem_device_description *dev_desc,
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
	t_device->dev_desc = dev_desc;

	snprintf(t_device->name, MAX_DEVICE_NAME_LEN, "%s", dev_name);
	t_device->ssmr_feature_id = ssmr_feat_id;
	t_device->mem_type = mem_type;

	ret = register_trusted_mem_device(mem_type, t_device);
	if (ret) {
		destroy_trusted_mem_device(t_device);
		pr_err("register device failed: %d:%s\n", mem_type, dev_name);
		return NULL;
	}

	return t_device;
}

int mtee_mchunks_init(void)
{
	struct trusted_mem_device *t_device;
	int idx;

	pr_info("%s:%d (%d)\n", __func__, __LINE__,
		(int)MTEE_MCHUNKS_DEVICE_COUNT);

	for (idx = 0; idx < MTEE_MCHUNKS_DEVICE_COUNT; idx++) {
		t_device = create_mtee_mchunk_device(
			mtee_mchunks[idx].kern_tmem_type,
			mtee_mchunks[idx].mem_cfg, &mtee_mchunks[idx],
			mtee_mchunks[idx].ssmr_feature_id,
			mtee_mchunks[idx].dev_name);
		if (INVALID(t_device)) {
			pr_err("create mchunk device failed: %d:%s\n",
			       mtee_mchunks[idx].kern_tmem_type,
			       mtee_mchunks[idx].dev_name);
			return TMEM_CREATE_DEVICE_FAILED;
		}
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

void mtee_mchunks_exit(void)
{
}
