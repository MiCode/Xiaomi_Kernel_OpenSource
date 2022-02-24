/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
//#include <mt-plat/mtk_ram_console.h>
#endif
#include "mt_emi_api.h"
#include <../emi_ctrl_v1.h>

#include "mt_emi.h"
#include "plat_debug_api.h"

static void __iomem *cen_emi_base;
static void __iomem *base0;
static void __iomem *base1;
static void __iomem *base2;

void plat_debug_api_init(void)
{
	cen_emi_base = mt_cen_emi_base_get();
	base0 = mt_emi_dbg_base_get(0);
	base1 = mt_chn_emi_base_get(0);
	base2 = mt_chn_emi_base_get(1);

	pr_debug("[EMI] >> cen: %p\n", cen_emi_base);
	pr_debug("[EMI] >> base0: %p\n", base0);
	pr_debug("[EMI] >> base1: %p\n", base1);
	pr_debug("[EMI] >> base2: %p\n", base2);
}

static void infra_dbg0(unsigned int v0, unsigned int v1,
		       unsigned int v2, unsigned int v3,
		       const char *str)
{
	writel(v0, base0+0x100);
	writel(v1, base0+0x104);
	writel(v2, base0+0x108);
	writel(v3, base0+0x10c);
	pr_debug("[EMI] %s: 0x%x\n", str, readl(cen_emi_base+0x7fc));
}

static void infra_dbg1(unsigned int v0, unsigned int v1,
		       const char *str)
{
	writel(v0, base1+0xa88);
	writel(v1, base1+0xa8c);
	pr_debug("[EMI] %s: 0x%x\n", str, readl(base1+0xa84));
}

static void infra_dbg2(unsigned int v0, unsigned int v1,
		       const char *str)
{
	writel(v0, base2+0xa88);
	writel(v1, base2+0xa8c);
	pr_debug("[EMI] %s: 0x%x\n", str, readl(base2+0xa84));
}

static void one_shot(void)
{
	pr_debug("\n----------------\n");
	writel(readl(cen_emi_base+0xe8)|(0x1<<8), cen_emi_base+0xe8);

	infra_dbg0(0x20, 0x40, 0x60, 0x80, "CEN_EMI_DEBUG_0");
	infra_dbg0(0xa0, 0xc0, 0xe0, 0x100, "CEN_EMI_DEBUG_1");
	infra_dbg0(0x120, 0x140, 0x160, 0x180, "CEN_EMI_DEBUG_2");
	infra_dbg0(0x1a0, 0x1c0, 0x1e0, 0x200, "CEN_EMI_DEBUG_3");
	infra_dbg0(0x220, 0x240, 0x260, 0x280, "CEN_EMI_DEBUG_4");
	infra_dbg0(0x2a0, 0x2c0, 0x2e0, 0x300, "CEN_EMI_DEBUG_5");
	infra_dbg0(0x320, 0x340, 0x360, 0x380, "CEN_EMI_DEBUG_6");
	infra_dbg0(0x3a0, 0x3c0, 0x3e0, 0x400, "CEN_EMI_DEBUG_7");
	infra_dbg0(0x420, 0x4e0, 0x500, 0x520, "CEN_EMI_DEBUG_8");
	infra_dbg0(0x540, 0x560, 0x580, 0x5a0, "CEN_EMI_DEBUG_9");
	infra_dbg0(0x6c0, 0x6e0, 0x700, 0x720, "CEN_EMI_DEBUG_A");
	infra_dbg0(0x740, 0x7a0, 0x7c0, 0x7e0, "CEN_EMI_DEBUG_B");
	pr_debug("[EMI] %s: 0x%x\n", "EMI_IOCL", readl(cen_emi_base+0xd0));
	pr_debug("[EMI] %s: 0x%x\n", "EMI_IOCL_2ND", readl(cen_emi_base+0xd4));
	pr_debug("[EMI] %s: 0x%x\n", "EMI_IOCM", readl(cen_emi_base+0xd8));
	pr_debug("[EMI] %s: 0x%x\n", "EMI_IOCM_2ND", readl(cen_emi_base+0xdc));

	writel(readl(base1+0xa80)|0x1, base1+0xa80);

	infra_dbg1(0x10000, 0x30002, "CHN0_EMI_DBG_0");
	infra_dbg1(0x50004, 0x70006, "CHN0_EMI_DBG_1");
	infra_dbg1(0x90008, 0xb000a, "CHN0_EMI_DBG_2");
	infra_dbg1(0xd000c, 0xf000e, "CHN0_EMI_DBG_3");
	infra_dbg1(0x110010, 0x130012, "CHN0_EMI_DBG_4");
	infra_dbg1(0x150014, 0x170016, "CHN0_EMI_DBG_5");
	infra_dbg1(0x190018, 0x1b001a, "CHN0_EMI_DBG_6");
	infra_dbg1(0x1d001c, 0x1f001e, "CHN0_EMI_DBG_7");
	infra_dbg1(0x210020, 0x230022, "CHN0_EMI_DBG_8");
	infra_dbg1(0x250024, 0x270026, "CHN0_EMI_DBG_9");
	infra_dbg1(0x290028, 0x310030, "CHN0_EMI_DBG_A");
	infra_dbg1(0x370036, 0x390038, "CHN0_EMI_DBG_B");

	writel(readl(base2+0xa80)|0x1, base2+0xa80);

	infra_dbg2(0x10000, 0x30002, "CHN1_EMI_DBG_0");
	infra_dbg2(0x50004, 0x70006, "CHN1_EMI_DBG_1");
	infra_dbg2(0x90008, 0xb000a, "CHN1_EMI_DBG_2");
	infra_dbg2(0xd000c, 0xf000e, "CHN1_EMI_DBG_3");
	infra_dbg2(0x110010, 0x130012, "CHN1_EMI_DBG_4");
	infra_dbg2(0x150014, 0x170016, "CHN1_EMI_DBG_5");
	infra_dbg2(0x190018, 0x1b001a, "CHN1_EMI_DBG_6");
	infra_dbg2(0x1d001c, 0x1f001e, "CHN1_EMI_DBG_7");
	infra_dbg2(0x210020, 0x230022, "CHN1_EMI_DBG_8");
	infra_dbg2(0x250024, 0x270026, "CHN1_EMI_DBG_9");
	infra_dbg2(0x290028, 0x310030, "CHN1_EMI_DBG_A");
	infra_dbg2(0x370036, 0x390038, "CHN1_EMI_DBG_B");
}

void dump_emi_outstanding(void)
{
	if (is_infra_timeout()) {
		lastbus_timeout_dump();
		one_shot();
		mdelay(1);
		one_shot();
		mdelay(1);
		one_shot();
	} else {
		pr_debug("[BUS] no infra timeout now\n");
		pr_debug("[BUS] should check those before bus\n");
	}
}
