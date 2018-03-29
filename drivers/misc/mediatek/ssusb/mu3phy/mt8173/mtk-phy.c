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

#define U3_PHY_LIB
#include <mu3phy/mtk-phy.h>
#undef U3_PHY_LIB
#include <linux/io.h>
#ifdef CONFIG_SSUSB_C60802_SUPPORT
#include <mu3phy/mtk-phy-c60802.h>
#endif
#ifdef CONFIG_SSUSB_D60802_SUPPORT
#include <mu3phy/mtk-phy-d60802.h>
#endif
#ifdef CONFIG_SSUSB_E60802_SUPPORT
#include <mu3phy/mtk-phy-e60802.h>
#endif
#ifdef CONFIG_SSUSB_PROJECT_PHY
#include <mu3phy/mtk-phy-asic.h>
#endif


PHY_INT32 u3phy_init(struct u3phy_reg_base *regs)
{
#ifndef CONFIG_SSUSB_PROJECT_PHY
	PHY_INT32 u3phy_version;
#endif

	if (u3phy != NULL)
		return PHY_TRUE;


	u3phy = kmalloc(sizeof(struct u3phy_info), GFP_NOIO);
#ifdef CONFIG_SSUSB_U3_PHY_GPIO_SUPPORT
	u3phy->phyd_version_addr = 0x2000e4;
#else
	u3phy->phyd_version_addr = U3_PHYD_B2_BASE + 0xe4;
#endif

#ifdef CONFIG_SSUSB_PROJECT_PHY

	u3phy->phy_regs.sif_base = regs->sif_base;
	/* u3phy->phy_regs.sif2_base = regs->sif2_base; */
	u3phy->phy_regs.phy_num = regs->phy_num;
	u3p_project_init(u3phy);
#else

	/* parse phy version */
	u3phy_version = U3PhyReadReg32(u3phy->phyd_version_addr);
	u3phy->phy_version = u3phy_version;

	if (u3phy_version == 0xc60802a) {
#ifdef CONFIG_SSUSB_C60802_SUPPORT

		u3p_c60802_init(u3phy);
#endif
	} else if (u3phy_version == 0xd60802a) {
#ifdef CONFIG_SSUSB_D60802_SUPPORT

		u3p_d60802_init(u3phy);
#endif
	} else if (u3phy_version == 0xe60802a) {
#ifdef CONFIG_SSUSB_E60802_SUPPORT

		u3p_e60802_init(u3phy);
#endif
	} else
		return PHY_FALSE;

#endif

	return PHY_TRUE;
}

void u3phy_exit(struct u3phy_reg_base *regs)
{
	kfree(u3phy);
}

#ifndef CONFIG_SSUSB_PROJECT_PHY

PHY_INT32 U3PhyWriteField8(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value)
{
	PHY_INT8 cur_value;
	PHY_INT8 new_value;

	cur_value = U3PhyReadReg8(addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	U3PhyWriteReg8(addr, new_value);

	return PHY_TRUE;
}

PHY_INT32 U3PhyWriteField32(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value)
{
	PHY_INT32 cur_value;
	PHY_INT32 new_value;

	cur_value = U3PhyReadReg32(addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	U3PhyWriteReg32(addr, new_value);

	return PHY_TRUE;
}

PHY_INT32 U3PhyReadField8(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask)
{

	return ((U3PhyReadReg8(addr) & mask) >> offset);
}

PHY_INT32 U3PhyReadField32(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask)
{

	return ((U3PhyReadReg32(addr) & mask) >> offset);
}
#else
/* only for project phy */
void u3phy_writel(void __iomem *base, u32 reg, u32 offset, u32 mask, u32 value)
{
	void __iomem *addr = base + reg;
	u32 new_value;

	new_value = (readl(addr) & (~mask)) | ((value << offset) & mask);

	writel(new_value, addr);
}

u32 u3phy_readl(void __iomem *base, u32 reg, u32 offset, u32 mask)
{
	return ((readl(base + reg) & mask) >> offset);
}
#endif
