// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <soc/mediatek/emi.h>

#define MPU_REGION_ID_SSPM	5
#define MPU_DOMAIN_D0_AP	0
#define MPU_DOMAIN_D8_SSPM	8

void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	struct emimpu_region_t rg_info;
	int ret;

	ret = mtk_emimpu_init_region(&rg_info, MPU_REGION_ID_SSPM);

	if (ret) {
		pr_info("[SSPM] set emimpu fail\n");
		return;
	}

	mtk_emimpu_set_addr(&rg_info, base, base + size - 1);

	mtk_emimpu_set_apc(&rg_info, MPU_DOMAIN_D0_AP,
		MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, MPU_DOMAIN_D8_SSPM,
		MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);
}
