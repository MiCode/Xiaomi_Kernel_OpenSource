// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012-2019 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "sprd_sip: " fmt

#include <linux/arm-smccc.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sprd_sip_svc.h>
#include <linux/string.h>
#include <linux/types.h>

/* SIP Function Identifier */

/* SIP General */
#define	SPRD_SIP_SVC_CALL_COUNT						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF00)

#define	SPRD_SIP_SVC_UID						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF01)

#define	SPRD_SIP_SVC_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0xFF03)

/* SIP performance operations */
#define	SPRD_SIP_SVC_PERF_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0100)

#define	SPRD_SIP_SVC_PERF_SET_FREQ					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0101)

#define	SPRD_SIP_SVC_PERF_GET_FREQ					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0102)

#define	SPRD_SIP_SVC_PERF_SET_DIV					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0103)

#define	SPRD_SIP_SVC_PERF_GET_DIV					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0104)

#define	SPRD_SIP_SVC_PERF_SET_PARENT					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0105)

#define	SPRD_SIP_SVC_PERF_GET_PARENT					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0106)

/* SIP debug operations */
#define SPRD_SIP_SVC_DBG_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0200)

#define SPRD_SIP_SVC_DBG_SET_HANG_HDL_AARCH32				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0201)

#define SPRD_SIP_SVC_DBG_SET_HANG_HDL_AARCH64				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_64,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0201)

#define SPRD_SIP_SVC_DBG_GET_HANG_CTX_AARCH32				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0202)

#define SPRD_SIP_SVC_DBG_GET_HANG_CTX_AARCH64				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_64,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0202)

/* SIP storage operations */
#define	SPRD_SIP_SVC_STORAGE_REV					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0300)

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
#define SPRD_SIP_SVC_STORAGE_UFS_CRYPTO_ENABLE                          \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0301)

#define SPRD_SIP_SVC_STORAGE_UFS_CRYPTO_DISABLE                         \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0302)
#endif

/* SIP power operations */
#define SPRD_SIP_SVC_PWR_REV						\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0500)

#define SPRD_SIP_SVC_PWR_GET_WAKEUP_SOURCE_GET				\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0501)

#define SPRD_SIP_SVC_PDBG_INFO_TRANS					\
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,				\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0502)
/* SIP dvfs operations */
#define SPRD_SIP_SVC_DVFS_REV						\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0600))

#define SPRD_SIP_SVC_DVFS_ENABLE					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0601))

#define SPRD_SIP_SVC_DVFS_TABLE_UPDATE					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0602))

#define SPRD_SIP_SVC_DVFS_STEP_SET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0603))

#define SPRD_SIP_SVC_DVFS_MARGIN_SET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0604))

#define SPRD_SIP_SVC_DVFS_FREQ_SET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0605))

#define SPRD_SIP_SVC_DVFS_FREQ_GET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0606))

#define SPRD_SIP_SVC_DVFS_PAIR_GET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0607))

#define SPRD_SIP_SVC_DVFS_PMIC_SET					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0608))

#define SPRD_SIP_SVC_DVFS_INIT						\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x060b))

#define SPRD_SIP_SVC_DVFS_DEBUG_INIT					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x060c))

/* SIP npu operations */
#define SPRD_SIP_SVC_NPU_REV						\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0700))

#define SPRD_SIP_SVC_NPU_SET_FREQ					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0701))

#define SPRD_SIP_SVC_NPU_DISABLE_IDLE					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0702))

#define SPRD_SIP_SVC_NPU_GET_MAX_STATE					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0703))

#define SPRD_SIP_SVC_NPU_GET_OPP					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0704))

#define SPRD_SIP_SVC_NPU_SET_VOLTS					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0705))

/* SIP gpu operations */
#define SPRD_SIP_SVC_GPU_REV						\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0800))

#define SPRD_SIP_SVC_GPU_GET_ID						\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0801))

#define SPRD_SIP_SVC_GPU_UPDATE_VOLTAGE_LIST 				\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0802))

/* SIP socdump operations */
#define SPRD_SIP_SVC_SOCDUMP_REV					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0900))

