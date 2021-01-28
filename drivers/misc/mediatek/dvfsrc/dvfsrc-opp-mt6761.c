// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mt-plat/mtk_devinfo.h>
#include <mtk_dramc.h>

#define MTK_SIP_VCOREFS_SET_FREQ 16
#define MTK_SIP_VCOREFS_SET_EFUSE 17
#define NUM_DRAM_OPP 3

static int __init dvfsrc_opp_init(void)
{
	int i;
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_SET_EFUSE,
		0, get_devinfo_with_index(56), 0, 0, 0, 0,
		&ares);

#if IS_ENABLED(CONFIG_MTK_DRAMC_LEGACY)
	for (i = 0; i < NUM_DRAM_OPP; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_SET_FREQ,
			i, dram_steps_freq(i), 0, 0, 0, 0,
			&ares);
	}
#endif
	return 0;
}
fs_initcall_sync(dvfsrc_opp_init);
