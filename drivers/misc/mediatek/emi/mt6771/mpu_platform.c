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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>

#include "mt_emi.h"

enum {
	MASTER_APM0 = 0,
	MASTER_APM1 = 1,
	MASTER_MM0 = 2,
	MASTER_MDMCU = 3,
	MASTER_MD = 4,
	MASTER_MM1 = 5,
	MASTER_GPU0_PERI = 6,
	MASTER_GPU1_LPDMA = 7,
	MASTER_ALL = 8
};

int is_md_master(unsigned int master_id, unsigned int domain_id)
{
	if ((master_id & 0x7) == MASTER_MDMCU)
		return 1;

	if ((master_id & 0x7) == MASTER_MD)
		return 1;

	return 0;
}

void set_ap_region_permission(unsigned int apc[EMI_MPU_DGROUP_NUM])
{
	SET_ACCESS_PERMISSION(apc, LOCK,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
		SEC_R_NSEC_R, FORBIDDEN, NO_PROTECTION, NO_PROTECTION,
		FORBIDDEN, SEC_R_NSEC_RW, FORBIDDEN, NO_PROTECTION,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
}

void bypass_init(unsigned int *init_flag)
{
	*init_flag = 1;
}

int bypass_violation(unsigned int mpus, unsigned int *init_flag)
{
	int ret;
	void __iomem *cen_emi_base = mt_cen_emi_base_get();
	void __iomem *infra_ao_base;
	unsigned int i;

	if (*init_flag)
		return 0;

	pr_info("[MPU] check bypass condition\n");

	infra_ao_base = ioremap(0x10001000, 0x1000);
	ret = 1;

	mt_reg_sync_writel(0x02000000, cen_emi_base + 0x4E8);
	mt_reg_sync_writel(0xFFFFFFFF, cen_emi_base + 0x4F8);
	mt_reg_sync_writel(0x00000001, cen_emi_base + 0x400);

	if ((mpus & 0x7) != 0x3)
		ret = 0;

	if (((mpus & 0x10000000) != 0x10000000)
		&& ((mpus & 0x08000000) != 0x08000000))
		ret = 0;

	if (readl(IOMEM(cen_emi_base + 0x518)) != 0)
		ret = 0;

	for (i = 0; i < 3; i++) {
		if (ret == 0)
			break;
		if ((readl(IOMEM(infra_ao_base + 0xD34)) & 0x1) != 0x1)
			ret = 0;
		if ((readl(IOMEM(infra_ao_base + 0xD54)) & 0xF000) != 0x7000)
			ret = 0;
	}

	*init_flag = 1;

	return ret;
}
