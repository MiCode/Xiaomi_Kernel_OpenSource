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

#include <mt-plat/sync_write.h>
#include <mt-plat/dma.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "mach/emi_mpu.h"
#include <mt-plat/mtk_secure_api.h>


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
}

unsigned int mt_emi_reg_read(unsigned int offset)
{
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	if (is_emi_mpu_reg(offset))
		return (unsigned int)emi_mpu_smc_read(offset);
#endif
		return 0;
}

int mt_emi_mpu_set_region_protection(unsigned int start,
unsigned int end, unsigned int region_permission)
{
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#ifdef CONFIG_ARM64
		return emi_mpu_smc_set(start, end, region_permission);
#else
		return emi_mpu_smc_set(start, end, region_permission, 0);
#endif
#endif
		return 0;
}
