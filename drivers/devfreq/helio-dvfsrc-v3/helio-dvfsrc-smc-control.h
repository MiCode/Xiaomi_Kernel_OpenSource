/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __HELIO_DVFSRC_COMMON_H__
#define __HELIO_DVFSRC_COMMON_H__
#if defined(DVFSRC_SMC_CONTROL)
#include<linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>

enum VCORE_REG_STATUS {
	DVFS_LV,
	DVFS_FINAL_LV,
	DVFS_R9,
	DVFS_MD_STATUS,
	DVFS_MD_SRCLKEN,
};

enum vcorefs_smc_cmd {
	VCOREFS_SMC_CMD_0,
	VCOREFS_SMC_CMD_1,
	VCOREFS_SMC_CMD_2,
	VCOREFS_SMC_CMD_3,
	VCOREFS_SMC_CMD_4,
	/* check spmfw status */
	VCOREFS_SMC_CMD_5,

	/* get spmfw type */
	VCOREFS_SMC_CMD_6,

	/* get spm reg status */
	VCOREFS_SMC_CMD_7,

	NUM_VCOREFS_SMC_CMD,
};

#define helio_dvfsrc_smc_impl(p1, p2, p3, p4, p5, res) \
				arm_smccc_smc(p1, p2, p3, p4\
							, p5, 0, 0, 0, &res)


#define helio_dvfsrc_smc(opid, p1, p2, p3) ({\
	struct arm_smccc_res res;\
	helio_dvfsrc_smc_impl(\
			MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,\
			opid, p1, p2, p3, res);\
	res.a0; })

#define helio_dvfsrc_spmfw_type()\
		helio_dvfsrc_smc(VCOREFS_SMC_CMD_6, 0, 0, 0)


#define helio_dvfsrc_firmware_status()\
		helio_dvfsrc_smc(VCOREFS_SMC_CMD_5, 0, 0, 0)


#define helio_dvfsrc_reg_status(offset)\
		helio_dvfsrc_smc(VCOREFS_SMC_CMD_7, offset, 0, 0)

#endif


extern int spm_load_firmware_status(void);
extern void mtk_spmfw_init(int dvfsrc_en, int skip_check);
extern void spm_dvfs_pwrap_cmd(int pwrap_cmd, int pwrap_vcore);
extern int spm_get_spmfw_idx(void);
#endif

