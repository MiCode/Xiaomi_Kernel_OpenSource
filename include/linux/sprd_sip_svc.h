/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012-2015 Spreadtrum Communications Inc.
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
#ifndef __SPRD_SIP_SVC_H__
#define __SPRD_SIP_SVC_H__

#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/types.h>

/*
 * Provides interfaces to SoC implementation-specific services on this
 * platform, for example secure platform initialization, configuration,
 * and some power control services.
 */

/* structure definitions */

/**
 * struct sprd_sip_svc_rev_info - version information structure
 *
 * @major_ver: Major ABI version. Change here implies risk of backward
 *	compatibility break.
 * @minor_ver: Minor ABI version. Change here implies new feature addition,
 *	or compatible change in ABI.
 */
struct sprd_sip_svc_rev_info {
	u32 major_ver;
	u32 minor_ver;
};

/**
 * struct sprd_sip_svc_perf_ops - represents the various operations provided
 *	by SPRD SIP PERF
 *
 * @set_freq: sets the frequency of a given device
 * @get_freq: gets the frequency of a given device
 *
 * @set_div: sets the clock divisor of a given device
 * @get_div: gets the clock divisor of a given device
 *
 * @set_parent: sets the parent of a given device
 * @get_parent: gets the parent of a given device
 */
struct sprd_sip_svc_perf_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_freq)(u32 id, u32 parent_id, u32 freq);
	int (*get_freq)(u32 id, u32 parent_id, u32 *p_freq);

	int (*set_div)(u32 id, u32 div);
	int (*get_div)(u32 id, u32 *p_div);

	int (*set_parent)(u32 id, u32 parent_id);
	int (*get_parent)(u32 id, u32 *p_parent_id);
};

/**
 * struct sprd_sip_svc_dbg_ops - represents the various operations provided
 *	by SPRD SIP DBG
 *
 * @set_hang_handle: set hang handle
 */
struct sprd_sip_svc_dbg_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_hang_hdl)(uintptr_t hdl, uintptr_t pdg, unsigned long level);
	int (*get_hang_ctx)(unsigned long id, unsigned long core_id, uintptr_t *val);
};

/**
 * struct sprd_sip_svc_pwr_ops - represents the various operations
 *      provided by SPRD SIP POWER
 *
 * @get_wakeup_source: gets the wakeup source
 *
 */
struct sprd_sip_svc_pwr_ops {
	struct sprd_sip_svc_rev_info rev;
	int (*get_wakeup_source)(u32 *major, u32 *second, u32 *thrid);
	u64 (*pdbg_info_trans)(u32 scene, u32 priv0, u32 priv1, u32 priv2,
			       struct arm_smccc_res *ret);
};

/**
 * struct sprd_sip_svc_dvfs_ops - represents the various operations
 *      provided by SPRD SIP DVFS
 *
 * @dvfs_enable: enable cluster dvfs
 * @table_update: update dvfs table for cluster
 *
 * @step_set: set the cluster dcdc update step
 * @margin_set: set the cluster dcdc update margin
 *
 * @freq_set: set cluster frequency
 * @freq_get: get cluster current frequency
 * @pair_get: get freq and volt of index
 *
 * @pmic_set: set the dcdc_cpu uses pmic type
 * @dvfs_init: apcpu hwdvfs logic init entry
 */
struct sprd_sip_svc_dvfs_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*dvfs_enable)(u32 cluster);
	int (*table_update)(u32 cluster, u32 temp, u32 *num);
	int (*step_set)(u32 cluster, u32 step);
	int (*margin_set)(u32 cluster, u32 margin);
	int (*freq_set)(u32 cluster, u32 index);
	int (*freq_get)(u32 cluster, u64 *freq);
	int (*pair_get)(u32 cluster, u32 index, u64 *freq, u64 *vol);
	int (*pmic_set)(u32 cluster, u32 num);
	int (*dvfs_init)(u32 flag, u32 flag2, u32 flag3);
	int (*dvfs_debug_init)(void);
};

