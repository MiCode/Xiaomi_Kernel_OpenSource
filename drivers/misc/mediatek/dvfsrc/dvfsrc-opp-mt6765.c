// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/module.h>
#include <soc/mediatek/dramc.h>

#define MTK_SIP_VCOREFS_SET_FREQ 16
#define NUM_DRAM_OPP 3

enum {
	SPMFW_LP4_2CH_3200 = 0,
	SPMFW_LP4X_2CH_3200,
	SPMFW_LP3_1CH_1866,
	SPMFW_LP4_2CH_2400,
	SPMFW_LP4X_2CH_2400,
};

int spmfw_init(void)
{
	struct arm_smccc_res ares;
	int spmfw_idx = -1;
	int ddr_type;
	int ddr_hz;

#if IS_ENABLED(CONFIG_MTK_DRAMC)
	ddr_type = mtk_dramc_get_ddr_type();
	ddr_hz = mtk_dramc_get_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4 && ddr_hz == 2400)
		spmfw_idx = SPMFW_LP4_2CH_2400;
	else if (ddr_type == TYPE_LPDDR4 && ddr_hz == 3200)
		spmfw_idx = SPMFW_LP4_2CH_3200;
	else if (ddr_type == TYPE_LPDDR4X && ddr_hz == 3200)
		spmfw_idx = SPMFW_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR4X && ddr_hz == 2400)
		spmfw_idx = SPMFW_LP4X_2CH_2400;
	else if (ddr_type == TYPE_LPDDR3)
		spmfw_idx = SPMFW_LP3_1CH_1866;

	pr_info("#@# %s(%d) __spmfw_idx 0x%x, ddr=[%d][%d]\n",
		__func__, __LINE__, spmfw_idx, ddr_type, ddr_hz);

	arm_smccc_smc(MTK_SIP_SMC_CMD(0x22A), 0,
		spmfw_idx, 0, 0, 0, 0, 0,
		&ares);
#endif
	return 0;
}

static int __init dvfsrc_opp_init(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	int i;
	struct arm_smccc_res ares;

	for (i = 0; i < NUM_DRAM_OPP; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_SET_FREQ,
			i, mtk_dramc_get_steps_freq(i), 0, 0, 0, 0,
			&ares);
	}
#endif
	spmfw_init();
	return 0;
}

subsys_initcall(dvfsrc_opp_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DVFSRC SPMFW INIT");


