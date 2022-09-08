// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <mt-plat/sync_write.h>
#include "sspm_define.h"

#if SSPM_EMI_PROTECTION_SUPPORT

#include <soc/mediatek/emi.h>
#define SSPM_MPU_PROCT_D0 0
#define SSPM_MPU_PROCT_D8 8

void sspm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	struct emimpu_region_t sspm_region;
	int ret = 0;

	ret = mtk_emimpu_init_region(&sspm_region, SSPM_MPU_REGION_ID);
	if (ret < 0)
		pr_info("%s fail to init emimpu region\n", __func__);
	mtk_emimpu_set_addr(&sspm_region, base, (base + size - 0x1));
	mtk_emimpu_set_apc(&sspm_region, SSPM_MPU_PROCT_D0,
		MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&sspm_region, SSPM_MPU_PROCT_D8,
		MTK_EMIMPU_NO_PROTECTION);
	ret = mtk_emimpu_set_protection(&sspm_region);
	if (ret < 0)
		pr_info("%s fail to set emimpu protection\n", __func__);
	mtk_emimpu_free_region(&sspm_region);
}

#endif