/**
 * struct sprd_sip_svc_storage_ops - represents the various operations
 * 	provided by SPRD SIP STORAGE
 *
 * @ufs_crypto_enable: make crypto cfg field configurable to normal world
 * @ufs_crypto_disable: make crypto cfg field non-configurable to normal world
 */
struct sprd_sip_svc_storage_ops {
	struct sprd_sip_svc_rev_info rev;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	int (*ufs_crypto_enable)(void);
	int (*ufs_crypto_disable)(void);
#endif
};

/* struct sprd_sip_svc_npu_ops - represents the various operations
 *	operationsprovided by SPRD SIP NPU
 *
 * @set_freq: set npu frequency
 * @disable_idle: disable npu idle and sw dvfs
 * @get_max_state: get the number of freq npu supported
 * @get_opp: get npu opp
 * @set_volts: set npu top volts
 *
 */
struct sprd_sip_svc_npu_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_freq)(u32 freq);
	int (*disable_idle)(void);
	int (*get_max_state)(u32 *max_state);
	int (*get_opp)(u32 index, u32 *freq, u32 *volt);
	int (*set_volts)(u32 high_temp);
};

/**
 * struct sprd_sip_svc_gpu_ops - represents the various operations
 * provided by SPRD SIP GPU
 *
 * @get_id: gets the chip id and gpu bin
 * @update_voltage_list: update the voltage index corresponding meaning for DVFS
 *
 */
struct sprd_sip_svc_gpu_ops {
	struct sprd_sip_svc_rev_info rev;
	int (*get_id)(u32 *chip_id, u32 *bin_index);
	int (*update_voltage_list)(u32 temp, u32 pos);
};

#if IS_ENABLED(CONFIG_UNISOC_CACHEDUMP)
/**
 * struct sprd_sip_svc_cachedump_ops - represents the various operations
 * provided by SPRD SIP CACHEDUMP
 *
 * @cachedump_func_api: execute cachedump function
 * @mesi: 0x1001 means save INVALID and MODIFED dcache
 * @valid: 0x1100 means save A64+A32 in A76, but means save Invalid+T32 in A55
 */
struct sprd_sip_svc_cachedump_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*cachedump_func_api)(uint8_t mesi, uint8_t valid);
};
#endif

/**
 * struct sprd_sip_svc_socdump_ops - represents the various operations
 * provided by SPRD SIP SOCDUMP
 *
 * @socdump_func_api: execute socdump function
 */
struct sprd_sip_svc_socdump_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*socdump_func_api)(void);
};

/**
 * struct sprd_sip_svc_handle - Handle returned to SPRD SIP clients for usage
 *
 * @perf_ops: pointer to set of performance operations
 * @dbg_ops: pointer to set of dbg operations
 * @storage_ops: pointer to set of storage operations
 * @gpu_ops: pointer to set of gpu operations
 */
struct sprd_sip_svc_handle {
	struct sprd_sip_svc_perf_ops perf_ops;
	struct sprd_sip_svc_dbg_ops dbg_ops;
	struct sprd_sip_svc_pwr_ops pwr_ops;
	struct sprd_sip_svc_dvfs_ops dvfs_ops;
	struct sprd_sip_svc_storage_ops storage_ops;
	struct sprd_sip_svc_npu_ops npu_ops;
	struct sprd_sip_svc_gpu_ops gpu_ops;
	struct sprd_sip_svc_socdump_ops socdump_ops;

#if IS_ENABLED(CONFIG_UNISOC_CACHEDUMP)
	struct sprd_sip_svc_cachedump_ops cachedump_ops;
#endif
};

/**
 * sprd_sip_svc_get_handle() - returns a pointer to SPRD SIP handle
 *
 * Return: a pointer to SPRD_SIP handle
 */
struct sprd_sip_svc_handle *sprd_sip_svc_get_handle(void);

#endif /* __SPRD_SIP_SVC_H__ */
