// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#ifdef wakelock
#include <linux/wakelock.h>
#endif
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "adsp_feature_define.h"
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"

#if ENABLE_ADSP_EMI_PROTECTION
#include <mt_emi_api.h>
#endif

#ifdef CONFIG_ARM64
#define IOMEM(a)                     ((void __force __iomem *)((a)))
#endif

#define INFRACFG_AO_NODE          "mediatek,infracfg_ao"
#define PERICFG_NODE              "mediatek,pericfg"
#define INFRA_AXI_PROT            (adsp_common.infracfg_ao + 0x0220)
#define INFRA_AXI_PROT_STA1       (adsp_common.infracfg_ao + 0x0228)
#define INFRA_AXI_PROT_SET        (adsp_common.infracfg_ao + 0x02A0)
#define INFRA_AXI_PROT_CLR        (adsp_common.infracfg_ao + 0x02A4)
#define ADSP_AXI_PROT_MASK        (0x1 << 15)
#define ADSP_AXI_PROT_READY_MASK  (0x1 << 15)
#define ADSP_WAY_EN_CTRL          (adsp_common.pericfg + 0x0240)
#define ADSP_WAY_EN_MASK          (0x1 << 13)

#define adsp_reg_read(addr)             __raw_readl(IOMEM(addr))
#define adsp_reg_sync_write(addr, val)  mt_reg_sync_writel(val, addr)

#if ENABLE_ADSP_EMI_PROTECTION
void set_adsp_mpu(phys_addr_t phys_addr, size_t size)
{
	struct emi_region_info_t region_info;

	region_info.start = phys_addr;
	region_info.end = phys_addr + size - 0x1;
	region_info.region = MPU_REGION_ID_ADSP_SMEM;
	SET_ACCESS_PERMISSION(region_info.apc, UNLOCK,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, NO_PROTECTION, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
}
#endif

int adsp_dts_mapping(void)
{
	struct device_node *node;

	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
	if (!node) {
		pr_info("error: cannot find node %s\n", INFRACFG_AO_NODE);
		return -1;
	}

	adsp_common.infracfg_ao = of_iomap(node, 0);
	if (IS_ERR(adsp_common.infracfg_ao)) {
		pr_info("error: cannot iomap infra cfg\n");
		return -1;
	}

	/* pericfg */
	node = of_find_compatible_node(NULL, NULL, PERICFG_NODE);
	if (!node) {
		pr_info("error: cannot find node %s\n", PERICFG_NODE);
		return -1;
	}

	adsp_common.pericfg = of_iomap(node, 0);
	if (IS_ERR(adsp_common.pericfg)) {
		pr_info("error: cannot iomap peri cfg\n");
		return -1;
	}

	return 0;
}

static bool is_adsp_bus_protect_ready(void)
{
	return ((adsp_reg_read(INFRA_AXI_PROT_STA1) & ADSP_AXI_PROT_READY_MASK)
		== ADSP_AXI_PROT_READY_MASK);
}

void adsp_bus_sleep_protect(uint32_t enable)
{
	int timeout = 1000;

	if (enable) {
		/* enable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_SET, ADSP_AXI_PROT_MASK);
		while (--timeout && !is_adsp_bus_protect_ready())
			udelay(1);
		if (!is_adsp_bus_protect_ready())
			pr_err("%s() ready timeout\n", __func__);
	} else {
		/* disable adsp bus protect */
		adsp_reg_sync_write(INFRA_AXI_PROT_CLR, ADSP_AXI_PROT_MASK);
	}
}

void adsp_way_en_ctrl(uint32_t enable)
{
	if (enable)
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) | ADSP_WAY_EN_MASK);
	else
		adsp_reg_sync_write(ADSP_WAY_EN_CTRL,
			adsp_reg_read(ADSP_WAY_EN_CTRL) & ~ADSP_WAY_EN_MASK);
}

