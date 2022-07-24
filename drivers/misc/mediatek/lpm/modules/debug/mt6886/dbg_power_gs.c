// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <lpm_dbg_fs_common.h>
#include <dbg_power_gs.h>

#include "mtk_power_gs_array.h"
#include <lpm_call.h>
#include <lpm_call_type.h>
#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>
int pwr_gs_set(unsigned int type, const struct lpm_data *val)
{
	int ret = 0;

	if (val->d.v_u32 & GS_PMIC)
		ret = lpm_pwr_gs_compare(LPM_GS_CMP_PMIC, type);
	if (ret)
		return ret;

	if (val->d.v_u32 & GS_DCM)
		ret = lpm_pwr_gs_compare_by_type(
			LPM_GS_CMP_CLK, type, GS_DCM);
	if (ret)
		return ret;
	if (val->d.v_u32 & GS_CG)
		ret = lpm_pwr_gs_compare_by_type(
			LPM_GS_CMP_CLK, type, GS_CG);

	return ret;
}
struct lpm_callee pwr_gs_callee = {
	.uid = LPM_CALLEE_PWR_GS,
	.i.simple = {
		.set = pwr_gs_set,
	},
};

struct lpm_gs_clk clk_cg = {
	.type = GS_CG,
	.name = "CG",
};
struct lpm_gs_clk clk_dcm = {
	.type = GS_DCM,
	.name = "DCM",
};
struct lpm_gs_clk *clks[] = {
	&clk_dcm,
	&clk_cg,
	NULL,
};

struct lpm_gs_clk_info clk_infos = {
	.attach = NULL,
	.dcm = clks,
};


struct lpm_gs_pmic *pmic[] = {
	NULL,
};


struct lpm_gs_pmic_info pmic_infos = {
	.pmic = pmic,
	.attach = NULL,
};

int power_gs_init(void)
{
	lpm_callee_registry(&pwr_gs_callee);
	/* initial gs compare method */
	lpm_pwr_gs_common_init();
	/* initial gs pmic information */
	lpm_pwr_gs_compare_init(LPM_GS_CMP_PMIC, &pmic_infos);
#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
	/* initial gs dcm information */
	lpm_pwr_gs_compare_init(LPM_GS_CMP_CLK, &clk_infos);
#endif
	return 0;
}

void power_gs_deinit(void)
{
	lpm_pwr_gs_common_deinit();
}
