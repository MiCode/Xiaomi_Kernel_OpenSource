// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <soc/mediatek/emi.h>

DEFINE_SPINLOCK(emidbg_lock);

/*
 * mtk_emidbg_dump - dump emi full status to atf log
 *
 */
void mtk_emidbg_dump(void)
{
	unsigned long spinlock_save_flags;
	struct arm_smccc_res smc_res;

	spin_lock_irqsave(&emidbg_lock, spinlock_save_flags);

	arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_DUMP,
		0, 0, 0, 0, 0, 0, &smc_res);

	while (smc_res.a0 > 0) {
		arm_smccc_smc(MTK_SIP_EMIMPU_CONTROL, MTK_EMIDBG_MSG,
		0, 0, 0, 0, 0, 0, &smc_res);

		pr_info("emidbg: %d, 0x%x, 0x%x, 0x%x\n",
			(int)smc_res.a0,
			(unsigned int)smc_res.a1,
			(unsigned int)smc_res.a2,
			(unsigned int)smc_res.a3);
	}

	spin_unlock_irqrestore(&emidbg_lock, spinlock_save_flags);
}
EXPORT_SYMBOL(mtk_emidbg_dump);

MODULE_DESCRIPTION("MediaTek EMI Driver");
MODULE_LICENSE("GPL v2");
