/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "helio-dvfsrc-smc-control.h"

#define SPM_FLAG_RUN_COMMON_SCENARIO          (1U << 10)
#define SPM_FLAG_DIS_VCORE_DVS                (1U << 3)
#define SPM_FLAG_DIS_VCORE_DFS                (1U << 4)

inline void mtk_spmfw_init(int dvfsrc_en, int skip_check)
{
	int dvfsrc_flag = dvfsrc_en >> 1;
		unsigned int spm_flags = SPM_FLAG_RUN_COMMON_SCENARIO;

	if (dvfsrc_en & 1) {
		if (dvfsrc_flag & 0x1)
			spm_flags |= SPM_FLAG_DIS_VCORE_DVS;

		if (dvfsrc_flag & 0x2)
			spm_flags |= SPM_FLAG_DIS_VCORE_DFS;
	} else
		spm_flags |= (
			SPM_FLAG_DIS_VCORE_DVS | SPM_FLAG_DIS_VCORE_DFS);

	if (helio_dvfsrc_firmware_status() &&
		!skip_check)
		return;

	helio_dvfsrc_smc(VCOREFS_SMC_CMD_0, 0, 0, 0);
	helio_dvfsrc_smc(VCOREFS_SMC_CMD_1, spm_flags, 0, 0);
}

inline void spm_dvfs_pwrap_cmd(int pwrap_cmd, int pwrap_vcore)
{
	helio_dvfsrc_smc(VCOREFS_SMC_CMD_3, pwrap_cmd, pwrap_vcore, 0);
}

inline int spm_load_firmware_status(void)
{
	return helio_dvfsrc_firmware_status();
}

inline int spm_get_spmfw_idx(void)
{
	return helio_dvfsrc_spmfw_type();
}

