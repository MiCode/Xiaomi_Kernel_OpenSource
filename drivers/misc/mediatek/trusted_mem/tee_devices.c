// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include "private/tmem_dev_desc.h"
#include "tee_impl/tee_ops.h"
#if defined(CONFIG_MTK_GZ_KREE)
#include "mtee_impl/mtee_invoke.h"
#endif

static struct trusted_mem_configs tee_smem_general_configs = {
	.session_keep_alive_enable = false,
	.minimal_chunk_size = SIZE_64K,
	.phys_mem_shift_bits = 6,
	.phys_limit_min_alloc_size = (1 << 6),
	.min_size_check_enable = false,
	.alignment_check_enable = true,
	.caps = 0,
};

static struct tmem_device_description tee_smem_devs[] = {
/* If CONFIG_MTK_SVP_ON_MTEE_SUPPORT enabled, then SVP on MTEE */
#ifndef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
#ifdef CONFIG_MTK_SECURE_MEM_SUPPORT
	{
		.kern_tmem_type = TRUSTED_MEM_SVP,
		.tee_smem_type = TEE_SMEM_SVP,
		.mtee_chunks_id = MTEE_MCUHNKS_INVALID,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_SVP,
#endif
		/* clang-format off */
		.u_ops_data.tee = {
			.tee_cmds[TEE_OP_ALLOC] = CMD_SEC_MEM_ALLOC,
			.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_SEC_MEM_ALLOC_ZERO,
			.tee_cmds[TEE_OP_FREE] = CMD_SEC_MEM_UNREF,
			.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_SEC_MEM_ENABLE,
			.tee_cmds[TEE_OP_REGION_DISABLE] = CMD_SEC_MEM_DISABLE,
		},
		/* clang-format on */
		.notify_remote = false,
		.notify_remote_fn = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_SVP",
	},
#endif
#endif

#ifdef CONFIG_MTK_CAM_SECURITY_SUPPORT
	{
		.kern_tmem_type = TRUSTED_MEM_2D_FR,
		.tee_smem_type = TEE_SMEM_2D_FR,
		.mtee_chunks_id = MTEE_MCUHNKS_INVALID,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_2D_FR,
#endif
		/* clang-format off */
		.u_ops_data.tee = {
			.tee_cmds[TEE_OP_ALLOC] = CMD_2D_FR_SMEM_ALLOC,
			.tee_cmds[TEE_OP_ALLOC_ZERO] =
				CMD_2D_FR_SMEM_ALLOC_ZERO,
			.tee_cmds[TEE_OP_FREE] = CMD_2D_FR_SMEM_UNREF,
			.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_2D_FR_SMEM_ENABLE,
			.tee_cmds[TEE_OP_REGION_DISABLE] =
				CMD_2D_FR_SMEM_DISABLE,
		},
		/* clang-format on */
		.notify_remote = false,
		.notify_remote_fn = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_2DFR",
	},
#endif

#ifndef CONFIG_MTK_SVP_ON_MTEE_SUPPORT
#ifdef CONFIG_MTK_WFD_SMEM_SUPPORT
	{
		.kern_tmem_type = TRUSTED_MEM_WFD,
		.tee_smem_type = TEE_SMEM_WFD,
		.mtee_chunks_id = MTEE_MCUHNKS_INVALID,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_WFD,
#endif
		/* clang-format off */
		.u_ops_data.tee = {
			.tee_cmds[TEE_OP_ALLOC] = CMD_WFD_SMEM_ALLOC,
			.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_WFD_SMEM_ALLOC_ZERO,
			.tee_cmds[TEE_OP_FREE] = CMD_WFD_SMEM_UNREF,
			.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_WFD_SMEM_ENABLE,
			.tee_cmds[TEE_OP_REGION_DISABLE] = CMD_WFD_SMEM_DISABLE,
		},
		/* clang-format on */
		.notify_remote = false,
		.notify_remote_fn = NULL,
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_WFD",
	},
#endif
#endif

#if defined(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT)                                \
	&& (defined(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE))
	{
		.kern_tmem_type = TRUSTED_MEM_SDSP_SHARED,
		.tee_smem_type = TEE_SMEM_SDSP_SHARED,
		.mtee_chunks_id = MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE,
#if defined(CONFIG_MTK_SSMR) || (defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP))
		.ssmr_feature_id = SSMR_FEAT_SDSP_TEE_SHAREDMEM,
#endif
		/* clang-format off */
		.u_ops_data.tee = {
			.tee_cmds[TEE_OP_ALLOC] = CMD_SDSP_SMEM_ALLOC,
			.tee_cmds[TEE_OP_ALLOC_ZERO] = CMD_SDSP_SMEM_ALLOC_ZERO,
			.tee_cmds[TEE_OP_FREE] = CMD_SDSP_SMEM_UNREF,
			.tee_cmds[TEE_OP_REGION_ENABLE] = CMD_SDSP_SMEM_ENABLE,
			.tee_cmds[TEE_OP_REGION_DISABLE] =
				CMD_SDSP_SMEM_DISABLE,
		},
/* clang-format on */
#if defined(CONFIG_MTK_GZ_KREE)
		.notify_remote = true,
		.notify_remote_fn = mtee_set_mchunks_region,
#else
		.notify_remote = false,
		.notify_remote_fn = NULL,
#endif
		.mem_cfg = &tee_smem_general_configs,
		.dev_name = "SECMEM_SDSP",
	},
#endif
};

#define TEE_SECURE_MEM_DEVICE_COUNT ARRAY_SIZE(tee_smem_devs)

static struct trusted_mem_device *
create_tee_smem_device(enum TRUSTED_MEM_TYPE mem_type,
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

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) || defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	get_tee_peer_ops(&t_device->peer_ops);
#endif
	t_device->dev_desc = dev_desc;

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

int tee_smem_devs_init(void)
{
	struct trusted_mem_device *t_device;
	int idx = 0;

	pr_info("%s:%d DEVICE COUNT:(%d), MIN CHUNK SIZE: (0x%x)\n", __func__, __LINE__,
		(int)TEE_SECURE_MEM_DEVICE_COUNT, tee_smem_general_configs.minimal_chunk_size);

	for (idx = 0; idx < TEE_SECURE_MEM_DEVICE_COUNT; idx++) {
		t_device = create_tee_smem_device(
			tee_smem_devs[idx].kern_tmem_type,
			tee_smem_devs[idx].mem_cfg, &tee_smem_devs[idx],
			tee_smem_devs[idx].ssmr_feature_id,
			tee_smem_devs[idx].dev_name);
		if (INVALID(t_device)) {
			pr_err("create tee smem device failed: %d:%s\n",
			       tee_smem_devs[idx].kern_tmem_type,
			       tee_smem_devs[idx].dev_name);
			return TMEM_CREATE_DEVICE_FAILED;
		}
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

void tee_smem_devs_exit(void)
{
}
