/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Trace Memory Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/amba/bus.h>
#include "../sprd_coresight-priv.h"
#include "../sprd_coresight-phy.h"

static struct cs_element_t s_cs_group[] = {
	{"pscp_funnel0", CS_FUNNEL, 0x78104000, 0x8, 0, true, 0x64910860, 0, 0xf0},
	{"pscp_funnel1", CS_FUNNEL, 0x78105000, 0x8, 0, true, 0x64910860, 0, 0xf0},
	{"pscp_funnel2", CS_FUNNEL, 0x78106000, 0x8, 0, true, 0x64910860, 0, 0xf0},
	{"phycp_funnel0", CS_FUNNEL, 0x78304000, 0x8, 0, true, 0x64910860, 0, 0xf00},
	{"phycp_funnel1", CS_FUNNEL, 0x78305000, 0x8, 0, true, 0x64910860, 0, 0xf00},
	{"phycp_funnel2", CS_FUNNEL, 0x78306000, 0x8, 0, true, 0x64910860, 0, 0xf00},
	{"soc_funnel", CS_FUNNEL, 0x78002000, 0x8, 0, false, 0, 0, 0},
	{"soc_etb_regs", CS_TMC, 0x78003000, 0x8000, 0, false, 0, 0, 0},
	{"etb_pscp_c0_regs", CS_TMC, 0x78101000, 0x1000, 0, true, 0x64910860, 0, 0xf0/* bit4:7 */},
	{"etb_pscp_c1_regs", CS_TMC, 0x78102000, 0x1000, 0, true, 0x64910860, 0, 0xf0/* bit4:7 */},
	{"etb_phycp_c0_regs", CS_TMC, 0x78301000, 0x1000, 0, true, 0x64910860, 0, 0xf00/* bit8:11 */},
	{"etb_phycp_c1_regs", CS_TMC, 0x78302000, 0x1000, 0, true, 0x64910860, 0, 0xf00/* bit8:11 */},
};

int cs_group_size(void)
{
	return ARRAY_SIZE(s_cs_group);
}
EXPORT_SYMBOL_GPL(cs_group_size);

struct cs_element_t *cs_group_ptr(void)
{
	return s_cs_group;
}
EXPORT_SYMBOL_GPL(cs_group_ptr);

void sprd_enable_coresight_atb_clk(struct device *dev)
{
	void __iomem *vir_addr;
	u32 regs;

	vir_addr = devm_ioremap(dev, 0x7800A00C, 0x8);
	regs = readl_relaxed(vir_addr);
	//enable phycp_atb_clk_ctrl_force_on & pscp_atb_clk_ctrl_force_on
	regs |= (1<<22)|(1<<24);
	writel_relaxed(regs, vir_addr);
}
EXPORT_SYMBOL_GPL(sprd_enable_coresight_atb_clk);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SHARKL6PRO CORESIGHT PHY Driver");
