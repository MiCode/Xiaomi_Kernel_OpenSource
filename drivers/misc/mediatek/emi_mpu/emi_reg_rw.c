/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include <mt-plat/mtk_device_apc.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/dma.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "mach/emi_mpu.h"
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> 

static void __iomem *emi_base;

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
static int is_emi_mpu_reg(unsigned int offset)
{
	if ((offset >= EMI_MPU_START) && (offset <= EMI_MPU_END))
		return 1;

	return 0;
}
#endif

void mt_emi_reg_write(unsigned int data, unsigned int offset)
{

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	struct arm_smccc_res res;
	if (is_emi_mpu_reg(offset)) {
		arm_smccc_smc(MTK_SIP_KERNEL_EMIMPU_WRITE, offset,data, 0, 0, 0, 0, 0, &res);
		return;
	}
#endif
	if (emi_base)
		mt_reg_sync_writel(data, emi_base + offset);
}

unsigned int mt_emi_reg_read(unsigned int offset)
{

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	struct arm_smccc_res res;
	if (is_emi_mpu_reg(offset))
		arm_smccc_smc(MTK_SIP_KERNEL_EMIMPU_READ, offset,0, 0, 0, 0, 0, 0, &res);
		return res.a0;
#endif
	if (emi_base)
		return readl((const void __iomem *)(emi_base + offset));

	return 0;
}

int mt_emi_mpu_set_region_protection(unsigned long long start,
				     unsigned long long end,
				     unsigned int region_permission)
{
#ifdef CONFIG_ARM64
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	struct arm_smccc_res res;
	arm_smccc_smc(MTK_SIP_KERNEL_EMIMPU_SET, start,end, region_permission, 0, 0, 0, 0, &res);
	return 0;
#endif
#endif
	return 0;
}

void mt_emi_reg_base_set(void *base)
{
	emi_base = base;
}

void *mt_emi_reg_base_get(void)
{
	return emi_base;
}
