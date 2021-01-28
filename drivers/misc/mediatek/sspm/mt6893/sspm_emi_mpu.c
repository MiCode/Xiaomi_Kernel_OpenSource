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
#include <memory/mediatek/emi.h>

static unsigned long long sspm_start;
static unsigned long long sspm_end;

void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	sspm_start = base;
	sspm_end = base + size - 1;
}

static int __init post_sspm_set_emi_mpu(void)
{
	struct emimpu_region_t rg_info;

	mtk_emimpu_init_region(&rg_info, SSPM_MPU_REGION_ID);

	mtk_emimpu_set_addr(&rg_info, sspm_start, sspm_end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, 8, MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);

	return 0;
}

late_initcall(post_sspm_set_emi_mpu);

#endif
