/*
 * Copyright (C) 2020 MediaTek Inc.
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

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>
#ifdef CONFIG_MEDIATEK_EMI
#include <memory/mediatek/emi.h>
#endif

#ifdef CONFIG_MEDIATEK_EMI
#define SECUER (1)
#define NON_SECURE (0)
static unsigned int is_secure(unsigned long long vio_addr)
{
	struct arm_smccc_res smc_res;
	unsigned int addr_h = (vio_addr >> 32);
	unsigned int addr_l = (vio_addr & 0xFFFFFFFF);

	arm_smccc_smc(MTK_SIP_TMEM_CONTROL, addr_h, addr_l, 0, 0, 0, 0, 0,
		      &smc_res);

	pr_debug("%s:%d 0x%X%X, %ld, %ld, %ld, %ld\n", __func__, __LINE__,
		 addr_h, addr_l, smc_res.a0, smc_res.a1, smc_res.a2,
		 smc_res.a3);

	if (smc_res.a0 == 0 && smc_res.a1 == SECUER)
		return 1;
	return 0;
}

#define EMI_MPUS_OFFSET (0x1F0)
#define EMI_MPUT_OFFSET (0x1F8)
#define EMI_MPUT_2ND_OFFSET (0x1FC)
#define EMI_PHY_START (0x40000000)
static irqreturn_t tmem_mpu_vio_handler(unsigned int emi_id,
					struct reg_info_t *dump,
					unsigned int len)
{
	unsigned int i;
	unsigned int mpus = 0, mput = 0, mput_2nd = 0;
	unsigned long long vio_addr;
	unsigned int master_id, domain_id;
	unsigned int port_id;
	unsigned int wr_vio, wr_oo_vio;
	unsigned int hp_wr_vio;

	for (i = 0; i < len; i++) {
		if (dump[i].offset == EMI_MPUS_OFFSET)
			mpus = dump[i].value;

		if (dump[i].offset == EMI_MPUT_OFFSET)
			mput = dump[i].value;

		if (dump[i].offset == EMI_MPUT_2ND_OFFSET)
			mput_2nd = dump[i].value;
	}

	if (!mput && !mput_2nd) {
		pr_err("%s:%d failed to get violation from emi\n", __func__,
		       __LINE__);
		return IRQ_NONE;
	}

	vio_addr = ((((unsigned long long)(mput_2nd & 0xF)) << 32) + mput
		    + EMI_PHY_START);
	hp_wr_vio = (mput_2nd >> 21) & 0x3;

	master_id = mpus & 0xFFFF;
	domain_id = (mpus >> 21) & 0xF;
	wr_vio = (mpus >> 29) & 0x3;
	wr_oo_vio = (mpus >> 27) & 0x3;
	port_id = master_id & 0x7;

	if ((wr_vio == 2) && (wr_oo_vio == 0)
	    && ((port_id == 0) || (port_id == 1))) {
		if (is_secure(vio_addr)) {
			pr_info("%s:%d vio msg ignored at emi%d (0x%x,%d,%d,%d,%d)\n",
				__func__, __LINE__, emi_id, master_id,
				domain_id, wr_vio, wr_oo_vio, port_id);
			return IRQ_HANDLED;
		}
	}

	return IRQ_NONE;
}
#endif

static int __init tmem_mpu_vio_init(void)
{
	int ret = 0;

#ifdef CONFIG_MEDIATEK_EMI
	ret = mtk_emimpu_register_callback(tmem_mpu_vio_handler);
	if (ret) {
		pr_err("%s:%d failed to register emi tmem handler, ret=%d!\n",
		       __func__, __LINE__, ret);
		return ret;
	}
#else
	pr_info("%s:%d vio checker is not registered!\n", __func__, __LINE__);
#endif

	return ret;
}

static void __exit tmem_mpu_vio_exit(void)
{
}

module_init(tmem_mpu_vio_init);
module_exit(tmem_mpu_vio_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Trusted Memory Violation Checker");
