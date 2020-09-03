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
#include <linux/arm-smccc.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/mtk_secure_api.h>
#include <devmpu.h>

#include "mt_emi.h"

static void __iomem *CEN_EMI_BASE;

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

#define MDMCU_DOMAIN		1
#define MDHW_DOMAIN		7

int is_md_master(unsigned int master_id,
		 unsigned int domain_id)
{
	if ((domain_id != MDMCU_DOMAIN) &&
	    (domain_id != MDHW_DOMAIN))
		return 0;

	if ((master_id & 0x7) == MASTER_MDMCU)
		return 1;

	if ((master_id & 0x7) == MASTER_MD)
		return 1;

	return 0;
}

void set_ap_region_permission(unsigned int apc[EMI_MPU_DGROUP_NUM])
{
	SET_ACCESS_PERMISSION(apc, LOCK,
		FORBIDDEN, FORBIDDEN, SEC_RW, NO_PROTECTION,
		NO_PROTECTION, FORBIDDEN, NO_PROTECTION, NO_PROTECTION,
		FORBIDDEN, SEC_R_NSEC_RW, FORBIDDEN, NO_PROTECTION,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
}

void bypass_init(unsigned int *init_flag)
{
	CEN_EMI_BASE = mt_cen_emi_base_get();

	*init_flag = 1;
}

static void clear_devmpu_violation(void)
{
	unsigned int mpus, mput, mput_2nd;
	unsigned int i;
	struct arm_smccc_res smc_res;

	/* clear violation status */
	for (i = 0; i < EMI_MPU_DOMAIN_NUM; i++) {
		/* clear region abort violation */
		mt_reg_sync_writel(0xFFFFFFFF, EMI_MPUD_ST(i));
		/* clear out-of-range violation */
		mt_reg_sync_writel(0x3, EMI_MPUD_ST2(i));
	}

	/* clear debug info */
	mt_reg_sync_writel(0x80000000, EMI_MPUS);

	mpus = readl(IOMEM(EMI_MPUS));
	mput = readl(IOMEM(EMI_MPUT));

	if (mpus) {
		pr_info("[DEVMPU] fail to clear violation\n");
		pr_info("[DEVMPU] EMI_MPUS: %x, EMI_MPUT: %x\n", mpus, mput);
	}

	/* clear hyp violation status */
	arm_smccc_smc(MTK_SIP_KERNEL_DEVMPU_VIO_CLR,
			0, 0, 0, 0, 0, 0, 0, &smc_res);
	mput_2nd = readl(IOMEM(EMI_MPUT_2ND));
	if ((smc_res.a0) || ((mput_2nd >> 21) & 0x3)) {
		pr_info("[MPU] fail to clear hypervisor violation\n");
		pr_info("[MPU] EMI_MPT_2ND: %x\n", mput_2nd);
	}
}

int bypass_violation(unsigned int mpus, unsigned int *init_flag)
{
	unsigned int mput, mput_2nd;
	unsigned long long vio_addr;
	unsigned int master_id, domain_id;
	unsigned int port_id;
	unsigned int wr_vio, wr_oo_vio;
	unsigned int hp_wr_vio;

	mput = readl(IOMEM(EMI_MPUT));
	mput_2nd = readl(IOMEM(EMI_MPUT_2ND));
	vio_addr = ((((unsigned long long)(mput_2nd & 0xF)) << 32) + mput +
		DRAM_OFFSET);
	hp_wr_vio = (mput_2nd >> 21) & 0x3;

	master_id = mpus & 0xFFFF;
	domain_id = (mpus >> 21) & 0xF;
	wr_vio = (mpus >> 29) & 0x3;
	wr_oo_vio = (mpus >> 27) & 0x3;
	port_id = master_id & 0x7;

	/* if is hyperviosr MPU violation, deliver to DevMPU */
	if (hp_wr_vio) {
		devmpu_print_violation(vio_addr, master_id, domain_id,
				hp_wr_vio, true);
		clear_devmpu_violation();
		return 1;
	}

#if defined(CONFIG_MTK_ENABLE_GENIEZONE) \
	|| defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	if ((wr_vio == 2) && (wr_oo_vio == 0) &&
	    ((port_id == 0) || (port_id == 1)))
		return 1;
#endif

	return 0;
}

