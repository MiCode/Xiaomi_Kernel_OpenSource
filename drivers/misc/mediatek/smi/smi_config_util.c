/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/io.h>
#include <linux/string.h>
#include "smi_reg.h"
#include "mt_smi.h"
#include "smi_common.h"
#include "smi_configuration.h"
#include "smi_config_util.h"


int smi_bus_regs_setting(int larb_id, int profile, struct SMI_SETTING *settings)
{
	int i = 0;
	int j = 0;

	if (!settings || profile < 0 || profile >= SMI_BWC_SCEN_CNT || !larb_id)
		return -1;

	if (settings->smi_common_reg_num == 0)
		return -1;

	/* set regs of common */
	if (larb_id) {
		for (i = 0 ; i < settings->smi_common_reg_num ; ++i) {
			M4U_WriteReg32(SMI_COMMON_EXT_BASE,
			settings->smi_common_setting_vals[i].offset,
			settings->smi_common_setting_vals[i].value);
		}
	}

	/* set regs of larbs */
	for (i = 0 ; i < SMI_LARB_NR ; ++i) {
		int larb_mask = 1 << i;

		if (larb_id & larb_mask) {
			for (j = 0 ; j < settings->smi_larb_reg_num[i] ; ++j) {
				M4U_WriteReg32(gLarbBaseAddr[i],
				settings->smi_larb_setting_vals[i][j].offset,
				settings->smi_larb_setting_vals[i][j].value);
			}
		}
	}
	return 0;
}

int smi_common_setting(int profile, struct SMI_SETTING *settings)
{
	int i = 0;

	if (!settings || profile < 0 || profile >= SMI_BWC_SCEN_CNT)
		return -1;

	if (settings->smi_common_reg_num == 0)
		return -1;

	/* set regs of common */

	for (i = 0 ; i < settings->smi_common_reg_num ; ++i) {
		M4U_WriteReg32(SMI_COMMON_EXT_BASE,
		settings->smi_common_setting_vals[i].offset,
		settings->smi_common_setting_vals[i].value);
	}

	return 0;
}
