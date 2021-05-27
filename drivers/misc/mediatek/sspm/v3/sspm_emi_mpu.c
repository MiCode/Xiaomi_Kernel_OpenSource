// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <memory/mediatek/emi.h>

#define SSPM_MPU_REGION_ID     5

static unsigned long long sspm_start;
static unsigned long long sspm_end;

void __init sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	sspm_start = base;
	sspm_end = base + size - 1;
}

static int __init post_sspm_set_emi_mpu(void)
{
	struct emimpu_region_t rg_info;
	int ret;

	ret = mtk_emimpu_init_region(&rg_info, SSPM_MPU_REGION_ID);

	if (ret) {
		pr_info("[SSPM] set emimpu fail\n");
		return ret;
	}

	mtk_emimpu_set_addr(&rg_info, sspm_start, sspm_end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, 8, MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);

	return 0;
}

late_initcall(post_sspm_set_emi_mpu);
