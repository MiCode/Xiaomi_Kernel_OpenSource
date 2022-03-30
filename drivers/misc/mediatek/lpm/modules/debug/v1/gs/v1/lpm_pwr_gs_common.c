// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_pwr_gs_internal.h>
#include <linux/module.h>

int lpm_pwr_gs_common_init(void)
{
	lpm_gs_dump_pmic_init();
	lpm_gs_dump_dcm_init();
	return 0;
}
EXPORT_SYMBOL(lpm_pwr_gs_common_init);

void lpm_pwr_gs_common_deinit(void)
{
	lpm_gs_dump_pmic_deinit();
	lpm_gs_dump_dcm_deinit();
}
EXPORT_SYMBOL(lpm_pwr_gs_common_deinit);
