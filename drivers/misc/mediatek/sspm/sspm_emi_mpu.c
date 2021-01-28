// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <mt-plat/sync_write.h>

#ifdef CONFIG_MTK_EMI_LEGACY
#include <mt_emi_api.h>
#elif CONFIG_MTK_EMI
#include <memory/mediatek/emi.h>
#endif


#ifdef CONFIG_MTK_EMI_LEGACY
void sspm_set_emi_mpu(unsigned int id, phys_addr_t base, phys_addr_t size)
{
	struct emi_region_info_t region_info;

	region_info.region = id;
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
#elif CONFIG_MTK_EMI
struct emimpu_region_t sspm_rg_info;

void sspm_set_emi_mpu(unsigned int id, phys_addr_t base, phys_addr_t size)
{
	sspm_rg_info.rg_num = id;
	sspm_rg_info.start = base;
	sspm_rg_info.end = base + size - 1;
}

static int __init post_sspm_set_emi_mpu(void)
{
	struct emimpu_region_t rg_info;
	int ret;

	mtk_emimpu_init_region(&rg_info, sspm_rg_info.rg_num);
	mtk_emimpu_set_addr(&rg_info, sspm_rg_info.start, sspm_rg_info.end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&rg_info, 8, MTK_EMIMPU_NO_PROTECTION);

	ret = mtk_emimpu_set_protection(&rg_info);
	pr_debug("[SSPM] MPU SSPM Share region <%d:%08llx:%08llx> %s\n",
		sspm_rg_info.rg_num, sspm_rg_info.start, sspm_rg_info.end,
		ret ? "fail" : "success");

	mtk_emimpu_free_region(&rg_info);

	return 0;
}

late_initcall(post_sspm_set_emi_mpu);
#endif
