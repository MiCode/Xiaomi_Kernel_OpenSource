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

#include <mt-plat/mt_device_apc.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/dma.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "mach/emi_mpu.h"
#include "mach/mt_secure_api.h"

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
	if (is_emi_mpu_reg(offset)) {
		emi_mpu_smc_write(offset, data);
		return;
	}
#endif
	if (emi_base)
		mt_reg_sync_writel(data, emi_base + offset);
}

unsigned int mt_emi_reg_read(unsigned int offset)
{
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	if (is_emi_mpu_reg(offset))
		return (unsigned int)emi_mpu_smc_read(offset);
#endif
	if (emi_base)
		return readl((const void __iomem *)(emi_base + offset));

	return 0;
}

int mt_emi_mpu_set_region_protection(unsigned long long start,
				     unsigned long long end,
				     unsigned int region_permission)
{
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	return emi_mpu_smc_set(start, end, region_permission);
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
