// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <gs/mtk_lpm_pwr_gs.h>
#include <gs/v1/mtk_lpm_pwr_gs_internal.h>


int mtk_lpm_pwr_gs_common_init(void)
{
	mtk_lpm_gs_dump_pmic_init();
	mtk_lpm_gs_dump_dcm_init();
	return 0;
}

void mtk_lpm_pwr_gs_common_deinit(void)
{
	mtk_lpm_gs_dump_pmic_deinit();
	mtk_lpm_gs_dump_dcm_deinit();
}