#define SPRD_SIP_SVC_SOCDUMP_FUNC_API					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			   ARM_SMCCC_SMC_32,				\
			   ARM_SMCCC_OWNER_SIP,				\
			   0x0901))

#if IS_ENABLED(CONFIG_UNISOC_CACHEDUMP)
/* SIP cache dump operations */
#define SPRD_SIP_SVC_CACHEDUMP_REV					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			    ARM_SMCCC_SMC_32,				\
			    ARM_SMCCC_OWNER_SIP,			\
			    0x0A00))

#define SPRD_SIP_SVC_CACHEDUMP_FUNC_API					\
	(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL,			\
			    ARM_SMCCC_SMC_32,				\
			    ARM_SMCCC_OWNER_SIP,			\
			    0x0A01))
#endif

#define SPRD_SIP_RET_UNK	0xFFFFFFFFUL

static struct sprd_sip_svc_handle sprd_sip_svc_handle = {};

static int sprd_sip_remap_err(unsigned long err)
{
	switch (err) {
	case 0:
		return 0;

	case SPRD_SIP_RET_UNK:
		return -EINVAL;

	default:
		return -EINVAL;
	}
}

struct sprd_sip_svc_handle *sprd_sip_svc_get_handle(void)
{
	return &sprd_sip_svc_handle;
}
EXPORT_SYMBOL(sprd_sip_svc_get_handle);

static int sprd_sip_svc_perf_set_freq(u32 id, u32 parent_id, u32 freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_FREQ,
			id, parent_id, freq, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_perf_get_freq(u32 id, u32 parent_id, u32 *p_freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_FREQ,
			id, parent_id, 0, 0, 0, 0, 0, &res);

	if (p_freq != NULL)
		*p_freq = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_perf_set_div(u32 id, u32 div)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_DIV,
			id, div, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_perf_get_div(u32 id, u32 *p_div)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_DIV,
			id, 0, 0, 0, 0, 0, 0, &res);

	if (p_div != NULL)
		*p_div = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_perf_set_parent(u32 id, u32 parent_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_SET_PARENT,
			id, parent_id, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_perf_get_parent(u32 id, u32 *p_parent_id)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PERF_GET_PARENT,
			id, 0, 0, 0, 0, 0, 0, &res);

	if (p_parent_id != NULL)
		*p_parent_id = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dbg_set_hang_hdl(uintptr_t hdl,
					 uintptr_t pgd,
					 unsigned long level)
{
	struct arm_smccc_res res;
	pr_notice("hdl for sprd svc sip : func_addr = 0x%lx, pdg = 0x%lx, level = 0x%lx\n",
		  hdl, pgd, level);
#ifdef CONFIG_ARM64
	arm_smccc_smc(SPRD_SIP_SVC_DBG_SET_HANG_HDL_AARCH64,
		      hdl, pgd, level, 0, 0, 0, 0, &res);
#else
	arm_smccc_smc(SPRD_SIP_SVC_DBG_SET_HANG_HDL_AARCH32,
		      hdl, pgd, level, 0, 0, 0, 0, &res);
#endif

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dbg_get_hang_ctx(unsigned long id, unsigned long core_id, uintptr_t *val)
{
	struct arm_smccc_res res;

#ifdef CONFIG_ARM64
	arm_smccc_smc(SPRD_SIP_SVC_DBG_GET_HANG_CTX_AARCH64,
		      id, core_id, 0, 0, 0, 0, 0, &res);
#else
	arm_smccc_smc(SPRD_SIP_SVC_DBG_GET_HANG_CTX_AARCH32,
		      id, core_id, 0, 0, 0, 0, 0, &res);
#endif

	if (val != NULL)
		*val = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_pwr_get_wakeup_source(u32 *major, u32 *second, u32 *thrid)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PWR_GET_WAKEUP_SOURCE_GET, 0, 0, 0, 0, 0, 0, 0, &res);

	if (major != NULL)
		*major = res.a1;

	if (second != NULL)
		*second = res.a2;

	if (thrid != NULL)
		*thrid = res.a3;

	return res.a0;
}

