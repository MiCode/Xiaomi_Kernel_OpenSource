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
#include "mtk-phy.h"
#undef U3_PHY_LIB

#ifdef CONFIG_C60802_SUPPORT
#include "mtk-phy-c60802.h"
#endif
#ifdef CONFIG_D60802_SUPPORT
#include "mtk-phy-d60802.h"
#endif
#ifdef CONFIG_E60802_SUPPORT
#include "mtk-phy-e60802.h"
#endif
#ifdef CONFIG_A60810_SUPPORT
#include "mtk-phy-a60810.h"
#endif
#ifdef CONFIG_PROJECT_PHY
#include "mtk-phy-asic.h"
#endif

#ifdef CONFIG_PROJECT_PHY
static struct u3phy_operator project_operators = {
	.init = phy_init_soc,
	.u2_slew_rate_calibration = u2_slew_rate_calibration,
};
#endif

PHY_INT32 u3phy_init(void)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
	/*Move phy init to I2C probe function*/
	return 0;
#else

	if (u3phy != NULL)
		return PHY_TRUE;

	u3phy = kmalloc(sizeof(struct u3phy_info), GFP_NOIO);
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	u3phy->phyd_version_addr = 0x2000e4;
#else
	u3phy->phyd_version_addr = U3_PHYD_B2_BASE + 0xe4;
#endif
	u3phy_ops = NULL;

#ifdef CONFIG_PROJECT_PHY
	u3phy->u2phy_regs_e = (struct u2phy_reg_e *)U2_PHY_BASE;
	u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)U3_PHYD_BASE;
	u3phy->u3phyd_bank2_regs_e =
		(struct u3phyd_bank2_reg_e *)U3_PHYD_B2_BASE;
	u3phy->u3phya_regs_e = (struct u3phya_reg_e *)U3_PHYA_BASE;
	u3phy->u3phya_da_regs_e =
		(struct u3phya_da_reg_e *)U3_PHYA_DA_BASE;
	u3phy->sifslv_chip_regs_e =
		(struct sifslv_chip_reg_e *)SIFSLV_CHIP_BASE;
	u3phy->spllc_regs_e = (struct spllc_reg_e *)SIFSLV_SPLLC_BASE;
	u3phy->sifslv_fm_regs_e =
		(struct sifslv_fm_feg_e *)SIFSLV_FM_FEG_BASE;
	u3phy_ops = (struct u3phy_operator *)&project_operators;
#else
#endif

	if (!u3phy_ops)
		return PHY_FALSE;
	else
		return PHY_TRUE;

#endif
}

PHY_INT32 U3PhyWriteField8(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value)
{
	PHY_INT8 cur_value;
	PHY_INT8 new_value;

	cur_value = U3PhyReadReg8((u3phy_addr_t) addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	mb();
	/**/ U3PhyWriteReg8((u3phy_addr_t) addr, new_value);

	mb();
	/**/ return PHY_TRUE;
}

PHY_INT32 U3PhyWriteField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value)
{
	PHY_INT32 cur_value;
	PHY_INT32 new_value;

	cur_value = U3PhyReadReg32((u3phy_addr_t) addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	mb();
	/**/ U3PhyWriteReg32((u3phy_addr_t) addr, new_value);

	mb();
	/**/ return PHY_TRUE;
}

PHY_INT32 U3PhyReadField8(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask)
{
	return (U3PhyReadReg8((u3phy_addr_t) addr) & mask) >> offset;
}

PHY_INT32 U3PhyReadField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask)
{

	return (U3PhyReadReg32((u3phy_addr_t) addr) & mask) >> offset;
}

void phy_hsrx_set(void)
{
	switch (u3phy->phy_version) {
#ifdef CONFIG_D60802_SUPPORT
	case 0xd60802a:
		U3PhyWriteField32(((phys_addr_t) &u3phy->u2phy_regs_d->usbphyacr6)
				  , D60802_RG_USB20_HSRX_MMODE_SELE_OFST,
				  D60802_RG_USB20_HSRX_MMODE_SELE, 0x2);

		pr_debug("%s: WRITE HSRX_MMODE_SELE(%d)\n", __func__,
			 U3PhyReadField32(((phys_addr_t) &u3phy->u2phy_regs_d->usbphyacr6)
					  , D60802_RG_USB20_HSRX_MMODE_SELE_OFST,
					  D60802_RG_USB20_HSRX_MMODE_SELE));
		break;
#endif
	}
}

void phy_hsrx_reset(void)
{
	switch (u3phy->phy_version) {
#ifdef CONFIG_D60802_SUPPORT
	case 0xd60802a:
		U3PhyWriteField32(((phys_addr_t) &u3phy->u2phy_regs_d->usbphyacr6)
				  , D60802_RG_USB20_HSRX_MMODE_SELE_OFST,
				  D60802_RG_USB20_HSRX_MMODE_SELE, 0x0);

		pr_debug("%s: WRITE HSRX_MMODE_SELE(%d)\n", __func__,
			 U3PhyReadField32(((phys_addr_t) &u3phy->u2phy_regs_d->usbphyacr6)
					  , D60802_RG_USB20_HSRX_MMODE_SELE_OFST,
					  D60802_RG_USB20_HSRX_MMODE_SELE));
		break;
#endif
	}
}
