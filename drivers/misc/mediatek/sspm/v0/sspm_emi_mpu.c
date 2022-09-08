// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <mt-plat/sync_write.h>
#include "sspm_define.h"

#if SSPM_EMI_PROTECTION_SUPPORT
#include <soc/mediatek/emi.h>

void sspm_set_emi_mpu(unsigned int id, phys_addr_t base, phys_addr_t size)
{
	unsigned long long start = base;
	unsigned long long end = base + size - 1;

	struct emimpu_region_t rg_info;
	int ret;

	mtk_emimpu_init_region(&rg_info, id);
	mtk_emimpu_set_addr(&rg_info, start, end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, 8, MTK_EMIMPU_NO_PROTECTION);

	ret = mtk_emimpu_set_protection(&rg_info);
	pr_debug("[SSPM] MPU SSPM Share region <%d:%08llx:%08llx> %s\n",
		id, start, end,
		ret ? "fail" : "success");

	mtk_emimpu_free_region(&rg_info);
	pr_debug("[SSPM] EMI MPU setup done");
}

#endif