static u64 sprd_sip_svc_pdbg_info_trans(u32 scene, u32 priv0, u32 priv1, u32 priv2,
					struct arm_smccc_res *ret)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_PDBG_INFO_TRANS, scene, priv0, priv1, priv2, 0, 0, 0, &res);

	if (ret != NULL)
		*ret = res;

	return res.a0;
}

static int sprd_sip_svc_dvfs_enable(u32 cluster)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_ENABLE,
		      cluster, 0, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_table_update(u32 cluster, u32 temp, u32 *num)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_TABLE_UPDATE,
		      cluster, temp, 0, 0, 0, 0, 0, &res);

	if (num)
		*num = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_step_set(u32 cluster, u32 step)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_STEP_SET,
		      cluster, step, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_margin_set(u32 cluster, u32 margin)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_MARGIN_SET,
		      cluster, margin, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_freq_set(u32 cluster, u32 index)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_FREQ_SET,
		      cluster, index, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_freq_get(u32 cluster, u64 *freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_FREQ_GET,
		      cluster, 0, 0, 0, 0, 0, 0, &res);

	if (freq)
		*freq = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_pair_get(u32 cluster, u32 index, u64 *freq, u64 *vol)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_PAIR_GET,
		      cluster, index, 0, 0, 0, 0, 0, &res);

	if (freq)
		*freq = res.a1;

	if (vol)
		*vol = res.a2;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_pmic_set(u32 cluster, u32 num)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_PMIC_SET,
		      cluster, num, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_init(u32 flag, u32 flag2, u32 flag3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_INIT,
		      flag, flag2, flag3, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_dvfs_debug_init(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_DVFS_DEBUG_INIT,
		      0, 0, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_gpu_get_id(u32 *chip_id, u32 *gpu_bin)
{
	struct arm_smccc_res res = {0, 0, 0, 0};

	arm_smccc_smc(SPRD_SIP_SVC_GPU_GET_ID,
				0, 0, 0, 0, 0, 0, 0, &res);

	if (chip_id)
		*chip_id = res.a1;
	if (gpu_bin)
		*gpu_bin = res.a2;

	return sprd_sip_remap_err(res.a0);
}
static int sprd_sip_svc_gpu_update_voltage_list(u32 temp, u32 pos)
{
	struct arm_smccc_res res = {0, 0, 0, 0};

	arm_smccc_smc(SPRD_SIP_SVC_GPU_UPDATE_VOLTAGE_LIST,
				temp, pos, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
static int sprd_sip_svc_storage_ufs_crypto_enable(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_STORAGE_UFS_CRYPTO_ENABLE,
			0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static int sprd_sip_svc_storage_ufs_crypto_disable(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_STORAGE_UFS_CRYPTO_DISABLE,
			0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}
#endif

static int sprd_sip_svc_npu_set_freq(u32 freq)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_NPU_SET_FREQ,
			freq, 0, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_npu_disable_idle(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_NPU_DISABLE_IDLE,
			0, 0, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_npu_get_max_state(u32 *max_state)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_NPU_GET_MAX_STATE,
			0, 0, 0, 0, 0, 0, 0, &res);

	if (max_state)
		*max_state = res.a1;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_npu_get_opp(u32 index, u32 *freq, u32 *volt)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_NPU_GET_OPP,
			index, 0, 0, 0, 0, 0, 0, &res);

	if (freq)
		*freq = res.a1;
	if (volt)
		*volt = res.a2;

	return sprd_sip_remap_err(res.a0);
}

static int sprd_sip_svc_npu_set_volts(u32 high_temp)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_NPU_SET_VOLTS,
			high_temp, 0, 0, 0, 0, 0, 0, &res);

	return sprd_sip_remap_err(res.a0);
}

#if IS_ENABLED(CONFIG_UNISOC_CACHEDUMP)
static int sprd_sip_svc_cachedump_func_api(uint8_t mesi, uint8_t valid)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_CACHEDUMP_FUNC_API,
			mesi, valid, 0, 0, 0, 0, 0, &res);

	return res.a0;
}
#endif

static int sprd_sip_svc_socdump_func_api(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SPRD_SIP_SVC_SOCDUMP_FUNC_API,
			0, 0, 0, 0, 0, 0, 0, &res);

	pr_info("%s: socdump in sml pass!\n", __func__);

	return res.a0;
}

