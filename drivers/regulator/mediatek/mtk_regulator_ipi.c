/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if 0
#include <mt-plat/upmu_common.h>
#include <sspm_ipi_pin.h>
#include <sspm_ipi.h>
#include <linux/regulator/mediatek/mtk_regulator_ipi.h>
#include <include/pmic_ipi_service_id.h>

#ifdef SSPM_STF
#include <linux/init.h>
#include "sspm_stf.h"
#endif

unsigned int mtk_regulator_ipi_to_sspm(void *buffer, void *retbuf,
	unsigned char lock)
{
	int ret = 0;

	switch (cmd) {
	case MTK_REGULATOR_GET:
		break;
	case MTK_REGULATOR_PUT:
		break;
	case MTK_REGULATOR_ENABLE:
		break;
	case MTK_REGULATOR_FORCE_DISABLE:
		break;
	case MTK_REGULATOR_IS_ENABLED:
		break;
	case MTK_REGULATOR_SET_VOLTAGE:
		break;
	case MTK_REGULATOR_GET_VOLTAGE:
		break;
	case MTK_REGULATOR_SET_MODE:
		break;
	case MTK_REGULATOR_GET_MODE:
		break;
	default:
		break;
	}

	return ret;
}


#endif
