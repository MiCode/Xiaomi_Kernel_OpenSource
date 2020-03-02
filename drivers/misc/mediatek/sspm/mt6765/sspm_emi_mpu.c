/*
 * Copyright (C) 2011-2015 MediaTek Inc.
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

#include <linux/module.h>       /* needed by all modules */
#include <mt-plat/sync_write.h>
#include "sspm_define.h"


#if SSPM_EMI_PROTECTION_SUPPORT
#include <mt_emi_api.h>

void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	struct emi_region_info_t region_info;

	region_info.region = SSPM_MPU_REGION_ID;
	region_info.start = base;
	region_info.end = base + size - 1;
	SET_ACCESS_PERMISSION(region_info.apc, UNLOCK,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
	pr_debug("[SSPM] MPU SSPM Share region<%d:%08llx:%08llx> %x, %x\n",
			region_info.region, region_info.start, region_info.end,
			region_info.apc[1], region_info.apc[0]);

	emi_mpu_set_protection(&region_info);
}
#endif