static int __init sprd_sip_svc_init(void)
{
	int ret = 0;
	struct arm_smccc_res res;

	/* init perf_ops */
	arm_smccc_smc(SPRD_SIP_SVC_PERF_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.perf_ops.rev.major_ver = res.a0;
	sprd_sip_svc_handle.perf_ops.rev.minor_ver = res.a1;

	sprd_sip_svc_handle.perf_ops.set_freq = sprd_sip_svc_perf_set_freq;
	sprd_sip_svc_handle.perf_ops.get_freq = sprd_sip_svc_perf_get_freq;

	sprd_sip_svc_handle.perf_ops.set_div = sprd_sip_svc_perf_set_div;
	sprd_sip_svc_handle.perf_ops.get_div = sprd_sip_svc_perf_get_div;

	sprd_sip_svc_handle.perf_ops.set_parent = sprd_sip_svc_perf_set_parent;
	sprd_sip_svc_handle.perf_ops.get_parent = sprd_sip_svc_perf_get_parent;

	pr_notice("SPRD SIP SVC PERF:v%d.%d detected in firmware.\n",
		sprd_sip_svc_handle.perf_ops.rev.major_ver,
		sprd_sip_svc_handle.perf_ops.rev.minor_ver);

	/* init debug_ops */
	arm_smccc_smc(SPRD_SIP_SVC_DBG_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.dbg_ops.rev.major_ver = res.a0;
	sprd_sip_svc_handle.dbg_ops.rev.minor_ver = res.a1;

	sprd_sip_svc_handle.dbg_ops.set_hang_hdl =
				sprd_sip_svc_dbg_set_hang_hdl;
	sprd_sip_svc_handle.dbg_ops.get_hang_ctx =
				sprd_sip_svc_dbg_get_hang_ctx;

	pr_notice("SPRD SIP SVC DBG:v%d.%d detected in firmware.\n",
		sprd_sip_svc_handle.dbg_ops.rev.major_ver,
		sprd_sip_svc_handle.dbg_ops.rev.minor_ver);

	/* init pwr_ops */
	arm_smccc_smc(SPRD_SIP_SVC_PWR_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.pwr_ops.rev.major_ver = res.a0;
	sprd_sip_svc_handle.pwr_ops.rev.minor_ver = res.a1;

	sprd_sip_svc_handle.pwr_ops.get_wakeup_source = sprd_sip_svc_pwr_get_wakeup_source;
	sprd_sip_svc_handle.pwr_ops.pdbg_info_trans = sprd_sip_svc_pdbg_info_trans;

	/* init dvfs_ops */
	arm_smccc_smc(SPRD_SIP_SVC_DVFS_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.dvfs_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.dvfs_ops.rev.minor_ver = (u32)(res.a1);

	sprd_sip_svc_handle.dvfs_ops.dvfs_enable = sprd_sip_svc_dvfs_enable;
	sprd_sip_svc_handle.dvfs_ops.table_update = sprd_sip_svc_dvfs_table_update;
	sprd_sip_svc_handle.dvfs_ops.step_set = sprd_sip_svc_dvfs_step_set;
	sprd_sip_svc_handle.dvfs_ops.margin_set = sprd_sip_svc_dvfs_margin_set;
	sprd_sip_svc_handle.dvfs_ops.freq_set = sprd_sip_svc_dvfs_freq_set;
	sprd_sip_svc_handle.dvfs_ops.freq_get = sprd_sip_svc_dvfs_freq_get;
	sprd_sip_svc_handle.dvfs_ops.pair_get = sprd_sip_svc_dvfs_pair_get;
	sprd_sip_svc_handle.dvfs_ops.pmic_set = sprd_sip_svc_dvfs_pmic_set;
	sprd_sip_svc_handle.dvfs_ops.dvfs_init = sprd_sip_svc_dvfs_init;
	sprd_sip_svc_handle.dvfs_ops.dvfs_debug_init = sprd_sip_svc_dvfs_debug_init;

	pr_notice("SPRD SIP SVC DVFS:v%d.%d detected in firmware.\n",
		  sprd_sip_svc_handle.dvfs_ops.rev.major_ver,
		  sprd_sip_svc_handle.dvfs_ops.rev.minor_ver);

	/* init storage_ops */
	arm_smccc_smc(SPRD_SIP_SVC_STORAGE_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.storage_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.storage_ops.rev.minor_ver = (u32)(res.a1);

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	sprd_sip_svc_handle.storage_ops.ufs_crypto_enable  = sprd_sip_svc_storage_ufs_crypto_enable;
	sprd_sip_svc_handle.storage_ops.ufs_crypto_disable = sprd_sip_svc_storage_ufs_crypto_disable;
#endif

	pr_notice("SPRD SIP SVC STORAGE:v%d.%d detected in firmware.\n",
		sprd_sip_svc_handle.storage_ops.rev.major_ver,
		sprd_sip_svc_handle.storage_ops.rev.minor_ver);

	/* init npu_ops */
	arm_smccc_smc(SPRD_SIP_SVC_NPU_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.npu_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.npu_ops.rev.minor_ver = (u32)(res.a1);

	sprd_sip_svc_handle.npu_ops.set_freq =
				sprd_sip_svc_npu_set_freq;
	sprd_sip_svc_handle.npu_ops.disable_idle =
				sprd_sip_svc_npu_disable_idle;
	sprd_sip_svc_handle.npu_ops.get_max_state =
				sprd_sip_svc_npu_get_max_state;
	sprd_sip_svc_handle.npu_ops.get_opp =
				sprd_sip_svc_npu_get_opp;
	sprd_sip_svc_handle.npu_ops.set_volts =
				sprd_sip_svc_npu_set_volts;

	pr_notice("SPRD SIP SVC NPU:v%d.%d detected in firmware.\n",
		  sprd_sip_svc_handle.npu_ops.rev.major_ver,
		  sprd_sip_svc_handle.npu_ops.rev.minor_ver);

	/* init gpu_ops */
	arm_smccc_smc(SPRD_SIP_SVC_GPU_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.gpu_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.gpu_ops.rev.minor_ver = (u32)(res.a1);

	sprd_sip_svc_handle.gpu_ops.get_id =
				sprd_sip_svc_gpu_get_id;
	sprd_sip_svc_handle.gpu_ops.update_voltage_list =
				sprd_sip_svc_gpu_update_voltage_list;

	pr_notice("SPRD SIP SVC GPU:v%d.%d detected in firmware.\n",
		sprd_sip_svc_handle.gpu_ops.rev.major_ver,
		sprd_sip_svc_handle.gpu_ops.rev.minor_ver);

#if IS_ENABLED(CONFIG_UNISOC_CACHEDUMP)
	/* init cachedump_ops */
	arm_smccc_smc(SPRD_SIP_SVC_CACHEDUMP_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.cachedump_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.cachedump_ops.rev.minor_ver = (u32)(res.a1);

	sprd_sip_svc_handle.cachedump_ops.cachedump_func_api = sprd_sip_svc_cachedump_func_api;

	pr_notice("SPRD SIP SVC CACHEDUMP:v%d.%d detected in firmware.\n",
			sprd_sip_svc_handle.cachedump_ops.rev.major_ver,
			sprd_sip_svc_handle.cachedump_ops.rev.minor_ver);
#endif

	/* init socdump_ops */
	arm_smccc_smc(SPRD_SIP_SVC_SOCDUMP_REV, 0, 0, 0, 0, 0, 0, 0, &res);
	sprd_sip_svc_handle.socdump_ops.rev.major_ver = (u32)(res.a0);
	sprd_sip_svc_handle.socdump_ops.rev.minor_ver = (u32)(res.a1);

	sprd_sip_svc_handle.socdump_ops.socdump_func_api = sprd_sip_svc_socdump_func_api;

	pr_notice("SPRD SIP SVC SOCDUMP:v%d.%d detected in firmware.\n",
		  sprd_sip_svc_handle.socdump_ops.rev.major_ver,
		  sprd_sip_svc_handle.socdump_ops.rev.minor_ver);

	return ret;
}

subsys_initcall(sprd_sip_svc_init);

MODULE_DESCRIPTION("sprd implementation-specific services");
MODULE_LICENSE("GPL v2");
